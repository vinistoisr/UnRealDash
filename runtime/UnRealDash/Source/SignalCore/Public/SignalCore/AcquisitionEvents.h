#pragma once
#include "SignalCore/Acquisition.h"
#include <algorithm>
#include <array>
namespace signal_core {
enum class AcquisitionEventKind : std::uint8_t { acquire, submit, rule_transition };
template <std::size_t Signals> struct AcquisitionEvent {
    std::uint32_t schema_version{1};
    AcquisitionEventKind kind{};
    std::uint64_t frame{};
    Time at{};
    std::array<SignalId, Signals> present{};
    std::size_t present_count{};
    std::uint32_t rule{};
    bool current{}, latched{};
};
// One producer per ring, one consumer. No slot is overwritten while the consumer owns it.
template <class Event, std::size_t Capacity> class AcquisitionEventRing {
  public:
    bool Push(const Event &event) {
        const auto head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) == Capacity) {
            ++drops_;
            return false;
        }
        entries_[head % Capacity] = event;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }
    bool Pop(Event &event) {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return false;
        event = entries_[tail % Capacity];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }
    std::uint64_t Drops() const { return drops_.load(); }

  private:
    static_assert(Capacity > 0);
    std::array<Event, Capacity> entries_{};
    std::atomic<std::uint64_t> head_{}, tail_{}, drops_{};
};
template <std::size_t Capacity, std::size_t Signals>
class RingAcquisitionEventSink final : public IAcquisitionEventSink {
  public:
    using Event = AcquisitionEvent<Signals>;
    void OnAcquire(std::uint64_t frame, Time at, std::span<const SignalId> present) override {
        if (present.size() > Signals) {
            ++oversize_;
            return;
        }
        Event event{};
        event.kind = AcquisitionEventKind::acquire;
        event.frame = frame;
        event.at = at;
        event.present_count = present.size();
        std::copy(present.begin(), present.end(), event.present.begin());
        game_.Push(event);
    }
    void OnSubmit(std::uint64_t frame, Time at) override {
        Event event{};
        event.kind = AcquisitionEventKind::submit;
        event.frame = frame;
        event.at = at;
        game_.Push(event);
    }
    void OnRuleTransition(std::uint32_t rule, Time at, bool current, bool latched) override {
        Event event{};
        event.kind = AcquisitionEventKind::rule_transition;
        event.rule = rule;
        event.at = at;
        event.current = current;
        event.latched = latched;
        acquisition_.Push(event);
    }
    bool PopGame(Event &event) { return game_.Pop(event); }
    bool PopAcquisition(Event &event) { return acquisition_.Pop(event); }
    std::uint64_t Drops() const { return game_.Drops() + acquisition_.Drops() + oversize_.load(); }

  private:
    AcquisitionEventRing<Event, Capacity> game_, acquisition_;
    std::atomic<std::uint64_t> oversize_{};
};
} // namespace signal_core
