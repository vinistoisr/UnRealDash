#pragma once
#include "SignalCore/Sample.h"
#include <span>

namespace signal_core {
enum class Pending : std::uint8_t { produced, need_more_data, done };
struct ITransport {
    virtual ~ITransport() = default;
    // Nonblocking; need_more_data with zero bytes means nothing is available.
    virtual Status Read(std::span<std::uint8_t> into, std::size_t &written) = 0;
    virtual Status Connect() = 0;
    virtual Status Disconnect() = 0;
    virtual bool Connected() const = 0;
};
struct Message {
    std::span<const std::uint8_t> bytes;
};
struct ISession {
    virtual ~ISession() = default;
    virtual Status Offer(std::span<const std::uint8_t> input, std::size_t &consumed) = 0;
    virtual Pending Next(Message &out) = 0;
    virtual void Reset() = 0;
};
struct DecodedField {
    SignalId signal;
    Sample sample;
};
struct IDecoder {
    virtual ~IDecoder() = default;
    virtual Status Offer(const Message &message) = 0;
    virtual Pending Next(DecodedField &out) = 0;
    virtual void Reset() = 0;
};
struct IMapping {
    virtual ~IMapping() = default;
    virtual Pending Map(const DecodedField &in, DecodedField &out) = 0;
};
struct ConnectionHealth {
    std::uint64_t generation{};
    bool connected{};
    Time last_byte_at{};
    std::uint64_t reconnects{};
    std::uint64_t bytes{};
    char last_error[128]{};
};
} // namespace signal_core
