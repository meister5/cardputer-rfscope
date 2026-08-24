// Screen state machine and the glue that owns every subsystem.
#pragma once

#include <cstdint>
#include <vector>

#include "audio_tone.h"
#include "input.h"
#include "net_manager.h"
#include "radio_ble.h"
#include "radio_wifi.h"
#include "rfscope_config.h"
#include "signal_stats.h"
#include "storage.h"

namespace rfscope {

enum class Screen : uint8_t {
    Menu,
    Spectrum,
    ChannelDetail,
    Networks,
    Password,
    Meter,
    BleList,
    BleTracker,
    Band,
    Settings,
    Diag,
    About,
};

class App {
public:
    void begin();
    void loop();

private:
    // --- lifecycle -------------------------------------------------------
    void go(Screen s);
    void applyRadioForScreen(Screen s);
    void applySettings();

    void handleKey(const KeyEvent& e);
    void draw();

    // --- screens (app.cpp) ----------------------------------------------
    void drawMenu();
    void keyMenu(const KeyEvent& e);
    void drawSettings();
    void keySettings(const KeyEvent& e);
    void drawDiag();
    void keyDiag(const KeyEvent& e);
    void drawAbout();

    // --- screens (screens_wifi.cpp) -------------------------------------
    void drawSpectrum();
    void keySpectrum(const KeyEvent& e);
    void drawChannelDetail();
    void keyChannelDetail(const KeyEvent& e);
    void drawNetworks();
    void keyNetworks(const KeyEvent& e);
    void drawPassword();
    void keyPassword(const KeyEvent& e);
    void drawMeter();
    void keyMeter(const KeyEvent& e);

    void beginConnectTo(int apIndex);
    // Rebuilds the sorted AP order and the saved-credential flags, but only
    // when the sweeper has actually published a new scan.
    void refreshApView();
    void refreshBleOrder();
    void sampleMeter(uint32_t now);
    void pushWaterfallRow();

    // --- screens (screens_ble.cpp) --------------------------------------
    void drawBleList();
    void keyBleList(const KeyEvent& e);
    void drawBleTracker();
    void keyBleTracker(const KeyEvent& e);
    void drawBand();
    void keyBand(const KeyEvent& e);

    void sampleBleTracker(uint32_t now);
    void ensureBle();

    // --- subsystems ------------------------------------------------------
    Input _input;
    WifiSweeper _sweeper;
    BleScanner _ble;
    NetManager _net;
    SignalAudio _audio;
    Settings _settings;

    std::vector<KeyEvent> _events;

    // Cached derived views. Sorting and NVS lookups are far too expensive to
    // redo on every frame.
    std::vector<int> _apOrder;
    std::vector<uint8_t> _apSaved;  // parallel to _sweeper.aps()
    uint32_t _apViewGen = 0;
    bool _apViewValid   = false;
    std::vector<int> _bleOrder;

    // --- navigation ------------------------------------------------------
    Screen _screen = Screen::Menu;
    int _menuSel   = 0;
    int _setSel    = 0;
    int _netSel    = 0;
    int _netTop    = 0;
    int _bleSel    = 0;
    int _bleTop    = 0;
    int _chCursor  = 6;
    int _detailTop = 0;
    bool _capsLock = false;

    // --- password entry --------------------------------------------------
    char _pwSsid[33] = {0};
    char _pwBuf[65]  = {0};
    int _pwLen       = 0;
    uint8_t _pwAuth  = 0;
    bool _pwReveal   = false;

    // --- meter -----------------------------------------------------------
    RssiTrace<METER_TRACE_LEN> _trace;
    PeakHold _peak{8.0f};
    Ewma _smooth{0.30f};
    uint32_t _lastSampleMs = 0;
    int8_t _scratch[METER_TRACE_LEN] = {0};

    // --- BLE tracker ------------------------------------------------------
    uint8_t _trackAddr[6] = {0};
    bool _tracking        = false;
    RssiTrace<METER_TRACE_LEN> _bleTrace;
    PeakHold _blePeak{8.0f};

    // --- waterfall (row 0 is the newest sweep) ---------------------------
    uint8_t _waterfall[WATERFALL_ROWS][CHANNEL_COUNT] = {};
    int _wfRows = 0;

    // --- diagnostics -------------------------------------------------------
    uint8_t _lastKeyCode = 0;
    uint32_t _frameMs    = 0;
    uint32_t _fps        = 0;
    uint32_t _fpsMark    = 0;
    uint32_t _fpsCount   = 0;
};

}  // namespace rfscope
