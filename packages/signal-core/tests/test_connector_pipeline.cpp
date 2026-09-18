#include "SignalCore/AcquisitionEvents.h"
#include "SignalCore/BinaryTelemetryConnector.h"
#include "SignalCore/MemoryConnector.h"
#include "support/FakeClock.h"
#include "support/TestPackData.h"
#include <cstring>
#include <vector>

// PLAN 4.9's adapter, driven by the real AcquisitionPipeline over bytes in the real wire format.
//
// This is the test that matters most in this chunk. The framer and the decoder were each covered
// before, but nothing had ever implemented ISession or IDecoder over them, and the pipeline's call
// pattern is specific: Offer must always consume, Next is drained fully between Offers, and a
// sample whose generation runs ahead of the pipeline's is rejected outright. Every one of those is
// a way for a connector to look correct in isolation and produce nothing once wired up.

namespace {
using namespace telemetry_test;

// The frame scripts/mock-relay.py emits, built here from the same three facts read out of
// BinaryTelemetryV1.cpp: a four byte tag, a little endian identifier, and an eight byte payload.
std::vector<std::uint8_t> Frame(std::uint32_t id, std::uint16_t a, std::uint16_t b, std::uint16_t c,
                                std::uint16_t d) {
    std::vector<std::uint8_t> bytes{0x44, 0x33, 0x22, 0x11};
    for (unsigned i = 0; i < 4; ++i)
        bytes.push_back(static_cast<std::uint8_t>((id >> (i * 8)) & 0xff));
    for (const auto field : {a, b, c, d}) {
        bytes.push_back(static_cast<std::uint8_t>(field & 0xff));
        bytes.push_back(static_cast<std::uint8_t>(field >> 8));
    }
    return bytes;
}

struct Rig {
    FakeClock clock;
    // Frame 3200's four fields, which the test pack binds to signal ids 0 through 3.
    std::array<Signal, 4> signals{{{0, "held", Unit::dimensionless, 500ms, false},
                                   {1, "live", Unit::dimensionless, 500ms, false},
                                   {2, "scaled", Unit::dimensionless, 500ms, false},
                                   {3, "offset", Unit::dimensionless, 500ms, false}}};
    std::array<SignalSample, 4> a{}, b{}, c{};
    // No rules in this rig, so the latch spans are empty. SnapshotExchange requires the
    // sample and latch spans to match the registry and the rule count exactly, and a
    // mismatch surfaces only at Publish, at the very end of Pump, after every sample has
    // already been applied and counted.
    std::array<std::uint8_t, 0> la{}, lb{}, lc{};
    std::array<Expiry, 16> expiries{};
    ExpirySchedule schedule{expiries};
    SnapshotExchange exchange{{a, la, 0}, {b, lb, 0}, {c, lc, 0}};
    SignalRegistry registry{clock.Get(), signals, schedule, exchange};

    std::vector<std::uint8_t> stream;
    MemoryTransport transport{{}};
    BinaryTelemetryV1Connector connector{pack, clock.Get(), identifiers};
    SiMapping mapping;
    NullAcquisitionEventSink sink;
    std::array<SignalSample, 64> display_storage{};
    SampleQueue display{display_storage};
    // The engine uses 4096; matching it here is what makes the connector's 320 frame ring bound
    // meaningful, since 4096 / 16 is 256 frames from a single full read.
    std::array<std::uint8_t, 4096> read{};
    std::array<RuleResult, 0> previous{};
    AcquisitionPipeline pipeline{clock.Get(), registry,  schedule,          {},      previous, read,
                                 display,     transport, connector.Session(), connector.Decoder(),
                                 mapping,     sink};

    explicit Rig(std::size_t frame_count) {
        REQUIRE(registry.Initialize().Ok());
        for (std::size_t i = 0; i < frame_count; ++i) {
            const auto value = static_cast<std::uint16_t>(i % 1000);
            const auto frame = Frame(0xc80, 4242, value, value, static_cast<std::uint16_t>(value + 1));
            stream.insert(stream.end(), frame.begin(), frame.end());
        }
        transport = MemoryTransport{stream};
        REQUIRE(pipeline.Start().Ok());
        // The generation is told to the connector rather than guessed by it; see the note on
        // BinaryTelemetryV1Connector for why a self-incrementing Reset cannot stay in step.
        REQUIRE(connector.SetGeneration(pipeline.Health().generation).Ok());
    }
};
} // namespace

TEST_CASE("4.9 relay bytes reach the registry through the real pipeline") {
    Rig f(3);
    f.clock.time = 10ms;
    const auto result = f.pipeline.Pump(1s);

    CHECK_MESSAGE(result.status.Ok(), result.status.message);
    CHECK(result.bytes_read == 48);
    // Four fields per frame, three frames.
    CHECK(result.samples_applied == 12);
    CHECK(f.connector.Framing().frames_emitted == 3);
    CHECK(f.connector.Decoding().samples_published == 12);
    CHECK(result.mapping_drops == 0);

    // And the values actually arrived, rather than merely being counted.
    const auto snapshot = f.exchange.Acquire();
    CHECK(snapshot.samples[1].sample.quality == Quality::valid);
    CHECK(snapshot.samples[1].sample.value == doctest::Approx(2.0));
    CHECK(snapshot.samples[2].sample.value == doctest::Approx(0.2));
}

TEST_CASE("4.9 a full read of 256 frames stays inside the ring") {
    // 4096 bytes is the engine's read buffer and 16 bytes is the frame, so this is the largest
    // number of frames one Offer can ever be handed. The ring is 320. If that arithmetic were
    // wrong, the overflow counter would catch it here rather than a signal going quiet on a device.
    Rig f(256);
    f.clock.time = 10ms;
    const auto result = f.pipeline.Pump(1s);

    CHECK_MESSAGE(result.status.Ok(), result.status.message);
    CHECK(f.connector.Framing().frames_emitted == 256);
    CHECK(result.samples_applied == 256 * 4);
    CHECK(f.connector.FrameOverflows() == 0);
    CHECK(f.connector.SampleOverflows() == 0);
    CHECK(f.connector.OversizePayloads() == 0);
}

TEST_CASE("4.9 a frame split across two reads is still decoded") {
    // The framer carries a partial frame between feeds. The adapter must not disturb that, and a
    // transport handing over eight bytes at a time is the hostile version of a real socket.
    struct Trickle final : ITransport {
        std::span<const std::uint8_t> bytes{};
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
            written = std::min<std::size_t>({into.size(), bytes.size() - offset, 8});
            if (!written)
                return Error(ErrorCode::need_more_data, "drained");
            std::memcpy(into.data(), bytes.data() + offset, written);
            offset += written;
            return {};
        }
    };

    Rig f(4);
    Trickle trickle;
    trickle.bytes = f.stream;
    AcquisitionPipeline pipeline{f.clock.Get(), f.registry,  f.schedule,          {},
                                 f.previous,    f.read,      f.display,           trickle,
                                 f.connector.Session(),      f.connector.Decoder(), f.mapping, f.sink};
    REQUIRE(pipeline.Start().Ok());
    REQUIRE(f.connector.SetGeneration(pipeline.Health().generation).Ok());

    std::uint32_t applied = 0;
    for (int i = 0; i < 40; ++i) {
        f.clock.time = std::chrono::milliseconds(10 + i);
        applied += pipeline.Pump(1s).samples_applied;
    }
    CHECK(applied == 16);
    CHECK(f.connector.Framing().frames_emitted == 4);
    CHECK(f.connector.FrameOverflows() == 0);
}

TEST_CASE("4.9 the connector's generation tracks the pipeline across failed attempts") {
    // The trap this chunk's adapter was rewritten to avoid. Start and Reconnect each Reset the
    // session and the decoder BEFORE attempting a connect, so a Reset that advanced the decoder's
    // generation would run ahead of the pipeline every time an attempt failed, and the pipeline
    // would then reject the first sample that did arrive.
    Rig f(2);
    for (int i = 0; i < 5; ++i) {
        f.connector.Session().Reset();
        f.connector.Decoder().Reset();
    }
    REQUIRE(f.pipeline.Reconnect().Ok());
    REQUIRE(f.connector.SetGeneration(f.pipeline.Health().generation).Ok());

    f.clock.time = 20ms;
    const auto result = f.pipeline.Pump(1s);
    CHECK_MESSAGE(result.status.Ok(), result.status.message);
    CHECK(result.samples_applied == 8);
    CHECK(f.pipeline.Health().reconnects == 1);
}
