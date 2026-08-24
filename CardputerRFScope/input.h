// Arduino-side keyboard glue for the Cardputer and Cardputer ADV.
//
// Wraps M5Cardputer's keyboard in the pure KeyEventGen state machine and, on
// the ADV, drains the TCA8418 event FIFO properly. See key_events.h for why
// the ADV needs the extra care.
#pragma once

#include <cstdint>
#include <vector>

#include "key_events.h"

namespace rfscope {

enum class Nav : uint8_t { None, Up, Down, Left, Right, Select, Back, Tab };

class Input {
public:
    void begin();

    // Reads the keyboard and appends this frame's edge events to `out`.
    void poll(std::vector<KeyEvent>& out);

    // Drop held-key state, e.g. when switching screens.
    void resync()
    {
        _gen.resync();
    }

    bool isAdvKeyboard() const
    {
        return _isAdv;
    }
    const char* driverName() const
    {
        return _isAdv ? "TCA8418" : "IOMatrix";
    }

private:
    KeyEventGen _gen;
    std::vector<uint8_t> _held;
    bool _isAdv = false;
};

// True for events that should act (a fresh press, or an auto-repeat).
inline bool isActionable(const KeyEvent& e)
{
    return e.action == KeyAction::Press || e.action == KeyAction::Repeat;
}

// The Cardputer keyboard has no arrow keys; ; . , / are the de-facto cluster
// (with or without Fn held, both are accepted).
Nav navFor(const KeyEvent& e);

// True if the key produces a character for a text field.
bool isPrintable(const KeyEvent& e);

// The character a printable key produces, honouring shift and caps lock.
char charFor(const KeyEvent& e, bool capsLocked);

}  // namespace rfscope
