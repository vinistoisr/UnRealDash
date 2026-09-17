#include "SignalCore/Scenarios.h"
#include <charconv>
#include <cstdio>
#include <string_view>
using namespace signal_core;
namespace {
Status Write(void *context, std::string_view text) {
    auto *file = static_cast<std::FILE *>(context);
    if (std::fwrite(text.data(), 1, text.size(), file) != text.size())
        return Error(ErrorCode::io_error, "output write failed");
    return {};
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 5) {
        std::fprintf(stderr, "usage: signal-core-scenario <scenario-name> <seed> <duration-seconds> <output-path>\n");
        return 1;
    }
    auto scenario = ParseScenario(argv[1]);
    std::uint64_t seed{};
    std::int64_t duration{};
    const std::string_view seed_text(argv[2]), duration_text(argv[3]);
    auto a = std::from_chars(seed_text.data(), seed_text.data() + seed_text.size(), seed);
    auto b = std::from_chars(duration_text.data(), duration_text.data() + duration_text.size(), duration);
    if (!scenario.Ok() || a.ec != std::errc{} || a.ptr != seed_text.data() + seed_text.size() || b.ec != std::errc{} ||
        b.ptr != duration_text.data() + duration_text.size() || duration <= 0 ||
        duration > Time::max().count() / 1000000000) {
        std::fprintf(stderr, "invalid scenario, seed or duration\n");
        return 1;
    }
    auto *file = std::fopen(argv[4], "wb");
    if (!file) {
        std::fprintf(stderr, "cannot open %s\n", argv[4]);
        return 1;
    }
    auto status = GenerateScenario({*scenario.Get(), seed, std::chrono::seconds(duration),
                                    std::chrono::milliseconds(50), std::chrono::milliseconds(500)},
                                   {file, Write});
    const auto closed = std::fclose(file);
    if (!status.Ok() || closed != 0) {
        std::fprintf(stderr, "%s: %s\n", argv[4], status.message);
        return 1;
    }
    return 0;
}
