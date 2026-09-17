#pragma once
#include "UnRealDashCore/DashAcquisition.h"
#include "UnRealDashCore/MonotonicClock.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "SignalCore/AcquisitionEvents.h"
#include "SignalCore/MemoryConnector.h"
#include <atomic>
#include <vector>

namespace UnRealDashCore {
struct FDashAcquisition::FImpl final : FRunnable {
    FMonotonicClock PlatformClock = MakePlatformClock();
    signal_core::Time Instant{};
    bool bCaptureInstant = true;
    signal_core::Clock LiveClock{this, [](void* Context) {
        auto& Self = *static_cast<FImpl*>(Context);
        const auto Now = signal_core::Time(Self.PlatformClock.NowNanoseconds(Self.PlatformClock.Context));
        if (Self.bCaptureInstant) { Self.Instant = Now; Self.bCaptureInstant = false; }
        return Now;
    }};
    signal_core::Clock FrozenClock{this, [](void* Context) { return static_cast<FImpl*>(Context)->Instant; }};
    std::vector<signal_core::Signal> Signals;
    std::vector<signal_core::SignalSample> Recorded, Buffers[3], ReplayBuffers[3], Comparison, DisplayStorage;
    std::vector<signal_core::Expiry> Expiries, ReplayExpiries;
    std::vector<signal_core::RuleEngine> Rules;
    std::vector<signal_core::RuleResult> Previous;
    std::vector<uint8> SnapshotLatches[3];
    TUniquePtr<signal_core::ExpirySchedule> Schedule, ReplaySchedule;
    TUniquePtr<signal_core::SnapshotExchange> Exchange, ReplayExchange;
    TUniquePtr<signal_core::SignalRegistry> Registry, ReplayRegistry;
    TUniquePtr<signal_core::Replay> Replay;
    TUniquePtr<signal_core::ReplayTransport> Transport;
    signal_core::FieldSession Session;
    signal_core::FieldDecoder Decoder;
    signal_core::SiMapping Mapping;
    signal_core::NullAcquisitionEventSink Sink;
    TUniquePtr<signal_core::SampleQueue> Display;
    std::array<uint8, 4096> ReadBuffer{};
    TUniquePtr<signal_core::AcquisitionPipeline> Pipeline;
    std::array<std::array<uint8, sizeof(signal_core::ConnectionHealth)>, 3> HealthStorage{};
    mutable signal_core::SnapshotExchange HealthExchange{{{}, HealthStorage[0], 0},
        {{}, HealthStorage[1], 0}, {{}, HealthStorage[2], 0}};
    std::atomic<bool> bStop{false}, bStarted{false};
    TUniquePtr<FRunnableThread> Thread;
    FString Error;
    uint64 Frame = 0;
    FImpl(const FString& Text, uint32 ExpectedSamplesPerFrame, const TArray<FAcquisitionThresholdRule>& Definitions);
    uint32 Run() override;
    void PublishHealth();
    void Stop() override { bStop.store(true, std::memory_order_release); }
};
}
