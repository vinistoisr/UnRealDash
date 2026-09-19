#include "DashPlayerConfigTestCommandlet.h"
#include "DashPlayerConfig.h"
#include <cstdio>

// The resolver's own tests, driven with fakes rather than by launching a player. A config resolver
// covered only by launching a player is a config resolver nobody runs: the flag gate takes about
// two minutes and needs a packaged build, and these cases take milliseconds and need neither.
//
// It follows chunk 07's USmokeConfigTestCommandlet exactly, including the reader and logger seams,
// so there is one answer in this project to "how is configuration tested" rather than two.
UDashPlayerConfigTestCommandlet::UDashPlayerConfigTestCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}
int32 UDashPlayerConfigTestCommandlet::Main(const FString& Params)
{
    (void)Params;
    int32 Assertions = 0;
    int32 Failures = 0;
    const auto Check = [&](bool Passed, const char* Name) {
        ++Assertions;
        if (!Passed) { ++Failures; std::printf("DashPlayerConfigTest failed: %s\n", Name); }
    };
    TArray<FString> Logs;
    const FSmokeLog Log = [&](const FString& Text) { Logs.Add(Text); };
    const FSmokeReadFile NoFile = [](const FString&) { return FSmokeFileRead{false, {}, TEXT("missing")}; };
    auto SourceOf = [&Logs](const TCHAR* Name) {
        const FString Prefix = FString::Printf(TEXT("config %s="), Name);
        FString Source;
        for (const FString& Line : Logs)
            if (Line.StartsWith(Prefix)) Source = Line.RightChop(Line.Find(TEXT(" source=")) + 8);
        return Source;
    };

    // A missing file leaves a runnable configuration on the defaults, which is what lets a
    // workstation run need no file at all.
    FString ReadPath;
    Logs.Reset();
    const auto Missing = ResolveDashPlayerConfig(TEXT("/saved"), TEXT(""),
        [&](const FString& Path) { ReadPath = Path; return FSmokeFileRead{false, {}, TEXT("missing")}; }, Log);
    Check(ReadPath == TEXT("/saved/player.json"), "reader path");
    Check(Missing.bRunnable && !Missing.bUnknown, "missing file is runnable");
    Check(Missing.Config.ConnectorHost == TEXT("127.0.0.1"), "default host");
    Check(Missing.Config.ConnectorPort == 35000, "default port");
    Check(Missing.Config.Udash.IsEmpty() && Missing.Config.Connector.IsEmpty(), "no package and no connector by default");
    Check(SourceOf(TEXT("connector-port")) == TEXT("default"), "a default says so");

    // Every field from the file alone, because a Shipping Android build receives no command line.
    const FSmokeReadFile Full = [](const FString&) {
        return FSmokeFileRead{true, TEXT(R"({"udash":"/a.udash","scenario":"idle","replay-file":"/r.bin","replay-loop":true,)")
            TEXT(R"("connector":"tcp","connector-host":"10.0.0.5","connector-port":4242,"screenshot":"/s.png",)")
            TEXT(R"("quit-after":12,"profile":"mobile","freeze-scenario-time":true,"ack-warnings-at":1.5,)")
            TEXT(R"("reveal-delay-ms":250,"rhi":"gles"})"), {}};
    };
    Logs.Reset();
    const auto File = ResolveDashPlayerConfig(TEXT("/saved"), TEXT(""), Full, Log);
    Check(File.bRunnable && File.Errors.IsEmpty(), "the file alone is runnable");
    Check(File.Config.Udash == TEXT("/a.udash") && File.Config.Scenario == TEXT("idle") &&
        File.Config.Replay == TEXT("/r.bin") && File.Config.bReplayLoop &&
        File.Config.Connector == TEXT("tcp") && File.Config.ConnectorHost == TEXT("10.0.0.5") &&
        File.Config.ConnectorPort == 4242 && File.Config.Screenshot == TEXT("/s.png") &&
        File.Config.QuitAfterSeconds == 12 && File.Config.Profile == TEXT("mobile") &&
        File.Config.bFreezeScenarioTime && File.Config.AckWarningsAtSeconds == 1.5 &&
        File.Config.RevealDelayMilliseconds == 250 && File.Config.Rhi == TEXT("gles"), "all fourteen fields from the file");
    Check(SourceOf(TEXT("connector-host")) == TEXT("player.json"), "a file field says where it came from");

    // Per field, not per source. Overriding one field must not discard the other thirteen.
    Logs.Reset();
    const auto Mixed = ResolveDashPlayerConfig(TEXT("/saved"),
        TEXT("-udash=/b.udash -connector-port=5555"), Full, Log);
    Check(Mixed.bRunnable, "a mixed configuration is runnable");
    Check(Mixed.Config.Udash == TEXT("/b.udash") && Mixed.Config.ConnectorPort == 5555, "the command line wins its fields");
    Check(Mixed.Config.Scenario == TEXT("idle") && Mixed.Config.ConnectorHost == TEXT("10.0.0.5") &&
        Mixed.Config.Profile == TEXT("mobile") && Mixed.Config.bReplayLoop, "the other fields survive");
    Check(SourceOf(TEXT("udash")) == TEXT("command line") && SourceOf(TEXT("scenario")) == TEXT("player.json"),
        "each field logs its own source");

    // Three hidden gate switches END IN a documented flag name, and -udash-profile= is passed on
    // every device batch run. They do not shadow the documented flags, because FParse::Value goes
    // through Strifind, which "requires non-alphanumeric lead-in" (CString.h:378), and the
    // character before the inner dash is a letter. The whole hidden set depends on that rule
    // holding, and an engine upgrade could change it silently, so it is pinned here.
    Logs.Reset();
    const auto Shadow = ResolveDashPlayerConfig(TEXT("/saved"),
        TEXT("-udash=/c.udash -udash-profile=mobile -udash-sweep=8 -udash-state=stale"), NoFile, Log);
    Check(Shadow.bRunnable && !Shadow.bUnknown, "the hidden gate switches are accepted");
    Check(Shadow.Config.Profile.IsEmpty(), "-udash-profile is not read as -profile");
    Check(Shadow.Config.Scenario.IsEmpty(), "-udash-sweep is not read as -scenario");
    Check(SourceOf(TEXT("profile")) == TEXT("default"), "the shadowed field still reads as a default");

    // An unknown flag in this project's own namespace, and an unknown key in the file, are both
    // errors rather than settings that quietly are not in force.
    Logs.Reset();
    const auto BadFlag = ResolveDashPlayerConfig(TEXT("/saved"), TEXT("-udash=/d.udash -udash-typo=1"), NoFile, Log);
    Check(BadFlag.bUnknown && !BadFlag.bRunnable, "an unknown -udash flag is rejected");
    Check(BadFlag.Errors.ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("-udash-typo")); }),
        "the unknown flag is named");
    Logs.Reset();
    const auto BadKey = ResolveDashPlayerConfig(TEXT("/saved"), TEXT(""),
        [](const FString&) { return FSmokeFileRead{true, TEXT(R"({"udash":"/e.udash","scenarioo":"idle"})"), {}}; }, Log);
    Check(BadKey.bUnknown && !BadKey.bRunnable, "an unknown player.json key is rejected");
    Check(BadKey.Errors.ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("scenarioo")); }),
        "the unknown key is named");

    // An invalid value is never quietly replaced by a default, which chunk 07 already decided.
    for (const TCHAR* Bad : {TEXT("-scenario=not_a_scenario"), TEXT("-connector=nonsense"),
                             TEXT("-connector-port=70000"), TEXT("-connector-port=zero"),
                             TEXT("-quit-after=-1"), TEXT("-profile=tablet"), TEXT("-rhi=metal")})
    {
        Logs.Reset();
        const auto Invalid = ResolveDashPlayerConfig(TEXT("/saved"), Bad, NoFile, Log);
        Check(!Invalid.bRunnable && !Invalid.Errors.IsEmpty(), "an invalid value is rejected");
    }
    Logs.Reset();
    const auto Malformed = ResolveDashPlayerConfig(TEXT("/saved"), TEXT(""),
        [](const FString&) { return FSmokeFileRead{true, TEXT("{not json"), {}}; }, Log);
    Check(!Malformed.bRunnable, "malformed JSON is rejected");

    // The three flags nothing consumes yet say so, naming the task that will.
    Logs.Reset();
    const auto Unconsumed = ResolveDashPlayerConfig(TEXT("/saved"),
        TEXT("-ack-warnings-at=2 -reveal-delay-ms=100 -rhi=vulkan"), NoFile, Log);
    Check(Unconsumed.bRunnable, "the unconsumed flags are accepted");
    for (const TCHAR* Name : {TEXT("ack-warnings-at"), TEXT("reveal-delay-ms"), TEXT("rhi")})
    {
        const FString Needle = FString::Printf(TEXT("config %s is accepted but not consumed yet"), Name);
        Check(Logs.ContainsByPredicate([&Needle](const FString& L) { return L.StartsWith(Needle); }),
            "an unconsumed flag says so");
    }
    // And they are silent when nobody passed them, or every run would carry three warnings.
    Logs.Reset();
    const auto Quiet = ResolveDashPlayerConfig(TEXT("/saved"), TEXT("-udash=/f.udash"), NoFile, Log);
    Check(Quiet.bRunnable, "a plain run is runnable");
    Check(!Logs.ContainsByPredicate([](const FString& L) { return L.Contains(TEXT("not consumed yet")); }),
        "nothing is warned about when nothing was passed");

    // The supported list is what an unknown flag prints, so it has to carry the whole surface.
    const FString Supported = DashPlayerSupportedFlags();
    for (const TCHAR* Name : {TEXT("-udash"), TEXT("-scenario"), TEXT("-replay-file"), TEXT("-replay-loop"),
                              TEXT("-connector"), TEXT("-connector-host"), TEXT("-connector-port"),
                              TEXT("-screenshot"), TEXT("-quit-after"), TEXT("-profile"),
                              TEXT("-freeze-scenario-time"), TEXT("-ack-warnings-at"),
                              TEXT("-reveal-delay-ms"), TEXT("-rhi")})
        Check(Supported.Contains(FString(Name) + TEXT("\n")), "the supported list names every documented flag");
    Check(!Supported.Contains(TEXT("-udash-state")), "the supported list does not advertise gate switches");

    std::printf("DashPlayerConfigTest assertions=%d failures=%d\n", Assertions, Failures);
    return Failures == 0 ? 0 : 1;
}
