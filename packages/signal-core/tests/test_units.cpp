#include "support/FakeClock.h"
#include <algorithm>
#include <cmath>
#include <cstring>
TEST_CASE("2.3 round trip within tolerance") {
    for (unsigned i = 0; i < 13; ++i)
        for (double input : {0.0, 1e-9, -1e-9, -40.0, 100.0, 1000000.0}) {
            const auto unit = static_cast<Unit>(i);
            auto si = ToSi(input, unit);
            REQUIRE(si.Ok());
            auto output = FromSi(*si.Get(), unit);
            REQUIRE(output.Ok());
            CHECK(std::abs(input - *output.Get()) <=
                  std::max(1e-5, 1e-6 * std::max(std::abs(input), std::abs(*output.Get()))));
        }
}
TEST_CASE("2.3 unknown unit string is rejected") {
    auto unit = ParseUnit("furlongs");
    CHECK_FALSE(unit.Ok());
    CHECK(unit.GetStatus().code == ErrorCode::unknown_unit);
    CHECK(std::strstr(unit.GetStatus().message, "furlongs") != nullptr);
    CHECK(unit.Get() == nullptr);
}
TEST_CASE("2.3 quantity classification") {
    const Quantity quantities[] = {Quantity::dimensionless, Quantity::temperature, Quantity::temperature,
                                   Quantity::temperature,   Quantity::pressure,    Quantity::pressure,
                                   Quantity::pressure,      Quantity::pressure,    Quantity::speed,
                                   Quantity::speed,         Quantity::speed,       Quantity::angular_rate,
                                   Quantity::angular_rate};
    for (unsigned i = 0; i < 13; ++i) {
        CHECK(QuantityOf(static_cast<Unit>(i)) == quantities[i]);
        auto parsed = ParseUnit(UnitName(static_cast<Unit>(i)));
        REQUIRE(parsed.Ok());
        CHECK(*parsed.Get() == static_cast<Unit>(i));
    }
}

TEST_CASE("review 9 conversion errors identify the input value and unit") {
    const auto to_si = ToSi(1e308, Unit::bar);
    const auto from_si = FromSi(1e308, Unit::revolutions_per_minute);
    const auto delta = DeltaToSi(1e308, Unit::bar);
    for (const auto &result : {to_si, from_si, delta}) {
        CHECK(result.GetStatus().code == ErrorCode::non_finite);
        CHECK(std::strstr(result.GetStatus().message, "1e+308") != nullptr);
    }
    CHECK(std::strstr(to_si.GetStatus().message, "bar") != nullptr);
    CHECK(std::strstr(from_si.GetStatus().message, "rpm") != nullptr);
    CHECK(std::strstr(delta.GetStatus().message, "bar") != nullptr);
}
