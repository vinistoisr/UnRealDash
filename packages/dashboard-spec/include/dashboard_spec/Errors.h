#pragma once
#include <string_view>
namespace dashboard_spec {
class ErrorText {
  public:
    ErrorText() = default;
    explicit ErrorText(std::string_view text);
    ErrorText(const ErrorText &other);
    ErrorText(ErrorText &&other) noexcept;
    ErrorText &operator=(const ErrorText &other);
    ErrorText &operator=(ErrorText &&other) noexcept;
    ~ErrorText();
    const char *c_str() const { return text_ ? text_ : ""; }
    std::string_view View() const { return {c_str(), length_}; }
    operator const char *() const { return c_str(); }

  private:
    char *text_{};
    std::size_t length_{};
};
enum class ErrorCode {
    none = 0,
    E_JSON_SYNTAX = 1,
    E_DUPLICATE_KEY = 2,
    E_DOC_TOO_LARGE = 3,
    E_JSON_TOO_DEEP = 4,
    E_JSON_TOO_MANY_NODES = 5,
    E_JSON_STRING_TOO_LONG = 6,
    E_SCHEMA = 7,
    E_UNRESOLVED_SIGNAL = 9,
    E_UNRESOLVED_THEME_TOKEN = 10,
    E_UNRESOLVED_PAGE = 11,
    E_UNRESOLVED_PARENT = 12,
    E_COMPONENT_CYCLE = 14,
    E_NO_ROOT = 15,
    E_MULTIPLE_ROOTS = 16,
    E_ORPHAN_SUBTREE = 17,
    E_ASPECT_POLICY_FORBIDDEN = 18,
    E_IMAGE_NOT_IN_MANIFEST = 19,
    E_ASSET_REFERENCE_FORBIDDEN = 20,
    E_FIELD_PAST_FRAME_LENGTH = 21,
    E_RULE_UNIT_MISMATCH = 23,
    E_EXPRESSION_TOO_DEEP = 24,
    E_EXPRESSION_TOO_MANY_NODES = 25,
    E_TOO_MANY_COMPONENTS = 26,
    E_TOO_MANY_HISTORY_SAMPLES = 27,
    E_PKG_EXPANDED_SIZE = 28,
    E_PKG_ENTRY_COUNT = 29,
    E_PKG_ASSET_SIZE = 30,
    E_PKG_PATH_ABSOLUTE = 31,
    E_PKG_PATH_TRAVERSAL = 32,
    E_PKG_PATH_DRIVE_LETTER = 33,
    E_PKG_PATH_LINK = 34,
    E_PKG_DUPLICATE_ENTRY = 35,
    E_PKG_CASE_COLLISION = 36,
    E_PKG_IMAGE_DIMENSIONS = 37,
    E_PKG_IMAGE_HEADER_MISMATCH = 38,
    E_PKG_IMAGE_FORMAT_UNSUPPORTED = 39,
    E_PKG_TEXTURE_BUDGET = 40,
    E_PKG_ZIP_MALFORMED = 41,
    E_PKG_UNSUPPORTED_DOCUMENT = 42,
    E_UNRESOLVED_COMPONENT = 43,
    E_PKG_ASSET_NOT_IN_PACKAGE = 44,
};
struct Error {
    ErrorCode code{};
    ErrorText pointer;
    ErrorText message;
    bool Ok() const { return code == ErrorCode::none; }
};
const char *CodeName(ErrorCode code);
Error Fail(ErrorCode code, std::string_view pointer, std::string_view message);
[[noreturn]] void ReportAssertion(const char *expression, const char *file, int line);
} // namespace dashboard_spec
