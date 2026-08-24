// Build-wide tuning constants.
#pragma once

#include <cstdint>

namespace rfscope {

constexpr const char* APP_NAME    = "RFSCOPE";
constexpr const char* APP_VERSION = "0.1.0";

// The Cardputer / Cardputer ADV panel in landscape.
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;

// Chrome heights carved out of the canvas.
constexpr int HEADER_H = 14;
constexpr int FOOTER_H = 11;
constexpr int BODY_Y   = HEADER_H;
constexpr int BODY_H   = SCREEN_H - HEADER_H - FOOTER_H;

// Sweep engine.
constexpr uint32_t DWELL_MIN_MS     = 40;
constexpr uint32_t DWELL_MAX_MS     = 400;
constexpr uint32_t DWELL_DEFAULT_MS = 120;
// How often the sweep pauses to run an AP scan so the bars can be labelled.
constexpr uint32_t AP_SCAN_INTERVAL_MS = 8000;
constexpr int MAX_APS                  = 48;

// Waterfall history depth, in completed sweeps.
constexpr int WATERFALL_ROWS = 48;

// Meter trace depth. One sample per UI frame at ~25 fps is ~7 s of history,
// which is about how long it takes to walk a room.
constexpr int METER_TRACE_LEN = 180;

// BLE.
constexpr int MAX_BLE_DEVICES        = 40;
constexpr uint32_t BLE_DEVICE_TTL_MS = 20000;

// Display floor/ceiling for every dBm axis in the UI.
constexpr int DBM_FLOOR   = -100;
constexpr int DBM_CEILING = -25;

}  // namespace rfscope
