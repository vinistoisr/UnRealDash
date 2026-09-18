#include "SignalCore/Reconnect.h"

namespace signal_core {

void BackoffPolicy::Failed(Time now) {
    ++attempts_;
    // The interval that just elapsed is doubled for the next one, to the ceiling. AddTime is used
    // rather than plain addition because it refuses a monotonic overflow, and a schedule that
    // silently wrapped would make every attempt due forever, which is the spin loop this exists to
    // prevent.
    const auto next = AddTime(now, interval_);
    next_attempt_ = next.Ok() ? *next.Get() : now;
    if (interval_ < ceiling) {
        interval_ = interval_ * 2;
        if (interval_ > ceiling)
            interval_ = ceiling;
    }
}

void BackoffPolicy::Succeeded() {
    interval_ = first;
    next_attempt_ = Time::zero();
    attempts_ = 0;
}

ConnectionSupervisor::Action ConnectionSupervisor::Poll(Time now, bool connected) const {
    if (connected)
        return Action::none;
    return policy_.Due(now) ? Action::reconnect : Action::waiting;
}

void ConnectionSupervisor::Attempted(Time now, bool succeeded) {
    if (succeeded)
        policy_.Succeeded();
    else
        policy_.Failed(now);
}

void ReportBackoff(const ConnectionSupervisor &supervisor, ConnectionHealth &health) {
    const auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(supervisor.Policy().Interval());
    health.backoff_ms = static_cast<std::uint64_t>(interval.count());
    health.attempts_since_connect = supervisor.AttemptsSinceConnect();
}

} // namespace signal_core
