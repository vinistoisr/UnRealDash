#pragma once
#include "../../../runtime/UnRealDash/Source/DashboardSpec/Private/Internal.h"
#include "dashboard_spec/Validator.h"
#include <doctest.h>
namespace ds = dashboard_spec;
inline std::filesystem::path Repo() { return REPO_ROOT; }
inline std::string Read(const std::filesystem::path &path) {
    std::string bytes;
    const bool ok = ds::ReadBounded(path, ds::limits::document_size, bytes);
    CHECK(ok);
    return bytes;
}
inline std::string Fixture(std::string_view name) { return Read(Repo() / "tests/fixtures/documents" / name); }
