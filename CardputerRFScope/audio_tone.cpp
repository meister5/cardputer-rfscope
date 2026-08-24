#include "audio_tone.h"

#include <M5Cardputer.h>

#include "rfscope_config.h"

namespace rfscope {
namespace {

constexpr uint32_t kSlowestMs = 700;
constexpr uint32_t kFastestMs = 55;
constexpr int kLowHz          = 320;
constexpr int kHighHz         = 1900;

float level(int dbm)
{
    if (dbm <= DBM_FLOOR) return 0.0f;
    if (dbm >= DBM_CEILING) return 1.0f;
    return static_cast<float>(dbm - DBM_FLOOR) / static_cast<float>(DBM_CEILING - DBM_FLOOR);
}

}  // namespace

void SignalAudio::begin()
{
    M5Cardputer.Speaker.setVolume(90);
}

void SignalAudio::setEnabled(bool on)
{
    _enabled = on;
    if (!on) silence();
}

void SignalAudio::silence()
{
    M5Cardputer.Speaker.stop();
}

void SignalAudio::update(int dbm, bool valid, uint32_t nowMs)
{
    if (!_enabled || !valid) return;
    if (nowMs < _nextAt) return;

    const float l         = level(dbm);
    const uint32_t period = kSlowestMs - static_cast<uint32_t>(l * (kSlowestMs - kFastestMs));
    const int freq        = kLowHz + static_cast<int>(l * (kHighHz - kLowHz));

    M5Cardputer.Speaker.tone(freq, 18);
    _nextAt = nowMs + period;
}

void SignalAudio::click()
{
    if (_enabled) M5Cardputer.Speaker.tone(1400, 12);
}

void SignalAudio::confirm()
{
    if (_enabled) M5Cardputer.Speaker.tone(1800, 60);
}

void SignalAudio::reject()
{
    if (_enabled) M5Cardputer.Speaker.tone(220, 120);
}

}  // namespace rfscope
