// 2.4 GHz sweep engine.
//
// The ESP32-S3 has no spectrum-sweep peripheral, so "spectrum" here means:
// park the radio on one channel, count every frame the PHY hands us and note
// its RSSI, then hop. Over a full sweep that yields real per-channel
// occupancy and signal levels -- channel resolution, not sub-MHz.
#pragma once

#include <cstdint>
#include <vector>

#include "channel_map.h"
#include "rfscope_config.h"
#include "signal_stats.h"

namespace rfscope {

struct ChannelStats {
    uint32_t frames = 0;
    uint32_t bytes  = 0;
    int8_t rssiAvg  = RSSI_INVALID;
    int8_t rssiPeak = RSSI_INVALID;
    int8_t rssiMin  = RSSI_INVALID;  // crude noise-floor proxy
    uint8_t apCount = 0;
    int8_t apBest   = RSSI_INVALID;
    bool usable     = true;  // false when the radio refuses this channel
};

struct ApInfo {
    char ssid[33] = {0};
    uint8_t bssid[6] = {0};
    int8_t rssi = 0;
    uint8_t channel = 0;
    uint8_t auth = 0;  // wifi_auth_mode_t
    bool hidden = false;
};

class WifiSweeper {
public:
    // Brings up the radio in station mode, disconnected, and probes whether
    // channel 14 is permitted by the current regulatory settings.
    void begin();

    void startSweep();
    void stopSweep();
    bool sweeping() const
    {
        return _sweeping;
    }

    // Pin the sniffer to one channel instead of hopping. Used by the meter,
    // where the radio is already associated and cannot leave its channel.
    void lockChannel(int ch);
    void unlock();

    void setDwellMs(uint32_t ms);
    uint32_t dwellMs() const
    {
        return _dwellMs;
    }

    // Force an AP scan on the next loop(), even when not sweeping. The
    // network picker uses this so it does not need its own scan code.
    void requestApScan()
    {
        _forceScan = true;
    }

    void setApScanEnabled(bool on)
    {
        _apScanEnabled = on;
    }
    bool apScanEnabled() const
    {
        return _apScanEnabled;
    }

    // Drive from the UI loop. Non-blocking.
    void loop();

    // Index 0 is channel 1.
    const ChannelStats* channels() const
    {
        return _stats;
    }
    int channelMax() const
    {
        return _channelMax;
    }
    int currentChannel() const
    {
        return _channel;
    }
    uint32_t sweepCount() const
    {
        return _sweepCount;
    }
    // Bumps once per completed sweep; the waterfall uses it to know when to
    // push a new row.
    bool consumeSweepTick();

    const std::vector<ApInfo>& aps() const
    {
        return _aps;
    }
    bool apScanInFlight() const
    {
        return _scanState != ScanState::Idle;
    }

    // Bumped each time the AP list is replaced. The UI caches sorting and
    // saved-credential lookups against this instead of redoing them per frame.
    uint32_t apGeneration() const
    {
        return _apGeneration;
    }

    // Total frames seen since begin(), for the "is anything happening" readout.
    uint32_t totalFrames() const
    {
        return _totalFrames;
    }

private:
    enum class ScanState : uint8_t { Idle, Requested, Running };

    void promiscuousOn();
    void promiscuousOff();
    bool hopTo(int ch);
    void latchCurrentChannel();
    void serviceApScan(uint32_t now);

    ChannelStats _stats[CHANNEL_COUNT];
    std::vector<ApInfo> _aps;

    uint32_t _dwellMs   = DWELL_DEFAULT_MS;
    int _channel        = CHANNEL_MIN;
    int _channelMax     = 13;  // 14 only if the radio accepts it
    int _lockedChannel  = 0;   // 0 = hopping
    bool _sweeping      = false;
    bool _promiscuous   = false;
    bool _apScanEnabled = true;
    bool _forceScan     = false;

    uint32_t _lastHopMs    = 0;
    uint32_t _lastApScanMs = 0;
    uint32_t _sweepCount   = 0;
    uint32_t _totalFrames  = 0;
    uint32_t _apGeneration = 0;
    bool _sweepTick        = false;
    ScanState _scanState   = ScanState::Idle;
};

}  // namespace rfscope
