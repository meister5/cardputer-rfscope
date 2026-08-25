// Keyboard edge detection, auto-repeat and stuck-key recovery.
//
// WHY THIS EXISTS -- Cardputer ADV specific:
// The original Cardputer reads its keyboard by rescanning a 74HC138 matrix
// every update, so the "currently held" set is always ground truth. The ADV
// instead uses a TCA8418 I2C controller that reports press/release *events*
// through a FIFO, and M5Cardputer's reader maintains the held-key list by
// adding on press and erasing on release. If a release event is ever missed
// (FIFO overflow during a slow frame, a dropped interrupt), that key stays in
// the list forever and the UI sees it held down until reboot.
//
// This layer turns the driver's level-triggered "held" set into edge events
// and force-releases a key that has been held implausibly long, refusing to
// re-press it until the driver actually stops reporting it.
//
// Pure C++ so the recovery logic can be unit-tested on the host.
#pragma once

#include <cstdint>
#include <vector>

namespace rfscope {

struct KeyMods {
    bool fn    = false;
    bool shift = false;
    bool ctrl  = false;
    bool alt   = false;
    bool opt   = false;
};

enum class KeyAction : uint8_t { Press, Repeat, Release };

struct KeyEvent {
    uint8_t code = 0;
    KeyAction action = KeyAction::Press;
    KeyMods mods;
};

// Timing for edge detection and the stuck-key watchdog. Declared at namespace
// scope because a nested type's default member initialisers are not usable in
// a default argument of its own enclosing class.
struct KeyEventConfig {
    uint32_t repeatDelayMs = 420;
    uint32_t repeatRateMs  = 70;
    // Long enough that a deliberate hold still delivers a long run of repeats
    // before the watchdog decides the key must be stuck.
    uint32_t stuckTimeoutMs = 4000;
};

class KeyEventGen {
public:
    using Config = KeyEventConfig;

    explicit KeyEventGen(Config cfg = Config{}) : _cfg(cfg)
    {
    }

    const Config& config() const
    {
        return _cfg;
    }

    // Used when changing screens so a key held during a transition cannot leak
    // into the new one. Clearing the held set would do the opposite: the key is
    // still physically down, so the very next poll would not find it and would
    // report a brand-new press to the screen we just moved to. Suppress instead
    // -- nothing is emitted for these keys until the driver reports them
    // released, which is the same mechanism the stuck-key watchdog uses.
    void resync()
    {
        for (auto& h : _held) h.suppressed = true;
    }

    void update(const std::vector<uint8_t>& pressed, const KeyMods& mods, uint32_t nowMs,
                std::vector<KeyEvent>& out)
    {
        for (auto& h : _held) h.seen = false;

        for (uint8_t code : pressed) {
            Held* h = find(code);

            if (h == nullptr) {
                _held.push_back(Held{code, nowMs, nowMs + _cfg.repeatDelayMs, false, true});
                out.push_back(KeyEvent{code, KeyAction::Press, mods});
                continue;
            }

            h->seen = true;
            if (h->suppressed) continue;  // stuck: wait for the driver to clear it

            if (nowMs - h->downMs >= _cfg.stuckTimeoutMs) {
                h->suppressed = true;
                out.push_back(KeyEvent{code, KeyAction::Release, mods});
                continue;
            }

            if (nowMs >= h->nextRepeatMs) {
                h->nextRepeatMs = nowMs + _cfg.repeatRateMs;
                out.push_back(KeyEvent{code, KeyAction::Repeat, mods});
            }
        }

        for (size_t i = _held.size(); i-- > 0;) {
            if (_held[i].seen) continue;
            // A suppressed key already had its release reported.
            if (!_held[i].suppressed) {
                out.push_back(KeyEvent{_held[i].code, KeyAction::Release, mods});
            }
            _held.erase(_held.begin() + static_cast<long>(i));
        }
    }

private:
    struct Held {
        uint8_t code;
        uint32_t downMs;
        uint32_t nextRepeatMs;
        bool suppressed;
        bool seen;
    };

    Held* find(uint8_t code)
    {
        for (auto& h : _held)
            if (h.code == code) return &h;
        return nullptr;
    }

    Config _cfg;
    std::vector<Held> _held;
};

}  // namespace rfscope
