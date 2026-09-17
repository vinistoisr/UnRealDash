#include "SignalCore/SnapshotExchange.h"
namespace signal_core {
SnapshotExchange::SnapshotExchange(SnapshotBuffer a, SnapshotBuffer b, SnapshotBuffer c) : buffers_{a, b, c} {}
bool SnapshotExchange::CompatibleBuffers() const {
    for (unsigned i = 0; i < 3; ++i) {
        if (buffers_[i].samples.size() != buffers_[0].samples.size() ||
            buffers_[i].latched.size() != buffers_[0].latched.size())
            return false;
        for (unsigned j = 0; j < i; ++j) {
            if (!buffers_[i].samples.empty() && buffers_[i].samples.data() == buffers_[j].samples.data())
                return false;
            if (!buffers_[i].latched.empty() && buffers_[i].latched.data() == buffers_[j].latched.data())
                return false;
        }
    }
    return true;
}
std::uint64_t SnapshotExchange::Publish() {
    ++writer_version_;
    const auto previous = ready_.exchange((writer_version_ << 2) | writer_, std::memory_order_acq_rel);
    writer_ = static_cast<unsigned>(previous & 3);
    return writer_version_;
}
Snapshot SnapshotExchange::Acquire() {
    auto ready = ready_.load(std::memory_order_acquire);
    bool changed = false;
    while ((ready >> 2) > reader_version_) {
        if (ready_.compare_exchange_weak(ready, (reader_version_ << 2) | reader_, std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
            reader_ = static_cast<unsigned>(ready & 3);
            reader_version_ = ready >> 2;
            changed = true;
            break;
        }
    }
    const auto &b = buffers_[reader_];
    return {b.samples, b.latched, b.generation, reader_version_, changed};
}
bool SampleQueue::Push(const SignalSample &sample) {
    const auto head = head_.load(std::memory_order_relaxed);
    if (head - tail_.load(std::memory_order_acquire) >= storage_.size()) {
        drops_.fetch_add(1);
        return false;
    }
    storage_[static_cast<std::size_t>(head % storage_.size())] = sample;
    head_.store(head + 1, std::memory_order_release);
    return true;
}
bool SampleQueue::Pop(SignalSample &sample) {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire))
        return false;
    sample = storage_[static_cast<std::size_t>(tail % storage_.size())];
    tail_.store(tail + 1, std::memory_order_release);
    return true;
}
std::size_t SampleQueue::Depth() const {
    const auto tail = tail_.load(std::memory_order_acquire);
    const auto head = head_.load(std::memory_order_acquire);
    return static_cast<std::size_t>(head - tail);
}
} // namespace signal_core
