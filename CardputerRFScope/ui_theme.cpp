#include "ui_theme.h"

namespace rfscope {
namespace {

// RGB565 helper usable in constant expressions.
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Phosphor green on black, the way a bench analyser looks.
constexpr Theme kRetro = {
    ThemeId::Retro,
    "RETRO",
    rgb(0, 0, 0),        // bg
    rgb(0, 48, 24),      // grid
    rgb(0, 40, 20),      // headerBg
    rgb(120, 255, 160),  // headerFg
    rgb(0, 150, 90),     // footerFg
    rgb(60, 255, 120),   // text
    rgb(0, 130, 70),     // textDim
    rgb(180, 255, 60),   // accent
    rgb(0, 0, 0),        // accentText
    rgb(255, 240, 120),  // peak
    rgb(255, 160, 40),   // warn
    rgb(60, 255, 120),   // good
    {rgb(0, 60, 30), rgb(0, 110, 55), rgb(0, 170, 80), rgb(40, 220, 110), rgb(120, 255, 150),
     rgb(220, 255, 200)},
};

// Thermal gradient, closer to a modern SDR waterfall.
constexpr Theme kHeat = {
    ThemeId::Heat,
    "HEAT",
    rgb(6, 8, 16),       // bg
    rgb(30, 36, 56),     // grid
    rgb(20, 26, 48),     // headerBg
    rgb(200, 220, 255),  // headerFg
    rgb(110, 130, 175),  // footerFg
    rgb(226, 232, 245),  // text
    rgb(120, 134, 165),  // textDim
    rgb(90, 170, 255),   // accent
    rgb(6, 8, 16),       // accentText
    rgb(255, 255, 255),  // peak
    rgb(255, 170, 60),   // warn
    rgb(80, 220, 140),   // good
    {rgb(10, 20, 60), rgb(20, 90, 170), rgb(30, 180, 190), rgb(230, 220, 70), rgb(240, 140, 40),
     rgb(240, 50, 50)},
};

ThemeId g_current = ThemeId::Retro;

}  // namespace

const Theme& themeGet(ThemeId id)
{
    return id == ThemeId::Heat ? kHeat : kRetro;
}

const Theme& theme()
{
    return themeGet(g_current);
}

void themeSet(ThemeId id)
{
    g_current = id;
}

ThemeId themeCurrent()
{
    return g_current;
}

uint16_t themeRamp(float level01)
{
    const Theme& t = theme();
    if (level01 <= 0.0f) return t.ramp[0];
    if (level01 >= 1.0f) return t.ramp[5];
    int idx = static_cast<int>(level01 * 5.999f);
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    return t.ramp[idx];
}

}  // namespace rfscope
