#pragma once
#include "SignalCore/BinaryTelemetryV1.h"
#include "SignalCore/Connector.h"
#include <array>

namespace signal_core {

// The adapter between the binary-telemetry-v1 parser and the acquisition pipeline.
//
// The parser is push based: the framer calls a FrameSink per frame and the decoder calls a
// SampleSink per field. The pipeline is pull based: ISession and IDecoder both offer input and are
// then drained through Next. This holds both halves and buffers between them.
//
// The connection generation is TOLD to this, not incremented by it.
//
// The obvious wiring is for Reset to call ReconnectBinaryTelemetryV1, which advances the decoder's
// generation by one. It does not survive contact with the pipeline. AcquisitionPipeline::Start and
// ::Reconnect each call session_.Reset() and then decoder_.Reset() BEFORE attempting the connect,
// while AcquisitionPipeline::Connect advances its own generation only on success. So two Resets per
// connect advance the decoder twice, and any failed attempt advances it again with the pipeline
// standing still. Either way the decoder ends up ahead, and the pipeline rejects the first decoded
// sample with "sample claims an unconnected generation".
//
// So the owner calls SetGeneration with AcquisitionPipeline::Health().generation after every
// successful connect, and the two cannot drift however many attempts failed on the way. Reset
// clears buffers and framer state and nothing else.
class SIGNALCORE_API BinaryTelemetryV1Connector {
  public:
    // A 4 KiB read of this format yields at most 256 frames: the framer consumes exactly 16 bytes
    // per frame in both the synced and the resync path. 320 is that bound with margin, so one Offer
    // can never overflow and the pipeline's requirement that Offer always make progress is met
    // without ever having to refuse input.
    static constexpr std::size_t frame_capacity = 320;
    // A frame's payload is 8 bytes and the narrowest field is 1 byte, so one frame cannot decode to
    // more than 8 samples. 32 is that bound with margin.
    static constexpr std::size_t sample_capacity = 32;
    // The format's fixed payload width. A frame claiming more is counted and dropped rather than
    // copied into a buffer sized for this one.
    static constexpr std::size_t payload_bytes = 8;

    BinaryTelemetryV1Connector(const DefinitionPack &pack, const Clock &clock,
                               std::span<const std::uint32_t> enumerated_identifiers);

    ISession &Session() { return session_; }
    IDecoder &Decoder() { return decoder_facade_; }

    // Call after every successful connect with the pipeline's own generation. Refuses a value past
    // what the wire format can carry rather than wrapping into a generation that looks valid.
    Status SetGeneration(std::uint64_t generation);

    const FramerCounters &Framing() const { return framer_.Counters(); }
    const DecoderCounters &Decoding() const { return decoder_.Counters(); }
    const TelemetryHealth &Health() const { return decoder_.Health(); }
    // Both must stay zero. They exist so the capacity arithmetic above is measured rather than
    // believed, which is the difference between a bound and an assumption.
    std::uint64_t FrameOverflows() const { return frame_overflows_; }
    std::uint64_t SampleOverflows() const { return sample_overflows_; }
    std::uint64_t OversizePayloads() const { return oversize_payloads_; }

  private:
    // One frame, copied out of the framer. The framer's FrameEvent::payload points into the
    // caller's buffer or into the framer's carry, and both are reused as soon as Feed returns, so
    // the payload has to be copied rather than referenced.
    struct Frame {
        std::uint32_t frame_id{};
        std::uint64_t stream_offset{};
        std::uint8_t payload_length{};
        std::array<std::byte, payload_bytes> payload{};
    };

    // ISession over the framer. Reset clears the frame buffer and the framer state.
    class SessionFacade final : public ISession {
      public:
        explicit SessionFacade(BinaryTelemetryV1Connector &owner) : owner_(owner) {}
        Status Offer(std::span<const std::uint8_t> input, std::size_t &consumed) override;
        Pending Next(Message &out) override;
        void Reset() override;

      private:
        BinaryTelemetryV1Connector &owner_;
    };

    // IDecoder over the decoder. Its Reset clears only its own buffer; see the class comment.
    class DecoderFacade final : public IDecoder {
      public:
        explicit DecoderFacade(BinaryTelemetryV1Connector &owner) : owner_(owner) {}
        Status Offer(const Message &message) override;
        Pending Next(DecodedField &out) override;
        void Reset() override;

      private:
        BinaryTelemetryV1Connector &owner_;
    };

    static void OnFrame(void *context, const FrameEvent &event);
    static void OnSample(void *context, const Sample &sample);

    BinaryTelemetryV1Framer framer_;
    BinaryTelemetryV1Decoder decoder_;
    SessionFacade session_{*this};
    DecoderFacade decoder_facade_{*this};

    std::array<Frame, frame_capacity> frames_{};
    std::size_t frame_count_{}, frame_read_{};
    // Message::bytes must point at storage that outlives Next, so the frame handed out is staged
    // here. Same-process only, exactly like FieldSession's record: never a disk or wire format.
    Frame staged_{};

    std::array<DecodedField, sample_capacity> samples_{};
    std::size_t sample_count_{}, sample_read_{};

    std::uint64_t frame_overflows_{}, sample_overflows_{}, oversize_payloads_{};
};

} // namespace signal_core
