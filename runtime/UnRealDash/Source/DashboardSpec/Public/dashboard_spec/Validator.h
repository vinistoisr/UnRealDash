#pragma once
#include "dashboard_spec/Export.h"
#include "dashboard_spec/Document.h"
namespace dashboard_spec {
class DS_EXPORT Validator {
  public:
    explicit Validator(std::string_view schema_directory);
    ~Validator();
    Validator(const Validator &) = delete;
    Validator &operator=(const Validator &) = delete;
    Error Validate(const Document &document) const;
    Error ValidateText(std::string_view input) const;

  private:
    struct Storage;
    Storage *storage_;
};
} // namespace dashboard_spec
