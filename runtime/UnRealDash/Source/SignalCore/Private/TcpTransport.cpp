#include "SignalCore/TcpTransport.h"
#include <cstring>

// Receive-only. There is deliberately no send, write or sendto call anywhere below, and
// scripts/doctor.ps1 checks this file for one.

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace signal_core {
namespace {

#if defined(_WIN32)
using RawSocket = SOCKET;
constexpr RawSocket invalid_socket = INVALID_SOCKET;
inline int LastError() { return WSAGetLastError(); }
inline bool WouldBlock(int error) { return error == WSAEWOULDBLOCK; }
inline bool InProgress(int error) { return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEALREADY; }
inline void CloseSocket(RawSocket s) { ::closesocket(s); }
inline bool SetNonBlocking(RawSocket s) {
    u_long mode = 1;
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
}
// Winsock needs process-wide startup. A function-local static runs once, is thread safe since
// C++11, and never shuts down: WSACleanup during teardown while another socket is live is a worse
// failure than leaking one refcount for the process lifetime.
struct WinsockOnce {
    WinsockOnce() {
        WSADATA data{};
        ok = ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }
    bool ok{};
};
inline bool EnsureWinsock() {
    static WinsockOnce once;
    return once.ok;
}
#else
using RawSocket = int;
constexpr RawSocket invalid_socket = -1;
inline int LastError() { return errno; }
inline bool WouldBlock(int error) { return error == EAGAIN || error == EWOULDBLOCK; }
inline bool InProgress(int error) { return error == EINPROGRESS || error == EALREADY; }
inline void CloseSocket(RawSocket s) { ::close(s); }
inline bool SetNonBlocking(RawSocket s) {
    const int flags = ::fcntl(s, F_GETFL, 0);
    return flags != -1 && ::fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}
inline bool EnsureWinsock() { return true; }
#endif

inline RawSocket Raw(std::uintptr_t handle) { return static_cast<RawSocket>(handle); }

// Zero timeout throughout. Every poll below returns immediately whatever the answer, which is what
// makes "connect never blocks the acquisition thread" true rather than merely intended.
bool Writable(RawSocket socket, bool &failed) {
    fd_set write{}, except{};
    FD_ZERO(&write);
    FD_ZERO(&except);
    FD_SET(socket, &write);
    FD_SET(socket, &except);
    timeval immediately{0, 0};
#if defined(_WIN32)
    const int ready = ::select(0, nullptr, &write, &except, &immediately);
#else
    const int ready = ::select(socket + 1, nullptr, &write, &except, &immediately);
#endif
    failed = ready > 0 && FD_ISSET(socket, &except);
    return ready > 0 && FD_ISSET(socket, &write);
}

} // namespace

TcpTransport::~TcpTransport() { Close(); }

Status TcpTransport::Configure(const char *host, std::uint16_t port) {
    if (!host || !*host)
        return Error(ErrorCode::invalid_configuration, "connector host is empty");
    const auto length = std::strlen(host);
    if (length + 1 > host_.size())
        return Error(ErrorCode::invalid_configuration, "connector host is too long");
    if (port == 0)
        return Error(ErrorCode::invalid_configuration, "connector port must not be zero");
    std::memcpy(host_.data(), host, length + 1);
    port_ = port;
    return {};
}

bool TcpTransport::Connected() const { return state_ == State::connected; }

void TcpTransport::Close() {
    if (open_)
        CloseSocket(Raw(socket_));
    open_ = false;
    socket_ = 0;
    state_ = State::idle;
}

Status TcpTransport::Begin() {
    if (!EnsureWinsock())
        return Error(ErrorCode::io_error, "socket library unavailable");
    if (!host_[0])
        (void)Configure(default_host, default_port);

    char service[8]{};
    std::snprintf(service, sizeof(service), "%u", static_cast<unsigned>(port_));
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo *resolved = nullptr;
    if (::getaddrinfo(host_.data(), service, &hints, &resolved) != 0 || !resolved)
        return Error(ErrorCode::io_error, "cannot resolve %s", host_.data());

    const RawSocket socket = ::socket(resolved->ai_family, resolved->ai_socktype, resolved->ai_protocol);
    if (socket == invalid_socket) {
        ::freeaddrinfo(resolved);
        return Error(ErrorCode::io_error, "cannot create socket");
    }
    // Non-blocking BEFORE connect. The whole point: a host that is not listening must return in
    // progress rather than park the caller for the operating system's connect timeout.
    if (!SetNonBlocking(socket)) {
        CloseSocket(socket);
        ::freeaddrinfo(resolved);
        return Error(ErrorCode::io_error, "cannot make socket non-blocking");
    }
    const int result = ::connect(socket, resolved->ai_addr, static_cast<int>(resolved->ai_addrlen));
    ::freeaddrinfo(resolved);

    socket_ = static_cast<std::uintptr_t>(socket);
    open_ = true;
    if (result == 0) {
        state_ = State::connected;
        return {};
    }
    if (InProgress(LastError())) {
        state_ = State::connecting;
        return Error(ErrorCode::need_more_data, "connect in progress");
    }
    const auto error = LastError();
    Close();
    return Error(ErrorCode::io_error, "connect refused, error %d", error);
}

Status TcpTransport::Finish() {
    bool failed = false;
    if (!Writable(Raw(socket_), failed)) {
        if (!failed)
            return Error(ErrorCode::need_more_data, "connect in progress");
        Close();
        return Error(ErrorCode::io_error, "connect failed");
    }
    // Writable can also mean "failed" on some stacks, so the socket's own error is the answer.
    int socket_error = 0;
#if defined(_WIN32)
    int length = sizeof(socket_error);
    ::getsockopt(Raw(socket_), SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&socket_error), &length);
#else
    socklen_t length = sizeof(socket_error);
    ::getsockopt(Raw(socket_), SOL_SOCKET, SO_ERROR, &socket_error, &length);
#endif
    if (socket_error != 0) {
        Close();
        return Error(ErrorCode::io_error, "connect failed, error %d", socket_error);
    }
    state_ = State::connected;
    return {};
}

Status TcpTransport::Connect() {
    // A connect already in flight is polled rather than restarted, so repeated attempts do not
    // abandon a socket that was about to come up.
    if (state_ == State::connecting)
        return Finish();
    if (state_ == State::connected)
        return {};
    return Begin();
}

Status TcpTransport::Disconnect() {
    Close();
    return {};
}

Status TcpTransport::Read(std::span<std::uint8_t> into, std::size_t &written) {
    written = 0;
    if (state_ != State::connected)
        return Error(ErrorCode::need_more_data, "not connected");
    if (into.empty())
        return {};

#if defined(_WIN32)
    const int count = ::recv(Raw(socket_), reinterpret_cast<char *>(into.data()), static_cast<int>(into.size()), 0);
#else
    const auto count = ::recv(Raw(socket_), into.data(), into.size(), 0);
#endif
    if (count > 0) {
        written = static_cast<std::size_t>(count);
        return {};
    }
    if (count == 0) {
        // An orderly close by the peer. Killing the relay lands here, and reporting it as
        // disconnected rather than as "no data yet" is what lets every mapped signal go stale at
        // its own deadline instead of hanging on a dead socket.
        Close();
        return Error(ErrorCode::io_error, "peer closed the connection");
    }
    const auto error = LastError();
    if (WouldBlock(error))
        return Error(ErrorCode::need_more_data, "no bytes available");
    Close();
    return Error(ErrorCode::io_error, "read failed, error %d", error);
}

} // namespace signal_core
