#include "SignalCore/Scenarios.h"
#include "support/FakeClock.h"
TEST_CASE("2.8 each scenario is identical across two runs with the same seed") {
    for (unsigned i = 0; i < 7; ++i) {
        std::string a, b;
        ScenarioOptions options{static_cast<Scenario>(i), 1234, 60s, 50ms, 500ms};
        REQUIRE(GenerateScenario(options, Sink(a)).Ok());
        REQUIRE(GenerateScenario(options, Sink(b)).Ok());
        CHECK(a == b);
    }
}
TEST_CASE("2.8 different seeds produce different output") {
    for (unsigned i = 0; i < 7; ++i) {
        std::string a, b;
        ScenarioOptions options{static_cast<Scenario>(i), 1234, 60s, 50ms, 500ms};
        REQUIRE(GenerateScenario(options, Sink(a)).Ok());
        ++options.seed;
        REQUIRE(GenerateScenario(options, Sink(b)).Ok());
        CHECK(a != b);
    }
}
TEST_CASE("2.8 disconnect scenario increments the generation") {
    std::string text;
    ScenarioEvent last{};
    ScenarioObserver observer{
        &last, [](void *context, const ScenarioEvent &event) { *static_cast<ScenarioEvent *>(context) = event; }};
    REQUIRE(GenerateScenario({Scenario::disconnect, 1234, 60s, 50ms, 500ms}, Sink(text), observer).Ok());
    CHECK(last.generation == 1);
    CHECK_FALSE(last.connected);
    REQUIRE(GenerateScenario({Scenario::reconnect, 1234, 60s, 50ms, 500ms}, Sink(text), observer).Ok());
    CHECK(last.generation == 2);
    CHECK(last.connected);
}
TEST_CASE("2.8 stale_heartbeat scenario leaves transport connected while signals expire") {
    std::string text;
    ScenarioEvent last{};
    REQUIRE(GenerateScenario({Scenario::stale_heartbeat, 1234, 60s, 50ms, 500ms}, Sink(text),
                             {&last, [](void *c, const ScenarioEvent &e) { *static_cast<ScenarioEvent *>(c) = e; }})
                .Ok());
    CHECK(last.connected);
    CHECK(last.heartbeat);
    CHECK(last.generation == 0);
    Fixture f;
    std::array<Signal, 2> signals;
    std::vector<SignalSample> samples(2400);
    auto recording = ReadRecording(text, signals, samples);
    REQUIRE(recording.Ok());
    f.signals = signals;
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start(*recording.Get(), false, 50ms).Ok());
    f.clock.time = 60s;
    REQUIRE(replay.Poll().Ok());
    for (const auto &entry : f.registry.Samples())
        CHECK(entry.sample.quality == Quality::stale);
}
