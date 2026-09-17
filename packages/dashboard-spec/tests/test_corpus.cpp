#include "TestSupport.h"
TEST_CASE("every document agrees with its expected code and pointer") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    std::size_t valid = 0, invalid = 0;
    for (const char *side : {"valid", "invalid"})
        for (const auto &entry : std::filesystem::directory_iterator(Repo() / "tests/fixtures/documents" / side)) {
            if (entry.path().extension() != ".json")
                continue;
            INFO(entry.path().generic_string());
            auto error = validator.ValidateText(Read(entry.path()));
            if (std::string(side) == "valid") {
                ++valid;
                CHECK_MESSAGE(error.Ok(), ds::CodeName(error.code), " ", error.pointer, " ", error.message);
            } else {
                ++invalid;
                auto path = entry.path();
                path.replace_extension(".expected");
                std::ifstream expected(path);
                std::string code, pointer;
                std::getline(expected, code);
                std::getline(expected, pointer);
                CHECK(code == ds::CodeName(error.code));
                CHECK(pointer == error.pointer.c_str());
            }
        }
    CHECK(valid >= 20);
    CHECK(invalid >= 30);
}
