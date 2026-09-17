#include "UnRealDashCore/SignalCoreAdapter.h"

namespace UnRealDashCore
{
FSignalSample ToEngineSample(const signal_core::Sample& Sample)
{
    return {Sample.value, Sample.unit, Sample.source, Sample.seq, Sample.t_recv.count(),
        Sample.t_source.count(), Sample.has_source_time, Sample.quality, Sample.age_evidence, Sample.generation};
}
FString ToString(signal_core::Quality Quality)
{
    switch (Quality)
    {
    case signal_core::Quality::valid: return TEXT("valid");
    case signal_core::Quality::stale: return TEXT("stale");
    case signal_core::Quality::unavailable: return TEXT("unavailable");
    case signal_core::Quality::invalid: return TEXT("invalid");
    }
    return TEXT("unrecognized quality");
}
FString ToString(signal_core::AgeEvidence Evidence)
{
    switch (Evidence)
    {
    case signal_core::AgeEvidence::measured: return TEXT("measured");
    case signal_core::AgeEvidence::unknown: return TEXT("unknown");
    }
    return TEXT("unrecognized age evidence");
}
}
