#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "app.h"
#include "storage.h"
#include "ui_theme.h"
#include "ui_widgets.h"

namespace rfscope {
namespace {

// Spectrum layout.
constexpr int kBarTop    = BODY_Y + 1;
constexpr int kBarH      = 54;
constexpr int kLabelY    = kBarTop + kBarH + 1;
constexpr int kWfTop     = kLabelY + 9;
constexpr int kWfH       = SCREEN_H - FOOTER_H - kWfTop;
constexpr int kSlotW     = 17;
constexpr int kBarInset  = 4;

int slotX(int chIndex)
{
    return 1 + chIndex * kSlotW;
}

void ssidLabel(const ApInfo& ap, char* out, size_t n)
{
    if (ap.hidden || ap.ssid[0] == '\0') {
        snprintf(out, n, "<hidden>");
    } else {
        snprintf(out, n, "%s", ap.ssid);
    }
}

}  // namespace

void App::refreshApView()
{
    const uint32_t gen = _sweeper.apGeneration();
    if (_apViewValid && gen == _apViewGen) return;

    const std::vector<ApInfo>& aps = _sweeper.aps();

    _apOrder.clear();
    _apOrder.reserve(aps.size());
    for (size_t i = 0; i < aps.size(); i++) _apOrder.push_back(static_cast<int>(i));
    std::sort(_apOrder.begin(), _apOrder.end(),
              [&aps](int a, int b) { return aps[static_cast<size_t>(a)].rssi >
                                            aps[static_cast<size_t>(b)].rssi; });

    // Each of these opens NVS, so it happens once per scan and never per frame.
    _apSaved.assign(aps.size(), 0);
    for (size_t i = 0; i < aps.size(); i++) {
        _apSaved[i] = CredentialStore::has(aps[i].ssid) ? 1 : 0;
    }

    _apViewGen   = gen;
    _apViewValid = true;
}

// --------------------------------------------------------------- spectrum --

void App::pushWaterfallRow()
{
    for (int r = WATERFALL_ROWS - 1; r > 0; r--) {
        memcpy(_waterfall[r], _waterfall[r - 1], CHANNEL_COUNT);
    }

    const ChannelStats* cs = _sweeper.channels();
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        uint8_t level = 0;
        if (cs[i].usable && cs[i].rssiPeak != RSSI_INVALID) {
            level = static_cast<uint8_t>(ui::dbmLevel(cs[i].rssiPeak) * 255.0f);
        }
        _waterfall[0][i] = level;
    }
    if (_wfRows < WATERFALL_ROWS) _wfRows++;
}

void App::drawSpectrum()
{
    auto& g                = ui::gfx();
    const Theme& t         = theme();
    const ChannelStats* cs = _sweeper.channels();

    char right[28];
    snprintf(right, sizeof(right), "ch%d  sweep %lu", _sweeper.currentChannel(),
             static_cast<unsigned long>(_sweeper.sweepCount()));
    ui::drawHeader("SPECTRUM 2.4G", right);

    // Reference lines every 25 dB across the bar field.
    for (int d = DBM_FLOOR + 25; d < DBM_CEILING; d += 25) {
        const int y = ui::dbmToY(d, kBarTop, kBarH);
        for (int x = 0; x < SCREEN_W; x += 6) g.drawPixel(x, y, t.grid);
    }

    // Occupancy is scaled against the busiest channel this sweep, so the
    // display stays readable whether the band is dead or saturated.
    uint32_t busiest = 1;
    for (int i = 0; i < CHANNEL_COUNT; i++)
        if (cs[i].frames > busiest) busiest = cs[i].frames;

    for (int i = 0; i < CHANNEL_COUNT; i++) {
        const int x       = slotX(i);
        const bool cursor = (i + CHANNEL_MIN) == _chCursor;

        if (!cs[i].usable) {
            g.setFont(&fonts::Font0);
            g.setTextDatum(top_center);
            g.setTextColor(t.grid, t.bg);
            g.drawString("-", x + kSlotW / 2, kBarTop + kBarH / 2);
            continue;
        }

        // Wide, dim bar: how much traffic this channel carried.
        const float occ  = static_cast<float>(cs[i].frames) / static_cast<float>(busiest);
        const int occH   = static_cast<int>(occ * static_cast<float>(kBarH - 1));
        if (occH > 0) {
            g.fillRect(x, kBarTop + kBarH - occH, kSlotW - 2, occH, t.ramp[0]);
        }

        // Narrow, bright bar: how strong the strongest frame was.
        if (cs[i].rssiPeak != RSSI_INVALID) {
            const float lvl = ui::dbmLevel(cs[i].rssiPeak);
            const int h     = static_cast<int>(lvl * static_cast<float>(kBarH - 1));
            if (h > 0) {
                g.fillRect(x + kBarInset, kBarTop + kBarH - h, kSlotW - 2 - kBarInset * 2, h,
                           ui::levelColor(lvl));
            }
        }

        // A tick for the strongest beaconing AP the scan found here.
        if (cs[i].apBest != RSSI_INVALID) {
            const int y = ui::dbmToY(cs[i].apBest, kBarTop, kBarH);
            g.drawFastHLine(x, y, kSlotW - 2, t.peak);
        }

        if (cursor) g.drawRect(x - 1, kBarTop - 1, kSlotW, kBarH + 2, t.accent);
    }

    // Channel numbers, with AP counts where the scan found any.
    g.setFont(&fonts::Font0);
    g.setTextDatum(top_center);
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        const int ch      = i + CHANNEL_MIN;
        const bool cursor = ch == _chCursor;
        g.setTextColor(cursor ? t.accent : (cs[i].usable ? t.textDim : t.grid), t.bg);
        char lbl[6];
        snprintf(lbl, sizeof(lbl), "%d", ch);
        g.drawString(lbl, slotX(i) + (kSlotW - 2) / 2, kLabelY);
    }
    g.setTextDatum(top_left);

    // Waterfall, newest row at the top.
    const int rows = (_wfRows < kWfH) ? _wfRows : kWfH;
    for (int r = 0; r < rows; r++) {
        for (int i = 0; i < CHANNEL_COUNT; i++) {
            const uint8_t v = _waterfall[r][i];
            const uint16_t c =
                v == 0 ? t.bg : ui::levelColor(static_cast<float>(v) / 255.0f);
            g.fillRect(slotX(i), kWfTop + r, kSlotW - 2, 1, c);
        }
    }

    char foot[48];
    const int idx = _chCursor - CHANNEL_MIN;
    if (idx >= 0 && idx < CHANNEL_COUNT && cs[idx].rssiPeak != RSSI_INVALID) {
        snprintf(foot, sizeof(foot), "ch%d %ddBm %lufr %dAP  ENTER", _chCursor, cs[idx].rssiPeak,
                 static_cast<unsigned long>(cs[idx].frames), cs[idx].apCount);
    } else {
        snprintf(foot, sizeof(foot), "ch%d quiet   , / move   ENTER detail", _chCursor);
    }
    ui::drawFooter(foot);
}

void App::keySpectrum(const KeyEvent& e)
{
    if (!isActionable(e)) return;
    switch (navFor(e)) {
        case Nav::Left:
            if (--_chCursor < CHANNEL_MIN) _chCursor = _sweeper.channelMax();
            break;
        case Nav::Right:
            if (++_chCursor > _sweeper.channelMax()) _chCursor = CHANNEL_MIN;
            break;
        case Nav::Select:
            _detailTop = 0;
            go(Screen::ChannelDetail);
            break;
        case Nav::Back:
            go(Screen::Menu);
            break;
        default:
            break;
    }
}

// --------------------------------------------------------- channel detail --

void App::drawChannelDetail()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();
    const int idx  = _chCursor - CHANNEL_MIN;

    char title[24];
    snprintf(title, sizeof(title), "CH %d  %d MHz", _chCursor, channelToFreqMhz(_chCursor));
    ui::drawHeader(title, nullptr);

    const ChannelStats* cs = _sweeper.channels();
    char line[52];
    g.setFont(&fonts::Font0);
    g.setTextColor(t.textDim, t.bg);

    if (idx >= 0 && idx < CHANNEL_COUNT) {
        snprintf(line, sizeof(line), "peak %d  avg %d  floor %d dBm",
                 cs[idx].rssiPeak == RSSI_INVALID ? 0 : cs[idx].rssiPeak,
                 cs[idx].rssiAvg == RSSI_INVALID ? 0 : cs[idx].rssiAvg,
                 cs[idx].rssiMin == RSSI_INVALID ? 0 : cs[idx].rssiMin);
        g.drawString(line, 4, BODY_Y + 2);
        snprintf(line, sizeof(line), "%lu frames  %lu bytes / dwell",
                 static_cast<unsigned long>(cs[idx].frames),
                 static_cast<unsigned long>(cs[idx].bytes));
        g.drawString(line, 4, BODY_Y + 11);
    }

    // APs whose own channel is this one.
    int matching = 0;
    for (int i : _apOrder) {
        if (_sweeper.aps()[static_cast<size_t>(i)].channel == _chCursor) matching++;
    }
    _detailCount = matching;
    if (_detailTop > matching - 1) _detailTop = matching - 1;
    if (_detailTop < 0) _detailTop = 0;

    int y     = BODY_Y + 23;
    int shown = 0;
    for (int i : _apOrder) {
        const ApInfo& ap = _sweeper.aps()[static_cast<size_t>(i)];
        if (ap.channel != _chCursor) continue;
        if (shown++ < _detailTop) continue;
        if (y > SCREEN_H - FOOTER_H - 10) break;

        char name[26];
        ssidLabel(ap, name, sizeof(name));
        g.setTextColor(t.text, t.bg);
        g.drawString(name, 4, y);

        g.setTextColor(t.textDim, t.bg);
        snprintf(line, sizeof(line), "%s", authModeName(ap.auth));
        g.setTextDatum(top_right);
        g.drawString(line, SCREEN_W - 46, y);
        g.setTextDatum(top_left);

        ui::drawSignalBar(SCREEN_W - 44, y, 26, 7, ap.rssi, true);
        snprintf(line, sizeof(line), "%d", ap.rssi);
        g.setTextDatum(top_right);
        g.setTextColor(ui::levelColor(ui::dbmLevel(ap.rssi)), t.bg);
        g.drawString(line, SCREEN_W - 2, y);
        g.setTextDatum(top_left);

        y += 10;
    }

    if (matching == 0) {
        g.setTextColor(t.textDim, t.bg);
        g.drawString(_sweeper.apScanInFlight() ? "scanning..." : "no beaconing APs here", 4, y);
    }

    ui::drawFooter("; . scroll   BKSP back");
}

void App::keyChannelDetail(const KeyEvent& e)
{
    if (!isActionable(e)) return;
    switch (navFor(e)) {
        case Nav::Up:
            if (_detailTop > 0) _detailTop--;
            break;
        case Nav::Down:
            if (_detailTop + 1 < _detailCount) _detailTop++;
            break;
        case Nav::Back:
            go(Screen::Spectrum);
            break;
        default:
            break;
    }
}

// --------------------------------------------------------------- networks --

void App::drawNetworks()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();

    char right[20];
    snprintf(right, sizeof(right), "%u found",
             static_cast<unsigned>(_sweeper.aps().size()));
    ui::drawHeader("SELECT NETWORK", right);

    const std::vector<int>& order = _apOrder;

    if (order.empty()) {
        g.setFont(&fonts::Font0);
        g.setTextColor(t.textDim, t.bg);
        g.drawString(_sweeper.apScanInFlight() ? "scanning the band..." : "no networks - press R",
                     6, BODY_Y + 20);
        ui::drawFooter("R rescan   BKSP back");
        return;
    }

    if (_netSel >= static_cast<int>(order.size())) _netSel = static_cast<int>(order.size()) - 1;
    if (_netSel < 0) _netSel = 0;

    constexpr int kRowH = 13;
    const int visible   = (SCREEN_H - FOOTER_H - BODY_Y) / kRowH;
    if (_netSel < _netTop) _netTop = _netSel;
    if (_netSel >= _netTop + visible) _netTop = _netSel - visible + 1;

    g.setFont(&fonts::Font0);
    for (int row = 0; row < visible; row++) {
        const int i = _netTop + row;
        if (i >= static_cast<int>(order.size())) break;

        const ApInfo& ap = _sweeper.aps()[static_cast<size_t>(order[static_cast<size_t>(i)])];
        const int y      = BODY_Y + 1 + row * kRowH;
        const bool on    = (i == _netSel);

        if (on) g.fillRect(1, y, SCREEN_W - 2, kRowH - 1, t.accent);

        char name[22];
        ssidLabel(ap, name, sizeof(name));
        g.setTextDatum(middle_left);
        g.setTextColor(on ? t.accentText : t.text, on ? t.accent : t.bg);
        g.drawString(name, 4, y + kRowH / 2 - 1);

        char meta[20];
        const size_t apIdx = static_cast<size_t>(order[static_cast<size_t>(i)]);
        snprintf(meta, sizeof(meta), "%s%s c%d",
                 (apIdx < _apSaved.size() && _apSaved[apIdx]) ? "* " : "",
                 authModeName(ap.auth), ap.channel);
        g.setTextColor(on ? t.accentText : t.textDim, on ? t.accent : t.bg);
        g.setTextDatum(middle_right);
        g.drawString(meta, SCREEN_W - 44, y + kRowH / 2 - 1);
        g.setTextDatum(top_left);

        ui::drawSignalBar(SCREEN_W - 42, y + 3, 24, 7, ap.rssi, true);
        char db[8];
        snprintf(db, sizeof(db), "%d", ap.rssi);
        g.setTextDatum(middle_right);
        g.setTextColor(on ? t.accentText : ui::levelColor(ui::dbmLevel(ap.rssi)),
                       on ? t.accent : t.bg);
        g.drawString(db, SCREEN_W - 2, y + kRowH / 2 - 1);
        g.setTextDatum(top_left);
    }

    ui::drawFooter("; . move  ENTER join  R rescan");
}

void App::keyNetworks(const KeyEvent& e)
{
    if (!isActionable(e)) return;

    if (e.code == 'r' || e.code == 'R') {
        _sweeper.requestApScan();
        _audio.click();
        return;
    }

    const std::vector<int>& order = _apOrder;

    switch (navFor(e)) {
        case Nav::Up:
            if (_netSel > 0) _netSel--;
            break;
        case Nav::Down:
            if (_netSel + 1 < static_cast<int>(order.size())) _netSel++;
            break;
        case Nav::Select:
            if (!order.empty()) beginConnectTo(order[static_cast<size_t>(_netSel)]);
            break;
        case Nav::Back:
            go(Screen::Menu);
            break;
        default:
            break;
    }
}

void App::beginConnectTo(int apIndex)
{
    if (apIndex < 0 || apIndex >= static_cast<int>(_sweeper.aps().size())) return;
    const ApInfo& ap = _sweeper.aps()[static_cast<size_t>(apIndex)];

    snprintf(_pwSsid, sizeof(_pwSsid), "%s", ap.ssid);
    _pwAuth  = ap.auth;
    _pwLen    = 0;
    _pwBuf[0] = '\0';
    _pwReveal = false;

    if (authModeIsOpen(ap.auth)) {
        _net.connect(_pwSsid, "", false);
        _trace.clear();
        _peak.reset();
        _smooth.reset();
        go(Screen::Meter);
        return;
    }

    // Pre-fill a saved password so a known network is one keypress away, but
    // still show the field: the user asked to be prompted.
    char saved[65];
    if (CredentialStore::load(_pwSsid, saved, sizeof(saved))) {
        snprintf(_pwBuf, sizeof(_pwBuf), "%s", saved);
        _pwLen = static_cast<int>(strlen(_pwBuf));
    }
    go(Screen::Password);
}

// --------------------------------------------------------------- password --

void App::drawPassword()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();
    ui::drawHeader("PASSWORD", authModeName(_pwAuth));

    g.setFont(&fonts::Font0);
    g.setTextColor(t.textDim, t.bg);
    g.drawString("network", 6, BODY_Y + 4);
    g.setTextColor(t.text, t.bg);
    g.drawString(_pwSsid, 6, BODY_Y + 14);

    const int fy = BODY_Y + 34;
    g.drawRect(5, fy, SCREEN_W - 10, 18, t.grid);

    char shown[66];
    if (_pwReveal) {
        snprintf(shown, sizeof(shown), "%s", _pwBuf);
    } else {
        const int n = (_pwLen > 30) ? 30 : _pwLen;
        for (int i = 0; i < n; i++) shown[i] = '*';
        shown[n] = '\0';
    }

    g.setTextColor(t.accent, t.bg);
    g.setTextDatum(middle_left);
    g.drawString(shown, 9, fy + 9);
    // Blinking caret.
    if ((millis() / 400) % 2 == 0) {
        const int cx = 9 + g.textWidth(shown);
        g.drawFastVLine(cx + 1, fy + 3, 12, t.accent);
    }
    g.setTextDatum(top_left);

    char info[44];
    snprintf(info, sizeof(info), "%d chars   TAB %s%s", _pwLen, _pwReveal ? "hide" : "show",
             _capsLock ? "   CAPS" : "");
    g.setTextColor(t.textDim, t.bg);
    g.drawString(info, 6, fy + 22);

    if (_pwLen > 0 && _pwLen < 8 && !authModeIsOpen(_pwAuth)) {
        g.setTextColor(t.warn, t.bg);
        g.drawString("WPA keys are 8+ characters", 6, fy + 32);
    }

    ui::drawFooter("ENTER join  Fn+TAB caps  ` cancel");
}

void App::keyPassword(const KeyEvent& e)
{
    if (!isActionable(e)) return;

    if (e.code == KEY_TAB) {
        if (e.mods.fn) {
            _capsLock = !_capsLock;
        } else {
            _pwReveal = !_pwReveal;
        }
        return;
    }
    if (e.code == '`') {
        go(Screen::Networks);
        return;
    }
    if (e.code == KEY_BACKSPACE) {
        if (_pwLen > 0) _pwBuf[--_pwLen] = '\0';
        return;
    }
    if (e.code == KEY_ENTER) {
        _net.connect(_pwSsid, _pwBuf, true);
        _apViewValid = false;  // the saved-credential marker may change
        _trace.clear();
        _peak.reset();
        _smooth.reset();
        _audio.confirm();
        go(Screen::Meter);
        return;
    }

    if (isPrintable(e) && _pwLen < static_cast<int>(sizeof(_pwBuf)) - 1) {
        _pwBuf[_pwLen++] = charFor(e, _capsLock);
        _pwBuf[_pwLen]   = '\0';
    }
}

// ------------------------------------------------------------------ meter --

void App::sampleMeter(uint32_t now)
{
    if (_net.state() != NetManager::State::Connected) return;
    if (now - _lastSampleMs < 40) return;
    _lastSampleMs = now;

    const int r = _net.rssi();
    if (r == 0 || r < -110) return;  // driver reports 0 when it has nothing

    const int8_t s = static_cast<int8_t>(r);
    _trace.push(s);
    _smooth.push(static_cast<float>(s));
    _peak.update(static_cast<float>(s), now);
    _audio.update(s, true, now);
}

void App::drawMeter()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();

    char right[24];
    if (_net.state() == NetManager::State::Connected) {
        snprintf(right, sizeof(right), "ch%d", _net.channel());
    } else {
        snprintf(right, sizeof(right), "--");
    }
    ui::drawHeader(_net.ssid()[0] ? _net.ssid() : "NOT CONNECTED", right);

    g.setFont(&fonts::Font0);

    if (_net.state() == NetManager::State::Idle) {
        g.setTextColor(t.textDim, t.bg);
        g.drawString("No network selected.", 8, BODY_Y + 24);
        g.drawString("Pick one from CONNECT first.", 8, BODY_Y + 36);
        ui::drawFooter("BKSP back");
        return;
    }

    if (_net.state() == NetManager::State::Connecting) {
        g.setTextColor(t.text, t.bg);
        g.drawString("associating...", 8, BODY_Y + 24);
        char s[32];
        snprintf(s, sizeof(s), "%lus", static_cast<unsigned long>(_net.connectElapsedMs() / 1000));
        g.setTextColor(t.textDim, t.bg);
        g.drawString(s, 8, BODY_Y + 36);
        ui::drawFooter("BKSP cancel");
        return;
    }

    if (_net.state() == NetManager::State::Failed) {
        g.setTextColor(t.warn, t.bg);
        g.drawString("connection failed", 8, BODY_Y + 24);
        g.setTextColor(t.textDim, t.bg);
        g.drawString(_net.errorText(), 8, BODY_Y + 36);
        ui::drawFooter("ENTER retry   BKSP back");
        return;
    }

    // --- connected --------------------------------------------------------
    const bool have = !_trace.empty();
    const int live  = have ? _trace.newest() : DBM_FLOOR;
    const int peak  = _peak.valid() ? static_cast<int>(_peak.value()) : DBM_FLOOR;

    // The needle is damped like a real analogue meter; the number beside it
    // stays raw so nothing is hidden from you.
    const int needle = _smooth.valid() ? static_cast<int>(_smooth.value()) : live;
    ui::drawGauge(50, 74, 34, needle, have, peak, _peak.valid());

    // Big reading.
    char buf[24];
    snprintf(buf, sizeof(buf), "%d", live);
    g.setFont(&fonts::Font4);
    g.setTextDatum(top_right);
    g.setTextColor(have ? ui::levelColor(ui::dbmLevel(live)) : t.textDim, t.bg);
    g.drawString(buf, 176, BODY_Y + 4);

    g.setFont(&fonts::Font0);
    g.setTextDatum(top_left);
    g.setTextColor(t.textDim, t.bg);
    g.drawString("dBm", 179, BODY_Y + 16);

    snprintf(buf, sizeof(buf), "%d%%", rssiToQuality(live));
    g.setFont(&fonts::Font2);
    g.setTextDatum(top_right);
    g.setTextColor(t.text, t.bg);
    g.drawString(buf, SCREEN_W - 4, BODY_Y + 28);
    g.setTextDatum(top_left);

    g.setFont(&fonts::Font0);
    g.setTextColor(t.textDim, t.bg);
    if (have) {
        snprintf(buf, sizeof(buf), "min %d  max %d", _trace.min(), _trace.max());
        g.drawString(buf, 100, BODY_Y + 46);
        snprintf(buf, sizeof(buf), "avg %.0f  jit %.1f", _trace.avg(), _trace.jitter());
        g.drawString(buf, 100, BODY_Y + 56);
    } else {
        g.drawString("waiting for samples", 100, BODY_Y + 46);
    }

    if (_settings.localSniff) {
        const int idx = _net.channel() - CHANNEL_MIN;
        if (idx >= 0 && idx < CHANNEL_COUNT) {
            snprintf(buf, sizeof(buf), "ch load %lu fr",
                     static_cast<unsigned long>(_sweeper.channels()[idx].frames));
            g.drawString(buf, 100, BODY_Y + 66);
        }
    }

    // Trace along the bottom.
    const int ty = SCREEN_H - FOOTER_H - 34;
    ui::drawTraceBox(2, ty, SCREEN_W - 4, 32);
    const size_t n = _trace.copyTo(_scratch, METER_TRACE_LEN);
    ui::drawTrace(2, ty, SCREEN_W - 4, 32, _scratch, static_cast<int>(n), 1);

    ui::drawFooter(_audio.enabled() ? "A audio off   BKSP back" : "A audio on   BKSP back");
}

void App::keyMeter(const KeyEvent& e)
{
    if (!isActionable(e)) return;

    if (e.code == 'a' || e.code == 'A') {
        _settings.audioEnabled = !_settings.audioEnabled;
        _audio.setEnabled(_settings.audioEnabled);
        _settings.save();
        return;
    }

    switch (navFor(e)) {
        case Nav::Select:
            if (_net.state() == NetManager::State::Failed) go(Screen::Networks);
            break;
        case Nav::Back:
            _audio.silence();
            go(Screen::Menu);
            break;
        default:
            break;
    }
}

}  // namespace rfscope
