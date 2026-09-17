#include "SignalCore/ThreadingStressSuite.h"
#include "support/FakeClock.h"
#include "support/HeapCounter.h"
#include <new>
#include <thread>
namespace {
struct Launcher {
    struct Handle {
        std::thread thread;
        void Join() { thread.join(); }
    };
    template <class Callable> Handle Launch(const char *, Callable callable) {
        return {std::thread(std::move(callable))};
    }
};
struct Probe {
    bool Supported() const { return true; }
    auto Read() const { return test_heap::Read(); }
};
void Run(unsigned index) {
    Launcher launcher;
    Probe probe;
    signal_core::threading_stress::RunScenario(
        index,
        [](bool passed, const char *name) {
            INFO(name);
            CHECK(passed);
        },
        launcher, probe);
}
} // namespace
TEST_CASE("2.13 sustained publication with a continuously swapping reader") { Run(0); }
TEST_CASE("review 7 heap counter observes scalar array aligned and nothrow allocations") {
    const auto before = test_heap::Read();
    void *scalar = ::operator new(17);
    void *array = ::operator new[](23, std::nothrow);
    void *aligned = ::operator new(64, std::align_val_t{64});
    void *aligned_array = ::operator new[](128, std::align_val_t{64}, std::nothrow);
    const auto allocated = test_heap::Read();
    const bool aligned_correctly = reinterpret_cast<std::uintptr_t>(aligned) % 64 == 0 &&
                                   reinterpret_cast<std::uintptr_t>(aligned_array) % 64 == 0;
    ::operator delete(scalar);
    ::operator delete[](array, std::nothrow);
    ::operator delete(aligned, std::size_t{64}, std::align_val_t{64});
    ::operator delete[](aligned_array, std::align_val_t{64}, std::nothrow);
    const auto after = test_heap::Read();
    CHECK(allocated.allocations == before.allocations + 4);
    CHECK(allocated.live_bytes == before.live_bytes + 17 + 23 + 64 + 128);
    CHECK(after.live_bytes == before.live_bytes);
    CHECK(aligned_correctly);
}
TEST_CASE("2.13 stalled reader holds a snapshot across 1000 publications") { Run(1); }
TEST_CASE("2.13 repeated acquires with the writer idle") { Run(2); }
TEST_CASE("2.13 generation change mid stream rejects in-flight older samples") { Run(3); }
TEST_CASE("2.13 bounded queue above capacity drops and counts") { Run(4); }
