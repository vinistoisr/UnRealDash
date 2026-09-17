#include "SmokeConfigTestCommandlet.h"
#include "SmokeConfig.h"
#include <cstdio>

USmokeConfigTestCommandlet::USmokeConfigTestCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}
int32 USmokeConfigTestCommandlet::Main(const FString& Params)
{
    (void)Params;
    int32 Assertions = 0;
    int32 Failures = 0;
    const auto Check = [&](bool Passed, const char* Name) {
        ++Assertions;
        if (!Passed) { ++Failures; std::printf("SmokeConfigTest failed: %s\n", Name); }
    };
    TArray<FString> Logs;
    const FSmokeLog Log = [&](const FString& Text) { Logs.Add(Text); };
    FString ReadPath;
    auto Missing = ResolveSmokeConfig(TEXT("/saved"), TEXT(""), [&](const FString& Path) {
        ReadPath = Path; return FSmokeFileRead{false, {}, TEXT("missing")};
    }, Log);
    Check(ReadPath == TEXT("/saved/player.json"), "reader path");
    Check(Missing.bRunnable && Missing.Errors.Num() == 1, "missing file is reported and runnable");
    Check(Missing.Config.Scenario == TEXT("acceleration"), "default scenario");
    Check(Missing.Config.Seed == 1, "default seed");
    Check(Missing.Config.DurationSeconds == 10, "default duration");
    Check(Missing.Config.IntervalMilliseconds == 100, "default interval");
    Check(Missing.Config.Image == TEXT("/saved/smoke.png"), "default image");
    Check(Missing.Config.OutputDirectory == TEXT("/saved"), "default output");
    Check(Missing.Config.RunSeconds == 10, "default run duration");
    Check(Logs.Num() == 9 && Logs[0].Contains(TEXT("existed=false")), "path and seven sources plus missing error logged");
    const FSmokeReadFile Json = [](const FString&) {
        return FSmokeFileRead{true, TEXT(R"({"scenario":"idle","seed":42,"duration_seconds":20,"interval_milliseconds":250,"image":"/input/from json.png","output_directory":"/output/json","run_seconds":30})"), {}};
    };
    const auto Shipping = ResolveSmokeConfig(TEXT("/saved"), TEXT(""), Json, Log);
    Check(Shipping.bRunnable && Shipping.Errors.IsEmpty(), "JSON alone is runnable");
    Check(Shipping.Config.Scenario == TEXT("idle") && Shipping.Config.Seed == 42 && Shipping.Config.DurationSeconds == 20 &&
        Shipping.Config.IntervalMilliseconds == 250 && Shipping.Config.Image == TEXT("/input/from json.png") &&
        Shipping.Config.OutputDirectory == TEXT("/output/json") && Shipping.Config.RunSeconds == 30, "all JSON fields");
    const auto Partial = ResolveSmokeConfig(TEXT("/saved"), TEXT("-seed=7 -image=\"/input/command image.png\""), Json, Log);
    Check(Partial.bRunnable && Partial.Config.Seed == 7 && Partial.Config.Image == TEXT("/input/command image.png"), "command line wins selected fields");
    Check(Partial.Config.Scenario == TEXT("idle") && Partial.Config.DurationSeconds == 20 && Partial.Config.IntervalMilliseconds == 250 &&
        Partial.Config.OutputDirectory == TEXT("/output/json") && Partial.Config.RunSeconds == 30, "unoverridden JSON fields survive");
    const auto All = ResolveSmokeConfig(TEXT("/saved"), TEXT("-scenario=disconnect -seed=8 -duration_seconds=4 -interval_milliseconds=50 -image=/command.png -output_directory=/command -run_seconds=6"), Json, Log);
    Check(All.bRunnable && All.Config.Scenario == TEXT("disconnect") && All.Config.Seed == 8 && All.Config.DurationSeconds == 4 &&
        All.Config.IntervalMilliseconds == 50 && All.Config.Image == TEXT("/command.png") && All.Config.OutputDirectory == TEXT("/command") && All.Config.RunSeconds == 6, "all command fields win");
    const auto Invalid = ResolveSmokeConfig(TEXT("/saved"), TEXT("-scenario=not_a_scenario"), Json, Log);
    Check(!Invalid.bRunnable && !Invalid.Errors.IsEmpty() && Invalid.Config.Scenario == TEXT("not_a_scenario"), "unknown scenario never defaulted");
    const auto InvalidJson = ResolveSmokeConfig(TEXT("/saved"), TEXT(""), [](const FString&) {
        return FSmokeFileRead{true, TEXT(R"({"scenario":"bad"})"), {}};
    }, Log);
    Check(!InvalidJson.bRunnable && InvalidJson.Config.Scenario == TEXT("bad"), "unknown JSON scenario never defaulted");
    const auto Empty = ResolveSmokeConfig(TEXT("/saved"), TEXT(""), [](const FString&) { return FSmokeFileRead{true, TEXT("{}"), {}}; }, Log);
    Check(Empty.bRunnable && Empty.Errors.IsEmpty() && Empty.Config.Scenario == Missing.Config.Scenario &&
        Empty.Config.Seed == Missing.Config.Seed && Empty.Config.DurationSeconds == Missing.Config.DurationSeconds &&
        Empty.Config.IntervalMilliseconds == Missing.Config.IntervalMilliseconds && Empty.Config.Image == Missing.Config.Image &&
        Empty.Config.OutputDirectory == Missing.Config.OutputDirectory && Empty.Config.RunSeconds == Missing.Config.RunSeconds, "empty JSON uses all defaults");
    const auto BadNumber = ResolveSmokeConfig(TEXT("/saved"), TEXT("-interval_milliseconds=0"), Json, Log);
    Check(!BadNumber.bRunnable && !BadNumber.Errors.IsEmpty(), "zero interval rejected");
    std::printf("SmokeConfigTest assertions=%d failures=%d\n", Assertions, Failures);
    return Failures == 0 ? 0 : 1;
}
