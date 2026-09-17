#pragma once
#include "SignalCore/Clock.h"
#include "SignalCore/Errors.h"
#include "SignalCore/Export.h"
#include <span>
namespace signal_core {
enum class ExpiryKind : std::uint8_t { freshness, hold_last, debounce };
struct Expiry {
    Time time{};
    ExpiryKind kind{};
    std::uint32_t id{};
};
// Single-writer ordered set. Storage and capacity belong to the caller.
class SIGNALCORE_API ExpirySchedule {
  public:
    explicit ExpirySchedule(std::span<Expiry> storage) : storage_(storage) {}
    Status Arm(Expiry expiry);
    void Cancel(ExpiryKind kind, std::uint32_t id);
    void Clear() { size_ = 0; }
    const Expiry *Earliest() const { return size_ ? &storage_[0] : nullptr; }
    bool PopDue(Time now, Expiry &expiry);
    std::size_t Size() const { return size_; }

  private:
    std::span<Expiry> storage_;
    std::size_t size_{};
};
} // namespace signal_core
