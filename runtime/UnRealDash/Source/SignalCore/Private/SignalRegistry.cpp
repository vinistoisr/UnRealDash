#include "SignalCore/SignalRegistry.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>
namespace signal_core {
SignalRegistry::SignalRegistry(Clock clock, std::span<const Signal> signals, ExpirySchedule &schedule,
                               SnapshotExchange &exchange)
    : clock_(clock), signals_(signals), storage_(exchange.WriterBuffer().samples), schedule_(schedule),
      exchange_(exchange) {}
Status SignalRegistry::Initialize() {
    if (!clock_.now || storage_.size() != signals_.size() || !exchange_.CompatibleBuffers())
        return Error(ErrorCode::invalid_configuration, "registry storage or clock");
    for (std::size_t i = 0; i < signals_.size(); ++i) {
        const auto &s = signals_[i];
        if (!IsUnit(s.unit) || s.deadline <= Time::zero() || !std::memchr(s.name, 0, sizeof(s.name)))
            return Error(ErrorCode::invalid_configuration, "signal id %u declaration", s.id);
        for (std::size_t j = 0; j < i; ++j)
            if (signals_[j].id == s.id)
                return Error(ErrorCode::invalid_configuration, "duplicate signal %s", s.name);
        storage_[i] = {s.id, {}};
    }
    return {};
}
Status SignalRegistry::SetGeneration(std::uint64_t generation) {
    if (generation < generation_)
        return Error(ErrorCode::old_generation, "generation is older than registry");
    if (generation == generation_)
        return {};
    generation_ = generation;
    schedule_.Clear();
    for (std::size_t i = 0; i < signals_.size(); ++i)
        storage_[i] = {signals_[i].id, {}};
    return {};
}
Status SignalRegistry::Apply(SignalId id, const Sample &sample) {
    if (sample.generation < generation_) {
        ++rejected_;
        return Error(ErrorCode::old_generation, "signal %u old generation", id);
    }
    std::size_t i = 0;
    for (; i < signals_.size() && signals_[i].id != id; ++i) {
    }
    if (i == signals_.size())
        return Error(ErrorCode::missing_signal, "signal %u not declared", id);
    const auto &declaration = signals_[i];
    if (!IsUnit(sample.unit) || QuantityOf(sample.unit) != QuantityOf(declaration.unit))
        return Error(ErrorCode::incompatible_units, "signal %s: %s and %s", declaration.name, UnitName(sample.unit),
                     UnitName(declaration.unit));
    if (sample.generation > generation_) {
        const auto status = SetGeneration(sample.generation);
        if (!status.Ok())
            return status;
    }
    const auto &old = storage_[i].sample;
    if (storage_[i].received && (sample.seq <= old.seq || sample.t_recv < old.t_recv)) {
        ++rejected_ordering_;
        return Error(ErrorCode::invalid_sequence, "signal %s sequence or receive time regressed", declaration.name);
    }
    Sample normalized = sample;
    normalized.unit = SiUnit(sample.unit);
    if (std::isfinite(sample.value) || sample.quality == Quality::valid) {
        auto value = ToSi(sample.value, sample.unit);
        if (!value.Ok()) {
            normalized.value = std::numeric_limits<double>::quiet_NaN();
            normalized.quality = Quality::invalid;
        } else
            normalized.value = *value.Get();
    }
    normalized.expiry = 0;
    if (normalized.quality == Quality::valid) {
        auto expiry = AddTime(sample.t_recv, declaration.deadline);
        if (!expiry.Ok())
            return Error(ErrorCode::invalid_configuration, "signal %s deadline overflow", declaration.name);
        Expiry armed{};
        armed.time = *expiry.Get();
        armed.kind = ExpiryKind::freshness;
        armed.id = id;
        armed.sample = normalized.id;
        armed.generation = normalized.generation;
        armed.becomes = static_cast<std::uint8_t>(Quality::stale);
        auto status = schedule_.Arm(armed);
        if (!status.Ok())
            return status;
    } else
        schedule_.Cancel(ExpiryKind::freshness, id);
    storage_[i] = {id, normalized, true};
    Expire();
    return {};
}
void SignalRegistry::MarkFired(const Expiry &expiry) {
    if (expiry.kind != ExpiryKind::freshness)
        return;
    for (std::size_t i = 0; i < signals_.size(); ++i) {
        if (signals_[i].id != expiry.id)
            continue;
        auto &sample = storage_[i].sample;
        if (sample.quality == Quality::valid)
            sample.quality = Quality::stale;
        sample.expiry = expiry.serial;
        return;
    }
}
void SignalRegistry::Expire() {
    const auto now = clock_.Now();
    for (std::size_t i = 0; i < signals_.size(); ++i) {
        auto &sample = storage_[i].sample;
        if (sample.quality != Quality::valid)
            continue;
        // Apply only stores a valid sample whose expiry is representable, so a failed AddTime here means
        // the invariant was broken elsewhere; treat it as expired rather than compute an overflowing sum.
        const auto expiry = AddTime(sample.t_recv, signals_[i].deadline);
        if (!expiry.Ok() || now >= *expiry.Get()) {
            sample.quality = Quality::stale;
            // Before the Fire, because Fire removes the entry and the serial is on it. Read from
            // the schedule rather than remembered here: PopDue may already have fired it in this
            // same Pump, in which case Earliest no longer holds it and the sample keeps the
            // serial that firing stamped.
            if (const std::uint64_t serial = schedule_.SerialOf(ExpiryKind::freshness, signals_[i].id))
                sample.expiry = serial;
            schedule_.Fire(ExpiryKind::freshness, signals_[i].id, now);
        }
    }
}
Status SignalRegistry::Publish(std::span<const std::uint8_t> latched) {
    auto &buffer = exchange_.WriterBuffer();
    if (buffer.samples.size() != signals_.size() || buffer.latched.size() != latched.size())
        return Error(ErrorCode::invalid_configuration, "snapshot spans must match registry and rule count");
    std::copy_n(storage_.begin(), signals_.size(), buffer.samples.begin());
    std::copy(latched.begin(), latched.end(), buffer.latched.begin());
    buffer.generation = generation_;
    exchange_.Publish();
    // Preserve writer state in the slot returned by publication. The published slot is
    // read-only until a later publication, so this copy can overlap reader acquisition.
    auto next = exchange_.WriterBuffer().samples;
    if (next.size() != signals_.size())
        return Error(ErrorCode::invalid_configuration, "snapshot sample spans differ");
    std::copy(storage_.begin(), storage_.end(), next.begin());
    storage_ = next;
    return {};
}
} // namespace signal_core
