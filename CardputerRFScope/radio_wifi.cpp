#include "radio_wifi.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

namespace rfscope {
namespace {

// Live accumulators, written from the WiFi task by the promiscuous callback
// and drained by the UI task once per dwell. Guarded by a spinlock rather
// than a mutex: the callback must not block.
struct Accum {
    uint32_t frames;
    uint32_t bytes;
    int32_t rssiSum;
    int8_t rssiPeak;
    int8_t rssiMin;
};

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
volatile Accum g_accum = {0, 0, 0, -128, 127};

void IRAM_ATTR snifferCb(void* buf, wifi_promiscuous_pkt_type_t type)
{
    if (buf == nullptr) return;
    const wifi_promiscuous_pkt_t* pkt = static_cast<const wifi_promiscuous_pkt_t*>(buf);
    const int8_t rssi                 = pkt->rx_ctrl.rssi;
    const uint32_t len                = static_cast<uint32_t>(pkt->rx_ctrl.sig_len);
    (void)type;

    portENTER_CRITICAL_ISR(&g_mux);
    g_accum.frames++;
    g_accum.bytes += len;
    g_accum.rssiSum += rssi;
    if (rssi > g_accum.rssiPeak) g_accum.rssiPeak = rssi;
    if (rssi < g_accum.rssiMin) g_accum.rssiMin = rssi;
    portEXIT_CRITICAL_ISR(&g_mux);
}

Accum takeAccum()
{
    Accum out;
    portENTER_CRITICAL(&g_mux);
    out.frames       = g_accum.frames;
    out.bytes        = g_accum.bytes;
    out.rssiSum      = g_accum.rssiSum;
    out.rssiPeak     = g_accum.rssiPeak;
    out.rssiMin      = g_accum.rssiMin;
    g_accum.frames   = 0;
    g_accum.bytes    = 0;
    g_accum.rssiSum  = 0;
    g_accum.rssiPeak = -128;
    g_accum.rssiMin  = 127;
    portEXIT_CRITICAL(&g_mux);
    return out;
}

}  // namespace

void WifiSweeper::begin()
{
    _aps.reserve(MAX_APS);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(50);

    // Channel 14 exists only under some regulatory domains. Ask the radio
    // rather than guessing, and drop it from the sweep if it refuses.
    _channelMax = (esp_wifi_set_channel(14, WIFI_SECOND_CHAN_NONE) == ESP_OK) ? 14 : 13;
    esp_wifi_set_channel(CHANNEL_MIN, WIFI_SECOND_CHAN_NONE);

    for (int i = 0; i < CHANNEL_COUNT; i++) {
        _stats[i]        = ChannelStats{};
        _stats[i].usable = (CHANNEL_MIN + i) <= _channelMax;
    }
}

void WifiSweeper::promiscuousOn()
{
    if (_promiscuous) return;
    wifi_promiscuous_filter_t filter = {};
    filter.filter_mask               = WIFI_PROMIS_FILTER_MASK_ALL;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&snifferCb);
    esp_wifi_set_promiscuous(true);
    _promiscuous = true;
    takeAccum();  // discard whatever leaked in before we were listening
}

void WifiSweeper::promiscuousOff()
{
    if (!_promiscuous) return;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    _promiscuous = false;
}

bool WifiSweeper::hopTo(int ch)
{
    if (!channelValid(ch)) return false;
    return esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE) == ESP_OK;
}

void WifiSweeper::startSweep()
{
    if (_sweeping) return;
    _sweeping   = true;
    _lastHopMs  = millis();
    _channel    = _lockedChannel ? _lockedChannel : CHANNEL_MIN;
    if (!_lockedChannel) hopTo(_channel);
    promiscuousOn();
}

void WifiSweeper::stopSweep()
{
    if (!_sweeping) return;
    promiscuousOff();
    _sweeping = false;
}

void WifiSweeper::lockChannel(int ch)
{
    _lockedChannel = channelValid(ch) ? ch : 0;
    if (_lockedChannel) _channel = _lockedChannel;
}

void WifiSweeper::unlock()
{
    _lockedChannel = 0;
}

void WifiSweeper::setDwellMs(uint32_t ms)
{
    if (ms < DWELL_MIN_MS) ms = DWELL_MIN_MS;
    if (ms > DWELL_MAX_MS) ms = DWELL_MAX_MS;
    _dwellMs = ms;
}

bool WifiSweeper::consumeSweepTick()
{
    const bool t = _sweepTick;
    _sweepTick   = false;
    return t;
}

void WifiSweeper::latchCurrentChannel()
{
    const Accum a  = takeAccum();
    const int idx  = _channel - CHANNEL_MIN;
    if (idx < 0 || idx >= CHANNEL_COUNT) return;

    ChannelStats& s = _stats[idx];
    s.frames        = a.frames;
    s.bytes         = a.bytes;
    if (a.frames > 0) {
        s.rssiAvg  = static_cast<int8_t>(a.rssiSum / static_cast<int32_t>(a.frames));
        s.rssiPeak = a.rssiPeak;
        s.rssiMin  = a.rssiMin;
    } else {
        s.rssiAvg  = RSSI_INVALID;
        s.rssiPeak = RSSI_INVALID;
        s.rssiMin  = RSSI_INVALID;
    }
    _totalFrames += a.frames;
}

void WifiSweeper::serviceApScan(uint32_t now)
{
    switch (_scanState) {
        case ScanState::Idle:
            if (_forceScan) {
                _forceScan = false;
                _scanState = ScanState::Requested;
                return;
            }
            if (!_apScanEnabled || _lockedChannel) return;
            if (now - _lastApScanMs < AP_SCAN_INTERVAL_MS) return;
            _scanState = ScanState::Requested;
            return;

        case ScanState::Requested: {
            // A scan cannot run while the sniffer owns the radio.
            promiscuousOff();
            WiFi.scanDelete();
            const int16_t rc = WiFi.scanNetworks(true, true, false, 110);
            if (rc == WIFI_SCAN_RUNNING || rc == WIFI_SCAN_FAILED) {
                _scanState = (rc == WIFI_SCAN_RUNNING) ? ScanState::Running : ScanState::Idle;
                if (_scanState == ScanState::Idle) {
                    _lastApScanMs = now;
                    if (_sweeping) promiscuousOn();
                }
            } else {
                _scanState = ScanState::Running;
            }
            return;
        }

        case ScanState::Running: {
            const int16_t n = WiFi.scanComplete();
            if (n == WIFI_SCAN_RUNNING) return;

            _aps.clear();
            for (int i = 0; i < CHANNEL_COUNT; i++) {
                _stats[i].apCount = 0;
                _stats[i].apBest  = RSSI_INVALID;
            }

            if (n > 0) {
                const int count = (n > MAX_APS) ? MAX_APS : n;
                for (int i = 0; i < count; i++) {
                    ApInfo ap;
                    const String ssid = WiFi.SSID(i);
                    ssid.toCharArray(ap.ssid, sizeof(ap.ssid));
                    ap.hidden  = ssid.length() == 0;
                    ap.rssi    = static_cast<int8_t>(WiFi.RSSI(i));
                    ap.channel = static_cast<uint8_t>(WiFi.channel(i));
                    ap.auth    = static_cast<uint8_t>(WiFi.encryptionType(i));
                    const uint8_t* b = WiFi.BSSID(i);
                    if (b) memcpy(ap.bssid, b, 6);
                    _aps.push_back(ap);

                    const int idx = ap.channel - CHANNEL_MIN;
                    if (idx >= 0 && idx < CHANNEL_COUNT) {
                        _stats[idx].apCount++;
                        if (_stats[idx].apBest == RSSI_INVALID || ap.rssi > _stats[idx].apBest) {
                            _stats[idx].apBest = ap.rssi;
                        }
                    }
                }
            }

            WiFi.scanDelete();
            _lastApScanMs = now;
            _scanState    = ScanState::Idle;
            if (_sweeping) {
                promiscuousOn();
                hopTo(_channel);
                _lastHopMs = now;
            }
            return;
        }
    }
}

void WifiSweeper::loop()
{
    const uint32_t now = millis();

    serviceApScan(now);
    if (_scanState != ScanState::Idle) return;  // radio is busy scanning
    if (!_sweeping) return;

    if (now - _lastHopMs < _dwellMs) return;
    _lastHopMs = now;

    latchCurrentChannel();

    if (_lockedChannel) return;  // meter mode: stay put

    int next = _channel + 1;
    if (next > _channelMax) {
        next        = CHANNEL_MIN;
        _sweepCount++;
        _sweepTick = true;
    }
    _channel = next;
    hopTo(_channel);
}

}  // namespace rfscope
