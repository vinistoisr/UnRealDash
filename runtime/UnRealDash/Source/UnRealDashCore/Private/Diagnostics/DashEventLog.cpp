#include "UnRealDashCore/DashEventLog.h"
#include "HAL/PlatformMemory.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "RHI.h"
#include "RHIStats.h"
#include "SignalCore/EventLog.h"
#include "UnRealDashCore/MonotonicClock.h"
#include <atomic>
#include <cstddef>

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <psapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace UnRealDashCore
{
namespace
{
// A frame row is about 190 bytes of JSON. 384 leaves room for the columns chunks 18 to 20 add
// without changing the slot size under them.
constexpr std::size_t kRowBytes = 384;
// A present row carries one nested object per bound signal, so it needs far more room than a
// frame row. Sixty-four bound signals at about forty bytes each, plus the frame's own columns.
constexpr std::size_t kWideRowBytes = 4096;
// The writer drains every kDrainMilliseconds, so this has to hold a drain interval's worth of
// frames at whatever rate the renderer actually runs. An unthrottled windowed run of a trivial
// document measured about 920 fps, which is 46 rows per interval; 1024 is headroom for twenty
// times that. Overflow is counted rather than silent.
constexpr std::size_t kFrameSlots = 1024;
// Receive rows arrive at the acquisition rate rather than the frame rate, which 4.3 puts at up to
// 200 Hz, so a 50 ms drain interval sees about ten. This is two orders of headroom.
constexpr std::size_t kReceiveSlots = 1024;
constexpr std::size_t kWideSlots = 256;
// The sampled stream is 1 Hz, but the drain cannot be: at 920 fps a one second drain interval
// overflowed any ring worth allocating, and dropped 7,974 of 11,060 frame rows on the first run.
// The thread therefore wakes twenty times a second to drain and writes its own row on the second.
constexpr double kDrainMilliseconds = 50.0;
// The render thread finishes a frame or two behind the game thread. Sixty-four is far more than
// that and costs nothing.
constexpr std::size_t kPresentSlots = 64;

struct FPresentStamp
{
    uint64 Frame = 0;
    int64 Nanoseconds = 0;
};

// Same single-producer single-consumer shape as the row ring, for a fixed struct rather than text.
class FPresentRing
{
public:
    bool Push(const FPresentStamp& Stamp)
    {
        const uint64 At = Head.load(std::memory_order_relaxed);
        if (At - Tail.load(std::memory_order_acquire) == kPresentSlots) return false;
        Slots[At % kPresentSlots] = Stamp;
        Head.store(At + 1, std::memory_order_release);
        return true;
    }
    bool Pop(FPresentStamp& Out)
    {
        const uint64 At = Tail.load(std::memory_order_relaxed);
        if (At == Head.load(std::memory_order_acquire)) return false;
        Out = Slots[At % kPresentSlots];
        Tail.store(At + 1, std::memory_order_release);
        return true;
    }
private:
    FPresentStamp Slots[kPresentSlots]{};
    std::atomic<uint64> Head{0}, Tail{0};
};
} // namespace

struct FDashEventLogImpl : public FRunnable
{
    signal_core::EventLog Log;
    signal_core::EventRowRing<kRowBytes, kFrameSlots> Rows;
    // One ring per producer thread. EventRowRing is single-producer by construction, and the
    // acquisition thread is not the game thread.
    signal_core::EventRowRing<kRowBytes, kReceiveSlots> ReceiveRows;
    signal_core::EventRowRing<kWideRowBytes, kWideSlots> WideRows;
    FPresentRing Presents;
    FMonotonicClock PlatformClock{};
    FDashEventLogOptions Options;

    // The sampler thread is the only thing that touches the file after Open. See
    // docs/build/chunk-17-event-log.md revision 2 C.
    FRunnableThread* Thread = nullptr;
    std::atomic<bool> bRunning{false};
    FEvent* Wake = nullptr;

    FDelegateHandle PresentHandle;

    // The per-frame row's memory columns come from the most recent sample rather than from a
    // syscall on the frame path. FPlatformMemory::GetStats and RHIGetTextureMemoryStats are both
    // system calls, and 4.8 asks for the columns rather than for a fresh reading per frame. The
    // header says so, so nobody reads them as per-frame measurements.
    std::atomic<uint64> ResidentBytes{0};
    std::atomic<uint64> TextureBytes{0};

    std::atomic<uint64> Frames{0};
    std::atomic<uint64> Samples{0};
    std::atomic<uint64> Dropped{0};
    std::atomic<uint64> Receives{0}, Acquires{0}, Submits{0}, PresentRows{0};

    // Game thread only.
    struct FPendingFrame
    {
        uint64 Frame = 0;
        int64 StartNanoseconds = 0;
        int64 FrameNanoseconds = 0;
    };
    TArray<FPendingFrame> Pending;
    int64 MissedThresholdNanoseconds = 0;
    // What the bindings rendered, per frame, waiting for that frame's present timestamp. Game
    // thread only, like Pending, and drained by the same join.
    TMap<uint64, TArray<FDashRenderedSignal>> Rendered;

    int64 Now() const { return PlatformClock.NowNanoseconds(PlatformClock.Context); }

    void SampleCounters(uint64& OutResident, uint64& OutTexture, uint64& OutHandles) const
    {
        const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
        OutResident = Memory.UsedPhysical;
        // Streaming plus non-streaming, which is what the engine actually has in texture memory.
        // TotalGraphicsMemory is the budget rather than the usage and would report a constant.
        FTextureMemoryStats Textures;
        RHIGetTextureMemoryStats(Textures);
        OutTexture = Textures.StreamingMemorySize + Textures.NonStreamingMemorySize;
        OutHandles = 0;
#if PLATFORM_WINDOWS
        DWORD Handles = 0;
        if (GetProcessHandleCount(GetCurrentProcess(), &Handles)) OutHandles = Handles;
#endif
    }

    // FRunnable. One row a second for the life of the process, on its own thread rather than on
    // the frame loop, because 6.6's whole point is that a run that drops to 5 fps still produces
    // a temperature and memory trace.
    uint32 Run() override
    {
        char Buffer[kWideRowBytes];
        int64 NextSample = Now();
        while (bRunning.load(std::memory_order_acquire))
        {
            Drain(Buffer);
            const int64 At = Now();
            if (At >= NextSample)
            {
                WriteSample();
                // Advanced by exactly a second rather than set from now, so the stream does not
                // drift later and later behind one row per wake.
                NextSample += 1000000000;
                if (NextSample < At) NextSample = At + 1000000000;
                (void)Log.Flush();
            }
            if (Wake) Wake->Wait(FTimespan::FromMilliseconds(kDrainMilliseconds));
        }
        // Whatever the game thread pushed after the last wake still belongs in the file.
        Drain(Buffer);
        return 0;
    }
    void Stop() override { bRunning.store(false, std::memory_order_release); if (Wake) Wake->Trigger(); }

    void Drain(char (&Buffer)[kWideRowBytes])
    {
        std::size_t Length = 0;
        while (Rows.Pop(std::span<char>(Buffer, kRowBytes), Length))
            (void)Log.Write(std::span<const char>(Buffer, Length));
        while (ReceiveRows.Pop(std::span<char>(Buffer, kRowBytes), Length))
            (void)Log.Write(std::span<const char>(Buffer, Length));
        while (WideRows.Pop(std::span<char>(Buffer, kWideRowBytes), Length))
            (void)Log.Write(std::span<const char>(Buffer, Length));
    }

    void WriteSample()
    {
        uint64 Resident = 0, Texture = 0, Handles = 0;
        SampleCounters(Resident, Texture, Handles);
        ResidentBytes.store(Resident, std::memory_order_relaxed);
        TextureBytes.store(Texture, std::memory_order_relaxed);

        char Storage[kRowBytes];
        signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
        Row.Begin("sampled")
            .Key("t").Integer(Now())
            .Key("resident_bytes").Unsigned(Resident)
            .Key("texture_bytes").Unsigned(Texture)
            .Key("handles").Unsigned(Handles);
        // Windows exposes no in-process temperature source. 6.5 names nvidia-smi, which is a
        // process per sample, so the harness reads it and this column says so rather than
        // inventing a number. The reason is in the header.
        Row.Key("temperature_c").Null().End();
        if (Log.Write(Row).Ok()) Samples.fetch_add(1, std::memory_order_relaxed);
        else Dropped.fetch_add(1, std::memory_order_relaxed);
    }
};

FDashEventLog::FDashEventLog() = default;
FDashEventLog::~FDashEventLog() { Close(); }
bool FDashEventLog::IsOpen() const { return Impl.IsValid(); }

FString FDashEventLog::Open(const FDashEventLogOptions& Options)
{
    if (Impl.IsValid()) return TEXT("the event log is already open");
    if (Options.Path.IsEmpty()) return TEXT("the event log needs a path");

    TUniquePtr<FDashEventLogImpl> Candidate = MakeUnique<FDashEventLogImpl>();
    Candidate->Options = Options;
    Candidate->PlatformClock = MakePlatformClock();
    const double Interval = Options.TargetFrameRate > 0 ? 1.0 / Options.TargetFrameRate : 1.0 / 60.0;
    // 1.5 intervals, written into the header as missed_frame_rule.
    Candidate->MissedThresholdNanoseconds = static_cast<int64>(Interval * 1.5 * 1e9);

    signal_core::Clock Clock{};
    Clock.context = Candidate.Get();
    Clock.now = [](void* Context) {
        return signal_core::Time(static_cast<FDashEventLogImpl*>(Context)->Now());
    };
    const signal_core::Status Opened = Candidate->Log.Open(TCHAR_TO_UTF8(*Options.Path), Clock,
        std::chrono::nanoseconds(static_cast<int64>(Options.FlushSeconds * 1e9)));
    if (!Opened.Ok()) return FString(Opened.message);

    // The schema and the header are written first and flushed before anything else. EventLog
    // cannot do this itself: it does not know which record types a run can write.
    {
        char Storage[1024];
        signal_core::RowBuilder Row(std::span<char>(Storage, sizeof(Storage)));
        Row.Begin("schema")
            .Key("records").BeginObject()
            .Key("frame").BeginArray()
                .String("frame").String("t_start").String("frame_ns").String("t_present")
                .String("missed").String("resident_bytes").String("texture_bytes")
            .EndArray()
            .Key("sampled").BeginArray()
                .String("t").String("resident_bytes").String("texture_bytes").String("handles")
                .String("temperature_c")
            .EndArray()
            .Key("receive").BeginArray()
                .String("sample").String("signal").String("t_recv").String("seq")
                .String("generation").String("quality").String("age_evidence")
            .EndArray()
            .Key("acquire").BeginArray()
                .String("frame").String("t_acquire").String("samples")
            .EndArray()
            .Key("submit").BeginArray()
                .String("frame").String("t_submit")
            .EndArray()
            .Key("present").BeginArray()
                .String("frame").String("t_present").String("signals")
            .EndArray()
            .EndObject()
            .End();
        if (!Candidate->Log.Write(Row).Ok()) return TEXT("the event log schema record did not write");
    }
    {
        char Storage[1024];
        signal_core::RowBuilder Row(std::span<char>(Storage, sizeof(Storage)));
        Row.Begin("header")
            .Key("build_id").String(TCHAR_TO_UTF8(*Options.BuildId))
            .Key("platform").String(TCHAR_TO_UTF8(ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName())))
            .Key("rhi").String(TCHAR_TO_UTF8(*Options.Rhi))
            .Key("width").Integer(Options.Resolution.X)
            .Key("height").Integer(Options.Resolution.Y)
            .Key("clock").String("monotonic_nanoseconds")
            .Key("target_fps").Number(Options.TargetFrameRate)
            .Key("present_source").String("end_of_render_thread_frame")
            .Key("missed_frame_rule").String("frame_ns > 1.5 * (1e9 / target_fps)")
            .Key("frame_memory_source").String("most_recent_sampled_row, not a per-frame syscall")
            .Key("temperature_source").String("not measured: no in-process source on this platform, see PLAN 6.5")
            // PLAN 4.8 asks the present row to name the sample an interpolated value came from.
            // Established rather than assumed: see the chunk 18 report.
            .Key("interpolation").String("none: the Stage 0 binding path renders samples directly, so derived_from is always null")
            .End();
        if (!Candidate->Log.Write(Row).Ok()) return TEXT("the event log header record did not write");
    }
    (void)Candidate->Log.Flush();

    // The render thread stamps each frame it finishes. GFrameCounterRenderThread is the same
    // numbering the game thread uses, offset in time, which is what lets the two sides be joined
    // by frame index rather than by arrival order.
    FDashEventLogImpl* Raw = Candidate.Get();
    Raw->PresentHandle = FCoreDelegates::OnEndFrameRT.AddLambda([Raw]()
    {
        FPresentStamp Stamp;
        Stamp.Frame = GFrameCounterRenderThread;
        Stamp.Nanoseconds = Raw->Now();
        if (!Raw->Presents.Push(Stamp)) Raw->Dropped.fetch_add(1, std::memory_order_relaxed);
    });

    Raw->Wake = FPlatformProcess::GetSynchEventFromPool(false);
    Raw->bRunning.store(true, std::memory_order_release);
    Raw->Thread = FRunnableThread::Create(Raw, TEXT("DashEventLog"), 0, TPri_BelowNormal);
    if (!Raw->Thread) return TEXT("the event log sampler thread did not start");

    Impl = MoveTemp(Candidate);
    return FString();
}

void FDashEventLog::Tick(float DeltaSeconds)
{
    if (!Impl.IsValid()) return;
    const int64 Now = Impl->Now();
    const int64 FrameNanoseconds = static_cast<int64>(static_cast<double>(DeltaSeconds) * 1e9);

    FDashEventLogImpl::FPendingFrame Frame;
    Frame.Frame = GFrameCounter;
    Frame.StartNanoseconds = Now - FrameNanoseconds;
    Frame.FrameNanoseconds = FrameNanoseconds;
    Impl->Pending.Add(Frame);

    // Join whatever the render thread has finished since the last tick. A frame with no present
    // stamp is not written yet, because writing it with a guessed timestamp would put a number
    // into 6.5's latency figures that nothing measured.
    FPresentStamp Stamp;
    while (Impl->Presents.Pop(Stamp))
    {
        const int32 Index = Impl->Pending.IndexOfByPredicate(
            [&Stamp](const FDashEventLogImpl::FPendingFrame& Candidate) { return Candidate.Frame == Stamp.Frame; });
        if (Index == INDEX_NONE) continue;
        const FDashEventLogImpl::FPendingFrame Done = Impl->Pending[Index];
        Impl->Pending.RemoveAt(Index);

        char Storage[kRowBytes];
        signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
        Row.Begin("frame")
            .Key("frame").Unsigned(Done.Frame)
            .Key("t_start").Integer(Done.StartNanoseconds)
            .Key("frame_ns").Integer(Done.FrameNanoseconds)
            .Key("t_present").Integer(Stamp.Nanoseconds)
            .Key("missed").Boolean(Done.FrameNanoseconds > Impl->MissedThresholdNanoseconds)
            .Key("resident_bytes").Unsigned(Impl->ResidentBytes.load(std::memory_order_relaxed))
            .Key("texture_bytes").Unsigned(Impl->TextureBytes.load(std::memory_order_relaxed))
            .End();
        if (Impl->Rows.Push(Row.View())) Impl->Frames.fetch_add(1, std::memory_order_relaxed);
        else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);

        // The present row, from the same join and the same timestamp. A second capture would let
        // the two rows disagree about when this frame reached the screen, which is chunk 17
        // revision 2 A.
        if (const TArray<FDashRenderedSignal>* Shown = Impl->Rendered.Find(Done.Frame))
        {
            char Wide[kWideRowBytes];
            signal_core::RowBuilder Present(std::span<char>(Wide, kWideRowBytes));
            Present.Begin("present")
                .Key("frame").Unsigned(Done.Frame)
                .Key("t_present").Integer(Stamp.Nanoseconds)
                .Key("signals").BeginArray();
            for (const FDashRenderedSignal& Entry : *Shown)
            {
                Present.BeginObject().Key("signal").Unsigned(Entry.Signal);
                // Null rather than omitted: a bound signal with no sample renders the
                // missing-data presentation, which is a real thing on screen.
                Present.Key("sample");
                if (Entry.SampleId != 0) Present.Unsigned(Entry.SampleId); else Present.Null();
                Present.Key("quality").Unsigned(Entry.Quality)
                    // Chunk 19 fills the expiry; the Stage 0 path never interpolates.
                    .Key("expiry").Null()
                    .Key("derived_from").Null()
                    .EndObject();
            }
            Present.EndArray().End();
            if (Impl->WideRows.Push(Present.View())) Impl->PresentRows.fetch_add(1, std::memory_order_relaxed);
            else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
            Impl->Rendered.Remove(Done.Frame);
        }
    }

    // A frame whose present stamp never arrives would otherwise accumulate for the life of the
    // run. Two seconds at 60 fps is far beyond any real pipeline depth.
    constexpr int32 kMaxPending = 120;
    if (Impl->Pending.Num() > kMaxPending)
    {
        const int32 Excess = Impl->Pending.Num() - kMaxPending;
        for (int32 Index = 0; Index < Excess; ++Index) Impl->Rendered.Remove(Impl->Pending[Index].Frame);
        Impl->Dropped.fetch_add(static_cast<uint64>(Excess), std::memory_order_relaxed);
        Impl->Pending.RemoveAt(0, Excess);
    }
}

void FDashEventLog::Close()
{
    if (!Impl.IsValid()) return;
    if (Impl->PresentHandle.IsValid()) FCoreDelegates::OnEndFrameRT.Remove(Impl->PresentHandle);
    Impl->Stop();
    if (Impl->Thread)
    {
        Impl->Thread->WaitForCompletion();
        delete Impl->Thread;
        Impl->Thread = nullptr;
    }
    if (Impl->Wake)
    {
        FPlatformProcess::ReturnSynchEventToPool(Impl->Wake);
        Impl->Wake = nullptr;
    }
    (void)Impl->Log.Close();
    Impl.Reset();
}

void FDashEventLog::WriteReceive(uint32 Signal, uint64 SampleId, int64 ReceiveNanoseconds, uint64 Sequence,
    uint64 Generation, uint8 Quality, uint8 AgeEvidence)
{
    if (!Impl.IsValid()) return;
    // Acquisition thread. Formats into a stack buffer and pushes; no allocation and no file.
    char Storage[kRowBytes];
    signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
    Row.Begin("receive")
        .Key("sample").Unsigned(SampleId)
        .Key("signal").Unsigned(Signal)
        .Key("t_recv").Integer(ReceiveNanoseconds)
        .Key("seq").Unsigned(Sequence)
        .Key("generation").Unsigned(Generation)
        .Key("quality").Unsigned(Quality)
        .Key("age_evidence").Unsigned(AgeEvidence)
        .End();
    if (Impl->ReceiveRows.Push(Row.View())) Impl->Receives.fetch_add(1, std::memory_order_relaxed);
    else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
}

void FDashEventLog::WriteAcquire(uint64 Frame, int64 Nanoseconds, TArrayView<const uint64> SampleIds)
{
    if (!Impl.IsValid()) return;
    char Storage[kWideRowBytes];
    signal_core::RowBuilder Row(std::span<char>(Storage, kWideRowBytes));
    Row.Begin("acquire").Key("frame").Unsigned(Frame).Key("t_acquire").Integer(Nanoseconds)
        .Key("samples").BeginArray();
    for (const uint64 Id : SampleIds) Row.Unsigned(Id);
    Row.EndArray().End();
    if (Impl->WideRows.Push(Row.View())) Impl->Acquires.fetch_add(1, std::memory_order_relaxed);
    else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
}

void FDashEventLog::WriteSubmit(uint64 Frame, int64 Nanoseconds)
{
    if (!Impl.IsValid()) return;
    char Storage[kRowBytes];
    signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
    Row.Begin("submit").Key("frame").Unsigned(Frame).Key("t_submit").Integer(Nanoseconds).End();
    if (Impl->Rows.Push(Row.View())) Impl->Submits.fetch_add(1, std::memory_order_relaxed);
    else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
}

void FDashEventLog::WriteRendered(uint64 Frame, TArrayView<const FDashRenderedSignal> Rendered)
{
    if (!Impl.IsValid()) return;
    // Stashed, not written. The present row needs this frame's present timestamp, which the
    // render thread has not produced yet.
    TArray<FDashRenderedSignal>& Slot = Impl->Rendered.FindOrAdd(Frame);
    Slot.Reset(Rendered.Num());
    Slot.Append(Rendered.GetData(), Rendered.Num());
}

FDashEventLogCounts FDashEventLog::Counts() const
{
    FDashEventLogCounts Counts;
    if (!Impl.IsValid()) return Counts;
    const signal_core::EventLogStats& Stats = Impl->Log.Stats();
    Counts.Records = Stats.records;
    Counts.Bytes = Stats.bytes;
    Counts.Dropped = Stats.dropped + Impl->Dropped.load(std::memory_order_relaxed) + Impl->Rows.Drops()
        + Impl->ReceiveRows.Drops() + Impl->WideRows.Drops();
    Counts.Frames = Impl->Frames.load(std::memory_order_relaxed);
    Counts.Samples = Impl->Samples.load(std::memory_order_relaxed);
    Counts.Receives = Impl->Receives.load(std::memory_order_relaxed);
    Counts.Acquires = Impl->Acquires.load(std::memory_order_relaxed);
    Counts.Submits = Impl->Submits.load(std::memory_order_relaxed);
    Counts.Presents = Impl->PresentRows.load(std::memory_order_relaxed);
    return Counts;
}

} // namespace UnRealDashCore
