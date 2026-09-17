#pragma once
#include "dashboard_spec/Export.h"
#include "dashboard_spec/Validator.h"
#include <cstdint>
#include <span>
namespace dashboard_spec {
enum class Profile { mobile, desktop };
class DS_EXPORT LoadedPackage {
  public:
    LoadedPackage();
    ~LoadedPackage();
    LoadedPackage(LoadedPackage &&) noexcept;
    LoadedPackage &operator=(LoadedPackage &&) noexcept;
    LoadedPackage(const LoadedPackage &) = delete;
    LoadedPackage &operator=(const LoadedPackage &) = delete;
    const Document &Doc() const;
    // Directory assets use a mutable lazy cache. Callers serialize first access.
    // Returned spans belong to this package until destruction or move-from.
    Error Asset(std::string_view normalized_name, std::span<const std::uint8_t> &out) const;
    std::span<const std::string_view> AssetNames() const;

  private:
    friend class PackageReader;
    struct Storage;
    Storage *storage_;
};
class DS_EXPORT PackageReader {
  public:
    explicit PackageReader(const Validator &validator) : validator_(validator) {}
    Error Read(std::string_view path, Profile profile = Profile::desktop) const;
    // Failure preserves out. Success replaces it atomically after validation.
    Error Load(std::string_view path, Profile profile, LoadedPackage &out) const;

  private:
    Error Validate(std::string_view path, Profile profile, LoadedPackage *out) const;
    const Validator &validator_;
};
} // namespace dashboard_spec
