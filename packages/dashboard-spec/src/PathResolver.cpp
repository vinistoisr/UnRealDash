#include "PackageInternal.h"
#include <cctype>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <locale.h>
#include <wctype.h>
#endif
namespace dashboard_spec {
namespace {
bool FoldCase(const std::string &text, std::string &output) {
#ifdef _WIN32
    const int length =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!length)
        return false;
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), length);
    const int size =
        LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, wide.data(), length, nullptr, 0, nullptr, nullptr, 0);
    if (!size)
        return false;
    std::wstring folded(static_cast<std::size_t>(size), L'\0');
    if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, wide.data(), length, folded.data(), size, nullptr,
                       nullptr, 0))
        return false;
    output.assign(reinterpret_cast<const char *>(folded.data()), folded.size() * sizeof(wchar_t));
    return true;
#else
    // Per-call locale ownership avoids changing the process locale.
    locale_t locale = newlocale(LC_CTYPE_MASK, "C.UTF-8", nullptr);
    if (!locale)
        return false;
    output.clear();
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        std::uint32_t point = first;
        unsigned remaining = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            point = first & 0x1f;
            remaining = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            point = first & 0x0f;
            remaining = 2;
        } else if (first >= 0xf0 && first <= 0xf4) {
            point = first & 7;
            remaining = 3;
        } else if (first >= 0x80) {
            freelocale(locale);
            return false;
        }
        const unsigned width = remaining;
        while (remaining--) {
            if (i == text.size() || (static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) {
                freelocale(locale);
                return false;
            }
            point = (point << 6) | (static_cast<unsigned char>(text[i++]) & 0x3f);
        }
        if ((width == 1 && point < 0x80) || (width == 2 && point < 0x800) || (width == 3 && point < 0x10000) ||
            point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) {
            freelocale(locale);
            return false;
        }
        point = static_cast<std::uint32_t>(towupper_l(static_cast<wint_t>(point), locale));
        for (unsigned byte = 0; byte < 4; ++byte)
            output += static_cast<char>((point >> (byte * 8)) & 0xff);
    }
    freelocale(locale);
    return true;
#endif
}
Error Normalize(std::string_view name, std::string &output) {
    const auto pointer = Join("/entries", name);
    if (name.find(':') != name.npos)
        return Fail(ErrorCode::E_PKG_PATH_DRIVE_LETTER, pointer, name);
    if (name.starts_with('/') || name.starts_with('\\'))
        return Fail(ErrorCode::E_PKG_PATH_ABSOLUTE, pointer, name);
    std::string portable(name);
    std::replace(portable.begin(), portable.end(), '\\', '/');
    output.clear();
    std::size_t start = 0;
    while (start <= portable.size()) {
        auto end = portable.find('/', start);
        if (end == portable.npos)
            end = portable.size();
        auto segment = portable.substr(start, end - start);
        if (segment == "..")
            return Fail(ErrorCode::E_PKG_PATH_TRAVERSAL, pointer, name);
        if (segment.find('\0') != segment.npos ||
            (!segment.empty() && (segment.back() == ' ' || segment.back() == '.') && segment != "."))
            return Fail(ErrorCode::E_PKG_PATH_TRAVERSAL, pointer, name);
        if (!segment.empty() && segment != ".") {
            if (!output.empty())
                output += '/';
            output += segment;
        }
        start = end + 1;
    }
    if (output.empty())
        return Fail(ErrorCode::E_PKG_PATH_TRAVERSAL, pointer, name);
    return {};
}
} // namespace
Error PathResolver::Add(std::string_view name, bool link, std::string &normalized) {
    auto error = Normalize(name, normalized);
    if (!error.Ok())
        return error;
    if (link)
        return Fail(ErrorCode::E_PKG_PATH_LINK, Join("/entries", name), name);
    if (!entries_.insert(normalized).second)
        return Fail(ErrorCode::E_PKG_DUPLICATE_ENTRY, Join("/entries", name), name);
    std::string folded;
    if (!FoldCase(normalized, folded))
        return Fail(ErrorCode::E_PKG_PATH_TRAVERSAL, Join("/entries", name), "entry name is not valid UTF-8");
    if (!folded_.insert(folded).second)
        return Fail(ErrorCode::E_PKG_CASE_COLLISION, Join("/entries", name), name);
    return {};
}
Error PathResolver::Resolve(std::string_view name, std::string &normalized) const {
    auto error = Normalize(name, normalized);
    if (!error.Ok())
        return error;
    if (!entries_.contains(normalized))
        return Fail(ErrorCode::E_PKG_ASSET_NOT_IN_PACKAGE, Join("/assets", name),
                    "declared asset has no inspected entry");
    return {};
}
} // namespace dashboard_spec
