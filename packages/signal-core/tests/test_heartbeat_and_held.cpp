#include "support/FakeClock.h"
#include "support/TestPackData.h"
namespace {
struct Traffic {
    FakeClock clock;
    std::array<Signal, 84> signals{};
    std::array<SignalSample, 84> a{}, b{}, c{};
    std::array<Expiry, 256> expiries{};
    ExpirySchedule schedule{expiries};
    SnapshotExchange exchange{{a, {}, 0}, {b, {}, 0}, {c, {}, 0}};
    SignalRegistry registry{clock.Get(), signals, schedule, exchange};
    BinaryTelemetryV1Decoder decoder{telemetry_test::pack, clock.Get()};
    Traffic() {
        for (std::size_t i = 0; i < signals.size(); ++i) {
            signals[i].id = static_cast<SignalId>(i);
            signals[i].unit = telemetry_test::fields[i].unit;
            signals[i].deadline = 500ms;
        }
        REQUIRE(registry.Initialize().Ok());
        clock.time = 5ms;
        for (auto id : telemetry_test::identifiers)
            Send(id);
    }
    void Send(std::uint32_t id) {
        std::array<std::byte, 8> payload{};
        decoder.OnBytesReceived();
        decoder.OnFrame(
            {id, 0, payload.data(), 8},
            [](void *p, const Sample &sample) {
                auto &self = *static_cast<Traffic *>(p);
                REQUIRE(self.registry.Apply(static_cast<SignalId>(sample.source), sample).Ok());
            },
            this);
        registry.Expire();
    }
    void Heartbeats() {
        for (int i = 1; i <= 30; ++i) {
            clock.time = std::chrono::seconds(i);
            Send(0xC82);
        }
    }
};
} // namespace
TEST_CASE("2.10 thirty seconds of heartbeat only traffic leaves transport connected") {
    FakeClock clock;
    BinaryTelemetryV1Decoder decoder(telemetry_test::pack, clock.Get());
    std::array<std::byte, 8> payload{};
    struct Observed {
        std::size_t published{};
        Time expected{};
    } observed;
    for (int i = 1; i <= 30; ++i) {
        clock.time = std::chrono::seconds(i);
        observed.expected = clock.time;
        decoder.OnFrame(
            {0xC82, 0, payload.data(), 8},
            [](void *p, const Sample &s) {
                auto &o = *static_cast<Observed *>(p);
                ++o.published;
                CHECK(s.quality == Quality::valid);
                CHECK(s.t_recv == o.expected);
                CHECK(s.age_evidence == AgeEvidence::unknown);
            },
            &observed);
        CHECK(decoder.Health().TransportConnected(clock.time, 2000ms));
    }
    CHECK(observed.published == 120);
    CHECK(decoder.Counters().samples_published == 120);
    CHECK(decoder.Counters().status_records == 30);
    CHECK_FALSE(decoder.Health().AcquisitionReceiving(clock.time, 2000ms));
}
TEST_CASE("2.10 thirty seconds of heartbeat only traffic marks every telemetry-role signal stale") {
    Traffic t;
    t.clock.time = 504ms;
    t.registry.Expire();
    for (const auto &s : t.registry.Samples())
        CHECK(s.sample.quality == Quality::valid);
    t.clock.time = 505ms;
    t.registry.Expire();
    for (const auto &s : t.registry.Samples())
        CHECK(s.sample.quality == Quality::stale);
    t.Heartbeats();
    for (const auto &s : t.registry.Samples()) {
        const bool status = s.signal >= 8 && s.signal < 12;
        CHECK(s.sample.quality == (status ? Quality::valid : Quality::stale));
        CHECK(s.sample.t_recv == (status ? 30s : 5ms));
        if (status)
            CHECK(s.sample.age_evidence == AgeEvidence::unknown);
    }
}
TEST_CASE("2.10 thirty seconds of unchanged held frames keeps the signal valid with age evidence unknown") {
    Traffic t;
    for (int i = 1; i <= 300; ++i) {
        t.clock.time = 5ms + std::chrono::milliseconds(i * 100);
        t.Send(0xC80);
        const auto &s = t.registry.Samples()[0].sample;
        CHECK(s.quality == Quality::valid);
        CHECK(s.age_evidence == AgeEvidence::unknown);
        CHECK(s.t_recv == t.clock.time);
    }
}
TEST_CASE("2.10 a live field in the same held frame stream stays valid with age evidence measured") {
    Traffic t;
    for (int i = 1; i <= 300; ++i) {
        t.clock.time = 5ms + std::chrono::milliseconds(i * 100);
        t.Send(0xC80);
        const auto &s = t.registry.Samples()[1].sample;
        CHECK(s.quality == Quality::valid);
        CHECK(s.age_evidence == AgeEvidence::measured);
    }
}
TEST_CASE("2.10 held signals go stale at their own deadlines once the held traffic stops") {
    Traffic t;
    for (int i = 1; i <= 300; ++i) {
        t.clock.time = 5ms + std::chrono::milliseconds(i * 100);
        t.Send(0xC80);
    }
    t.clock.time += 499ms;
    t.registry.Expire();
    CHECK(t.registry.Samples()[0].sample.quality == Quality::valid);
    t.clock.time += 1ms;
    t.registry.Expire();
    CHECK(t.registry.Samples()[0].sample.quality == Quality::stale);
}
TEST_CASE("2.10 acquisition health reports receiving during telemetry traffic and not during heartbeat only traffic") {
    Traffic t;
    CHECK(t.decoder.Health().AcquisitionReceiving(t.clock.time, 2000ms));
    t.Heartbeats();
    CHECK_FALSE(t.decoder.Health().AcquisitionReceiving(t.clock.time, 2000ms));
    CHECK(t.decoder.Health().TransportConnected(t.clock.time, 2000ms));
    t.Send(0xC80);
    CHECK(t.decoder.Health().AcquisitionReceiving(t.clock.time, 2000ms));
    t.clock.time += 2001ms;
    CHECK_FALSE(t.decoder.Health().TransportConnected(t.clock.time, 2000ms));
}
TEST_CASE("telemetry health requires a first record and raw bytes do not imply acquisition") {
    FakeClock clock;
    BinaryTelemetryV1Decoder decoder(telemetry_test::pack, clock.Get());
    CHECK_FALSE(decoder.Health().TransportConnected(clock.time, 2000ms));
    CHECK_FALSE(decoder.Health().AcquisitionReceiving(clock.time, 2000ms));
    decoder.OnBytesReceived();
    CHECK(decoder.Health().TransportConnected(clock.time, 2000ms));
    CHECK_FALSE(decoder.Health().AcquisitionReceiving(clock.time, 2000ms));
    clock.time = 2000ms;
    CHECK(decoder.Health().TransportConnected(clock.time, 2000ms));
    clock.time += 1ns;
    CHECK_FALSE(decoder.Health().TransportConnected(clock.time, 2000ms));
}
