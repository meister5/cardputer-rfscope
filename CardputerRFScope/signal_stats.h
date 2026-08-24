// Pure signal-statistics helpers shared by the WiFi meter and the BLE tracker.
//
// Deliberately free of Arduino/ESP headers so it can be unit-tested on the
// host -- this is the only part of the firmware that can be verified without
// the hardware in hand.
#pragma once

#include <cstddef>
#include <cstdint>

namespace rfscope {

// Sentinel for "no reading yet". Matches nothing a real radio reports.
constexpr int8_t RSSI_INVALID = -128;

// Link quality in percent, using the conventional linear mapping where
// -50 dBm and better is a full bar and -100 dBm is nothing.
inline int rssiToQuality(int dbm)
{
    if (dbm <= -100) return 0;
    if (dbm >= -50) return 100;
    return 2 * (dbm + 100);
}

// Fixed-capacity ring of RSSI samples. Index 0 is the oldest retained sample,
// size()-1 the newest, which is the order the trace is drawn in.
template <size_t N>
class RssiTrace {
    static_assert(N > 0, "a trace needs capacity");

public:
    void clear()
    {
        _count = 0;
        _head  = 0;
    }

    void push(int8_t dbm)
    {
        _buf[_head] = dbm;
        _head       = (_head + 1) % N;
        if (_count < N) _count++;
    }

    size_t size() const
    {
        return _count;
    }
    constexpr size_t capacity() const
    {
        return N;
    }
    bool empty() const
    {
        return _count == 0;
    }

    int8_t at(size_t i) const
    {
        if (i >= _count) return RSSI_INVALID;
        const size_t oldest = (_head + N - _count) % N;
        return _buf[(oldest + i) % N];
    }

    int8_t newest() const
    {
        return _count ? at(_count - 1) : RSSI_INVALID;
    }

    // Copies samples oldest-first into `out`. If `out` is smaller than the
    // trace, the newest `n` samples are kept -- that is what a scrolling plot
    // wants. Returns how many were written.
    size_t copyTo(int8_t* out, size_t n) const
    {
        if (!out || n == 0 || _count == 0) return 0;
        const size_t take  = (_count < n) ? _count : n;
        const size_t first = _count - take;
        for (size_t i = 0; i < take; i++) out[i] = at(first + i);
        return take;
    }

    int8_t min() const
    {
        if (!_count) return RSSI_INVALID;
        int8_t m = at(0);
        for (size_t i = 1; i < _count; i++)
            if (at(i) < m) m = at(i);
        return m;
    }

    int8_t max() const
    {
        if (!_count) return RSSI_INVALID;
        int8_t m = at(0);
        for (size_t i = 1; i < _count; i++)
            if (at(i) > m) m = at(i);
        return m;
    }

    float avg() const
    {
        if (!_count) return 0.0f;
        long sum = 0;
        for (size_t i = 0; i < _count; i++) sum += at(i);
        return static_cast<float>(sum) / static_cast<float>(_count);
    }

    // Mean absolute step between consecutive samples: a cheap stand-in for
    // "how unstable is this link", which is what you actually feel when
    // walking around with the device.
    float jitter() const
    {
        if (_count < 2) return 0.0f;
        long sum = 0;
        for (size_t i = 1; i < _count; i++) {
            int d = at(i) - at(i - 1);
            sum += (d < 0) ? -d : d;
        }
        return static_cast<float>(sum) / static_cast<float>(_count - 1);
    }

private:
    int8_t _buf[N] = {};
    size_t _head   = 0;
    size_t _count  = 0;
};

// Exponentially weighted moving average, seeded by its first sample so the
// needle does not sweep up from zero on every mode change.
class Ewma {
public:
    explicit Ewma(float alpha) : _alpha(alpha)
    {
    }

    void reset()
    {
        _valid = false;
        _value = 0.0f;
    }

    void push(float v)
    {
        if (!_valid) {
            _value = v;
            _valid = true;
        } else {
            _value += _alpha * (v - _value);
        }
    }

    bool valid() const
    {
        return _valid;
    }
    float value() const
    {
        return _value;
    }

private:
    float _alpha;
    float _value = 0.0f;
    bool _valid  = false;
};

// Classic analyser peak-hold: jumps instantly to a stronger reading, then
// bleeds back down at a fixed dB/second, never falling below the live signal.
class PeakHold {
public:
    explicit PeakHold(float decayDbPerSec = 6.0f) : _decay(decayDbPerSec)
    {
    }

    void reset()
    {
        _valid = false;
    }

    void update(float v, uint32_t nowMs)
    {
        if (!_valid) {
            _value  = v;
            _lastMs = nowMs;
            _valid  = true;
            return;
        }
        const uint32_t dt = nowMs - _lastMs;
        _lastMs           = nowMs;
        _value -= _decay * (static_cast<float>(dt) / 1000.0f);
        if (_value < v) _value = v;
    }

    float value() const
    {
        return _value;
    }
    bool valid() const
    {
        return _valid;
    }

private:
    float _decay;
    float _value     = 0.0f;
    uint32_t _lastMs = 0;
    bool _valid      = false;
};

}  // namespace rfscope
