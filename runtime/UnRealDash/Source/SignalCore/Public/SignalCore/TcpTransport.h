#pragma once
#include "SignalCore/Connector.h"
#include <array>

namespace signal_core {

// A receive-only, non-blocking TCP client for the PLAN 4.9 endpoint, default 127.0.0.1:35000.
//
// **This class has no send path.** There is no Write, no Send, and no call to send, write or sendto
// anywhere in its source. Stage 0 telemetry is receive-only, and the cheapest way to be sure of
// that is for the capability not to exist rather than for a test to watch it go unused. The gate
// checks the source for a send call as well as counting bytes at the socket, because a test only
// proves the paths it exercised and a grep proves the ones it did not.
//
// Connect never blocks. The socket is made non-blocking before connect is called, so a host that is
// not listening returns in progress immediately and the acquisition thread is never parked on a
// dead endpoint. A connect still in flight is reported as need_more_data rather than as success,
// because AcquisitionPipeline::Connect treats an Ok status with Connected() false as the error
// "connect succeeded without connection", and because the connection generation must not advance
// for a link that is not up.
class SIGNALCORE_API TcpTransport final : public ITransport {
  public:
    static constexpr std::uint16_t default_port = 35000;
    static constexpr const char *default_host = "127.0.0.1";

    TcpTransport() = default;
    ~TcpTransport() override;
    TcpTransport(const TcpTransport &) = delete;
    TcpTransport &operator=(const TcpTransport &) = delete;

    // Host is copied; it is a numeric address or a name the platform resolves. Resolution happens
    // in Connect, which is called from the supervisor's retry path rather than from a read, so a
    // slow name server delays a retry rather than stalling acquisition.
    Status Configure(const char *host, std::uint16_t port);

    Status Read(std::span<std::uint8_t> into, std::size_t &written) override;
    Status Connect() override;
    Status Disconnect() override;
    bool Connected() const override;

    // Bytes this transport has written to the socket. It is here so a test can assert it, and it is
    // a constant zero: nothing increments it, because nothing writes.
    std::uint64_t BytesSent() const { return 0; }

  private:
    enum class State : std::uint8_t { idle, connecting, connected };

    Status Begin();
    Status Finish();
    void Close();

    std::array<char, 256> host_{};
    std::uint16_t port_{default_port};
    // The platform handle, kept as an integer so this header needs no platform sockets. Winsock's
    // SOCKET is an unsigned pointer-sized handle and a BSD descriptor is an int; both fit.
    std::uintptr_t socket_{};
    bool open_{};
    State state_{State::idle};
};

} // namespace signal_core
