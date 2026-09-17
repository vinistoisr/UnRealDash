#include "TestSupport.h"
TEST_CASE("bounded SAX parse rejects duplicates with escaped pointers") {
    ds::Document doc;
    auto error = ds::BoundedParse(R"({"a/b":{"~x":1,"~x":2}})", doc);
    CHECK(error.code == ds::ErrorCode::E_DUPLICATE_KEY);
    CHECK(std::string(error.pointer) == "/a~1b/~0x");
    CHECK(ds::BoundedParse("{}", doc).Ok());
    CHECK(ds::BoundedParse("{}x", doc).code == ds::ErrorCode::E_JSON_SYNTAX);
    CHECK(ds::BoundedParse("[NaN]", doc).code == ds::ErrorCode::E_JSON_SYNTAX);
    CHECK(ds::BoundedParse(std::string_view("{}\0x", 4), doc).code == ds::ErrorCode::E_JSON_SYNTAX);
    CHECK(ds::BoundedParse("1" + std::string(309, '0'), doc).code == ds::ErrorCode::E_JSON_SYNTAX);
    const auto depth = ds::limits::json_depth;
    CHECK(ds::BoundedParse(std::string(depth, '[') + "0" + std::string(depth, ']'), doc).Ok());
    CHECK(ds::BoundedParse(std::string(depth + 1, '[') + "0" + std::string(depth + 1, ']'), doc).code ==
          ds::ErrorCode::E_JSON_TOO_DEEP);
}

TEST_CASE("long escaped error pointers are not truncated") {
    ds::Document doc;
    const std::string key(10000, 'x');
    const auto error = ds::BoundedParse("{\"" + key + "\":0,\"" + key + "\":1}", doc);
    CHECK(error.code == ds::ErrorCode::E_DUPLICATE_KEY);
    CHECK(std::string(error.pointer) == "/" + key);
}

TEST_CASE("error values preserve embedded zero bytes") {
    ds::Document document;
    auto error = ds::BoundedParse(R"({"a\u0000b":1,"a\u0000b":2})", document);
    CHECK(error.code == ds::ErrorCode::E_DUPLICATE_KEY);
    const auto copy = error;
    CHECK(copy.pointer.View() == std::string_view("/a\0b", 4));
}
