#include "SmokeConfig.h"
#include "Misc/DefaultValueHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Parse.h"
#include "UnRealDashCore/SmokeSimulator.h"

FSmokeConfigResult ResolveSmokeConfig(const FString& SavedDirectory, const FString& CommandLine,
    const FSmokeReadFile& ReadFile, const FSmokeLog& Log)
{
    FSmokeConfigResult Result;
    Result.Config.Image = SavedDirectory / TEXT("smoke.png");
    Result.Config.OutputDirectory = SavedDirectory;
    const FString Path = SavedDirectory / TEXT("player.json");
    const FSmokeFileRead File = ReadFile(Path);
    Log(FString::Printf(TEXT("player.json path=%s existed=%s"), *Path, File.bExists ? TEXT("true") : TEXT("false")));
    TSharedPtr<FJsonObject> Json;
    if (!File.Error.IsEmpty() || !File.bExists)
    {
        Result.Errors.Add(FString::Printf(TEXT("player.json %s: %s; using available command line fields and defaults"),
            *Path, File.Error.IsEmpty() ? TEXT("missing file") : *File.Error));
    }
    else if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(File.Contents), Json) || !Json.IsValid())
    {
        Result.Errors.Add(TEXT("player.json ") + Path + TEXT(": malformed JSON object"));
        Result.bRunnable = false;
    }
    auto Error = [&Result](const FString& Name) {
        Result.Errors.Add(TEXT("Invalid smoke configuration field: ") + Name);
        Result.bRunnable = false;
    };
    auto Field = [&](const TCHAR* Name, FString Default, bool bNumber) {
        FString Value = Default;
        FString Source = TEXT("default");
        FString Override;
        // Test the highest-precedence source first so an overridden invalid JSON field is immaterial.
        if (FParse::Value(*CommandLine, *(FString(TEXT("-")) + Name + TEXT("=")), Override))
        {
            Value = Override;
            Source = TEXT("command line");
        }
        else if (Json.IsValid() && Json->HasField(Name))
        {
            Source = TEXT("player.json");
            if (bNumber)
            {
                double Number;
                if (Json->TryGetNumberField(Name, Number)) { Value = FString::Printf(TEXT("%.17g"), Number); }
                else { Error(Name); Value = TEXT("invalid"); }
            }
            else if (!Json->TryGetStringField(Name, Value)) { Error(Name); Value = TEXT("invalid"); }
        }
        Log(FString::Printf(TEXT("smoke %s=%s source=%s"), Name, *Value, *Source));
        return Value;
    };
    auto Number = [&](const TCHAR* Name, double Default, double Minimum, double Maximum, bool bInteger) {
        const FString Text = Field(Name, FString::Printf(TEXT("%.17g"), Default), true);
        // FDefaultValueHelper rather than std::from_chars: the Android NDK r27 libc++ declares the
        // floating point from_chars overloads deleted, so this file did not compile for Android.
        // signal-core solved the same problem by vendoring fast_float, which does not apply here,
        // because this is engine side code and Unreal already supplies a portable parser. ParseDouble
        // validates the whole string before converting, so trailing junk is rejected as from_chars
        // rejected it, rather than being silently ignored the way Atod would.
        double Value = 0;
        if (!FDefaultValueHelper::ParseDouble(Text, Value) || !FMath::IsFinite(Value) || Value < Minimum || Value > Maximum ||
            (bInteger && FMath::FloorToDouble(Value) != Value)) { Error(Name); return Default; }
        return Value;
    };
    auto& Config = Result.Config;
    Config.Scenario = Field(TEXT("scenario"), Config.Scenario, false);
    if (!UnRealDashCore::IsKnownScenario(Config.Scenario)) { Error(TEXT("scenario=") + Config.Scenario); }
    // JSON numbers preserve integers exactly only through 2^53 - 1.
    Config.Seed = static_cast<uint64>(Number(TEXT("seed"), 1, 0, 9007199254740991.0, true));
    Config.DurationSeconds = Number(TEXT("duration_seconds"), 10, 0.001, 3600, false);
    Config.IntervalMilliseconds = static_cast<int32>(Number(TEXT("interval_milliseconds"), 100, 1, 3600000, true));
    Config.Image = Field(TEXT("image"), Config.Image, false);
    Config.OutputDirectory = Field(TEXT("output_directory"), Config.OutputDirectory, false);
    Config.RunSeconds = Number(TEXT("run_seconds"), 10, 0.001, 3600, false);
    const auto IsAbsolute = [](const FString& Value) {
        return Value.StartsWith(TEXT("/")) || Value.StartsWith(TEXT("\\\\")) ||
            (Value.Len() >= 3 && FChar::IsAlpha(Value[0]) && Value[1] == ':' && (Value[2] == '/' || Value[2] == '\\'));
    };
    if (!IsAbsolute(Config.Image)) { Error(TEXT("image requires an absolute path")); }
    if (!IsAbsolute(Config.OutputDirectory)) { Error(TEXT("output_directory requires an absolute path")); }
    if (Config.DurationSeconds * 1000.0 / Config.IntervalMilliseconds > 500000)
    {
        Error(TEXT("duration_seconds/interval_milliseconds exceeds 500000 steps"));
    }
    for (const FString& Message : Result.Errors) { Log(TEXT("smoke error: ") + Message); }
    return Result;
}

