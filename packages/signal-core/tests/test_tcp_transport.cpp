#include "SignalCore/Reconnect.h"
#include "SignalCore/TcpTransport.h"
#include "support/LoopbackListener.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <doctest.h>
#include <thread>
#include <vector>

using namespace signal_core;
using namespace std::chrono_literals;

namespace {
// Drives a non-blocking connect to completion the way the supervisor does, by asking again rather
// than by waiting. Bounded, so a broken connect fails the test instead of hanging it.
bool Settle(TcpTransport &transport, LoopbackListener &listener, int rounds = 200) {
    for (int i = 0; i < rounds; ++i) {
        const auto status = transport.Connect();
        listener.Accept();
        if (status.Ok() && transport.Connected())
            return listener.Accept();
        if (!status.Ok() && status.code != ErrorCode::need_more_data)
            return false;
    }
    return false;
}
} // namespace

TEST_CASE("4.9 the tcp transport connects, reads, and sends nothing") {
    LoopbackListener listener;
    REQUIRE(listener.Open());

    TcpTransport transport;
    REQUIRE(transport.Configure("127.0.0.1", listener.port).Ok());
    REQUIRE_FALSE(transport.Connected());
    REQUIRE(Settle(transport, listener));
    CHECK(transport.Connected());

    const std::vector<std::uint8_t> payload{1, 2, 3, 4, 5, 6, 7, 8};
    REQUIRE(listener.Send(payload));

    std::array<std::uint8_t, 64> buffer{};
    std::size_t written = 0;
    bool got = false;
    for (int i = 0; i < 200 && !got; ++i) {
        const auto status = transport.Read(buffer, written);
        if (written) {
            got = true;
            CHECK(written == payload.size());
            CHECK(std::equal(payload.begin(), payload.end(), buffer.begin()));
        } else {
            CHECK(status.code == ErrorCode::need_more_data);
        }
    }
    CHECK(got);

    // The measurement PLAN 4.9 asks for, taken at the socket rather than asserted about the code:
    // after a full connect and read session the server has received nothing at all.
    CHECK(listener.Received() == 0);
    CHECK(transport.BytesSent() == 0);
}

TEST_CASE("4.9 a peer that goes away is reported as disconnected, not as quiet") {
    LoopbackListener listener;
    REQUIRE(listener.Open());
    TcpTransport transport;
    REQUIRE(transport.Configure("127.0.0.1", listener.port).Ok());
    REQUIRE(Settle(transport, listener));
    REQUIRE(transport.Connected());

    listener.DropClient();

    // Killing the relay must land as a disconnect. Reported as "no data yet" instead, every mapped
    // signal would sit valid forever on a socket that is never going to speak again, which is the
    // failure PLAN 4.9's stale-on-kill criterion exists to catch.
    std::array<std::uint8_t, 64> buffer{};
    std::size_t written = 0;
    bool disconnected = false;
    for (int i = 0; i < 500 && !disconnected; ++i) {
        const auto status = transport.Read(buffer, written);
        if (status.code == ErrorCode::io_error)
            disconnected = true;
    }
    CHECK(disconnected);
    CHECK_FALSE(transport.Connected());
}

TEST_CASE("4.9 connecting to a closed port never blocks and never sends") {
    // A listener opened only to borrow a port, then closed, so the port is almost certainly unused.
    std::uint16_t port = 0;
    {
        LoopbackListener probe;
        REQUIRE(probe.Open());
        port = probe.port;
    }

    TcpTransport transport;
    REQUIRE(transport.Configure("127.0.0.1", port).Ok());

    // The supervisor's shape: attempt, be told it did not work, wait out the backoff. What matters
    // here is that each attempt returns immediately rather than parking on a connect timeout.
    ConnectionSupervisor supervisor;
    Time now{};
    int attempts = 0;
    for (int poll = 0; poll < 2000; ++poll) {
        now = std::chrono::milliseconds(poll * 10);
        if (supervisor.Poll(now, transport.Connected()) != ConnectionSupervisor::Action::reconnect)
            continue;
        ++attempts;
        (void)transport.Disconnect();
        const auto status = transport.Connect();
        supervisor.Attempted(now, status.Ok() && transport.Connected());
    }
    CHECK_FALSE(transport.Connected());
    // Twenty seconds of simulated time: one immediate attempt, then 500, 1000, 2000, 4000 and then
    // every 5000 ms, which is 7 by 20 s. The exact schedule is pinned in test_reconnect.cpp; what
    // this adds is that a real socket against a dead port keeps to it.
    CHECK(attempts == 7);
    CHECK(transport.BytesSent() == 0);
}

// PLAN 4.9 criterion 7, the wall-clock confirmation of the schedule the unit tests pin.
//
// Skipped by default: it takes a real minute, and a suite people avoid running is worse than one
// that covers less. Run it with --no-skip, or --test-case="4.9 wall clock*" --no-skip.
//
// What it adds over test_reconnect.cpp is that a REAL non-blocking connect against a dead port
// returns promptly enough for the schedule to hold in real time, and that the process is asleep
// between attempts rather than spinning. The unit tests prove the policy; this proves the socket
// does not get in its way.
TEST_CASE("4.9 wall clock backoff against an absent relay" * doctest::skip()) {
    std::uint16_t port = 0;
    {
        LoopbackListener probe;
        REQUIRE(probe.Open());
        port = probe.port;
    }
    TcpTransport transport;
    REQUIRE(transport.Configure("127.0.0.1", port).Ok());

    const auto ProcessCpuSeconds = []() -> double {
#if defined(_WIN32)
        FILETIME creation{}, exit{}, kernel{}, user{};
        if (!::GetProcessTimes(::GetCurrentProcess(), &creation, &exit, &kernel, &user))
            return -1.0;
        const auto ToSeconds = [](const FILETIME &t) {
            return ((static_cast<std::uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 1e7;
        };
        return ToSeconds(kernel) + ToSeconds(user);
#else
        return static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
#endif
    };

    ConnectionSupervisor supervisor;
    const auto start = std::chrono::steady_clock::now();
    const double cpu_start = ProcessCpuSeconds();
    std::vector<double> attempt_seconds;

    for (;;) {
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= 60s)
            break;
        const Time now = std::chrono::duration_cast<Time>(elapsed);
        if (supervisor.Poll(now, transport.Connected()) == ConnectionSupervisor::Action::reconnect) {
            attempt_seconds.push_back(std::chrono::duration<double>(elapsed).count());
            (void)transport.Disconnect();
            const auto status = transport.Connect();
            supervisor.Attempted(now, status.Ok() && transport.Connected());
        }
        // The poll cadence an engine tick would give it. Nothing here busy waits, which is the
        // other half of what this criterion measures.
        std::this_thread::sleep_for(10ms);
    }
    const double cpu = ProcessCpuSeconds() - cpu_start;

    const std::vector<double> expected{0, 0.5, 1.5, 3.5, 7.5, 12.5, 17.5, 22.5,
                                       27.5, 32.5, 37.5, 42.5, 47.5, 52.5, 57.5};
    MESSAGE("attempts: " << attempt_seconds.size() << ", cpu seconds: " << cpu);
    REQUIRE(attempt_seconds.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        MESSAGE("attempt " << i << " at " << attempt_seconds[i] << " s, expected " << expected[i]);
        // 100 ms, which is ten poll periods. A real clock and a real socket cannot land on the
        // simulated timestamps exactly, and a band an order of magnitude under the shortest
        // interval still catches a schedule that is wrong rather than merely jittery.
        CHECK(std::abs(attempt_seconds[i] - expected[i]) < 0.1);
    }
    // Under one percent of a core over the minute. An absent relay must cost nothing.
    CHECK(cpu < 0.6);
    CHECK(transport.BytesSent() == 0);
}
