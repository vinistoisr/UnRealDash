#include "SignalCoreSmokeCommandlet.h"
#include "SignalCore/SmokeSubset.h"
#include "UnRealDashCore/SignalCoreAdapter.h"
#include "Containers/StringConv.h"
#include <cstdio>

USignalCoreSmokeCommandlet::USignalCoreSmokeCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 USignalCoreSmokeCommandlet::Main(const FString& Params)
{
    (void)Params;
    int32 Assertions = 0;
    int32 Failures = 0;
    const auto Check = [&Assertions, &Failures](bool Passed, const char* Name)
    {
        ++Assertions;
        if (!Passed)
        {
            ++Failures;
            std::printf("SignalCoreSmoke failed: %s\n", Name);
        }
    };
    signal_core::RunSmokeSubset(Check);
    const auto Sample = UnRealDashCore::ToEngineSample(signal_core::Sample{});
    std::printf("SignalCoreSmoke adapter quality=%s age=%s\n",
        TCHAR_TO_UTF8(*UnRealDashCore::ToString(Sample.Quality)),
        TCHAR_TO_UTF8(*UnRealDashCore::ToString(Sample.AgeEvidence)));
    std::printf("SignalCoreSmoke assertions=%d failures=%d\n", Assertions, Failures);
    return Failures == 0 ? 0 : 1;
}
