#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "app.h"
#include "ui_theme.h"
#include "ui_widgets.h"

namespace rfscope {
namespace {

// BLE advertising channels and their centres.
struct AdvChannel {
    uint8_t index;
    int freqMhz;
};
constexpr AdvChannel kAdvChannels[3] = {{37, 2402}, {38, 2426}, {39, 2480}};

// Band view maps the whole ISM span onto the screen width.
constexpr int kBandLoMhz = 2398;
constexpr int kBandHiMhz = 2492;

int freqToX(int mhz)
{
    const float f = static_cast<float>(mhz - kBandLoMhz) / static_cast<float>(kBandHiMhz - kBandLoMhz);
    int x         = static_cast<int>(f * static_cast<float>(SCREEN_W - 1));
    if (x < 0) x = 0;
    if (x > SCREEN_W - 1) x = SCREEN_W - 1;
    return x;
}

void bleLabel(const BleDevice& d, char* out, size_t n)
{
    if (d.hasName()) {
        snprintf(out, n, "%s", d.name);
    } else {
        char addr[20];
        d.addrToString(addr, sizeof(addr));
        snprintf(out, n, "%s", addr);
    }
}

}  // namespace

void App::refreshBleOrder()
{
    const std::vector<BleDevice>& devs = _ble.devices();
    _bleOrder.clear();
    _bleOrder.reserve(devs.size());
    for (size_t i = 0; i < devs.size(); i++) _bleOrder.push_back(static_cast<int>(i));
    // BLE RSSI moves constantly, so unlike the AP list this is re-sorted every
    // frame -- but into a vector that keeps its capacity, so it does not churn
    // the heap.
    std::sort(_bleOrder.begin(), _bleOrder.end(), [&devs](int a, int b) {
        return devs[static_cast<size_t>(a)].rssi > devs[static_cast<size_t>(b)].rssi;
    });
}

// -------------------------------------------------------------- ble list --

void App::drawBleList()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();

    char right[24];
    snprintf(right, sizeof(right), "%u dev  %lu/s",
             static_cast<unsigned>(_ble.devices().size()),
             static_cast<unsigned long>(_ble.advRate()));
    ui::drawHeader("BLE DEVICES", right);

    g.setFont(&fonts::Font0);

    if (!_ble.available()) {
        g.setTextColor(t.warn, t.bg);
        g.drawString("BLE stack failed to start", 6, BODY_Y + 24);
        g.setTextColor(t.textDim, t.bg);
        g.drawString("not enough heap for NimBLE", 6, BODY_Y + 36);
        ui::drawFooter("BKSP back");
        return;
    }

    // Strongest first, so the thing you are walking toward stays near the top.
    const std::vector<int>& order = _bleOrder;

    if (order.empty()) {
        g.setTextColor(t.textDim, t.bg);
        g.drawString("listening for advertisements...", 6, BODY_Y + 24);
        ui::drawFooter("BKSP back");
        return;
    }

    if (_bleSel >= static_cast<int>(order.size())) _bleSel = static_cast<int>(order.size()) - 1;
    if (_bleSel < 0) _bleSel = 0;

    constexpr int kRowH = 13;
    const int visible   = (SCREEN_H - FOOTER_H - BODY_Y) / kRowH;
    if (_bleSel < _bleTop) _bleTop = _bleSel;
    if (_bleSel >= _bleTop + visible) _bleTop = _bleSel - visible + 1;

    for (int row = 0; row < visible; row++) {
        const int i = _bleTop + row;
        if (i >= static_cast<int>(order.size())) break;

        const BleDevice& d = _ble.devices()[static_cast<size_t>(order[static_cast<size_t>(i)])];
        const int y        = BODY_Y + 1 + row * kRowH;
        const bool on      = (i == _bleSel);

        if (on) g.fillRect(1, y, SCREEN_W - 2, kRowH - 1, t.accent);

        char label[24];
        bleLabel(d, label, sizeof(label));
        g.setTextDatum(middle_left);
        g.setTextColor(on ? t.accentText : t.text, on ? t.accent : t.bg);
        g.drawString(label, 4, y + kRowH / 2 - 1);

        char meta[16];
        snprintf(meta, sizeof(meta), "%s", d.connectable ? "conn" : "adv");
        g.setTextColor(on ? t.accentText : t.textDim, on ? t.accent : t.bg);
        g.setTextDatum(middle_right);
        g.drawString(meta, SCREEN_W - 44, y + kRowH / 2 - 1);
        g.setTextDatum(top_left);

        ui::drawSignalBar(SCREEN_W - 42, y + 3, 24, 7, d.rssi, d.rssi != RSSI_INVALID);
        char db[8];
        snprintf(db, sizeof(db), "%d", d.rssi);
        g.setTextDatum(middle_right);
        g.setTextColor(on ? t.accentText : ui::levelColor(ui::dbmLevel(d.rssi)),
                       on ? t.accent : t.bg);
        g.drawString(db, SCREEN_W - 2, y + kRowH / 2 - 1);
        g.setTextDatum(top_left);
    }

    ui::drawFooter("; . move   ENTER track   BKSP back");
}

void App::keyBleList(const KeyEvent& e)
{
    if (!isActionable(e)) return;

    const std::vector<int>& order = _bleOrder;

    switch (navFor(e)) {
        case Nav::Up:
            if (_bleSel > 0) _bleSel--;
            break;
        case Nav::Down:
            if (_bleSel + 1 < static_cast<int>(order.size())) _bleSel++;
            break;
        case Nav::Select:
            if (!order.empty()) {
                const BleDevice& d =
                    _ble.devices()[static_cast<size_t>(order[static_cast<size_t>(_bleSel)])];
                memcpy(_trackAddr, d.addr, 6);
                _tracking = true;
                _bleTrace.clear();
                _blePeak.reset();
                go(Screen::BleTracker);
            }
            break;
        case Nav::Back:
            go(Screen::Menu);
            break;
        default:
            break;
    }
}

// ----------------------------------------------------------- ble tracker --

void App::sampleBleTracker(uint32_t now)
{
    if (!_tracking) return;
    const int idx = _ble.findDevice(_trackAddr);
    if (idx < 0) return;

    const BleDevice& d = _ble.devices()[static_cast<size_t>(idx)];
    if (d.rssi == RSSI_INVALID) return;
    if (now - _lastSampleMs < 60) return;
    _lastSampleMs = now;

    _bleTrace.push(d.rssi);
    _blePeak.update(static_cast<float>(d.rssi), now);
    _audio.update(d.rssi, true, now);
}

void App::drawBleTracker()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();
    const int idx  = _tracking ? _ble.findDevice(_trackAddr) : -1;

    char title[26] = "TRACKING";
    if (idx >= 0) bleLabel(_ble.devices()[static_cast<size_t>(idx)], title, sizeof(title));
    ui::drawHeader(title, "BLE");

    g.setFont(&fonts::Font0);

    if (idx < 0) {
        g.setTextColor(t.warn, t.bg);
        g.drawString("device out of range", 8, BODY_Y + 24);
        g.setTextColor(t.textDim, t.bg);
        g.drawString("it stopped advertising", 8, BODY_Y + 36);
        ui::drawFooter("BKSP back to list");
        return;
    }

    const BleDevice& d = _ble.devices()[static_cast<size_t>(idx)];
    const bool have    = !_bleTrace.empty();
    const int live     = have ? _bleTrace.newest() : DBM_FLOOR;
    const int peak     = _blePeak.valid() ? static_cast<int>(_blePeak.value()) : DBM_FLOOR;

    ui::drawGauge(50, 74, 34, live, have, peak, _blePeak.valid());

    char buf[28];
    snprintf(buf, sizeof(buf), "%d", live);
    g.setFont(&fonts::Font4);
    g.setTextDatum(top_right);
    g.setTextColor(have ? ui::levelColor(ui::dbmLevel(live)) : t.textDim, t.bg);
    g.drawString(buf, 176, BODY_Y + 4);

    g.setFont(&fonts::Font0);
    g.setTextDatum(top_left);
    g.setTextColor(t.textDim, t.bg);
    g.drawString("dBm", 179, BODY_Y + 16);

    d.addrToString(buf, sizeof(buf));
    g.drawString(buf, 100, BODY_Y + 30);

    snprintf(buf, sizeof(buf), "min %d  max %d", _bleTrace.min(), _bleTrace.max());
    g.drawString(buf, 100, BODY_Y + 42);
    snprintf(buf, sizeof(buf), "avg %.0f  jit %.1f", _bleTrace.avg(), _bleTrace.jitter());
    g.drawString(buf, 100, BODY_Y + 52);
    snprintf(buf, sizeof(buf), "%lu packets", static_cast<unsigned long>(d.packets));
    g.drawString(buf, 100, BODY_Y + 62);

    const int ty = SCREEN_H - FOOTER_H - 34;
    ui::drawTraceBox(2, ty, SCREEN_W - 4, 32);
    const size_t n = _bleTrace.copyTo(_scratch, METER_TRACE_LEN);
    ui::drawTrace(2, ty, SCREEN_W - 4, 32, _scratch, static_cast<int>(n), 1);

    ui::drawFooter(_audio.enabled() ? "A audio off   BKSP back" : "A audio on   BKSP back");
}

void App::keyBleTracker(const KeyEvent& e)
{
    if (!isActionable(e)) return;

    if (e.code == 'a' || e.code == 'A') {
        _settings.audioEnabled = !_settings.audioEnabled;
        _audio.setEnabled(_settings.audioEnabled);
        _settings.save();
        return;
    }
    if (navFor(e) == Nav::Back) {
        _audio.silence();
        _tracking = false;
        go(Screen::BleList);
    }
}

// ------------------------------------------------------------- band view --

void App::drawBand()
{
    auto& g                = ui::gfx();
    const Theme& t         = theme();
    const ChannelStats* cs = _sweeper.channels();

    ui::drawHeader("BAND 2.4G", "WIFI + BLE");

    const int top = BODY_Y + 10;
    const int h   = SCREEN_H - FOOTER_H - top - 10;

    // Coexistence warning: sharing the radio costs both sides duty cycle, and
    // pretending otherwise would make these numbers look better than they are.
    g.setFont(&fonts::Font0);
    g.setTextColor(t.warn, t.bg);
    g.drawString("COEX - reduced duty cycle", 4, BODY_Y);

    // Baseline and frequency ticks.
    g.drawFastHLine(0, top + h, SCREEN_W, t.grid);
    for (int f = 2400; f <= 2490; f += 20) {
        const int x = freqToX(f);
        g.drawFastVLine(x, top + h, 3, t.grid);
        char lbl[8];
        snprintf(lbl, sizeof(lbl), "%d", f);
        g.setTextDatum(top_center);
        g.setTextColor(t.textDim, t.bg);
        g.drawString(lbl, x, top + h + 4);
        g.setTextDatum(top_left);
    }

    // WiFi channels, positioned by their true centre frequency.
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        if (!cs[i].usable || cs[i].rssiPeak == RSSI_INVALID) continue;
        const int ch    = i + CHANNEL_MIN;
        const int x     = freqToX(channelToFreqMhz(ch));
        const float lvl = ui::dbmLevel(cs[i].rssiPeak);
        const int bh    = static_cast<int>(lvl * static_cast<float>(h));
        if (bh > 0) g.fillRect(x - 3, top + h - bh, 7, bh, ui::levelColor(lvl));
    }

    // BLE advertising channels. Height is the observed advert rate, scaled so
    // a busy room fills the plot.
    const uint32_t rate = _ble.advRate();
    const float rlvl    = rate > 60 ? 1.0f : static_cast<float>(rate) / 60.0f;
    for (const auto& ac : kAdvChannels) {
        const int x  = freqToX(ac.freqMhz);
        const int bh = static_cast<int>(rlvl * static_cast<float>(h));
        g.drawFastVLine(x, top, h, t.grid);
        if (bh > 0) g.fillRect(x - 1, top + h - bh, 3, bh, t.accent);

        char lbl[6];
        snprintf(lbl, sizeof(lbl), "%u", static_cast<unsigned>(ac.index));
        g.setTextDatum(bottom_center);
        g.setTextColor(t.accent, t.bg);
        g.drawString(lbl, x, top - 1);
        g.setTextDatum(top_left);
    }

    char foot[48];
    snprintf(foot, sizeof(foot), "%u BLE dev  %lu adv/s  BKSP back",
             static_cast<unsigned>(_ble.devices().size()),
             static_cast<unsigned long>(rate));
    ui::drawFooter(foot);
}

void App::keyBand(const KeyEvent& e)
{
    if (!isActionable(e)) return;
    if (navFor(e) == Nav::Back) go(Screen::Menu);
}

}  // namespace rfscope
