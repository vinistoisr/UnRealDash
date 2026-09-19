#include "DashPlayerConfig.h"
#include "Dom/JsonObject.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnRealDashCore/SmokeSimulator.h"

namespace
{
// The documented surface, exactly PLAN 4.7's list. Every name is both a command-line flag and a
// player.json key, spelled the same either way.
const TCHAR* const Documented[] = {
    TEXT("udash"), TEXT("scenario"), TEXT("replay-file"), TEXT("replay-loop"), TEXT("connector"),
    TEXT("connector-host"), TEXT("connector-port"), TEXT("screenshot"), TEXT("quit-after"),
    TEXT("profile"), TEXT("freeze-scenario-time"), TEXT("ack-warnings-at"),
    TEXT("reveal-delay-ms"), TEXT("rhi"),
};

// Gate machinery, deliberately outside the documented surface: forcing a missing-data state,
// pinning a gauge at a deflection, delaying a capture, and loading every fixture in one run. They
// mean nothing to a user of the player. They are listed rather than merely tolerated so the
// unknown-flag check below can tell a hidden switch from a typo of one.
const TCHAR* const Hidden[] = {
    TEXT("udash-state"), TEXT("udash-fraction"), TEXT("udash-shot-at"), TEXT("udash-sweep"),
    TEXT("udash-batch"), TEXT("udash-profile"),
};

bool IsKnownName(const FString& Name)
{
    for (const TCHAR* Known : Documented) if (Name.Equals(Known, ESearchCase::IgnoreCase)) return true;
    for (const TCHAR* Known : Hidden) if (Name.Equals(Known, ESearchCase::IgnoreCase)) return true;
    return false;
}

// Every token on the command line that begins with a dash, with its value stripped.
void FlagNames(const FString& CommandLine, TArray<FString>& Out)
{
    // FParse::Token, so a quoted path with spaces does not split into two flags.
    const TCHAR* Cursor = *CommandLine;
    FString Token;
    while (FParse::Token(Cursor, Token, false))
    {
        if (!Token.StartsWith(TEXT("-"))) continue;
        FString Name = Token.RightChop(1);
        int32 Equals = INDEX_NONE;
        if (Name.FindChar(TEXT('='), Equals)) Name = Name.Left(Equals);
        Out.Add(Name);
    }
}
}

FString DashPlayerSupportedFlags()
{
    FString Text = TEXT("Supported flags, each also a player.json key of the same name:\n");
    for (const TCHAR* Name : Documented) Text += FString::Printf(TEXT("  -%s\n"), Name);
    return Text;
}

FDashPlayerConfigResult ResolveDashPlayerConfig(const FString& SavedDirectory, const FString& CommandLine,
    const FSmokeReadFile& ReadFile, const FSmokeLog& Log)
{
    FDashPlayerConfigResult Result;
    const FString Path = SavedDirectory / TEXT("player.json");
    const FSmokeFileRead File = ReadFile(Path);
    Log(FString::Printf(TEXT("player.json path=%s existed=%s"), *Path, File.bExists ? TEXT("true") : TEXT("false")));

    TSharedPtr<FJsonObject> Json;
    if (!File.bExists || !File.Error.IsEmpty())
    {
        // Missing is not fatal: the command line and the defaults still make a runnable player,
        // which is what lets a workstation run need no file at all.
        Result.Errors.Add(FString::Printf(TEXT("player.json %s: %s; using the command line and defaults"),
            *Path, File.Error.IsEmpty() ? TEXT("missing file") : *File.Error));
    }
    else if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(File.Contents), Json) || !Json.IsValid())
    {
        Result.Errors.Add(TEXT("player.json ") + Path + TEXT(": malformed JSON object"));
        Result.bRunnable = false;
    }
    else
    {
        // The whole namespace belongs to this project, so an unrecognised key is unambiguous: it is
        // a setting the author believes is in force and is not.
        for (auto It = Json->Values.CreateConstIterator(); It; ++It)
        {
            // UE 5.8 keys a JSON object with UE::TSharedString rather than FString.
            const FString Key(It.Key().ToView());
            if (IsKnownName(Key)) continue;
            Result.Errors.Add(FString::Printf(TEXT("player.json has no such key: %s"), *Key));
            Result.bUnknown = true;
            Result.bRunnable = false;
        }
    }

    // The command line is shared with the engine, which owns hundreds of flags, so only this
    // project's own prefix is policed. A typo inside it is caught; a typo of a documented
    // non-prefixed flag such as -scenari= is indistinguishable from an engine flag and is caught
    // instead by the per-field source log below reading "default" for the flag you meant to set.
    TArray<FString> Names;
    FlagNames(CommandLine, Names);
    for (const FString& Name : Names)
    {
        if (!Name.StartsWith(TEXT("udash"), ESearchCase::IgnoreCase)) continue;
        if (IsKnownName(Name)) continue;
        Result.Errors.Add(FString::Printf(TEXT("No such flag: -%s"), *Name));
        Result.bUnknown = true;
        Result.bRunnable = false;
    }

    auto Invalid = [&Result](const FString& Name, const FString& Value) {
        Result.Errors.Add(FString::Printf(TEXT("Invalid value for %s: %s"), *Name, *Value));
        Result.bRunnable = false;
    };

    // One field, resolved from the command line over the file over the default, and logged with
    // the source it came from. The highest-precedence source is tested first, so an overridden
    // invalid file field does not matter.
    auto Field = [&](const TCHAR* Name, const FString& Default, bool& bFound) {
        FString Value = Default;
        FString Source = TEXT("default");
        FString Override;
        bFound = false;
        // FParse::Value requires a non-alphanumeric lead-in (CString.h:378), so "-profile=" does
        // not match inside "-udash-profile=" and the hidden gate switches cannot shadow the
        // documented flags they end with. That is load-bearing and is pinned by
        // DashPlayerConfigTestCommandlet rather than assumed.
        if (FParse::Value(*CommandLine, *(FString(TEXT("-")) + Name + TEXT("=")), Override))
        {
            Value = Override;
            Source = TEXT("command line");
            bFound = true;
        }
        else if (Json.IsValid() && Json->HasField(Name))
        {
            Source = TEXT("player.json");
            bFound = true;
            double Number = 0;
            if (Json->TryGetStringField(Name, Value)) {}
            else if (Json->TryGetNumberField(Name, Number)) { Value = FString::Printf(TEXT("%.17g"), Number); }
            else { Invalid(Name, TEXT("wrong type")); bFound = false; }
        }
        Log(FString::Printf(TEXT("config %s=%s source=%s"), Name, *Value, *Source));
        return Value;
    };

    // A switch with no value. Present on the command line, or true in the file.
    auto Flag = [&](const TCHAR* Name) {
        bool Value = false;
        FString Source = TEXT("default");
        if (FParse::Param(*CommandLine, Name)) { Value = true; Source = TEXT("command line"); }
        else if (Json.IsValid() && Json->HasField(Name))
        {
            Source = TEXT("player.json");
            if (!Json->TryGetBoolField(Name, Value)) Invalid(Name, TEXT("wrong type"));
        }
        Log(FString::Printf(TEXT("config %s=%s source=%s"), Name, Value ? TEXT("true") : TEXT("false"), *Source));
        return Value;
    };

    bool bFound = false;
    auto& Config = Result.Config;
    Config.Udash = Field(TEXT("udash"), FString(), bFound);

    Config.Scenario = Field(TEXT("scenario"), FString(), bFound);
    // Reported where it is configured rather than where it is used, and never a silent fall back
    // to a default, which chunk 07 already decided for the same field.
    if (bFound && !Config.Scenario.IsEmpty() && !UnRealDashCore::IsKnownScenario(Config.Scenario))
        Invalid(TEXT("scenario"), Config.Scenario);

    // Not -replay=. The engine owns that one: GameInstance.cpp:650 reads "-REPLAY=" and hands the
    // value to PlayReplay, which never loads the map, so the player hung before BeginPlay every
    // time. FParse::Value matches a substring, so -udash-replay= would have been caught by it too;
    // only a spelling with no "replay=" in it escapes. Recorded as a deviation from PLAN 4.7.
    Config.Replay = Field(TEXT("replay-file"), FString(), bFound);
    Config.bReplayLoop = Flag(TEXT("replay-loop"));

    Config.Connector = Field(TEXT("connector"), FString(), bFound);
    if (bFound && !Config.Connector.IsEmpty() && Config.Connector != TEXT("sim") &&
        Config.Connector != TEXT("replay") && Config.Connector != TEXT("tcp"))
        Invalid(TEXT("connector"), Config.Connector);

    Config.ConnectorHost = Field(TEXT("connector-host"), TEXT("127.0.0.1"), bFound);
    const FString PortText = Field(TEXT("connector-port"), TEXT("35000"), bFound);
    if (!FDefaultValueHelper::ParseInt(PortText, Config.ConnectorPort) ||
        Config.ConnectorPort <= 0 || Config.ConnectorPort > 65535)
        Invalid(TEXT("connector-port"), PortText);

    Config.Screenshot = Field(TEXT("screenshot"), FString(), bFound);

    const FString QuitText = Field(TEXT("quit-after"), TEXT("0"), bFound);
    if (!FDefaultValueHelper::ParseDouble(QuitText, Config.QuitAfterSeconds) || Config.QuitAfterSeconds < 0)
        Invalid(TEXT("quit-after"), QuitText);

    Config.Profile = Field(TEXT("profile"), FString(), bFound);
    if (bFound && !Config.Profile.IsEmpty() && Config.Profile != TEXT("mobile") && Config.Profile != TEXT("desktop"))
        Invalid(TEXT("profile"), Config.Profile);

    Config.bFreezeScenarioTime = Flag(TEXT("freeze-scenario-time"));

    const FString AckText = Field(TEXT("ack-warnings-at"), TEXT("-1"), bFound);
    if (!FDefaultValueHelper::ParseDouble(AckText, Config.AckWarningsAtSeconds)) Invalid(TEXT("ack-warnings-at"), AckText);
    if (bFound) Log(TEXT("config ack-warnings-at is accepted but not consumed yet: the package path builds no rules from the document"));

    const FString RevealText = Field(TEXT("reveal-delay-ms"), TEXT("-1"), bFound);
    if (!FDefaultValueHelper::ParseInt(RevealText, Config.RevealDelayMilliseconds)) Invalid(TEXT("reveal-delay-ms"), RevealText);
    if (bFound) Log(TEXT("config reveal-delay-ms is accepted but not consumed yet: the startup reveal is PLAN 5.6"));

    Config.Rhi = Field(TEXT("rhi"), FString(), bFound);
    if (bFound && !Config.Rhi.IsEmpty() && Config.Rhi != TEXT("vulkan") && Config.Rhi != TEXT("gles"))
        Invalid(TEXT("rhi"), Config.Rhi);
    if (bFound && !Config.Rhi.IsEmpty())
        Log(TEXT("config rhi is accepted but not consumed yet: RHI selection is a device concern, PLAN 4.11"));

    for (const FString& Error : Result.Errors) Log(TEXT("config error: ") + Error);
    return Result;
}
