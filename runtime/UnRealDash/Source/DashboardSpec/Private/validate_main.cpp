#ifndef DASHBOARD_SPEC_UBT
#include "Internal.h"
#include "dashboard_spec/PackageReader.h"
#include "dashboard_spec/Validator.h"
#include <iostream>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
namespace ds = dashboard_spec;
namespace {
ds::Error Check(const std::filesystem::path &path, const ds::Validator &validator) {
    std::string bytes;
    if (!ds::ReadBounded(path, ds::limits::document_size, bytes))
        return ds::Fail(ds::ErrorCode::E_JSON_SYNTAX, "", path.generic_string());
    return validator.ValidateText(bytes);
}
std::string Field(std::string_view value) {
    std::string result;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : value) {
        if (c <= 32 || c == '\\') {
            result += "\\u00";
            result += hex[c >> 4];
            result += hex[c & 15];
        } else
            result += static_cast<char>(c);
    }
    return result;
}
std::string Row(const std::string &path, const ds::Error &error) {
    return Field(path) + "\t" +
           (error.Ok() ? "PASS\t\t"
                       : "FAIL\t" + std::string(ds::CodeName(error.code)) + "\t" + Field(error.pointer.View())) +
           "\n";
}
void Print(const ds::Error &error, bool json) {
    if (!json) {
        if (error.Ok())
            std::cout << "PASS\n";
        else
            std::cout << ds::CodeName(error.code) << " " << error.pointer << ": " << error.message << "\n";
        return;
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("status");
    writer.String(error.Ok() ? "PASS" : "FAIL");
    writer.Key("code");
    writer.String(ds::CodeName(error.code));
    writer.Key("pointer");
    writer.String(error.pointer.c_str(), static_cast<rapidjson::SizeType>(error.pointer.View().size()));
    writer.Key("message");
    writer.String(error.message.c_str(), static_cast<rapidjson::SizeType>(error.message.View().size()));
    writer.EndObject();
    std::cout << buffer.GetString() << "\n";
}
} // namespace
int main(int argc, char **argv) {
    std::string path, report;
    ds::Profile profile = ds::Profile::desktop;
    bool package = false, json = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--package")
            package = true;
        else if (arg == "--json")
            json = true;
        else if ((arg == "--report" || arg == "--profile") && i + 1 < argc) {
            const std::string value = argv[++i];
            if (arg == "--report")
                report = value;
            else if (value == "mobile")
                profile = ds::Profile::mobile;
            else if (value != "desktop") {
                std::cerr << "Unknown profile " << value << "\n";
                return 2;
            }
        } else if (arg.starts_with('-') || !path.empty()) {
            std::cerr << "Arguments: <path> [--package] [--report path] [--profile mobile|desktop] [--json]\n";
            return 2;
        } else
            path = arg;
    }
    if (path.empty()) {
        std::cerr << "A document, corpus or package path is required\n";
        return 2;
    }
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    if (package) {
        const auto error = ds::PackageReader(validator).Read(path, profile);
        Print(error, json);
        return error.Ok() ? 0 : 1;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec)) {
        auto error = Check(path, validator);
        Print(error, json);
        if (!report.empty()) {
            std::ofstream out(report, std::ios::binary);
            out << Row(std::filesystem::path(path).filename().generic_string(), error);
            if (!out)
                return 2;
        }
        return error.Ok() ? 0 : 1;
    }
    std::vector<std::filesystem::path> files;
    for (const char *side : {"valid", "invalid"}) {
        std::filesystem::directory_iterator it(std::filesystem::path(path) / side, ec), end;
        if (ec) {
            std::cerr << "Cannot read corpus " << path << "\n";
            return 2;
        }
        for (; it != end; it.increment(ec)) {
            if (ec)
                return 2;
            if (it->path().extension() == ".json")
                files.push_back(it->path());
        }
    }
    std::sort(files.begin(), files.end());
    std::string rows;
    std::size_t valid = 0, invalid = 0, mismatches = 0;
    for (const auto &file : files) {
        const auto relative = file.lexically_relative(path).generic_string();
        auto error = Check(file, validator);
        rows += Row(relative, error);
        bool match;
        if (file.parent_path().filename() == "valid") {
            ++valid;
            match = error.Ok();
        } else {
            ++invalid;
            auto expected = file;
            expected.replace_extension(".expected");
            std::ifstream stream(expected);
            std::string code, pointer;
            const bool read =
                static_cast<bool>(std::getline(stream, code)) && static_cast<bool>(std::getline(stream, pointer));
            if (!code.empty() && code.back() == '\r')
                code.pop_back();
            if (!pointer.empty() && pointer.back() == '\r')
                pointer.pop_back();
            match = read && !error.Ok() && code == ds::CodeName(error.code) && pointer == error.pointer.c_str();
        }
        if (!match) {
            ++mismatches;
            std::cerr << relative << ": " << ds::CodeName(error.code) << " " << error.pointer << " " << error.message
                      << "\n";
        }
    }
    if (!report.empty()) {
        std::ofstream out(report, std::ios::binary);
        out << rows;
        if (!out)
            return 2;
    }
    if (json)
        std::cout << "{\"valid\":" << valid << ",\"invalid\":" << invalid << ",\"mismatches\":" << mismatches << "}\n";
    else
        std::cout << "valid=" << valid << " invalid=" << invalid << " mismatches=" << mismatches << "\n";
    return mismatches || !valid || !invalid ? 1 : 0;
}

#endif
