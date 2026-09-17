#pragma once
#include "SignalCore/RuleEngine.h"
#include "dashboard_spec/Document.h"
#include <span>
namespace dashboard_spec {
struct SignalBinding {
    std::string_view name;
    std::uint32_t index;
};
// Caller owns this storage and keeps it alive while SignalCore consumes the rule.
class RuleTreeBuilder {
  public:
    RuleTreeBuilder();
    ~RuleTreeBuilder();
    RuleTreeBuilder(const RuleTreeBuilder &) = delete;
    RuleTreeBuilder &operator=(const RuleTreeBuilder &) = delete;
    Error Build(const Document &document, std::string_view rule_name, std::uint32_t rule_id,
                std::span<const SignalBinding> signals);
    const signal_core::RuleDefinition &Rule() const;

  private:
    struct Storage;
    Storage *storage_;
};
} // namespace dashboard_spec
