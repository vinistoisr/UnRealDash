#include "support/FakeClock.h"
#include "support/HeapCounter.h"
#include <atomic>
#include <barrier>
#include <cstring>
#include <new>
#include <random>
#include <thread>
TEST_CASE("2.13 sustained publication with a continuously swapping reader") {
    std::array<SignalSample, 2> a{}, b{}, c{};
    std::array<Expiry, 2> expiries{};
    FakeClock clock;
    const Signal signals[] = {{1, "a", Unit::kelvin, 500ms, false}, {2, "b", Unit::kelvin, 500ms, false}};
    ExpirySchedule schedule(expiries);
    SnapshotExchange exchange({a, {}, 0}, {b, {}, 0}, {c, {}, 0});
    SignalRegistry registry(clock.Get(), signals, schedule, exchange);
    REQUIRE(registry.Initialize().Ok());
    std::atomic<bool> done = false, good = true, run = false, release = false;
    std::atomic<unsigned> ready{}, finished{};
    std::barrier warmup(2);
    std::thread writer([&] {
        std::mt19937_64 random(1234);
        Sample initial(0, Unit::kelvin, 1ns);
        initial.seq = 1;
        if (!registry.Apply(1, initial).Ok() || !registry.Apply(2, initial).Ok() || !registry.Publish().Ok())
            good = false;
        warmup.arrive_and_wait();
        ready.fetch_add(1, std::memory_order_release);
        while (!run.load(std::memory_order_acquire))
            std::this_thread::yield();
        for (std::uint64_t i = 2; i <= 200001; ++i) {
            Sample sample(static_cast<double>(random() >> 11) * 0x1.0p-53, Unit::kelvin,
                          Time(static_cast<std::int64_t>(i)));
            sample.seq = i;
            clock.time = sample.t_recv;
            if (!registry.Apply(1, sample).Ok() || !registry.Apply(2, sample).Ok() || !registry.Publish().Ok())
                good = false;
        }
        done.store(true, std::memory_order_release);
        finished.fetch_add(1, std::memory_order_release);
        while (!release.load(std::memory_order_acquire))
            std::this_thread::yield();
    });
    std::thread reader([&] {
        warmup.arrive_and_wait();
        if (exchange.Acquire().version != 1)
            good = false;
        ready.fetch_add(1, std::memory_order_release);
        while (!run.load(std::memory_order_acquire))
            std::this_thread::yield();
        std::uint64_t prior = 1;
        do {
            auto snapshot = exchange.Acquire();
            if (snapshot.version < prior)
                good = false;
            prior = snapshot.version;
            if (snapshot.samples[0].sample.seq != snapshot.samples[1].sample.seq ||
                snapshot.samples[0].sample.value != snapshot.samples[1].sample.value)
                good = false;
        } while (!done.load(std::memory_order_acquire));
        if (exchange.Acquire().version != 200001)
            good = false;
        finished.fetch_add(1, std::memory_order_release);
        while (!release.load(std::memory_order_acquire))
            std::this_thread::yield();
    });
    // Workers and their runtime state are live before counting. No doctest call occurs in this window.
    while (ready.load(std::memory_order_acquire) != 2)
        std::this_thread::yield();
    const auto before = test_heap::Read();
    run.store(true, std::memory_order_release);
    while (finished.load(std::memory_order_acquire) != 2)
        std::this_thread::yield();
    const auto after = test_heap::Read();
    release.store(true, std::memory_order_release);
    writer.join();
    reader.join();
    CHECK(good.load());
    CHECK(after.allocations == before.allocations);
    CHECK(after.live_bytes == before.live_bytes);
    CHECK(exchange.WriterBlockedCount() == 0);
}
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
TEST_CASE("2.13 stalled reader holds a snapshot across 1000 publications") {
    Fixture f;
    f.Apply();
    f.Publish(true);
    std::barrier held(2), finished(2);
    std::atomic<bool> good = true;
    std::thread reader([&] {
        auto snapshot = f.exchange.Acquire();
        std::array<std::byte, sizeof(SignalSample) * 2> before{};
        std::memcpy(before.data(), snapshot.samples.data(), before.size());
        held.arrive_and_wait();
        finished.arrive_and_wait();
        if (std::memcmp(before.data(), snapshot.samples.data(), before.size()) != 0 || snapshot.latched[0] != 1)
            good = false;
    });
    std::thread writer([&] {
        held.arrive_and_wait();
        for (std::uint64_t i = 0; i < 1000; ++i) {
            auto sample = f.Make(static_cast<double>(i), Time(6000000 + static_cast<std::int64_t>(i)), i + 2);
            f.clock.time = sample.t_recv;
            const std::uint8_t warning = 0;
            if (!f.registry.Apply(1, sample).Ok() || !f.registry.Publish({&warning, 1}).Ok())
                good = false;
        }
        finished.arrive_and_wait();
    });
    writer.join();
    reader.join();
    CHECK(good.load());
    CHECK(f.exchange.WriterBlockedCount() == 0);
}
TEST_CASE("2.13 repeated acquires with the writer idle") {
    Fixture f;
    std::barrier published(2), finished(2);
    std::atomic<bool> good = true;
    std::thread writer([&] {
        auto sample = f.Make();
        const std::uint8_t warning = 1;
        if (!f.registry.Apply(1, sample).Ok() || !f.registry.Publish({&warning, 1}).Ok())
            good = false;
        published.arrive_and_wait();
        finished.arrive_and_wait();
    });
    std::thread reader([&] {
        published.arrive_and_wait();
        std::uint64_t previous = 0;
        for (int i = 0; i < 1000; ++i) {
            auto snapshot = f.exchange.Acquire();
            if (snapshot.version < previous || (i > 0 && snapshot.new_publication) ||
                snapshot.samples[0].sample.value != 300 || snapshot.latched[0] != 1)
                good = false;
            previous = snapshot.version;
        }
        finished.arrive_and_wait();
    });
    writer.join();
    reader.join();
    CHECK(good.load());
}
TEST_CASE("2.13 generation change mid stream rejects in-flight older samples") {
    Fixture f;
    std::array<SignalSample, 64> entries{};
    SampleQueue queue(entries);
    std::barrier queued(2);
    std::atomic<bool> good = true;
    std::thread reader([&] {
        for (std::uint64_t i = 0; i < 64; ++i)
            if (!queue.Push({1, f.Make(999, Time(5000000 + static_cast<std::int64_t>(i)), i + 1, 0)}))
                good = false;
        queued.arrive_and_wait();
    });
    std::thread writer([&] {
        if (!f.registry.SetGeneration(1).Ok())
            good = false;
        queued.arrive_and_wait();
        SignalSample entry;
        while (queue.Pop(entry))
            if (f.registry.Apply(entry.signal, entry.sample).code != ErrorCode::old_generation)
                good = false;
    });
    writer.join();
    reader.join();
    CHECK(good.load());
    CHECK(f.registry.RejectedGenerations() == 64);
    CHECK(f.Storage()[0].sample.quality == Quality::unavailable);
}
TEST_CASE("2.13 bounded queue above capacity drops and counts") {
    std::array<SignalSample, 256> entries{};
    SampleQueue queue(entries);
    std::barrier full(2);
    std::atomic<bool> good = true;
    std::uint64_t consumed = 0;
    std::thread writer([&] {
        for (int i = 0; i < 1000; ++i) {
            queue.Push({});
            if (queue.Depth() > 256)
                good = false;
        }
        full.arrive_and_wait();
    });
    std::thread reader([&] {
        full.arrive_and_wait();
        SignalSample entry;
        while (queue.Pop(entry))
            ++consumed;
    });
    writer.join();
    reader.join();
    CHECK(good.load());
    CHECK(consumed == 256);
    CHECK(queue.Depth() == 0);
    CHECK(queue.Drops() == 1000 - consumed);
}
