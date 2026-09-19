#include "SignalCoreAdapter.h"

namespace UnRealDashCore
{
FSignalSample ToEngineSample(const signal_core::Sample& Sample)
{
    return {Sample.id, Sample.value, static_cast<uint8>(Sample.unit), Sample.source, Sample.seq, Sample.t_recv.count(),
        Sample.t_source.count(), Sample.has_source_time, static_cast<ESignalQuality>(Sample.quality), static_cast<ESignalAge>(Sample.age_evidence), Sample.generation, Sample.expiry};
}
FString ToString(ESignalQuality Quality)
{
    switch (Quality)
    {
    case ESignalQuality::Valid: return TEXT("valid");
    case ESignalQuality::Stale: return TEXT("stale");
    case ESignalQuality::Unavailable: return TEXT("unavailable");
    case ESignalQuality::Invalid: return TEXT("invalid");
    }
    return TEXT("unrecognized quality");
}
FString ToString(ESignalAge Evidence)
{
    switch (Evidence)
    {
    case ESignalAge::Measured: return TEXT("measured");
    case ESignalAge::Unknown: return TEXT("unknown");
    }
    return TEXT("unrecognized age evidence");
}
}
