#include "SignalCore/Scenarios.h"
#include <random>
namespace signal_core {
Result<Scenario> ParseScenario(std::string_view name) {
    constexpr const char *names[] = {"idle",       "acceleration", "high_temperature", "missing_signal",
                                     "disconnect", "reconnect",    "stale_heartbeat"};
    for (unsigned i = 0; i < std::size(names); ++i)
        if (name == names[i])
            return static_cast<Scenario>(i);
    return Error(ErrorCode::invalid_configuration, "unknown scenario '%.*s'", static_cast<int>(name.size()),
                 name.data());
}
Status GenerateScenario(const ScenarioOptions &options, TextSink sink, ScenarioObserver observer) {
    if (options.duration <= Time::zero() || options.interval <= Time::zero() || options.deadline <= Time::zero() ||
        static_cast<unsigned>(options.scenario) > 6)
        return Error(ErrorCode::invalid_configuration, "scenario timing or name");
    const Signal signals[] = {{1, "speed", Unit::metres_per_second, options.deadline, false},
                              {2, "temperature", Unit::kelvin, options.deadline, false}};
    auto status = WriteHeader(sink, signals);
    if (!status.Ok())
        return status;
    std::mt19937_64 random(options.seed);
    std::uint64_t seq = 0, generation = 0;
    bool disconnected = false;
    const auto steps = (options.duration.count() - 1) / options.interval.count() + 1;
    for (std::int64_t step = 0; step < steps; ++step) {
        const Time time(step * options.interval.count());
        const double phase = static_cast<double>(time.count()) / static_cast<double>(options.duration.count());
        const bool interruption = phase >= 0.25 && (options.scenario != Scenario::reconnect || phase < 0.5);
        const bool transport_down =
            interruption && (options.scenario == Scenario::disconnect || options.scenario == Scenario::reconnect);
        if (transport_down && !disconnected) {
            ++generation;
            disconnected = true;
        }
        if (!transport_down && disconnected) {
            ++generation;
            disconnected = false;
        }
        if (observer.event)
            observer.event(
                observer.context,
                {time, !transport_down, options.scenario == Scenario::stale_heartbeat && interruption, generation});
        if (transport_down || (options.scenario == Scenario::stale_heartbeat && interruption))
            continue;
        // A triangular ramp expressed as linear pieces avoids platform libm variation.
        const double ramp = phase < 0.5 ? phase * 2 : (1 - phase) * 2;
        const double noise = static_cast<double>(random() >> 11) * 0x1.0p-53;
        for (unsigned i = 0; i < 2; ++i) {
            if (i == 1 && options.scenario == Scenario::missing_signal && interruption)
                continue;
            double value = i == 0 ? 1 + noise * 0.1 : 293.15 + noise;
            if (i == 0 && options.scenario == Scenario::acceleration)
                value += ramp * 30;
            if (i == 1 && options.scenario == Scenario::high_temperature)
                value += ramp * 100;
            Sample sample(value, signals[i].unit, time);
            sample.source = 1;
            sample.seq = ++seq;
            sample.generation = generation;
            sample.age_evidence = i == 1 ? AgeEvidence::unknown : AgeEvidence::measured;
            sample.has_source_time = true;
            sample.t_source = time;
            status = WriteSample(sink, {signals[i].id, sample});
            if (!status.Ok())
                return status;
        }
    }
    return {};
}
} // namespace signal_core
