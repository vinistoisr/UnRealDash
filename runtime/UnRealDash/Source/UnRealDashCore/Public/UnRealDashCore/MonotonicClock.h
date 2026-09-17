#pragma once
#include "CoreTypes.h"
namespace UnRealDashCore {
struct FMonotonicClock { void* Context; int64 (*NowNanoseconds)(void*); };
UNREALDASHCORE_API FMonotonicClock MakePlatformClock();
}
