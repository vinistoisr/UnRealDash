#pragma once
#include "SignalCore/RuleEngine.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace signal_core {
// Both runners own reporting; all assertions live here to keep the subsets identical.
// The callback is named ReportCheck rather than check because Unreal defines check() as a
// one-argument macro, and this header is included by an engine module. Signal-core headers must
// avoid identifiers the engine defines as macros: check, ensure, verify, TEXT among them.
template <class CheckCallback> void RunSmokeSubset(CheckCallback ReportCheck) {
    using namespace std::chrono_literals;
    Time now = 5ms;
    Clock clock{&now, [](void *context) { return *static_cast<Time *>(context); }};
    const std::array<Signal, 1> signals{{{1, "temperature", Unit::kelvin, 500ms, false}}};
    std::array<SignalSample, 1> a{}, b{}, c{};
    std::array<Expiry, 4> expiries{};
    ExpirySchedule schedule{expiries};
    SnapshotExchange exchange{{a, {}, 0}, {b, {}, 0}, {c, {}, 0}};
    SignalRegistry registry{clock, signals, schedule, exchange};
    ReportCheck(registry.Initialize().Ok(), "freshness initialize");
    Sample sample{300, Unit::kelvin, now};
    sample.seq = 1;
    sample.age_evidence = AgeEvidence::measured;
    ReportCheck(registry.Apply(1, sample).Ok(), "freshness apply at 5 ms");
    ReportCheck(schedule.Earliest() && schedule.Earliest()->time == 505ms, "freshness deadline 505 ms");
    now = 504ms;
    registry.Expire();
    ReportCheck(registry.Samples()[0].sample.quality == Quality::valid, "freshness valid before deadline");
    now = 506ms;
    registry.Expire();
    ReportCheck(registry.Samples()[0].sample.quality == Quality::stale, "freshness stale after deadline");

    for (const char *name : {"degC", "degF", "K"}) {
        const auto unit = ParseUnit(name);
        ReportCheck(unit.Ok(), "temperature unit parses");
        for (double input : {0.0, 1e-9, -1e-9, -40.0, 100.0, 1000000.0}) {
            const auto si = ToSi(input, unit.Ok() ? *unit.Get() : Unit::dimensionless);
            ReportCheck(si.Ok(), "temperature to SI");
            const auto output = FromSi(si.Ok() ? *si.Get() : 0.0,
                                       unit.Ok() ? *unit.Get() : Unit::dimensionless);
            ReportCheck(output.Ok(), "temperature from SI");
            ReportCheck(output.Ok() && std::abs(input - *output.Get()) <=
                      std::max(1e-5, 1e-6 * std::max(std::abs(input), std::abs(*output.Get()))),
                  "temperature round trip tolerance");
        }
    }
    const auto unknown = ParseUnit("furlongs");
    ReportCheck(!unknown.Ok(), "unknown unit rejected");
    ReportCheck(unknown.GetStatus().code == ErrorCode::unknown_unit, "unknown unit error code");

    const std::array<ExpressionNode, 3> nodes{{
        {Operation::greater, 0, Unit::dimensionless, 0, 1, 2, "/root"},
        {Operation::signal_reference, 0, Unit::dimensionless, 1, 0, 0, "/signal"},
        {Operation::literal, 100, Unit::degree_celsius, 0, 0, 0, "/threshold"}}};
    RuleDefinition definition{7, nodes, 2, Unit::kelvin, 0ns, MissingInputPolicy::treat_unavailable, 0ns};
    RuleEngine rule{clock, schedule};
    ReportCheck(rule.Load(definition, signals).Ok(), "rule loads unit literal and hysteresis");
    // A 100 degC threshold and 2 K band turn on above 102 degC, off at 98 degC.
    const std::array<double, 6> values{370.15, 373.15, 376.15, 373.15, 372.15, 370.15};
    const std::array<bool, 6> expected{false, false, true, true, true, false};
    for (std::size_t index = 0; index < values.size(); ++index) {
        now += 1ms;
        sample.value = values[index];
        sample.t_recv = now;
        ++sample.seq;
        ReportCheck(registry.Apply(1, sample).Ok(), "rule sample applied");
        const auto result = rule.Evaluate(registry.Samples(), registry.Generation());
        ReportCheck(result.quality == Quality::valid, "rule output valid");
        ReportCheck(result.current == expected[index], "rule threshold and hysteresis output");
    }
}
} // namespace signal_core
