#pragma once
#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "SignalCore/Sample.h"

namespace UnRealDashCore
{
struct FSignalSample
{
    double Value;
    signal_core::Unit Unit;
    uint64 Source;
    uint64 Sequence;
    int64 ReceiveNanoseconds;
    int64 SourceNanoseconds;
    bool bHasSourceTime;
    signal_core::Quality Quality;
    signal_core::AgeEvidence AgeEvidence;
    uint64 Generation;
};
UNREALDASHCORE_API FSignalSample ToEngineSample(const signal_core::Sample& Sample);
UNREALDASHCORE_API FString ToString(signal_core::Quality Quality);
UNREALDASHCORE_API FString ToString(signal_core::AgeEvidence Evidence);
}
