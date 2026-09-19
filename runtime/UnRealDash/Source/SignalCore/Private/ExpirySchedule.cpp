#include "SignalCore/ExpirySchedule.h"
namespace signal_core {

bool ExpirySchedule::Remove(ExpiryKind kind, std::uint32_t id, Expiry *out) {
    for (std::size_t i = 0; i < size_; ++i)
        if (storage_[i].kind == kind && storage_[i].id == id) {
            if (out)
                *out = storage_[i];
            for (std::size_t j = i + 1; j < size_; ++j)
                storage_[j - 1] = storage_[j];
            --size_;
            return true;
        }
    return false;
}

void ExpirySchedule::Cancel(ExpiryKind kind, std::uint32_t id, CancelReason reason, std::uint64_t by) {
    Expiry removed{};
    // Only a removal is a cancellation. A cancel of something that already fired, which
    // SignalRegistry::Expire does after PopDue has taken the entry, must write nothing: the
    // one-status-row rule is exactly this condition.
    if (Remove(kind, id, &removed) && sink_)
        sink_->OnCancelled(removed, reason, by);
}

void ExpirySchedule::Fire(ExpiryKind kind, std::uint32_t id, Time at) {
    Expiry removed{};
    if (Remove(kind, id, &removed) && sink_)
        sink_->OnFired(removed, at);
}

void ExpirySchedule::Clear(CancelReason reason, std::uint64_t by) {
    if (sink_)
        for (std::size_t i = 0; i < size_; ++i)
            sink_->OnCancelled(storage_[i], reason, by);
    size_ = 0;
}

std::uint64_t ExpirySchedule::SerialOf(ExpiryKind kind, std::uint32_t id) const {
    for (std::size_t i = 0; i < size_; ++i)
        if (storage_[i].kind == kind && storage_[i].id == id)
            return storage_[i].serial;
    return 0;
}

Status ExpirySchedule::Arm(Expiry expiry) {
    // Re-arming is a cancellation of the previous deadline, and PLAN wants it recorded as one,
    // naming the sample that moved it.
    Cancel(expiry.kind, expiry.id, CancelReason::rearmed, expiry.sample);
    if (size_ == storage_.size())
        return Error(ErrorCode::capacity_exceeded, "expiry capacity for id %u", expiry.id);
    expiry.serial = next_serial_++;
    std::size_t i = size_++;
    while (i && storage_[i - 1].time > expiry.time) {
        storage_[i] = storage_[i - 1];
        --i;
    }
    storage_[i] = expiry;
    if (sink_)
        sink_->OnArmed(expiry);
    return {};
}

bool ExpirySchedule::PopDue(Time now, Expiry &expiry) {
    if (!size_ || storage_[0].time > now)
        return false;
    expiry = storage_[0];
    // Removed directly rather than through Cancel: this is a firing, and routing it through the
    // cancellation path would write two status rows for one arming.
    Remove(expiry.kind, expiry.id, nullptr);
    if (sink_)
        sink_->OnFired(expiry, now);
    return true;
}

} // namespace signal_core
