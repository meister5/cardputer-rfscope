// Geiger-counter style audio feedback for signal hunting: the stronger the
// signal, the higher and faster the clicks. Non-blocking -- update() is
// called every frame and decides when the next click is due.
#pragma once

#include <cstdint>

namespace rfscope {

class SignalAudio {
public:
    void begin();

    void setEnabled(bool on);
    bool enabled() const
    {
        return _enabled;
    }

    // Drive with the current reading. Pass valid=false to go quiet.
    void update(int dbm, bool valid, uint32_t nowMs);

    // One-shot UI feedback.
    void click();
    void confirm();
    void reject();

    void silence();

private:
    bool _enabled     = true;
    uint32_t _nextAt  = 0;
};

}  // namespace rfscope
