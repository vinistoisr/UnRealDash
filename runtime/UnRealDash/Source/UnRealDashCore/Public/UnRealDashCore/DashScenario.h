#pragma once
#include "CoreMinimal.h"

namespace UnRealDashCore
{
// Builds a recording covering a document's own signals, so the live binding path can be driven
// before a connector exists.
//
// It generates BYTES, not readings. Everything downstream of it is the real thing: the real
// registry, the real freshness deadlines, the real expiry schedule, the real snapshot exchange,
// and therefore real staleness when the series ends. That is the whole reason for going through a
// recording rather than pushing values at the widgets: a stale signal here is stale because a
// deadline passed, not because something decided to say so.
//
// PLAN 4.9's connectors replace this with a socket or a file. Until then it is the value source,
// and it is named a scenario in the switch that turns it on so nobody mistakes it for data.
struct FDashScenarioOptions
{
    // Seconds of series to generate, and the gap between samples.
    double DurationSeconds = 4.0;
    int32 IntervalMilliseconds = 20;
    // Where each signal starts in its sweep, so two signals in one document are not identical.
    double PhaseStep = 0.25;
};

// Signals are written with the id that is their index in Names, which is the same rule
// FDashBindingTable resolves against, so the two agree by construction.
//
// Returns an empty string on failure with the reason in OutError, matching the convention the rest
// of the engine seam uses: errors are values here for the same reason they are in signal-core.
// Ranges is index-aligned with Names and says what each signal has to sweep for the gauges bound
// to it to move across their whole faces. It comes from those gauges' own declared minimum and
// maximum, so the numbers generated mean something in the document's terms rather than in this
// function's.
UNREALDASHCORE_API FString BuildScenarioRecording(const TArray<FString>& Names,
    const TArray<FVector2D>& Ranges, const FDashScenarioOptions& Options, FString& OutError);

// A named signal-core scenario, written out as a recording. PLAN 4.7's -scenario=<name> means one
// of these, not a waveform this project made up: idle, acceleration, high_temperature,
// missing_signal, disconnect, reconnect and stale_heartbeat, carrying speed and temperature.
//
// Its signals are the scenario's, so a document binding to them has to name them. That is the same
// join every connector needs and it is reported as an unresolved binding when it is wrong.
UNREALDASHCORE_API FString GenerateNamedScenario(const FString& Name, uint64 Seed,
    double DurationSeconds, int32 IntervalMilliseconds, FString& OutError);
}
