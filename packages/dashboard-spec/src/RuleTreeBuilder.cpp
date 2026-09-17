#include "dashboard_spec/RuleTreeBuilder.h"
#include "Internal.h"
#include <functional>
namespace dashboard_spec {
struct RuleTreeBuilder::Storage {
    std::vector<signal_core::ExpressionNode> nodes;
    std::vector<std::string> pointers;
    signal_core::RuleDefinition rule;
};
RuleTreeBuilder::RuleTreeBuilder() : storage_(new Storage) {}
RuleTreeBuilder::~RuleTreeBuilder() { delete storage_; }
const signal_core::RuleDefinition &RuleTreeBuilder::Rule() const { return storage_->rule; }
Error RuleTreeBuilder::Build(const Document &document, std::string_view rule_name, std::uint32_t rule_id,
                             std::span<const SignalBinding> signals) {
    storage_->rule = {};
    storage_->nodes.clear();
    storage_->pointers.clear();
    std::vector<NamedDocument> docs;
    auto error = SplitDocuments(document, docs);
    if (!error.Ok())
        return error;
    const Value *rule = nullptr;
    std::string pointer;
    for (const auto &doc : docs)
        if (doc.name == "dashboard" && doc.value->IsObject() && doc.value->HasMember("rules") &&
            (*doc.value)["rules"].IsObject() && Has((*doc.value)["rules"], rule_name)) {
            rule = &At((*doc.value)["rules"], rule_name);
            pointer = Join(doc.pointer + "/rules", rule_name);
        }
    if (!rule || !rule->IsObject() || !rule->HasMember("expression") || !rule->HasMember("hysteresis") ||
        !rule->HasMember("debounce_ms") || !rule->HasMember("missing_input_policy"))
        return Fail(ErrorCode::E_SCHEMA, pointer, rule_name);
    // Build accepts validated documents. It still bounds conversion before allocating nodes.
    struct Pending {
        const Value *value;
        std::string pointer;
        std::size_t depth;
    };
    std::vector<Pending> pending{{&(*rule)["expression"], pointer + "/expression", 1}};
    constexpr const char *operations[] = {"literal", "signal",    "add",           "subtract",      "multiply",
                                          "divide",  "less",      "less_or_equal", "greater",       "greater_or_equal",
                                          "equal",   "not_equal", "logical_and",   "logical_or",    "logical_not",
                                          "minimum", "maximum",   "clamp",         "absolute_value"};
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto task = pending[i];
        const auto &value = *task.value;
        if (task.depth > limits::expression_depth)
            return Fail(ErrorCode::E_EXPRESSION_TOO_DEEP, task.pointer, rule_name);
        if (pending.size() > limits::rule_nodes)
            return Fail(ErrorCode::E_EXPRESSION_TOO_MANY_NODES, task.pointer, rule_name);
        if (!value.IsObject() || !value.HasMember("op") || !value["op"].IsString())
            return Fail(ErrorCode::E_SCHEMA, task.pointer, "expression needs an operation");
        signal_core::ExpressionNode node;
        const auto op = Text(value["op"]);
        const auto found =
            std::find_if(std::begin(operations), std::end(operations), [&](const char *name) { return op == name; });
        if (found == std::end(operations))
            return Fail(ErrorCode::E_SCHEMA, task.pointer + "/op", op);
        node.operation = static_cast<signal_core::Operation>(found - std::begin(operations));
        if (op == "literal") {
            if (!value.HasMember("unit") || !value["unit"].IsString() || !value.HasMember("value") ||
                !value["value"].IsNumber())
                return Fail(ErrorCode::E_SCHEMA, task.pointer, "literal needs value and unit");
            auto unit = signal_core::ParseUnit(Text(value["unit"]));
            if (!unit.Ok())
                return Fail(ErrorCode::E_SCHEMA, task.pointer + "/unit", Text(value["unit"]));
            node.literal_unit = *unit.Get();
            node.literal_value = value["value"].GetDouble();
        } else if (op == "signal") {
            if (!value.HasMember("signal") || !value["signal"].IsString())
                return Fail(ErrorCode::E_SCHEMA, task.pointer, "signal reference needs a name");
            const auto name = Text(value["signal"]);
            const auto signal = std::find_if(signals.begin(), signals.end(),
                                             [&](const SignalBinding &binding) { return binding.name == name; });
            if (signal == signals.end())
                return Fail(ErrorCode::E_UNRESOLVED_SIGNAL, task.pointer + "/signal", name);
            node.signal = signal->index;
        } else {
            const bool unary = op == "logical_not" || op == "absolute_value";
            if ((unary && !value.HasMember("a")) || (!unary && (!value.HasMember("args") || !value["args"].IsArray() ||
                                                                value["args"].Size() != (op == "clamp" ? 3u : 2u))))
                return Fail(ErrorCode::E_SCHEMA, task.pointer, "incorrect operand count");
            node.first_child = static_cast<std::uint16_t>(pending.size());
            if (value.HasMember("a")) {
                node.child_count = 1;
                pending.push_back({&value["a"], task.pointer + "/a", task.depth + 1});
            } else {
                node.child_count = static_cast<std::uint8_t>(value["args"].Size());
                for (rapidjson::SizeType j = 0; j < value["args"].Size(); ++j)
                    pending.push_back({&value["args"][j], task.pointer + "/args/" + std::to_string(j), task.depth + 1});
            }
        }
        storage_->nodes.push_back(node);
        storage_->pointers.push_back(task.pointer);
    }
    for (std::size_t i = 0; i < storage_->nodes.size(); ++i)
        storage_->nodes[i].source_pointer = storage_->pointers[i].c_str();
    const auto &hysteresis = (*rule)["hysteresis"];
    if (!hysteresis.IsObject() || !hysteresis.HasMember("unit") || !hysteresis["unit"].IsString() ||
        !hysteresis.HasMember("value") || !hysteresis["value"].IsNumber() ||
        !(*rule)["missing_input_policy"].IsString())
        return Fail(ErrorCode::E_SCHEMA, pointer, "invalid rule metadata");
    const auto policy = Text((*rule)["missing_input_policy"]);
    if (policy != "hold_last" && policy != "treat_unavailable" && policy != "force_warn")
        return Fail(ErrorCode::E_SCHEMA, pointer + "/missing_input_policy", policy);
    for (const char *key : {"debounce_ms", "maximum_hold_ms"}) {
        if (std::string_view(key) == "maximum_hold_ms" && policy != "hold_last")
            continue;
        if (!rule->HasMember(key) || !(*rule)[key].IsNumber() || (*rule)[key].GetDouble() < 0 ||
            (*rule)[key].GetDouble() >= static_cast<double>(std::chrono::nanoseconds::max().count()) / 1000000.0)
            return Fail(ErrorCode::E_SCHEMA, pointer + "/" + key, "duration outside nanosecond representation");
    }
    auto unit = signal_core::ParseUnit(Text(hysteresis["unit"]));
    if (!unit.Ok())
        return Fail(ErrorCode::E_SCHEMA, pointer + "/hysteresis/unit", "unknown unit");
    auto &result = storage_->rule;
    result.rule_id = rule_id;
    result.nodes = storage_->nodes;
    result.hysteresis_band = (*rule)["hysteresis"]["value"].GetDouble();
    result.hysteresis_unit = *unit.Get();
    auto duration = [&](const char *key) -> std::chrono::nanoseconds {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double, std::milli>((*rule)[key].GetDouble()));
    };
    result.debounce = duration("debounce_ms");
    result.missing_input_policy = policy == "hold_last"    ? signal_core::MissingInputPolicy::hold_last
                                  : policy == "force_warn" ? signal_core::MissingInputPolicy::force_warn
                                                           : signal_core::MissingInputPolicy::treat_unavailable;
    if (policy == "hold_last")
        result.maximum_hold = duration("maximum_hold_ms");
    return {};
}
} // namespace dashboard_spec
