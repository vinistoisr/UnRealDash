#pragma once
#include "SignalCore/ExpirySchedule.h"
#include "SignalCore/Export.h"
#include "SignalCore/SnapshotExchange.h"
namespace signal_core {
// Acquisition thread owns all methods except SnapshotExchange::Acquire on the reader.
class SIGNALCORE_API SignalRegistry {
  public:
    SignalRegistry(Clock clock, std::span<const Signal> signals, ExpirySchedule &schedule, SnapshotExchange &exchange);
    Status Initialize();
    Status Apply(SignalId id, const Sample &sample);
    void Expire();
    Status SetGeneration(std::uint64_t generation);
    Status Publish(std::span<const std::uint8_t> latched = {});
    std::span<const SignalSample> Samples() const { return storage_.first(signals_.size()); }
    std::span<const Signal> Signals() const { return signals_; }
    std::uint64_t Generation() const { return generation_; }
    std::uint64_t RejectedOrdering() const { return rejected_ordering_; }
    std::uint64_t RejectedGenerations() const { return rejected_; }

  private:
    Clock clock_;
    std::span<const Signal> signals_;
    std::span<SignalSample> storage_;
    ExpirySchedule &schedule_;
    SnapshotExchange &exchange_;
    std::uint64_t generation_{};
    std::uint64_t rejected_{};
    std::uint64_t rejected_ordering_{};
};
} // namespace signal_core
