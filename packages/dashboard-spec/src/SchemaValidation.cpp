#include "Internal.h"
#include "dashboard_spec/SemanticPass.h"
#include "dashboard_spec/Validator.h"
// v1.0.5's adapter tests macro presence instead of its numeric value. Load the
// exception policy first, then hide the zero-valued macro for this adapter only.
#include <valijson/exceptions.hpp>
#undef VALIJSON_USE_EXCEPTIONS
#include <valijson/adapters/rapidjson_adapter.hpp>
#define VALIJSON_USE_EXCEPTIONS 0
#include <memory>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>
namespace dashboard_spec {
namespace {
constexpr const char *names[] = {"dashboard", "definition-pack", "manifest", "showcase", "signals"};
}
Error SplitDocuments(const Document &document, std::vector<NamedDocument> &output) {
    const auto &v = document.Data().json;
    if (!v.IsObject())
        return Fail(ErrorCode::E_SCHEMA, "", "expected document object");
    const char *name = nullptr;
    if (v.HasMember("reference_viewport"))
        name = "dashboard";
    else if (v.HasMember("schema_version"))
        name = "manifest";
    else if (v.HasMember("frames"))
        name = "definition-pack";
    else if (v.HasMember("material_scalars"))
        name = "showcase";
    else if (v.HasMember("signals") && v["signals"].IsArray())
        name = "signals";
    if (name) {
        output.push_back({name, &v, ""});
        return {};
    }
    if (v.ObjectEmpty())
        return Fail(ErrorCode::E_SCHEMA, "", "empty document set");
    for (const auto &key : Keys(v)) {
        if (std::find_if(std::begin(names), std::end(names), [&](const char *n) { return key == n; }) ==
            std::end(names))
            return Fail(ErrorCode::E_SCHEMA, "", "unrecognized document set");
        output.push_back({key, &At(v, key), Join("", key)});
    }
    return {};
}
struct Validator::Storage {
    std::map<std::string, std::unique_ptr<valijson::Schema>> schemas;
    Error error;
};
Validator::Validator(std::string_view directory) : storage_(new Storage) {
    for (const char *name : names) {
        std::string bytes;
        if (!ReadBounded(std::filesystem::path(directory) / (std::string(name) + ".schema.json"), limits::document_size,
                         bytes)) {
            storage_->error = Fail(ErrorCode::E_SCHEMA, "", std::string("cannot read schema ") + name);
            return;
        }
        Document document;
        storage_->error = BoundedParse(bytes, document);
        if (!storage_->error.Ok())
            return;
        auto schema = std::make_unique<valijson::Schema>();
        valijson::SchemaParser parser(valijson::SchemaParser::kDraft7);
        valijson::adapters::RapidJsonAdapter adapter(document.Data().json);
        parser.populateSchema(adapter, *schema);
        storage_->schemas.emplace(name, std::move(schema));
    }
}
Validator::~Validator() { delete storage_; }
Error Validator::Validate(const Document &document) const {
    if (!storage_->error.Ok())
        return storage_->error;
    std::vector<NamedDocument> docs;
    auto error = SplitDocuments(document, docs);
    if (!error.Ok())
        return error;
    for (const auto &doc : docs) {
        valijson::Validator validator(valijson::Validator::kStrongTypes);
        valijson::ValidationResults results;
        valijson::adapters::RapidJsonAdapter adapter(*doc.value);
        if (validator.validate(*storage_->schemas.at(doc.name), adapter, &results))
            continue;
        std::vector<std::pair<std::string, std::string>> failures;
        valijson::ValidationResults::Error item;
        while (results.popError(item)) {
            const auto &message = item.description;
            if (message.starts_with("Failed to validate") || message.starts_with("Cannot validate item"))
                continue;
            std::string pointer = doc.pointer;
            for (const auto &part : item.context) {
                if (part.size() >= 2 && part.front() == '[' && part.back() == ']')
                    pointer = Join(pointer, part.substr(1, part.size() - 2));
            }
            if (message.starts_with("Missing required property '") ||
                message.starts_with("Object contains a property")) {
                auto start = message.starts_with("Missing") ? message.find('\'') : message.rfind(": '") + 2;
                auto end = message.rfind('\'');
                if (start != message.npos && end > start)
                    pointer = Join(pointer, message.substr(start + 1, end - start - 1));
            }
            failures.emplace_back(pointer, message);
        }
        if (failures.empty())
            return Fail(ErrorCode::E_SCHEMA, doc.pointer, "schema validation failed");
        std::sort(failures.begin(), failures.end());
        return Fail(ErrorCode::E_SCHEMA, failures.front().first, failures.front().second);
    }
    return SemanticPassAccess::Run(document);
}
Error Validator::ValidateText(std::string_view input) const {
    Document document;
    auto error = BoundedParse(input, document);
    return error.Ok() ? Validate(document) : error;
}
} // namespace dashboard_spec
