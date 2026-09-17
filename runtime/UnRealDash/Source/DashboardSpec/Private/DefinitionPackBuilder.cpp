#include "dashboard_spec/DefinitionPackBuilder.h"
#include "Internal.h"
#include <rapidjson/pointer.h>
namespace dashboard_spec {
struct DefinitionPackBuilder::Storage {
    signal_core::DefinitionPack pack{};
    std::string id, version;
    std::vector<std::string> names;
    std::vector<std::vector<std::int64_t>> sentinels;
    std::vector<signal_core::FieldDefinition> fields;
    std::vector<signal_core::FrameDefinition> frames;
};
DefinitionPackBuilder::DefinitionPackBuilder() : storage_(new Storage) {}
DefinitionPackBuilder::~DefinitionPackBuilder() { delete storage_; }
const signal_core::DefinitionPack &DefinitionPackBuilder::Pack() const { return storage_->pack; }
Error DefinitionPackBuilder::Build(std::string_view json, const Validator &validator) {
    *storage_ = {};
    Document document;
    auto error = BoundedParse(json, document);
    if (!error.Ok())
        return error;
    error = validator.Validate(document);
    if (!error.Ok()) {
        if (error.pointer.View().ends_with("/unit")) {
            const auto *value = rapidjson::Pointer(error.pointer.c_str()).Get(document.Data().json);
            if (value && value->IsString() && !signal_core::ParseUnit(Text(*value)).Ok())
                return Fail(ErrorCode::E_SCHEMA, error.pointer.View(), Text(*value));
        }
        return error;
    }
    const auto &root = document.Data().json;
    if (!root.HasMember("frames"))
        return Fail(ErrorCode::E_SCHEMA, "", "expected definition pack");
    auto &s = *storage_;
    s.id = Text(root["id"]);
    s.version = Text(root["version"]);
    std::vector<std::pair<const Value *, std::size_t>> ordered;
    std::size_t total = 0;
    for (rapidjson::SizeType i = 0; i < root["frames"].Size(); ++i) {
        const auto &frame = root["frames"][i];
        ordered.emplace_back(&frame, i);
        total += frame["signals"].Size();
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const auto &a, const auto &b) { return (*a.first)["id"].GetUint() < (*b.first)["id"].GetUint(); });
    s.names.reserve(total);
    s.sentinels.reserve(total);
    s.fields.reserve(total);
    s.frames.reserve(ordered.size());
    for (const auto &[ptr, original] : ordered) {
        const auto &frame = *ptr;
        const auto base = "/frames/" + std::to_string(original);
        if (frame["length"].GetUint() > 255)
            return Fail(ErrorCode::E_SCHEMA, base + "/length", "payload exceeds struct representation");
        const auto start = s.fields.size();
        for (rapidjson::SizeType i = 0; i < frame["signals"].Size(); ++i) {
            const auto &value = frame["signals"][i];
            const auto path = base + "/signals/" + std::to_string(i);
            signal_core::FieldDefinition field;
            const auto type = Text(value["type"]);
            field.width_bytes = type == "uint8" ? 1 : type == "uint16" ? 2 : 4;
            if (!value["byte_offset"].IsUint() || value["byte_offset"].GetUint() > 255 ||
                value["byte_offset"].GetUint() + field.width_bytes > frame["length"].GetUint())
                return Fail(ErrorCode::E_SCHEMA, path + "/byte_offset", "field exceeds payload length");
            field.byte_offset = static_cast<std::uint8_t>(value["byte_offset"].GetUint());
            field.is_signed = value["signed"].GetBool();
            field.little_endian = Text(value["byte_order"]) == "little";
            field.scale = value["scale"].GetDouble();
            field.offset = value["offset"].GetDouble();
            const auto unit = signal_core::ParseUnit(Text(value["unit"]));
            if (!unit.Ok())
                return Fail(ErrorCode::E_SCHEMA, path + "/unit", Text(value["unit"]));
            field.unit = *unit.Get();
            field.acquisition =
                Text(value["acquisition"]) == "held" ? signal_core::Acquisition::held : signal_core::Acquisition::live;
            field.signal = static_cast<std::uint32_t>(s.fields.size());
            s.names.push_back(Text(value["name"]));
            field.name = s.names.back().c_str();
            s.sentinels.emplace_back();
            if (value.HasMember("sentinels"))
                for (const auto &sentinel : value["sentinels"].GetArray()) {
                    if (!sentinel.IsInt64())
                        return Fail(ErrorCode::E_SCHEMA, path + "/sentinels", "raw sentinel exceeds int64");
                    s.sentinels.back().push_back(sentinel.GetInt64());
                }
            field.sentinels = s.sentinels.back();
            s.fields.push_back(field);
        }
        s.frames.push_back(
            {frame["id"].GetUint(),
             Text(frame["role"]) == "status" ? signal_core::FrameRole::status : signal_core::FrameRole::telemetry,
             static_cast<std::uint8_t>(frame["length"].GetUint()),
             std::span<const signal_core::FieldDefinition>(s.fields).subspan(start)});
    }
    signal_core::DefinitionPack pack{s.id.c_str(), s.version.c_str(), root["api"].GetUint(), s.frames};
    const auto status = signal_core::ValidatePack(pack);
    if (!status.Ok())
        return Fail(ErrorCode::E_SCHEMA, "/frames", status.message);
    s.pack = pack;
    return {};
}
} // namespace dashboard_spec
