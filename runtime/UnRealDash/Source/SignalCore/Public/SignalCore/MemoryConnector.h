#pragma once
#include "SignalCore/Connector.h"
#include "SignalCore/Recording.h"
#include <array>

namespace signal_core {
// Byte storage outlives the transport. Connect resumes the supplied stream.
class SIGNALCORE_API MemoryTransport final : public ITransport {
  public:
    explicit MemoryTransport(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    Status Read(std::span<std::uint8_t> into, std::size_t &written) override;
    Status Connect() override {
        connected_ = true;
        return {};
    }
    Status Disconnect() override {
        connected_ = false;
        return {};
    }
    bool Connected() const override { return connected_; }

  private:
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
    bool connected_{};
};
// Internal, same-process records only, not a disk or network format.
class SIGNALCORE_API FieldSession final : public ISession {
  public:
    Status Offer(std::span<const std::uint8_t> input, std::size_t &consumed) override;
    Pending Next(Message &out) override;
    void Reset() override {
        size_ = 0;
        delivered_ = false;
    }

  private:
    std::array<std::uint8_t, sizeof(DecodedField)> record_{};
    std::size_t size_{};
    bool delivered_{};
};
class SIGNALCORE_API FieldDecoder final : public IDecoder {
  public:
    Status Offer(const Message &message) override;
    Pending Next(DecodedField &out) override;
    void Reset() override { ready_ = false; }

  private:
    DecodedField field_{};
    bool ready_{};
};
class SIGNALCORE_API SiMapping final : public IMapping {
  public:
    Pending Map(const DecodedField &in, DecodedField &out) override;
};
// Replay writes ONLY to a private staging registry, never the pipeline's registry.
// Caller owns Replay, staging registry and the comparison storage.
class SIGNALCORE_API ReplayTransport final : public ITransport {
  public:
    ReplayTransport(Replay &replay, SignalRegistry &staging, RecordingView recording, std::span<SignalSample> previous)
        : replay_(replay), staging_(staging), recording_(recording), previous_(previous) {}
    Status Read(std::span<std::uint8_t> into, std::size_t &written) override;
    Status Connect() override;
    Status Disconnect() override {
        connected_ = false;
        offset_ = 0;
        size_ = 0;
        return {};
    }
    bool Connected() const override { return connected_; }

  private:
    Replay &replay_;
    SignalRegistry &staging_;
    RecordingView recording_;
    std::span<SignalSample> previous_;
    std::array<std::uint8_t, sizeof(DecodedField)> record_{};
    std::size_t offset_{}, size_{};
    bool connected_{};
};
} // namespace signal_core
