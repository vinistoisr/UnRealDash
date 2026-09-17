#include "dashboard_spec/Errors.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace dashboard_spec {
ErrorText::ErrorText(std::string_view text) : text_(new char[text.size() + 1]), length_(text.size()) {
    if (!text.empty())
        std::memcpy(text_, text.data(), text.size());
    text_[text.size()] = '\0';
}
ErrorText::ErrorText(const ErrorText &other) : ErrorText(other.View()) {}
ErrorText::ErrorText(ErrorText &&other) noexcept : text_(other.text_), length_(other.length_) {
    other.text_ = nullptr;
    other.length_ = 0;
}
ErrorText &ErrorText::operator=(const ErrorText &other) {
    if (this != &other) {
        ErrorText copy(other);
        std::swap(text_, copy.text_);
        std::swap(length_, copy.length_);
    }
    return *this;
}
ErrorText &ErrorText::operator=(ErrorText &&other) noexcept {
    if (this != &other) {
        delete[] text_;
        text_ = other.text_;
        length_ = other.length_;
        other.text_ = nullptr;
        other.length_ = 0;
    }
    return *this;
}
ErrorText::~ErrorText() { delete[] text_; }
const char *CodeName(ErrorCode code) {
    switch (code) {
    case ErrorCode::E_PKG_UNSUPPORTED_DOCUMENT:
        return "E_PKG_UNSUPPORTED_DOCUMENT";
    case ErrorCode::E_UNRESOLVED_COMPONENT:
        return "E_UNRESOLVED_COMPONENT";
    case ErrorCode::E_PKG_ASSET_NOT_IN_PACKAGE:
        return "E_PKG_ASSET_NOT_IN_PACKAGE";
    case ErrorCode::none:
        return "";
    case ErrorCode::E_JSON_SYNTAX:
        return "E_JSON_SYNTAX";
    case ErrorCode::E_DUPLICATE_KEY:
        return "E_DUPLICATE_KEY";
    case ErrorCode::E_DOC_TOO_LARGE:
        return "E_DOC_TOO_LARGE";
    case ErrorCode::E_JSON_TOO_DEEP:
        return "E_JSON_TOO_DEEP";
    case ErrorCode::E_JSON_TOO_MANY_NODES:
        return "E_JSON_TOO_MANY_NODES";
    case ErrorCode::E_JSON_STRING_TOO_LONG:
        return "E_JSON_STRING_TOO_LONG";
    case ErrorCode::E_SCHEMA:
        return "E_SCHEMA";
    case ErrorCode::E_UNRESOLVED_SIGNAL:
        return "E_UNRESOLVED_SIGNAL";
    case ErrorCode::E_UNRESOLVED_THEME_TOKEN:
        return "E_UNRESOLVED_THEME_TOKEN";
    case ErrorCode::E_UNRESOLVED_PAGE:
        return "E_UNRESOLVED_PAGE";
    case ErrorCode::E_UNRESOLVED_PARENT:
        return "E_UNRESOLVED_PARENT";
    case ErrorCode::E_COMPONENT_CYCLE:
        return "E_COMPONENT_CYCLE";
    case ErrorCode::E_NO_ROOT:
        return "E_NO_ROOT";
    case ErrorCode::E_MULTIPLE_ROOTS:
        return "E_MULTIPLE_ROOTS";
    case ErrorCode::E_ORPHAN_SUBTREE:
        return "E_ORPHAN_SUBTREE";
    case ErrorCode::E_ASPECT_POLICY_FORBIDDEN:
        return "E_ASPECT_POLICY_FORBIDDEN";
    case ErrorCode::E_IMAGE_NOT_IN_MANIFEST:
        return "E_IMAGE_NOT_IN_MANIFEST";
    case ErrorCode::E_ASSET_REFERENCE_FORBIDDEN:
        return "E_ASSET_REFERENCE_FORBIDDEN";
    case ErrorCode::E_FIELD_PAST_FRAME_LENGTH:
        return "E_FIELD_PAST_FRAME_LENGTH";
    case ErrorCode::E_RULE_UNIT_MISMATCH:
        return "E_RULE_UNIT_MISMATCH";
    case ErrorCode::E_EXPRESSION_TOO_DEEP:
        return "E_EXPRESSION_TOO_DEEP";
    case ErrorCode::E_EXPRESSION_TOO_MANY_NODES:
        return "E_EXPRESSION_TOO_MANY_NODES";
    case ErrorCode::E_TOO_MANY_COMPONENTS:
        return "E_TOO_MANY_COMPONENTS";
    case ErrorCode::E_TOO_MANY_HISTORY_SAMPLES:
        return "E_TOO_MANY_HISTORY_SAMPLES";
    case ErrorCode::E_PKG_EXPANDED_SIZE:
        return "E_PKG_EXPANDED_SIZE";
    case ErrorCode::E_PKG_ENTRY_COUNT:
        return "E_PKG_ENTRY_COUNT";
    case ErrorCode::E_PKG_ASSET_SIZE:
        return "E_PKG_ASSET_SIZE";
    case ErrorCode::E_PKG_PATH_ABSOLUTE:
        return "E_PKG_PATH_ABSOLUTE";
    case ErrorCode::E_PKG_PATH_TRAVERSAL:
        return "E_PKG_PATH_TRAVERSAL";
    case ErrorCode::E_PKG_PATH_DRIVE_LETTER:
        return "E_PKG_PATH_DRIVE_LETTER";
    case ErrorCode::E_PKG_PATH_LINK:
        return "E_PKG_PATH_LINK";
    case ErrorCode::E_PKG_DUPLICATE_ENTRY:
        return "E_PKG_DUPLICATE_ENTRY";
    case ErrorCode::E_PKG_CASE_COLLISION:
        return "E_PKG_CASE_COLLISION";
    case ErrorCode::E_PKG_IMAGE_DIMENSIONS:
        return "E_PKG_IMAGE_DIMENSIONS";
    case ErrorCode::E_PKG_IMAGE_HEADER_MISMATCH:
        return "E_PKG_IMAGE_HEADER_MISMATCH";
    case ErrorCode::E_PKG_IMAGE_FORMAT_UNSUPPORTED:
        return "E_PKG_IMAGE_FORMAT_UNSUPPORTED";
    case ErrorCode::E_PKG_TEXTURE_BUDGET:
        return "E_PKG_TEXTURE_BUDGET";
    case ErrorCode::E_PKG_ZIP_MALFORMED:
        return "E_PKG_ZIP_MALFORMED";
    }
    return "";
}
Error Fail(ErrorCode code, std::string_view pointer, std::string_view message) {
    Error e;
    e.code = code;
    e.pointer = ErrorText(pointer);
    e.message = ErrorText(message);
    return e;
}
[[noreturn]] void ReportAssertion(const char *expression, const char *file, int line) {
    std::fprintf(stderr, "RapidJSON contract failure: %s at %s:%d\n", expression, file, line);
    std::abort();
}
} // namespace dashboard_spec
