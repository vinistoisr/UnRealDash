#include "SignalCore/AcquisitionEvents.h"
#include "SignalCore/MemoryConnector.h"
#include "support/FakeClock.h"
#include "support/HeapCounter.h"
#include <cstring>
#include <thread>

TEST_CASE("4.3 RuleId exposes the loaded id") {
    RuleFixture f;
    f.Load();
    CHECK(f.rule.RuleId() == 7);
}
TEST_CASE("4.3 ClearLatch preserves current and clears latched") {
    RuleFixture f;
    f.Load();
    f.Apply(400);
    f.Evaluate();
    f.rule.ClearLatch();
    CHECK(f.rule.Current().current);
    CHECK_FALSE(f.rule.Current().latched);
    CHECK(f.Evaluate().latched);
}
namespace {
struct Input final : ITransport {
    std::span<const DecodedField> fields{};
    std::size_t offset{};
    bool connected{};
    Status Connect() override {
        connected = true;
        return {};
    }
    Status Disconnect() override {
        connected = false;
        return {};
    }
    bool Connected() const override { return connected; }
    Status Read(std::span<std::uint8_t> into, std::size_t &written) override {
        const auto bytes = std::as_bytes(fields);
        written = std::min(into.size(), bytes.size() - offset);
        if (written)
            std::memcpy(into.data(), bytes.data() + offset, written);
        offset += written;
        return {};
    }
    void Feed(std::span<const DecodedField> input) {
        fields = input;
        offset = 0;
    }
};
struct Rig : RuleFixture {
    Input input;
    FieldSession session;
    FieldDecoder decoder;
    SiMapping mapping;
    RingAcquisitionEventSink<4096, 2> sink;
    std::array<SignalSample, 1> display_storage{};
    SampleQueue display{display_storage};
    std::array<std::uint8_t, 1024> read{};
    std::array<RuleResult, 1> previous{};
    AcquisitionPipeline pipeline{clock.Get(), registry, schedule, {&rule, 1}, previous, read,
                                 display,     input,    session,  decoder,    mapping,  sink};
    Rig() {
        Load();
        REQUIRE(pipeline.Start().Ok());
    }
    DecodedField Field(double value, std::uint64_t seq, std::uint64_t generation = 1) {
        return {1, Make(value, clock.time, seq, generation)};
    }
    std::uint64_t DrainTransitions() {
        RingAcquisitionEventSink<4096, 2>::Event event{};
        std::uint64_t count{};
        while (sink.PopAcquisition(event))
            ++count;
        return count;
    }
};
} // namespace
TEST_CASE("4.3 ordering evaluates a warning dropped by a full display queue") {
    Rig f;
    // Drop-newest requires a prefilled queue to make the FIRST threshold crossing drop.
    REQUIRE(f.display.Push({}));
    const std::array fields{f.Field(400, 1), f.Field(300, 2)};
    f.input.Feed(fields);
    const auto result = f.pipeline.Pump(1s);
    CHECK(result.status.Ok());
    CHECK(result.display_dropped == 2);
    CHECK(f.pipeline.RuleEvaluations() == f.pipeline.SamplesApplied());
    CHECK(result.expiries_fired == 0);
    RingAcquisitionEventSink<4096, 2>::Event event{};
    REQUIRE(f.sink.PopAcquisition(event));
    CHECK(event.current);
    CHECK(event.latched);
    REQUIRE(f.sink.PopAcquisition(event));
    CHECK_FALSE(event.current);
    CHECK(event.latched);
    SignalSample displayed{};
    REQUIRE(f.display.Pop(displayed));
    CHECK(displayed.signal == 0);
}
TEST_CASE("4.3 forced reconnect rejects older-generation decoded samples") {
    Rig f;
    struct InFlightDecoder final : IDecoder {
        FieldDecoder decoder;
        std::uint64_t old_generation{};
        unsigned resets{};
        Status Offer(const Message &message) override { return decoder.Offer(message); }
        Pending Next(DecodedField &out) override {
            const auto result = decoder.Next(out);
            if (result == Pending::produced)
                out.sample.generation = old_generation;
            return result;
        }
        void Reset() override {
            ++resets;
            decoder.Reset();
        }
    } decoder;
    AcquisitionPipeline pipeline(f.clock.Get(), f.registry, f.schedule, {&f.rule, 1}, f.previous, f.read, f.display,
                                 f.input, f.session, decoder, f.mapping, f.sink);
    REQUIRE(pipeline.Start().Ok());
    decoder.old_generation = pipeline.Health().generation;
    REQUIRE(pipeline.Reconnect().Ok());
    const std::array fields{f.Field(400, 1), f.Field(300, 2)};
    f.input.Feed(fields);
    CHECK(pipeline.Pump(1s).status.Ok());
    CHECK(pipeline.SamplesApplied() == 0);
    CHECK(f.registry.RejectedGenerations() == 2);
    CHECK_FALSE(f.registry.Samples()[0].received);
    CHECK(decoder.resets == 2);
}
TEST_CASE("4.3 acknowledge is drained on acquisition and supports ids through 63") {
    Rig f;
    const std::array fields{f.Field(400, 1), f.Field(300, 2)};
    f.input.Feed(fields);
    f.pipeline.Pump(1s);
    bool accepted = false;
    std::thread caller([&] { accepted = f.pipeline.Acknowledge(7).Ok(); });
    caller.join();
    CHECK(accepted);
    CHECK(f.rule.Current().latched);
    CHECK_FALSE(f.pipeline.Acknowledge(64).Ok());
    f.pipeline.Pump(1s);
    CHECK_FALSE(f.rule.Current().latched);
}
TEST_CASE("4.3 silence drives freshness and hold-last expiry") {
    Rig f;
    f.definition.missing_input_policy = MissingInputPolicy::hold_last;
    f.Load();
    const std::array fields{f.Field(400, 1)};
    f.input.Feed(fields);
    f.pipeline.Pump(1s);
    f.clock.time = 500ms;
    CHECK(f.pipeline.Pump(1s).expiries_fired == 1);
    f.clock.time = 2500ms;
    CHECK(f.pipeline.Pump(3s).expiries_fired == 1);
    CHECK(f.rule.Current().quality == Quality::unavailable);
    CHECK(f.pipeline.Health().last_byte_at == 0ns);
}
TEST_CASE("4.3 invalid caller storage is rejected before starting") {
    Rig f;
    AcquisitionPipeline invalid(f.clock.Get(), f.registry, f.schedule, {&f.rule, 1}, {}, f.read, f.display, f.input,
                                f.session, f.decoder, f.mapping, f.sink);
    CHECK_FALSE(invalid.Start().Ok());
    CHECK_FALSE(invalid.Pump(1s).status.Ok());
}
TEST_CASE("4.3 only successful connections may advance the pipeline generation") {
    Rig f;
    const std::array fields{f.Field(400, 1, 2)};
    f.input.Feed(fields);
    CHECK_FALSE(f.pipeline.Pump(1s).status.Ok());
    CHECK(f.registry.Generation() == 1);
    CHECK(f.pipeline.Health().generation == 1);
    CHECK(f.pipeline.SamplesApplied() == 0);
}
TEST_CASE("4.3 transport errors report bytes while idle reads preserve last-byte time") {
    Rig f;
    struct FailingTransport final : ITransport {
        bool connected{}, first{true};
        unsigned disconnects{};
        Status Connect() override {
            connected = true;
            return {};
        }
        Status Disconnect() override {
            connected = false;
            ++disconnects;
            return {};
        }
        bool Connected() const override { return connected; }
        Status Read(std::span<std::uint8_t> into, std::size_t &written) override {
            written = first ? 1 : 0;
            if (first) {
                first = false;
                into[0] = 1;
                return Error(ErrorCode::io_error, "test receive error");
            }
            return Error(ErrorCode::need_more_data, "idle");
        }
    } transport;
    AcquisitionPipeline pipeline(f.clock.Get(), f.registry, f.schedule, {&f.rule, 1}, f.previous, f.read, f.display,
                                 transport, f.session, f.decoder, f.mapping, f.sink);
    REQUIRE(pipeline.Start().Ok());
    f.clock.time = 5ms;
    const auto failed = pipeline.Pump(1s);
    CHECK(failed.status.code == ErrorCode::io_error);
    CHECK(failed.bytes_read == 1);
    CHECK(pipeline.Health().last_byte_at == 5ms);
    f.clock.time = 10ms;
    const auto idle = pipeline.Pump(1s);
    CHECK(idle.status.Ok());
    CHECK(idle.bytes_read == 0);
    CHECK(pipeline.Health().last_byte_at == 5ms);
    CHECK(pipeline.Health().bytes == 1);
    CHECK(pipeline.PublishedCount() == 2);
    CHECK(pipeline.Stop().Ok());
    CHECK(pipeline.Stop().Ok());
    CHECK(transport.disconnects == 1);
}
TEST_CASE("4.3 deadline reads stay live while event timestamps use the entry instant") {
    Rig f;
    struct Advancing {
        int reads{};
        Clock Get() {
            return {this, [](void *context) { return Time(static_cast<Advancing *>(context)->reads++ * 1000000); }};
        }
    } clock;
    std::array<std::uint8_t, sizeof(DecodedField)> scratch{};
    AcquisitionPipeline pipeline(clock.Get(), f.registry, f.schedule, {&f.rule, 1}, f.previous, scratch, f.display,
                                 f.input, f.session, f.decoder, f.mapping, f.sink);
    REQUIRE(pipeline.Start().Ok());
    const std::array fields{f.Field(400, 1, 2), f.Field(300, 2, 2), f.Field(400, 3, 2)};
    f.input.Feed(fields);
    const auto result = pipeline.Pump(2ms);
    CHECK(result.samples_applied == 2);
    CHECK(clock.reads == 4);
    CHECK(pipeline.Health().last_byte_at == 0ns);
    RingAcquisitionEventSink<4096, 2>::Event event{};
    REQUIRE(f.sink.PopAcquisition(event));
    CHECK(event.at == 0ns);
    REQUIRE(f.sink.PopAcquisition(event));
    CHECK(event.at == 0ns);
    CHECK(pipeline.Pump(2ms).samples_applied == 0);
}
TEST_CASE("4.3 mapping drops do not become display drops or rule passes") {
    Rig f;
    const auto sample = f.Field(400, 1);
    auto bad = sample;
    bad.sample.unit = static_cast<Unit>(65535);
    const std::array fields{bad, sample};
    f.input.Feed(fields);
    const auto result = f.pipeline.Pump(1s);
    CHECK(result.status.Ok());
    CHECK(result.mapping_drops == 1);
    CHECK(result.display_dropped == 0);
    CHECK(result.rule_evaluations == 1);
    CHECK(f.pipeline.MappingDrops() == 1);
    CHECK(f.pipeline.Pump(1s).mapping_drops == 0);
}
TEST_CASE("4.3 replay transport preserves gaps and feeds the pipeline through fragmented framing") {
    Rig f;
    Fixture staging;
    Replay replay(f.clock.Get(), staging.registry);
    const std::array<SignalSample, 2> recording{{{1, f.Make(400, 0ns, 1, 0)}, {1, f.Make(300, 3s, 2, 0)}}};
    std::array<SignalSample, 2> comparison{};
    ReplayTransport transport(replay, staging.registry, {staging.signals, recording}, comparison);
    std::array<std::uint8_t, 7> scratch{};
    // A separate pipeline starts generation 1 against the staging replay's generation 1.
    RuleFixture target;
    target.Load();
    AcquisitionPipeline pipeline(f.clock.Get(), target.registry, target.schedule, {&target.rule, 1}, f.previous,
                                 scratch, f.display, transport, f.session, f.decoder, f.mapping, f.sink);
    REQUIRE(pipeline.Start().Ok());
    CHECK(pipeline.Pump(1s).samples_applied == 1);
    CHECK(target.registry.Samples()[0].sample.value == 400);
    f.clock.time = 500ms;
    target.clock.time = 500ms;
    CHECK(pipeline.Pump(1s).expiries_fired == 1);
    CHECK(target.registry.Samples()[0].sample.quality == Quality::stale);
    f.clock.time = 3s;
    target.clock.time = 3s;
    CHECK(pipeline.Pump(4s).samples_applied == 1);
    CHECK(target.registry.Samples()[0].sample.value == 300);
    CHECK(pipeline.Pump(4s).samples_applied == 0);
}
TEST_CASE("4.3 event ring copies present ids and counts overflow separately per caller") {
    RingAcquisitionEventSink<1, 2> sink;
    std::array<SignalId, 2> ids{3, 9};
    sink.OnAcquire(1, 5ns, ids);
    ids[0] = 99;
    sink.OnSubmit(1, 6ns);
    sink.OnRuleTransition(7, 7ns, true, true);
    RingAcquisitionEventSink<1, 2>::Event event{};
    REQUIRE(sink.PopGame(event));
    CHECK(event.present[0] == 3);
    CHECK(event.present_count == 2);
    REQUIRE(sink.PopAcquisition(event));
    CHECK(event.rule == 7);
    CHECK(event.schema_version == 1);
    CHECK(sink.Drops() == 1);
}
TEST_CASE("4.3 event sink accepts concurrent game and acquisition producers") {
    RingAcquisitionEventSink<256, 2> sink;
    std::atomic<unsigned> ready{};
    std::thread game([&] {
        ++ready;
        while (ready.load() != 2)
            std::this_thread::yield();
        const std::array<SignalId, 1> ids{1};
        for (std::uint64_t i = 0; i < 128; ++i) {
            sink.OnAcquire(i, 1ns, ids);
            sink.OnSubmit(i, 2ns);
        }
    });
    std::thread acquisition([&] {
        ++ready;
        while (ready.load() != 2)
            std::this_thread::yield();
        for (unsigned i = 0; i < 256; ++i)
            sink.OnRuleTransition(7, Time(i), i % 2 == 0, true);
    });
    game.join();
    acquisition.join();
    unsigned game_count{}, acquisition_count{};
    RingAcquisitionEventSink<256, 2>::Event event{};
    while (sink.PopGame(event))
        ++game_count;
    while (sink.PopAcquisition(event))
        ++acquisition_count;
    CHECK(game_count == 256);
    CHECK(acquisition_count == 256);
    CHECK(sink.Drops() == 0);
}
namespace {
struct GateResult {
    bool bounded{}, statuses{}, allocations{}, live_bytes{};
    std::uint64_t transitions{}, counted{}, drops{}, sink_drops{}, stall_iterations{};
};
GateResult RunGate(bool stalled, std::uint64_t samples) {
    Rig f;
    GateResult result{true, true, true, true};
    std::atomic<unsigned> ready{}, finished{};
    std::atomic<bool> run{}, release{};
    std::atomic<std::uint64_t> published{}, consumed{};
    std::thread producer([&] {
        ++ready;
        while (!run.load(std::memory_order_acquire))
            std::this_thread::yield();
        for (std::uint64_t i = 0; i < samples; ++i) {
            f.clock.time = Time(static_cast<std::int64_t>(i) * 5000000);
            const std::array fields{f.Field(i % 2 ? 300 : 400, i + 1)};
            f.input.Feed(fields);
            result.statuses = f.pipeline.Pump(f.clock.time).status.Ok() && result.statuses;
            result.bounded = result.bounded && f.display.Depth() <= 1;
            result.transitions += f.DrainTransitions();
            if (i >= 200 && i < 600)
                ++result.stall_iterations;
            const bool frame = (i * 60 / 200) != ((i + 1) * 60 / 200);
            if ((frame && (!stalled || i < 200 || i >= 600)) || i + 1 == samples) {
                published.store(i + 1, std::memory_order_release);
                while (consumed.load(std::memory_order_acquire) != i + 1)
                    std::this_thread::yield();
            }
        }
        ++finished;
        while (!release.load(std::memory_order_acquire))
            std::this_thread::yield();
    });
    std::thread consumer([&] {
        ++ready;
        while (!run.load(std::memory_order_acquire))
            std::this_thread::yield();
        std::uint64_t prior{};
        while (prior != samples) {
            const auto next = published.load(std::memory_order_acquire);
            if (next == prior) {
                std::this_thread::yield();
                continue;
            }
            // During the scripted stall the producer issues no consumer rendezvous.
            f.exchange.Acquire();
            SignalSample sample{};
            f.display.Pop(sample);
            prior = next;
            consumed.store(next, std::memory_order_release);
        }
        ++finished;
        while (!release.load(std::memory_order_acquire))
            std::this_thread::yield();
    });
    while (ready.load(std::memory_order_acquire) != 2)
        std::this_thread::yield();
    const auto before = test_heap::Read();
    run.store(true, std::memory_order_release);
    while (finished.load(std::memory_order_acquire) != 2)
        std::this_thread::yield();
    const auto after = test_heap::Read();
    release.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    result.allocations = before.allocations == after.allocations;
    result.live_bytes = before.live_bytes == after.live_bytes;
    result.counted = f.pipeline.RuleTransitions();
    result.drops = f.pipeline.DisplayDrops();
    result.sink_drops = f.sink.Drops();
    return result;
}
} // namespace
TEST_CASE("4.3 200 Hz into 60 Hz remains bounded without allocation growth") {
    const auto result = RunGate(false, 120000); // Ten simulated minutes at 200 Hz.
    CHECK(result.statuses);
    CHECK(result.bounded);
    CHECK(result.drops > 0);
    CHECK(result.transitions == result.counted);
    CHECK(result.sink_drops == 0);
    CHECK(result.allocations);
    CHECK(result.live_bytes);
}
TEST_CASE("4.3 consumer stalled two simulated seconds preserves producer progress") {
    const auto control = RunGate(false, 1000);
    const auto stalled = RunGate(true, 1000);
    CHECK(stalled.statuses);
    CHECK(stalled.bounded);
    CHECK(stalled.transitions == stalled.counted);
    CHECK(stalled.sink_drops == 0);
    CHECK(stalled.stall_iterations == 400);
    // Scripted tolerance is zero. Completion while the reader waits proves progress.
    CHECK(stalled.stall_iterations == control.stall_iterations);
}
