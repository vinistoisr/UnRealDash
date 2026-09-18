#include "SignalCore/Reconnect.h"
#include "SignalCore/TcpTransport.h"
#include "support/LoopbackListener.h"
#include <doctest.h>
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
