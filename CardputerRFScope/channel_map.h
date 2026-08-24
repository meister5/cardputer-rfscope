// 2.4 GHz channel plan arithmetic. Pure, host-testable.
#pragma once

#include <cmath>

namespace rfscope {

constexpr int CHANNEL_MIN   = 1;
constexpr int CHANNEL_MAX   = 14;
constexpr int CHANNEL_COUNT = CHANNEL_MAX - CHANNEL_MIN + 1;

// Width of the 802.11b/g/n spectral mask we model, in MHz. Channels are
// spaced 5 MHz apart but occupy ~22 MHz, which is the whole reason 2.4 GHz
// is so congested.
constexpr float CHANNEL_MASK_MHZ = 22.0f;

inline int channelToFreqMhz(int ch)
{
    if (ch >= 1 && ch <= 13) return 2412 + 5 * (ch - 1);
    if (ch == 14) return 2484;  // Japan's outlier, 12 MHz above ch 13
    return 0;
}

inline bool channelValid(int ch)
{
    return channelToFreqMhz(ch) != 0;
}

// How much of `src`'s energy lands on `dst`, as a 0..1 weight. A triangular
// approximation of the spectral mask: full weight on itself, zero once the
// centres are a mask-width apart. Channels 5 apart (25 MHz) score 0, which is
// exactly why 1/6/11 is the non-overlapping set.
inline float channelOverlap(int src, int dst)
{
    const int fa = channelToFreqMhz(src);
    const int fb = channelToFreqMhz(dst);
    if (fa == 0 || fb == 0) return 0.0f;
    const float d = std::fabs(static_cast<float>(fa - fb));
    const float w = 1.0f - d / CHANNEL_MASK_MHZ;
    return w > 0.0f ? w : 0.0f;
}

}  // namespace rfscope
