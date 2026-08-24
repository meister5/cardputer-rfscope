#include "test_main.h"
#include "../CardputerRFScope/signal_stats.h"

using namespace rfscope;

TEST(quality_maps_the_useful_dbm_range)
{
    CHECK_EQ(rssiToQuality(-50), 100);
    CHECK_EQ(rssiToQuality(-100), 0);
    CHECK_EQ(rssiToQuality(-75), 50);
}

TEST(quality_clamps_outside_the_range)
{
    CHECK_EQ(rssiToQuality(-20), 100);
    CHECK_EQ(rssiToQuality(-130), 0);
}

TEST(empty_trace_reports_no_samples)
{
    RssiTrace<8> t;
    CHECK(t.empty());
    CHECK_EQ(t.size(), 0);
    CHECK_EQ(t.capacity(), 8);
    CHECK_EQ(t.newest(), RSSI_INVALID);
}

TEST(trace_keeps_newest_samples_when_it_wraps)
{
    RssiTrace<4> t;
    for (int8_t v : {-10, -20, -30, -40, -50, -60}) t.push(v);
    CHECK_EQ(t.size(), 4);
    // oldest two were dropped
    CHECK_EQ(t.at(0), -30);
    CHECK_EQ(t.at(3), -60);
    CHECK_EQ(t.newest(), -60);
}

TEST(trace_computes_min_max_avg)
{
    RssiTrace<8> t;
    for (int8_t v : {-40, -60, -50}) t.push(v);
    CHECK_EQ(t.min(), -60);
    CHECK_EQ(t.max(), -40);
    CHECK_NEAR(t.avg(), -50.0, 0.001);
}

TEST(jitter_is_mean_absolute_step_between_samples)
{
    RssiTrace<8> t;
    for (int8_t v : {-40, -45, -41}) t.push(v);  // steps of 5 and 4
    CHECK_NEAR(t.jitter(), 4.5, 0.001);
}

TEST(jitter_of_a_single_sample_is_zero)
{
    RssiTrace<8> t;
    t.push(-40);
    CHECK_NEAR(t.jitter(), 0.0, 0.001);
}

TEST(clearing_a_trace_resets_it)
{
    RssiTrace<4> t;
    t.push(-40);
    t.clear();
    CHECK(t.empty());
    CHECK_EQ(t.newest(), RSSI_INVALID);
}

TEST(ewma_seeds_on_first_sample_then_blends)
{
    Ewma e(0.5f);
    CHECK(!e.valid());
    e.push(-40.0f);
    CHECK(e.valid());
    CHECK_NEAR(e.value(), -40.0, 0.001);
    e.push(-60.0f);
    CHECK_NEAR(e.value(), -50.0, 0.001);
}

TEST(peak_hold_holds_then_decays_toward_the_signal)
{
    PeakHold p(10.0f);  // 10 dB per second
    p.update(-40.0f, 0);
    CHECK_NEAR(p.value(), -40.0, 0.001);
    // signal drops; peak holds at the old value at t=0
    p.update(-80.0f, 0);
    CHECK_NEAR(p.value(), -40.0, 0.001);
    // after 1s the peak has decayed 10 dB
    p.update(-80.0f, 1000);
    CHECK_NEAR(p.value(), -50.0, 0.001);
}

TEST(peak_hold_never_decays_below_the_current_signal)
{
    PeakHold p(10.0f);
    p.update(-40.0f, 0);
    p.update(-45.0f, 100000);  // huge elapsed time
    CHECK_NEAR(p.value(), -45.0, 0.001);
}

TEST(peak_hold_jumps_straight_up_on_a_stronger_signal)
{
    PeakHold p(10.0f);
    p.update(-80.0f, 0);
    p.update(-30.0f, 10);
    CHECK_NEAR(p.value(), -30.0, 0.001);
}

TEST(copy_to_writes_oldest_first_and_reports_the_count)
{
    RssiTrace<4> t;
    for (int8_t v : {-10, -20, -30, -40, -50}) t.push(v);
    int8_t out[4] = {0, 0, 0, 0};
    CHECK_EQ(t.copyTo(out, 4), 4);
    CHECK_EQ(out[0], -20);
    CHECK_EQ(out[3], -50);
}

TEST(copy_to_respects_a_smaller_destination_by_keeping_the_newest)
{
    RssiTrace<8> t;
    for (int8_t v : {-10, -20, -30, -40}) t.push(v);
    int8_t out[2] = {0, 0};
    CHECK_EQ(t.copyTo(out, 2), 2);
    CHECK_EQ(out[0], -30);
    CHECK_EQ(out[1], -40);
}

TEST(copy_to_an_empty_trace_copies_nothing)
{
    RssiTrace<4> t;
    int8_t out[4] = {9, 9, 9, 9};
    CHECK_EQ(t.copyTo(out, 4), 0);
    CHECK_EQ(out[0], 9);
}
