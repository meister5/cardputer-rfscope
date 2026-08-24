// Shared drawing surface and the widgets more than one screen needs.
#pragma once

#include <M5Cardputer.h>

#include <cstdint>

#include "rfscope_config.h"
#include "ui_theme.h"

namespace rfscope {
namespace ui {

// Creates the off-screen canvas. Returns false if there was not enough heap,
// in which case drawing falls through to the panel directly (no tearing
// protection, but the app still runs).
bool begin();
bool doubleBuffered();

// The surface every screen draws on. M5GFX (the panel) and M5Canvas (the
// off-screen sprite) share LovyanGFX as their base, so screens are written
// once and work with or without the buffer.
lgfx::LovyanGFX& gfx();

// Blit to the panel (a no-op when not double buffered).
void present();

void clearBody();
void drawHeader(const char* title, const char* right);
void drawFooter(const char* hints);

// Map dBm onto 0..1 across the UI's fixed display window.
float dbmLevel(int dbm);
int dbmToY(int dbm, int top, int height);

// Colour for a signal level, sampled from the active theme ramp.
uint16_t levelColor(float level01);

// A horizontal strength bar with a dBm-derived fill.
void drawSignalBar(int x, int y, int w, int h, int dbm, bool valid);

// The analogue-style arc gauge used by both the WiFi meter and the BLE
// tracker: -100 dBm at the left stop, -25 dBm at the right.
void drawGauge(int cx, int cy, int radius, int dbm, bool valid, int peakDbm, bool peakValid);

// Scrolling dBm-vs-time trace. `sample(i)` returns the i-th oldest sample.
void drawTraceBox(int x, int y, int w, int h);
void drawTrace(int x, int y, int w, int h, const int8_t* samples, int count, int stride);

// Centre-aligned helpers.
void textCentered(const char* s, int cx, int y, uint16_t color, const lgfx::IFont* font);

}  // namespace ui
}  // namespace rfscope
