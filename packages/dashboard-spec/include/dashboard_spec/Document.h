#pragma once
#include "dashboard_spec/Errors.h"
#include <string_view>
namespace dashboard_spec {
class Document {
  public:
    Document();
    ~Document();
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;
    struct Storage;
    Storage &Data();
    const Storage &Data() const;

  private:
    Storage *storage_;
};
Error BoundedParse(std::string_view input, Document &output);
} // namespace dashboard_spec
