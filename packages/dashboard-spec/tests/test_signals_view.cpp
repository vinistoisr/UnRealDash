#include "TestSupport.h"
#include "dashboard_spec/PackageReader.h"

// Document::Signals reaches the signals document a package ships beside dashboard.json. A binding
// names a signal by string and the registry works in numbers, so this is the table that joins them,
// and PLAN 4.9's live binding path cannot resolve a single gauge without it.

TEST_CASE("4.9 a package's signals document is reachable") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    ds::PackageReader reader(validator);
    ds::LoadedPackage loaded;
    // The chunk 12 dial fixture ships signals.json declaring one signal, engine_speed, which every
    // gauge in it is bound to.
    REQUIRE(reader.Load((Repo() / "tests/fixtures/packages/dials-stage0.udash").string(), ds::Profile::desktop,
                        loaded)
                .Ok());

    const auto signals = loaded.Doc().Signals();
    REQUIRE(signals.Exists());
    const auto list = signals.Member("signals");
    REQUIRE(list.Exists());
    REQUIRE(list.Size() == 1);

    std::string_view name;
    REQUIRE(list.Element(0).Member("id").String(name));
    CHECK(name == "engine_speed");

    std::string_view unit;
    REQUIRE(list.Element(0).Member("unit").String(unit));
    CHECK(unit == "rpm");

    double deadline = 0;
    REQUIRE(list.Element(0).Member("freshness_deadline_ms").Number(deadline));
    CHECK(deadline == 200);

    // The dashboard is still reached through Root, which reads the bundle's dashboard member.
    // Signals sits beside it rather than inside it, and confusing the two would silently return an
    // empty view rather than fail, so both are checked together.
    CHECK(loaded.Doc().Root().Member("components").Exists());
    CHECK_FALSE(loaded.Doc().Root().Member("signals").Exists());
}

TEST_CASE("4.9 a package with no signals document reports an empty view") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    ds::PackageReader reader(validator);
    ds::LoadedPackage loaded;
    // well-formed ships dashboard.json and manifest.json only.
    REQUIRE(
        reader.Load((Repo() / "tests/fixtures/packages/well-formed.udash").string(), ds::Profile::desktop, loaded)
            .Ok());
    // Empty rather than a crash or a fabricated table. A document that binds nothing has nothing to
    // resolve, and the binding table reports an unresolved signal against the component's pointer.
    CHECK_FALSE(loaded.Doc().Signals().Exists());
}
