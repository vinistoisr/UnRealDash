#pragma once
#include "SignalCore/Clock.h"
#include "SignalCore/Units.h"
#include <limits>
namespace signal_core {
using Value = double;
using SignalId = std::uint32_t;
enum class Quality : std::uint8_t { valid, stale, unavailable, invalid };
enum class AgeEvidence : std::uint8_t { measured, unknown };
struct Signal {
    SignalId id{};
    char name[128]{};
    Unit unit{};
    Time deadline{};
    bool discrete{};
};
struct Sample {
    // PLAN 4.8's sample identity. Assigned by AcquisitionPipeline immediately before Apply
    // accepts the sample, from a counter that starts at 1 and never repeats within a run. Zero
    // means it never went through the pipeline, so a row carrying one is visible rather than
    // plausible. seq cannot serve: it is the source's sequence and repeats across signals and
    // across reconnects.
    std::uint64_t id{};
    Value value{std::numeric_limits<double>::quiet_NaN()};
    Unit unit{};
    std::uint64_t source{};
    std::uint64_t seq{};
    Time t_recv{};
    Time t_source{};
    bool has_source_time{};
    Quality quality{Quality::unavailable};
    AgeEvidence age_evidence{AgeEvidence::unknown};
    std::uint64_t generation{};
    Sample() = default;
    Sample(Value v, Unit u, Time received) : value(v), unit(u), t_recv(received), quality(Quality::valid) {}
};
struct SignalSample {
    SignalId signal{};
    Sample sample{};
    // Registry bookkeeping, independent of quality; reset on a generation change.
    bool received{};
};
} // namespace signal_core
