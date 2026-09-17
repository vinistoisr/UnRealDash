#include "UnRealDashCore/SmokeSimulator.h"
#include "SignalCore/Scenarios.h"

namespace UnRealDashCore
{

bool IsKnownScenario(const FString& Name)
{
    return signal_core::ParseScenario(TCHAR_TO_UTF8(*Name)).Ok();
}
FSmokeSimulator::FSmokeSimulator(signal_core::Clock InClock)
    : Schedule(Expiries), Exchange({Buffers[0], {}}, {Buffers[1], {}}, {Buffers[2], {}}), Clock(InClock)
{
}
FString FSmokeSimulator::Start(const FSmokeSimulatorOptions& Config)
{
    const auto Scenario = signal_core::ParseScenario(TCHAR_TO_UTF8(*Config.Scenario));
    if (!Scenario.Ok()) { return UTF8_TO_TCHAR(Scenario.GetStatus().message); }
    const auto Duration = signal_core::Time(static_cast<int64>(Config.DurationSeconds * 1.e9));
    const auto Interval = std::chrono::milliseconds(Config.IntervalMilliseconds);
    Text.clear();
    auto Status = signal_core::GenerateScenario({*Scenario.Get(), Config.Seed, Duration, Interval, std::chrono::milliseconds(500)},
        {&Text, [](void* Context, std::string_view Part) -> signal_core::Status {
            static_cast<std::string*>(Context)->append(Part);
            return {};
        }});
    if (!Status.Ok()) { return UTF8_TO_TCHAR(Status.message); }
    Samples.resize(2 * ((Duration.count() - 1) / std::chrono::duration_cast<signal_core::Time>(Interval).count() + 1));
    const auto Recording = signal_core::ReadRecording(Text, Signals, Samples);
    if (!Recording.Ok()) { return UTF8_TO_TCHAR(Recording.GetStatus().message); }
    Registry = MakeUnique<signal_core::SignalRegistry>(Clock, Recording.Get()->signals, Schedule, Exchange);
    Status = Registry->Initialize();
    if (!Status.Ok()) { return UTF8_TO_TCHAR(Status.message); }
    Replay = MakeUnique<signal_core::Replay>(Clock, *Registry);
    Status = Replay->Start(*Recording.Get(), false, signal_core::Time::zero());
    return Status.Ok() ? FString() : FString(UTF8_TO_TCHAR(Status.message));
}
FString FSmokeSimulator::Tick(FSignalSample& OutSample)
{
    if (!Replay) { return TEXT("Smoke replay has not started"); }
    const auto Status = Replay->Poll();
    if (!Status.Ok()) { return UTF8_TO_TCHAR(Status.message); }
    OutSample = ToEngineSample(Registry->Samples()[0].sample);
    return {};
}
}
