#pragma once
#include "SignalCore/DefinitionPack.h"
#include "dashboard_spec/Validator.h"
namespace dashboard_spec {
// Storage belongs to the builder. Pack views remain valid until Build or destruction.
class DefinitionPackBuilder {
  public:
    DefinitionPackBuilder();
    ~DefinitionPackBuilder();
    DefinitionPackBuilder(const DefinitionPackBuilder &) = delete;
    DefinitionPackBuilder &operator=(const DefinitionPackBuilder &) = delete;
    Error Build(std::string_view json, const Validator &validator);
    const signal_core::DefinitionPack &Pack() const;

  private:
    struct Storage;
    Storage *storage_;
};
} // namespace dashboard_spec
