#include "SignalCore/EventLog.h"
#include <array>
#include <cstdio>
#include <doctest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace signal_core;

namespace {
// A clock the test moves by hand, because the thing being checked is that a flush happens on the
// second and a real clock would make that a race.
struct FakeClock {
    Time now{};
    static Time Read(void *context) { return static_cast<FakeClock *>(context)->now; }
    Clock Handle() { return Clock{this, &Read}; }
};

struct TempPath {
    std::filesystem::path path;
    std::string text;
    explicit TempPath(const std::string &name) {
        path = std::filesystem::temp_directory_path() / name;
        text = path.string();
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    ~TempPath() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    std::vector<std::string> Lines() const {
        std::vector<std::string> lines;
        std::ifstream in(path, std::ios::binary);
        std::string line;
        while (std::getline(in, line))
            lines.push_back(line);
        return lines;
    }
    std::uintmax_t Size() const {
        std::error_code ignored;
        const auto size = std::filesystem::file_size(path, ignored);
        return ignored ? 0 : size;
    }
};

std::string Build(std::size_t capacity, void (*fill)(RowBuilder &), bool &ok) {
    std::vector<char> storage(capacity);
    RowBuilder row(std::span<char>(storage.data(), storage.size()));
    fill(row);
    ok = row.Ok();
    return std::string(row.View().data(), row.View().size());
}
} // namespace

TEST_CASE("RowBuilder writes a record a reader can parse") {
    std::array<char, 256> storage{};
    RowBuilder row(storage);
    row.Begin("frame")
        .Key("frame")
        .Unsigned(42)
        .Key("t_start")
        .Integer(1234567890)
        .Key("frame_ms")
        .Number(16.5)
        .Key("missed")
        .Boolean(true)
        .End();
    REQUIRE(row.Ok());
    const std::string text(row.View().data(), row.View().size());
    CHECK(text == R"({"type":"frame","frame":42,"t_start":1234567890,"frame_ms":16.5,"missed":true})");
}

TEST_CASE("RowBuilder escapes what would otherwise break the line") {
    std::array<char, 256> storage{};
    RowBuilder row(storage);
    // Every Windows path this log records is full of backslashes, and one unescaped backslash
    // makes the line unparseable rather than merely ugly.
    row.Begin("header").Key("path").String("C:\\Saved\\a \"b\"\nc\t").End();
    REQUIRE(row.Ok());
    const std::string text(row.View().data(), row.View().size());
    // Built outside the macro: MSVC's preprocessor mangles a raw string passed as a
    // macro argument, and this is the only expectation here containing a backslash.
    const std::string expected = R"("C:\\Saved\\a \"b\"\nc\t")";
    CHECK(text.find(expected) != std::string::npos);
    CHECK(text.find('\n') == std::string::npos);
}

TEST_CASE("RowBuilder writes null rather than a token no reader accepts") {
    std::array<char, 128> storage{};
    RowBuilder row(storage);
    // volatile so the compiler cannot fold it: a constant 0.0/0.0 is a compile error, not a NaN.
    volatile double zero = 0.0;
    const double nan = zero / zero;
    const double infinity = 1.0 / zero;
    row.Begin("frame").Key("a").Number(nan).Key("b").Number(infinity).End();
    REQUIRE(row.Ok());
    const std::string text(row.View().data(), row.View().size());
    CHECK(text == R"({"type":"frame","a":null,"b":null})");
}

TEST_CASE("RowBuilder overflow is sticky and never yields a partial row") {
    bool ok = true;
    // Far too small on purpose. The failure that matters is a half-written line in the file, so
    // the builder has to refuse the whole row rather than return what fitted.
    const std::string text = Build(16, [](RowBuilder &row) {
        row.Begin("frame").Key("frame").Unsigned(1).Key("padding").String("aaaaaaaaaaaaaaaaaaaaaaaa").End();
    }, ok);
    CHECK_FALSE(ok);
    // And it stays false even though the last call would have fitted on its own.
    std::array<char, 8> storage{};
    RowBuilder row(storage);
    row.Begin("aaaaaaaaaaaaaaaa");
    CHECK_FALSE(row.Ok());
    row.Reset();
    CHECK(row.Ok());
}

TEST_CASE("RowBuilder rejects an unbalanced row") {
    std::array<char, 128> storage{};
    RowBuilder row(storage);
    row.Begin("present").Key("ids").BeginArray().Unsigned(1).Unsigned(2).End();
    // End() with an array still open is not a record, and catching it here keeps a malformed line
    // out of the file rather than out of the reader.
    CHECK_FALSE(row.Ok());
}

TEST_CASE("RowBuilder writes a nested array") {
    std::array<char, 128> storage{};
    RowBuilder row(storage);
    row.Begin("acquire").Key("samples").BeginArray().Unsigned(7).Unsigned(8).EndArray().End();
    REQUIRE(row.Ok());
    const std::string text(row.View().data(), row.View().size());
    CHECK(text == R"({"type":"acquire","samples":[7,8]})");
}

TEST_CASE("EventLog rejects a configuration it cannot use") {
    FakeClock clock;
    EventLog log;
    CHECK_FALSE(log.Open(nullptr, clock.Handle(), Time(1)).Ok());
    CHECK_FALSE(log.Open("", clock.Handle(), Time(1)).Ok());
    CHECK_FALSE(log.Open("x.jsonl", Clock{}, Time(1)).Ok());
    CHECK_FALSE(log.IsOpen());
    // Writing to a log that never opened is an error and a counted drop, not a crash and not a
    // silent success: a run whose log failed to open still runs, and has to be able to say so.
    std::array<char, 64> storage{};
    RowBuilder row(storage);
    row.Begin("frame").End();
    CHECK_FALSE(log.Write(row).Ok());
    CHECK(log.Stats().dropped == 1);
}

TEST_CASE("EventLog appends one line per record") {
    TempPath file("signal-core-eventlog-append.jsonl");
    FakeClock clock;
    EventLog log;
    REQUIRE(log.Open(file.text.c_str(), clock.Handle(), Time(0)).Ok());
    for (int i = 0; i < 5; ++i) {
        std::array<char, 128> storage{};
        RowBuilder row(storage);
        row.Begin("frame").Key("frame").Unsigned(static_cast<std::uint64_t>(i)).End();
        REQUIRE(log.Write(row).Ok());
    }
    REQUIRE(log.Close().Ok());
    const auto lines = file.Lines();
    REQUIRE(lines.size() == 5);
    CHECK(lines[0] == R"({"type":"frame","frame":0})");
    CHECK(lines[4] == R"({"type":"frame","frame":4})");
    CHECK(log.Stats().records == 5);
    CHECK(log.Stats().dropped == 0);
    CHECK(log.Stats().bytes == file.Size());
}

TEST_CASE("EventLog flushes on the interval, not on the record") {
    TempPath file("signal-core-eventlog-flush.jsonl");
    FakeClock clock;
    EventLog log;
    // One second, which is 4.8's ceiling on how much a kill can cost.
    REQUIRE(log.Open(file.text.c_str(), clock.Handle(), std::chrono::seconds(1)).Ok());
    const auto write = [&](std::uint64_t index) {
        std::array<char, 128> storage{};
        RowBuilder row(storage);
        row.Begin("frame").Key("frame").Unsigned(index).End();
        REQUIRE(log.Write(row).Ok());
    };

    const std::uint64_t opened = log.Stats().flushes;
    for (std::uint64_t i = 0; i < 60; ++i) {
        clock.now += std::chrono::milliseconds(16);
        write(i);
    }
    // 60 frames at 16 ms is 960 ms, so the second has not elapsed and no flush was forced by the
    // record count. A per-record flush would put a syscall on the frame path.
    CHECK(log.Stats().flushes == opened);

    clock.now += std::chrono::milliseconds(100);
    write(60);
    CHECK(log.Stats().flushes == opened + 1);

    // And the bytes really are on disk before the process ends, which is the whole point.
    CHECK(file.Size() > 0);
    REQUIRE(log.Close().Ok());
}

TEST_CASE("EventLog flushes when the clock goes backwards") {
    TempPath file("signal-core-eventlog-backwards.jsonl");
    FakeClock clock;
    clock.now = std::chrono::seconds(100);
    EventLog log;
    REQUIRE(log.Open(file.text.c_str(), clock.Handle(), std::chrono::seconds(1)).Ok());
    const std::uint64_t opened = log.Stats().flushes;
    // A clock that jumped back would otherwise never satisfy the interval again, and the log
    // would stop flushing for the rest of the run without reporting anything.
    clock.now = std::chrono::seconds(1);
    std::array<char, 128> storage{};
    RowBuilder row(storage);
    row.Begin("frame").Key("frame").Unsigned(1).End();
    REQUIRE(log.Write(row).Ok());
    CHECK(log.Stats().flushes == opened + 1);
    REQUIRE(log.Close().Ok());
}

TEST_CASE("EventLog counts a row that did not build instead of writing it") {
    TempPath file("signal-core-eventlog-drop.jsonl");
    FakeClock clock;
    EventLog log;
    REQUIRE(log.Open(file.text.c_str(), clock.Handle(), Time(0)).Ok());

    std::array<char, 16> tiny{};
    RowBuilder overflowed(tiny);
    overflowed.Begin("frame").Key("padding").String("aaaaaaaaaaaaaaaaaaaaaaaaaaaa").End();
    REQUIRE_FALSE(overflowed.Ok());
    CHECK_FALSE(log.Write(overflowed).Ok());

    std::array<char, 128> storage{};
    RowBuilder good(storage);
    good.Begin("frame").Key("frame").Unsigned(1).End();
    REQUIRE(log.Write(good).Ok());
    REQUIRE(log.Close().Ok());

    CHECK(log.Stats().dropped == 1);
    CHECK(log.Stats().records == 1);
    // The dropped row left nothing behind. A half-written line would cost the reader every line
    // after it, not just this one.
    const auto lines = file.Lines();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == R"({"type":"frame","frame":1})");
}

TEST_CASE("EventLog reopening truncates rather than appending to a stale run") {
    TempPath file("signal-core-eventlog-reopen.jsonl");
    FakeClock clock;
    {
        EventLog log;
        REQUIRE(log.Open(file.text.c_str(), clock.Handle(), Time(0)).Ok());
        std::array<char, 128> storage{};
        RowBuilder row(storage);
        row.Begin("frame").Key("run").Unsigned(1).End();
        REQUIRE(log.Write(row).Ok());
        REQUIRE(log.Close().Ok());
    }
    {
        EventLog log;
        REQUIRE(log.Open(file.text.c_str(), clock.Handle(), Time(0)).Ok());
        // A second run must not look like a continuation of the first, or every count computed
        // over the file would span two runs.
        CHECK_FALSE(log.Open(file.text.c_str(), clock.Handle(), Time(0)).Ok());
        std::array<char, 128> storage{};
        RowBuilder row(storage);
        row.Begin("frame").Key("run").Unsigned(2).End();
        REQUIRE(log.Write(row).Ok());
        REQUIRE(log.Close().Ok());
    }
    const auto lines = file.Lines();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == R"({"type":"frame","run":2})");
}

TEST_CASE("EventLog closing twice is not an error") {
    TempPath file("signal-core-eventlog-close.jsonl");
    FakeClock clock;
    EventLog log;
    REQUIRE(log.Open(file.text.c_str(), clock.Handle(), Time(0)).Ok());
    CHECK(log.Close().Ok());
    CHECK(log.Close().Ok());
    CHECK_FALSE(log.IsOpen());
}

TEST_CASE("EventRowRing carries a row from one thread to another") {
    EventRowRing<64, 8> ring;
    std::array<char, 64> out{};
    std::size_t length = 0;
    CHECK_FALSE(ring.Pop(out, length));

    const std::string row = R"({"type":"frame","frame":1})";
    REQUIRE(ring.Push(std::span<const char>(row.data(), row.size())));
    REQUIRE(ring.Pop(out, length));
    CHECK(std::string(out.data(), length) == row);
    CHECK(ring.Drops() == 0);
}

TEST_CASE("EventRowRing counts what it cannot carry instead of truncating it") {
    EventRowRing<16, 2> ring;
    const std::string oversize(32, 'a');
    CHECK_FALSE(ring.Push(std::span<const char>(oversize.data(), oversize.size())));
    CHECK_FALSE(ring.Push(std::span<const char>()));

    const std::string row(8, 'b');
    REQUIRE(ring.Push(std::span<const char>(row.data(), row.size())));
    REQUIRE(ring.Push(std::span<const char>(row.data(), row.size())));
    // Full. A dropped row is counted, because a log missing rows silently would make every count
    // computed from it wrong.
    CHECK_FALSE(ring.Push(std::span<const char>(row.data(), row.size())));
    CHECK(ring.Drops() == 3);
}

TEST_CASE("One writer draining two producers yields only whole lines") {
    // The failure this exists to catch is two threads calling EventLog::Write directly: a record
    // is a write and then a newline, so they interleave and produce a line no reader can parse.
    // The rings are what make that impossible, so the test drives the real shape.
    TempPath file("signal-core-eventlog-threads.jsonl");
    FakeClock clock;
    EventLog log;
    REQUIRE(log.Open(file.text.c_str(), clock.Handle(), Time(0)).Ok());

    EventRowRing<128, 512> frames;
    EventRowRing<128, 512> samples;
    std::atomic<bool> producing{true};
    constexpr int kRows = 400;

    const auto produce = [&](EventRowRing<128, 512> &ring, const char *type) {
        for (int i = 0; i < kRows; ++i) {
            std::array<char, 128> storage{};
            RowBuilder row(storage);
            row.Begin(type).Key("i").Integer(i).End();
            while (!ring.Push(row.View()))
                std::this_thread::yield();
        }
    };
    std::thread frame_producer(produce, std::ref(frames), "frame");
    std::thread sample_producer(produce, std::ref(samples), "sampled");

    int written = 0;
    std::array<char, 128> out{};
    std::size_t length = 0;
    while (written < kRows * 2) {
        bool moved = false;
        if (frames.Pop(out, length)) {
            REQUIRE(log.Write(std::span<const char>(out.data(), length)).Ok());
            ++written;
            moved = true;
        }
        if (samples.Pop(out, length)) {
            REQUIRE(log.Write(std::span<const char>(out.data(), length)).Ok());
            ++written;
            moved = true;
        }
        if (!moved)
            std::this_thread::yield();
    }
    producing = false;
    frame_producer.join();
    sample_producer.join();
    REQUIRE(log.Close().Ok());

    const auto lines = file.Lines();
    REQUIRE(lines.size() == static_cast<std::size_t>(kRows * 2));
    int frame_lines = 0, sample_lines = 0;
    for (const std::string &line : lines) {
        // Whole lines: one object each, balanced, nothing concatenated.
        REQUIRE(line.size() > 2);
        CHECK(line.front() == '{');
        CHECK(line.back() == '}');
        CHECK(line.find('}', 0) == line.size() - 1);
        if (line.find(R"("type":"frame")") != std::string::npos)
            ++frame_lines;
        else if (line.find(R"("type":"sampled")") != std::string::npos)
            ++sample_lines;
    }
    CHECK(frame_lines == kRows);
    CHECK(sample_lines == kRows);
    CHECK(frames.Drops() == 0);
    CHECK(samples.Drops() == 0);
}
