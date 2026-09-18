#pragma once
// A one-client TCP listener on 127.0.0.1, for testing TcpTransport against a real socket.
//
// It binds port 0 so the operating system picks a free port, which is what keeps this safe to run
// on a shared CI machine and safe to run twice at once.
#include <cstdint>
#include <cstring>
#include <span>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

struct LoopbackListener {
#if defined(_WIN32)
    using Raw = SOCKET;
    static constexpr Raw invalid = INVALID_SOCKET;
    static void CloseOne(Raw s) {
        if (s != invalid)
            ::closesocket(s);
    }
    static bool Nonblocking(Raw s) {
        u_long mode = 1;
        return ::ioctlsocket(s, FIONBIO, &mode) == 0;
    }
    static bool WouldBlock() { return ::WSAGetLastError() == WSAEWOULDBLOCK; }
#else
    using Raw = int;
    static constexpr Raw invalid = -1;
    static void CloseOne(Raw s) {
        if (s != invalid)
            ::close(s);
    }
    static bool Nonblocking(Raw s) {
        const int flags = ::fcntl(s, F_GETFL, 0);
        return flags != -1 && ::fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
    }
    static bool WouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }
#endif

    Raw listener{invalid};
    Raw client{invalid};
    std::uint16_t port{};

    bool Open() {
#if defined(_WIN32)
        WSADATA data{};
        if (::WSAStartup(MAKEWORD(2, 2), &data) != 0)
            return false;
#endif
        listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == invalid)
            return false;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = 0; // the operating system picks a free port
        address.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        if (::bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
            return false;
        if (::listen(listener, 1) != 0)
            return false;
#if defined(_WIN32)
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        if (::getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length) != 0)
            return false;
        port = ::ntohs(address.sin_port);
        return Nonblocking(listener);
    }

    // Nonblocking; returns whether a client is now attached.
    bool Accept() {
        if (client != invalid)
            return true;
        client = ::accept(listener, nullptr, nullptr);
        if (client == invalid)
            return false;
        return Nonblocking(client);
    }

    bool Send(std::span<const std::uint8_t> bytes) {
        if (client == invalid)
            return false;
#if defined(_WIN32)
        return ::send(client, reinterpret_cast<const char *>(bytes.data()), static_cast<int>(bytes.size()), 0) ==
               static_cast<int>(bytes.size());
#else
        return ::send(client, bytes.data(), bytes.size(), 0) == static_cast<ssize_t>(bytes.size());
#endif
    }

    // How many bytes the client has sent us. This is the measurement behind PLAN 4.9's
    // "zero bytes sent": asked at the socket, not asserted about the code.
    std::size_t Received() {
        if (client == invalid)
            return 0;
        std::uint8_t buffer[512];
        std::size_t total = 0;
        for (;;) {
#if defined(_WIN32)
            const int count = ::recv(client, reinterpret_cast<char *>(buffer), sizeof(buffer), 0);
#else
            const auto count = ::recv(client, buffer, sizeof(buffer), 0);
#endif
            if (count > 0) {
                total += static_cast<std::size_t>(count);
                continue;
            }
            break;
        }
        return total;
    }

    void DropClient() {
        CloseOne(client);
        client = invalid;
    }

    ~LoopbackListener() {
        DropClient();
        CloseOne(listener);
    }
};
