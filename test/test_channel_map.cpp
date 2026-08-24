#include "test_main.h"
#include "../CardputerRFScope/channel_map.h"

using namespace rfscope;

TEST(channel_centres_match_the_24ghz_plan)
{
    CHECK_EQ(channelToFreqMhz(1), 2412);
    CHECK_EQ(channelToFreqMhz(6), 2437);
    CHECK_EQ(channelToFreqMhz(11), 2462);
    CHECK_EQ(channelToFreqMhz(13), 2472);
    CHECK_EQ(channelToFreqMhz(14), 2484);  // the odd one out
}

TEST(out_of_range_channels_have_no_frequency)
{
    CHECK_EQ(channelToFreqMhz(0), 0);
    CHECK_EQ(channelToFreqMhz(15), 0);
    CHECK_EQ(channelToFreqMhz(-3), 0);
}

TEST(a_channel_fully_overlaps_itself)
{
    CHECK_NEAR(channelOverlap(6, 6), 1.0, 0.001);
}

TEST(channels_five_apart_do_not_overlap)
{
    // 25 MHz separation is wider than a 22 MHz mask: this is why 1/6/11 is the
    // classic non-overlapping set.
    CHECK_NEAR(channelOverlap(1, 6), 0.0, 0.001);
    CHECK_NEAR(channelOverlap(6, 11), 0.0, 0.001);
}

TEST(adjacent_channels_overlap_heavily)
{
    CHECK_NEAR(channelOverlap(1, 2), 1.0 - 5.0 / 22.0, 0.001);
    CHECK_NEAR(channelOverlap(1, 3), 1.0 - 10.0 / 22.0, 0.001);
}

TEST(overlap_is_symmetric)
{
    CHECK_NEAR(channelOverlap(3, 7), channelOverlap(7, 3), 0.0001);
}

TEST(overlap_with_an_invalid_channel_is_zero)
{
    CHECK_NEAR(channelOverlap(0, 6), 0.0, 0.001);
    CHECK_NEAR(channelOverlap(6, 99), 0.0, 0.001);
}

TEST(channel_14_sits_far_from_channel_13)
{
    // 2484 - 2472 = 12 MHz
    CHECK_NEAR(channelOverlap(13, 14), 1.0 - 12.0 / 22.0, 0.001);
}
