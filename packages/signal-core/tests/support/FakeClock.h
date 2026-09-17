#pragma once
#include "SignalCore/Recording.h"
#include "SignalCore/RuleEngine.h"
#include <array>
#include <doctest.h>
#include <string>
#include <vector>
using namespace signal_core;
using namespace std::chrono_literals;
struct FakeClock {
    Time time{};
    Time wall{};
    Clock Get() {
        return {this, [](void *p) { return static_cast<FakeClock *>(p)->time; }};
    }
};
struct Fixture {
    FakeClock clock;
    std::array<Signal, 2> signals{
        {{1, "temperature", Unit::kelvin, 500ms, false}, {2, "pressure", Unit::pascal, 500ms, false}}};
    std::array<SignalSample, 2> a{}, b{}, c{};
    std::array<std::uint8_t, 1> la{}, lb{}, lc{};
    std::array<Expiry, 16> expiries{};
    ExpirySchedule schedule{expiries};
    SnapshotExchange exchange{{a, la, 0}, {b, lb, 0}, {c, lc, 0}};
    SignalRegistry registry{clock.Get(), signals, schedule, exchange};
    Fixture() { REQUIRE(registry.Initialize().Ok()); }
    std::span<SignalSample> Storage() { return exchange.WriterBuffer().samples; }
    Sample Make(double value = 300, Time time = 5ms, std::uint64_t seq = 1, std::uint64_t generation = 0) {
        Sample sample(value, Unit::kelvin, time);
        sample.seq = seq;
        sample.generation = generation;
        sample.age_evidence = AgeEvidence::measured;
        return sample;
    }
    void Apply(double value = 300, Time time = 5ms, std::uint64_t seq = 1, std::uint64_t generation = 0) {
        clock.time = time;
        REQUIRE(registry.Apply(1, Make(value, time, seq, generation)).Ok());
    }
    void Publish(bool latched = false) {
        const std::uint8_t warning = latched ? 1 : 0;
        REQUIRE(registry.Publish({&warning, 1}).Ok());
    }
};
struct RuleFixture : Fixture {
    std::array<ExpressionNode, 3> nodes{{{Operation::greater, 0, Unit::dimensionless, 0, 1, 2, "/root"},
                                         {Operation::signal_reference, 0, Unit::dimensionless, 1, 0, 0, "/signal"},
                                         {Operation::literal, 100, Unit::degree_celsius, 0, 0, 0, "/threshold"}}};
    RuleDefinition definition{7, nodes, 0, Unit::kelvin, 0ns, MissingInputPolicy::treat_unavailable, 2000ms};
    RuleEngine rule{clock.Get(), schedule};
    void Load() { REQUIRE(rule.Load(definition, signals).Ok()); }
    RuleResult Evaluate() { return rule.Evaluate(registry.Samples(), registry.Generation()); }
};
inline Status Append(void *context, std::string_view text) {
    static_cast<std::string *>(context)->append(text);
    return {};
}
inline TextSink Sink(std::string &text) { return {&text, Append}; }
