#include "input.h"

#include <M5Cardputer.h>

#include "utility/Keyboard/Keyboard_def.h"

namespace rfscope {
namespace {

// The ADV's TCA8418 reader handles at most one FIFO event per updateKeyList()
// call. Draining several per frame keeps fast typing from queueing up (and a
// deep queue is exactly what leads to a dropped release and a stuck key).
constexpr int kAdvDrainPerFrame = 12;

bool isModifierCode(uint8_t c)
{
    return c == KEY_FN || c == KEY_OPT || c == KEY_LEFT_CTRL || c == KEY_LEFT_SHIFT ||
           c == KEY_LEFT_ALT;
}

// value_first -> value_second, by scanning the library's key map. 56 entries,
// so a linear scan is cheaper than keeping a second table in sync.
char shiftedChar(uint8_t base)
{
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 14; x++) {
            if (static_cast<uint8_t>(_key_value_map[y][x].value_first) == base) {
                return _key_value_map[y][x].value_second;
            }
        }
    }
    return static_cast<char>(base);
}

}  // namespace

void Input::begin()
{
    _isAdv = (M5.getBoard() == m5::board_t::board_M5CardputerADV);
    _gen.resync();
    _held.reserve(8);
}

void Input::poll(std::vector<KeyEvent>& out)
{
    // M5Cardputer::update() ticks M5Unified and processes one key event.
    M5Cardputer.update();

    if (_isAdv) {
        for (int i = 0; i < kAdvDrainPerFrame; i++) {
            M5Cardputer.Keyboard.updateKeyList();
        }
        M5Cardputer.Keyboard.updateKeysState();
    }

    _held.clear();
    for (const auto& pos : M5Cardputer.Keyboard.keyList()) {
        const uint8_t code =
            static_cast<uint8_t>(M5Cardputer.Keyboard.getKeyValue(pos).value_first);
        if (isModifierCode(code)) continue;
        _held.push_back(code);
    }

    const auto& ks = M5Cardputer.Keyboard.keysState();
    KeyMods mods;
    mods.fn    = ks.fn;
    mods.shift = ks.shift;
    mods.ctrl  = ks.ctrl;
    mods.alt   = ks.alt;
    mods.opt   = ks.opt;

    _gen.update(_held, mods, millis(), out);
}

Nav navFor(const KeyEvent& e)
{
    switch (e.code) {
        case ';':
            return Nav::Up;
        case '.':
            return Nav::Down;
        case ',':
            return Nav::Left;
        case '/':
            return Nav::Right;
        case KEY_ENTER:
            return Nav::Select;
        case KEY_BACKSPACE:
            return Nav::Back;
        case '`':
            return Nav::Back;
        case KEY_TAB:
            return Nav::Tab;
        default:
            return Nav::None;
    }
}

bool isPrintable(const KeyEvent& e)
{
    if (e.code == KEY_ENTER || e.code == KEY_BACKSPACE || e.code == KEY_TAB) return false;
    return e.code >= 0x20 && e.code < 0x7F;
}

char charFor(const KeyEvent& e, bool capsLocked)
{
    const bool upper = e.mods.shift || capsLocked;
    return upper ? shiftedChar(e.code) : static_cast<char>(e.code);
}

}  // namespace rfscope
