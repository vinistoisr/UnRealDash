#pragma once
#include "SignalCore/RuleEngine.h"
#include <array>
#include <atomic>
#include <barrier>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>
namespace signal_core {
namespace threading_stress {
using namespace std::chrono_literals;
struct FakeClock {
    Time time{};
    Clock Get() {
        return {this, [](void *p) { return static_cast<FakeClock *>(p)->time; }};
    }
};
struct Counts {
    std::uint64_t allocations{}, live_bytes{};
};
struct NullAllocationProbe {
    bool Supported() const { return false; }
    Counts Read() const { return {}; }
};
struct ScenarioResult {
    unsigned checks{}, failures{}, skipped{};
};
template <class Reporter> struct Fixture {
    Reporter &ReportCheck;
    FakeClock clock;
    std::array<Signal, 2> signals{
        {{1, "temperature", Unit::kelvin, 500ms, false}, {2, "pressure", Unit::pascal, 500ms, false}}};
    std::array<SignalSample, 2> a{}, b{}, c{};
    std::array<std::uint8_t, 1> la{}, lb{}, lc{};
    std::array<Expiry, 16> expiries{};
    ExpirySchedule schedule{expiries};
    SnapshotExchange exchange{{a, la, 0}, {b, lb, 0}, {c, lc, 0}};
    SignalRegistry registry{clock.Get(), signals, schedule, exchange};
    explicit Fixture(Reporter &reporter) : ReportCheck(reporter) {
        ReportCheck(registry.Initialize().Ok(), "registry initialize");
    }
    std::span<SignalSample> Storage() { return exchange.WriterBuffer().samples; }
    Sample Make(double value = 300, Time time = 5ms, std::uint64_t seq = 1, std::uint64_t generation = 0) {
        Sample sample(value, Unit::kelvin, time);
        sample.seq = seq;
        sample.generation = generation;
        sample.age_evidence = AgeEvidence::measured;
        return sample;
    }
    void Apply() {
        clock.time = 5ms;
        ReportCheck(registry.Apply(1, Make()).Ok(), "registry apply");
    }
    void Publish(bool latched) {
        const std::uint8_t warning = latched ? 1 : 0;
        ReportCheck(registry.Publish({&warning, 1}).Ok(), "registry publish");
    }
};
template <class Reporter, class ThreadLauncher, class AllocationProbe>
ScenarioResult RunScenario(unsigned index, Reporter reporter, ThreadLauncher &launcher, AllocationProbe &probe) {
    ScenarioResult result{};
    const auto ReportCheck = [&](bool passed, const char *name) {
        ++result.checks;
        if (!passed)
            ++result.failures;
        reporter(passed, name);
    };
    switch (index) {
    case 0: {
        std::array<SignalSample, 2> a{}, b{}, c{};
        std::array<Expiry, 2> expiries{};
        FakeClock clock;
        const Signal signals[] = {{1, "a", Unit::kelvin, 500ms, false}, {2, "b", Unit::kelvin, 500ms, false}};
        ExpirySchedule schedule(expiries);
        SnapshotExchange exchange({a, {}, 0}, {b, {}, 0}, {c, {}, 0});
        SignalRegistry registry(clock.Get(), signals, schedule, exchange);
        ReportCheck(registry.Initialize().Ok(), "registry.Initialize().Ok()");
        std::atomic<bool> done = false, good = true, run = false, release = false;
        std::atomic<unsigned> ready{}, finished{};
        std::barrier warmup(2);
        auto writer = launcher.Launch("writer", [&] {
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
        auto reader = launcher.Launch("reader", [&] {
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
        const auto before = probe.Read();
        run.store(true, std::memory_order_release);
        while (finished.load(std::memory_order_acquire) != 2)
            std::this_thread::yield();
        const auto after = probe.Read();
        release.store(true, std::memory_order_release);
        writer.Join();
        reader.Join();
        ReportCheck(good.load(), "good.load()");
        if (probe.Supported())
            ReportCheck(after.allocations == before.allocations, "sustained allocation count");
        else {
            ++result.skipped;
            std::printf("skipped: sustained allocation count\n");
        }
        if (probe.Supported())
            ReportCheck(after.live_bytes == before.live_bytes, "sustained live bytes");
        else {
            ++result.skipped;
            std::printf("skipped: sustained live bytes\n");
        }

        break;
    }
    case 1: {
        Fixture f(ReportCheck);
        f.Apply();
        f.Publish(true);
        std::barrier held(2), finished(2);
        std::atomic<bool> good = true;
        auto reader = launcher.Launch("reader", [&] {
            auto snapshot = f.exchange.Acquire();
            std::array<std::byte, sizeof(SignalSample) * 2> before{};
            std::memcpy(before.data(), snapshot.samples.data(), before.size());
            held.arrive_and_wait();
            finished.arrive_and_wait();
            if (std::memcmp(before.data(), snapshot.samples.data(), before.size()) != 0 || snapshot.latched[0] != 1)
                good = false;
        });
        auto writer = launcher.Launch("writer", [&] {
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
        writer.Join();
        reader.Join();
        ReportCheck(good.load(), "good.load()");

        break;
    }
    case 2: {
        Fixture f(ReportCheck);
        std::barrier published(2), finished(2);
        std::atomic<bool> good = true;
        auto writer = launcher.Launch("writer", [&] {
            auto sample = f.Make();
            const std::uint8_t warning = 1;
            if (!f.registry.Apply(1, sample).Ok() || !f.registry.Publish({&warning, 1}).Ok())
                good = false;
            published.arrive_and_wait();
            finished.arrive_and_wait();
        });
        auto reader = launcher.Launch("reader", [&] {
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
        writer.Join();
        reader.Join();
        ReportCheck(good.load(), "good.load()");

        break;
    }
    case 3: {
        Fixture f(ReportCheck);
        std::array<SignalSample, 64> entries{};
        SampleQueue queue(entries);
        std::barrier queued(2);
        std::atomic<bool> good = true;
        auto reader = launcher.Launch("reader", [&] {
            for (std::uint64_t i = 0; i < 64; ++i)
                if (!queue.Push({1, f.Make(999, Time(5000000 + static_cast<std::int64_t>(i)), i + 1, 0)}))
                    good = false;
            queued.arrive_and_wait();
        });
        auto writer = launcher.Launch("writer", [&] {
            if (!f.registry.SetGeneration(1).Ok())
                good = false;
            queued.arrive_and_wait();
            SignalSample entry;
            while (queue.Pop(entry))
                if (f.registry.Apply(entry.signal, entry.sample).code != ErrorCode::old_generation)
                    good = false;
        });
        writer.Join();
        reader.Join();
        ReportCheck(good.load(), "good.load()");
        ReportCheck(f.registry.RejectedGenerations() == 64, "f.registry.RejectedGenerations() == 64");
        ReportCheck(f.Storage()[0].sample.quality == Quality::unavailable,
                    "f.Storage()[0].sample.quality == Quality::unavailable");

        break;
    }
    case 4: {
        std::array<SignalSample, 256> entries{};
        SampleQueue queue(entries);
        std::barrier full(2);
        std::atomic<bool> good = true;
        std::uint64_t consumed = 0;
        auto writer = launcher.Launch("writer", [&] {
            for (int i = 0; i < 1000; ++i) {
                queue.Push({});
                if (queue.Depth() > 256)
                    good = false;
            }
            full.arrive_and_wait();
        });
        auto reader = launcher.Launch("reader", [&] {
            full.arrive_and_wait();
            SignalSample entry;
            while (queue.Pop(entry))
                ++consumed;
        });
        writer.Join();
        reader.Join();
        ReportCheck(good.load(), "good.load()");
        ReportCheck(consumed == 256, "consumed == 256");
        ReportCheck(queue.Depth() == 0, "queue.Depth() == 0");
        ReportCheck(queue.Drops() == 1000 - consumed, "queue.Drops() == 1000 - consumed");

        break;
    }
    default:
        ReportCheck(false, "invalid scenario index");
        break;
    }
    std::printf("scenario=%u checks=%u failures=%u skipped=%u\n", index, result.checks, result.failures,
                result.skipped);
    return result;
}
} // namespace threading_stress
} // namespace signal_core
