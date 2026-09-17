#include "TestSupport.h"
#include "dashboard_spec/SemanticPass.h"
#include <chrono>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
TEST_CASE("hierarchy and inherited policies are checked") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    CHECK(validator.ValidateText(Fixture("valid/inherited-preserve.json")).Ok());
    CHECK(validator.ValidateText(Fixture("invalid/inherited-stretch.json")).code ==
          ds::ErrorCode::E_ASPECT_POLICY_FORBIDDEN);
    CHECK(validator.ValidateText(Fixture("invalid/orphan-subtree.json")).code == ds::ErrorCode::E_ORPHAN_SUBTREE);
    CHECK(validator.ValidateText(Fixture("valid/shared-image.json")).Ok());
    CHECK(validator.ValidateText(Fixture("valid/temperature-compatible.json")).Ok());
    CHECK(validator.ValidateText(Fixture("invalid/unit-mismatch.json")).code == ds::ErrorCode::E_RULE_UNIT_MISMATCH);
}

TEST_CASE("semantic public entry enforces the validation pipeline") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    for (const char *input : {R"({"reference_viewport":null})", R"({"dashboard":{"components":null}})"}) {
        ds::Document document;
        REQUIRE(ds::BoundedParse(input, document).Ok());
        CHECK(ds::SemanticPass(document, validator).code == ds::ErrorCode::E_SCHEMA);
    }
    ds::Document document;
    REQUIRE(ds::BoundedParse(Fixture("valid/component-id-token.json"), document).Ok());
    CHECK(ds::SemanticPass(document, validator).Ok());
}

TEST_CASE("semantic field guards survive a mistakenly widened schema") {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("dashboard-schema-guards-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    REQUIRE(std::filesystem::create_directory(directory, ec));
    for (const auto &file : std::filesystem::directory_iterator(Repo() / "packages/dashboard-spec/schema"))
        std::filesystem::copy_file(file.path(), directory / file.path().filename(), ec);
    REQUIRE(!ec);
    ds::Document schema;
    REQUIRE(ds::BoundedParse(Read(directory / "dashboard.schema.json"), schema).Ok());
    auto &component = schema.Data().json["properties"]["components"]["patternProperties"].MemberBegin()->value;
    auto *branch = &component["allOf"][0];
    while (ds::Text((*branch)["if"]["properties"]["type"]["const"]) != "bar_gauge")
        branch = &(*branch)["else"];
    auto &properties = (*branch)["then"]["properties"]["properties"];
    properties["required"].SetArray();
    properties["properties"]["circular"].SetObject();
    rapidjson::StringBuffer bytes;
    rapidjson::Writer<rapidjson::StringBuffer> writer(bytes);
    schema.Data().json.Accept(writer);
    {
        std::ofstream out(directory / "dashboard.schema.json", std::ios::binary);
        out.write(bytes.GetString(), bytes.GetSize());
    }
    ds::Validator validator(directory.string());
    for (bool missing : {false, true}) {
        ds::Document document;
        REQUIRE(ds::BoundedParse(Fixture("valid/nine-primitives.json"), document).Ok());
        auto &value = document.Data().json["dashboard"]["components"]["bar_gauge"]["properties"];
        if (missing)
            value.RemoveMember("circular");
        else
            value["circular"].SetString("wrong type");
        const auto result = ds::SemanticPass(document, validator);
        CHECK(result.code == ds::ErrorCode::E_SCHEMA);
        CHECK(result.pointer.View() == "/dashboard/components/bar_gauge/properties/circular");
    }
    for (const auto &file : std::filesystem::directory_iterator(directory))
        std::filesystem::remove(file.path(), ec);
    std::filesystem::remove(directory, ec);
}
