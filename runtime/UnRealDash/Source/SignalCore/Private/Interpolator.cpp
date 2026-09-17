#include "SignalCore/Interpolator.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace signal_core {
Status Interpolator::Configure(const Signal &signal, Time maximum_extrapolation) {
    configured_ = false;
    if (maximum_extrapolation < Time::zero() || signal.deadline <= Time::zero() ||
        (signal.discrete && maximum_extrapolation > Time::zero()))
        return Error(ErrorCode::invalid_configuration, "signal %s interpolation configuration", signal.name);
    signal_ = signal;
    window_ = maximum_extrapolation;
    configured_ = true;
    return {};
}
DisplayValue Interpolator::Display(const Sample &previous, const Sample &latest, Time now) const {
    if (!configured_)
        return {};
    if (latest.quality != Quality::valid)
        return {latest.value, latest.quality};
    const auto age_result = SubtractTime(now, latest.t_recv);
    if (!age_result.Ok())
        return {std::numeric_limits<double>::quiet_NaN(), Quality::invalid, age_result.GetStatus()};
    const auto age = *age_result.Get();
    const bool stale = age >= signal_.deadline;
    double value = latest.value;
    if (previous.quality == Quality::valid && previous.generation == latest.generation &&
        latest.t_recv > previous.t_recv && window_ > Time::zero()) {
        const auto interval = SubtractTime(latest.t_recv, previous.t_recv);
        if (!interval.Ok())
            return {std::numeric_limits<double>::quiet_NaN(), Quality::invalid, interval.GetStatus()};
        const auto elapsed = std::clamp(age, -*interval.Get(), std::min(window_, signal_.deadline));
        value += (latest.value - previous.value) * static_cast<double>(elapsed.count()) /
                 static_cast<double>(interval.Get()->count());
    }
    if (!std::isfinite(value))
        return {std::numeric_limits<double>::quiet_NaN(), Quality::invalid};
    return {value, stale ? Quality::stale : Quality::valid};
}
} // namespace signal_core
