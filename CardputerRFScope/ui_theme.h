// Two interchangeable palettes: retro test-equipment phosphor, and a modern
// thermal heat-map. Everything the UI draws pulls its colours from here so a
// theme switch never needs a screen to know which theme is active.
#pragma once

#include <cstdint>

namespace rfscope {

enum class ThemeId : uint8_t { Retro = 0, Heat = 1 };

struct Theme {
    ThemeId id;
    const char* name;

    uint16_t bg;
    uint16_t grid;
    uint16_t headerBg;
    uint16_t headerFg;
    uint16_t footerFg;
    uint16_t text;
    uint16_t textDim;
    uint16_t accent;      // selection / cursor
    uint16_t accentText;  // text drawn on top of accent
    uint16_t peak;        // peak-hold trace
    uint16_t warn;
    uint16_t good;

    // Signal-strength ramp, weakest first. Bars, traces and the waterfall all
    // colour themselves by sampling this.
    uint16_t ramp[6];
};

const Theme& themeGet(ThemeId id);
const Theme& theme();          // currently active
void themeSet(ThemeId id);
ThemeId themeCurrent();

// Map a level in 0..1 onto the active theme's ramp.
uint16_t themeRamp(float level01);

}  // namespace rfscope
