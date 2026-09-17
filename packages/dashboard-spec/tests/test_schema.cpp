#include "TestSupport.h"
#include <regex>
TEST_CASE("error vocabulary and stable values agree with Python") {
    const auto python = Read(Repo() / "tools/dashboard_spec/errors.py");
    const auto header = Read(Repo() / "packages/dashboard-spec/include/dashboard_spec/Errors.h");
    std::regex expression("E_[A-Z_]+");
    std::vector<std::string> left, right;
    for (auto it = std::sregex_iterator(python.begin(), python.end(), expression); it != std::sregex_iterator(); ++it)
        left.push_back(it->str());
    for (auto it = std::sregex_iterator(header.begin(), header.end(), expression); it != std::sregex_iterator(); ++it)
        right.push_back(it->str());
    CHECK(left == right);
    std::regex cpp_values("(E_[A-Z_]+) = ([0-9]+)");
    std::regex python_values("\"(E_[A-Z_]+)\": ([0-9]+)");
    std::map<std::string, int> cpp, py;
    for (auto it = std::sregex_iterator(header.begin(), header.end(), cpp_values); it != std::sregex_iterator(); ++it) {
        const auto value = std::stoi((*it)[2].str());
        cpp[(*it)[1].str()] = value;
        CHECK((*it)[1].str() == ds::CodeName(static_cast<ds::ErrorCode>(value)));
    }
    for (auto it = std::sregex_iterator(python.begin(), python.end(), python_values); it != std::sregex_iterator();
         ++it)
        py[(*it)[1].str()] = std::stoi((*it)[2].str());
    CHECK(cpp == py);
}
TEST_CASE("schema rejects extra fields and unknown operations") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    CHECK(validator.ValidateText(Fixture("invalid/bit-field.json")).code == ds::ErrorCode::E_SCHEMA);
    CHECK(validator.ValidateText(Fixture("invalid/unknown-operation.json")).code == ds::ErrorCode::E_SCHEMA);
    CHECK(validator.ValidateText(Fixture("valid/nine-primitives.json")).Ok());
}
