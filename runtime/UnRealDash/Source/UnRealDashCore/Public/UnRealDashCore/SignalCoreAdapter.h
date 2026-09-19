#pragma once
#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include <limits>
namespace UnRealDashCore {
enum class ESignalQuality : uint8 { Valid, Stale, Unavailable, Invalid };
enum class ESignalAge : uint8 { Measured, Unknown };
struct FSignalSample {
    // PLAN 4.8's sample identity, carried across the boundary because the present row has to name
    // the sample it rendered and the engine side is where rendering happens.
    uint64 Id = 0;
    double Value = std::numeric_limits<double>::quiet_NaN();
    uint8 Unit = 0;
    uint64 Source = 0, Sequence = 0;
    int64 ReceiveNanoseconds = 0, SourceNanoseconds = 0;
    bool bHasSourceTime = false;
    ESignalQuality Quality = ESignalQuality::Unavailable;
    ESignalAge AgeEvidence = ESignalAge::Unknown;
    uint64 Generation = 0;
};
UNREALDASHCORE_API FString ToString(ESignalQuality Quality);
UNREALDASHCORE_API FString ToString(ESignalAge Evidence);
}
