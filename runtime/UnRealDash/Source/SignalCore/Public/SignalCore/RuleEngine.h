#pragma once
#include "SignalCore/Export.h"
#include "SignalCore/SignalRegistry.h"
#include <array>
namespace signal_core {
enum class Operation : std::uint8_t {
    literal,
    signal_reference,
    add,
    subtract,
    multiply,
    divide,
    less,
    less_or_equal,
    greater,
    greater_or_equal,
    equal,
    not_equal,
    logical_and,
    logical_or,
    logical_not,
    minimum,
    maximum,
    clamp,
    absolute_value
};
enum class MissingInputPolicy : std::uint8_t { hold_last, treat_unavailable, force_warn };
// Chunk 03's builder sets this when a literal node has no unit.
inline constexpr Unit kLiteralUnitOmitted = static_cast<Unit>(UINT16_MAX);
struct ExpressionNode {
    Operation operation{};
    double literal_value{};
    Unit literal_unit{};
    std::uint32_t signal{};
    std::uint16_t first_child{};
    std::uint8_t child_count{};
    const char *source_pointer{};
};
struct RuleDefinition {
    std::uint32_t rule_id{};
    std::span<const ExpressionNode> nodes{};
    double hysteresis_band{};
    Unit hysteresis_unit{};
    std::chrono::nanoseconds debounce{};
    MissingInputPolicy missing_input_policy{};
    std::chrono::nanoseconds maximum_hold{};
};
struct RuleResult {
    double value{std::numeric_limits<double>::quiet_NaN()};
    Quality quality{Quality::unavailable};
    bool current{};
    bool latched{};
    bool age_unknown{};
};
// One compiled rule per instance. Evaluation order: quality check, missing-input policy,
// evaluate, hysteresis, debounce. Schedule is shared with registry freshness deadlines.
// Caller evaluates on arrivals and earliest armed expiry, after registry.Expire().
// Nonzero hysteresis requires a root ordered comparison; band is a full offset on either side.
// The builder represents an omitted literal unit with kLiteralUnitOmitted.
class SIGNALCORE_API RuleEngine {
  public:
    RuleEngine(Clock clock, ExpirySchedule &schedule) : clock_(clock), schedule_(schedule) {}
    Status Load(const RuleDefinition &definition, std::span<const Signal> signals);
    RuleResult Evaluate(std::span<const SignalSample> samples, std::uint64_t generation);
    Status Acknowledge(std::uint32_t rule_id);
    std::uint32_t RuleId() const { return definition_.rule_id; }
    void ClearLatch() { result_.latched = false; }
    const RuleResult &Current() const { return result_; }

  private:
    struct Dimension {
        std::array<int, 4> powers{};
        bool operator==(const Dimension &) const = default;
    };
    Status Validate(std::size_t index, unsigned depth, std::span<const Signal> signals);
    double EvaluateNode(std::size_t index, std::span<const SignalSample> samples);
    void Reset(std::uint64_t generation);
    Clock clock_;
    ExpirySchedule &schedule_;
    RuleDefinition definition_{};
    std::array<ExpressionNode, 512> nodes_{};
    std::array<Dimension, 512> dimensions_{};
    std::array<Unit, 512> units_{};
    std::array<bool, 512> visited_{};
    std::array<double, 512> values_{};
    std::size_t count_{};
    double band_{};
    RuleResult result_{};
    std::uint64_t generation_{};
    bool loaded_{};
    bool hysteresis_{};
    bool pending_{};
    bool pending_value_{};
    bool holding_{};
    Time pending_since_{};
    Time hold_since_{};
};
} // namespace signal_core
