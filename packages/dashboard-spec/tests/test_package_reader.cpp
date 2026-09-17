#include "PackageInternal.h"
#include "TestSupport.h"
#include "dashboard_spec/PackageReader.h"
TEST_CASE("package cases agree in archive and directory forms") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    ds::PackageReader reader(validator);
    ds::Document cases;
    REQUIRE(ds::BoundedParse(Read(Repo() / "tests/fixtures/packages/cases.json"), cases).Ok());
    for (const auto &c : cases.Data().json.GetArray()) {
        const auto name = ds::Text(c["name"]);
        INFO(name);
        const auto profile =
            c.HasMember("profile") && ds::Text(c["profile"]) == "desktop" ? ds::Profile::desktop : ds::Profile::mobile;
        const auto packed = reader.Read((Repo() / "tests/fixtures/packages" / (name + ".udash")).string(), profile);
        CHECK_MESSAGE(ds::Text(c["code"]) == ds::CodeName(packed.code), packed.pointer, " ", packed.message);
        if (!c["archive_only"].GetBool()) {
            const auto unpacked = reader.Read((Repo() / "tests/fixtures/packages" / name).string(), profile);
            CHECK_MESSAGE(ds::Text(c["code"]) == ds::CodeName(unpacked.code), unpacked.pointer, " ", unpacked.message);
        }
    }
}

TEST_CASE("unpacked hard links cannot reach the asset reader") {
    std::error_code error;
    const auto root =
        std::filesystem::temp_directory_path(error) /
        ("dashboard-hardlink-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(!error);
    REQUIRE(std::filesystem::create_directory(root, error));
    REQUIRE(!error);
    const auto original = root / "original.bin";
    const auto alias = root / "alias.bin";
    {
        std::ofstream file(original, std::ios::binary);
        file << "synthetic";
    }
    std::filesystem::create_hard_link(original, alias, error);
    const bool linked = !error;
    CHECK_MESSAGE(linked, error.message());
    if (linked) {
        ds::Validator validator(DASHBOARD_SCHEMA_DIR);
        CHECK(ds::PackageReader(validator).Read(root.string()).code == ds::ErrorCode::E_PKG_PATH_LINK);
    }
    std::filesystem::remove(alias, error);
    std::filesystem::remove(original, error);
    std::filesystem::remove(root, error);
}

TEST_CASE("Unicode case aliases share the same resolver key") {
    ds::PathResolver resolver;
    std::string normalized;
    REQUIRE(resolver.Add("assets/\xc3\x89.png", false, normalized).Ok());
    CHECK(resolver.Add("assets/\xc3\xa9.png", false, normalized).code == ds::ErrorCode::E_PKG_CASE_COLLISION);
}

TEST_CASE("central directory limits reject tiny declarations before miniz") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    const auto path = Repo() / "tests/fixtures/packages/central-directory-entry-count.udash";
    CHECK(std::filesystem::file_size(path) == 98);
    CHECK(ds::PackageReader(validator).Read(path.string()).code == ds::ErrorCode::E_PKG_ENTRY_COUNT);
}

TEST_CASE("ZIP64 locators are checked without legacy sentinels") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    ds::PackageReader reader(validator);
    const auto root = Repo() / "tests/fixtures/packages";
    const auto oversized = root / "zip64-small-legacy.udash";
    const auto outside = root / "zip64-locator-outside-file.udash";
    CHECK(std::filesystem::file_size(oversized) == 98);
    CHECK(std::filesystem::file_size(outside) == 98);
    CHECK(reader.Read(oversized.string()).code == ds::ErrorCode::E_PKG_ENTRY_COUNT);
    CHECK(reader.Read(outside.string()).code == ds::ErrorCode::E_PKG_ZIP_MALFORMED);
}
