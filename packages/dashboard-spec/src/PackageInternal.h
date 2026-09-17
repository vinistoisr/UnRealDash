#pragma once
#include "Internal.h"
namespace dashboard_spec {
class PathResolver {
  public:
    Error Add(std::string_view name, bool link, std::string &normalized);
    Error Resolve(std::string_view name, std::string &normalized) const;

  private:
    std::set<std::string> entries_;
    std::set<std::string> folded_;
};
struct ImageInfo {
    std::uint32_t width{};
    std::uint32_t height{};
    std::string format;
};
Error ImageHeader(std::string_view bytes, std::string_view path, ImageInfo &output);
} // namespace dashboard_spec
