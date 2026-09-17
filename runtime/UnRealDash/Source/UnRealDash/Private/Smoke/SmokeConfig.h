#pragma once
#include "CoreMinimal.h"

struct FSmokeFileRead
{
    bool bExists = false;
    FString Contents;
    FString Error;
};
using FSmokeReadFile = TFunction<FSmokeFileRead(const FString&)>;
using FSmokeLog = TFunction<void(const FString&)>;

struct FSmokeConfig
{
    FString Scenario = TEXT("acceleration"); // signal_core scenario name.
    uint64 Seed = 1;
    double DurationSeconds = 10;
    int32 IntervalMilliseconds = 100;
    FString Image; // Defaults to <absolute Saved directory>/smoke.png.
    FString OutputDirectory; // Defaults to the absolute Saved directory.
    double RunSeconds = 10;
};
struct FSmokeConfigResult
{
    FSmokeConfig Config;
    TArray<FString> Errors;
    bool bRunnable = true; // Missing or unreadable player.json permits defaults; invalid fields do not.
};
FSmokeConfigResult ResolveSmokeConfig(const FString& SavedDirectory, const FString& CommandLine,
    const FSmokeReadFile& ReadFile, const FSmokeLog& Log);
