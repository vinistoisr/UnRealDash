#pragma once
#include "SignalCore/Recording.h"
namespace signal_core::json_lines {
class Writer {
  public:
    explicit Writer(TextSink sink) : sink_(sink) {}
    void Text(std::string_view text);
    void String(std::string_view text);
    void Number(double value);
    void Integer(std::int64_t value);
    void Unsigned(std::uint64_t value);
    Status Finish() const { return status_; }

  private:
    TextSink sink_;
    Status status_{};
};
class Reader {
  public:
    Reader(std::string_view line, std::size_t number, std::size_t offset)
        : line_(line), number_(number), offset_(offset) {}
    bool Token(std::string_view text);
    bool String(std::span<char> output);
    bool Unsigned(std::uint64_t &value);
    bool Integer(std::int64_t &value);
    bool Number(double &value);
    bool Boolean(bool &value);
    bool Done();
    bool Peek(char ch) const { return cursor_ < line_.size() && line_[cursor_] == ch; }
    Status Failure() const;

  private:
    std::string_view line_;
    std::size_t number_{};
    std::size_t offset_{};
    std::size_t cursor_{};
};
} // namespace signal_core::json_lines
