#include "SignalCore/Interpolator.h"
#include "support/FakeClock.h"
#include <cmath>
TEST_CASE("2.6 rule evaluates the raw sample while the display value differs") {
    RuleFixture f;
    f.Load();
    f.Apply(370, 105ms);
    auto previous = f.Make(360, 5ms);
    Interpolator display;
    REQUIRE(display.Configure(f.signals[0], 100ms).Ok());
    CHECK(display.Display(previous, f.Storage()[0].sample, 155ms).value == 375);
    CHECK_FALSE(f.Evaluate().current);
}
TEST_CASE("2.6 interpolation stops at the window bound") {
    Fixture f;
    Interpolator display;
    REQUIRE(display.Configure(f.signals[0], 100ms).Ok());
    auto previous = f.Make(300, 5ms), latest = f.Make(310, 105ms);
    CHECK(display.Display(previous, latest, 205ms).value == 320);
    CHECK(display.Display(previous, latest, 305ms).value == 320);
}
TEST_CASE("2.6 interpolation does not cross into the stale period") {
    Fixture f;
    Interpolator display;
    REQUIRE(display.Configure(f.signals[0], 1000ms).Ok());
    auto previous = f.Make(300, 5ms), latest = f.Make(310, 105ms);
    auto at = display.Display(previous, latest, 605ms), after = display.Display(previous, latest, 705ms);
    CHECK(at.quality == Quality::stale);
    CHECK(after.value == at.value);
    CHECK(at.value == 360);
}
TEST_CASE("2.6 discrete signal configured for interpolation is a configuration error") {
    Fixture f;
    f.signals[0].discrete = true;
    Interpolator display;
    CHECK(display.Configure(f.signals[0], 100ms).code == ErrorCode::invalid_configuration);
    CHECK(display.Configure(f.signals[0], 0ns).Ok());
}

TEST_CASE("review 5 interpolation reports timestamp subtraction overflow") {
    Fixture f;
    Interpolator display;
    REQUIRE(display.Configure(f.signals[0], 100ms).Ok());
    auto previous = f.Make(300, Time::min());
    auto latest = f.Make(310, 1ns);
    const auto interval_overflow = display.Display(previous, latest, 1ns);
    CHECK(interval_overflow.quality == Quality::invalid);
    CHECK(interval_overflow.status.code == ErrorCode::invalid_configuration);
    CHECK(std::isnan(interval_overflow.value));
    previous.t_recv = 0ns;
    const auto age_overflow = display.Display(previous, latest, Time::min());
    CHECK(age_overflow.quality == Quality::invalid);
    CHECK(age_overflow.status.code == ErrorCode::invalid_configuration);
    CHECK(std::isnan(age_overflow.value));
}
