#pragma once
#include "SignalCore/Errors.h"
#include <chrono>
namespace signal_core {
using Time = std::chrono::nanoseconds;
inline Result<Time> AddTime(Time time, Time duration) {
    if (duration < Time::zero() || time.count() > Time::max().count() - duration.count())
        return Error(ErrorCode::invalid_configuration, "monotonic time overflow or negative duration");
    return time + duration;
}
inline Result<Time> SubtractTime(Time left, Time right) {
    if ((right.count() > 0 && left.count() < Time::min().count() + right.count()) ||
        (right.count() < 0 && left.count() > Time::max().count() + right.count()))
        return Error(ErrorCode::invalid_configuration, "monotonic time subtraction overflow");
    return left - right;
}
struct Clock {
    void *context{};
    Time (*now)(void *){};
    Time Now() const { return now(context); }
};
} // namespace signal_core
