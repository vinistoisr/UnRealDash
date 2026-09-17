#include "JsonLinesText.h"
#include <charconv>
#include <cmath>
// std::from_chars for floating point is not available on every toolchain this project
// builds with: the Android NDK r27 libc++ declares that overload deleted. A per-platform
// fallback would give two conversion paths that can disagree in the last place, which the
// byte-identical recording round trip in task 2.7 would catch. fast_float is the same
// correctly rounded implementation newer libc++ releases use, so every target runs the
// same code. Integer parsing stays on std::from_chars, which every toolchain provides.
#include "fast_float.h"
namespace signal_core::json_lines {
void Writer::Text(std::string_view text) {
    if (status_.Ok())
        status_ =
            sink_.write ? sink_.write(sink_.context, text) : Error(ErrorCode::io_error, "missing output callback");
}
void Writer::String(std::string_view text) {
    Text("\"");
    for (char c : text) {
        switch (c) {
        case '"':
            Text("\\\"");
            break;
        case '\\':
            Text("\\\\");
            break;
        case '\b':
            Text("\\b");
            break;
        case '\f':
            Text("\\f");
            break;
        case '\n':
            Text("\\n");
            break;
        case '\r':
            Text("\\r");
            break;
        case '\t':
            Text("\\t");
            break;
        default:
            if (static_cast<unsigned char>(c) < 32)
                status_ = Error(ErrorCode::invalid_configuration, "string contains unsupported control byte");
            else
                Text({&c, 1});
        }
    }
    Text("\"");
}
void Writer::Number(double value) {
    if (!std::isfinite(value)) {
        Text("null");
        return;
    }
    char buffer[64];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec != std::errc{}) {
        status_ = Error(ErrorCode::non_finite, "number formatting failed");
        return;
    }
    Text({buffer, static_cast<std::size_t>(result.ptr - buffer)});
}
void Writer::Integer(std::int64_t value) {
    char buffer[32];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    Text({buffer, static_cast<std::size_t>(result.ptr - buffer)});
}
void Writer::Unsigned(std::uint64_t value) {
    char buffer[32];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    Text({buffer, static_cast<std::size_t>(result.ptr - buffer)});
}
bool Reader::Token(std::string_view text) {
    if (line_.substr(cursor_, text.size()) != text)
        return false;
    cursor_ += text.size();
    return true;
}
bool Reader::String(std::span<char> output) {
    if (!Token("\""))
        return false;
    std::size_t count = 0;
    while (cursor_ < line_.size()) {
        char c = line_[cursor_++];
        if (c == '"') {
            if (count >= output.size())
                return false;
            output[count] = 0;
            return true;
        }
        if (static_cast<unsigned char>(c) < 32)
            return false;
        if (c == '\\') {
            if (cursor_ == line_.size())
                return false;
            switch (line_[cursor_++]) {
            case '"':
                c = '"';
                break;
            case '\\':
                c = '\\';
                break;
            case '/':
                c = '/';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            default:
                return false;
            }
        }
        if (c == 0 || count + 1 >= output.size())
            return false;
        output[count++] = c;
    }
    return false;
}
bool Reader::Unsigned(std::uint64_t &value) {
    const auto start = cursor_;
    while (cursor_ < line_.size() && line_[cursor_] >= '0' && line_[cursor_] <= '9')
        ++cursor_;
    if (cursor_ == start || (cursor_ - start > 1 && line_[start] == '0'))
        return false;
    auto result = std::from_chars(line_.data() + start, line_.data() + cursor_, value);
    return result.ec == std::errc{} && result.ptr == line_.data() + cursor_;
}
bool Reader::Integer(std::int64_t &value) {
    const auto start = cursor_;
    const bool negative = Token("-");
    std::uint64_t magnitude{};
    if (!Unsigned(magnitude))
        return false;
    (void)negative;
    auto result = std::from_chars(line_.data() + start, line_.data() + cursor_, value);
    return result.ec == std::errc{} && result.ptr == line_.data() + cursor_;
}
bool Reader::Number(double &value) {
    if (Token("null")) {
        value = std::numeric_limits<double>::quiet_NaN();
        return true;
    }
    const auto start = cursor_;
    Token("-");
    std::uint64_t integral{};
    // Scan the integer part separately without imposing an integer magnitude bound on doubles.
    const auto digits = cursor_;
    while (cursor_ < line_.size() && line_[cursor_] >= '0' && line_[cursor_] <= '9')
        ++cursor_;
    if (cursor_ == digits || (cursor_ - digits > 1 && line_[digits] == '0'))
        return false;
    if (Token(".")) {
        const auto fraction = cursor_;
        while (cursor_ < line_.size() && line_[cursor_] >= '0' && line_[cursor_] <= '9')
            ++cursor_;
        if (cursor_ == fraction)
            return false;
    }
    if (Token("e") || Token("E")) {
        if (!Token("+"))
            Token("-");
        const auto exponent = cursor_;
        while (cursor_ < line_.size() && line_[cursor_] >= '0' && line_[cursor_] <= '9')
            ++cursor_;
        if (cursor_ == exponent)
            return false;
    }
    (void)integral;
    auto result = fast_float::from_chars(line_.data() + start, line_.data() + cursor_, value,
                                        fast_float::chars_format::general);
    return result.ec == std::errc{} && result.ptr == line_.data() + cursor_ && std::isfinite(value);
}
bool Reader::Boolean(bool &value) {
    if (Token("true")) {
        value = true;
        return true;
    }
    if (Token("false")) {
        value = false;
        return true;
    }
    return false;
}
bool Reader::Done() { return cursor_ == line_.size(); }
Status Reader::Failure() const {
    return Error(ErrorCode::recording_malformed_line, "recording line %zu byte offset %zu", number_, offset_ + cursor_);
}
} // namespace signal_core::json_lines
