#include "SignalCore/BinaryTelemetryV1.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace signal_core {
namespace {
bool Tag(const std::byte *p) {
    return p[0] == std::byte{0x44} && p[1] == std::byte{0x33} && p[2] == std::byte{0x22} && p[3] == std::byte{0x11};
}
std::uint32_t Identifier(const std::byte *p) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= std::to_integer<std::uint32_t>(p[i]) << (i * 8);
    return value;
}
bool Recent(Time now, Time previous, Time deadline) {
    const auto elapsed = SubtractTime(now, previous);
    return elapsed.Ok() && *elapsed.Get() >= Time::zero() && *elapsed.Get() <= deadline;
}
} // namespace
BinaryTelemetryV1Framer::BinaryTelemetryV1Framer(std::span<const std::uint32_t> ids) : identifiers_(ids) {}
bool BinaryTelemetryV1Framer::Candidate(const std::byte *p) const {
    return Tag(p) && std::find(identifiers_.begin(), identifiers_.end(), Identifier(p + 4)) != identifiers_.end();
}
void BinaryTelemetryV1Framer::Consume(std::size_t count, bool discarded) {
    if (discarded)
        counters_.bytes_discarded += count;
    offset_ += count;
    used_ -= count;
    std::memmove(carry_.data(), carry_.data() + count, used_);
}
void BinaryTelemetryV1Framer::Feed(std::span<const std::byte> bytes, FrameSink sink, void *context) {
    while (true) {
        if (used_ == 0 && state_ == FramerState::resync && bytes.size() >= 8) {
            if (Tag(bytes.data()) && !Candidate(bytes.data())) {
                ++counters_.unknown_identifier_dropped;
                ++offset_;
                ++counters_.bytes_discarded;
                bytes = bytes.subspan(1);
                continue;
            }
            if (!Candidate(bytes.data()) || (bytes.size() >= 20 && !Tag(bytes.data() + 16))) {
                ++offset_;
                ++counters_.bytes_discarded;
                bytes = bytes.subspan(1);
                continue;
            }
            if (bytes.size() >= 20) {
                if (sink)
                    sink(context, {Identifier(bytes.data() + 4), offset_, bytes.data() + 8, 8});
                ++counters_.frames_emitted;
                offset_ += 16;
                bytes = bytes.subspan(16);
                state_ = FramerState::synced;
                ++counters_.locks;
                continue;
            }
        }
        // Once locked, scan complete caller-owned frames without copying them.
        if (used_ == 0 && state_ == FramerState::synced && bytes.size() >= 16) {
            if (Candidate(bytes.data())) {
                if (sink)
                    sink(context, {Identifier(bytes.data() + 4), offset_, bytes.data() + 8, 8});
                ++counters_.frames_emitted;
                offset_ += 16;
                bytes = bytes.subspan(16);
                continue;
            }
            ++counters_.resyncs;
            state_ = FramerState::resync;
        }
        const std::size_t needed =
            state_ == FramerState::synced ? 16 : (used_ >= 8 && Candidate(carry_.data()) ? 20 : 8);
        if (used_ < needed) {
            const auto count = std::min(needed - used_, bytes.size());
            if (count)
                std::memcpy(carry_.data() + used_, bytes.data(), count);
            used_ += count;
            bytes = bytes.subspan(count);
            if (used_ < needed)
                return;
        }
        if (state_ == FramerState::synced) {
            if (!Candidate(carry_.data())) {
                ++counters_.resyncs;
                state_ = FramerState::resync;
                continue;
            }
        } else {
            if (Tag(carry_.data()) && !Candidate(carry_.data())) {
                ++counters_.unknown_identifier_dropped;
                Consume(1, true);
                continue;
            }
            if (!Candidate(carry_.data())) {
                Consume(1, true);
                continue;
            }
            if (used_ < 20)
                continue;
            if (!Tag(carry_.data() + 16)) {
                Consume(1, true);
                continue;
            }
            state_ = FramerState::synced;
            ++counters_.locks;
        }
        if (sink)
            sink(context, {Identifier(carry_.data() + 4), offset_, carry_.data() + 8, 8});
        ++counters_.frames_emitted;
        Consume(16, false);
    }
}
void BinaryTelemetryV1Framer::EndOfStream() {
    if (state_ == FramerState::resync && used_ >= 8 && Candidate(carry_.data()))
        ++counters_.unconfirmed_candidates_discarded;
    Consume(used_, true);
    state_ = FramerState::resync;
}
void BinaryTelemetryV1Framer::Reset() {
    EndOfStream();
    offset_ = 0;
}
bool TelemetryHealth::TransportConnected(Time now, Time deadline) const { return Recent(now, last_bytes, deadline); }
bool TelemetryHealth::AcquisitionReceiving(Time now, Time deadline) const {
    return Recent(now, last_telemetry_record, deadline);
}
BinaryTelemetryV1Decoder::BinaryTelemetryV1Decoder(const DefinitionPack &pack, const Clock &clock)
    : pack_(pack), clock_(clock), validation_(ValidatePack(pack)) {
    health_ = {Time::min(), Time::min(), Time::min()};
}
void BinaryTelemetryV1Decoder::SetConnectionGeneration(std::uint32_t generation) {
    generation_ = generation;
    sequence_ = 0;
    health_ = {Time::min(), Time::min(), Time::min()};
}
void BinaryTelemetryV1Decoder::OnBytesReceived() { health_.last_bytes = clock_.Now(); }
void BinaryTelemetryV1Decoder::OnFrame(const FrameEvent &event, SampleSink sink, void *context) {
    if (!validation_.Ok())
        return;
    const auto found = std::find_if(pack_.frames.begin(), pack_.frames.end(),
                                    [&](const auto &f) { return f.frame_id == event.frame_id; });
    if (found == pack_.frames.end()) {
        ++counters_.unknown_identifier_dropped;
        return;
    }
    if (!event.payload || event.payload_length < found->payload_length)
        return;
    const auto now = clock_.Now();
    health_.last_bytes = now;
    if (found->role == FrameRole::status) {
        ++counters_.status_records;
        health_.last_status_record = now;
    } else
        health_.last_telemetry_record = now;
    for (const auto &field : found->fields) {
        if (!field.width_bytes || field.width_bytes > 4 ||
            static_cast<unsigned>(field.byte_offset) + field.width_bytes > event.payload_length)
            continue;
        std::uint64_t bits = 0;
        for (unsigned i = 0; i < field.width_bytes; ++i) {
            const auto index = field.little_endian ? i : field.width_bytes - 1u - i;
            bits |= std::to_integer<std::uint64_t>(event.payload[field.byte_offset + index]) << (i * 8);
        }
        auto raw = static_cast<std::int64_t>(bits);
        const auto bit_count = field.width_bytes * 8u;
        if (field.is_signed && (bits & (std::uint64_t{1} << (bit_count - 1))))
            raw -= std::int64_t{1} << bit_count;
        Sample sample;
        sample.unit = field.unit;
        // SampleSink has no SignalId argument. Source carries the pack's binding key.
        sample.source = field.signal;
        sample.seq = ++sequence_;
        sample.t_recv = now;
        sample.generation = generation_;
        sample.age_evidence = field.acquisition == Acquisition::held ? AgeEvidence::unknown : AgeEvidence::measured;
        if (std::find(field.sentinels.begin(), field.sentinels.end(), raw) != field.sentinels.end()) {
            ++counters_.sentinel_rejections;
            sample.value = std::numeric_limits<double>::quiet_NaN();
            sample.quality = Quality::unavailable;
        } else {
            sample.value = static_cast<double>(raw) * field.scale + field.offset;
            if (std::isfinite(sample.value))
                sample.quality = Quality::valid;
            else {
                sample.value = std::numeric_limits<double>::quiet_NaN();
                sample.quality = Quality::invalid;
                ++counters_.non_finite_results;
            }
        }
        if (sink) {
            sink(context, sample);
            ++counters_.samples_published;
        }
    }
}
Status ReconnectBinaryTelemetryV1(BinaryTelemetryV1Framer &framer, BinaryTelemetryV1Decoder &decoder) {
    if (!decoder.GetStatus().Ok())
        return decoder.GetStatus();
    if (decoder.ConnectionGeneration() == std::numeric_limits<std::uint32_t>::max())
        return Error(ErrorCode::invalid_configuration, "connection generation exhausted");
    framer.Reset();
    decoder.SetConnectionGeneration(decoder.ConnectionGeneration() + 1);
    return {};
}
} // namespace signal_core
