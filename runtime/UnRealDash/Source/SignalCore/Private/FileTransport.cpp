#include "SignalCore/FileTransport.h"
#include <cstring>

namespace signal_core {

FileTransport::~FileTransport() { (void)Disconnect(); }

Status FileTransport::Configure(const char *path, bool loop) {
    if (!path || !*path)
        return Error(ErrorCode::invalid_configuration, "replay path is empty");
    const auto length = std::strlen(path);
    if (length + 1 > path_.size())
        return Error(ErrorCode::invalid_configuration, "replay path is too long");
    std::memcpy(path_.data(), path, length + 1);
    loop_ = loop;
    return {};
}

Status FileTransport::Connect() {
    if (file_)
        return {};
    if (!path_[0])
        return Error(ErrorCode::invalid_configuration, "no replay path configured");
    // Binary, because this is a byte stream and a text-mode translation on Windows would corrupt
    // any frame whose payload happens to contain 0x0d.
    file_ = std::fopen(path_.data(), "rb");
    if (!file_)
        return Error(ErrorCode::io_error, "cannot open replay file %s", path_.data());
    loops_ = 0;
    return {};
}

Status FileTransport::Disconnect() {
    if (file_)
        std::fclose(file_);
    file_ = nullptr;
    return {};
}

Status FileTransport::Read(std::span<std::uint8_t> into, std::size_t &written) {
    written = 0;
    if (!file_)
        return Error(ErrorCode::need_more_data, "not connected");
    if (into.empty())
        return {};

    written = std::fread(into.data(), 1, into.size(), file_);
    if (written)
        return {};
    if (std::ferror(file_)) {
        (void)Disconnect();
        return Error(ErrorCode::io_error, "replay read failed");
    }
    if (!loop_) {
        // End of a replay is a disconnect, not silence. Reported as quiet instead, every mapped
        // signal would sit valid forever on a file that has nothing left to say, which is the same
        // failure a dead socket would cause.
        (void)Disconnect();
        return Error(ErrorCode::io_error, "replay reached the end of the file");
    }
    std::rewind(file_);
    ++loops_;
    written = std::fread(into.data(), 1, into.size(), file_);
    if (!written) {
        // An empty file loops forever producing nothing, which is a spin rather than a replay.
        (void)Disconnect();
        return Error(ErrorCode::io_error, "replay file is empty");
    }
    return {};
}

} // namespace signal_core
