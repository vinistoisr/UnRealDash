#include "SignalCore/Units.h"
#include <cmath>
#include <iterator>
namespace signal_core {
namespace {
struct Entry {
    const char *name;
    Quantity quantity;
    Unit si;
    double scale;
    double offset;
};
constexpr Entry entries[] = {{"dimensionless", Quantity::dimensionless, Unit::dimensionless, 1, 0},
                             {"K", Quantity::temperature, Unit::kelvin, 1, 0},
                             {"degC", Quantity::temperature, Unit::kelvin, 1, 273.15},
                             {"degF", Quantity::temperature, Unit::kelvin, 5.0 / 9.0, 273.15 - 32.0 * 5.0 / 9.0},
                             {"Pa", Quantity::pressure, Unit::pascal, 1, 0},
                             {"kPa", Quantity::pressure, Unit::pascal, 1000, 0},
                             {"bar", Quantity::pressure, Unit::pascal, 100000, 0},
                             {"psi", Quantity::pressure, Unit::pascal, 6894.757293168, 0},
                             {"m/s", Quantity::speed, Unit::metres_per_second, 1, 0},
                             {"km/h", Quantity::speed, Unit::metres_per_second, 1.0 / 3.6, 0},
                             {"mph", Quantity::speed, Unit::metres_per_second, 0.44704, 0},
                             {"rad/s", Quantity::angular_rate, Unit::radians_per_second, 1, 0},
                             {"rpm", Quantity::angular_rate, Unit::radians_per_second, 0.10471975511965977, 0}};
Result<double> Finite(double value, double input, Unit unit) {
    if (!std::isfinite(value))
        return Error(ErrorCode::non_finite, "non-finite unit conversion: input %.17g, unit %s", input, UnitName(unit));
    return value;
}
} // namespace
bool IsUnit(Unit unit) { return static_cast<unsigned>(unit) < std::size(entries); }
const char *UnitName(Unit unit) { return IsUnit(unit) ? entries[static_cast<unsigned>(unit)].name : "unspecified"; }
Quantity QuantityOf(Unit unit) {
    return IsUnit(unit) ? entries[static_cast<unsigned>(unit)].quantity : Quantity::dimensionless;
}
Unit SiUnit(Unit unit) { return IsUnit(unit) ? entries[static_cast<unsigned>(unit)].si : unit; }
Result<Unit> ParseUnit(std::string_view text) {
    for (unsigned i = 0; i < std::size(entries); ++i)
        if (text == entries[i].name)
            return static_cast<Unit>(i);
    return Error(ErrorCode::unknown_unit, "unknown unit '%.*s'", static_cast<int>(text.size()), text.data());
}
Result<double> ToSi(double value, Unit unit) {
    if (!IsUnit(unit))
        return Error(ErrorCode::unknown_unit, "unknown unit %u", static_cast<unsigned>(unit));
    const auto &e = entries[static_cast<unsigned>(unit)];
    return Finite(value * e.scale + e.offset, value, unit);
}
Result<double> FromSi(double value, Unit unit) {
    if (!IsUnit(unit))
        return Error(ErrorCode::unknown_unit, "unknown unit %u", static_cast<unsigned>(unit));
    const auto &e = entries[static_cast<unsigned>(unit)];
    return Finite((value - e.offset) / e.scale, value, unit);
}
Result<double> DeltaToSi(double value, Unit unit) {
    if (!IsUnit(unit))
        return Error(ErrorCode::unknown_unit, "unknown unit %u", static_cast<unsigned>(unit));
    return Finite(value * entries[static_cast<unsigned>(unit)].scale, value, unit);
}
} // namespace signal_core
