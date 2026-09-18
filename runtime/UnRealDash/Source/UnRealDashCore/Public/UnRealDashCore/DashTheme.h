#pragma once
#include "CoreMinimal.h"
#include "UnRealDashCore/DashPackageLoader.h"

namespace UnRealDashCore
{
// Day and night are the only two token sets the schema defines. Selection is an engine-side call
// with no document field behind it and no automatic switching; PLAN 5.x owns that.
enum class EDashThemeMode : uint8 { Day, Night };

// Which missing-data state a signal-bound primitive renders. Valid is the ordinary case; the other
// four are the schema's four required missing_data states.
enum class EDashSignalState : uint8 { Valid, Stale, Unavailable, Invalid, AgeUnknown };

UNREALDASHCORE_API const TCHAR* SignalStateName(EDashSignalState State);
// Parses the -udash-state= gate switch. Returns false for anything else, so an unknown value is a
// refusal rather than a silent fall back to Valid, which would let a mistyped gate run look green.
UNREALDASHCORE_API bool ParseSignalState(const FString& Text, EDashSignalState& Out);

// The document's colour vocabulary. Colours, and only colours, come from the theme section.
class UNREALDASHCORE_API FDashTheme
{
public:
    // Reads both token sets. A malformed colour string is an error rather than a silent black.
    bool Load(const FDashPackage& Package, FDashLoadError& OutError);

    void SetMode(EDashThemeMode InMode) { Mode = InMode; }
    EDashThemeMode GetMode() const { return Mode; }

    // Resolves a token in the active mode. A token the document does not declare is an error
    // naming the token and the caller's JSON pointer. docs/ARCHITECTURE.md forbids a silent
    // fallback, and a silently defaulted colour reads as a design choice rather than a fault.
    //
    // Through LoadPackage this cannot fire: the semantic pass resolves every token in the document,
    // component colours and missing_data tokens alike, before a builder ever runs. It is kept
    // because FDashTheme is callable outside a validated document. See chunk-11 revision 4, C7.
    bool Resolve(const FString& Token, const FString& Pointer, FLinearColor& OutColour,
        FDashLoadError& OutError) const;

    // The schema's colour union: either a "#rrggbb" or "#rrggbbaa" string, or {"token": "..."}.
    bool ResolveColour(const FDashValue& Value, const FString& Pointer, FLinearColor& OutColour,
        FDashLoadError& OutError) const;

    static bool ParseHexColour(const FString& Text, FLinearColor& OutColour);

private:
    bool LoadSet(const FDashValue& Set, const TCHAR* Which, TMap<FString, FLinearColor>& Out,
        FDashLoadError& OutError);

    TMap<FString, FLinearColor> DayTokens, NightTokens;
    EDashThemeMode Mode = EDashThemeMode::Day;
    FString PackagePath;
};
}
