#include "../../../third_party/miniz/miniz.h"
#include "TestSupport.h"
#include "dashboard_spec/PackageReader.h"
#include <atomic>
#include <iostream>
#include <new>
#include <type_traits>
namespace {
std::atomic<std::size_t> fail_asset_allocation_size{};
}
// Replace only the standard nothrow array allocator in this test executable.
// The payload-sized failure is armed after validation and consumed once.
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
    std::size_t expected = size;
    if (fail_asset_allocation_size.compare_exchange_strong(expected, 0))
        return nullptr;
    return ::operator new[](size);
}
TEST_CASE("Load preserves Read verdicts and archive-directory pointer parity") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    ds::PackageReader reader(validator);
    ds::Document cases;
    REQUIRE(ds::BoundedParse(Read(Repo() / "tests/fixtures/packages/cases.json"), cases).Ok());
    std::size_t accepted = 0, rejected = 0, both = 0, archive_only = 0;
    for (const auto &c : cases.Data().json.GetArray()) {
        const auto name = ds::Text(c["name"]);
        INFO(name);
        const auto profile =
            c.HasMember("profile") && ds::Text(c["profile"]) == "desktop" ? ds::Profile::desktop : ds::Profile::mobile;
        const auto root = Repo() / "tests/fixtures/packages";
        ds::LoadedPackage packed;
        const auto load = reader.Load((root / (name + ".udash")).string(), profile, packed);
        const auto read = reader.Read((root / (name + ".udash")).string(), profile);
        CHECK(ds::Text(c["code"]) == ds::CodeName(load.code));
        CHECK(load.code == read.code);
        CHECK(load.pointer.View() == read.pointer.View());
        if (ds::Text(c["code"]).empty())
            ++accepted;
        else
            ++rejected;
        if (c.HasMember("archive_only") && c["archive_only"].GetBool()) {
            ++archive_only;
            continue;
        }
        ++both;
        ds::LoadedPackage directory;
        const auto unpacked = reader.Load((root / name).string(), profile, directory);
        CHECK(unpacked.Ok() == load.Ok());
        CHECK(unpacked.code == load.code);
        CHECK(unpacked.pointer.View() == load.pointer.View());
        if (load.Ok()) {
            CHECK(packed.Doc().ComponentCount() == directory.Doc().ComponentCount());
            REQUIRE(packed.AssetNames().size() == directory.AssetNames().size());
            for (auto asset : packed.AssetNames()) {
                std::span<const std::uint8_t> a, b;
                REQUIRE(packed.Asset(asset, a).Ok());
                REQUIRE(directory.Asset(asset, b).Ok());
                CHECK(std::equal(a.begin(), a.end(), b.begin(), b.end()));
            }
        }
    }
    std::cout << "Load corpus accepted=" << accepted << " rejected=" << rejected << " both=" << both
              << " archive_only=" << archive_only << '\n';
    CHECK(accepted + rejected == cases.Data().json.Size());
    CHECK(both + archive_only == cases.Data().json.Size());
}
TEST_CASE("loaded content owns its lifetime and move preserves spans") {
    static_assert(!std::is_copy_constructible_v<ds::LoadedPackage>);
    ds::LoadedPackage loaded;
    {
        ds::Validator validator(DASHBOARD_SCHEMA_DIR);
        REQUIRE(ds::PackageReader(validator)
                    .Load((Repo() / "tests/fixtures/packages/well-formed.udash").string(), ds::Profile::mobile, loaded)
                    .Ok());
    }
    const auto names = loaded.AssetNames();
    REQUIRE(!names.empty());
    std::span<const std::uint8_t> bytes;
    REQUIRE(loaded.Asset(names.front(), bytes).Ok());
    REQUIRE(!bytes.empty());
    const auto *original = bytes.data();
    ds::LoadedPackage moved(std::move(loaded));
    REQUIRE(moved.Asset(names.front(), bytes).Ok());
    CHECK(bytes.data() == original);
    ds::LoadedPackage assigned;
    assigned = std::move(moved);
    REQUIRE(assigned.Asset(names.front(), bytes).Ok());
    CHECK(bytes.data() == original);
    CHECK(assigned.Asset("missing", bytes).code == ds::ErrorCode::E_PKG_ASSET_NOT_IN_PACKAGE);
    CHECK(bytes.empty());
    bool saw_root = false, saw_image = false;
    for (std::size_t i = 0; i < assigned.Doc().ComponentCount(); ++i) {
        ds::ComponentView node;
        REQUIRE(assigned.Doc().ComponentAt(i, node));
        CHECK(node.pointer.View() == "/dashboard/components/" + std::string(node.id));
        if (node.id == "root") {
            CHECK(node.parent.empty());
            saw_root = true;
        }
        if (node.type == "image") {
            std::string_view asset;
            CHECK(node.properties.Member("asset").String(asset));
            CHECK(node.parent == "root");
            saw_image = true;
        }
        double width = 0;
        CHECK(node.node.Member("rect").Member("width").Number(width));
        CHECK(width > 0);
        bool clipping = false;
        CHECK(node.node.Member("clipping").Boolean(clipping));
        CHECK(clipping);
    }
    CHECK(saw_root);
    CHECK(saw_image);
    ds::ComponentView absent;
    CHECK_FALSE(assigned.Doc().ComponentAt(assigned.Doc().ComponentCount(), absent));
    std::string_view token;
    CHECK(assigned.Doc().Theme().Member("day").Member("foreground").String(token));
    CHECK(token == "#ffffff");
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    CHECK_FALSE(ds::PackageReader(validator).Load("does-not-exist", ds::Profile::desktop, assigned).Ok());
    CHECK(assigned.Doc().ComponentCount() > 0);
}
TEST_CASE("directory assets are lazy bounded cached and reject changed links") {
    const auto root = std::filesystem::temp_directory_path() /
                      ("dashboard-load-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }
    } cleanup{root};
    std::error_code ec;
    std::filesystem::copy(Repo() / "tests/fixtures/packages/well-formed", root,
                          std::filesystem::copy_options::recursive, ec);
    REQUIRE(!ec);
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    ds::PackageReader reader(validator);
    ds::LoadedPackage missing, bounded, cached, linked;
    for (auto *package : {&missing, &bounded, &cached, &linked})
        REQUIRE(reader.Load(root.string(), ds::Profile::mobile, *package).Ok());
    const auto file = root / "assets/shared.png";
    const auto original = Read(file);
    std::span<const std::uint8_t> bytes;
    REQUIRE(cached.Asset("assets/shared.png", bytes).Ok());
    const auto *address = bytes.data();
    std::filesystem::remove(file, ec);
    REQUIRE(!ec);
    CHECK(missing.Asset("assets/shared.png", bytes).code == ds::ErrorCode::E_PKG_ASSET_UNREADABLE);
    REQUIRE(cached.Asset("assets/shared.png", bytes).Ok());
    CHECK(bytes.data() == address);
    {
        std::ofstream stream(file, std::ios::binary);
        stream.seekp(static_cast<std::streamoff>(ds::limits::asset_size));
        stream.put('x');
    }
    CHECK(bounded.Asset("assets/shared.png", bytes).code == ds::ErrorCode::E_PKG_ASSET_SIZE);
    {
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream << original;
    }
    std::filesystem::create_hard_link(file, root / "alias.png", ec);
    REQUIRE(!ec);
    CHECK(linked.Asset("assets/shared.png", bytes).code == ds::ErrorCode::E_PKG_PATH_LINK);
}

TEST_CASE("Read does not decompress unreferenced archive assets") {
    const auto archive_path = std::filesystem::temp_directory_path() /
                              ("dashboard-read-only-" +
                               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".udash");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{archive_path};
    mz_zip_archive archive{};
    REQUIRE(mz_zip_writer_init_file(&archive, archive_path.string().c_str(), 0));
    const auto fixture = Repo() / "tests/fixtures/packages/well-formed";
    for (const auto &entry : std::filesystem::recursive_directory_iterator(fixture)) {
        if (entry.is_directory())
            continue;
        const auto bytes = Read(entry.path());
        const auto name = entry.path().lexically_relative(fixture).generic_string();
        REQUIRE(mz_zip_writer_add_mem(&archive, name.c_str(), bytes.data(), bytes.size(), 0));
    }
    const std::string marker = "unused-asset-with-invalid-crc";
    REQUIRE(mz_zip_writer_add_mem(&archive, "unused.bin", marker.data(), marker.size(), 0));
    REQUIRE(mz_zip_writer_finalize_archive(&archive));
    REQUIRE(mz_zip_writer_end(&archive));
    auto bytes = Read(archive_path);
    const auto position = bytes.find(marker);
    REQUIRE(position != std::string::npos);
    bytes[position] = 'X';
    {
        std::ofstream file(archive_path, std::ios::binary | std::ios::trunc);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    ds::PackageReader reader(validator);
    CHECK(reader.Read(archive_path.string()).Ok());
    ds::LoadedPackage loaded;
    CHECK_FALSE(reader.Load(archive_path.string(), ds::Profile::desktop, loaded).Ok());
}

TEST_CASE("asset allocation failure is an error value and can be retried") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    ds::LoadedPackage loaded;
    REQUIRE(ds::PackageReader(validator)
                .Load((Repo() / "tests/fixtures/packages/well-formed").string(), ds::Profile::mobile, loaded)
                .Ok());
    const auto size = std::filesystem::file_size(Repo() / "tests/fixtures/packages/well-formed/assets/shared.png");
    fail_asset_allocation_size = static_cast<std::size_t>(size) + 1;
    std::span<const std::uint8_t> bytes;
    const auto error = loaded.Asset("assets/shared.png", bytes);
    fail_asset_allocation_size = 0;
    CHECK(error.code == ds::ErrorCode::E_PKG_ASSET_UNREADABLE);
    CHECK(bytes.empty());
    REQUIRE(loaded.Asset("assets/shared.png", bytes).Ok());
    CHECK(bytes.size() == size);
}
