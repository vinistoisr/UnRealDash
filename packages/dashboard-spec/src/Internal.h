#pragma once
#include "dashboard_spec/Document.h"
#include "dashboard_spec/Limits.h"
#define RAPIDJSON_ASSERT(x) ((x) ? static_cast<void>(0) : dashboard_spec::ReportAssertion(#x, __FILE__, __LINE__))
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <rapidjson/document.h>
#include <set>
#include <string>
#include <vector>
namespace dashboard_spec {
struct Document::Storage {
    rapidjson::Document json;
};
using Value = rapidjson::Value;
inline Value::ConstMemberIterator Member(const Value &value, std::string_view name) {
    return value.FindMember(Value(rapidjson::StringRef(name.data(), static_cast<rapidjson::SizeType>(name.size()))));
}
inline bool Has(const Value &value, std::string_view name) {
    return value.IsObject() && Member(value, name) != value.MemberEnd();
}
inline const Value &At(const Value &value, std::string_view name) { return Member(value, name)->value; }
inline std::string Text(const Value &v) { return {v.GetString(), v.GetStringLength()}; }
inline std::string Join(std::string_view prefix, std::string_view part) {
    std::string result(prefix);
    result += '/';
    for (char c : part) {
        if (c == '~')
            result += "~0";
        else if (c == '/')
            result += "~1";
        else
            result += c;
    }
    return result;
}
inline std::vector<std::string> Keys(const Value &object) {
    std::vector<std::string> keys;
    for (auto i = object.MemberBegin(); i != object.MemberEnd(); ++i)
        keys.push_back(Text(i->name));
    std::sort(keys.begin(), keys.end());
    return keys;
}
struct NamedDocument {
    std::string name;
    const Value *value;
    std::string pointer;
};
Error SplitDocuments(const Document &document, std::vector<NamedDocument> &output);
inline bool ForbiddenAsset(std::string_view path) {
    if (path.starts_with('/') || path.find('\\') != path.npos || path.find(':') != path.npos)
        return true;
    std::size_t start = 0;
    while (start <= path.size()) {
        auto end = path.find('/', start);
        if (end == path.npos)
            end = path.size();
        if (path.substr(start, end - start) == "..")
            return true;
        start = end + 1;
    }
    return false;
}
inline std::string AssetKey(std::string_view path) {
    std::string result;
    for (std::size_t start = 0; start <= path.size();) {
        auto end = path.find('/', start);
        if (end == path.npos)
            end = path.size();
        auto part = path.substr(start, end - start);
        if (!part.empty() && part != ".") {
            if (!result.empty())
                result += '/';
            result += part;
        }
        start = end + 1;
    }
    return result;
}
bool ReadBounded(const std::filesystem::path &path, std::size_t limit, std::string &bytes);
} // namespace dashboard_spec
