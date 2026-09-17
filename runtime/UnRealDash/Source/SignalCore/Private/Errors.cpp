#include "SignalCore/Errors.h"
#include <cstdarg>
#include <cstdio>
namespace signal_core {
Status Error(ErrorCode code, const char *format, ...) {
    Status result{};
    result.code = code;
    va_list args;
    va_start(args, format);
    std::vsnprintf(result.message, sizeof(result.message), format, args);
    va_end(args);
    return result;
}
} // namespace signal_core
