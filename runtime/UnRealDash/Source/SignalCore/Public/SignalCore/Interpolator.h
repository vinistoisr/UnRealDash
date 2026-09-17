#pragma once
#include "SignalCore/Export.h"
#include "SignalCore/Sample.h"
namespace signal_core {
struct DisplayValue {
    double value{std::numeric_limits<double>::quiet_NaN()};
    Quality quality{Quality::unavailable};
    Status status{};
};
class SIGNALCORE_API Interpolator {
  public:
    Status Configure(const Signal &signal, Time maximum_extrapolation);
    DisplayValue Display(const Sample &previous, const Sample &latest, Time now) const;

  private:
    Signal signal_{};
    Time window_{};
    bool configured_{};
};
} // namespace signal_core
