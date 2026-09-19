#include "SignalCore/EventLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace signal_core {

void RowBuilder::Reset() {
    size_ = 0;
    ok_ = true;
    first_ = true;
    depth_ = 0;
}

void RowBuilder::RawChar(char value) {
    if (!ok_)
        return;
    if (size_ + 1 > into_.size()) {
        // Sticky. A row that did not fit is dropped whole by the caller, never appended in part.
        ok_ = false;
        return;
    }
    into_[size_++] = value;
}

void RowBuilder::Raw(const char *text) {
    for (const char *cursor = text; *cursor; ++cursor)
        RawChar(*cursor);
}

void RowBuilder::Separate() {
    if (!first_)
        RawChar(',');
    first_ = false;
}

RowBuilder &RowBuilder::Begin(const char *type) {
    Reset();
    RawChar('{');
    ++depth_;
    Key("type");
    String(type);
    return *this;
}

RowBuilder &RowBuilder::Key(const char *name) {
    Separate();
    Quoted(name);
    RawChar(':');
    // A key is followed by exactly one value, which takes no comma of its own. The same flag
    // makes the first element of an array comma-free, so arrays and objects share one rule.
    first_ = true;
    return *this;
}

RowBuilder &RowBuilder::Integer(std::int64_t value) {
    Separate();
    char text[24];
    const int written = std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(value));
    if (written <= 0) {
        ok_ = false;
        return *this;
    }
    Raw(text);
    return *this;
}

RowBuilder &RowBuilder::Unsigned(std::uint64_t value) {
    Separate();
    char text[24];
    const int written = std::snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(value));
    if (written <= 0) {
        ok_ = false;
        return *this;
    }
    Raw(text);
    return *this;
}

RowBuilder &RowBuilder::Number(double value) {
    // JSON has no spelling for NaN or the infinities, and a reader that meets a bare NaN token
    // stops at that line and loses the rest of the file. A missing value is null.
    if (!std::isfinite(value))
        return Null();
    Separate();
    char text[40];
    // 17 significant digits round-trips a double exactly, which matters because 6.5 subtracts
    // these numbers from each other.
    const int written = std::snprintf(text, sizeof(text), "%.17g", value);
    if (written <= 0) {
        ok_ = false;
        return *this;
    }
    Raw(text);
    return *this;
}

RowBuilder &RowBuilder::Boolean(bool value) {
    Separate();
    Raw(value ? "true" : "false");
    return *this;
}

RowBuilder &RowBuilder::Null() {
    Separate();
    Raw("null");
    return *this;
}

RowBuilder &RowBuilder::String(const char *value) {
    Separate();
    Quoted(value);
    return *this;
}

void RowBuilder::Quoted(const char *value) {
    RawChar('"');
    for (const char *cursor = value ? value : ""; *cursor; ++cursor) {
        const unsigned char character = static_cast<unsigned char>(*cursor);
        // Backslash matters here rather than in principle: every path this log records on Windows
        // is full of them, and an unescaped one makes the line unparseable.
        if (character == '"' || character == '\\') {
            RawChar('\\');
            RawChar(static_cast<char>(character));
        } else if (character == '\n') {
            Raw("\\n");
        } else if (character == '\r') {
            Raw("\\r");
        } else if (character == '\t') {
            Raw("\\t");
        } else if (character < 0x20) {
            char escape[8];
            std::snprintf(escape, sizeof(escape), "\\u%04x", character);
            Raw(escape);
        } else {
            RawChar(static_cast<char>(character));
        }
    }
    RawChar('"');
}

RowBuilder &RowBuilder::BeginArray() {
    Separate();
    RawChar('[');
    ++depth_;
    first_ = true;
    return *this;
}

RowBuilder &RowBuilder::EndArray() {
    RawChar(']');
    --depth_;
    first_ = false;
    return *this;
}

RowBuilder &RowBuilder::BeginObject() {
    Separate();
    RawChar('{');
    ++depth_;
    first_ = true;
    return *this;
}

RowBuilder &RowBuilder::EndObject() {
    RawChar('}');
    --depth_;
    first_ = false;
    return *this;
}

RowBuilder &RowBuilder::End() {
    RawChar('}');
    --depth_;
    // An unbalanced row is not a row. Catching it here rather than in the reader keeps a
    // malformed line out of the file entirely.
    if (depth_ != 0)
        ok_ = false;
    return *this;
}

EventLog::~EventLog() { (void)Close(); }

Status EventLog::Open(const char *path, Clock clock, Time flush_interval) {
    if (!path || !*path)
        return Error(ErrorCode::invalid_configuration, "event log path is empty");
    if (!clock.now)
        return Error(ErrorCode::invalid_configuration, "event log needs a clock");
    if (file_)
        return Error(ErrorCode::invalid_configuration, "event log is already open");
    // Text mode would translate newlines on Windows and make the byte counts this log reports
    // disagree with the file on disk.
    file_ = std::fopen(path, "wb");
    if (!file_)
        return Error(ErrorCode::io_error, "cannot open event log %s", path);
    clock_ = clock;
    interval_ = flush_interval;
    last_flush_ = clock_.Now();
    stats_ = {};
    return {};
}

Status EventLog::Write(const RowBuilder &row) {
    if (!row.Ok()) {
        ++stats_.dropped;
        return Error(ErrorCode::invalid_configuration, "event log row did not build");
    }
    return Write(row.View());
}

Status EventLog::Write(std::span<const char> line) {
    if (!file_) {
        ++stats_.dropped;
        return Error(ErrorCode::io_error, "event log is not open");
    }
    if (line.empty()) {
        ++stats_.dropped;
        return Error(ErrorCode::invalid_configuration, "event log record is empty");
    }
    const std::size_t written = std::fwrite(line.data(), 1, line.size(), file_);
    if (written != line.size()) {
        ++stats_.dropped;
        return Error(ErrorCode::io_error, "event log write failed");
    }
    if (std::fputc('\n', file_) == EOF) {
        ++stats_.dropped;
        return Error(ErrorCode::io_error, "event log write failed");
    }
    ++stats_.records;
    stats_.bytes += line.size() + 1;
    return Poll();
}

Status EventLog::Poll() {
    if (!file_)
        return {};
    if (interval_ <= Time::zero())
        return Flush();
    const Time now = clock_.Now();
    // A clock that went backwards would otherwise never flush again.
    if (now < last_flush_ || now - last_flush_ >= interval_)
        return Flush();
    return {};
}

Status EventLog::Flush() {
    if (!file_)
        return {};
    last_flush_ = clock_.Now();
    ++stats_.flushes;
    if (std::fflush(file_) != 0)
        return Error(ErrorCode::io_error, "event log flush failed");
    return {};
}

Status EventLog::Close() {
    if (!file_)
        return {};
    // The last flush is the one that matters on an orderly exit, so it happens before the close
    // and its failure is still reported.
    const Status flushed = Flush();
    if (std::fclose(file_) != 0) {
        file_ = nullptr;
        return Error(ErrorCode::io_error, "event log close failed");
    }
    file_ = nullptr;
    return flushed;
}

} // namespace signal_core
