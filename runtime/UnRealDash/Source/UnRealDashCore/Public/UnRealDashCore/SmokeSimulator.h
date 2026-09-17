#pragma once
#include "CoreMinimal.h"
#include "SignalCore/Clock.h"
#include "SignalCore/ExpirySchedule.h"
#include "SignalCore/Recording.h"
#include "SignalCore/SignalRegistry.h"
#include "SignalCore/SnapshotExchange.h"
#include "UnRealDashCore/SignalCoreAdapter.h"
#include <array>
#include <string>
#include <vector>

namespace UnRealDashCore
{
// Generates a signal-core scenario into memory and replays it against an injected clock. It lives
// here rather than in the game module because PLAN 4.1 makes this module the one that wraps
// signal-core: a game module calling signal-core directly would not link, and the layering in
// docs/ARCHITECTURE.md is what says which side of the boundary this belongs on.
struct FSmokeSimulatorOptions
{
    FString Scenario;            // A name signal_core::ParseScenario accepts.
    uint64 Seed = 0;
    double DurationSeconds = 0;
    int32 IntervalMilliseconds = 0;
};

// Whether signal-core recognises this scenario name. Exposed so callers can reject a bad name
// where it is configured rather than where it is used, without including signal-core headers.
UNREALDASHCORE_API bool IsKnownScenario(const FString& Name);

class UNREALDASHCORE_API FSmokeSimulator
{
public:
    explicit FSmokeSimulator(signal_core::Clock Clock);
    // Both return an empty string on success and a message on failure. Errors are values here for
    // the same reason they are in signal-core: a spike that asserts tells you nothing on a device.
    FString Start(const FSmokeSimulatorOptions& Options);
    FString Tick(FSignalSample& OutSample);

private:
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
};
}
