#pragma once
#include "SignalCore/Export.h"
#include "SignalCore/Recording.h"
namespace signal_core {
enum class Scenario : std::uint8_t {
    idle,
    acceleration,
    high_temperature,
    missing_signal,
    disconnect,
    reconnect,
    stale_heartbeat
};
SIGNALCORE_API Result<Scenario> ParseScenario(std::string_view name);
struct ScenarioOptions {
    Scenario scenario{};
    std::uint64_t seed{};
    Time duration{};
    Time interval{};
    Time deadline{};
};
struct ScenarioEvent {
    Time time{};
    bool connected{};
    bool heartbeat{};
    std::uint64_t generation{};
};
struct ScenarioObserver {
    void *context{};
    void (*event)(void *, const ScenarioEvent &){};
};
SIGNALCORE_API Status GenerateScenario(const ScenarioOptions &options, TextSink sink, ScenarioObserver observer = {});
} // namespace signal_core
