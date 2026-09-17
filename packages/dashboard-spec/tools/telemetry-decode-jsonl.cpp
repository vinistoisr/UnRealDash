#include "SignalCore/BinaryTelemetryV1.h"
#include "dashboard_spec/DefinitionPackBuilder.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <vector>
namespace {
using namespace signal_core;
struct Context {
    const DefinitionPack &pack;
    BinaryTelemetryV1Decoder &decoder;
    FrameEvent event{};
};
void SampleRow(void *p, const Sample &sample) {
    const auto &c = *static_cast<Context *>(p);
    const FieldDefinition *field = nullptr;
    for (const auto &frame : c.pack.frames)
        if (frame.frame_id == c.event.frame_id)
            for (const auto &f : frame.fields)
                if (f.signal == sample.source)
                    field = &f;
    if (!field)
        return;
    std::int64_t raw = 0;
    for (unsigned i = 0; i < field->width_bytes; ++i) {
        const auto at = field->little_endian ? i : field->width_bytes - 1u - i;
        raw |= std::to_integer<std::int64_t>(c.event.payload[field->byte_offset + at]) << (8 * i);
    }
    const auto bits = 8u * field->width_bytes;
    if (field->is_signed && (raw & (std::int64_t{1} << (bits - 1))))
        raw -= std::int64_t{1} << bits;
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
    w.StartObject();
    w.Key("frame_id");
    w.Uint(c.event.frame_id);
    w.Key("field");
    w.String(field->name);
    w.Key("raw");
    w.Int64(raw);
    w.Key("physical");
    if (sample.quality != Quality::valid)
        w.Null();
    else
        w.Double(sample.value);
    w.Key("unit");
    w.String(UnitName(sample.unit));
    w.Key("quality");
    w.String(sample.quality == Quality::valid     ? "valid"
             : sample.quality == Quality::invalid ? "invalid"
                                                  : "unavailable");
    w.Key("age_evidence");
    w.String(sample.age_evidence == AgeEvidence::measured ? "measured" : "unknown");
    w.Key("stream_offset");
    w.Uint64(c.event.stream_offset);
    w.EndObject();
    std::cout << buffer.GetString() << '\n';
}
void Frame(void *p, const FrameEvent &event) {
    auto &c = *static_cast<Context *>(p);
    c.event = event;
    c.decoder.OnFrame(event, SampleRow, p);
}
int Hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 3 || std::string_view(argv[1]) != "--pack") {
        std::cerr << "Arguments: --pack <path>; input is JSON Lines on stdin\n";
        return 2;
    }
    std::ifstream file(argv[2], std::ios::binary);
    if (!file)
        return 2;
    const std::string text{std::istreambuf_iterator<char>(file), {}};
    dashboard_spec::Validator validator(DASHBOARD_SCHEMA_DIR);
    dashboard_spec::DefinitionPackBuilder builder;
    const auto error = builder.Build(text, validator);
    if (!error.Ok()) {
        std::cerr << error.pointer << ": " << error.message << '\n';
        return 1;
    }
    std::vector<std::uint32_t> ids;
    for (const auto &f : builder.Pack().frames) {
        if (f.payload_length != 8)
            return 2;
        ids.push_back(f.frame_id);
    }
    const Clock clock{nullptr, [](void *) { return Time{1}; }};
    BinaryTelemetryV1Framer framer(ids);
    BinaryTelemetryV1Decoder decoder(builder.Pack(), clock);
    if (!decoder.GetStatus().Ok()) {
        std::cerr << decoder.GetStatus().message << "\n";
        return 1;
    }
    Context context{builder.Pack(), decoder};
    std::string line;
    while (std::getline(std::cin, line)) {
        rapidjson::Document row;
        row.Parse(line.data(), line.size());
        if (row.HasParseError() || !row.IsObject() || !row.HasMember("bytes") || !row["bytes"].IsString() ||
            row["bytes"].GetStringLength() != 32 || !row.HasMember("frame_id") || !row["frame_id"].IsUint())
            return 2;
        const auto *hex = row["bytes"].GetString();
        std::array<std::byte, 16> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const auto a = Hex(hex[2 * i]), b = Hex(hex[2 * i + 1]);
            if (a < 0 || b < 0)
                return 2;
            bytes[i] = static_cast<std::byte>(a * 16 + b);
        }
        std::uint32_t id = 0;
        for (unsigned i = 0; i < 4; ++i)
            id |= std::to_integer<std::uint32_t>(bytes[4 + i]) << (8 * i);
        if (id != row["frame_id"].GetUint())
            return 2;
        decoder.OnBytesReceived();
        framer.Feed(bytes, Frame, &context);
    }
    // Four tag bytes provide an explicit stream delimiter for a single-record input.
    const std::array<std::byte, 4> delimiter{std::byte{0x44}, std::byte{0x33}, std::byte{0x22}, std::byte{0x11}};
    framer.Feed(delimiter, Frame, &context);
    framer.EndOfStream();
    const auto &f = framer.Counters();
    const auto &d = decoder.Counters();
    std::cerr << "{\"frames_emitted\":" << f.frames_emitted << ",\"resyncs\":" << f.resyncs << ",\"locks\":" << f.locks
              << ",\"framer_unknown_identifier_dropped\":" << f.unknown_identifier_dropped
              << ",\"non_finite_results\":" << d.non_finite_results << ",\"bytes_discarded\":" << f.bytes_discarded
              << ",\"unconfirmed_candidates_discarded\":" << f.unconfirmed_candidates_discarded
              << ",\"samples_published\":" << d.samples_published
              << ",\"unknown_identifier_dropped\":" << d.unknown_identifier_dropped
              << ",\"status_records\":" << d.status_records << ",\"sentinel_rejections\":" << d.sentinel_rejections
              << "}\n";
    return std::cout && std::cin.eof() ? 0 : 2;
}
