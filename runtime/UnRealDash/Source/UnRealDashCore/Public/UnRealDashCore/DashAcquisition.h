#pragma once
#include "CoreMinimal.h"
#include "UnRealDashCore/SignalCoreAdapter.h"

namespace UnRealDashCore {
struct FAcquisitionThresholdRule {
    uint32 Id = 0, Signal = 0;
    double Threshold = 0, Hysteresis = 0;
    uint8 Unit = 0;
    int64 DebounceNanoseconds = 0;
};
struct FFrameSnapshot {
    TArray<FSignalSample> Samples;
    TArray<uint32> Present;
    uint64 FrameIndex = 0;
    int64 AcquisitionNanoseconds = 0;
    uint64 Generation = 0;
};
struct FConnectionHealth {
    uint64 Generation = 0;
    bool bConnected = false;
    int64 LastByteNanoseconds = 0;
    uint64 Reconnects = 0, Bytes = 0;
    FString LastError;
};
// The recording and all borrowed pipeline storage are owned behind the engine seam.
class UNREALDASHCORE_API FDashAcquisition {
  public:
    explicit FDashAcquisition(const FString& RecordingText, uint32 ExpectedSamplesPerFrame = 4,
                             const TArray<FAcquisitionThresholdRule>& Rules = {});
    ~FDashAcquisition();
    FString Start();
    void Stop();
    bool AcquireFrameSnapshot(FFrameSnapshot& Out);
    void RecordSubmit(uint64 FrameIndex);
    void RequestAcknowledge(uint32 RuleId);
    FConnectionHealth GetHealth() const;
  private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};
}
