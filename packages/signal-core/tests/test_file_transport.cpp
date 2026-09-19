#include "SignalCore/FileTransport.h"
#include <cstdio>
#include <doctest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace signal_core;

namespace {
// A named file rather than a stream, because the transport's whole job is to open one by path and
// the point of these cases is what happens at its edges.
struct TempFile {
    std::filesystem::path path;
    std::string text;
    explicit TempFile(const std::string &name, const std::vector<std::uint8_t> &bytes) {
        path = std::filesystem::temp_directory_path() / name;
        text = path.string();
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!bytes.empty())
            out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    ~TempFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

std::vector<std::uint8_t> Frames(int count) {
    std::vector<std::uint8_t> bytes;
    for (int i = 0; i < count; ++i) {
        // binary-telemetry-v1: the tag, a little-endian id, and eight payload bytes.
        const std::uint8_t frame[16] = {0x44, 0x33, 0x22, 0x11, static_cast<std::uint8_t>(i + 1), 0, 0, 0,
                                        1,    0,    0,    0,    0,                                0, 0, 0};
        bytes.insert(bytes.end(), std::begin(frame), std::end(frame));
    }
    return bytes;
}
} // namespace

TEST_CASE("FileTransport rejects a path it cannot use") {
    FileTransport transport;
    CHECK_FALSE(transport.Configure(nullptr, false).Ok());
    CHECK_FALSE(transport.Configure("", false).Ok());
    CHECK_FALSE(transport.Configure(std::string(2048, 'x').c_str(), false).Ok());
    // Never configured, so connecting is a configuration error rather than a file error.
    CHECK(transport.Connect().code == ErrorCode::invalid_configuration);
    CHECK_FALSE(transport.Connected());
}

TEST_CASE("FileTransport reports a missing file rather than pretending to connect") {
    FileTransport transport;
    const auto missing = (std::filesystem::temp_directory_path() / "signal-core-absent.bin").string();
    REQUIRE(transport.Configure(missing.c_str(), false).Ok());
    CHECK(transport.Connect().code == ErrorCode::io_error);
    CHECK_FALSE(transport.Connected());
}

TEST_CASE("FileTransport reads in bounded chunks and never more than the caller's buffer") {
    const TempFile file("signal-core-replay-bounded.bin", Frames(64));
    FileTransport transport;
    REQUIRE(transport.Configure(file.text.c_str(), false).Ok());
    REQUIRE(transport.Connect().Ok());
    REQUIRE(transport.Connected());

    // Deliberately not a multiple of the frame size. A transport moves bytes, and a reader that
    // only works when a read lands on a frame boundary is a framer in the wrong place.
    std::uint8_t buffer[100]{};
    std::size_t total = 0;
    std::size_t written = 0;
    while (transport.Read(std::span<std::uint8_t>(buffer, sizeof(buffer)), written).Ok()) {
        CHECK(written <= sizeof(buffer));
        CHECK(written > 0);
        total += written;
    }
    CHECK(total == 64 * 16);
}

TEST_CASE("FileTransport treats the end of a file as a disconnect, not as silence") {
    const TempFile file("signal-core-replay-end.bin", Frames(1));
    FileTransport transport;
    REQUIRE(transport.Configure(file.text.c_str(), false).Ok());
    REQUIRE(transport.Connect().Ok());

    std::uint8_t buffer[64]{};
    std::size_t written = 0;
    REQUIRE(transport.Read(std::span<std::uint8_t>(buffer, sizeof(buffer)), written).Ok());
    CHECK(written == 16);

    // Quiet would leave every mapped signal valid forever on a file with nothing left to say,
    // which is the same failure a dead socket causes.
    const auto status = transport.Read(std::span<std::uint8_t>(buffer, sizeof(buffer)), written);
    CHECK(status.code == ErrorCode::io_error);
    CHECK(written == 0);
    CHECK_FALSE(transport.Connected());
}

TEST_CASE("FileTransport loops from the beginning and counts the laps") {
    const TempFile file("signal-core-replay-loop.bin", Frames(2));
    FileTransport transport;
    REQUIRE(transport.Configure(file.text.c_str(), true).Ok());
    REQUIRE(transport.Connect().Ok());
    CHECK(transport.Loops() == 0);

    std::uint8_t buffer[32]{};
    std::size_t written = 0;
    for (int lap = 0; lap < 5; ++lap) {
        REQUIRE(transport.Read(std::span<std::uint8_t>(buffer, sizeof(buffer)), written).Ok());
        CHECK(written == 32);
        CHECK(transport.Connected());
    }
    CHECK(transport.Loops() == 4);
}

TEST_CASE("FileTransport disconnects on an empty file instead of spinning") {
    // Looping over nothing produces nothing forever, which is a spin rather than a replay.
    const TempFile file("signal-core-replay-empty.bin", {});
    FileTransport transport;
    REQUIRE(transport.Configure(file.text.c_str(), true).Ok());
    REQUIRE(transport.Connect().Ok());

    std::uint8_t buffer[16]{};
    std::size_t written = 0;
    CHECK(transport.Read(std::span<std::uint8_t>(buffer, sizeof(buffer)), written).code == ErrorCode::io_error);
    CHECK_FALSE(transport.Connected());
}

TEST_CASE("FileTransport accepts an empty destination without consuming the file") {
    const TempFile file("signal-core-replay-nodest.bin", Frames(1));
    FileTransport transport;
    REQUIRE(transport.Configure(file.text.c_str(), false).Ok());
    REQUIRE(transport.Connect().Ok());

    std::size_t written = 1;
    CHECK(transport.Read(std::span<std::uint8_t>(), written).Ok());
    CHECK(written == 0);

    std::uint8_t buffer[16]{};
    REQUIRE(transport.Read(std::span<std::uint8_t>(buffer, sizeof(buffer)), written).Ok());
    CHECK(written == 16);
}
