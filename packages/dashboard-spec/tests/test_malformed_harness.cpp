#include "TestSupport.h"
#include <chrono>
#include <iostream>
#include <random>
TEST_CASE("malformed document harness is bounded") {
    ds::Validator validator(DASHBOARD_SCHEMA_DIR);
    std::vector<std::string> seeds;
    for (const auto &entry : std::filesystem::directory_iterator(Repo() / "tests/fixtures/malformed-json"))
        if (entry.path().extension() == ".json")
            seeds.push_back(Read(entry.path()));
    REQUIRE(!seeds.empty());
    std::mt19937 random(20260916);
    const auto start = std::chrono::steady_clock::now();
    std::size_t count = 0;
    while (count < 1000000 && std::chrono::steady_clock::now() - start < std::chrono::seconds(30)) {
        auto input = seeds[count % seeds.size()];
        if (count % 3 == 0) {
            input.assign(random() % 512, '[');
            input += '0';
        } else if (!input.empty())
            input[random() % input.size()] = static_cast<char>(random() % 256);
        const auto error = validator.ValidateText(input);
        CHECK((error.Ok() || std::string(ds::CodeName(error.code)) != ""));
        ++count;
    }
    std::cout << "Malformed harness inputs: " << count << "\n";
    CHECK(count > 0);
}
