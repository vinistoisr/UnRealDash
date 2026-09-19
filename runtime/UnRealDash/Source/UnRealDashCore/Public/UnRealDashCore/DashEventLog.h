#pragma once
#include "CoreMinimal.h"

namespace UnRealDashCore
{

// PLAN 4.8's event log, engine side. The writer, the schema and the flush cadence live in
// signal-core; this owns the two things that cannot: reading the platform's counters, and knowing
// when a frame started and when the render thread finished with it.
//
// No signal_core name appears in this header, so the game module can drive the log without
// reaching past UnRealDashCore, which is the one module allowed to call signal-core.
struct FDashEventLogOptions
{
    // Where the log goes. The player logs the absolute path so a gate does not have to guess it.
    FString Path;
    FString BuildId;
    FString Rhi;
    FIntPoint Resolution = FIntPoint(0, 0);
    // Only used to decide what counts as a missed frame. The rule goes in the header so a reader
    // never has to guess what "missed frames: 4" was measured against.
    double TargetFrameRate = 60.0;
    // 4.8's ceiling on what a kill can cost.
    double FlushSeconds = 1.0;
};

struct FDashEventLogCounts
{
    uint64 Records = 0;
    uint64 Bytes = 0;
    // Rows that did not fit their buffer or their ring. Counted, never silent: a log quietly
    // missing rows would make every count computed from it wrong.
    uint64 Dropped = 0;
    uint64 Frames = 0;
    uint64 Samples = 0;
};

class UNREALDASHCORE_API FDashEventLog
{
public:
    FDashEventLog();
    ~FDashEventLog();
    FDashEventLog(const FDashEventLog&) = delete;
    FDashEventLog& operator=(const FDashEventLog&) = delete;

    // Returns an empty string on success, and a reason on failure. A failure is not fatal: a
    // dashboard that refuses to start because it could not write telemetry about itself is worse
    // than one that starts without it.
    FString Open(const FDashEventLogOptions& Options);
    void Close();
    bool IsOpen() const;

    // Game thread, once per tick. Records when this frame began and how long the last one took,
    // and emits the rows whose present timestamps have since arrived from the render thread.
    void Tick(float DeltaSeconds);

    FDashEventLogCounts Counts() const;

private:
    TUniquePtr<struct FDashEventLogImpl> Impl;
};

} // namespace UnRealDashCore
