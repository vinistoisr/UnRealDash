#include "support/FakeClock.h"
TEST_CASE("2.5 oscillation at 20 Hz changes output at most twice") {
    RuleFixture f;
    f.definition.hysteresis_band = 0;
    f.definition.debounce = 75ms;
    f.Load();
    int transitions = 0;
    bool prior = false;
    for (int i = 0; i < 100; ++i) {
        f.Apply(i % 2 ? 374.15 : 372.15, 5ms + 25ms * i, static_cast<std::uint64_t>(i + 1));
        auto result = f.Evaluate();
        if (result.current != prior)
            ++transitions;
        prior = result.current;
    }
    CHECK(transitions <= 2);
    CHECK_FALSE(prior);
    f.Apply(400, 2505ms, 101);
    CHECK_FALSE(f.Evaluate().current);
    f.clock.time = 2580ms;
    CHECK(f.Evaluate().current);
}
TEST_CASE("2.5 treat_unavailable reports unavailable not false") {
    RuleFixture f;
    f.Load();
    f.Apply(400);
    CHECK(f.Evaluate().current);
    f.clock.time = 505ms;
    f.registry.Expire();
    CHECK(f.Evaluate().quality == Quality::unavailable);
}
TEST_CASE("2.5 hold_last flips to unavailable at its maximum hold and not before") {
    RuleFixture f;
    f.definition.missing_input_policy = MissingInputPolicy::hold_last;
    f.Load();
    f.Apply(400);
    CHECK(f.Evaluate().current);
    f.clock.time = 505ms;
    f.registry.Expire();
    CHECK(f.Evaluate().quality == Quality::valid);
    REQUIRE(f.schedule.Earliest());
    CHECK(f.schedule.Earliest()->time == 2505ms);
    f.clock.time = 2505ms - 1ns;
    CHECK(f.Evaluate().quality == Quality::valid);
    f.clock.time = f.schedule.Earliest()->time;
    CHECK(f.Evaluate().quality == Quality::unavailable);
}
TEST_CASE("2.5 invalid input triggers the policy not arithmetic") {
    RuleFixture f;
    f.definition.missing_input_policy = MissingInputPolicy::force_warn;
    f.Load();
    f.Apply(-1e300);
    f.Storage()[0].sample.quality = Quality::invalid;
    CHECK(f.Evaluate().current);
}
TEST_CASE("2.5 generation change clears hysteresis and debounce state") {
    RuleFixture f;
    f.definition.hysteresis_band = 2;
    f.definition.debounce = 100ms;
    f.Load();
    f.Apply(400);
    CHECK_FALSE(f.Evaluate().current);
    f.Apply(374, 55ms, 2, 1);
    f.clock.time = 105ms;
    CHECK_FALSE(f.Evaluate().current);
    CHECK_FALSE(f.Evaluate().latched);
    CHECK(f.schedule.Size() == 1);
}
TEST_CASE("2.5 current clears while latched stays set") {
    RuleFixture f;
    f.Load();
    f.Apply(400);
    CHECK(f.Evaluate().current);
    f.Apply(300, 6ms, 2);
    auto result = f.Evaluate();
    CHECK_FALSE(result.current);
    CHECK(result.latched);
}
TEST_CASE("2.5 acknowledge clears latched") {
    RuleFixture f;
    f.Load();
    f.Apply(400);
    CHECK(f.Evaluate().latched);
    REQUIRE(f.rule.Acknowledge(7).Ok());
    CHECK_FALSE(f.rule.Current().latched);
}
TEST_CASE("2.5 acknowledge while still asserting relatches on the next evaluation") {
    RuleFixture f;
    f.Load();
    f.Apply(400);
    CHECK(f.Evaluate().latched);
    REQUIRE(f.rule.Acknowledge(7).Ok());
    CHECK_FALSE(f.rule.Current().latched);
    CHECK(f.Evaluate().latched);
}
TEST_CASE("2.5 expiry schedule arms the earliest entry") {
    Fixture f;
    REQUIRE(f.schedule.Arm({505ms, ExpiryKind::freshness, 1}).Ok());
    REQUIRE(f.schedule.Arm({107ms, ExpiryKind::debounce, 7}).Ok());
    REQUIRE(f.schedule.Arm({2505ms, ExpiryKind::hold_last, 7}).Ok());
    CHECK(f.schedule.Earliest()->time == 107ms);
    Expiry expiry;
    CHECK_FALSE(f.schedule.PopDue(107ms - 1ns, expiry));
    CHECK(f.schedule.PopDue(107ms, expiry));
    CHECK(expiry.kind == ExpiryKind::debounce);
    CHECK(f.schedule.Earliest()->time == 505ms);
}
TEST_CASE("2.5 expiry schedule re-arms on a new sample before the deadline") {
    Fixture f;
    f.Apply();
    f.Apply(300, 405ms, 2);
    CHECK(f.schedule.Size() == 1);
    CHECK(f.schedule.Earliest()->time == 905ms);
    f.clock.time = 505ms;
    f.registry.Expire();
    CHECK(f.Storage()[0].sample.quality == Quality::valid);
}
TEST_CASE("2.5 expiry cancelled by a generation change yields no firing") {
    Fixture f;
    f.Apply();
    REQUIRE(f.registry.SetGeneration(1).Ok());
    Expiry expiry;
    CHECK_FALSE(f.schedule.PopDue(505ms, expiry));
    CHECK(f.schedule.Earliest() == nullptr);
}
TEST_CASE("2.5 value exactly on the threshold") {
    RuleFixture f;
    f.Load();
    f.Apply(373.15);
    CHECK_FALSE(f.Evaluate().current);
}
TEST_CASE("2.5 value exactly on each hysteresis band edge") {
    RuleFixture f;
    f.definition.hysteresis_band = 2;
    f.Load();
    f.Apply(375.15);
    CHECK_FALSE(f.Evaluate().current);
    f.Apply(375.16, 6ms, 2);
    CHECK(f.Evaluate().current);
    f.Apply(371.16, 7ms, 3);
    CHECK(f.Evaluate().current);
    f.Apply(371.15, 8ms, 4);
    CHECK_FALSE(f.Evaluate().current);
}
TEST_CASE("2.5 transition exactly at the debounce boundary") {
    RuleFixture f;
    f.definition.debounce = 100ms;
    f.Load();
    f.Apply(400);
    CHECK_FALSE(f.Evaluate().current);
    CHECK(f.schedule.Earliest()->time == 105ms);
    f.clock.time = 105ms - 1ns;
    CHECK_FALSE(f.Evaluate().current);
    f.clock.time = 105ms;
    CHECK(f.Evaluate().current);
}
