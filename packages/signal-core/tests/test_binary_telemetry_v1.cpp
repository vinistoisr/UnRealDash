#include "support/FakeClock.h"
#include "support/TestPackData.h"
#include <algorithm>
#include <cmath>
#include <random>
namespace {
using namespace telemetry_test;
using Bytes = std::vector<std::byte>;
struct Emitted {
    std::uint32_t id;
    std::uint64_t offset;
    std::array<std::byte, 8> payload;
    bool operator==(const Emitted &) const = default;
};
struct Output {
    std::vector<Emitted> frames;
    FramerCounters counters;
    bool operator==(const Output &other) const {
        return frames == other.frames && counters.frames_emitted == other.counters.frames_emitted &&
               counters.resyncs == other.counters.resyncs && counters.locks == other.counters.locks &&
               counters.unknown_identifier_dropped == other.counters.unknown_identifier_dropped &&
               counters.bytes_discarded == other.counters.bytes_discarded &&
               counters.unconfirmed_candidates_discarded == other.counters.unconfirmed_candidates_discarded;
    }
};
void Capture(void *p, const FrameEvent &f) {
    Emitted row{f.frame_id, f.stream_offset, {}};
    std::copy_n(f.payload, 8, row.payload.begin());
    static_cast<Output *>(p)->frames.push_back(row);
}
void Add(Bytes &bytes, std::uint32_t id = 0xC80) {
    const auto r = Record(id);
    bytes.insert(bytes.end(), r.begin(), r.end());
}
Output Run(const Bytes &bytes, const std::vector<std::size_t> &cuts = {}) {
    BinaryTelemetryV1Framer framer(identifiers);
    Output output;
    std::size_t start = 0;
    for (const auto end : cuts) {
        framer.Feed(std::span(bytes).subspan(start, end - start), Capture, &output);
        start = end;
    }
    framer.Feed(std::span(bytes).subspan(start), Capture, &output);
    framer.EndOfStream();
    output.counters = framer.Counters();
    return output;
}
Bytes OverlappingUnknown() {
    const auto unknown = Record(0xC95);
    Bytes bytes(unknown.begin(), unknown.begin() + 8);
    Add(bytes);
    Add(bytes, 0xC81);
    return bytes;
}
void CheckOverlappingUnknown(const Bytes &bytes, const Output &output) {
    if (bytes != OverlappingUnknown())
        return;
    REQUIRE(output.frames.size() == 2);
    CHECK(output.frames[0].offset == 8);
    CHECK(output.frames[1].offset == 24);
    CHECK(output.counters.unknown_identifier_dropped == 1);
    CHECK(output.counters.bytes_discarded == 8);
}
std::vector<Bytes> Adversaries() {
    std::vector<Bytes> streams;
    for (unsigned mode = 0; mode < 4; ++mode) {
        Bytes b{std::byte{0x77}, std::byte{0x44}};
        Add(b);
        Add(b, 0xC81);
        b[18] = std::byte{0x55};
        Add(b, 0xC83);
        Add(b, 0xC84);
        if (mode == 1) {
            const auto r = Record();
            std::copy_n(r.begin(), 8, b.begin() + 10);
        }
        if (mode == 2) {
            Bytes prefix;
            Add(prefix);
            prefix.push_back(std::byte{0});
            prefix.insert(prefix.end(), b.begin(), b.end());
            b = prefix;
        }
        if (mode == 3) {
            Bytes unknown;
            Add(unknown, 0xC95);
            unknown.insert(unknown.end(), b.begin(), b.end());
            b = unknown;
        }
        streams.push_back(b);
    }
    streams.push_back(OverlappingUnknown());
    return streams;
}
struct DecodeCase {
    FakeClock clock;
    BinaryTelemetryV1Decoder decoder{pack, clock.Get()};
    std::vector<Sample> samples;
    void Decode(std::uint32_t id = 0xC80, std::array<std::byte, 8> payload = {}) {
        decoder.OnFrame(
            {id, 0, payload.data(), 8},
            [](void *p, const Sample &s) { static_cast<std::vector<Sample> *>(p)->push_back(s); }, &samples);
    }
};
} // namespace
TEST_CASE("2.9 fresh parser starts in resync") {
    BinaryTelemetryV1Framer p(identifiers);
    CHECK(p.State() == FramerState::resync);
}
TEST_CASE("2.9 garbage prefix carrying tag and enumerated identifier is never emitted") {
    Bytes b;
    Add(b);
    b.push_back(std::byte{0});
    Add(b, 0xC81);
    Add(b, 0xC83);
    const auto r = Run(b);
    REQUIRE(r.frames.size() == 2);
    CHECK(r.frames[0].offset == 17);
    CHECK(r.counters.bytes_discarded == 17);
}
TEST_CASE("2.9 first emitted frame is the first lookahead-confirmed candidate") {
    Bytes b;
    Add(b);
    const auto r = Run(b);
    CHECK(r.frames.empty());
    Add(b, 0xC81);
    CHECK(Run(b).frames[0].offset == 0);
}
TEST_CASE("2.9 reset returns the parser to the initial resync behaviour") {
    BinaryTelemetryV1Framer p(identifiers);
    Output out;
    Bytes b;
    Add(b);
    Add(b);
    p.Feed(b, Capture, &out);
    REQUIRE(p.State() == FramerState::synced);
    p.Reset();
    CHECK(p.State() == FramerState::resync);
    CHECK(p.Counters().frames_emitted == 2);
    out.frames.clear();
    p.Feed(std::span(b).first(16), Capture, &out);
    p.EndOfStream();
    CHECK(out.frames.empty());
    CHECK(p.Counters().unconfirmed_candidates_discarded == 1);
}
TEST_CASE("2.9 record split across two reads") {
    Bytes b;
    Add(b);
    Add(b);
    CHECK(Run(b, {11}) == Run(b));
    CHECK(Run(b, {11}).frames.size() == 2);
}
TEST_CASE("2.9 two whole records coalesced in one read") {
    Bytes b;
    Add(b);
    Add(b);
    CHECK(Run(b).frames.size() == 2);
}
TEST_CASE("2.9 corrupted tag followed by a valid record") {
    Bytes b;
    Add(b);
    Add(b);
    Add(b);
    b[32] = std::byte{0};
    Add(b);
    Add(b);
    const auto r = Run(b);
    CHECK(r.frames.size() == 4);
    CHECK(r.counters.resyncs == 1);
    CHECK(r.counters.locks == 2);
    CHECK(r.counters.bytes_discarded == 16);
}
TEST_CASE("2.9 payload containing the tag bytes is not a frame start") {
    Bytes b;
    Add(b);
    Add(b);
    const auto r = Record();
    std::copy_n(r.begin(), 4, b.begin() + 8);
    CHECK(Run(b).frames.size() == 2);
    CHECK(Run(b).counters.bytes_discarded == 0);
}
TEST_CASE("2.9 payload carrying tag plus an enumerated identifier is bounded and counted") {
    Bytes b(8, std::byte{0});
    const auto r = Record();
    b.insert(b.end(), r.begin(), r.begin() + 8);
    b.insert(b.end(), 8, std::byte{0});
    b.insert(b.end(), r.begin(), r.begin() + 8);
    b.insert(b.end(), 8, std::byte{0});
    const auto out = Run(b);
    REQUIRE(out.frames.size() == 2);
    CHECK(out.frames[0].offset == 8);
    CHECK(out.counters.frames_emitted == 2);
    CHECK(out.counters.bytes_discarded == 8);
    CHECK(out.counters.resyncs == 0);
}
TEST_CASE("2.9 identifier absent from the pack enumeration is dropped with a counter") {
    DecodeCase d;
    d.Decode(0xC95);
    d.Decode(0xC7F);
    CHECK(d.samples.empty());
    CHECK(d.decoder.Counters().unknown_identifier_dropped == 2);
    Bytes b;
    Add(b, 0xC95);
    Add(b, 0xC7F);
    CHECK(Run(b).frames.empty());
    CHECK(Run(b).counters.bytes_discarded == 32);
    CHECK(Run(b).counters.unknown_identifier_dropped == 2);
    b.push_back(std::byte{0x77});
    CHECK(Run(b).counters.bytes_discarded == 33);
    for (std::size_t i = 0; i <= b.size(); ++i)
        CHECK(Run(b, {i}) == Run(b));
    const std::array<std::uint32_t, 2> sparse{0xC80, 0xC82};
    BinaryTelemetryV1Framer p(sparse);
    Output o;
    const auto record = Record(0xC81);
    p.Feed(record, Capture, &o);
    p.EndOfStream();
    CHECK(o.frames.empty());
    CHECK(p.Counters().bytes_discarded == 16);
    CHECK(p.Counters().unknown_identifier_dropped == 1);
}
TEST_CASE("2.9 unconfirmed candidate at end of stream is discarded and counted") {
    Bytes b;
    Add(b);
    const auto r = Run(b);
    CHECK(r.frames.empty());
    CHECK(r.counters.unconfirmed_candidates_discarded == 1);
    CHECK(r.counters.bytes_discarded == 16);
}
TEST_CASE("2.9 every single split yields the same frames and counters") {
    for (const auto &b : Adversaries()) {
        const auto expected = Run(b);
        CheckOverlappingUnknown(b, expected);
        for (std::size_t i = 0; i <= b.size(); ++i)
            CHECK(Run(b, {i}) == expected);
    }
}
TEST_CASE("2.9 every pair of splits yields the same frames and counters") {
    for (const auto &b : Adversaries()) {
        const auto expected = Run(b);
        CheckOverlappingUnknown(b, expected);
        for (std::size_t i = 0; i <= b.size(); ++i)
            for (std::size_t j = i; j <= b.size(); ++j)
                CHECK(Run(b, {i, j}) == expected);
    }
}
TEST_CASE("2.9 ten thousand seeded random multi-splits yield the same frames and counters") {
    std::mt19937 random(1234);
    for (const auto &b : Adversaries()) {
        const auto expected = Run(b);
        CheckOverlappingUnknown(b, expected);
        for (unsigned n = 0; n < 10000; ++n) {
            std::vector<std::size_t> cuts;
            for (std::size_t i = 1; i < b.size(); ++i)
                if (random() % 3 == 0)
                    cuts.push_back(i);
            CHECK(Run(b, cuts) == expected);
        }
    }
}
TEST_CASE("2.9 exhaustive partitions of a sixteen byte input yield the same frames and counters") {
    Bytes b;
    Add(b);
    const auto expected = Run(b);
    for (unsigned mask = 0; mask < (1u << 15); ++mask) {
        std::vector<std::size_t> cuts;
        for (unsigned bit = 0; bit < 15; ++bit)
            if (mask & (1u << bit))
                cuts.push_back(bit + 1);
        CHECK(Run(b, cuts) == expected);
    }
}
TEST_CASE("2.9 offset origin is payload byte zero") {
    DecodeCase d;
    d.Decode(0xC80, {std::byte{42}});
    CHECK(d.samples[0].value == 42);
    CHECK(d.samples[1].value == 0);
}
TEST_CASE("2.9 negative temperature via the frame signed default") {
    DecodeCase d;
    d.Decode(0xC81, {std::byte{0xF6}, std::byte{0xFF}});
    CHECK(d.samples[0].value == -10);
}
TEST_CASE("2.9 unsigned override inside an otherwise signed frame") {
    DecodeCase d;
    d.Decode(0xC81, {std::byte{0}, std::byte{0}, std::byte{0xFF}, std::byte{0xFF}});
    CHECK(d.samples[1].value == 65535);
}
TEST_CASE("2.9 non-zero offset decodes to the expected physical value") {
    DecodeCase d;
    d.Decode();
    CHECK(d.samples[3].value == -40);
}
TEST_CASE("2.9 scaled field decodes to the expected physical value") {
    DecodeCase d;
    std::array<std::byte, 8> p{};
    p[4] = std::byte{123};
    d.Decode(0xC80, p);
    CHECK(d.samples[2].value == doctest::Approx(12.3));
}
TEST_CASE("2.9 sentinel raw value publishes unavailable and not a converted number") {
    DecodeCase d;
    std::array<std::byte, 8> p{};
    p[6] = p[7] = std::byte{255};
    d.Decode(0xC80, p);
    CHECK(d.samples[3].quality == Quality::unavailable);
    CHECK(std::isnan(d.samples[3].value));
    CHECK(d.decoder.Counters().sentinel_rejections == 1);
}
TEST_CASE("2.9 held field publishes age evidence unknown with a refreshed receive timestamp") {
    DecodeCase d;
    d.clock.time = 5ms;
    d.Decode();
    d.clock.time = 405ms;
    d.Decode();
    CHECK(d.samples[0].age_evidence == AgeEvidence::unknown);
    CHECK(d.samples[4].age_evidence == AgeEvidence::unknown);
    CHECK(d.samples[4].t_recv == 405ms);
}
TEST_CASE("2.9 live field in the same frame publishes age evidence measured") {
    DecodeCase d;
    d.Decode();
    CHECK(d.samples[1].age_evidence == AgeEvidence::measured);
}
TEST_CASE("2.9 all twenty one enumerated identifiers decode from the fixture pack") {
    DecodeCase d;
    d.decoder.SetConnectionGeneration(7);
    for (auto id : identifiers)
        d.Decode(id);
    REQUIRE(d.samples.size() == 84);
    for (std::size_t i = 8; i < 12; ++i) {
        CHECK(d.samples[i].age_evidence == AgeEvidence::unknown);
        CHECK(d.samples[i].quality == Quality::valid);
        CHECK(d.samples[i].t_recv == d.clock.time);
        CHECK(d.samples[i].source == i);
    }
    CHECK(d.decoder.Counters().status_records == 1);
    for (const auto &s : d.samples)
        CHECK(s.generation == 7);
}
TEST_CASE("sentinel publishes exactly one initialized unavailable sample and counts it") {
    FakeClock clock;
    const std::array<std::int64_t, 1> rejected{65535};
    const FieldDefinition field{"sentinel",        7,       0, 2, false, true, 1, 0, Unit::dimensionless,
                                Acquisition::live, rejected};
    const FrameDefinition frame{0xC80, FrameRole::telemetry, 8, {&field, 1}};
    const DefinitionPack single{"single", "1", 1, {&frame, 1}};
    BinaryTelemetryV1Decoder decoder(single, clock.Get());
    REQUIRE(decoder.GetStatus().Ok());
    std::array<std::byte, 8> payload{std::byte{255}, std::byte{255}};
    std::vector<Sample> samples;
    decoder.OnFrame(
        {0xC80, 0, payload.data(), 8},
        [](void *p, const Sample &s) { static_cast<std::vector<Sample> *>(p)->push_back(s); }, &samples);
    REQUIRE(samples.size() == 1);
    CHECK(samples[0].quality == Quality::unavailable);
    CHECK(std::isnan(samples[0].value));
    CHECK(decoder.Counters().samples_published == 1);
    CHECK(decoder.Counters().sentinel_rejections == 1);
    CHECK(decoder.Counters().non_finite_results == 0);
}
TEST_CASE("decoder rejects invalid packs with the validation status and no publication") {
    for (unsigned mode = 0; mode < 3; ++mode) {
        FakeClock clock;
        FieldDefinition field{"bad", 0, 0, 2};
        std::array<FrameDefinition, 2> bad_frames{
            {{0xC80, FrameRole::telemetry, 8, {&field, 1}}, {0xC81, FrameRole::telemetry, 8, {}}}};
        if (mode == 0)
            field.scale = std::numeric_limits<double>::quiet_NaN();
        if (mode == 1)
            bad_frames[1].frame_id = bad_frames[0].frame_id;
        if (mode == 2)
            std::swap(bad_frames[0], bad_frames[1]);
        const DefinitionPack invalid{"invalid", "1", 1, bad_frames};
        BinaryTelemetryV1Decoder decoder(invalid, clock.Get());
        CHECK_FALSE(decoder.GetStatus().Ok());
        CHECK(decoder.GetStatus().code == ValidatePack(invalid).code);
        std::array<std::byte, 8> payload{};
        decoder.OnFrame(
            {0xC80, 0, payload.data(), 8}, [](void *, const Sample &) { FAIL("invalid pack published"); }, nullptr);
        CHECK(decoder.Counters().samples_published == 0);
        CHECK_FALSE(decoder.Health().AcquisitionReceiving(clock.time, 2000ms));
    }
}
TEST_CASE("finite affine scale overflow publishes invalid and counts the result") {
    FakeClock clock;
    const FieldDefinition field{"overflow", 0, 0, 2, false, true, 1e308};
    const FrameDefinition frame{0xC80, FrameRole::telemetry, 8, {&field, 1}};
    const DefinitionPack single{"single", "1", 1, {&frame, 1}};
    BinaryTelemetryV1Decoder decoder(single, clock.Get());
    REQUIRE(decoder.GetStatus().Ok());
    std::array<std::byte, 8> payload{std::byte{2}};
    std::size_t samples = 0;
    decoder.OnFrame(
        {0xC80, 0, payload.data(), 8},
        [](void *p, const Sample &s) {
            ++*static_cast<std::size_t *>(p);
            CHECK(s.quality == Quality::invalid);
            CHECK(std::isnan(s.value));
        },
        &samples);
    CHECK(samples == 1);
    CHECK(decoder.Counters().samples_published == 1);
    CHECK(decoder.Counters().non_finite_results == 1);
    CHECK(decoder.Counters().sentinel_rejections == 0);
}
TEST_CASE("reconnect glue resets lock carry and generation before frame-aligned garbage") {
    DecodeCase d;
    BinaryTelemetryV1Framer framer(identifiers);
    const auto sink = [](void *p, const FrameEvent &event) {
        auto &c = *static_cast<DecodeCase *>(p);
        c.decoder.OnFrame(
            event, [](void *q, const Sample &s) { static_cast<std::vector<Sample> *>(q)->push_back(s); }, &c.samples);
    };
    Bytes initial;
    Add(initial);
    Add(initial);
    framer.Feed(initial, sink, &d);
    REQUIRE(framer.State() == FramerState::synced);
    REQUIRE(d.samples.size() == 8);
    const auto partial = Record();
    framer.Feed(std::span(partial).first(3), sink, &d);
    REQUIRE(ReconnectBinaryTelemetryV1(framer, d.decoder).Ok());
    CHECK(framer.State() == FramerState::resync);
    CHECK(d.decoder.ConnectionGeneration() == 1);
    CHECK_FALSE(d.decoder.Health().AcquisitionReceiving(d.clock.time, 2000ms));
    d.samples.clear();
    framer.Feed(partial, sink, &d);
    CHECK(d.samples.empty());
    Bytes tail{std::byte{0x77}};
    Add(tail, 0xC81);
    Add(tail, 0xC83);
    framer.Feed(tail, sink, &d);
    REQUIRE(d.samples.size() == 8);
    CHECK(d.samples[0].source == 4);
    CHECK(d.samples[0].seq == 1);
    for (const auto &s : d.samples)
        CHECK(s.generation == 1);
    CHECK(framer.Counters().bytes_discarded == 20);
    CHECK(framer.Counters().locks == 2);
    // Lose synchronization and retain a complete candidate without its lookahead.
    Bytes pending(16, std::byte{0});
    Add(pending);
    framer.Feed(pending, sink, &d);
    REQUIRE(framer.State() == FramerState::resync);
    const auto before = framer.Counters();
    REQUIRE(ReconnectBinaryTelemetryV1(framer, d.decoder).Ok());
    CHECK(framer.Counters().unconfirmed_candidates_discarded == before.unconfirmed_candidates_discarded + 1);
    CHECK(framer.Counters().bytes_discarded == before.bytes_discarded + 16);
    CHECK(d.decoder.ConnectionGeneration() == 2);
    framer.Reset();
    CHECK(framer.Counters().unconfirmed_candidates_discarded == before.unconfirmed_candidates_discarded + 1);
    Output restarted;
    framer.Feed(initial, Capture, &restarted);
    REQUIRE(restarted.frames.size() == 2);
    CHECK(restarted.frames[0].offset == 0);
    d.decoder.SetConnectionGeneration(std::numeric_limits<std::uint32_t>::max());
    CHECK_FALSE(ReconnectBinaryTelemetryV1(framer, d.decoder).Ok());
    CHECK(framer.State() == FramerState::synced);
}

TEST_CASE("unknown identifier is counted once as soon as eight bytes arrive") {
    const auto bytes = Record(0xC95);
    for (std::size_t split = 0; split <= 8; ++split) {
        BinaryTelemetryV1Framer framer(identifiers);
        framer.Feed(std::span(bytes).first(split), nullptr, nullptr);
        framer.Feed(std::span(bytes).subspan(split, 8 - split), nullptr, nullptr);
        CHECK(framer.Counters().unknown_identifier_dropped == 1);
        CHECK(framer.Counters().bytes_discarded == 1);
        framer.Feed({}, nullptr, nullptr);
        framer.EndOfStream();
        CHECK(framer.Counters().unknown_identifier_dropped == 1);
        CHECK(framer.Counters().bytes_discarded == 8);
    }
}
