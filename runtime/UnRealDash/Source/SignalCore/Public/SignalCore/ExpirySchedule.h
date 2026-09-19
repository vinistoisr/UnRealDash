#pragma once
#include "SignalCore/Clock.h"
#include "SignalCore/Errors.h"
#include "SignalCore/Export.h"
#include <span>
namespace signal_core {
enum class ExpiryKind : std::uint8_t { freshness, hold_last, debounce };

// Why an armed expiry will not fire. PLAN 4.8 wants the reason in the status row, because
// "cancelled" alone cannot tell a deadline that moved from a connection that ended.
enum class CancelReason : std::uint8_t { rearmed, generation, run_end };

struct Expiry {
    Time time{};
    ExpiryKind kind{};
    std::uint32_t id{};
    // PLAN 4.8's expiry identifier: unique per ARMING, not per signal. Arm has always cancelled
    // and replaced an entry with the same kind and id, so without this nothing can tell two
    // armings of the same deadline apart, and a status row cannot be matched to the arming it
    // belongs to. Assigned by the schedule; zero means unassigned.
    std::uint64_t serial{};
    // The sample and generation that armed it, and the quality the signal takes when it fires.
    // Carried on the entry rather than looked up by the sink, which has no way to know them.
    std::uint64_t sample{};
    std::uint64_t generation{};
    std::uint8_t becomes{};
};

// PLAN 4.8's armed-expiry rows. Optional: a run with no event log passes nothing and pays
// nothing.
//
// Exactly one status row per arming is a property of two functions rather than of a convention.
// Fire and PopDue are the only paths that fire, each removes the entry, and whichever runs second
// finds nothing and emits nothing. Cancel emits only when it actually removed something, and
// PopDue's own removal is not a cancel.
struct IExpiryEventSink {
    virtual ~IExpiryEventSink() = default;
    virtual void OnArmed(const Expiry &expiry) = 0;
    virtual void OnFired(const Expiry &expiry, Time at) = 0;
    virtual void OnCancelled(const Expiry &expiry, CancelReason reason, std::uint64_t by) = 0;
};

// Single-writer ordered set. Storage and capacity belong to the caller.
class SIGNALCORE_API ExpirySchedule {
  public:
    explicit ExpirySchedule(std::span<Expiry> storage) : storage_(storage) {}
    // Null detaches. Set before the first Arm, or the rows describe half a run.
    void SetSink(IExpiryEventSink *sink) { sink_ = sink; }

    Status Arm(Expiry expiry);
    // Calls an expiry off. Emits nothing when there was nothing to remove, which is what keeps a
    // second cancel of an already-fired entry from writing a second status row.
    void Cancel(ExpiryKind kind, std::uint32_t id, CancelReason reason = CancelReason::rearmed,
                std::uint64_t by = 0);
    // The deadline passed and the caller has acted on it. Not a cancel: SignalRegistry::Expire
    // marking a sample stale IS the firing, and recording it as a cancellation would lose the
    // start endpoint of every expiry-to-present observation.
    void Fire(ExpiryKind kind, std::uint32_t id, Time at);
    // Every remaining entry is cancelled with the given reason, which is run_end on an orderly
    // shutdown and generation on a reconnect.
    void Clear(CancelReason reason = CancelReason::generation, std::uint64_t by = 0);

    const Expiry *Earliest() const { return size_ ? &storage_[0] : nullptr; }
    // The identifier of the entry currently armed for this kind and id, or zero. A caller that
    // is about to act on a deadline needs it before the entry goes away.
    std::uint64_t SerialOf(ExpiryKind kind, std::uint32_t id) const;
    bool PopDue(Time now, Expiry &expiry);
    std::size_t Size() const { return size_; }

  private:
    // Removes without notifying. The one place that decides what an entry's removal MEANS is the
    // caller of this, which is why it is private.
    bool Remove(ExpiryKind kind, std::uint32_t id, Expiry *out);

    std::span<Expiry> storage_;
    std::size_t size_{};
    IExpiryEventSink *sink_{};
    // Starts at 1 so zero keeps meaning unassigned.
    std::uint64_t next_serial_{1};
};
} // namespace signal_core
