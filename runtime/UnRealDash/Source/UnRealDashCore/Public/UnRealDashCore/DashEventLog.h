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

// One bound signal's contribution to a present row: what was actually on screen for it in that
// frame. A bound signal that has received nothing renders the missing-data presentation, which is
// a real thing on screen, so its entry is present with a null sample id rather than omitted.
struct FDashRenderedSignal
{
    uint32 Signal = 0;
    // Zero means no sample has ever arrived for this signal.
    uint64 SampleId = 0;
    // valid, stale, unavailable or invalid, as rendered rather than as stored.
    uint8 Quality = 0;
    // Which expiry firing this reading is displaying, or zero. The end endpoint of PLAN 4.8's
    // expiry-to-present latency is matched on this rather than on the signal, or a later
    // episode's frame could close an earlier expiry.
    uint64 Expiry = 0;
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
    uint64 Receives = 0;
    uint64 Acquires = 0;
    uint64 Submits = 0;
    uint64 Presents = 0;
    uint64 ExpiriesArmed = 0;
    uint64 ExpiryStatuses = 0;
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

    // PLAN 4.8's receive row. Called from the ACQUISITION thread, so it has its own ring: an
    // EventRowRing is single-producer by construction and the frame ring's producer is the game
    // thread.
    void WriteReceive(uint32 Signal, uint64 SampleId, int64 ReceiveNanoseconds, uint64 Sequence,
        uint64 Generation, uint8 Quality, uint8 AgeEvidence);
    // The acquisition and submit rows, from the game thread.
    void WriteAcquire(uint64 Frame, int64 Nanoseconds, TArrayView<const uint64> SampleIds);
    void WriteSubmit(uint64 Frame, int64 Nanoseconds);
    // What the bindings put on screen for a frame. Stashed rather than written: the present row
    // needs the present timestamp, which arrives from the render thread one or two frames later,
    // and it is written inside the same join the per-frame row already uses. One join, one
    // present timestamp, so the two rows cannot disagree about when a frame reached the screen.
    void WriteRendered(uint64 Frame, TArrayView<const FDashRenderedSignal> Rendered);

    // PLAN 4.8's armed-expiry and rule-expiry rows, and their one status row each. Called from
    // the acquisition thread, through the same ring the receive rows use.
    void WriteExpiryArmed(uint8 Kind, uint32 Id, uint64 Serial, uint64 Sample, uint64 Generation,
        int64 DeadlineNanoseconds, uint8 Becomes);
    void WriteExpiryFired(uint8 Kind, uint64 Serial, int64 Nanoseconds);
    void WriteExpiryCancelled(uint8 Kind, uint64 Serial, uint8 Reason, uint64 By);

    FDashEventLogCounts Counts() const;

private:
    TUniquePtr<struct FDashEventLogImpl> Impl;
};

} // namespace UnRealDashCore
