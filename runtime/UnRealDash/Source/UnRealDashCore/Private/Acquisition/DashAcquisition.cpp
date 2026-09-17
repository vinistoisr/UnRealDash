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
void FDashAcquisition::FImpl::PublishHealth() {
    static_assert(std::is_trivially_copyable_v<signal_core::ConnectionHealth>);
    const auto& Health = Pipeline->Health();
    std::memcpy(HealthExchange.WriterBuffer().latched.data(), &Health, sizeof(Health));
    HealthExchange.Publish();
}
uint32 FDashAcquisition::FImpl::Run() {
    bCaptureInstant = true; LiveClock.Now();
    auto Status = Pipeline->Start();
    PublishHealth();
    bStarted.store(Status.Ok(), std::memory_order_release);
    while (Status.Ok() && !bStop.load(std::memory_order_acquire)) {
        const auto Deadline = signal_core::Time(PlatformClock.NowNanoseconds(PlatformClock.Context)) + std::chrono::milliseconds(1);
        bCaptureInstant = true;
        Status = Pipeline->Pump(Deadline).status;
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
    Out.Present.SetNum(static_cast<int32>(Snapshot.samples.size()));
    int32 Present = 0;
    for (std::size_t Index = 0; Index < Snapshot.samples.size(); ++Index) {
        Out.Samples[static_cast<int32>(Index)] = ToEngineSample(Snapshot.samples[Index].sample);
        if (Snapshot.samples[Index].received) Out.Present[Present++] = Snapshot.samples[Index].signal;
    }
    Out.Present.SetNum(Present, EAllowShrinking::No);
    Out.FrameIndex = Impl->Frame++;
    Out.Generation = Snapshot.generation;
    Out.AcquisitionNanoseconds = Impl->PlatformClock.NowNanoseconds(Impl->PlatformClock.Context);
    Impl->Sink.OnAcquire(Out.FrameIndex, signal_core::Time(Out.AcquisitionNanoseconds),
        {Out.Present.GetData(), static_cast<std::size_t>(Present)});
    signal_core::SignalSample Ignored{};
    while (Impl->Display->Pop(Ignored)) {}
    return true;
}
void FDashAcquisition::RecordSubmit(uint64 FrameIndex) {
    Impl->Sink.OnSubmit(FrameIndex, signal_core::Time(Impl->PlatformClock.NowNanoseconds(Impl->PlatformClock.Context)));
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
            Health.bytes, FString(UTF8_TO_TCHAR(Health.last_error))};
}
}
