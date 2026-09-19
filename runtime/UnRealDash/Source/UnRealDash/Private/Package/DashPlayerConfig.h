#pragma once
#include "CoreMinimal.h"
#include "Smoke/SmokeConfig.h"

// PLAN 4.7's command-line surface, resolved from the command line over player.json, field by field.
//
// The file is not a convenience. A Shipping Android build receives no command line at all, so the
// file alone has to be sufficient; and a developer overriding one field from a shell must not lose
// the other thirteen, which is why precedence is per field rather than per source.
//
// It reuses the reader and logger seams chunk 07 built for ResolveSmokeConfig, so both resolvers
// are testable without launching anything, and so there is one answer in this project to "where
// does configuration come from" rather than two that can drift.
struct FDashPlayerConfig
{
    FString Udash;
    // A signal-core scenario name. Empty means no scenario.
    FString Scenario;
    FString Replay;
    bool bReplayLoop = false;
    // sim, replay or tcp. Empty means no connector, which is a valid way to look at a document.
    FString Connector;
    FString ConnectorHost = TEXT("127.0.0.1");
    int32 ConnectorPort = 35000;
    FString Screenshot;
    double QuitAfterSeconds = 0;
    // mobile or desktop. Empty means the platform default.
    FString Profile;
    bool bFreezeScenarioTime = false;

    // Accepted, validated and stored, and nothing consumes them yet. Each is logged as such with
    // the task that will. Rejecting them would stop the task they were specified for from passing
    // the flag it needs; ignoring them silently is the failure the unknown-flag check exists to
    // prevent. Saying so is the honest third option.
    double AckWarningsAtSeconds = -1.0;   // PLAN 2.5 acknowledge; the package path builds no rules
    int32 RevealDelayMilliseconds = -1;   // PLAN 5.6 startup reveal
    FString Rhi;                          // Android RHI selection, PLAN 4.11 and 6.x
};

struct FDashPlayerConfigResult
{
    FDashPlayerConfig Config;
    TArray<FString> Errors;
    // A missing player.json still leaves a runnable configuration; an invalid field does not.
    bool bRunnable = true;
    // An unrecognised flag or key. The caller prints the supported list and exits non-zero rather
    // than running something that is not what was asked for.
    bool bUnknown = false;
};

// SavedDirectory is where player.json lives, which is FPaths::ProjectSavedDir() in the player.
FDashPlayerConfigResult ResolveDashPlayerConfig(const FString& SavedDirectory, const FString& CommandLine,
    const FSmokeReadFile& ReadFile, const FSmokeLog& Log);

// The documented surface, one flag per line, for the message an unknown flag prints.
FString DashPlayerSupportedFlags();
