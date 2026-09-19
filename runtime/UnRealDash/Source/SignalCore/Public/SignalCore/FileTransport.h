#pragma once
#include "SignalCore/Connector.h"
#include <array>
#include <cstdio>

namespace signal_core {

// Replays recorded bytes from a file, in bounded chunks, for PLAN 4.7's -replay=<file>.
//
// Bytes, not samples. It sits in the same place a socket does, so the same framer, decoder and
// mapping run over a file as over a wire, and a bug that only shows up in one of them cannot hide
// in the other. That is also why it is not built on Recording: a recording is samples that have
// already been decoded, which would skip the whole parser.
//
// Reads are bounded by the caller's buffer and nothing is loaded whole, so a recording larger than
// memory replays the same as a small one.
class SIGNALCORE_API FileTransport final : public ITransport {
  public:
    FileTransport() = default;
    ~FileTransport() override;
    FileTransport(const FileTransport &) = delete;
    FileTransport &operator=(const FileTransport &) = delete;

    // Loop restarts at the beginning on reaching the end instead of disconnecting, which is
    // PLAN 4.7's -replay-loop.
    Status Configure(const char *path, bool loop);

    Status Read(std::span<std::uint8_t> into, std::size_t &written) override;
    Status Connect() override;
    Status Disconnect() override;
    bool Connected() const override { return file_ != nullptr; }

    std::uint64_t Loops() const { return loops_; }

  private:
    std::array<char, 1024> path_{};
    std::FILE *file_{};
    bool loop_{};
    std::uint64_t loops_{};
};

} // namespace signal_core
