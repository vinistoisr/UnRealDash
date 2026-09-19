#include "support/FakeClock.h"
#include <array>
#include <map>
#include <vector>
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

namespace {
// PLAN 4.8's armed-expiry rows, recorded rather than formatted. The property under test is that
// one arming produces exactly one status row, which no amount of reading the callers can show.
struct ScheduleWitness final : IExpiryEventSink {
    struct Event {
        enum class Kind { armed, fired, cancelled } kind{};
        std::uint64_t serial{};
        CancelReason reason{};
        std::uint64_t by{};
    };
    std::vector<Event> events;
    void OnArmed(const Expiry &expiry) override {
        events.push_back({Event::Kind::armed, expiry.serial, {}, 0});
    }
    void OnFired(const Expiry &expiry, Time) override {
        events.push_back({Event::Kind::fired, expiry.serial, {}, 0});
    }
    void OnCancelled(const Expiry &expiry, CancelReason reason, std::uint64_t by) override {
        events.push_back({Event::Kind::cancelled, expiry.serial, reason, by});
    }
    // The invariant, computed rather than eyeballed: every serial armed has exactly one status.
    bool OneStatusEach() const {
        std::map<std::uint64_t, int> armed, status;
        for (const Event &event : events) {
            if (event.kind == Event::Kind::armed) ++armed[event.serial];
            else ++status[event.serial];
        }
        if (armed.size() != status.size()) return false;
        for (const auto &[serial, count] : armed)
            if (count != 1 || status.count(serial) != 1 || status.at(serial) != 1) return false;
        return true;
    }
    std::size_t Count(Event::Kind kind) const {
        std::size_t total = 0;
        for (const Event &event : events)
            if (event.kind == kind) ++total;
        return total;
    }
};

Expiry Freshness(Time at, std::uint32_t id, std::uint64_t sample = 0) {
    Expiry expiry{};
    expiry.time = at;
    expiry.kind = ExpiryKind::freshness;
    expiry.id = id;
    expiry.sample = sample;
    return expiry;
}
} // namespace

TEST_CASE("4.8 every arming gets its own identifier") {
    std::array<Expiry, 8> storage{};
    ExpirySchedule schedule(storage);
    ScheduleWitness witness;
    schedule.SetSink(&witness);

    REQUIRE(schedule.Arm(Freshness(10ns, 1, 100)).Ok());
    // Re-arming the same signal is a different arming, and the old one is cancelled naming the
    // sample that moved it. Arm has always replaced silently; the point of the serial is that the
    // two are now tellable apart.
    REQUIRE(schedule.Arm(Freshness(20ns, 1, 101)).Ok());
    REQUIRE(witness.events.size() == 3);
    CHECK(witness.events[0].kind == ScheduleWitness::Event::Kind::armed);
    CHECK(witness.events[1].kind == ScheduleWitness::Event::Kind::cancelled);
    CHECK(witness.events[1].serial == witness.events[0].serial);
    CHECK(witness.events[1].reason == CancelReason::rearmed);
    CHECK(witness.events[1].by == 101);
    CHECK(witness.events[2].kind == ScheduleWitness::Event::Kind::armed);
    CHECK(witness.events[2].serial != witness.events[0].serial);
    CHECK_FALSE(witness.OneStatusEach());  // the second arming has no status row yet

    Expiry popped{};
    REQUIRE(schedule.PopDue(30ns, popped));
    CHECK(popped.serial == witness.events[2].serial);
    CHECK(witness.OneStatusEach());
}

TEST_CASE("4.8 a firing is not also a cancellation") {
    std::array<Expiry, 4> storage{};
    ExpirySchedule schedule(storage);
    ScheduleWitness witness;
    schedule.SetSink(&witness);
    REQUIRE(schedule.Arm(Freshness(10ns, 1)).Ok());

    Expiry popped{};
    REQUIRE(schedule.PopDue(10ns, popped));
    // PopDue removes its own entry internally. Routing that through Cancel would have written a
    // cancelled row for an expiry that fired, which is the failure the private Remove exists for.
    CHECK(witness.Count(ScheduleWitness::Event::Kind::fired) == 1);
    CHECK(witness.Count(ScheduleWitness::Event::Kind::cancelled) == 0);

    // And the cancel SignalRegistry::Expire would issue afterwards finds nothing and says
    // nothing, which is the other half of the one-status-row rule.
    schedule.Cancel(ExpiryKind::freshness, 1);
    schedule.Fire(ExpiryKind::freshness, 1, 11ns);
    CHECK(witness.Count(ScheduleWitness::Event::Kind::cancelled) == 0);
    CHECK(witness.Count(ScheduleWitness::Event::Kind::fired) == 1);
    CHECK(witness.OneStatusEach());
}

TEST_CASE("4.8 Fire and PopDue cannot both fire the same arming") {
    std::array<Expiry, 4> storage{};
    ExpirySchedule schedule(storage);
    ScheduleWitness witness;
    schedule.SetSink(&witness);
    REQUIRE(schedule.Arm(Freshness(10ns, 1)).Ok());

    // Whichever observes the deadline first removes the entry; the other finds nothing. Both
    // orders, because a Pump runs both and the order depends on when the sample arrived.
    schedule.Fire(ExpiryKind::freshness, 1, 10ns);
    Expiry popped{};
    CHECK_FALSE(schedule.PopDue(10ns, popped));
    CHECK(witness.Count(ScheduleWitness::Event::Kind::fired) == 1);
    CHECK(witness.OneStatusEach());
}

TEST_CASE("4.8 a clean shutdown cancels what is still armed, and says why") {
    std::array<Expiry, 8> storage{};
    ExpirySchedule schedule(storage);
    ScheduleWitness witness;
    schedule.SetSink(&witness);
    REQUIRE(schedule.Arm(Freshness(100ns, 1)).Ok());
    REQUIRE(schedule.Arm(Freshness(200ns, 2)).Ok());

    schedule.Clear(CancelReason::run_end, 0);
    CHECK(witness.Count(ScheduleWitness::Event::Kind::cancelled) == 2);
    for (const auto &event : witness.events)
        if (event.kind == ScheduleWitness::Event::Kind::cancelled)
            CHECK(event.reason == CancelReason::run_end);
    // Every arming accounted for, which is the clause PLAN puts on an orderly exit.
    CHECK(witness.OneStatusEach());
    CHECK(schedule.Size() == 0);
}

TEST_CASE("4.8 rule expiries carry the same identity as freshness ones") {
    // signal-core's RuleEngine really does arm hold_last and debounce entries. The Stage 0 player
    // loads no rules, so a player run has none, but the record type is not hypothetical.
    std::array<Expiry, 8> storage{};
    ExpirySchedule schedule(storage);
    ScheduleWitness witness;
    schedule.SetSink(&witness);

    Expiry hold{};
    hold.time = 50ns;
    hold.kind = ExpiryKind::hold_last;
    hold.id = 7;
    REQUIRE(schedule.Arm(hold).Ok());
    Expiry debounce{};
    debounce.time = 60ns;
    debounce.kind = ExpiryKind::debounce;
    debounce.id = 7;
    REQUIRE(schedule.Arm(debounce).Ok());
    // Same rule id, different kinds, so neither cancels the other.
    CHECK(witness.Count(ScheduleWitness::Event::Kind::cancelled) == 0);
    CHECK(schedule.Size() == 2);

    schedule.Cancel(ExpiryKind::hold_last, 7, CancelReason::rearmed, 0);
    Expiry popped{};
    REQUIRE(schedule.PopDue(60ns, popped));
    CHECK(popped.kind == ExpiryKind::debounce);
    CHECK(witness.OneStatusEach());
}
