#pragma once
#include "dashboard_spec/Validator.h"
namespace dashboard_spec {
enum class Profile { mobile, desktop };
class PackageReader {
  public:
    explicit PackageReader(const Validator &validator) : validator_(validator) {}
    Error Read(std::string_view path, Profile profile = Profile::desktop) const;

  private:
    const Validator &validator_;
};
} // namespace dashboard_spec
