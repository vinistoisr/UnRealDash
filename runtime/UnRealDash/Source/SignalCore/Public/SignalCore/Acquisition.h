#pragma once
#include "SignalCore/Connector.h"
#include "SignalCore/RuleEngine.h"
#include <atomic>

namespace signal_core {
struct IAcquisitionEventSink {
    virtual ~IAcquisitionEventSink() = default;
    // PLAN 4.8's receive row, one per published sample, at the moment Apply accepts it. The
    // sample carries its own id by then, so nothing downstream has to re-derive identity.
    virtual void OnReceive(SignalId signal, const Sample &sample) = 0;
    // Sample ids, not signal ids: 4.8's acquisition row lists "the sample ids present in that
    // snapshot", which is what lets a later pass tell an acquired sample from a superseded one.
    virtual void OnAcquire(std::uint64_t frame, Time at, std::span<const std::uint64_t> samples) = 0;
    virtual void OnSubmit(std::uint64_t frame, Time at) = 0;
    virtual void OnRuleTransition(std::uint32_t rule, Time at, bool current, bool latched) = 0;
};
struct NullAcquisitionEventSink final : IAcquisitionEventSink {
    void OnReceive(SignalId, const Sample &) override {}
    void OnAcquire(std::uint64_t, Time, std::span<const std::uint64_t>) override {}
    void OnSubmit(std::uint64_t, Time) override {}
    void OnRuleTransition(std::uint32_t, Time, bool, bool) override {}
};
struct PumpResult {
    std::uint32_t bytes_read{}, samples_applied{}, rule_evaluations{}, transitions{};
    std::uint32_t display_pushed{}, display_dropped{}, mapping_drops{}, expiries_fired{};
    Status status{};
};
// No thread. All calls except Acknowledge belong to the acquisition owner.
// Health and cumulative counters must be copied by that owner before cross-thread access.
// Registry/rules use a host-frozen clock to share the Pump entry instant.
// read_buffer must also hold one latch byte per rule; it is reused only after decoding.
class SIGNALCORE_API AcquisitionPipeline {
  public:
    AcquisitionPipeline(Clock clock, SignalRegistry &registry, ExpirySchedule &schedule, std::span<RuleEngine> rules,
                        std::span<RuleResult> previous_results, std::span<std::uint8_t> read_buffer,
                        SampleQueue &display, ITransport &transport, ISession &session, IDecoder &decoder,
                        IMapping &mapping, IAcquisitionEventSink &sink);
    Status Start();
    Status Stop();
    Status Reconnect();
    PumpResult Pump(Time deadline);
    const ConnectionHealth &Health() const { return health_; }
    // Thread safe. Only rule ids 0 through 63 can be acknowledged.
    Status Acknowledge(std::uint32_t rule_id);
    std::uint64_t PublishedCount() const { return published_; }
    std::uint64_t DisplayDrops() const { return display_drops_; }
    std::uint64_t MappingDrops() const { return mapping_drops_; }
    std::uint64_t SamplesApplied() const { return applied_; }
    std::uint64_t RuleEvaluations() const { return evaluations_; }
    std::uint64_t RuleTransitions() const { return transitions_; }

  private:
    Status Connect();
    void Evaluate(Time now, PumpResult &result);
    void Transition(std::size_t index, Time now, PumpResult &result);
    Status Remember(Status status);
    Clock clock_;
    SignalRegistry &registry_;
    ExpirySchedule &schedule_;
    std::span<RuleEngine> rules_;
    std::span<RuleResult> previous_;
    std::span<std::uint8_t> read_buffer_, latched_;
    SampleQueue &display_;
    ITransport &transport_;
    ISession &session_;
    IDecoder &decoder_;
    IMapping &mapping_;
    IAcquisitionEventSink &sink_;
    std::atomic<std::uint64_t> acknowledges_{};
    ConnectionHealth health_{};
    Status configuration_{};
    std::uint64_t published_{}, display_drops_{}, mapping_drops_{}, applied_{}, evaluations_{}, transitions_{};
    // Starts at 1 so that zero stays the "never went through here" value.
    std::uint64_t next_sample_id_{1};
};
} // namespace signal_core
