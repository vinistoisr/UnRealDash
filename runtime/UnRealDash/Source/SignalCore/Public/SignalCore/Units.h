#pragma once
#include "SignalCore/Errors.h"
#include "SignalCore/Export.h"
#include <string_view>
namespace signal_core {
enum class Quantity : std::uint8_t { dimensionless, temperature, pressure, speed, angular_rate };
enum class Unit : std::uint16_t {
    dimensionless,
    kelvin,
    degree_celsius,
    degree_fahrenheit,
    pascal,
    kilopascal,
    bar,
    psi,
    metres_per_second,
    kilometres_per_hour,
    miles_per_hour,
    radians_per_second,
    revolutions_per_minute
};
SIGNALCORE_API Result<Unit> ParseUnit(std::string_view text);
SIGNALCORE_API Quantity QuantityOf(Unit unit);
SIGNALCORE_API bool IsUnit(Unit unit);
SIGNALCORE_API const char *UnitName(Unit unit);
SIGNALCORE_API Unit SiUnit(Unit unit);
SIGNALCORE_API Result<double> ToSi(double value, Unit unit);
SIGNALCORE_API Result<double> FromSi(double value, Unit unit);
SIGNALCORE_API Result<double> DeltaToSi(double value, Unit unit);
} // namespace signal_core
