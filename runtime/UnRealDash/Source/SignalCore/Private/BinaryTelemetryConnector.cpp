#include "SignalCore/BinaryTelemetryConnector.h"
#include <cstring>
#include <limits>

namespace signal_core {

BinaryTelemetryV1Connector::BinaryTelemetryV1Connector(const DefinitionPack &pack, const Clock &clock,
                                                       std::span<const std::uint32_t> enumerated_identifiers)
    : framer_(enumerated_identifiers), decoder_(pack, clock) {}

void BinaryTelemetryV1Connector::OnFrame(void *context, const FrameEvent &event) {
    auto &self = *static_cast<BinaryTelemetryV1Connector *>(context);
    if (event.payload_length > payload_bytes) {
        // Unreachable with this framer, which emits a fixed 8 byte payload. Counted rather than
        // truncated, because a truncated payload decodes to a plausible wrong number.
        ++self.oversize_payloads_;
        return;
    }
    if (self.frame_count_ >= frame_capacity) {
        ++self.frame_overflows_;
        return;
    }
    auto &slot = self.frames_[self.frame_count_++];
    slot.frame_id = event.frame_id;
    slot.stream_offset = event.stream_offset;
    slot.payload_length = event.payload_length;
    slot.payload = {};
    if (event.payload && event.payload_length)
        std::memcpy(slot.payload.data(), event.payload, event.payload_length);
}

void BinaryTelemetryV1Connector::OnSample(void *context, const Sample &sample) {
    auto &self = *static_cast<BinaryTelemetryV1Connector *>(context);
    if (self.sample_count_ >= sample_capacity) {
        ++self.sample_overflows_;
        return;
    }
    // The decoder's SampleSink carries no SignalId, so the pack's binding key travels in
    // Sample::source. BinaryTelemetryV1.cpp says so at the assignment; this is the other end of it.
    self.samples_[self.sample_count_++] = DecodedField{static_cast<SignalId>(sample.source), sample};
}

Status BinaryTelemetryV1Connector::SessionFacade::Offer(std::span<const std::uint8_t> input, std::size_t &consumed) {
    consumed = 0;
    if (input.empty())
        return Error(ErrorCode::need_more_data, "no input offered");
    // Whatever is still undrained stays at the front; the pipeline drains fully between Offers, so
    // in practice this starts empty every time.
    if (owner_.frame_read_ >= owner_.frame_count_)
        owner_.frame_count_ = owner_.frame_read_ = 0;

    owner_.framer_.Feed({reinterpret_cast<const std::byte *>(input.data()), input.size()}, &OnFrame, &owner_);
    // The framer consumes everything it is given, so the whole span is consumed. The pipeline
    // requires Offer to always make progress and treats a zero or over-long consume as a fault.
    consumed = input.size();
    return {};
}

Pending BinaryTelemetryV1Connector::SessionFacade::Next(Message &out) {
    if (owner_.frame_read_ >= owner_.frame_count_) {
        owner_.frame_count_ = owner_.frame_read_ = 0;
        return Pending::done;
    }
    owner_.staged_ = owner_.frames_[owner_.frame_read_++];
    // The staged frame is the message. Its bytes are a same-process record, the same arrangement
    // FieldSession uses, and never a disk or network encoding.
    out.bytes = {reinterpret_cast<const std::uint8_t *>(&owner_.staged_), sizeof(owner_.staged_)};
    return Pending::produced;
}

void BinaryTelemetryV1Connector::SessionFacade::Reset() {
    owner_.frame_count_ = owner_.frame_read_ = 0;
    // Carry and lock state only. The generation is set by the owner after a successful connect, not
    // advanced here; see the note on the class for why advancing it on Reset cannot be made to
    // track the pipeline across failed attempts.
    owner_.framer_.Reset();
}

Status BinaryTelemetryV1Connector::DecoderFacade::Offer(const Message &message) {
    if (message.bytes.size() != sizeof(Frame))
        return Error(ErrorCode::invalid_configuration, "frame record has the wrong size");
    owner_.sample_count_ = owner_.sample_read_ = 0;

    Frame frame{};
    std::memcpy(&frame, message.bytes.data(), sizeof(frame));
    const FrameEvent event{frame.frame_id, frame.stream_offset, frame.payload.data(), frame.payload_length};
    owner_.decoder_.OnFrame(event, &OnSample, &owner_);
    return {};
}

Pending BinaryTelemetryV1Connector::DecoderFacade::Next(DecodedField &out) {
    if (owner_.sample_read_ >= owner_.sample_count_) {
        owner_.sample_count_ = owner_.sample_read_ = 0;
        return Pending::done;
    }
    out = owner_.samples_[owner_.sample_read_++];
    return Pending::produced;
}

void BinaryTelemetryV1Connector::DecoderFacade::Reset() {
    owner_.sample_count_ = owner_.sample_read_ = 0;
}

Status BinaryTelemetryV1Connector::SetGeneration(std::uint64_t generation) {
    if (generation > std::numeric_limits<std::uint32_t>::max())
        return Error(ErrorCode::invalid_configuration, "connection generation exhausted");
    decoder_.SetConnectionGeneration(static_cast<std::uint32_t>(generation));
    return {};
}

} // namespace signal_core
