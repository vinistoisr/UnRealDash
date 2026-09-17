#include "SignalCore/MemoryConnector.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace signal_core {
static_assert(std::is_trivially_copyable_v<DecodedField>);
Status MemoryTransport::Read(std::span<std::uint8_t> into, std::size_t &written) {
    written = 0;
    if (!connected_)
        return Error(ErrorCode::io_error, "memory transport disconnected");
    written = std::min(into.size(), bytes_.size() - offset_);
    if (written)
        std::memcpy(into.data(), bytes_.data() + offset_, written);
    offset_ += written;
    return written ? Status{} : Error(ErrorCode::need_more_data, "memory transport has no data");
}
Status FieldSession::Offer(std::span<const std::uint8_t> input, std::size_t &consumed) {
    if (delivered_)
        Reset();
    consumed = std::min(input.size(), record_.size() - size_);
    if (consumed)
        std::memcpy(record_.data() + size_, input.data(), consumed);
    size_ += consumed;
    return {};
}
Pending FieldSession::Next(Message &out) {
    if (size_ != record_.size() || delivered_)
        return Pending::need_more_data;
    out.bytes = record_;
    delivered_ = true;
    return Pending::produced;
}
Status FieldDecoder::Offer(const Message &message) {
    if (ready_ || message.bytes.size() != sizeof(field_))
        return Error(ErrorCode::invalid_configuration, "field message size or undrained decoder");
    std::memcpy(&field_, message.bytes.data(), sizeof(field_));
    ready_ = true;
    return {};
}
Pending FieldDecoder::Next(DecodedField &out) {
    if (!ready_)
        return Pending::need_more_data;
    out = field_;
    ready_ = false;
    return Pending::produced;
}
Pending SiMapping::Map(const DecodedField &in, DecodedField &out) {
    out = in;
    if (!IsUnit(in.sample.unit))
        return Pending::done;
    if (std::isfinite(in.sample.value)) {
        auto value = ToSi(in.sample.value, in.sample.unit);
        if (value.Ok())
            out.sample.value = *value.Get();
        else {
            out.sample.value = std::numeric_limits<double>::quiet_NaN();
            out.sample.quality = Quality::invalid;
        }
    }
    out.sample.unit = SiUnit(in.sample.unit);
    return Pending::produced;
}
Status ReplayTransport::Connect() {
    if (previous_.size() != staging_.Samples().size())
        return Error(ErrorCode::invalid_configuration, "replay transport comparison storage");
    if (staging_.Generation() == UINT64_MAX)
        return Error(ErrorCode::invalid_configuration, "replay generation overflow");
    auto status = staging_.SetGeneration(staging_.Generation() + 1);
    if (!status.Ok())
        return status;
    status = replay_.Start(recording_, false, Time::zero());
    if (!status.Ok())
        return status;
    std::copy(staging_.Samples().begin(), staging_.Samples().end(), previous_.begin());
    connected_ = true;
    return {};
}
Status ReplayTransport::Read(std::span<std::uint8_t> into, std::size_t &written) {
    written = 0;
    if (!connected_)
        return Error(ErrorCode::io_error, "replay transport disconnected");
    if (offset_ == size_) {
        auto step = replay_.Step();
        if (!step.Ok())
            return step.GetStatus();
        if (!*step.Get())
            return Error(ErrorCode::need_more_data, "replay has no due sample");
        bool found = false;
        auto samples = staging_.Samples();
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto &entry = samples[i];
            if (entry.received && (!previous_[i].received || entry.sample.seq != previous_[i].sample.seq ||
                                   entry.sample.generation != previous_[i].sample.generation)) {
                DecodedField field{entry.signal, entry.sample};
                std::memcpy(record_.data(), &field, sizeof(field));
                found = true;
            }
            previous_[i] = entry;
        }
        if (!found)
            return Error(ErrorCode::invalid_configuration, "replay step emitted no identifiable sample");
        offset_ = 0;
        size_ = record_.size();
    }
    written = std::min(into.size(), size_ - offset_);
    if (written)
        std::memcpy(into.data(), record_.data() + offset_, written);
    offset_ += written;
    return {};
}
} // namespace signal_core
