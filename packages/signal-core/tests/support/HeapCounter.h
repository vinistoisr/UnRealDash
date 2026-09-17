#pragma once
#include <cstdint>
namespace test_heap {
struct Counts {
    std::uint64_t allocations{};
    std::uint64_t live_bytes{};
};
Counts Read();
} // namespace test_heap
