#include "UnRealDashCore/DashTheme.h"

namespace UnRealDashCore
{
const TCHAR* SignalStateName(EDashSignalState State)
{
    switch (State)
    {
    case EDashSignalState::Stale: return TEXT("stale");
    case EDashSignalState::Unavailable: return TEXT("unavailable");
    case EDashSignalState::Invalid: return TEXT("invalid");
    case EDashSignalState::AgeUnknown: return TEXT("age_unknown");
    default: return TEXT("valid");
    }
}
bool ParseSignalState(const FString& Text, EDashSignalState& Out)
{
    if (Text.Equals(TEXT("valid"), ESearchCase::IgnoreCase)) { Out = EDashSignalState::Valid; return true; }
    if (Text.Equals(TEXT("stale"), ESearchCase::IgnoreCase)) { Out = EDashSignalState::Stale; return true; }
    if (Text.Equals(TEXT("unavailable"), ESearchCase::IgnoreCase)) { Out = EDashSignalState::Unavailable; return true; }
    if (Text.Equals(TEXT("invalid"), ESearchCase::IgnoreCase)) { Out = EDashSignalState::Invalid; return true; }
    if (Text.Equals(TEXT("age_unknown"), ESearchCase::IgnoreCase)) { Out = EDashSignalState::AgeUnknown; return true; }
    return false;
}
bool FDashTheme::ParseHexColour(const FString& Text, FLinearColor& OutColour)
{
    // The schema pins lowercase "#rrggbb" or "#rrggbbaa". Parsing is strict rather than forgiving:
    // a colour this function cannot read is a fault to report, not one to approximate.
    if (Text.Len() != 7 && Text.Len() != 9) return false;
    if (Text[0] != TEXT('#')) return false;
    uint32 Channels[4] = { 0, 0, 0, 255 };
    const int32 Count = Text.Len() == 9 ? 4 : 3;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        uint32 Value = 0;
        for (int32 Digit = 0; Digit < 2; ++Digit)
        {
            const TCHAR Character = Text[1 + Index * 2 + Digit];
            uint32 Nibble;
            if (Character >= TEXT('0') && Character <= TEXT('9')) Nibble = static_cast<uint32>(Character - TEXT('0'));
            else if (Character >= TEXT('a') && Character <= TEXT('f')) Nibble = static_cast<uint32>(Character - TEXT('a')) + 10;
            else if (Character >= TEXT('A') && Character <= TEXT('F')) Nibble = static_cast<uint32>(Character - TEXT('A')) + 10;
            else return false;
            Value = Value * 16 + Nibble;
        }
        Channels[Index] = Value;
    }
    // FColor is sRGB bytes; the widgets want linear. Going through FColor applies the same
    // conversion the rest of the engine applies to authored colours, so a token and a hand-set
    // Slate colour of the same hex land on the same pixel.
    OutColour = FLinearColor(FColor(static_cast<uint8>(Channels[0]), static_cast<uint8>(Channels[1]),
        static_cast<uint8>(Channels[2]), static_cast<uint8>(Channels[3])));
    return true;
}
bool FDashTheme::LoadSet(const FDashValue& Set, const TCHAR* Which, TMap<FString, FLinearColor>& Out,
    FDashLoadError& OutError)
{
    Out.Reset();
    const int32 Count = Set.Num();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FString Name = Set.Key(Index);
        FString Text;
        if (!Set.Member(Name).String(Text))
        {
            OutError = { TEXT("E_SCHEMA"), 7, FString::Printf(TEXT("/theme/%s/%s"), Which, *Name),
                TEXT("Theme token value must be a colour string"), PackagePath };
            return false;
        }
        FLinearColor Colour;
        if (!ParseHexColour(Text, Colour))
        {
            OutError = { TEXT("E_SCHEMA"), 7, FString::Printf(TEXT("/theme/%s/%s"), Which, *Name),
                FString::Printf(TEXT("Theme token is not a #rrggbb or #rrggbbaa colour: %s"), *Text), PackagePath };
            return false;
        }
        Out.Add(Name, Colour);
    }
    return true;
}
bool FDashTheme::Load(const FDashPackage& Package, FDashLoadError& OutError)
{
    PackagePath = Package.Path();
    const FDashValue Theme = Package.Theme();
    if (!LoadSet(Theme.Member(TEXT("day")), TEXT("day"), DayTokens, OutError)) return false;
    if (!LoadSet(Theme.Member(TEXT("night")), TEXT("night"), NightTokens, OutError)) return false;
    OutError = {};
    return true;
}
bool FDashTheme::Resolve(const FString& Token, const FString& Pointer, FLinearColor& OutColour,
    FDashLoadError& OutError) const
{
    const TMap<FString, FLinearColor>& Set = Mode == EDashThemeMode::Night ? NightTokens : DayTokens;
    if (const FLinearColor* Found = Set.Find(Token)) { OutColour = *Found; return true; }
    OutError = { TEXT("E_UNRESOLVED_THEME_TOKEN"), 10, Pointer,
        FString::Printf(TEXT("Theme has no token %s in %s mode"), *Token,
            Mode == EDashThemeMode::Night ? TEXT("night") : TEXT("day")), PackagePath };
    return false;
}
bool FDashTheme::ResolveColour(const FDashValue& Value, const FString& Pointer, FLinearColor& OutColour,
    FDashLoadError& OutError) const
{
    FString Text;
    if (Value.String(Text))
    {
        if (ParseHexColour(Text, OutColour)) return true;
        OutError = { TEXT("E_SCHEMA"), 7, Pointer,
            FString::Printf(TEXT("Colour is not a #rrggbb or #rrggbbaa value: %s"), *Text), PackagePath };
        return false;
    }
    FString Token;
    if (Value.Member(TEXT("token")).String(Token))
        return Resolve(Token, Pointer + TEXT("/token"), OutColour, OutError);
    OutError = { TEXT("E_SCHEMA"), 7, Pointer, TEXT("Colour must be a hex string or a token object"), PackagePath };
    return false;
}
}
