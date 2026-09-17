#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "support/HeapCounter.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
namespace {
// Test-executable replacements observe allocations from every translation unit and worker thread.
constinit std::atomic<std::uint64_t> allocation_count{};
constinit std::atomic<std::uint64_t> live_bytes{};
struct AllocationHeader {
    void *base;
    std::size_t bytes;
};
void *Allocate(std::size_t bytes, std::size_t alignment) noexcept {
    alignment = alignment < alignof(AllocationHeader) ? alignof(AllocationHeader) : alignment;
    const auto payload = bytes ? bytes : 1;
    const auto maximum = std::numeric_limits<std::size_t>::max();
    if (alignment > maximum - sizeof(AllocationHeader) || payload > maximum - sizeof(AllocationHeader) - alignment)
        return nullptr;
    void *base = std::malloc(payload + sizeof(AllocationHeader) + alignment);
    if (!base)
        return nullptr;
    const auto start = reinterpret_cast<std::uintptr_t>(base) + sizeof(AllocationHeader);
    const auto address = (start + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
    auto *header = reinterpret_cast<AllocationHeader *>(address) - 1;
    ::new (static_cast<void *>(header)) AllocationHeader{base, bytes};
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    live_bytes.fetch_add(bytes, std::memory_order_relaxed);
    return reinterpret_cast<void *>(address);
}
void *RequiredAllocate(std::size_t bytes, std::size_t alignment) {
    auto *pointer = Allocate(bytes, alignment);
    if (!pointer)
        std::abort(); // This test executable is compiled without exceptions.
    return pointer;
}
void Deallocate(void *pointer) noexcept {
    if (!pointer)
        return;
    const auto *header = reinterpret_cast<AllocationHeader *>(pointer) - 1;
    live_bytes.fetch_sub(header->bytes, std::memory_order_relaxed);
    std::free(header->base);
}
} // namespace
namespace test_heap {
Counts Read() { return {allocation_count.load(std::memory_order_relaxed), live_bytes.load(std::memory_order_relaxed)}; }
} // namespace test_heap
void *operator new(std::size_t bytes) { return RequiredAllocate(bytes, alignof(std::max_align_t)); }
void *operator new[](std::size_t bytes) { return RequiredAllocate(bytes, alignof(std::max_align_t)); }
void *operator new(std::size_t bytes, const std::nothrow_t &) noexcept {
    return Allocate(bytes, alignof(std::max_align_t));
}
void *operator new[](std::size_t bytes, const std::nothrow_t &) noexcept {
    return Allocate(bytes, alignof(std::max_align_t));
}
void *operator new(std::size_t bytes, std::align_val_t alignment) {
    return RequiredAllocate(bytes, static_cast<std::size_t>(alignment));
}
void *operator new[](std::size_t bytes, std::align_val_t alignment) {
    return RequiredAllocate(bytes, static_cast<std::size_t>(alignment));
}
void *operator new(std::size_t bytes, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return Allocate(bytes, static_cast<std::size_t>(alignment));
}
void *operator new[](std::size_t bytes, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return Allocate(bytes, static_cast<std::size_t>(alignment));
}
void operator delete(void *p) noexcept { Deallocate(p); }
void operator delete[](void *p) noexcept { Deallocate(p); }
void operator delete(void *p, std::size_t) noexcept { Deallocate(p); }
void operator delete[](void *p, std::size_t) noexcept { Deallocate(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { Deallocate(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { Deallocate(p); }
void operator delete(void *p, std::align_val_t) noexcept { Deallocate(p); }
void operator delete[](void *p, std::align_val_t) noexcept { Deallocate(p); }
void operator delete(void *p, std::size_t, std::align_val_t) noexcept { Deallocate(p); }
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept { Deallocate(p); }
void operator delete(void *p, std::align_val_t, const std::nothrow_t &) noexcept { Deallocate(p); }
void operator delete[](void *p, std::align_val_t, const std::nothrow_t &) noexcept { Deallocate(p); }
