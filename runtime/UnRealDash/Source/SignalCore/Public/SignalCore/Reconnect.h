#pragma once
#include "SignalCore/Clock.h"
#include "SignalCore/Connector.h"

namespace signal_core {

// The retry schedule, with no socket, no thread and no real clock in it.
//
// PLAN 4.9 requires exponential backoff from 500 ms to a 5 s ceiling, and requires proving that an
// absent relay is not a spin loop. Keeping the schedule separate from the socket is what makes that
// proof cheap: the whole of it is a unit test over an injected clock that runs in microseconds,
// rather than a sixty second wall-clock test nobody will run twice.
class SIGNALCORE_API BackoffPolicy {
  public:
    static constexpr Time first = std::chrono::milliseconds(500);
    static constexpr Time ceiling = std::chrono::seconds(5);

    // Whether another attempt is due. The first attempt after a reset is always due, so a connector
    // does not wait 500 ms before trying at all.
    bool Due(Time now) const { return attempts_ == 0 || now >= next_attempt_; }

    // Records that an attempt was made and failed, and schedules the next one.
    void Failed(Time now);

    // A successful connection. The schedule returns to the first interval, so a link that drops
    // once a minute does not creep up to the ceiling and stay there.
    void Succeeded();

    Time Interval() const { return interval_; }
    Time NextAttemptAt() const { return next_attempt_; }
    std::uint64_t Attempts() const { return attempts_; }

  private:
    Time interval_{first};
    Time next_attempt_{};
    std::uint64_t attempts_{};
};

// Decides when a disconnected pipeline should be told to reconnect.
//
// It does not live inside AcquisitionPipeline and it is not a transport decorator, for a reason
// worth stating where someone will read it. Pump's read loop is gated on health_.connected
// (Acquisition.cpp), so a disconnected transport is never polled and a decorator's Read would never
// be called. And Connect is the only place the connection generation advances, with the registry
// rejecting samples from an unconnected generation, so a decorator that quietly reconnected the
// socket underneath a still-"connected" pipeline would defeat exactly the guard that exists to
// catch a stale link.
//
// So the supervisor sits beside the pipeline. The owner calls Poll once per tick, and when it says
// reconnect is due the owner calls AcquisitionPipeline::Reconnect and reports the outcome back.
class SIGNALCORE_API ConnectionSupervisor {
  public:
    enum class Action : std::uint8_t {
        // Connected and reading; nothing to do.
        none,
        // Disconnected and the schedule says try now.
        reconnect,
        // Disconnected and waiting out the backoff. The caller does nothing, which is what makes an
        // absent relay cost no work rather than a connect attempt per tick.
        waiting,
    };

    Action Poll(Time now, bool connected) const;

    // The outcome of the attempt Poll asked for.
    void Attempted(Time now, bool succeeded);

    const BackoffPolicy &Policy() const { return policy_; }
    // Attempts since the last successful connection, which is what the health interface reports
    // alongside the current interval.
    std::uint64_t AttemptsSinceConnect() const { return policy_.Attempts(); }

  private:
    BackoffPolicy policy_;
};

// Fills in the backoff half of ConnectionHealth from a supervisor. Kept as a free function so the
// supervisor does not need to know about the health struct, and so a connector that never retries
// simply does not call it and reports zeros.
SIGNALCORE_API void ReportBackoff(const ConnectionSupervisor &supervisor, ConnectionHealth &health);

} // namespace signal_core
