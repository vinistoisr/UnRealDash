#include "dashboard_spec/SemanticPass.h"
#include "Internal.h"
#include "dashboard_spec/Validator.h"
#include <functional>
namespace dashboard_spec {
namespace {
// Every semantic read retains its pointer and checks its required type before
// entering RapidJSON. Schema validation remains the sole public entry gate.
class Guarded {
  public:
    Guarded(const Value &value, std::string pointer, Error &error)
        : value_(&value), pointer_(std::move(pointer)), error_(&error) {}
    Guarded operator[](std::string_view key) const {
        const auto pointer = Join(pointer_, key);
        if (!value_ || !value_->IsObject()) {
            Reject(pointer_, "expected object");
            return {nullptr, pointer, error_};
        }
        if (!Has(*value_, key)) {
            Reject(pointer, "missing required member");
            return {nullptr, pointer, error_};
        }
        return Guarded(At(*value_, key), pointer, *error_);
    }
    Guarded operator[](rapidjson::SizeType index) const {
        const auto pointer = Join(pointer_, std::to_string(index));
        if (!value_ || !value_->IsArray() || index >= value_->Size()) {
            Reject(pointer, "missing array item");
            return {nullptr, pointer, error_};
        }
        return Guarded((*value_)[index], pointer, *error_);
    }
    bool HasMember(std::string_view name) const {
        if (!value_ || !value_->IsObject()) {
            Reject(pointer_, "expected object");
            return false;
        }
        return Has(*value_, name);
    }
    bool IsString() const { return value_ && value_->IsString(); }
    bool IsObject() const { return value_ && value_->IsObject(); }
    std::string String() const {
        if (!IsString()) {
            Reject(pointer_, "expected string");
            return {};
        }
        return Text(*value_);
    }
    double GetDouble() const {
        if (!value_ || !value_->IsNumber()) {
            Reject(pointer_, "expected number");
            return 0;
        }
        return value_->GetDouble();
    }
    bool GetBool() const {
        if (!value_ || !value_->IsBool()) {
            Reject(pointer_, "expected boolean");
            return false;
        }
        return value_->GetBool();
    }
    rapidjson::SizeType Size() const {
        if (!value_ || !value_->IsArray()) {
            Reject(pointer_, "expected array");
            return 0;
        }
        return value_->Size();
    }
    std::vector<Guarded> GetArray() const {
        std::vector<Guarded> result;
        for (rapidjson::SizeType i = 0; i < Size(); ++i)
            result.push_back((*this)[i]);
        return result;
    }
    std::vector<std::string> Names() const {
        if (!IsObject()) {
            Reject(pointer_, "expected object");
            return {};
        }
        return Keys(*value_);
    }

  private:
    Guarded(const Value *value, std::string pointer, Error *error)
        : value_(value), pointer_(std::move(pointer)), error_(error) {}
    void Reject(std::string_view pointer, std::string_view message) const {
        if (error_->Ok())
            *error_ = Fail(ErrorCode::E_SCHEMA, pointer, message);
    }
    const Value *value_;
    std::string pointer_;
    Error *error_;
};
std::string Text(const Guarded &value) { return value.String(); }
std::vector<std::string> Keys(const Guarded &value) { return value.Names(); }
Guarded At(const Guarded &value, std::string_view key) { return value[key]; }
bool Has(const Guarded &value, std::string_view key) { return value.HasMember(key); }
using Dimension = std::array<int, 4>;
Dimension UnitDimension(std::string_view unit) {
    if (unit == "K" || unit == "degC" || unit == "degF")
        return {1, 0, 0, 0};
    if (unit == "Pa" || unit == "kPa" || unit == "bar" || unit == "psi")
        return {0, 1, 0, 0};
    if (unit == "m/s" || unit == "km/h" || unit == "mph")
        return {0, 0, 1, 0};
    if (unit == "rad/s" || unit == "rpm")
        return {0, 0, 0, 1};
    return {};
}
struct ExpressionCheck {
    const std::map<std::string, std::string> &signals;
    std::size_t &total;
    std::size_t count{};
    Error &error;
    Dimension Check(const Guarded &node, const std::string &pointer, std::size_t depth) {
        ++count;
        ++total;
        if (depth > limits::expression_depth) {
            error = Fail(ErrorCode::E_EXPRESSION_TOO_DEEP, pointer, "expression depth exceeded");
            return {};
        }
        if (count > limits::rule_nodes || total > limits::expression_nodes) {
            error = Fail(ErrorCode::E_EXPRESSION_TOO_MANY_NODES, pointer, "expression count exceeded");
            return {};
        }
        const auto op = Text(node["op"]);
        if (!error.Ok())
            return {};
        if (op == "literal")
            return UnitDimension(Text(node["unit"]));
        if (op == "signal") {
            const auto signal = Text(node["signal"]);
            const auto i = signals.find(signal);
            if (i == signals.end()) {
                error = Fail(ErrorCode::E_UNRESOLVED_SIGNAL, pointer + "/signal", signal);
                return {};
            }
            return UnitDimension(i->second);
        }
        std::vector<Dimension> children;
        if (node.HasMember("a")) {
            children.push_back(Check(node["a"], pointer + "/a", depth + 1));
            if (!error.Ok())
                return {};
        } else
            for (rapidjson::SizeType i = 0; i < node["args"].Size(); ++i) {
                children.push_back(Check(node["args"][i], pointer + "/args/" + std::to_string(i), depth + 1));
                if (!error.Ok())
                    return {};
            }
        if (!error.Ok())
            return {};
        if (children.empty() || ((op == "multiply" || op == "divide") && children.size() != 2)) {
            error = Fail(ErrorCode::E_SCHEMA, pointer, "incorrect operand count");
            return {};
        }
        if (op == "multiply" || op == "divide") {
            Dimension result{};
            for (std::size_t i = 0; i < result.size(); ++i)
                result[i] = children[0][i] + (op == "multiply" ? children[1][i] : -children[1][i]);
            return result;
        }
        const bool logical = op == "logical_and" || op == "logical_or" || op == "logical_not";
        for (const auto &child : children)
            if (child != (logical ? Dimension{} : children[0])) {
                error = Fail(ErrorCode::E_RULE_UNIT_MISMATCH, pointer, op);
                return {};
            }
        if (logical || op == "less" || op == "less_or_equal" || op == "greater" || op == "greater_or_equal" ||
            op == "equal" || op == "not_equal")
            return {};
        return children[0];
    }
};
} // namespace
Error SemanticPass(const Document &document, const Validator &validator) { return validator.Validate(document); }
Error SemanticPassAccess::Run(const Document &document) {
    std::vector<NamedDocument> docs;
    auto error = SplitDocuments(document, docs);
    if (!error.Ok())
        return error;
    auto fail = [&](ErrorCode code, std::string_view pointer, std::string_view message) {
        return error.Ok() ? Fail(code, pointer, message) : error;
    };
    const NamedDocument *dashboard = nullptr;
    std::set<std::string> assets;
    std::map<std::string, std::string> signals;
    for (const auto &doc : docs) {
        if (doc.name == "dashboard")
            dashboard = &doc;
        if (doc.name == "signals")
            for (const auto &signal : Guarded(*doc.value, doc.pointer, error)["signals"].GetArray())
                signals[Text(signal["id"])] = Text(signal["unit"]);
        if (doc.name == "manifest") {
            for (const auto &path : Keys(Guarded(*doc.value, doc.pointer, error)["assets"])) {
                assets.insert(AssetKey(path));
                if (ForbiddenAsset(path))
                    return fail(ErrorCode::E_ASSET_REFERENCE_FORBIDDEN, Join(doc.pointer + "/assets", path), path);
            }
        }
    }
    for (const auto &doc : docs)
        if (doc.name == "definition-pack") {
            std::size_t i = 0;
            for (const auto &frame : Guarded(*doc.value, doc.pointer, error)["frames"].GetArray()) {
                std::size_t j = 0;
                for (const auto &field : frame["signals"].GetArray()) {
                    auto type = Text(field["type"]);
                    const double width = type == "uint8" ? 1 : type == "uint16" ? 2 : 4;
                    if (field["byte_offset"].GetDouble() + width > frame["length"].GetDouble())
                        return fail(ErrorCode::E_FIELD_PAST_FRAME_LENGTH,
                                    doc.pointer + "/frames/" + std::to_string(i) + "/signals/" + std::to_string(j) +
                                        "/byte_offset",
                                    Text(field["name"]));
                    ++j;
                }
                ++i;
            }
        }
    if (!dashboard)
        return error;
    const Guarded d(*dashboard->value, dashboard->pointer, error);
    const auto dp = dashboard->pointer;
    const auto cp = dp + "/components";
    const auto &components = d["components"];
    const auto keys = Keys(components);
    if (keys.size() > limits::components)
        return fail(ErrorCode::E_TOO_MANY_COMPONENTS, cp, "component count exceeded");
    std::map<std::string, std::string> parents;
    std::vector<std::string> roots;
    for (const auto &key : keys) {
        const auto &component = At(components, key);
        std::string parent;
        if (component.HasMember("parent")) {
            const auto &value = component["parent"];
            parent = Text(value);
            if (!Has(components, parent))
                return fail(ErrorCode::E_UNRESOLVED_PARENT, Join(cp, key) + "/parent", parent);
        }
        parents[key] = parent;
        if (parent.empty())
            roots.push_back(key);
    }
    for (const auto &key : keys) {
        std::set<std::string> seen;
        auto node = key;
        while (!node.empty()) {
            if (!seen.insert(node).second)
                return fail(!roots.empty() && key != parents[key] ? ErrorCode::E_ORPHAN_SUBTREE
                                                                  : ErrorCode::E_COMPONENT_CYCLE,
                            Join(cp, key) + "/parent", key);
            node = parents[node];
        }
    }
    if (roots.empty())
        return fail(ErrorCode::E_NO_ROOT, cp, "hierarchy has no root");
    if (roots.size() > 1)
        return fail(ErrorCode::E_MULTIPLE_ROOTS, cp, "hierarchy has multiple roots");
    for (const auto &key : keys) {
        const auto &c = At(components, key);
        const auto pointer = Join(cp, key);
        const auto type = Text(c["type"]);
        const auto &properties = c["properties"];
        auto policy = Text(c["aspect_policy"]);
        auto parent = parents[key];
        while (policy == "inherit" && !parent.empty()) {
            const auto &ancestor = At(components, parent);
            if (Text(ancestor["type"]) == "container")
                policy = Text(ancestor["aspect_policy"]);
            parent = parents[parent];
        }
        if ((type == "analog_dial" || (type == "bar_gauge" && properties["circular"].GetBool())) && policy == "stretch")
            return fail(ErrorCode::E_ASPECT_POLICY_FORBIDDEN, pointer + "/aspect_policy", key);
        if (type == "history_graph" &&
            properties["history_samples"].GetDouble() > static_cast<double>(limits::history_samples))
            return fail(ErrorCode::E_TOO_MANY_HISTORY_SAMPLES, pointer + "/properties/history_samples", key);
        if (type == "image") {
            const auto asset = Text(properties["asset"]);
            if (ForbiddenAsset(asset))
                return fail(ErrorCode::E_ASSET_REFERENCE_FORBIDDEN, pointer + "/properties/asset", asset);
            if (!assets.contains(AssetKey(asset)))
                return fail(ErrorCode::E_IMAGE_NOT_IN_MANIFEST, pointer + "/properties/asset", asset);
        }
        if (type == "page_switch") {
            std::size_t i = 0;
            std::set<std::string> selected;
            for (const auto &page : properties["pages"].GetArray()) {
                const auto name = Text(page);
                selected.insert(name);
                if (!Has(d["pages"], name))
                    return fail(ErrorCode::E_UNRESOLVED_PAGE, pointer + "/properties/pages/" + std::to_string(i), name);
                ++i;
            }
            const auto page = Text(properties["initial_page"]);
            if (!Has(d["pages"], page) || !selected.contains(page))
                return fail(ErrorCode::E_UNRESOLVED_PAGE, pointer + "/properties/initial_page", page);
        }
    }
    auto token = [&](const Guarded &value, const std::string &pointer, std::size_t depth) -> Error {
        if (!error.Ok())
            return error;
        if (depth > limits::json_depth)
            return fail(ErrorCode::E_JSON_TOO_DEEP, pointer, "theme reference depth exceeded");
        if (!value.IsString())
            return fail(ErrorCode::E_SCHEMA, pointer, "theme reference must be a string");
        const auto name = Text(value);
        if (!Has(d["theme"]["day"], name) || !Has(d["theme"]["night"], name))
            return fail(ErrorCode::E_UNRESOLVED_THEME_TOKEN, pointer, name);
        return {};
    };
    auto colour = [&](const Guarded &value, const std::string &pointer, std::size_t depth) -> Error {
        if (!error.Ok())
            return error;
        if (depth > limits::json_depth)
            return fail(ErrorCode::E_JSON_TOO_DEEP, pointer, "colour depth exceeded");
        if (value.IsObject()) {
            if (!Has(value, "token"))
                return fail(ErrorCode::E_SCHEMA, pointer + "/token", "colour needs a token");
            return token(At(value, "token"), pointer + "/token", depth + 1);
        }
        return {};
    };
    const std::size_t depth = dp.empty() ? 3 : 4;
    for (const auto &key : keys) {
        const auto &component = At(components, key);
        const auto pointer = Join(cp, key);
        for (const auto &state : Keys(component["missing_data"])) {
            error = token(At(component["missing_data"], state)["token"], pointer + "/missing_data/" + state + "/token",
                          depth + 3);
            if (!error.Ok())
                return error;
        }
        const auto type = Text(component["type"]);
        if ((type == "readout" || type == "shape" || type == "analog_dial" || type == "bar_gauge" ||
             type == "indicator" || type == "history_graph") &&
            Has(component["properties"], "colour")) {
            error = colour(component["properties"]["colour"], pointer + "/properties/colour", depth + 2);
            if (!error.Ok())
                return error;
        }
    }
    for (const auto &page : Keys(d["pages"])) {
        std::size_t i = 0;
        for (const auto &target : At(d["pages"], page).GetArray()) {
            if (!Has(components, Text(target)))
                return fail(ErrorCode::E_UNRESOLVED_PAGE, Join(dp + "/pages", page) + "/" + std::to_string(i),
                            Text(target));
            ++i;
        }
    }
    for (const auto &key : Keys(d["bindings"])) {
        const auto pointer = Join(dp + "/bindings", key);
        const auto &binding = At(d["bindings"], key);
        if (!Has(components, key))
            return fail(ErrorCode::E_UNRESOLVED_COMPONENT, pointer, key);
        if (!signals.contains(Text(binding["signal"])))
            return fail(ErrorCode::E_UNRESOLVED_SIGNAL, pointer + "/signal", Text(binding["signal"]));
    }
    std::size_t total = 0;
    for (const auto &key : Keys(d["rules"])) {
        const auto pointer = Join(dp + "/rules", key);
        const auto &rule = At(d["rules"], key);
        ExpressionCheck check{signals, total, 0, error};
        check.Check(rule["expression"], pointer + "/expression", 1);
        if (!check.error.Ok())
            return check.error;
        std::size_t i = 0;
        for (const auto &target : rule["targets"].GetArray()) {
            if (!Has(components, Text(target)))
                return fail(ErrorCode::E_UNRESOLVED_COMPONENT, pointer + "/targets/" + std::to_string(i), Text(target));
            ++i;
        }
    }
    return error;
}
} // namespace dashboard_spec
