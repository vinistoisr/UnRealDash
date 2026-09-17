#include "../../signal-core/tests/support/TestPackData.h"
#include "dashboard_spec/DefinitionPackBuilder.h"
#include <doctest.h>
#include <fstream>
#include <iterator>
namespace {
std::string FixtureJson() {
    std::ifstream file(REPO_ROOT "/tests/fixtures/packs/binary-telemetry-v1.test.json");
    return {std::istreambuf_iterator<char>(file), {}};
}
void Replace(std::string &text, const std::string &from, const std::string &to) {
    const auto at = text.find(from);
    REQUIRE(at != text.npos);
    text.replace(at, from.size(), to);
}
} // namespace
TEST_CASE("3.1 fixture pack validates against the definition pack schema") {
    dashboard_spec::Validator v(DASHBOARD_SCHEMA_DIR);
    CHECK(v.ValidateText(FixtureJson()).Ok());
}
TEST_CASE("3.1 builder output equals the literal test pack") {
    dashboard_spec::Validator v(DASHBOARD_SCHEMA_DIR);
    dashboard_spec::DefinitionPackBuilder b;
    REQUIRE(b.Build(FixtureJson(), v).Ok());
    const auto &a = b.Pack();
    const auto &e = telemetry_test::pack;
    CHECK(std::string(a.id) == e.id);
    CHECK(std::string(a.version) == e.version);
    CHECK(a.api == e.api);
    REQUIRE(a.frames.size() == e.frames.size());
    for (std::size_t i = 0; i < a.frames.size(); ++i) {
        const auto &af = a.frames[i];
        const auto &ef = e.frames[i];
        CHECK(af.frame_id == ef.frame_id);
        CHECK(af.role == ef.role);
        CHECK(af.payload_length == ef.payload_length);
        REQUIRE(af.fields.size() == ef.fields.size());
        for (std::size_t j = 0; j < af.fields.size(); ++j) {
            const auto &x = af.fields[j];
            const auto &y = ef.fields[j];
            CHECK(std::string(x.name) == y.name);
            CHECK(x.signal == y.signal);
            CHECK(x.byte_offset == y.byte_offset);
            CHECK(x.width_bytes == y.width_bytes);
            CHECK(x.is_signed == y.is_signed);
            CHECK(x.little_endian == y.little_endian);
            CHECK(x.scale == y.scale);
            CHECK(x.offset == y.offset);
            CHECK(x.unit == y.unit);
            CHECK(x.acquisition == y.acquisition);
            REQUIRE(x.sentinels.size() == y.sentinels.size());
            for (std::size_t k = 0; k < x.sentinels.size(); ++k)
                CHECK(x.sentinels[k] == y.sentinels[k]);
        }
    }
}
TEST_CASE("3.1 builder rejects a field with no acquisition") {
    auto json = FixtureJson();
    Replace(json, "\"acquisition\": \"held\",", "");
    dashboard_spec::Validator v(DASHBOARD_SCHEMA_DIR);
    dashboard_spec::DefinitionPackBuilder b;
    const auto e = b.Build(json, v);
    CHECK_FALSE(e.Ok());
    CHECK_FALSE(e.pointer.View().empty());
    CHECK(b.Pack().frames.empty());
}
TEST_CASE("3.1 builder rejects a field running past the payload length") {
    auto json = FixtureJson();
    Replace(json, "\"byte_offset\": 0", "\"byte_offset\": 7");
    dashboard_spec::Validator v(DASHBOARD_SCHEMA_DIR);
    dashboard_spec::DefinitionPackBuilder b;
    const auto e = b.Build(json, v);
    CHECK_FALSE(e.Ok());
    CHECK(e.pointer.View().find("byte_offset") != std::string_view::npos);
}
TEST_CASE("3.1 builder maps type to width and applies the per-value signedness override") {
    dashboard_spec::Validator v(DASHBOARD_SCHEMA_DIR);
    dashboard_spec::DefinitionPackBuilder b;
    REQUIRE(b.Build(FixtureJson(), v).Ok());
    CHECK(b.Pack().frames[1].fields[0].is_signed);
    CHECK_FALSE(b.Pack().frames[1].fields[1].is_signed);
    CHECK(b.Pack().frames[1].fields[0].width_bytes == 2);
}
TEST_CASE("builder rejects missing role and unknown unit and clears previous output") {
    dashboard_spec::Validator v(DASHBOARD_SCHEMA_DIR);
    dashboard_spec::DefinitionPackBuilder b;
    for (bool role : {false, true}) {
        REQUIRE(b.Build(FixtureJson(), v).Ok());
        auto json = FixtureJson();
        if (role)
            Replace(json, "\"role\": \"telemetry\",", "");
        else
            Replace(json, "\"unit\": \"dimensionless\"", "\"unit\": \"not-a-unit\"");
        const auto error = b.Build(json, v);
        CHECK_FALSE(error.Ok());
        if (!role) {
            CHECK(error.pointer.View() == "/frames/0/signals/0/unit");
            CHECK(error.message.View().find("not-a-unit") != std::string_view::npos);
        }
        CHECK(b.Pack().frames.empty());
    }
}
