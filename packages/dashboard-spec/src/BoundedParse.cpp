#include "Internal.h"
#include <functional>
#include <rapidjson/error/en.h>
#include <rapidjson/reader.h>
namespace dashboard_spec {
Document::Document() : storage_(new Storage) {}
Document::~Document() { delete storage_; }
Document::Storage &Document::Data() { return *storage_; }
const Document::Storage &Document::Data() const { return *storage_; }
bool ReadBounded(const std::filesystem::path &path, std::size_t limit, std::string &bytes) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return false;
    bytes.resize(limit + 1);
    stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(stream.gcount()));
    return !stream.bad();
}
namespace {
struct Frame {
    bool object;
    std::string pointer;
    std::string key;
    std::size_t index{};
    std::set<std::string> keys;
};
class Bounds : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, Bounds> {
  public:
    Error error;
    std::vector<Frame> stack;
    std::size_t nodes{};
    std::string Pointer() const {
        if (stack.empty())
            return "";
        const auto &f = stack.back();
        return Join(f.pointer, f.object ? f.key : std::to_string(f.index));
    }
    bool Reject(ErrorCode code, std::string_view pointer, std::string_view message) {
        error = Fail(code, pointer, message);
        return false;
    }
    bool Node(bool container = false) {
        const auto pointer = Pointer();
        if (++nodes > limits::json_nodes)
            return Reject(ErrorCode::E_JSON_TOO_MANY_NODES, pointer, "JSON node limit exceeded");
        if (container && stack.size() + 1 > limits::json_depth)
            return Reject(ErrorCode::E_JSON_TOO_DEEP, pointer, "JSON container depth exceeded");
        if (!stack.empty())
            ++stack.back().index;
        return true;
    }
    bool Null() { return Node(); }
    bool Bool(bool) { return Node(); }
    bool Int(int) { return Node(); }
    bool Uint(unsigned) { return Node(); }
    bool Int64(std::int64_t) { return Node(); }
    bool Uint64(std::uint64_t) { return Node(); }
    bool Double(double) { return Node(); }
    bool String(const char *, rapidjson::SizeType length, bool) {
        auto pointer = Pointer();
        if (!Node())
            return false;
        return length <= limits::string_length ||
               Reject(ErrorCode::E_JSON_STRING_TOO_LONG, pointer, "string byte limit exceeded");
    }
    bool Key(const char *text, rapidjson::SizeType length, bool) {
        auto &frame = stack.back();
        if (length > limits::string_length)
            return Reject(ErrorCode::E_JSON_STRING_TOO_LONG, frame.pointer, "key byte limit exceeded");
        frame.key.assign(text, length);
        if (!frame.keys.insert(frame.key).second)
            return Reject(ErrorCode::E_DUPLICATE_KEY, Pointer(), frame.key);
        return true;
    }
    bool Start(bool object) {
        const auto pointer = Pointer();
        if (!Node(true))
            return false;
        stack.push_back({object, pointer, "", 0, {}});
        return true;
    }
    bool StartObject() { return Start(true); }
    bool StartArray() { return Start(false); }
    bool EndObject(rapidjson::SizeType) {
        stack.pop_back();
        return true;
    }
    bool EndArray(rapidjson::SizeType) {
        stack.pop_back();
        return true;
    }
};
} // namespace
Error BoundedParse(std::string_view input, Document &output) {
    if (input.size() > limits::document_size)
        return Fail(ErrorCode::E_DOC_TOO_LARGE, "", "document byte limit exceeded");
    rapidjson::MemoryStream stream(input.data(), input.size());
    rapidjson::Reader reader;
    Bounds bounds;
    if (!reader.Parse<rapidjson::kParseValidateEncodingFlag>(stream, bounds)) {
        if (!bounds.error.Ok())
            return bounds.error;
        return Fail(ErrorCode::E_JSON_SYNTAX, "", rapidjson::GetParseError_En(reader.GetParseErrorCode()));
    }
    if (stream.Tell() != input.size())
        return Fail(ErrorCode::E_JSON_SYNTAX, "", "trailing input after JSON value");
    output.Data().json.Parse<rapidjson::kParseValidateEncodingFlag>(input.data(), input.size());
    if (output.Data().json.HasParseError())
        return Fail(ErrorCode::E_JSON_SYNTAX, "", "DOM parse failed");
    // Schema diagnostics must not depend on the author's object member order.
    std::function<void(Value &)> sort_members = [&](Value &value) {
        if (value.IsObject()) {
            std::sort(value.MemberBegin(), value.MemberEnd(), [](const auto &a, const auto &b) {
                return std::string_view(a.name.GetString(), a.name.GetStringLength()) <
                       std::string_view(b.name.GetString(), b.name.GetStringLength());
            });
            for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
                sort_members(member->value);
        } else if (value.IsArray()) {
            for (auto &child : value.GetArray())
                sort_members(child);
        }
    };
    sort_members(output.Data().json);
    return {};
}
} // namespace dashboard_spec
