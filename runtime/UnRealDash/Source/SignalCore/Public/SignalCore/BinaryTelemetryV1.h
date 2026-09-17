#pragma once
#include "SignalCore/DefinitionPack.h"
#include "SignalCore/Sample.h"
#include <array>
namespace signal_core {
struct FrameEvent {
    std::uint32_t frame_id{};
    std::uint64_t stream_offset{};
    const std::byte *payload{};
    std::uint8_t payload_length{};
};
struct FramerCounters {
    std::uint64_t frames_emitted{};
    std::uint64_t resyncs{};
    // All skipped bytes, including unknown-header scanning and termination/reset discards.
    std::uint64_t bytes_discarded{};
    std::uint64_t unconfirmed_candidates_discarded{};
    // Once per scanned tag with an unknown identifier, classified from eight bytes.
    // RESYNC advances one byte at that position; bytes_discarded includes that byte.
    std::uint64_t unknown_identifier_dropped{};
    // RESYNC to SYNCED transitions. Per stream, false_locks = max(locks - 1 - reconnects, 0).
    // This recovery metric is distinct from emitted frames at incorrect generated boundaries.
    std::uint64_t locks{};
};
enum class FramerState : std::uint8_t { synced, resync };
using FrameSink = void (*)(void *, const FrameEvent &);
class SIGNALCORE_API BinaryTelemetryV1Framer {
  public:
    explicit BinaryTelemetryV1Framer(std::span<const std::uint32_t> enumerated_identifiers);
    void Feed(std::span<const std::byte> bytes, FrameSink sink, void *context);
    void EndOfStream();
    // Applies EndOfStream accounting, retains cumulative counters, and resets the stream offset.
    void Reset();
    FramerState State() const { return state_; }
    const FramerCounters &Counters() const { return counters_; }

  private:
    bool Candidate(const std::byte *bytes) const;
    void Consume(std::size_t count, bool discarded);
    std::span<const std::uint32_t> identifiers_;
    std::array<std::byte, 64> carry_{};
    std::size_t used_{};
    std::uint64_t offset_{};
    FramerState state_{FramerState::resync};
    FramerCounters counters_{};
};
struct DecoderCounters {
    std::uint64_t samples_published{};
    std::uint64_t unknown_identifier_dropped{};
    std::uint64_t status_records{};
    std::uint64_t sentinel_rejections{};
    std::uint64_t non_finite_results{};
};
struct SIGNALCORE_API TelemetryHealth {
    Time last_bytes{};
    Time last_telemetry_record{};
    Time last_status_record{};
    bool TransportConnected(Time now, Time transport_deadline) const;
    bool AcquisitionReceiving(Time now, Time acquisition_deadline) const;
};
using SampleSink = void (*)(void *, const Sample &);
class SIGNALCORE_API BinaryTelemetryV1Decoder {
  public:
    BinaryTelemetryV1Decoder(const DefinitionPack &pack, const Clock &clock);
    void SetConnectionGeneration(std::uint32_t generation);
    void OnFrame(const FrameEvent &frame, SampleSink sink, void *context);
    const DecoderCounters &Counters() const { return counters_; }
    const Status &GetStatus() const { return validation_; }
    std::uint32_t ConnectionGeneration() const { return generation_; }
    // The transport calls this on every nonempty read, including undecodable bytes.
    void OnBytesReceived();
    const TelemetryHealth &Health() const { return health_; }

  private:
    DefinitionPack pack_;
    Clock clock_;
    Status validation_;
    DecoderCounters counters_{};
    TelemetryHealth health_{};
    std::uint32_t generation_{};
    std::uint64_t sequence_{};
};
// Connection glue: discard prior carry/lock state and advance the sample generation together.
SIGNALCORE_API Status ReconnectBinaryTelemetryV1(BinaryTelemetryV1Framer &framer, BinaryTelemetryV1Decoder &decoder);
} // namespace signal_core
