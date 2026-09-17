#pragma once
#include "CoreMinimal.h"
#include "UnRealDashCore/SignalCoreAdapter.h"
#include "UnRealDashCore/MonotonicClock.h"

namespace UnRealDashCore
{
// Owns the portable simulator behind an engine-only public interface.
struct FSmokeSimulatorOptions
{
    FString Scenario;            // A supported scenario name.
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
    explicit FSmokeSimulator(FMonotonicClock Clock);
    ~FSmokeSimulator();
    // Both return an empty string on success and a message on failure. Errors are values here for
    // the same reason they are in signal-core: a spike that asserts tells you nothing on a device.
    FString Start(const FSmokeSimulatorOptions& Options);
    FString Tick(FSignalSample& OutSample);

private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};
}
