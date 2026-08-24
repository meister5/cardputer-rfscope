#include "ui_widgets.h"

#include <cmath>

#include "signal_stats.h"

namespace rfscope {
namespace ui {
namespace {

M5Canvas g_canvas(&M5Cardputer.Display);
bool g_buffered = false;

}  // namespace

bool begin()
{
    M5Cardputer.Display.setRotation(1);
    g_canvas.setColorDepth(16);
    g_buffered = (g_canvas.createSprite(SCREEN_W, SCREEN_H) != nullptr);
    if (g_buffered) {
        g_canvas.setTextWrap(false);
    }
    M5Cardputer.Display.setTextWrap(false);
    return g_buffered;
}

bool doubleBuffered()
{
    return g_buffered;
}

lgfx::LovyanGFX& gfx()
{
    if (g_buffered) return g_canvas;
    return M5Cardputer.Display;
}

void present()
{
    if (g_buffered) g_canvas.pushSprite(0, 0);
}

void clearBody()
{
    gfx().fillRect(0, BODY_Y, SCREEN_W, BODY_H, theme().bg);
}

void drawHeader(const char* title, const char* right)
{
    auto& g        = gfx();
    const Theme& t = theme();

    g.fillRect(0, 0, SCREEN_W, HEADER_H, t.headerBg);
    g.drawFastHLine(0, HEADER_H - 1, SCREEN_W, t.grid);

    g.setFont(&fonts::Font0);
    g.setTextDatum(middle_left);
    g.setTextColor(t.headerFg, t.headerBg);
    g.drawString(title, 3, HEADER_H / 2 - 1);

    if (right && *right) {
        g.setTextDatum(middle_right);
        g.setTextColor(t.footerFg, t.headerBg);
        g.drawString(right, SCREEN_W - 3, HEADER_H / 2 - 1);
    }
    g.setTextDatum(top_left);
}

void drawFooter(const char* hints)
{
    auto& g        = gfx();
    const Theme& t = theme();
    const int y    = SCREEN_H - FOOTER_H;

    g.fillRect(0, y, SCREEN_W, FOOTER_H, t.headerBg);
    g.drawFastHLine(0, y, SCREEN_W, t.grid);
    g.setFont(&fonts::Font0);
    g.setTextDatum(middle_left);
    g.setTextColor(t.footerFg, t.headerBg);
    g.drawString(hints, 3, y + FOOTER_H / 2);
    g.setTextDatum(top_left);
}

float dbmLevel(int dbm)
{
    if (dbm <= DBM_FLOOR) return 0.0f;
    if (dbm >= DBM_CEILING) return 1.0f;
    return static_cast<float>(dbm - DBM_FLOOR) / static_cast<float>(DBM_CEILING - DBM_FLOOR);
}

int dbmToY(int dbm, int top, int height)
{
    const float l = dbmLevel(dbm);
    return top + height - 1 - static_cast<int>(l * static_cast<float>(height - 1));
}

uint16_t levelColor(float level01)
{
    return themeRamp(level01);
}

void drawSignalBar(int x, int y, int w, int h, int dbm, bool valid)
{
    auto& g        = gfx();
    const Theme& t = theme();

    g.drawRect(x, y, w, h, t.grid);
    if (!valid) return;

    const float l  = dbmLevel(dbm);
    const int fill = static_cast<int>(l * static_cast<float>(w - 2));
    if (fill > 0) g.fillRect(x + 1, y + 1, fill, h - 2, levelColor(l));
}

void drawGauge(int cx, int cy, int radius, int dbm, bool valid, int peakDbm, bool peakValid)
{
    auto& g        = gfx();
    const Theme& t = theme();

    // 210 deg sweep, opening downward, drawn with M5GFX's screen angles
    // (0 = 3 o'clock, growing clockwise).
    constexpr float kStart = 165.0f;
    constexpr float kEnd   = 375.0f;

    // Coloured scale arc, drawn in segments so it carries the ramp.
    constexpr int kSegs = 24;
    for (int i = 0; i < kSegs; i++) {
        const float a0 = kStart + (kEnd - kStart) * (static_cast<float>(i) / kSegs);
        const float a1 = kStart + (kEnd - kStart) * (static_cast<float>(i + 1) / kSegs);
        const uint16_t c =
            levelColor(static_cast<float>(i) / static_cast<float>(kSegs - 1));
        g.fillArc(cx, cy, radius, radius - 4, a0, a1 + 0.5f, c);
    }

    // Tick marks every 25 dB.
    g.setFont(&fonts::Font0);
    g.setTextDatum(middle_center);
    g.setTextColor(t.textDim, t.bg);
    for (int d = DBM_FLOOR; d <= DBM_CEILING; d += 25) {
        const float frac = dbmLevel(d);
        const float a    = (kStart + (kEnd - kStart) * frac) * static_cast<float>(M_PI) / 180.0f;
        const int x0     = cx + static_cast<int>(std::cos(a) * (radius - 5));
        const int y0     = cy + static_cast<int>(std::sin(a) * (radius - 5));
        const int x1     = cx + static_cast<int>(std::cos(a) * (radius - 10));
        const int y1     = cy + static_cast<int>(std::sin(a) * (radius - 10));
        g.drawLine(x0, y0, x1, y1, t.textDim);
    }

    // Peak-hold marker.
    if (peakValid) {
        const float a =
            (kStart + (kEnd - kStart) * dbmLevel(peakDbm)) * static_cast<float>(M_PI) / 180.0f;
        const int x0 = cx + static_cast<int>(std::cos(a) * (radius - 1));
        const int y0 = cy + static_cast<int>(std::sin(a) * (radius - 1));
        const int x1 = cx + static_cast<int>(std::cos(a) * (radius - 12));
        const int y1 = cy + static_cast<int>(std::sin(a) * (radius - 12));
        g.drawLine(x0, y0, x1, y1, t.peak);
    }

    // The needle.
    if (valid) {
        const float a =
            (kStart + (kEnd - kStart) * dbmLevel(dbm)) * static_cast<float>(M_PI) / 180.0f;
        const int nx = cx + static_cast<int>(std::cos(a) * (radius - 7));
        const int ny = cy + static_cast<int>(std::sin(a) * (radius - 7));
        g.drawLine(cx, cy, nx, ny, t.text);
        g.drawLine(cx, cy - 1, nx, ny, t.text);
    }
    g.fillCircle(cx, cy, 3, t.text);
    g.setTextDatum(top_left);
}

void drawTraceBox(int x, int y, int w, int h)
{
    auto& g        = gfx();
    const Theme& t = theme();
    g.drawRect(x, y, w, h, t.grid);
    // Horizontal reference lines every 25 dB.
    for (int d = DBM_FLOOR + 25; d < DBM_CEILING; d += 25) {
        const int ly = dbmToY(d, y + 1, h - 2);
        for (int px = x + 2; px < x + w - 2; px += 4) g.drawPixel(px, ly, t.grid);
    }
}

void drawTrace(int x, int y, int w, int h, const int8_t* samples, int count, int stride)
{
    if (count <= 0) return;
    auto& g = gfx();

    const int inner   = w - 2;
    const int startAt = (count > inner) ? count - inner : 0;
    int prevX = -1, prevY = -1;

    for (int i = startAt; i < count; i++) {
        const int8_t s = samples[i * stride];
        if (s == RSSI_INVALID) {
            prevX = -1;
            continue;
        }
        const int px = x + 1 + (i - startAt);
        const int py = dbmToY(s, y + 1, h - 2);
        const uint16_t c = levelColor(dbmLevel(s));
        if (prevX >= 0) {
            g.drawLine(prevX, prevY, px, py, c);
        } else {
            g.drawPixel(px, py, c);
        }
        prevX = px;
        prevY = py;
    }
}

void textCentered(const char* s, int cx, int y, uint16_t color, const lgfx::IFont* font)
{
    auto& g = gfx();
    g.setFont(font);
    g.setTextDatum(top_center);
    g.setTextColor(color, theme().bg);
    g.drawString(s, cx, y);
    g.setTextDatum(top_left);
}

}  // namespace ui
}  // namespace rfscope
