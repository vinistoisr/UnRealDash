#include "UnRealDashCore/DashEventLog.h"
#include "HAL/PlatformMemory.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformTime.h"
#include "RenderingThread.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#if PLATFORM_ANDROID
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#endif
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

DEFINE_LOG_CATEGORY_STATIC(LogDashEvents, Log, All);

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
    uint64 Counter = 0;
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
    // Asset completions are render-thread producers, so they cannot share a game-thread ring.
    signal_core::EventRowRing<kWideRowBytes, kWideSlots> AssetRows;
    FPresentRing Presents;
    FMonotonicClock PlatformClock{};
    FDashEventLogOptions Options;

    // The sampler thread owns the file, including open and close. See
    // docs/build/chunk-17-event-log.md revision 2 C.
    FRunnableThread* Thread = nullptr;
    std::atomic<bool> bRunning{false};
    FEvent* Wake = nullptr;
    FEvent* Initialized = nullptr;
    FString InitFailure;
    FString Initialize();
    void JoinPresents();
    uint64 LaunchReturnCounter = 0;
    uint64 CounterFrequency = 0;
    uint64 ProcessStartTicks = 0, ProcessTicksPerSecond = 0;
    int64 BootOffsetNanoseconds = 0;
    bool bBootClockAvailable = false;
    int64 BootClockPairSpanNanoseconds = 0;
    FString AmStartOutput, AmLaunchState;
    char AmOutputUtf8[2048]{}, AmStateUtf8[16]{};
    double AndroidSpreadNanoseconds = -1;
    FString ThermalPath;
    FString TemperatureSource = TEXT("not measured: no in-process source on this platform, see PLAN 6.5");
    bool bFirstUsable = false;

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
    std::atomic<uint64> Armed{0}, Statuses{0};
    std::atomic<uint64> WrittenRecords{0}, WrittenBytes{0}, WriteDrops{0};
    bool bWriteFailureReported = false;
    uint64 ReportedDrops = 0;

    void RecordFailure(const signal_core::Status& Status)
    {
        if (!Status.Ok() && !bWriteFailureReported)
        {
            bWriteFailureReported = true;
            UE_LOG(LogDashEvents, Error, TEXT("Event log write unavailable: %s; run continues"), UTF8_TO_TCHAR(Status.message));
        }
    }

    void PublishCounts()
    {
        const uint64 ProducerDrops = Dropped.load(std::memory_order_relaxed);
        if (ProducerDrops != ReportedDrops)
        {
            ReportedDrops = ProducerDrops;
            UE_LOG(LogDashEvents, Warning, TEXT("Event log dropped %llu rows: bounded row, ring or pending-frame capacity exceeded"), ProducerDrops);
        }
        const auto& Stats = Log.Stats();
        WrittenRecords.store(Stats.records, std::memory_order_relaxed);
        WrittenBytes.store(Stats.bytes, std::memory_order_relaxed);
        WriteDrops.store(Stats.dropped, std::memory_order_relaxed);
    }

    // Game thread only.
    struct FPendingFrame
    {
        uint64 Frame = 0;
        int64 StartNanoseconds = 0;
        int64 FrameNanoseconds = 0;
        // What the bindings rendered for this frame, waiting for its present timestamp. Held on
        // the pending entry rather than in a second map keyed by the same frame: with the two
        // separate, the join could find the frame and miss the rendered set, which is exactly
        // what happened under -udash-hold-frame-ms and cost every present row in the run.
        bool bRendered = false;
        FDashRenderedSignal Rendered[64]{};
        int32 RenderedCount = 0;
    };
    static constexpr int32 MaxPending = 120;
    FPendingFrame Pending[MaxPending]{};
    uint64 PendingHead = 0, PendingTail = 0;
    int64 MissedThresholdNanoseconds = 0;

    FPendingFrame* Find(uint64 Frame)
    {
        for (uint64 At = PendingTail; At < PendingHead; ++At)
            if (Pending[At % MaxPending].Frame == Frame) return &Pending[At % MaxPending];
        return nullptr;
    }

    int64 Now() const { return PlatformClock.NowNanoseconds(PlatformClock.Context); }

    bool SampleCounters(uint64& OutResident, uint64& OutTexture, uint64& OutHandles) const
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
        return true;
#elif PLATFORM_ANDROID
        DIR* Directory = opendir("/proc/self/fd");
        if (!Directory) return false;
        while (const dirent* Entry = readdir(Directory))
            if (Entry->d_name[0] != '.') ++OutHandles;
        closedir(Directory);
        return true;
#else
        return true;
#endif
    }

    // FRunnable. One row a second for the life of the process, on its own thread rather than on
    // the frame loop, because 6.6's whole point is that a run that drops to 5 fps still produces
    // a temperature and memory trace.
    uint32 Run() override
    {
        InitFailure = Initialize();
        Initialized->Trigger();
        if (!InitFailure.IsEmpty()) { (void)Log.Close(); return 0; }
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
                RecordFailure(Log.Flush());
            }
            PublishCounts();
            if (Wake) Wake->Wait(FTimespan::FromMilliseconds(kDrainMilliseconds));
        }
        // Whatever the game thread pushed after the last wake still belongs in the file.
        Drain(Buffer);
        RecordFailure(Log.Close());
        PublishCounts();
        return 0;
    }
    void Stop() override { bRunning.store(false, std::memory_order_release); if (Wake) Wake->Trigger(); }

    void Drain(char (&Buffer)[kWideRowBytes])
    {
        std::size_t Length = 0;
        while (Rows.Pop(std::span<char>(Buffer, kRowBytes), Length))
            RecordFailure(Log.Write(std::span<const char>(Buffer, Length)));
        while (ReceiveRows.Pop(std::span<char>(Buffer, kRowBytes), Length))
            RecordFailure(Log.Write(std::span<const char>(Buffer, Length)));
        while (AssetRows.Pop(std::span<char>(Buffer, kWideRowBytes), Length))
            RecordFailure(Log.Write(std::span<const char>(Buffer, Length)));
        while (WideRows.Pop(std::span<char>(Buffer, kWideRowBytes), Length))
            RecordFailure(Log.Write(std::span<const char>(Buffer, Length)));
    }

    void WriteSample()
    {
        uint64 Resident = 0, Texture = 0, Handles = 0;
        const bool HasHandles = SampleCounters(Resident, Texture, Handles);
        ResidentBytes.store(Resident, std::memory_order_relaxed);
        TextureBytes.store(Texture, std::memory_order_relaxed);

        char Storage[kRowBytes];
        signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
        Row.Begin("sampled")
            .Key("t").Integer(Now())
            .Key("resident_bytes").Unsigned(Resident)
            .Key("texture_bytes").Unsigned(Texture)
            .Key("handles");
        if (HasHandles) Row.Unsigned(Handles); else Row.Null();
        // Windows exposes no in-process temperature source. 6.5 names nvidia-smi, which is a
        // process per sample, so the harness reads it and this column says so rather than
        // inventing a number. The reason is in the header.
        Row.Key("temperature_c");
#if PLATFORM_ANDROID
        double Millidegrees = 0;
        FILE* Temperature = ThermalPath.IsEmpty() ? nullptr : std::fopen(TCHAR_TO_UTF8(*ThermalPath), "r");
        const bool HasTemperature = Temperature && std::fscanf(Temperature, "%lf", &Millidegrees) == 1 && FMath::IsFinite(Millidegrees);
        if (Temperature) std::fclose(Temperature);
        if (HasTemperature) Row.Number(Millidegrees / 1000.0); else Row.Null();
#else
        Row.Null();
#endif
        Row.End();
        const auto Written = Log.Write(Row);
        if (Written.Ok()) Samples.fetch_add(1, std::memory_order_relaxed);
        else RecordFailure(Written);
    }
};

FDashEventLog::FDashEventLog() = default;
FDashEventLog::~FDashEventLog() { Close(); }
bool FDashEventLog::IsOpen() const { return Impl.IsValid(); }

FString FDashEventLogImpl::Initialize()
{
    CounterFrequency = static_cast<uint64>(0.5 + 1.0 / FPlatformTime::GetSecondsPerCycle64());
    if (!Options.LaunchEvidencePath.IsEmpty())
    {
        // The launcher can return before or after us. Wait only during startup, never in a frame.
        for (int32 Attempt = 0; Attempt < 100 && !IFileManager::Get().FileExists(*Options.LaunchEvidencePath); ++Attempt)
            FPlatformProcess::SleepNoStats(0.01f);
        FString Text;
        TSharedPtr<FJsonObject> Evidence;
        if (FFileHelper::LoadFileToString(Text, *Options.LaunchEvidencePath) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Evidence) && Evidence.IsValid())
        {
            FString Counter, Start;
            Evidence->TryGetStringField(TEXT("launch_counter"), Start);
            Evidence->TryGetStringField(TEXT("return_counter"), Counter);
            if (FCString::Strtoui64(*Start, nullptr, 10) == Options.LaunchCounter)
                LaunchReturnCounter = FCString::Strtoui64(*Counter, nullptr, 10);
            Evidence->TryGetStringField(TEXT("am_start_output"), AmStartOutput);
            Evidence->TryGetStringField(TEXT("am_launch_state"), AmLaunchState);
            Evidence->TryGetNumberField(TEXT("observed_spread_ns"), AndroidSpreadNanoseconds);
        }
        else UE_LOG(LogDashEvents, Warning, TEXT("Launch evidence unavailable: %s"), *Options.LaunchEvidencePath);
    }
#if PLATFORM_ANDROID
    const long Ticks = sysconf(_SC_CLK_TCK);
    if (Ticks > 0) ProcessTicksPerSecond = static_cast<uint64>(Ticks);
    FILE* Stat = std::fopen("/proc/self/stat", "r");
    char StatLine[4096]{};
    if (Stat)
    {
        if (std::fgets(StatLine, sizeof(StatLine), Stat))
        {
            // comm can contain spaces and parentheses; only the last ')' ends field 2.
            char* Cursor = std::strrchr(StatLine, ')');
            if (Cursor)
            {
                ++Cursor;
                for (int Field = 3; Field <= 22; ++Field)
                {
                    while (*Cursor == ' ') ++Cursor;
                    if (Field == 22) { ProcessStartTicks = std::strtoull(Cursor, nullptr, 10); break; }
                    while (*Cursor && *Cursor != ' ') ++Cursor;
                }
            }
        }
        std::fclose(Stat);
    }
    timespec Boot{};
    const int64 BeforeBootRead = Now();
    if (clock_gettime(CLOCK_BOOTTIME, &Boot) == 0)
    {
        const int64 AfterBootRead = Now();
        BootClockPairSpanNanoseconds = AfterBootRead - BeforeBootRead;
        BootOffsetNanoseconds = static_cast<int64>(Boot.tv_sec) * 1000000000 + Boot.tv_nsec -
            (BeforeBootRead + BootClockPairSpanNanoseconds / 2);
        bBootClockAvailable = true;
    }
    else UE_LOG(LogDashEvents, Warning, TEXT("Android boot-time clock unavailable"));
    if (!ProcessStartTicks || !ProcessTicksPerSecond)
        UE_LOG(LogDashEvents, Warning, TEXT("Android process-start endpoint unavailable from /proc/self/stat"));
    // NDK thermal status is a severity, not degrees Celsius. Sysfs supplies the required unit.
    TemperatureSource = TEXT("not measured: no readable CPU or GPU thermal zone");
    DIR* Zones = opendir("/sys/class/thermal");
    if (Zones)
    {
        while (const dirent* Entry = readdir(Zones))
        {
            if (std::strncmp(Entry->d_name, "thermal_zone", 12) != 0) continue;
            const FString Zone = FString(TEXT("/sys/class/thermal/")) + UTF8_TO_TCHAR(Entry->d_name);
            FILE* Type = std::fopen(TCHAR_TO_UTF8(*(Zone / TEXT("type"))), "r");
            char Name[256]{};
            if (Type) { (void)std::fgets(Name, sizeof(Name), Type); std::fclose(Type); }
            const FString Lower = FString(UTF8_TO_TCHAR(Name)).ToLower();
            if (!Lower.Contains(TEXT("cpu")) && !Lower.Contains(TEXT("gpu"))) continue;
            FILE* Temp = std::fopen(TCHAR_TO_UTF8(*(Zone / TEXT("temp"))), "r");
            double Value = 0;
            const bool Readable = Temp && std::fscanf(Temp, "%lf", &Value) == 1 && FMath::IsFinite(Value);
            if (Temp) std::fclose(Temp);
            if (!Readable) continue;
            ThermalPath = Zone / TEXT("temp");
            TemperatureSource = ThermalPath + TEXT(" type=") + Lower.TrimStartAndEnd();
            break;
        }
        closedir(Zones);
    }
#endif
    const FTCHARToUTF8 Output(*AmStartOutput), State(*AmLaunchState);
    if (Output.Length() < sizeof(AmOutputUtf8) && State.Length() < sizeof(AmStateUtf8))
    {
        FMemory::Memcpy(AmOutputUtf8, Output.Get(), Output.Length() + 1);
        FMemory::Memcpy(AmStateUtf8, State.Get(), State.Length() + 1);
    }
    else UE_LOG(LogDashEvents, Warning, TEXT("Android launch evidence exceeds the bounded row storage"));
    signal_core::Clock Clock{};
    Clock.context = this;
    Clock.now = [](void* Context) {
        return signal_core::Time(static_cast<FDashEventLogImpl*>(Context)->Now());
    };
    const signal_core::Status Opened = Log.Open(TCHAR_TO_UTF8(*Options.Path), Clock,
        std::chrono::nanoseconds(static_cast<int64>(Options.FlushSeconds * 1e9)));
    if (!Opened.Ok()) return FString(Opened.message);

    // The schema and the header are written first and flushed before anything else. EventLog
    // cannot do this itself: it does not know which record types a run can write.
    {
        char Storage[16384];
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
            .Key("expiry_armed").BeginArray()
                .String("expiry").String("kind").String("signal").String("sample")
                .String("generation").String("t_armed").String("becomes")
            .EndArray()
            .Key("expiry_status").BeginArray()
                .String("expiry").String("kind").String("status").String("t_fired")
                .String("reason").String("by")
            .EndArray()
            .Key("asset_import").BeginArray()
                .String("asset").String("bytes").String("width").String("height")
                .String("t_decode_start").String("t_decode_end").String("t_upload_start").String("t_first_usable")
            .EndArray()
            .Key("first_usable").BeginArray().String("frame").String("t_present").EndArray()
            .Key("launch").BeginArray()
                .String("launch_counter").String("first_usable_counter").String("counter_frequency")
                .String("process_start_ticks").String("process_ticks_per_second")
                .String("first_usable_boot_ns").String("am_start_output").String("am_launch_state")
            .EndArray()
            .EndObject()
            .End();
        if (!Log.Write(Row).Ok()) return TEXT("the event log schema record did not write");
    }
    {
        char Storage[16384];
        signal_core::RowBuilder Row(std::span<char>(Storage, sizeof(Storage)));
        Row.Begin("header")
            .Key("build_id").String(TCHAR_TO_UTF8(*Options.BuildId))
            .Key("platform").String(TCHAR_TO_UTF8(ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName())))
            .Key("rhi").String(TCHAR_TO_UTF8(*Options.Rhi))
            .Key("width").Integer(Options.Resolution.X)
            .Key("height").Integer(Options.Resolution.Y)
            .Key("clock").String("monotonic_nanoseconds")
            .Key("target_fps").Number(Options.TargetFrameRate)
            .Key("present_source").String("first end_of_render_thread_frame after the frame's game tick, paired in order")
            .Key("missed_frame_rule").String("frame_ns > 1.5 * (1e9 / target_fps)")
            .Key("frame_memory_source").String("most_recent_sampled_row, not a per-frame syscall")
            .Key("temperature_source").String(TCHAR_TO_UTF8(*TemperatureSource))
#if PLATFORM_ANDROID
            .Key("handles_source").String("/proc/self/fd entry count, excluding dot entries, including the enumeration descriptor; null if unreadable")
#endif
            // PLAN 4.8 asks the present row to name the sample an interpolated value came from.
            // Established rather than assumed: see the chunk 18 report.
            .Key("interpolation").String("none: the Stage 0 binding path renders samples directly, so derived_from is always null")
            .Key("declared_rules").BeginArray();
        for (const FDashDeclaredRule& Rule : Options.DeclaredRules)
            Row.BeginObject().Key("id").Unsigned(Rule.Id).Key("name").String(TCHAR_TO_UTF8(*Rule.Name)).EndObject();
        Row.EndArray()
            .Key("rule_expiries").String(Options.DeclaredRules.IsEmpty() ?
                "Stage 0 loads no threshold rules, so it arms no hold_last or debounce entries" : "rule ids map to document names in declared_rules")
            .Key("expiry_kinds").BeginObject().Key("0").String("freshness")
                .Key("1").String("hold_last").Key("2").String("debounce").EndObject()
            .Key("launch_clock").String("Windows: system-wide QueryPerformanceCounter; Android: process start ticks on CLOCK_BOOTTIME")
            .Key("android_launch").BeginObject().Key("process_start_ticks");
        if (ProcessStartTicks) Row.Unsigned(ProcessStartTicks); else Row.Null();
        Row.Key("process_ticks_per_second");
        if (ProcessTicksPerSecond) Row.Unsigned(ProcessTicksPerSecond); else Row.Null();
        Row.Key("am_start_output");
        if (!AmStartOutput.IsEmpty()) Row.String(TCHAR_TO_UTF8(*AmStartOutput)); else Row.Null();
        Row.Key("am_launch_state");
        if (!AmLaunchState.IsEmpty()) Row.String(TCHAR_TO_UTF8(*AmLaunchState)); else Row.Null();
        Row.Key("auxiliary_source").String(AmStartOutput.IsEmpty() ?
            "not measured: no am start -W evidence supplied by the device harness" : "device harness evidence file")
            .EndObject().Key("uncertainty").BeginObject()
                .Key("windows").String("launch request: counter read to Start-Process return, including launcher overhead; null means evidence unavailable")
                .Key("windows_launch_overhead_counter");
        if (Options.LaunchCounter && LaunchReturnCounter >= Options.LaunchCounter)
            Row.Unsigned(LaunchReturnCounter - Options.LaunchCounter);
        else Row.Null();
        Row.Key("android").String("process start: /proc/self/stat field 22, boot-time clock; one scheduler tick granularity; observed spread against am start -W is auxiliary and unverified without device evidence; boot offset uses a bracketed clock read and assumes no suspend before first usable")
            .Key("android_tick_ns");
        if (ProcessTicksPerSecond) Row.Number(1e9 / ProcessTicksPerSecond); else Row.Null();
        Row.Key("android_clock_pair_span_ns");
        if (bBootClockAvailable) Row.Integer(BootClockPairSpanNanoseconds); else Row.Null();
        Row.Key("android_am_spread_ns");
        if (AndroidSpreadNanoseconds >= 0) Row.Number(AndroidSpreadNanoseconds); else Row.Null();
        Row.EndObject().End();
        if (!Log.Write(Row).Ok()) return TEXT("the event log header record did not write");
    }
    const auto Flushed = Log.Flush();
    if (!Flushed.Ok()) return FString(Flushed.message);
    PublishCounts();
    return FString();
}

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

    FDashEventLogImpl* Raw = Candidate.Get();
    Raw->Wake = FPlatformProcess::GetSynchEventFromPool(false);
    Raw->Initialized = FPlatformProcess::GetSynchEventFromPool(false);
    Raw->bRunning.store(true, std::memory_order_release);
    Raw->Thread = FRunnableThread::Create(Raw, TEXT("DashEventLog"), 0, TPri_BelowNormal);
    if (Raw->Thread) Raw->Initialized->Wait();
    const FString Failure = Raw->Thread ? Raw->InitFailure : TEXT("the event log sampler thread did not start");
    FPlatformProcess::ReturnSynchEventToPool(Raw->Initialized);
    Raw->Initialized = nullptr;
    if (!Failure.IsEmpty())
    {
        if (Raw->Thread) { Raw->Thread->WaitForCompletion(); delete Raw->Thread; }
        FPlatformProcess::ReturnSynchEventToPool(Raw->Wake);
        return Failure;
    }
    Raw->PresentHandle = FCoreDelegates::OnEndFrameRT.AddLambda([Raw]()
    {
        FPresentStamp Stamp;
        Stamp.Frame = GFrameCounterRenderThread;
        Stamp.Counter = FPlatformTime::Cycles64();
        Stamp.Nanoseconds = Raw->Now();
        if (!Raw->Presents.Push(Stamp)) Raw->Dropped.fetch_add(1, std::memory_order_relaxed);
    });

    Impl = MoveTemp(Candidate);
    return FString();
}

void FDashEventLogImpl::JoinPresents()
{
    // Join whatever the render thread has finished since the last tick.
    //
    // Paired in order, not by frame index. GFrameCounterRenderThread has already advanced by the
    // time OnEndFrameRT fires, so a stamp carries the number of the frame the render thread is
    // moving on to rather than the one it just finished. Matching on it paired every frame row
    // with the wrong frame's rendered set, which went unnoticed until -udash-hold-frame-ms made
    // the two numberings stop overlapping and every present row disappeared.
    //
    // What a present timestamp means here is therefore: the first end-of-render-thread-frame
    // after this frame's game tick. The header says so. The stamp's own frame number is kept
    // only as evidence, never as the key.
    FPresentStamp Stamp;
    while (Presents.Pop(Stamp))
    {
        // The oldest frame still waiting, and only if the stamp is not older than it. A stamp
        // that predates the frame cannot be its completion.
        if (PendingHead == PendingTail) continue;
        const FDashEventLogImpl::FPendingFrame& Done = Pending[PendingTail % FDashEventLogImpl::MaxPending];
        if (Stamp.Nanoseconds < Done.StartNanoseconds) continue;
        ++PendingTail;

        char Storage[kRowBytes];
        signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
        Row.Begin("frame")
            .Key("frame").Unsigned(Done.Frame)
            .Key("t_start").Integer(Done.StartNanoseconds)
            .Key("frame_ns").Integer(Done.FrameNanoseconds)
            .Key("t_present").Integer(Stamp.Nanoseconds)
            .Key("missed").Boolean(Done.FrameNanoseconds > MissedThresholdNanoseconds)
            .Key("resident_bytes").Unsigned(ResidentBytes.load(std::memory_order_relaxed))
            .Key("texture_bytes").Unsigned(TextureBytes.load(std::memory_order_relaxed))
            .End();
        if (Row.Ok() && Rows.Push(Row.View())) Frames.fetch_add(1, std::memory_order_relaxed);
        else Dropped.fetch_add(1, std::memory_order_relaxed);

        // The present row, from the same join and the same timestamp. A second capture would let
        // the two rows disagree about when this frame reached the screen, which is chunk 17
        // revision 2 A.
        if (Done.bRendered)
        {
            char Wide[kWideRowBytes];
            signal_core::RowBuilder Present(std::span<char>(Wide, kWideRowBytes));
            Present.Begin("present")
                .Key("frame").Unsigned(Done.Frame)
                .Key("t_present").Integer(Stamp.Nanoseconds)
                .Key("signals").BeginArray();
            for (const FDashRenderedSignal& Entry : MakeArrayView(Done.Rendered, Done.RenderedCount))
            {
                Present.BeginObject().Key("signal").Unsigned(Entry.Signal);
                // Null rather than omitted: a bound signal with no sample renders the
                // missing-data presentation, which is a real thing on screen.
                Present.Key("sample");
                if (Entry.SampleId != 0) Present.Unsigned(Entry.SampleId); else Present.Null();
                Present.Key("quality").Unsigned(Entry.Quality)
                    .Key("expiry");
                if (Entry.Expiry != 0) Present.Unsigned(Entry.Expiry); else Present.Null();
                Present.Key("derived_from").Null()
                    .EndObject();
            }
            Present.EndArray().End();
            if (Present.Ok() && WideRows.Push(Present.View()))
            {
                PresentRows.fetch_add(1, std::memory_order_relaxed);
                bool Valid = false;
                for (int32 Index = 0; Index < Done.RenderedCount; ++Index)
                    Valid |= Done.Rendered[Index].Quality == 0;
                if (Valid && !bFirstUsable)
                {
                    bFirstUsable = true;
                    char FirstStorage[kRowBytes];
                    signal_core::RowBuilder First(std::span<char>(FirstStorage, sizeof(FirstStorage)));
                    First.Begin("first_usable").Key("frame").Unsigned(Done.Frame)
                        .Key("t_present").Integer(Stamp.Nanoseconds).End();
                    if (!First.Ok() || !Rows.Push(First.View())) Dropped.fetch_add(1, std::memory_order_relaxed);
                    char LaunchStorage[kWideRowBytes];
                    signal_core::RowBuilder Launch(std::span<char>(LaunchStorage, sizeof(LaunchStorage)));
                    Launch.Begin("launch").Key("launch_counter");
                    if (Options.LaunchCounter) Launch.Unsigned(Options.LaunchCounter); else Launch.Null();
                    Launch.Key("first_usable_counter").Unsigned(Stamp.Counter)
                        .Key("counter_frequency").Unsigned(CounterFrequency)
                        .Key("process_start_ticks");
                    if (ProcessStartTicks) Launch.Unsigned(ProcessStartTicks); else Launch.Null();
                    Launch.Key("process_ticks_per_second");
                    if (ProcessTicksPerSecond) Launch.Unsigned(ProcessTicksPerSecond); else Launch.Null();
                    Launch.Key("first_usable_boot_ns");
#if PLATFORM_ANDROID
                    if (bBootClockAvailable) Launch.Integer(Stamp.Nanoseconds + BootOffsetNanoseconds); else Launch.Null();
#else
                    Launch.Null();
#endif
                    Launch.Key("am_start_output");
                    // UTF-8 conversion is prepared by the sampler, never on the frame path.
                    if (AmOutputUtf8[0]) Launch.String(AmOutputUtf8); else Launch.Null();
                    Launch.Key("am_launch_state");
                    if (AmStateUtf8[0]) Launch.String(AmStateUtf8); else Launch.Null();
                    Launch.End();
                    if (!Launch.Ok() || !WideRows.Push(Launch.View())) Dropped.fetch_add(1, std::memory_order_relaxed);
                }
            }
            else Dropped.fetch_add(1, std::memory_order_relaxed);
        }
    }

}

void FDashEventLog::Tick(float DeltaSeconds)
{
    if (!Impl.IsValid()) return;
    const int64 Now = Impl->Now();
    const int64 FrameNanoseconds = static_cast<int64>(static_cast<double>(DeltaSeconds) * 1e9);

    Impl->JoinPresents();

    // Added AFTER the join, not before. This frame's rendered set is stashed later in the same
    // tick, so joining it now would pair it with a stamp while it still had nothing to present.
    if (Impl->PendingHead - Impl->PendingTail == FDashEventLogImpl::MaxPending)
    {
        ++Impl->PendingTail;
        Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
    }
    auto& Frame = Impl->Pending[Impl->PendingHead++ % FDashEventLogImpl::MaxPending];
    Frame.Frame = GFrameCounter;
    Frame.StartNanoseconds = Now - FrameNanoseconds;
    Frame.FrameNanoseconds = FrameNanoseconds;
    Frame.bRendered = false;
    Frame.RenderedCount = 0;
}

void FDashEventLog::Close()
{
    if (!Impl.IsValid()) return;
    if (Impl->PresentHandle.IsValid()) FCoreDelegates::OnEndFrameRT.Remove(Impl->PresentHandle);
    // Queued asset completions borrow Impl. Drain the render commands before destroying it.
    FlushRenderingCommands();
    Impl->JoinPresents();
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
    if (Row.Ok() && Impl->ReceiveRows.Push(Row.View())) Impl->Receives.fetch_add(1, std::memory_order_relaxed);
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
    if (Row.Ok() && Impl->WideRows.Push(Row.View())) Impl->Acquires.fetch_add(1, std::memory_order_relaxed);
    else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
}

void FDashEventLog::WriteSubmit(uint64 Frame, int64 Nanoseconds)
{
    if (!Impl.IsValid()) return;
    char Storage[kRowBytes];
    signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
    Row.Begin("submit").Key("frame").Unsigned(Frame).Key("t_submit").Integer(Nanoseconds).End();
    if (Row.Ok() && Impl->Rows.Push(Row.View())) Impl->Submits.fetch_add(1, std::memory_order_relaxed);
    else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
}

void FDashEventLog::WriteRendered(uint64 Frame, TArrayView<const FDashRenderedSignal> Rendered)
{
    if (!Impl.IsValid()) return;
    // Stashed, not written. The present row needs this frame's present timestamp, which the
    // render thread has not produced yet.
    FDashEventLogImpl::FPendingFrame* Slot = Impl->Find(Frame);
    if (!Slot)
    {
        // The frame row's own data arrives from Tick, which runs first every frame. A missing
        // entry here means the join already consumed it, so the present row has nowhere to go
        // and is counted rather than lost quietly.
        Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    Slot->bRendered = true;
    if (Rendered.Num() > UE_ARRAY_COUNT(Slot->Rendered))
    {
        Slot->bRendered = false;
        Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    Slot->RenderedCount = Rendered.Num();
    for (int32 Index = 0; Index < Rendered.Num(); ++Index) Slot->Rendered[Index] = Rendered[Index];
}

void FDashEventLog::WriteExpiryArmed(uint8 Kind, uint32 Id, uint64 Serial, uint64 Sample, uint64 Generation,
    int64 DeadlineNanoseconds, uint8 Becomes)
{
    if (!Impl.IsValid()) return;
    char Storage[kRowBytes];
    signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
    Row.Begin("expiry_armed")
        .Key("expiry").Unsigned(Serial)
        .Key("kind").Unsigned(Kind)
        .Key("signal").Unsigned(Id)
        .Key("sample").Unsigned(Sample)
        .Key("generation").Unsigned(Generation)
        .Key("t_armed").Integer(DeadlineNanoseconds)
        .Key("becomes").Unsigned(Becomes)
        .End();
    if (Row.Ok() && Impl->ReceiveRows.Push(Row.View())) Impl->Armed.fetch_add(1, std::memory_order_relaxed);
    else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
}

void FDashEventLog::WriteExpiryFired(uint8 Kind, uint64 Serial, int64 Nanoseconds)
{
    if (!Impl.IsValid()) return;
    char Storage[kRowBytes];
    signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
    Row.Begin("expiry_status")
        .Key("expiry").Unsigned(Serial)
        .Key("kind").Unsigned(Kind)
        .Key("status").String("fired")
        .Key("t_fired").Integer(Nanoseconds)
        .Key("reason").Null()
        .Key("by").Null()
        .End();
    if (Row.Ok() && Impl->ReceiveRows.Push(Row.View())) Impl->Statuses.fetch_add(1, std::memory_order_relaxed);
    else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
}

void FDashEventLog::WriteExpiryCancelled(uint8 Kind, uint64 Serial, uint8 Reason, uint64 By)
{
    if (!Impl.IsValid()) return;
    char Storage[kRowBytes];
    signal_core::RowBuilder Row(std::span<char>(Storage, kRowBytes));
    Row.Begin("expiry_status")
        .Key("expiry").Unsigned(Serial)
        .Key("kind").Unsigned(Kind)
        .Key("status").String("cancelled")
        // t_fired is null for a cancellation: nothing fired, and a timestamp here would be a
        // start endpoint for an observation that must never exist.
        .Key("t_fired").Null()
        .Key("reason").Unsigned(Reason)
        .Key("by").Unsigned(By)
        .End();
    if (Row.Ok() && Impl->ReceiveRows.Push(Row.View())) Impl->Statuses.fetch_add(1, std::memory_order_relaxed);
    else Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
}

void FDashEventLog::SampledMemory(uint64& Resident, uint64& Texture) const
{
    Resident = Impl ? Impl->ResidentBytes.load(std::memory_order_relaxed) : 0;
    Texture = Impl ? Impl->TextureBytes.load(std::memory_order_relaxed) : 0;
}

void FDashEventLog::QueueAssetImport(const FString& Asset, uint64 Bytes, FIntPoint Pixels,
    int64 DecodeStart, int64 DecodeEnd, int64 UploadStart)
{
    if (!Impl) return;
    const FTCHARToUTF8 Name(*Asset);
    struct FImport { char Name[512]{}; } Import;
    if (Name.Length() >= sizeof(Import.Name))
    {
        Impl->Dropped.fetch_add(1, std::memory_order_relaxed);
        UE_LOG(LogDashEvents, Warning, TEXT("Asset import identifier exceeds row storage: %s"), *Asset);
        return;
    }
    FMemory::Memcpy(Import.Name, Name.Get(), Name.Length() + 1);
    FDashEventLogImpl* Raw = Impl.Get();
    ENQUEUE_RENDER_COMMAND(DashAssetUsable)([Raw, Import, Bytes, Pixels, DecodeStart, DecodeEnd, UploadStart](FRHICommandListImmediate&)
    {
        const int64 Usable = Raw->Now();
        char Storage[kWideRowBytes];
        signal_core::RowBuilder Row(std::span<char>(Storage, sizeof(Storage)));
        Row.Begin("asset_import").Key("asset").String(Import.Name)
            .Key("bytes").Unsigned(Bytes).Key("width").Integer(Pixels.X).Key("height").Integer(Pixels.Y)
            .Key("t_decode_start").Integer(DecodeStart).Key("t_decode_end").Integer(DecodeEnd)
            .Key("t_upload_start").Integer(UploadStart).Key("t_first_usable").Integer(Usable).End();
        if (!Row.Ok() || !Raw->AssetRows.Push(Row.View())) Raw->Dropped.fetch_add(1, std::memory_order_relaxed);
    });
}

FDashEventLogCounts FDashEventLog::Counts() const
{
    FDashEventLogCounts Counts;
    if (!Impl.IsValid()) return Counts;
    Counts.Records = Impl->WrittenRecords.load(std::memory_order_relaxed);
    Counts.Bytes = Impl->WrittenBytes.load(std::memory_order_relaxed);
    Counts.Dropped = Impl->WriteDrops.load(std::memory_order_relaxed) + Impl->Dropped.load(std::memory_order_relaxed);
    Counts.Frames = Impl->Frames.load(std::memory_order_relaxed);
    Counts.Samples = Impl->Samples.load(std::memory_order_relaxed);
    Counts.Receives = Impl->Receives.load(std::memory_order_relaxed);
    Counts.Acquires = Impl->Acquires.load(std::memory_order_relaxed);
    Counts.Submits = Impl->Submits.load(std::memory_order_relaxed);
    Counts.Presents = Impl->PresentRows.load(std::memory_order_relaxed);
    Counts.ExpiriesArmed = Impl->Armed.load(std::memory_order_relaxed);
    Counts.ExpiryStatuses = Impl->Statuses.load(std::memory_order_relaxed);
    return Counts;
}

} // namespace UnRealDashCore
