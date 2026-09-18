#pragma once
#include "dashboard_spec/Export.h"
#include "dashboard_spec/Validator.h"
#include <cstdint>
#include <span>
#include <string>
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
    // Turns a reference as a document writes it into the normalized name Asset expects. The
    // document may spell an asset "assets//shared.png" or "assets/./shared.png"; the package
    // reader normalizes entry names when it admits them, so a raw reference does not match one.
    // Callers outside this library cannot normalize for themselves and must not try: the rules
    // are the path gate, and a second implementation of them would be a second gate that drifts.
    Error ResolveAssetName(std::string_view reference, std::string &normalized) const;
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
