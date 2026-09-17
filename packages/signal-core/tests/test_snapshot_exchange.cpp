#include "support/FakeClock.h"
TEST_CASE("2.2 publication version increments by one") {
    Fixture f;
    for (std::uint64_t i = 1; i <= 100; ++i) {
        f.Publish();
        CHECK(f.exchange.Acquire().version == i);
    }
}
TEST_CASE("2.2 reader keeps its buffer when no newer version is ready") {
    Fixture f;
    f.Apply();
    f.Publish();
    auto first = f.exchange.Acquire();
    auto second = f.exchange.Acquire();
    CHECK_FALSE(second.new_publication);
    CHECK(first.samples.data() == second.samples.data());
    CHECK(first.version == second.version);
}
TEST_CASE("2.2 reader never receives a buffer it deposited") {
    Fixture f;
    auto old = f.exchange.Acquire();
    f.Publish();
    auto fresh = f.exchange.Acquire();
    CHECK(fresh.samples.data() != old.samples.data());
    for (int i = 0; i < 10; ++i)
        CHECK(f.exchange.Acquire().samples.data() == fresh.samples.data());
}
TEST_CASE("2.2 unconsumed publication is overwritten") {
    Fixture f;
    for (int i = 0; i < 10; ++i) {
        f.Apply(300 + i, Time(5000000 + i), static_cast<std::uint64_t>(i + 1));
        f.Publish();
    }
    auto latest = f.exchange.Acquire();
    CHECK(latest.version == 10);
    CHECK(latest.samples[0].sample.value == 309);
}
