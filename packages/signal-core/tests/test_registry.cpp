#include "support/FakeClock.h"
#include <bit>
#include <cmath>
TEST_CASE("2.2 deadline expiry marks stale with no new sample") {
    Fixture f;
    f.Apply();
    REQUIRE(f.schedule.Earliest());
    CHECK(f.schedule.Earliest()->time == 505ms);
    f.clock.time = 504ms;
    f.registry.Expire();
    CHECK(f.Storage()[0].sample.quality == Quality::valid);
    f.clock.time = 506ms;
    f.registry.Expire();
    CHECK(f.Storage()[0].sample.quality == Quality::stale);
}
TEST_CASE("2.2 wall clock jump does not change freshness") {
    Fixture f;
    f.Apply();
    for (auto wall : {-1000000s, 1000000s}) {
        f.clock.wall = wall;
        f.clock.time = 504ms;
        f.registry.Expire();
        CHECK(f.Storage()[0].sample.quality == Quality::valid);
    }
    f.clock.time = 505ms;
    f.registry.Expire();
    CHECK(f.Storage()[0].sample.quality == Quality::stale);
}
TEST_CASE("2.2 older generation sample is rejected and counted") {
    Fixture f;
    f.Apply(300, 5ms, 1, 2);
    auto status = f.registry.Apply(1, f.Make(999, 6ms, 2, 1));
    CHECK(status.code == ErrorCode::old_generation);
    CHECK(f.registry.RejectedGenerations() == 1);
    CHECK(f.Storage()[0].sample.value == 300);
}
TEST_CASE("2.2 snapshot never mixes publications") {
    Fixture f;
    f.Apply();
    f.Publish();
    auto initial = f.exchange.Acquire();
    CHECK(initial.samples[0].sample.value == 300);
    f.Apply(400, 6ms, 2);
    auto mid = f.exchange.Acquire();
    CHECK_FALSE(mid.new_publication);
    CHECK(mid.samples[0].sample.value == 300);
    auto pressure = f.Make(400, 6ms, 2);
    pressure.unit = Unit::pascal;
    REQUIRE(f.registry.Apply(2, pressure).Ok());
    f.Publish();
    auto next = f.exchange.Acquire();
    CHECK(next.samples[0].sample.value == 400);
    CHECK(next.samples[1].sample.value == 400);
}
TEST_CASE("2.2 held snapshot survives 1000 publications") {
    Fixture f;
    f.Apply();
    f.Publish(true);
    auto held = f.exchange.Acquire();
    const std::array<SignalSample, 2> original{held.samples[0], held.samples[1]};
    const auto original_generation = held.generation;
    const auto original_version = held.version;
    const auto original_publication = held.new_publication;
    const std::array<std::uint8_t, 1> original_warnings{held.latched[0]};
    for (std::uint64_t i = 0; i < 1000; ++i) {
        f.Apply(static_cast<double>(i), Time(6000000 + static_cast<std::int64_t>(i)), i + 2);
        f.Publish();
    }
    CHECK(held.samples.size() == original.size());
    for (std::size_t i = 0; i < original.size(); ++i) {
        const auto &actual = held.samples[i];
        const auto &expected = original[i];
        CHECK(actual.signal == expected.signal);
        CHECK(actual.received == expected.received);
        CHECK(std::bit_cast<std::uint64_t>(actual.sample.value) == std::bit_cast<std::uint64_t>(expected.sample.value));
        CHECK(actual.sample.unit == expected.sample.unit);
        CHECK(actual.sample.source == expected.sample.source);
        CHECK(actual.sample.seq == expected.sample.seq);
        CHECK(actual.sample.t_recv == expected.sample.t_recv);
        CHECK(actual.sample.t_source == expected.sample.t_source);
        CHECK(actual.sample.has_source_time == expected.sample.has_source_time);
        CHECK(actual.sample.quality == expected.sample.quality);
        CHECK(actual.sample.age_evidence == expected.sample.age_evidence);
        CHECK(actual.sample.generation == expected.sample.generation);
    }
    CHECK(held.generation == original_generation);
    CHECK(held.version == original_version);
    CHECK(held.new_publication == original_publication);
    CHECK(held.latched.size() == original_warnings.size());
    for (std::size_t i = 0; i < original_warnings.size(); ++i)
        CHECK(held.latched[i] == original_warnings[i]);
    CHECK(f.exchange.WriterBlockedCount() == 0);
}
TEST_CASE("registry held measurements refresh receive time") {
    Fixture f;
    auto sample = f.Make();
    sample.age_evidence = AgeEvidence::unknown;
    REQUIRE(f.registry.Apply(1, sample).Ok());
    sample.seq = 2;
    sample.t_recv = 405ms;
    f.clock.time = 405ms;
    REQUIRE(f.registry.Apply(1, sample).Ok());
    f.clock.time = 505ms;
    f.registry.Expire();
    CHECK(f.Storage()[0].sample.quality == Quality::valid);
    CHECK(f.Storage()[0].sample.t_recv == 405ms);
    CHECK(f.schedule.Earliest()->time == 905ms);
}

TEST_CASE("review 1 unavailable sample still enforces ordering until generation reset") {
    Fixture f;
    auto unavailable = f.Make(300, 5ms, 100);
    unavailable.quality = Quality::unavailable;
    REQUIRE(f.registry.Apply(1, unavailable).Ok());
    f.Publish(); // The received marker must survive transfer to another writer slot.
    CHECK(f.registry.Apply(1, f.Make(400, 6ms, 1)).code == ErrorCode::invalid_sequence);
    CHECK(f.registry.RejectedOrdering() == 1);
    CHECK(f.Storage()[0].sample.seq == 100);
    CHECK(f.Storage()[0].sample.quality == Quality::unavailable);
    CHECK(f.registry.Apply(1, f.Make(400, 4ms, 101)).code == ErrorCode::invalid_sequence);
    CHECK(f.registry.RejectedOrdering() == 2);
    REQUIRE(f.registry.SetGeneration(1).Ok());
    CHECK_FALSE(f.Storage()[0].received);
    CHECK(f.registry.Apply(1, f.Make(400, 4ms, 1, 1)).Ok());
    CHECK(f.Storage()[0].sample.seq == 1);
}
TEST_CASE("review 2 non-valid finite samples normalize without changing quality") {
    for (auto quality : {Quality::stale, Quality::unavailable, Quality::invalid}) {
        Fixture f;
        auto sample = f.Make(100);
        sample.unit = Unit::degree_celsius;
        sample.quality = quality;
        REQUIRE(f.registry.Apply(1, sample).Ok());
        CHECK(f.Storage()[0].sample.unit == Unit::kelvin);
        CHECK(f.Storage()[0].sample.value == doctest::Approx(373.15));
        CHECK(f.Storage()[0].sample.quality == quality);
        sample.value = std::numeric_limits<double>::quiet_NaN();
        sample.seq = 2;
        REQUIRE(f.registry.Apply(1, sample).Ok());
        CHECK(std::isnan(f.Storage()[0].sample.value));
        CHECK(f.Storage()[0].sample.quality == quality);
    }
}
