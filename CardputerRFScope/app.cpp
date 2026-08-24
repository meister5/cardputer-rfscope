#include "app.h"

#include <M5Cardputer.h>

#include <cstdio>
#include <cstring>

#include "ui_theme.h"
#include "ui_widgets.h"

namespace rfscope {
namespace {

struct MenuItem {
    const char* label;
    const char* blurb;
    Screen target;
};

const MenuItem kMenu[] = {
    {"SPECTRUM", "2.4GHz channel sweep", Screen::Spectrum},
    {"CONNECT", "join a network", Screen::Networks},
    {"METER", "live RSSI of the link", Screen::Meter},
    {"BLE SCAN", "advertising devices", Screen::BleList},
    {"BAND", "wifi + ble together", Screen::Band},
    {"SETTINGS", "theme, dwell, audio", Screen::Settings},
    {"DIAG", "board self-test", Screen::Diag},
    {"ABOUT", "what this can measure", Screen::About},
};
constexpr int kMenuCount = static_cast<int>(sizeof(kMenu) / sizeof(kMenu[0]));

constexpr int kSettingCount = 7;

const char* boardName()
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5CardputerADV:
            return "Cardputer ADV";
        case m5::board_t::board_M5Cardputer:
            return "Cardputer";
        default:
            return "unknown board";
    }
}

}  // namespace

void App::begin()
{
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    ui::begin();
    _settings.load();
    applySettings();

    _input.begin();
    _audio.begin();
    _net.begin();
    _sweeper.begin();
    _sweeper.setApScanEnabled(_settings.apScanEnabled);
    _sweeper.setDwellMs(_settings.dwellMs);

    _events.reserve(8);
    _fpsMark = millis();
    go(Screen::Menu);
}

void App::applySettings()
{
    themeSet(_settings.theme);
    M5Cardputer.Display.setBrightness(_settings.brightness);
    _audio.setEnabled(_settings.audioEnabled);
    _sweeper.setDwellMs(_settings.dwellMs);
    _sweeper.setApScanEnabled(_settings.apScanEnabled);
}

void App::go(Screen s)
{
    _screen = s;
    _input.resync();  // a key held across a transition must not leak through
    _audio.silence();
    applyRadioForScreen(s);
}

void App::applyRadioForScreen(Screen s)
{
    switch (s) {
        case Screen::Spectrum:
        case Screen::ChannelDetail:
            _sweeper.unlock();
            _sweeper.startSweep();
            break;

        case Screen::Band:
            _sweeper.unlock();
            _sweeper.startSweep();
            ensureBle();
            break;

        case Screen::Networks:
            _sweeper.stopSweep();
            _sweeper.requestApScan();
            _apViewValid = false;  // re-check saved credentials on entry
            break;

        case Screen::Meter:
            // Associated: the radio cannot leave the AP's channel, so the
            // sniffer is pinned there (and only if the user allows it).
            if (_net.state() == NetManager::State::Connected && _settings.localSniff) {
                _sweeper.lockChannel(_net.channel());
                _sweeper.startSweep();
            } else {
                _sweeper.stopSweep();
            }
            break;

        case Screen::BleList:
        case Screen::BleTracker:
            _sweeper.stopSweep();
            ensureBle();
            break;

        case Screen::Password:
        case Screen::Menu:
        case Screen::Settings:
        case Screen::Diag:
        case Screen::About:
            _sweeper.stopSweep();
            break;
    }
}

void App::ensureBle()
{
    if (!_ble.available()) _ble.begin();
    if (_ble.available() && !_ble.running()) _ble.start();
}

void App::loop()
{
    const uint32_t now = millis();

    _events.clear();
    _input.poll(_events);
    for (const auto& e : _events) {
        if (isActionable(e)) _lastKeyCode = e.code;
        handleKey(e);
    }

    _sweeper.loop();
    _net.loop();
    if (_ble.available()) _ble.loop();

    // The meter is entered while still associating, so the decision about
    // sniffing the local channel has to be re-made once the link comes up
    // (and undone if it drops).
    if (_screen == Screen::Meter) {
        const bool linked = _net.state() == NetManager::State::Connected;
        if (linked && _settings.localSniff && !_sweeper.sweeping()) {
            _sweeper.lockChannel(_net.channel());
            _sweeper.startSweep();
        } else if ((!linked || !_settings.localSniff) && _sweeper.sweeping()) {
            _sweeper.stopSweep();
        }
    }

    if (_sweeper.consumeSweepTick()) pushWaterfallRow();

    refreshApView();
    if (_screen == Screen::BleList) refreshBleOrder();

    if (_screen == Screen::Meter) sampleMeter(now);
    if (_screen == Screen::BleTracker) sampleBleTracker(now);

    // ~25 fps is plenty for this data and leaves the radio tasks room.
    if (now - _frameMs < 40) return;
    _frameMs = now;

    draw();

    _fpsCount++;
    if (now - _fpsMark >= 1000) {
        _fps      = _fpsCount * 1000 / (now - _fpsMark);
        _fpsCount = 0;
        _fpsMark  = now;
    }
}

void App::handleKey(const KeyEvent& e)
{
    switch (_screen) {
        case Screen::Menu:
            keyMenu(e);
            break;
        case Screen::Spectrum:
            keySpectrum(e);
            break;
        case Screen::ChannelDetail:
            keyChannelDetail(e);
            break;
        case Screen::Networks:
            keyNetworks(e);
            break;
        case Screen::Password:
            keyPassword(e);
            break;
        case Screen::Meter:
            keyMeter(e);
            break;
        case Screen::BleList:
            keyBleList(e);
            break;
        case Screen::BleTracker:
            keyBleTracker(e);
            break;
        case Screen::Band:
            keyBand(e);
            break;
        case Screen::Settings:
            keySettings(e);
            break;
        case Screen::Diag:
            keyDiag(e);
            break;
        case Screen::About:
            if (isActionable(e) && navFor(e) != Nav::None) go(Screen::Menu);
            break;
    }
}

void App::draw()
{
    ui::gfx().fillScreen(theme().bg);

    switch (_screen) {
        case Screen::Menu:
            drawMenu();
            break;
        case Screen::Spectrum:
            drawSpectrum();
            break;
        case Screen::ChannelDetail:
            drawChannelDetail();
            break;
        case Screen::Networks:
            drawNetworks();
            break;
        case Screen::Password:
            drawPassword();
            break;
        case Screen::Meter:
            drawMeter();
            break;
        case Screen::BleList:
            drawBleList();
            break;
        case Screen::BleTracker:
            drawBleTracker();
            break;
        case Screen::Band:
            drawBand();
            break;
        case Screen::Settings:
            drawSettings();
            break;
        case Screen::Diag:
            drawDiag();
            break;
        case Screen::About:
            drawAbout();
            break;
    }

    ui::present();
}

// ---------------------------------------------------------------- menu ----

void App::drawMenu()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();

    char right[24];
    snprintf(right, sizeof(right), "v%s", APP_VERSION);
    ui::drawHeader(APP_NAME, right);

    const int rowH = 12;
    const int top  = BODY_Y + 2;

    g.setFont(&fonts::Font0);
    for (int i = 0; i < kMenuCount; i++) {
        const int y   = top + i * rowH;
        const bool on = (i == _menuSel);

        if (on) g.fillRect(2, y - 1, SCREEN_W - 4, rowH, t.accent);
        g.setTextDatum(middle_left);
        g.setTextColor(on ? t.accentText : t.text, on ? t.accent : t.bg);
        g.drawString(kMenu[i].label, 8, y + rowH / 2 - 1);
        g.setTextColor(on ? t.accentText : t.textDim, on ? t.accent : t.bg);
        g.drawString(kMenu[i].blurb, 78, y + rowH / 2 - 1);
    }
    g.setTextDatum(top_left);

    ui::drawFooter("; . move   ENTER select");
}

void App::keyMenu(const KeyEvent& e)
{
    if (!isActionable(e)) return;

    switch (navFor(e)) {
        case Nav::Up:
            _menuSel = (_menuSel + kMenuCount - 1) % kMenuCount;
            _audio.click();
            break;
        case Nav::Down:
            _menuSel = (_menuSel + 1) % kMenuCount;
            _audio.click();
            break;
        case Nav::Select:
            go(kMenu[_menuSel].target);
            break;
        default:
            break;
    }
}

// ------------------------------------------------------------ settings ----

void App::drawSettings()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();
    ui::drawHeader("SETTINGS", nullptr);

    char values[kSettingCount][20];
    const char* labels[kSettingCount] = {
        "Theme", "Sweep dwell", "Audio", "Brightness", "AP overlay", "Sniff while linked",
        "Forget networks",
    };

    snprintf(values[0], 20, "%s", themeGet(_settings.theme).name);
    snprintf(values[1], 20, "%lu ms", static_cast<unsigned long>(_settings.dwellMs));
    snprintf(values[2], 20, "%s", _settings.audioEnabled ? "ON" : "OFF");
    snprintf(values[3], 20, "%u", static_cast<unsigned>(_settings.brightness));
    snprintf(values[4], 20, "%s", _settings.apScanEnabled ? "ON" : "OFF");
    snprintf(values[5], 20, "%s", _settings.localSniff ? "ON" : "OFF");
    snprintf(values[6], 20, "%s", "press ENTER");

    const int rowH = 13;
    const int top  = BODY_Y + 3;

    g.setFont(&fonts::Font0);
    for (int i = 0; i < kSettingCount; i++) {
        const int y   = top + i * rowH;
        const bool on = (i == _setSel);
        if (on) g.fillRect(2, y - 1, SCREEN_W - 4, rowH, t.accent);
        g.setTextDatum(middle_left);
        g.setTextColor(on ? t.accentText : t.text, on ? t.accent : t.bg);
        g.drawString(labels[i], 6, y + rowH / 2 - 1);
        g.setTextDatum(middle_right);
        g.drawString(values[i], SCREEN_W - 6, y + rowH / 2 - 1);
    }
    g.setTextDatum(top_left);

    ui::drawFooter(", / adjust   BKSP back");
}

void App::keySettings(const KeyEvent& e)
{
    if (!isActionable(e)) return;
    const Nav n = navFor(e);
    int delta   = 0;

    switch (n) {
        case Nav::Up:
            _setSel = (_setSel + kSettingCount - 1) % kSettingCount;
            return;
        case Nav::Down:
            _setSel = (_setSel + 1) % kSettingCount;
            return;
        case Nav::Back:
            _settings.save();
            go(Screen::Menu);
            return;
        case Nav::Left:
            delta = -1;
            break;
        case Nav::Right:
            delta = 1;
            break;
        case Nav::Select:
            delta = 1;
            break;
        default:
            return;
    }

    switch (_setSel) {
        case 0:
            _settings.theme =
                (_settings.theme == ThemeId::Retro) ? ThemeId::Heat : ThemeId::Retro;
            break;
        case 1: {
            long d = static_cast<long>(_settings.dwellMs) + delta * 20;
            if (d < static_cast<long>(DWELL_MIN_MS)) d = DWELL_MIN_MS;
            if (d > static_cast<long>(DWELL_MAX_MS)) d = DWELL_MAX_MS;
            _settings.dwellMs = static_cast<uint32_t>(d);
            break;
        }
        case 2:
            _settings.audioEnabled = !_settings.audioEnabled;
            break;
        case 3: {
            int b = _settings.brightness + delta * 15;
            if (b < 10) b = 10;
            if (b > 255) b = 255;
            _settings.brightness = static_cast<uint8_t>(b);
            break;
        }
        case 4:
            _settings.apScanEnabled = !_settings.apScanEnabled;
            break;
        case 5:
            _settings.localSniff = !_settings.localSniff;
            break;
        case 6:
            if (n == Nav::Select) {
                CredentialStore::forgetAll();
                _audio.confirm();
            }
            break;
        default:
            break;
    }

    applySettings();
    _settings.save();
}

// ---------------------------------------------------------- diagnostics ----

void App::drawDiag()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();
    ui::drawHeader("DIAGNOSTICS", nullptr);

    char line[48];
    int y = BODY_Y + 3;
    g.setFont(&fonts::Font0);
    g.setTextDatum(top_left);

    auto row = [&](const char* label, const char* value, uint16_t color) {
        g.setTextColor(t.textDim, t.bg);
        g.drawString(label, 5, y);
        g.setTextColor(color, t.bg);
        g.drawString(value, 106, y);
        y += 10;
    };

    row("board", boardName(), M5.getBoard() == m5::board_t::board_M5CardputerADV ? t.good : t.text);
    row("keyboard", _input.driverName(), t.text);

    snprintf(line, sizeof(line), "%dx%d rot%d", static_cast<int>(M5Cardputer.Display.width()),
             static_cast<int>(M5Cardputer.Display.height()),
             static_cast<int>(M5Cardputer.Display.getRotation()));
    row("display", line, t.text);

    row("canvas", ui::doubleBuffered() ? "buffered" : "DIRECT (low heap)",
        ui::doubleBuffered() ? t.good : t.warn);

    snprintf(line, sizeof(line), "%u KB free", static_cast<unsigned>(ESP.getFreeHeap() / 1024));
    row("heap", line, t.text);

    snprintf(line, sizeof(line), "ch1-%d  %lu frames", _sweeper.channelMax(),
             static_cast<unsigned long>(_sweeper.totalFrames()));
    row("wifi", line, t.text);

    snprintf(line, sizeof(line), "%s  %lu adv/s", _ble.available() ? "ready" : "not started",
             static_cast<unsigned long>(_ble.advRate()));
    row("ble", line, t.text);

    snprintf(line, sizeof(line), "%lu adv", static_cast<unsigned long>(_ble.dropped()));
    row("dropped", line, _ble.dropped() ? t.warn : t.textDim);

    if (_lastKeyCode == 0) {
        snprintf(line, sizeof(line), "press any key");
    } else if (_lastKeyCode >= 0x20 && _lastKeyCode < 0x7F) {
        snprintf(line, sizeof(line), "0x%02X  '%c'", _lastKeyCode, static_cast<char>(_lastKeyCode));
    } else {
        snprintf(line, sizeof(line), "0x%02X", _lastKeyCode);
    }
    row("last key", line, t.accent);

    snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(_fps));
    row("fps", line, t.text);

    ui::drawFooter("type to test keys   ` to exit");
}

void App::keyDiag(const KeyEvent& e)
{
    if (!isActionable(e)) return;
    // Backspace is the key under test as well as the way out, so require the
    // grave key to leave and let every other key echo.
    if (e.code == '`') go(Screen::Menu);
}

// ---------------------------------------------------------------- about ----

void App::drawAbout()
{
    auto& g        = ui::gfx();
    const Theme& t = theme();
    ui::drawHeader("ABOUT", APP_VERSION);

    g.setFont(&fonts::Font0);
    g.setTextDatum(top_left);
    int y = BODY_Y + 3;

    auto para = [&](const char* s, uint16_t c) {
        g.setTextColor(c, t.bg);
        g.drawString(s, 5, y);
        y += 10;
    };

    para("RFSCOPE - Cardputer ADV", t.text);
    para("", t.text);
    para("The ESP32-S3 cannot sweep raw RF.", t.textDim);
    para("Bars are per-channel frame counts", t.textDim);
    para("and RSSI from monitor mode, so the", t.textDim);
    para("resolution is 14 channels, not MHz.", t.textDim);
    para("", t.text);
    para("Bluetooth is BLE only (no Classic)", t.textDim);
    para("and only the 3 advert channels.", t.textDim);

    ui::drawFooter("any key to go back");
}

}  // namespace rfscope
