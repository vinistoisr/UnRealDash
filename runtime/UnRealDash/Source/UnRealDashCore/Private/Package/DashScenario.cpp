#include "UnRealDashCore/DashScenario.h"
#include "SignalCore/Scenarios.h"
#include <string>

namespace UnRealDashCore
{
namespace
{
// A triangle over the sweep rather than a sine, because its value at a given time is obvious to
// anyone reading a capture and arguing about what a gauge should be showing.
double TriangleAt(double Phase)
{
    const double Wrapped = Phase - FMath::FloorToDouble(Phase);
    return Wrapped < 0.5 ? Wrapped * 2.0 : (1.0 - Wrapped) * 2.0;
}
}

FString BuildScenarioRecording(const TArray<FString>& Names, const TArray<FVector2D>& Ranges,
    const FDashScenarioOptions& Options, FString& OutError)
{
    OutError.Reset();
    if (Names.Num() == 0)
    {
        OutError = TEXT("The document declares no signals, so there is nothing to generate");
        return FString();
    }
    if (Options.IntervalMilliseconds <= 0 || Options.DurationSeconds <= 0.0)
    {
        OutError = TEXT("Scenario duration and interval must both be positive");
        return FString();
    }

    // The header, in the format Recording.cpp reads. Every signal is dimensionless with a deadline
    // of half a second: the units a document declares belong to the connector that will replace
    // this, and claiming one here would be asserting a physical quantity nothing measured.
    //
    // The deadline is what makes staleness real. When the series ends the registry expires each
    // signal on its own schedule, exactly as it would for a relay that stopped.
    FString Text = TEXT("{\"format\":\"unrealdash-recording\",\"version\":1,\"signals\":[");
    for (int32 Index = 0; Index < Names.Num(); ++Index)
    {
        if (Index) Text += TEXT(",");
        Text += FString::Printf(
            TEXT("{\"id\":%d,\"name\":\"%s\",\"unit\":\"dimensionless\",\"deadline_ns\":500000000,\"discrete\":false}"),
            Index, *Names[Index]);
    }
    Text += TEXT("]}\n");

    const int64 IntervalNanoseconds = static_cast<int64>(Options.IntervalMilliseconds) * 1000000;
    const int32 Steps = FMath::Max(2, FMath::RoundToInt(Options.DurationSeconds * 1000.0 / Options.IntervalMilliseconds));
    uint64 Sequence = 0;
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        const int64 Nanoseconds = static_cast<int64>(Step) * IntervalNanoseconds;
        const double Seconds = static_cast<double>(Step) * Options.IntervalMilliseconds / 1000.0;
        for (int32 Index = 0; Index < Names.Num(); ++Index)
        {
            // One full sweep over the whole duration, offset per signal so a document with several
            // gauges does not show the same needle position on all of them.
            const double Phase = Seconds / Options.DurationSeconds + Index * Options.PhaseStep;
            const FVector2D Range = Ranges.IsValidIndex(Index) ? Ranges[Index] : FVector2D(0.0, 1.0);
            const double Value = Range.X + TriangleAt(Phase) * (Range.Y - Range.X);
            Text += FString::Printf(
                TEXT("{\"t_recv_ns\":%lld,\"signal\":%d,\"value\":%.6f,\"unit\":\"dimensionless\",\"source\":0,")
                TEXT("\"quality\":\"valid\",\"age_evidence\":\"measured\",\"seq\":%llu,\"generation\":0}\n"),
                Nanoseconds, Index, Value, ++Sequence);
        }
    }
    return Text;
}
}

namespace UnRealDashCore
{
FString GenerateNamedScenario(const FString& Name, uint64 Seed, double DurationSeconds,
    int32 IntervalMilliseconds, FString& OutError)
{
    OutError.Reset();
    const std::string Text(TCHAR_TO_UTF8(*Name));
    const auto Parsed = signal_core::ParseScenario({Text.data(), Text.size()});
    if (!Parsed.Ok()) { OutError = UTF8_TO_TCHAR(Parsed.GetStatus().message); return FString(); }
    if (DurationSeconds <= 0.0 || IntervalMilliseconds <= 0)
    { OutError = TEXT("Scenario duration and interval must both be positive"); return FString(); }

    signal_core::ScenarioOptions Options{};
    Options.scenario = *Parsed.Get();
    Options.seed = Seed;
    Options.duration = std::chrono::milliseconds(static_cast<int64>(DurationSeconds * 1000.0));
    Options.interval = std::chrono::milliseconds(IntervalMilliseconds);
    // The freshness deadline the scenario writes into its signals. Half a second, matching every
    // other source here, so a stale reading means the same thing whichever connector produced it.
    Options.deadline = std::chrono::milliseconds(500);

    std::string Recording;
    const signal_core::TextSink Sink{&Recording, [](void* Context, std::string_view Chunk)
        { static_cast<std::string*>(Context)->append(Chunk); return signal_core::Status{}; }};
    const auto Status = signal_core::GenerateScenario(Options, Sink);
    if (!Status.Ok()) { OutError = UTF8_TO_TCHAR(Status.message); return FString(); }
    return FString(UTF8_TO_TCHAR(Recording.c_str()));
}
}
