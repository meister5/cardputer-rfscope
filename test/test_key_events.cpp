#include "test_main.h"
#include "../CardputerRFScope/key_events.h"

using namespace rfscope;

static std::vector<KeyEvent> step(KeyEventGen& g, std::vector<uint8_t> held, uint32_t now,
                                  KeyMods mods = KeyMods{})
{
    std::vector<KeyEvent> out;
    g.update(held, mods, now, out);
    return out;
}

TEST(a_new_key_produces_one_press)
{
    KeyEventGen g;
    auto ev = step(g, {'a'}, 0);
    CHECK_EQ(ev.size(), 1);
    CHECK_EQ(ev[0].code, 'a');
    CHECK(ev[0].action == KeyAction::Press);
}

TEST(holding_a_key_does_not_re_press_it)
{
    KeyEventGen g;
    step(g, {'a'}, 0);
    auto ev = step(g, {'a'}, 10);
    CHECK_EQ(ev.size(), 0);
}

TEST(releasing_a_key_produces_a_release)
{
    KeyEventGen g;
    step(g, {'a'}, 0);
    auto ev = step(g, {}, 10);
    CHECK_EQ(ev.size(), 1);
    CHECK(ev[0].action == KeyAction::Release);
    CHECK_EQ(ev[0].code, 'a');
}

TEST(auto_repeat_starts_after_the_delay_then_runs_at_the_repeat_rate)
{
    KeyEventGen::Config cfg;
    cfg.repeatDelayMs = 400;
    cfg.repeatRateMs  = 100;
    KeyEventGen g(cfg);

    step(g, {'a'}, 0);
    CHECK_EQ(step(g, {'a'}, 399).size(), 0);   // still inside the delay
    auto first = step(g, {'a'}, 400);
    CHECK_EQ(first.size(), 1);
    CHECK(first[0].action == KeyAction::Repeat);

    CHECK_EQ(step(g, {'a'}, 499).size(), 0);
    CHECK_EQ(step(g, {'a'}, 500).size(), 1);   // one repeat period later
}

TEST(two_keys_pressed_in_one_frame_both_report)
{
    KeyEventGen g;
    auto ev = step(g, {'a', 'b'}, 0);
    CHECK_EQ(ev.size(), 2);
}

TEST(modifiers_are_captured_on_the_event)
{
    KeyEventGen g;
    KeyMods m;
    m.fn    = true;
    m.shift = true;
    auto ev = step(g, {';'}, 0, m);
    CHECK_EQ(ev.size(), 1);
    CHECK(ev[0].mods.fn);
    CHECK(ev[0].mods.shift);
    CHECK(!ev[0].mods.ctrl);
}

// --- Cardputer ADV specific: the TCA8418 driver is event/FIFO driven, so a
// --- dropped release leaves a key reported as held forever. These guard that.

TEST(a_key_held_past_the_stuck_timeout_is_force_released)
{
    KeyEventGen::Config cfg;
    cfg.stuckTimeoutMs = 1000;
    KeyEventGen g(cfg);

    step(g, {'a'}, 0);
    auto ev = step(g, {'a'}, 1001);
    bool sawRelease = false;
    for (const auto& e : ev)
        if (e.action == KeyAction::Release && e.code == 'a') sawRelease = true;
    CHECK(sawRelease);
}

TEST(a_stuck_key_does_not_re_press_while_the_driver_still_reports_it)
{
    KeyEventGen::Config cfg;
    cfg.stuckTimeoutMs = 1000;
    KeyEventGen g(cfg);

    step(g, {'a'}, 0);
    step(g, {'a'}, 1001);  // force-released here
    for (uint32_t t = 1002; t < 5000; t += 100) {
        auto ev = step(g, {'a'}, t);
        for (const auto& e : ev) CHECK(e.action != KeyAction::Press);
    }
}

TEST(a_stuck_key_works_again_once_the_driver_finally_clears_it)
{
    KeyEventGen::Config cfg;
    cfg.stuckTimeoutMs = 1000;
    KeyEventGen g(cfg);

    step(g, {'a'}, 0);
    step(g, {'a'}, 1001);   // force-released, now suppressed
    step(g, {}, 1100);      // driver finally drops it -> suppression lifts
    auto ev = step(g, {'a'}, 1200);
    CHECK_EQ(ev.size(), 1);
    CHECK(ev[0].action == KeyAction::Press);
}

TEST(the_stuck_timeout_is_long_enough_for_real_auto_repeat)
{
    // A genuine hold must still deliver a useful run of repeats before the
    // watchdog gives up on it.
    KeyEventGen::Config cfg;
    CHECK(cfg.stuckTimeoutMs >= cfg.repeatDelayMs + 20 * cfg.repeatRateMs);
}

TEST(resync_forgets_every_held_key_without_emitting_events)
{
    KeyEventGen g;
    step(g, {'a'}, 0);
    g.resync();
    auto ev = step(g, {}, 10);
    CHECK_EQ(ev.size(), 0);
}
