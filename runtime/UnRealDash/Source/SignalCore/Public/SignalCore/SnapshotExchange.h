#pragma once
#include "SignalCore/Export.h"
#include "SignalCore/Sample.h"
#include <atomic>
#include <span>
namespace signal_core {
struct SnapshotBuffer {
    std::span<SignalSample> samples{};
    std::span<std::uint8_t> latched{};
    std::uint64_t generation{};
};
struct Snapshot {
    std::span<const SignalSample> samples{};
    std::span<const std::uint8_t> latched{};
    std::uint64_t generation{};
    std::uint64_t version{};
    bool new_publication{};
};
// Exactly one writer and one reader. A snapshot lives until that reader's next successful acquire.
// Slot ownership transfers with acquire/release on the packed ready version and index.
class SIGNALCORE_API SnapshotExchange {
  public:
    SnapshotExchange(SnapshotBuffer first, SnapshotBuffer second, SnapshotBuffer third);
    bool CompatibleBuffers() const;
    SnapshotBuffer &WriterBuffer() { return buffers_[writer_]; }
    std::uint64_t Publish();
    Snapshot Acquire();
    std::uint64_t WriterBlockedCount() const { return 0; }

  private:
    SnapshotBuffer buffers_[3];
    std::atomic<std::uint64_t> ready_{1};
    unsigned writer_{0};
    unsigned reader_{2};
    std::uint64_t writer_version_{};
    std::uint64_t reader_version_{};
};
// Single-producer single-consumer queue, drop newest on full. The caller provides the bound.
class SIGNALCORE_API SampleQueue {
  public:
    explicit SampleQueue(std::span<SignalSample> storage) : storage_(storage) {}
    bool Push(const SignalSample &sample);
    bool Pop(SignalSample &sample);
    std::uint64_t Drops() const { return drops_.load(); }
    std::size_t Depth() const;

  private:
    std::span<SignalSample> storage_;
    std::atomic<std::uint64_t> head_{};
    std::atomic<std::uint64_t> tail_{};
    std::atomic<std::uint64_t> drops_{};
};
} // namespace signal_core
