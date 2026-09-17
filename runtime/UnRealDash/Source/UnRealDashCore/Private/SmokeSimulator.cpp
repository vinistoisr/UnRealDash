#include "UnRealDashCore/SmokeSimulator.h"
#include "SignalCore/Scenarios.h"
#include "SignalCore/Recording.h"
#include "SignalCoreAdapter.h"
#include <array>
#include <string>
#include <vector>

namespace UnRealDashCore
{

bool IsKnownScenario(const FString& Name)
{
    return signal_core::ParseScenario(TCHAR_TO_UTF8(*Name)).Ok();
}
struct FSmokeSimulator::FImpl {
    std::string Text;
    std::array<signal_core::Signal, 2> Signals;
    std::vector<signal_core::SignalSample> Samples;
    std::array<signal_core::Expiry, 2> Expiries;
    std::array<signal_core::SignalSample, 2> Buffers[3];
    signal_core::ExpirySchedule Schedule;
    signal_core::SnapshotExchange Exchange;
    TUniquePtr<signal_core::SignalRegistry> Registry;
    TUniquePtr<signal_core::Replay> Replay;
    signal_core::Clock Clock;
    FMonotonicClock EngineClock;
    explicit FImpl(FMonotonicClock InClock)
        : Schedule(Expiries), Exchange({Buffers[0], {}}, {Buffers[1], {}}, {Buffers[2], {}}),
          Clock{this, [](void* Context) { auto& Self = *static_cast<FImpl*>(Context);
              return signal_core::Time(Self.EngineClock.NowNanoseconds(Self.EngineClock.Context)); }}, EngineClock(InClock) {}
    FString Start(const FSmokeSimulatorOptions& Config);
    FString Tick(FSignalSample& OutSample);
};
FSmokeSimulator::FSmokeSimulator(FMonotonicClock InClock) : Impl(MakeUnique<FImpl>(InClock)) {}
FSmokeSimulator::~FSmokeSimulator() = default;
FString FSmokeSimulator::Start(const FSmokeSimulatorOptions& Config) { return Impl->Start(Config); }
FString FSmokeSimulator::Tick(FSignalSample& OutSample) { return Impl->Tick(OutSample); }
FString FSmokeSimulator::FImpl::Start(const FSmokeSimulatorOptions& Config)
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
FString FSmokeSimulator::FImpl::Tick(FSignalSample& OutSample)
{
    if (!Replay) { return TEXT("Smoke replay has not started"); }
    const auto Status = Replay->Poll();
    if (!Status.Ok()) { return UTF8_TO_TCHAR(Status.message); }
    OutSample = ToEngineSample(Registry->Samples()[0].sample);
    return {};
}
}
