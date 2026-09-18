#include "SignalCore/Reconnect.h"
#include "support/FakeClock.h"

// PLAN 4.9 criteria 1 and 2. Both are unit tests over an injected clock rather than wall-clock runs:
// Clock is a function pointer and a context, so a test advances time freely and the whole schedule
// is measured in microseconds. The sixty second wall-clock run in the gate script confirms this
// rather than being the proof, because a proof nobody runs twice is not one.

TEST_CASE("4.9 the backoff schedule is 500 ms doubling to a 5 s ceiling") {
    BackoffPolicy policy;
    Time now{};

    // The first attempt is due immediately. A connector that waited 500 ms before trying at all
    // would take half a second to notice a relay that was there the whole time.
    CHECK(policy.Due(now));
    CHECK(policy.Attempts() == 0);

    const std::array<std::int64_t, 8> expected{500, 1000, 2000, 4000, 5000, 5000, 5000, 5000};
    std::vector<std::int64_t> measured;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        // The interval recorded is the one this failure schedules: the wait before attempt i + 1.
        const Time before = policy.Interval();
        policy.Failed(now);
        measured.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(before).count());

        // Not due one millisecond early, due exactly on time. Both halves matter: a schedule that
        // is always due is the spin loop, and one that is never due is a link that never returns.
        CHECK_FALSE(policy.Due(policy.NextAttemptAt() - 1ms));
        CHECK(policy.Due(policy.NextAttemptAt()));
        now = policy.NextAttemptAt();
    }
    CHECK(measured == std::vector<std::int64_t>(expected.begin(), expected.end()));
    CHECK(policy.Attempts() == expected.size());
}

TEST_CASE("4.9 a successful connection resets the schedule to the first interval") {
    BackoffPolicy policy;
    Time now{};
    for (int i = 0; i < 6; ++i) {
        policy.Failed(now);
        now = policy.NextAttemptAt();
    }
    CHECK(policy.Interval() == BackoffPolicy::ceiling);
    CHECK(policy.Attempts() == 6);

    policy.Succeeded();
    CHECK(policy.Interval() == BackoffPolicy::first);
    CHECK(policy.Attempts() == 0);
    // A link that drops once a minute must not creep up to the ceiling and stay there.
    CHECK(policy.Due(now));
}

TEST_CASE("4.9 an absent relay costs the scheduled number of attempts and no more") {
    // Sixty seconds of simulated time, polled every 10 ms, which is 6000 polls. The schedule after
    // the immediate first attempt is 0.5, 1, 2, 4, then 5 s forever, so the attempt times are
    // 0, 0.5, 1.5, 3.5, 7.5, 12.5, 17.5, ... 57.5 s. That is 5 attempts in the first 7.5 seconds
    // and one every 5 seconds after, so 5 + 11 = 16 by 60 seconds.
    ConnectionSupervisor supervisor;
    Time now{};
    std::vector<std::int64_t> attempt_ms;
    std::uint64_t waits = 0;

    for (int poll = 0; poll <= 6000; ++poll) {
        now = std::chrono::milliseconds(poll * 10);
        const auto action = supervisor.Poll(now, /*connected=*/false);
        if (action == ConnectionSupervisor::Action::reconnect) {
            attempt_ms.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
            supervisor.Attempted(now, /*succeeded=*/false);
        } else if (action == ConnectionSupervisor::Action::waiting) {
            ++waits;
        }
    }

    const std::vector<std::int64_t> expected{0, 500, 1500, 3500, 7500, 12500, 17500, 22500,
                                             27500, 32500, 37500, 42500, 47500, 52500, 57500};
    CHECK(attempt_ms == expected);
    // The point of the criterion: 6001 polls produced 15 connect attempts, so the other 5986 did
    // nothing at all. An absent relay is a schedule, not a spin loop.
    CHECK(waits == 6001 - expected.size());

    // And the interval really did reach the ceiling rather than stopping short.
    CHECK(supervisor.Policy().Interval() == BackoffPolicy::ceiling);
}

TEST_CASE("4.9 a connected supervisor asks for nothing") {
    ConnectionSupervisor supervisor;
    for (int poll = 0; poll < 1000; ++poll)
        CHECK(supervisor.Poll(std::chrono::milliseconds(poll * 10), /*connected=*/true)
              == ConnectionSupervisor::Action::none);
    CHECK(supervisor.AttemptsSinceConnect() == 0);
}

TEST_CASE("4.9 health reports the backoff state") {
    ConnectionSupervisor supervisor;
    ConnectionHealth health{};

    ReportBackoff(supervisor, health);
    CHECK(health.backoff_ms == 500);
    CHECK(health.attempts_since_connect == 0);

    Time now{};
    for (int i = 0; i < 3; ++i) {
        supervisor.Attempted(now, false);
        now = supervisor.Policy().NextAttemptAt();
    }
    ReportBackoff(supervisor, health);
    CHECK(health.backoff_ms == 4000);
    CHECK(health.attempts_since_connect == 3);

    supervisor.Attempted(now, true);
    ReportBackoff(supervisor, health);
    CHECK(health.backoff_ms == 500);
    CHECK(health.attempts_since_connect == 0);
}
