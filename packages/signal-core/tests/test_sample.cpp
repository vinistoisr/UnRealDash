#include "support/FakeClock.h"
#include <cmath>
#include <type_traits>
static_assert(!std::is_constructible_v<Sample, Value, Unit>);
TEST_CASE("2.1 default sample is unavailable") {
    Sample sample;
    CHECK(sample.quality == Quality::unavailable);
    CHECK(std::isnan(sample.value));
}
TEST_CASE("2.1 zero is never a missing-data stand-in") {
    Sample sample(0, Unit::kelvin, 5ms);
    Sample missing;
    CHECK(sample.value == 0);
    CHECK(sample.quality == Quality::valid);
    CHECK(missing.quality != sample.quality);
    CHECK(std::isnan(missing.value));
}
TEST_CASE("2.1 quality and age evidence are independent") {
    for (auto quality : {Quality::valid, Quality::stale})
        for (auto age : {AgeEvidence::measured, AgeEvidence::unknown}) {
            Sample sample(1, Unit::kelvin, 5ms);
            sample.quality = quality;
            sample.age_evidence = age;
            CHECK(sample.quality == quality);
            CHECK(sample.age_evidence == age);
        }
}
