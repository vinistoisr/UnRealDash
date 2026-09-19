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
    // One signal id per entry of Samples, in the same order. Present is a COMPACTED list of the
    // ids that were received this publication, so its indices do not line up with Samples and a
    // consumer cannot use it to find out which signal a sample belongs to. A live binding needs
    // exactly that, so it gets its own aligned array rather than an index arithmetic rule someone
    // has to remember.
    TArray<uint32> SignalIds;
    TArray<uint32> Present;
    // PLAN 4.8's acquisition row lists the SAMPLE ids a frame held. Aligned with Samples, like
    // SignalIds and unlike the compacted Present, so a consumer can tell which sample a row is
    // about without an index arithmetic rule someone has to remember.
    TArray<uint64> SampleIds;
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
    // PLAN 4.9 requires a connector to report its backoff state. Zero for the connectors that
    // never retry, which is the honest reading for a scenario or a file: they have no schedule to
    // be waiting on.
    uint64 BackoffMilliseconds = 0, AttemptsSinceConnect = 0;
};
// A receive-only binary-telemetry-v1 client. Signals come from the definition pack's fields, so
// their numeric ids are the pack's and not the document's ordering; FDashBindingTable is told the
// mapping rather than assuming one.
struct FDashTcpOptions {
    FString Host = TEXT("127.0.0.1");
    uint16 Port = 35000;
    // The package's definition pack, as JSON text.
    FString DefinitionPackText;
    // Absolute path to the schema directory, for validating the pack.
    FString SchemaDirectory;
    // A file of recorded bytes instead of a socket. Same framer, same decoder, same mapping, so a
    // bug that only shows up in one of them cannot hide in the other. Empty means TCP.
    FString ReplayPath;
    bool bReplayLoop = false;
};
// The recording and all borrowed pipeline storage are owned behind the engine seam.
class UNREALDASHCORE_API FDashAcquisition {
  public:
    explicit FDashAcquisition(const FString& RecordingText, uint32 ExpectedSamplesPerFrame = 4,
                             const TArray<FAcquisitionThresholdRule>& Rules = {});
    explicit FDashAcquisition(const FDashTcpOptions& Options);
    // Signal name to the numeric id the pack gives it. Empty for the recording path, which numbers
    // signals by document position.
    const TMap<FString, uint32>& SignalIds() const;
    // Stops advancing the instant the registry and the replay pace against, so a scenario holds
    // and a capture lands at a known point of it. PLAN 4.7's -freeze-scenario-time.
    void FreezeScenarioTime();
    ~FDashAcquisition();
    FString Start();
    void Stop();
    bool AcquireFrameSnapshot(FFrameSnapshot& Out);
    void RecordSubmit(uint64 FrameIndex);
    void RequestAcknowledge(uint32 RuleId);
    // PLAN 4.8. Null detaches, which is what Close does before the log goes away under the
    // acquisition thread.
    void SetEventLog(class FDashEventLog* Log);
    // The pipeline's own count of samples Apply accepted, which is PLAN 4.8's published count.
    uint64 SamplesApplied() const;
    FConnectionHealth GetHealth() const;
  private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};
}
