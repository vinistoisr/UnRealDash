#include "UnRealDashCore/MonotonicClock.h"
#include "HAL/PlatformTime.h"
namespace UnRealDashCore {
FMonotonicClock MakePlatformClock() {
    return {nullptr, [](void*) -> int64 {
        return static_cast<int64>(FPlatformTime::ToSeconds64(FPlatformTime::Cycles64()) * 1.e9);
    }};
}
}
