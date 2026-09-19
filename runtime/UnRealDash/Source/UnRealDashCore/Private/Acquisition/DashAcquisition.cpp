#include "Acquisition/DashAcquisition.h"
#include "SignalCoreAdapter.h"
#include "HAL/PlatformProcess.h"
#include <algorithm>
#include <string>
#include <cstring>
#include <type_traits>

namespace UnRealDashCore {
FDashAcquisition::FImpl::FImpl(const FString& Text, uint32 ExpectedSamplesPerFrame,
                             const TArray<FAcquisitionThresholdRule>& Definitions) {
    const std::string Utf8(TCHAR_TO_UTF8(*Text));
    // A line bounds a sample and a byte bounds a signal declaration. Allocation is startup-only.
    Signals.resize(Utf8.size() / 16 + 1);
    Recorded.resize(static_cast<std::size_t>(std::count(Utf8.begin(), Utf8.end(), '\n')) + 1);
    const auto Recording = signal_core::ReadRecording(Utf8, Signals, Recorded);
    if (!Recording.Ok()) { Error = UTF8_TO_TCHAR(Recording.GetStatus().message); return; }
    const auto Count = Recording.Get()->signals.size();
    for (auto& Buffer : Buffers) Buffer.resize(Count);
    for (auto& Buffer : ReplayBuffers) Buffer.resize(Count);
    Comparison.resize(Count);
    Expiries.resize(Count + static_cast<std::size_t>(Definitions.Num()) * 2); ReplayExpiries.resize(Count);
    Previous.resize(Definitions.Num());
    for (auto& Buffer : SnapshotLatches) Buffer.resize(Definitions.Num());
    DisplayStorage.resize(std::max<std::size_t>(64, static_cast<std::size_t>(ExpectedSamplesPerFrame) * 4));
    Schedule = MakeUnique<signal_core::ExpirySchedule>(Expiries);
    ReplaySchedule = MakeUnique<signal_core::ExpirySchedule>(ReplayExpiries);
    Exchange = MakeUnique<signal_core::SnapshotExchange>(signal_core::SnapshotBuffer{Buffers[0], SnapshotLatches[0]},
        signal_core::SnapshotBuffer{Buffers[1], SnapshotLatches[1]}, signal_core::SnapshotBuffer{Buffers[2], SnapshotLatches[2]});
    ReplayExchange = MakeUnique<signal_core::SnapshotExchange>(signal_core::SnapshotBuffer{ReplayBuffers[0], {}},
        signal_core::SnapshotBuffer{ReplayBuffers[1], {}}, signal_core::SnapshotBuffer{ReplayBuffers[2], {}});
    // The recording names its own signals, so a binding resolves against its numbering rather
    // than the document's ordering. Same rule as the definition pack path; both are told.
    for (const auto& Signal : Recording.Get()->signals)
        NameToId.Add(UTF8_TO_TCHAR(Signal.name), Signal.id);
    Registry = MakeUnique<signal_core::SignalRegistry>(FrozenClock, Recording.Get()->signals, *Schedule, *Exchange);
    ReplayRegistry = MakeUnique<signal_core::SignalRegistry>(FrozenClock, Recording.Get()->signals, *ReplaySchedule, *ReplayExchange);
    auto Status = Registry->Initialize();
    if (Status.Ok()) Status = ReplayRegistry->Initialize();
    if (!Status.Ok()) { Error = UTF8_TO_TCHAR(Status.message); return; }
    Rules.reserve(Definitions.Num());
    for (const auto& Definition : Definitions) {
        if (Definition.Id >= 64) { Error = TEXT("Acquisition rule id exceeds 63"); return; }
        const auto Unit = static_cast<signal_core::Unit>(Definition.Unit);
        const std::array<signal_core::ExpressionNode, 3> Nodes{{
            {signal_core::Operation::greater, 0, signal_core::Unit::dimensionless, 0, 1, 2, "/threshold"},
            {signal_core::Operation::signal_reference, 0, signal_core::Unit::dimensionless, Definition.Signal, 0, 0, "/signal"},
            {signal_core::Operation::literal, Definition.Threshold, Unit, 0, 0, 0, "/value"}}};
        Rules.emplace_back(FrozenClock, *Schedule);
        Status = Rules.back().Load({Definition.Id, Nodes, Definition.Hysteresis, Unit,
            signal_core::Time(Definition.DebounceNanoseconds), signal_core::MissingInputPolicy::force_warn, {}}, Recording.Get()->signals);
        if (!Status.Ok()) { Error = UTF8_TO_TCHAR(Status.message); return; }
    }
    Replay = MakeUnique<signal_core::Replay>(FrozenClock, *ReplayRegistry);
    Transport = MakeUnique<signal_core::ReplayTransport>(*Replay, *ReplayRegistry, *Recording.Get(), Comparison);
    Display = MakeUnique<signal_core::SampleQueue>(DisplayStorage);
    Pipeline = MakeUnique<signal_core::AcquisitionPipeline>(LiveClock, *Registry, *Schedule,
        Rules, Previous, ReadBuffer, *Display, *Transport, Session, Decoder, Mapping, Sink);
}
FDashAcquisition::FImpl::FImpl(const FDashTcpOptions& Options) {
    const std::string PackJson(TCHAR_TO_UTF8(*Options.DefinitionPackText));
    if (PackJson.empty()) { Error = TEXT("The package ships no definition pack, which a binary telemetry connector needs"); return; }
    const std::string SchemaDirectory(TCHAR_TO_UTF8(*Options.SchemaDirectory));
    PackBuilder = MakeUnique<dashboard_spec::DefinitionPackBuilder>();
    const dashboard_spec::Validator Validator({SchemaDirectory.data(), SchemaDirectory.size()});
    const auto Built = PackBuilder->Build({PackJson.data(), PackJson.size()}, Validator);
    if (!Built.Ok()) { Error = UTF8_TO_TCHAR(Built.message.View().data()); return; }
    const auto& Pack = PackBuilder->Pack();

    // Signals come from the pack's fields, which is where their numeric ids come from too. A
    // document orders its signals however it likes; a telemetry stream numbers them, and the two
    // are joined by name rather than by position.
    for (const auto& PackFrame : Pack.frames) {
        Identifiers.push_back(PackFrame.frame_id);
        for (const auto& Field : PackFrame.fields) {
            const auto Existing = std::find_if(Signals.begin(), Signals.end(),
                [&](const auto& S) { return S.id == Field.signal; });
            if (Existing != Signals.end()) continue;
            signal_core::Signal Signal{};
            Signal.id = Field.signal;
            Signal.unit = Field.unit;
            // Half a second, matching the scenario source. A per-signal deadline belongs in the
            // signals document and reading it here would need a second join; PLAN 4.9's gate turns
            // on staleness happening at all, not on its exact instant.
            Signal.deadline = std::chrono::milliseconds(500);
            Signal.discrete = false;
            const auto Name = Field.name ? Field.name : "";
            std::snprintf(Signal.name, sizeof(Signal.name), "%s", Name);
            Signals.push_back(Signal);
            NameToId.Add(UTF8_TO_TCHAR(Name), Field.signal);
        }
    }
    if (Signals.empty()) { Error = TEXT("The definition pack declares no fields"); return; }

    const auto Count = Signals.size();
    for (auto& Buffer : Buffers) Buffer.resize(Count);
    for (auto& Buffer : SnapshotLatches) Buffer.resize(0);
    Expiries.resize(Count);
    DisplayStorage.resize(std::max<std::size_t>(64, Count * 4));
    Schedule = MakeUnique<signal_core::ExpirySchedule>(Expiries);
    Exchange = MakeUnique<signal_core::SnapshotExchange>(
        signal_core::SnapshotBuffer{Buffers[0], SnapshotLatches[0]},
        signal_core::SnapshotBuffer{Buffers[1], SnapshotLatches[1]},
        signal_core::SnapshotBuffer{Buffers[2], SnapshotLatches[2]});
    Registry = MakeUnique<signal_core::SignalRegistry>(FrozenClock, Signals, *Schedule, *Exchange);
    const auto Status = Registry->Initialize();
    if (!Status.Ok()) { Error = UTF8_TO_TCHAR(Status.message); return; }

    signal_core::ITransport* Chosen = nullptr;
    if (Options.ReplayPath.IsEmpty()) {
        Tcp = MakeUnique<signal_core::TcpTransport>();
        const std::string Host(TCHAR_TO_UTF8(*Options.Host));
        const auto Configured = Tcp->Configure(Host.c_str(), Options.Port);
        if (!Configured.Ok()) { Error = UTF8_TO_TCHAR(Configured.message); return; }
        Chosen = Tcp.Get();
    } else {
        // Same framer, same decoder, same mapping as the socket. Only the source of the bytes
        // differs, which is the point of putting a file behind ITransport rather than behind
        // Recording: a file replay that skipped the parser would test a different program.
        File = MakeUnique<signal_core::FileTransport>();
        const std::string Path(TCHAR_TO_UTF8(*Options.ReplayPath));
        const auto Configured = File->Configure(Path.c_str(), Options.bReplayLoop);
        if (!Configured.Ok()) { Error = UTF8_TO_TCHAR(Configured.message); return; }
        Chosen = File.Get();
    }
    Telemetry = MakeUnique<signal_core::BinaryTelemetryV1Connector>(Pack, LiveClock, Identifiers);
    Display = MakeUnique<signal_core::SampleQueue>(DisplayStorage);
    Pipeline = MakeUnique<signal_core::AcquisitionPipeline>(LiveClock, *Registry, *Schedule,
        Rules, Previous, ReadBuffer, *Display, *Chosen, Telemetry->Session(), Telemetry->Decoder(), Mapping, Sink);
}
void FDashAcquisition::FImpl::ServiceConnection() {
    if (!Tcp || !Telemetry) return;  // A file that has ended has ended; only a socket retries.
    const signal_core::Time Now(PlatformClock.NowNanoseconds(PlatformClock.Context));
    const bool bConnected = Pipeline->Health().connected;
    if (bConnected) return;

    // A connect already in flight is finished, not restarted, and it is polled every tick rather
    // than on the backoff schedule. This is the bug that made the first working connector never
    // reconnect: a non-blocking connect to loopback reports in progress, Reconnect closes the
    // socket before calling Connect, so every scheduled attempt tore down a connection that was
    // about to succeed and the link never came back. The schedule governs starting an attempt; it
    // has nothing to say about completing one.
    const bool bInFlight = Tcp->Connecting();
    if (!bInFlight && Supervisor.Poll(Now, bConnected) != signal_core::ConnectionSupervisor::Action::reconnect)
        return;

    // Start never disconnects, so it is what finishes a pending connect as well as what opens the
    // first one. Reconnect is only for getting a fresh socket after a real drop, and it is what
    // increments the reconnects counter PLAN 4.9's criterion 6 reads.
    const auto Status = (bHasConnected && !bInFlight) ? Pipeline->Reconnect() : Pipeline->Start();
    const bool bNowConnected = Pipeline->Health().connected;
    // A poll of a connect already in flight is not a new attempt, so it does not advance the
    // schedule. Counting it would race the backoff down to nothing.
    if (!bInFlight || bNowConnected) Supervisor.Attempted(Now, bNowConnected);
    if (bNowConnected) {
        if (bHasConnected) ++Reconnections;
        bHasConnected = true;
        // The generation is told to the decoder, never guessed by it. See the note on
        // BinaryTelemetryV1Connector for why a self-incrementing Reset cannot stay in step across
        // failed attempts, and failed attempts are the normal case for a non-blocking connect.
        const auto Generation = Telemetry->SetGeneration(Pipeline->Health().generation);
        if (!Generation.Ok()) Error = UTF8_TO_TCHAR(Generation.message);
    } else if (!Status.Ok() && Status.code != signal_core::ErrorCode::need_more_data) {
        // A refusal is expected while a relay is down and is not worth logging every five seconds.
    }
}
void FDashAcquisition::FImpl::PublishHealth() {
    static_assert(std::is_trivially_copyable_v<signal_core::ConnectionHealth>);
    auto Health = Pipeline->Health();
    if (Tcp) {
        signal_core::ReportBackoff(Supervisor, Health);
        Health.reconnects = Reconnections;
    }
    std::memcpy(HealthExchange.WriterBuffer().latched.data(), &Health, sizeof(Health));
    HealthExchange.Publish();
}
uint32 FDashAcquisition::FImpl::Run() {
    bCaptureInstant = true; LiveClock.Now();
    signal_core::Status Status{};
    if (!Tcp) {
        // The recording and file paths connect once, immediately, and a failure there is fatal.
        Status = Pipeline->Start();
        bStarted.store(Status.Ok(), std::memory_order_release);
    } else {
        // The telemetry path may start with nothing listening, which is not an error: the whole
        // point of the backoff is that an absent relay is a state to wait in rather than a failure
        // to report. The snapshot is published either way, so a reader sees unavailable signals.
        bStarted.store(true, std::memory_order_release);
    }
    PublishHealth();
    while (Status.Ok() && !bStop.load(std::memory_order_acquire)) {
        ServiceConnection();
        const auto Deadline = signal_core::Time(PlatformClock.NowNanoseconds(PlatformClock.Context)) + std::chrono::milliseconds(1);
        // Frozen means the instant stops advancing, so the registry never expires anything and the
        // replay finds no sample due: the scenario holds where it was. The render loop is
        // unaffected, because Pump's deadline comes from the platform clock directly.
        bCaptureInstant = !bFrozen.load(std::memory_order_acquire);
        Status = Pipeline->Pump(Deadline).status;
        // A dead socket surfaces as an io_error from Pump. On the telemetry path that is a
        // disconnect to be waited out, not a reason to stop acquiring: the supervisor will
        // reconnect and every mapped signal goes stale at its deadline in the meantime.
        if (Tcp && !Status.Ok() && Status.code == signal_core::ErrorCode::io_error) Status = {};
        // A replay that reaches its end stops the loop rather than spinning on a closed file, and
        // the snapshot already published leaves every signal to go stale at its own deadline.
        if (File && !Status.Ok() && Status.code == signal_core::ErrorCode::io_error) break;
        PublishHealth();
        FPlatformProcess::SleepNoStats(0);
    }
    Pipeline->Stop();
    PublishHealth();
    bStarted.store(false, std::memory_order_release);
    return Status.Ok() ? 0 : 1;
}
FDashAcquisition::FDashAcquisition(const FString& Text, uint32 ExpectedSamplesPerFrame,
                                 const TArray<FAcquisitionThresholdRule>& Rules)
    : Impl(MakeUnique<FImpl>(Text, ExpectedSamplesPerFrame, Rules)) {}
FDashAcquisition::FDashAcquisition(const FDashTcpOptions& Options) : Impl(MakeUnique<FImpl>(Options)) {}
const TMap<FString, uint32>& FDashAcquisition::SignalIds() const { return Impl->NameToId; }
void FDashAcquisition::FreezeScenarioTime() { Impl->bFrozen.store(true, std::memory_order_release); }
FDashAcquisition::~FDashAcquisition() {
    Stop();
    if (Impl->Thread) Impl->Thread->WaitForCompletion();
}
FString FDashAcquisition::Start() {
    if (!Impl->Error.IsEmpty()) return Impl->Error;
    if (Impl->Thread) return TEXT("Acquisition already started");
    Impl->Thread.Reset(FRunnableThread::Create(Impl.Get(), TEXT("DashAcquisition")));
    return Impl->Thread ? FString() : FString(TEXT("Acquisition thread creation failed"));
}
void FDashAcquisition::Stop() { Impl->Stop(); }
bool FDashAcquisition::AcquireFrameSnapshot(FFrameSnapshot& Out) {
    if (!Impl->bStarted.load(std::memory_order_acquire)) return false;
    const auto Snapshot = Impl->Exchange->Acquire();
    Out.Samples.SetNum(static_cast<int32>(Snapshot.samples.size()));
    Out.SignalIds.SetNum(static_cast<int32>(Snapshot.samples.size()));
    Out.Present.SetNum(static_cast<int32>(Snapshot.samples.size()));
    Out.SampleIds.SetNum(static_cast<int32>(Snapshot.samples.size()));
    int32 Present = 0;
    // The sample ids actually held this frame, which is what the acquisition row carries. A
    // signal with no sample yet has id zero and is not one of them.
    // std::uint64_t, not UE's uint64: on Android one is unsigned long and the other unsigned
    // long long, and this array is handed straight to a std::span the sink takes.
    TArray<std::uint64_t, TInlineAllocator<64>> Held;
    for (std::size_t Index = 0; Index < Snapshot.samples.size(); ++Index) {
        Out.Samples[static_cast<int32>(Index)] = ToEngineSample(Snapshot.samples[Index].sample);
        Out.SignalIds[static_cast<int32>(Index)] = Snapshot.samples[Index].signal;
        Out.SampleIds[static_cast<int32>(Index)] = Snapshot.samples[Index].sample.id;
        if (Snapshot.samples[Index].sample.id != 0) Held.Add(Snapshot.samples[Index].sample.id);
        if (Snapshot.samples[Index].received) Out.Present[Present++] = Snapshot.samples[Index].signal;
    }
    Out.Present.SetNum(Present, EAllowShrinking::No);
    Out.FrameIndex = Impl->Frame++;
    Out.Generation = Snapshot.generation;
    Out.AcquisitionNanoseconds = Impl->PlatformClock.NowNanoseconds(Impl->PlatformClock.Context);
    Impl->Sink.OnAcquire(Out.FrameIndex, signal_core::Time(Out.AcquisitionNanoseconds),
        {Held.GetData(), static_cast<std::size_t>(Held.Num())});
    signal_core::SignalSample Ignored{};
    while (Impl->Display->Pop(Ignored)) {}
    return true;
}
void FDashAcquisition::RecordSubmit(uint64 FrameIndex) {
    Impl->Sink.OnSubmit(FrameIndex, signal_core::Time(Impl->PlatformClock.NowNanoseconds(Impl->PlatformClock.Context)));
}
uint64 FDashAcquisition::SamplesApplied() const {
    return Impl->Pipeline ? Impl->Pipeline->SamplesApplied() : 0;
}
void FDashAcquisition::SetEventLog(FDashEventLog* Log) {
    Impl->Sink.Log.store(Log, std::memory_order_release);
}
void FDashAcquisition::RequestAcknowledge(uint32 RuleId) {
    if (Impl->Pipeline) {
        const auto Status = Impl->Pipeline->Acknowledge(RuleId);
        if (!Status.Ok()) UE_LOG(LogTemp, Warning, TEXT("%s"), UTF8_TO_TCHAR(Status.message));
    }
}
FConnectionHealth FDashAcquisition::GetHealth() const {
    signal_core::ConnectionHealth Health{};
    const auto Snapshot = Impl->HealthExchange.Acquire();
    std::memcpy(&Health, Snapshot.latched.data(), sizeof(Health));
    return {Health.generation, Health.connected, Health.last_byte_at.count(), Health.reconnects,
            Health.bytes, FString(UTF8_TO_TCHAR(Health.last_error)),
            Health.backoff_ms, Health.attempts_since_connect};
}
}
