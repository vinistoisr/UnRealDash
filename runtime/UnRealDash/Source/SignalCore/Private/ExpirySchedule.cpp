#include "SignalCore/ExpirySchedule.h"
namespace signal_core {
void ExpirySchedule::Cancel(ExpiryKind kind, std::uint32_t id) {
    for (std::size_t i = 0; i < size_; ++i)
        if (storage_[i].kind == kind && storage_[i].id == id) {
            for (std::size_t j = i + 1; j < size_; ++j)
                storage_[j - 1] = storage_[j];
            --size_;
            return;
        }
}
Status ExpirySchedule::Arm(Expiry expiry) {
    Cancel(expiry.kind, expiry.id);
    if (size_ == storage_.size())
        return Error(ErrorCode::capacity_exceeded, "expiry capacity for id %u", expiry.id);
    std::size_t i = size_++;
    while (i && storage_[i - 1].time > expiry.time) {
        storage_[i] = storage_[i - 1];
        --i;
    }
    storage_[i] = expiry;
    return {};
}
bool ExpirySchedule::PopDue(Time now, Expiry &expiry) {
    if (!size_ || storage_[0].time > now)
        return false;
    expiry = storage_[0];
    Cancel(expiry.kind, expiry.id);
    return true;
}
} // namespace signal_core
