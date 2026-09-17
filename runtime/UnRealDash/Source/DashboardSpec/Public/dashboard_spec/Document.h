#pragma once
#include "dashboard_spec/Errors.h"
#include "dashboard_spec/Export.h"
#include <string_view>
namespace dashboard_spec {
// Non-owning typed JSON access. Views live as long as the owning document.
class DS_EXPORT PropertyView {
  public:
    PropertyView() = default;
    explicit PropertyView(const void *value) : value_(value) {}
    PropertyView Member(std::string_view name) const;
    PropertyView Element(std::size_t index) const;
    std::size_t Size() const;
    std::string_view Key(std::size_t index) const;
    bool String(std::string_view &out) const;
    bool Number(double &out) const;
    bool Boolean(bool &out) const;
    bool Exists() const { return value_ != nullptr; }

  private:
    const void *value_{};
};
struct ComponentView {
    std::string_view type, id, parent;
    ErrorText pointer;
    PropertyView properties;
    PropertyView node;
};
class DS_EXPORT Document {
  public:
    Document();
    ~Document();
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;
    struct Storage;
    Storage &Data();
    const Storage &Data() const;
    std::size_t ComponentCount() const;
    bool ComponentAt(std::size_t index, ComponentView &out) const;
    PropertyView Theme() const;

  private:
    Storage *storage_;
};
DS_EXPORT Error BoundedParse(std::string_view input, Document &output);
} // namespace dashboard_spec
