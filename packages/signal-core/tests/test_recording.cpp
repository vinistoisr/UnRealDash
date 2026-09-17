#include "SignalCore/Scenarios.h"
#include "support/FakeClock.h"
#include "support/TempPath.h"
#include <cstring>
#include <fstream>
#include <iterator>
TEST_CASE("2.7 sixty second round trip is byte identical") {
    std::string original;
    REQUIRE(GenerateScenario({Scenario::idle, 1234, 60s, 50ms, 500ms}, Sink(original)).Ok());
    std::array<Signal, 2> signals{};
    std::vector<SignalSample> samples(2400);
    auto read = ReadRecording(original, signals, samples);
    REQUIRE(read.Ok());
    CHECK(read.Get()->samples.size() == 2400);
    std::string rewritten;
    REQUIRE(WriteHeader(Sink(rewritten), read.Get()->signals).Ok());
    for (const auto &sample : read.Get()->samples)
        REQUIRE(WriteSample(Sink(rewritten), sample).Ok());
    CHECK(original == rewritten);
    CHECK(original.find('\r') == std::string::npos);
    CHECK(samples[1].sample.age_evidence == AgeEvidence::unknown);
    TempPath path("unrealdash-recording-roundtrip.jsonl");
    {
        std::ofstream output(path.path, std::ios::binary);
        REQUIRE(output.good());
        output.write(original.data(), static_cast<std::streamsize>(original.size()));
        REQUIRE(output.good());
    }
    std::ifstream input(path.path, std::ios::binary);
    std::string disk((std::istreambuf_iterator<char>(input)), {});
    CHECK(disk == original);
    Fixture f;
    f.signals = signals;
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start(*read.Get(), false, 50ms).Ok());
    std::string replayed;
    REQUIRE(WriteHeader(Sink(replayed), signals).Ok());
    for (std::size_t i = 0; i < samples.size(); i += 2) {
        f.clock.time = samples[i].sample.t_recv;
        REQUIRE(replay.Poll().Ok());
        for (const auto &entry : f.registry.Samples())
            REQUIRE(WriteSample(Sink(replayed), entry).Ok());
    }
    CHECK(replayed == original);
}
TEST_CASE("2.7 three second gap produces stale at the deadline and valid on the next sample") {
    Fixture f;
    std::array<SignalSample, 2> samples{{{1, f.Make(300, 10005ms, 1)}, {1, f.Make(310, 13005ms, 2)}}};
    f.clock.time = 5ms;
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start({f.signals, samples}, false, 50ms).Ok());
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Storage()[0].sample.t_recv == 5ms);
    CHECK(f.schedule.Earliest()->time == 505ms);
    f.clock.time = 505ms - 1ns;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Storage()[0].sample.quality == Quality::valid);
    f.clock.time = 505ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Storage()[0].sample.quality == Quality::stale);
    REQUIRE(replay.NextTime().Ok());
    CHECK(*replay.NextTime().Get() == 3005ms);
    f.clock.time = 3005ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Storage()[0].sample.quality == Quality::valid);
    CHECK(f.Storage()[0].sample.value == 310);
}
TEST_CASE("2.7 end of file holds then goes stale at the deadline") {
    Fixture f;
    SignalSample sample{1, f.Make()};
    f.clock.time = 5ms;
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start({f.signals, {&sample, 1}}, false, 50ms).Ok());
    REQUIRE(replay.Poll().Ok());
    CHECK(replay.Ended());
    f.clock.time = 504ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Storage()[0].sample.quality == Quality::valid);
    f.clock.time = 505ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Storage()[0].sample.quality == Quality::stale);
    CHECK(f.Storage()[0].sample.value == 300);
    CHECK(f.registry.Generation() == 0);
}
TEST_CASE("2.7 loop flag resets hysteresis debounce and hold_last at the wrap") {
    RuleFixture f;
    f.definition.hysteresis_band = 2;
    f.definition.debounce = 100ms;
    f.definition.missing_input_policy = MissingInputPolicy::hold_last;
    f.Load();
    SignalSample sample{1, f.Make(400)};
    f.clock.time = 5ms;
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start({f.signals, {&sample, 1}}, true, 1000ms).Ok());
    REQUIRE(replay.Poll().Ok());
    CHECK_FALSE(f.Evaluate().current);
    f.clock.time = 105ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Evaluate().current);
    f.clock.time = 505ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Evaluate().current);
    REQUIRE(f.schedule.Earliest());
    CHECK(f.schedule.Earliest()->kind == ExpiryKind::hold_last);
    f.clock.time = 1005ms;
    REQUIRE(replay.Poll().Ok());
    auto result = f.Evaluate();
    CHECK(f.registry.Generation() == 1);
    CHECK_FALSE(result.current);
    CHECK_FALSE(result.latched);
    CHECK(f.schedule.Earliest()->kind == ExpiryKind::debounce);
    CHECK(f.schedule.Earliest()->time == 1105ms);
}
TEST_CASE("2.7 malformed line is rejected with line and byte offset") {
    Fixture f;
    std::string header;
    REQUIRE(WriteHeader(Sink(header), f.signals).Ok());
    std::array<Signal, 2> signals;
    std::array<SignalSample, 2> samples;
    for (const auto *malformed : {"{}\n", "{\"t_recv_ns\":[] }\n", "[[[[0]]]]\n", "{\"x\":1,\"x\":2}\n"}) {
        auto result = ReadRecording(header + malformed, signals, samples);
        CHECK(result.GetStatus().code == ErrorCode::recording_malformed_line);
        CHECK(std::string(result.GetStatus().message).find("line 2") != std::string::npos);
        CHECK(std::string(result.GetStatus().message).find("byte offset") != std::string::npos);
    }
    auto escaped = header;
    escaped.replace(escaped.find("temperature"), 11, "\\u0041");
    CHECK(ReadRecording(escaped, signals, samples).GetStatus().code == ErrorCode::recording_malformed_line);
}
TEST_CASE("2.7 unknown format version is rejected") {
    Fixture f;
    std::string header;
    REQUIRE(WriteHeader(Sink(header), f.signals).Ok());
    header.replace(header.find("\"version\":1"), 11, "\"version\":2");
    std::array<Signal, 2> signals;
    std::array<SignalSample, 2> samples;
    CHECK(ReadRecording(header, signals, samples).GetStatus().code == ErrorCode::recording_version);
}
TEST_CASE("recording preserves optional timestamps escapes and unavailable values") {
    Fixture f;
    std::strcpy(f.signals[0].name, "temperature\\\"\n\t");
    std::string text;
    REQUIRE(WriteHeader(Sink(text), f.signals).Ok());
    auto sample = f.Make();
    sample.source = UINT64_MAX;
    sample.has_source_time = true;
    sample.t_source = -7ns;
    REQUIRE(WriteSample(Sink(text), {1, sample}).Ok());
    sample.seq = 2;
    sample.quality = Quality::unavailable;
    sample.value = std::numeric_limits<double>::quiet_NaN();
    sample.has_source_time = false;
    REQUIRE(WriteSample(Sink(text), {1, sample}).Ok());
    std::array<Signal, 2> signals;
    std::array<SignalSample, 2> samples;
    auto result = ReadRecording(text, signals, samples);
    REQUIRE(result.Ok());
    std::string again;
    REQUIRE(WriteHeader(Sink(again), result.Get()->signals).Ok());
    for (const auto &entry : result.Get()->samples)
        REQUIRE(WriteSample(Sink(again), entry).Ok());
    CHECK(again == text);
    CHECK(samples[0].sample.t_source == -7ns);
    CHECK(samples[0].sample.source == UINT64_MAX);
    CHECK_FALSE(samples[1].sample.has_source_time);
}

TEST_CASE("replay loop advances a nonzero recorded generation by one") {
    Fixture f;
    SignalSample sample{1, f.Make(300, 10005ms, 1, 10)};
    f.clock.time = 5ms;
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start({f.signals, {&sample, 1}}, true, 50ms).Ok());
    REQUIRE(replay.Poll().Ok());
    CHECK(f.registry.Generation() == 10);
    f.clock.time = 55ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.registry.Generation() == 11);
}

TEST_CASE("review 3 replay steps preserve same-timestamp warning and normal samples") {
    RuleFixture f;
    f.Load();
    f.clock.time = 5ms;
    std::array<SignalSample, 2> samples{{{1, f.Make(400, 5ms, 1)}, {1, f.Make(300, 5ms, 2)}}};
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start({f.signals, samples}, false, 50ms).Ok());
    auto first = replay.Step();
    REQUIRE(first.Ok());
    REQUIRE(*first.Get());
    CHECK(f.Storage()[0].sample.seq == 1);
    CHECK(f.Storage()[0].sample.value == 400);
    auto warning = f.Evaluate();
    CHECK(warning.current);
    CHECK(warning.latched);
    auto second = replay.Step();
    REQUIRE(second.Ok());
    REQUIRE(*second.Get());
    CHECK(f.Storage()[0].sample.seq == 2);
    CHECK(f.Storage()[0].sample.value == 300);
    auto normal = f.Evaluate();
    CHECK_FALSE(normal.current);
    CHECK(normal.latched);
    auto ended = replay.Step();
    REQUIRE(ended.Ok());
    CHECK_FALSE(*ended.Get());
}
TEST_CASE("review 6 replay wrap resets hysteresis inside its band") {
    RuleFixture f;
    f.definition.hysteresis_band = 2;
    f.Load();
    f.clock.time = 5ms;
    std::array<SignalSample, 3> samples{
        {{1, f.Make(374, 5ms, 1)}, {1, f.Make(400, 55ms, 2)}, {1, f.Make(374, 105ms, 3)}}};
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start({f.signals, samples}, true, 50ms).Ok());
    REQUIRE(replay.Poll().Ok());
    CHECK_FALSE(f.Evaluate().current);
    f.clock.time = 55ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Evaluate().current);
    f.clock.time = 105ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Evaluate().current);
    f.clock.time = 155ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.registry.Generation() == 1);
    CHECK(f.Storage()[0].sample.value == 374);
    CHECK_FALSE(f.Evaluate().current);
    CHECK_FALSE(f.Evaluate().latched);
}
TEST_CASE("review 6 replay wrap discards hold_last result and timer during missing input") {
    RuleFixture f;
    f.definition.missing_input_policy = MissingInputPolicy::hold_last;
    f.Load();
    f.clock.time = 5ms;
    auto missing = f.Make(400, 5ms, 1);
    missing.quality = Quality::unavailable;
    auto missing_again = missing;
    missing_again.t_recv = 105ms;
    missing_again.seq = 3;
    std::array<SignalSample, 3> samples{{{1, missing}, {1, f.Make(400, 55ms, 2)}, {1, missing_again}}};
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start({f.signals, samples}, true, 50ms).Ok());
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Evaluate().quality == Quality::unavailable);
    f.clock.time = 55ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Evaluate().current);
    f.clock.time = 105ms;
    REQUIRE(replay.Poll().Ok());
    CHECK(f.Evaluate().current);
    REQUIRE(f.schedule.Earliest());
    CHECK(f.schedule.Earliest()->time == 2105ms);
    f.clock.time = 155ms;
    REQUIRE(replay.Poll().Ok());
    auto result = f.Evaluate();
    CHECK(f.registry.Generation() == 1);
    CHECK(result.quality == Quality::unavailable);
    CHECK_FALSE(result.current);
    CHECK_FALSE(result.latched);
    REQUIRE(f.schedule.Earliest());
    CHECK(f.schedule.Earliest()->kind == ExpiryKind::hold_last);
    CHECK(f.schedule.Earliest()->time == 2155ms);
}
TEST_CASE("review 6 replay wrap restarts a pending debounce window") {
    RuleFixture f;
    f.definition.debounce = 100ms;
    f.Load();
    f.clock.time = 5ms;
    SignalSample sample{1, f.Make(400)};
    Replay replay(f.clock.Get(), f.registry);
    REQUIRE(replay.Start({f.signals, {&sample, 1}}, true, 75ms).Ok());
    REQUIRE(replay.Poll().Ok());
    CHECK_FALSE(f.Evaluate().current);
    REQUIRE(f.schedule.Earliest());
    CHECK(f.schedule.Earliest()->time == 105ms);
    f.clock.time = 80ms;
    REQUIRE(replay.Poll().Ok());
    CHECK_FALSE(f.Evaluate().current);
    REQUIRE(f.schedule.Earliest());
    CHECK(f.schedule.Earliest()->time == 180ms);
    f.clock.time = 105ms;
    CHECK_FALSE(f.Evaluate().current);
    CHECK_FALSE(f.Evaluate().latched);
}

TEST_CASE("replay Poll uses one entry cutoff with an advancing clock and short loop") {
    struct AdvancingClock {
        Time time{};
        unsigned reads{};
        Time Read() {
            const auto result = time;
            // Saturation makes a regressed moving-cutoff implementation fail instead of hanging the suite.
            if (++reads <= 32)
                time += 10ms;
            return result;
        }
        Clock Get() {
            return {this, [](void *context) { return static_cast<AdvancingClock *>(context)->Read(); }};
        }
    } clock;
    const Signal signal{1, "temperature", Unit::kelvin, 500ms, false};
    std::array<SignalSample, 1> a{}, b{}, c{};
    std::array<Expiry, 1> expiries{};
    ExpirySchedule schedule(expiries);
    SnapshotExchange exchange({a, {}, 0}, {b, {}, 0}, {c, {}, 0});
    SignalRegistry registry(clock.Get(), {&signal, 1}, schedule, exchange);
    REQUIRE(registry.Initialize().Ok());
    Sample value(300, Unit::kelvin, 0ns);
    value.seq = 1;
    const SignalSample sample{1, value};
    Replay replay(clock.Get(), registry);
    REQUIRE(replay.Start({{&signal, 1}, {&sample, 1}}, true, 1ms).Ok());
    CHECK(clock.reads == 1); // Start captured origin 0 ms; Poll's entry read will return 10 ms.
    REQUIRE(replay.Poll().Ok());
    CHECK(registry.Generation() == 10); // Exactly the initial sample plus ten wraps through the inclusive cutoff.
    CHECK(registry.Samples()[0].sample.t_recv == 10ms);
    CHECK(registry.Samples()[0].sample.value == 300);
    const auto next = replay.NextTime();
    REQUIRE(next.Ok());
    CHECK(*next.Get() == 11ms);
    // Start, Poll entry, eleven Apply freshness checks, and the final Expire check.
    CHECK(clock.reads == 14);
    CHECK(clock.time == 140ms);
}
