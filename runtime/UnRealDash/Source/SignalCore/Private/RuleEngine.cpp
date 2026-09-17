#include "SignalCore/RuleEngine.h"
#include <algorithm>
#include <cmath>
namespace signal_core {
namespace {
enum class DimensionRule { leaf, compatible, multiply, divide, comparison, boolean };
struct OperationEntry {
    std::uint8_t arity;
    DimensionRule dimensions;
    double (*evaluate)(double, double, double);
};
constexpr OperationEntry operations[] = {
    {0, DimensionRule::leaf, nullptr},
    {0, DimensionRule::leaf, nullptr},
    {2, DimensionRule::compatible, [](double a, double b, double) { return a + b; }},
    {2, DimensionRule::compatible, [](double a, double b, double) { return a - b; }},
    {2, DimensionRule::multiply, [](double a, double b, double) { return a * b; }},
    {2, DimensionRule::divide,
     [](double a, double b, double) { return b == 0 ? std::numeric_limits<double>::quiet_NaN() : a / b; }},
    {2, DimensionRule::comparison, [](double a, double b, double) { return static_cast<double>(a < b); }},
    {2, DimensionRule::comparison, [](double a, double b, double) { return static_cast<double>(a <= b); }},
    {2, DimensionRule::comparison, [](double a, double b, double) { return static_cast<double>(a > b); }},
    {2, DimensionRule::comparison, [](double a, double b, double) { return static_cast<double>(a >= b); }},
    {2, DimensionRule::comparison, [](double a, double b, double) { return static_cast<double>(a == b); }},
    {2, DimensionRule::comparison, [](double a, double b, double) { return static_cast<double>(a != b); }},
    {2, DimensionRule::boolean, [](double a, double b, double) { return static_cast<double>(a != 0 && b != 0); }},
    {2, DimensionRule::boolean, [](double a, double b, double) { return static_cast<double>(a != 0 || b != 0); }},
    {1, DimensionRule::boolean, [](double a, double, double) { return static_cast<double>(a == 0); }},
    {2, DimensionRule::compatible, [](double a, double b, double) { return std::min(a, b); }},
    {2, DimensionRule::compatible, [](double a, double b, double) { return std::max(a, b); }},
    {3, DimensionRule::compatible,
     [](double a, double b, double c) {
         return b > c ? std::numeric_limits<double>::quiet_NaN() : std::clamp(a, b, c);
     }},
    {1, DimensionRule::compatible, [](double a, double, double) { return std::abs(a); }}};
static_assert(std::size(operations) == static_cast<unsigned>(Operation::absolute_value) + 1);
const char *Pointer(const ExpressionNode &n) { return n.source_pointer ? n.source_pointer : "/"; }
bool Ordered(Operation op) { return op >= Operation::less && op <= Operation::greater_or_equal; }
} // namespace
Status RuleEngine::Validate(std::size_t index, unsigned depth, std::span<const Signal> signals) {
    if (index >= count_)
        return Error(ErrorCode::malformed_expression, "node index %zu out of bounds", index);
    const auto &n = nodes_[index];
    if (depth > 32)
        return Error(ErrorCode::expression_depth, "node %s exceeds depth 32", Pointer(n));
    if (visited_[index])
        return Error(ErrorCode::malformed_expression, "node %s is reused or cyclic", Pointer(n));
    visited_[index] = true;
    auto op = static_cast<unsigned>(n.operation);
    if (op >= std::size(operations))
        return Error(ErrorCode::unknown_operation, "unknown operation at %s", Pointer(n));
    if (n.child_count != operations[op].arity || static_cast<std::size_t>(n.first_child) + n.child_count > count_)
        return Error(ErrorCode::malformed_expression, "children at %s", Pointer(n));
    if (n.operation == Operation::literal || n.operation == Operation::signal_reference) {
        Unit unit = n.literal_unit;
        if (n.operation == Operation::signal_reference) {
            bool found = false;
            for (const auto &signal : signals)
                if (signal.id == n.signal) {
                    unit = signal.unit;
                    found = true;
                    break;
                }
            if (!found)
                return Error(ErrorCode::missing_signal, "signal %u at %s not declared", n.signal, Pointer(n));
        }
        if (!IsUnit(unit))
            return Error(ErrorCode::missing_literal_unit, "explicit unit required at %s", Pointer(n));
        units_[index] = unit;
        dimensions_[index] = {};
        auto q = static_cast<unsigned>(QuantityOf(unit));
        if (q)
            dimensions_[index].powers[q - 1] = 1;
        if (n.operation == Operation::literal) {
            auto converted = ToSi(n.literal_value, unit);
            if (!converted.Ok())
                return Error(ErrorCode::non_finite, "literal at %s", Pointer(n));
            nodes_[index].literal_value = *converted.Get();
        }
        return {};
    }
    for (unsigned c = 0; c < n.child_count; ++c) {
        auto status = Validate(n.first_child + c, depth + 1, signals);
        if (!status.Ok())
            return status;
    }
    const auto first = static_cast<std::size_t>(n.first_child);
    auto dim = dimensions_[first];
    units_[index] = units_[first];
    if (operations[op].dimensions == DimensionRule::multiply || operations[op].dimensions == DimensionRule::divide) {
        for (std::size_t j = 0; j < 4; ++j)
            dim.powers[j] +=
                (operations[op].dimensions == DimensionRule::multiply ? 1 : -1) * dimensions_[first + 1].powers[j];
    } else if (operations[op].dimensions == DimensionRule::boolean) {
        for (unsigned c = 0; c < n.child_count; ++c)
            if (!(dimensions_[first + c] == Dimension{}))
                return Error(ErrorCode::incompatible_units, "boolean operand %s at %s", UnitName(units_[first + c]),
                             Pointer(n));
        dim = {};
    } else {
        for (unsigned c = 1; c < n.child_count; ++c)
            if (!(dim == dimensions_[first + c]))
                return Error(ErrorCode::incompatible_units, "%s and %s at %s", UnitName(units_[first]),
                             UnitName(units_[first + c]), Pointer(n));
        if (operations[op].dimensions == DimensionRule::comparison)
            dim = {};
    }
    dimensions_[index] = dim;
    return {};
}
Status RuleEngine::Load(const RuleDefinition &definition, std::span<const Signal> signals) {
    if (loaded_) {
        schedule_.Cancel(ExpiryKind::hold_last, definition_.rule_id);
        schedule_.Cancel(ExpiryKind::debounce, definition_.rule_id);
    }
    loaded_ = false;
    result_ = {};
    if (definition.nodes.size() > nodes_.size())
        return Error(ErrorCode::expression_nodes, "node count exceeds 512 at %s", Pointer(definition.nodes[512]));
    if (definition.nodes.empty())
        return Error(ErrorCode::malformed_expression, "empty rule tree /");
    if (!clock_.now || definition.debounce < Time::zero() || !std::isfinite(definition.hysteresis_band) ||
        definition.hysteresis_band < 0 || static_cast<unsigned>(definition.missing_input_policy) > 2 ||
        (definition.missing_input_policy == MissingInputPolicy::hold_last && definition.maximum_hold <= Time::zero()))
        return Error(ErrorCode::invalid_configuration, "rule %u timing or policy", definition.rule_id);
    count_ = definition.nodes.size();
    std::copy(definition.nodes.begin(), definition.nodes.end(), nodes_.begin());
    visited_.fill(false);
    dimensions_.fill({});
    auto status = Validate(0, 1, signals);
    if (!status.Ok())
        return status;
    for (std::size_t i = 0; i < count_; ++i)
        if (!visited_[i])
            return Error(ErrorCode::malformed_expression, "unreachable node %s", Pointer(nodes_[i]));
    band_ = 0;
    if (definition.hysteresis_band != 0) {
        if (!Ordered(nodes_[0].operation) || !IsUnit(definition.hysteresis_unit))
            return Error(ErrorCode::invalid_configuration, "rule %u hysteresis requires ordered comparison",
                         definition.rule_id);
        Dimension dimension{};
        auto q = static_cast<unsigned>(QuantityOf(definition.hysteresis_unit));
        if (q)
            dimension.powers[q - 1] = 1;
        if (!(dimension == dimensions_[nodes_[0].first_child]))
            return Error(ErrorCode::incompatible_units, "hysteresis %s at %s", UnitName(definition.hysteresis_unit),
                         Pointer(nodes_[0]));
        auto band = DeltaToSi(definition.hysteresis_band, definition.hysteresis_unit);
        if (!band.Ok())
            return band.GetStatus();
        band_ = *band.Get();
    }
    definition_ = definition;
    definition_.nodes = {nodes_.data(), count_};
    loaded_ = true;
    Reset(0);
    return {};
}
void RuleEngine::Reset(std::uint64_t generation) {
    generation_ = generation;
    result_ = {};
    hysteresis_ = false;
    pending_ = false;
    holding_ = false;
    schedule_.Cancel(ExpiryKind::hold_last, definition_.rule_id);
    schedule_.Cancel(ExpiryKind::debounce, definition_.rule_id);
}
double RuleEngine::EvaluateNode(std::size_t index, std::span<const SignalSample> samples) {
    const auto &n = nodes_[index];
    double a = 0, b = 0, c = 0;
    if (n.child_count > 0)
        a = EvaluateNode(n.first_child, samples);
    if (n.child_count > 1)
        b = EvaluateNode(static_cast<std::size_t>(n.first_child) + 1, samples);
    if (n.child_count > 2)
        c = EvaluateNode(static_cast<std::size_t>(n.first_child) + 2, samples);
    double v = std::numeric_limits<double>::quiet_NaN();
    if ((n.child_count > 0 && !std::isfinite(a)) || (n.child_count > 1 && !std::isfinite(b)) ||
        (n.child_count > 2 && !std::isfinite(c)))
        return v;
    if (n.operation == Operation::literal)
        v = n.literal_value;
    else if (n.operation == Operation::signal_reference) {
        for (const auto &sample : samples)
            if (sample.signal == n.signal) {
                v = sample.sample.value;
                break;
            }
    } else
        v = operations[static_cast<unsigned>(n.operation)].evaluate(a, b, c);
    values_[index] = v;
    return v;
}
RuleResult RuleEngine::Evaluate(std::span<const SignalSample> samples, std::uint64_t generation) {
    if (!loaded_)
        return {};
    if (generation != generation_)
        Reset(generation);
    bool missing = false, unknown = false;
    for (std::size_t i = 0; i < count_; ++i)
        if (nodes_[i].operation == Operation::signal_reference) {
            const Sample *input = nullptr;
            for (const auto &s : samples)
                if (s.signal == nodes_[i].signal) {
                    input = &s.sample;
                    break;
                }
            if (!input || input->quality != Quality::valid || !std::isfinite(input->value))
                missing = true;
            if (input && input->age_evidence == AgeEvidence::unknown)
                unknown = true;
        }
    const auto now = clock_.Now();
    if (missing) {
        pending_ = false;
        schedule_.Cancel(ExpiryKind::debounce, definition_.rule_id);
        if (definition_.missing_input_policy == MissingInputPolicy::hold_last) {
            if (!holding_) {
                holding_ = true;
                hold_since_ = now;
                auto deadline = AddTime(now, definition_.maximum_hold);
                auto status = deadline.Ok()
                                  ? schedule_.Arm({*deadline.Get(), ExpiryKind::hold_last, definition_.rule_id})
                                  : deadline.GetStatus();
                if (!status.Ok()) {
                    holding_ = false;
                    pending_ = false;
                    result_.quality = Quality::invalid;
                    result_.value = std::numeric_limits<double>::quiet_NaN();
                    return result_;
                }
            }
            if (now < hold_since_ + definition_.maximum_hold)
                return result_;
            schedule_.Cancel(ExpiryKind::hold_last, definition_.rule_id);
        }
        result_.age_unknown = unknown;
        if (definition_.missing_input_policy == MissingInputPolicy::force_warn) {
            result_.quality = Quality::valid;
            result_.current = true;
            result_.latched = true;
            result_.value = 1;
        } else {
            result_.quality = Quality::unavailable;
            result_.value = std::numeric_limits<double>::quiet_NaN();
            result_.current = false;
        }
        return result_;
    }
    holding_ = false;
    schedule_.Cancel(ExpiryKind::hold_last, definition_.rule_id);
    const auto value = EvaluateNode(0, samples);
    result_.age_unknown = unknown;
    if (!std::isfinite(value)) {
        result_.quality = Quality::invalid;
        result_.value = std::numeric_limits<double>::quiet_NaN();
        result_.current = false;
        pending_ = false;
        schedule_.Cancel(ExpiryKind::debounce, definition_.rule_id);
        return result_;
    }
    bool desired = value != 0;
    if (band_ > 0) {
        const auto child = nodes_[0].first_child;
        const auto a = values_[child];
        const auto b = values_[static_cast<std::size_t>(child) + 1];
        const auto op = nodes_[0].operation;
        const bool greater = op == Operation::greater || op == Operation::greater_or_equal;
        const auto boundary = b + (greater ? (hysteresis_ ? -band_ : band_) : (hysteresis_ ? band_ : -band_));
        if (op == Operation::greater)
            desired = a > boundary;
        if (op == Operation::greater_or_equal)
            desired = a >= boundary;
        if (op == Operation::less)
            desired = a < boundary;
        if (op == Operation::less_or_equal)
            desired = a <= boundary;
    }
    hysteresis_ = desired;
    if (desired == result_.current) {
        pending_ = false;
        schedule_.Cancel(ExpiryKind::debounce, definition_.rule_id);
    } else {
        if (!pending_ || pending_value_ != desired) {
            pending_ = true;
            pending_value_ = desired;
            pending_since_ = now;
            auto deadline = AddTime(now, definition_.debounce);
            auto status = deadline.Ok() ? schedule_.Arm({*deadline.Get(), ExpiryKind::debounce, definition_.rule_id})
                                        : deadline.GetStatus();
            if (!status.Ok()) {
                holding_ = false;
                pending_ = false;
                result_.quality = Quality::invalid;
                result_.value = std::numeric_limits<double>::quiet_NaN();
                return result_;
            }
        }
        if (now >= pending_since_ + definition_.debounce) {
            result_.current = desired;
            pending_ = false;
            schedule_.Cancel(ExpiryKind::debounce, definition_.rule_id);
        }
    }
    result_.quality = Quality::valid;
    result_.value = value;
    if (result_.current)
        result_.latched = true;
    return result_;
}
Status RuleEngine::Acknowledge(std::uint32_t rule_id) {
    if (!loaded_ || rule_id != definition_.rule_id)
        return Error(ErrorCode::invalid_configuration, "rule %u not loaded", rule_id);
    result_.latched = false;
    return {};
}
} // namespace signal_core
