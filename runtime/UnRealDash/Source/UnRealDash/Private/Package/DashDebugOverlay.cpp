#include "DashDebugOverlay.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include <algorithm>

void UDashDebugOverlay::InitializeSignals(const TArray<FString>& Names, const TMap<FString, uint32>& Ids)
{
    SignalNames = Names;
    SignalIds.Reset(Names.Num());
    for (int32 Index = 0; Index < Names.Num(); ++Index)
    {
        const uint32* Id = Ids.Find(Names[Index]);
        SignalIds.Add(Id ? *Id : static_cast<uint32>(Index));
    }
}

TSharedRef<SWidget> UDashDebugOverlay::RebuildWidget()
{
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("DebugOverlayTree"));
    auto Label = [this](const FString& Text)
    {
        UTextBlock* Widget = WidgetTree->ConstructWidget<UTextBlock>();
        Widget->SetText(FText::FromString(Text));
        FSlateFontInfo Font = Widget->GetFont();
        Font.Size = 16;
        Widget->SetFont(Font);
        Widget->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        return Widget;
    };
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.85f));
    Background->SetPadding(FMargin(10.f));
    UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Rows);
    WidgetTree->RootWidget = Background;
    const TCHAR* Labels[] = {TEXT("fps "), TEXT("frame p95 ms "), TEXT("frame p99 ms "),
        TEXT("missed "), TEXT("resident MiB "), TEXT("texture MiB ")};
    for (int32 Row = 0; Row < 6; ++Row)
    {
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
        Rows->AddChild(Line);
        Line->AddChild(Label(Labels[Row]));
        for (int32 Digit = 0; Digit < 7; ++Digit)
        {
            if (Row < 3 && Digit == 6) Line->AddChild(Label(TEXT(".")));
            UWidgetSwitcher* Switch = WidgetTree->ConstructWidget<UWidgetSwitcher>();
            for (int32 Value = 0; Value < 10; ++Value) Switch->AddChild(Label(FString::FromInt(Value)));
            Line->AddChild(Switch);
            Digits[Row][Digit] = Switch;
        }
    }
    States.Reset(SignalNames.Num());
    for (const FString& Name : SignalNames)
    {
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
        Rows->AddChild(Line);
        Line->AddChild(Label(Name + TEXT(" ")));
        UWidgetSwitcher* State = WidgetTree->ConstructWidget<UWidgetSwitcher>();
        for (const TCHAR* Text : {TEXT("valid"), TEXT("stale"), TEXT("unavailable"), TEXT("invalid"), TEXT("age unknown")})
            State->AddChild(Label(Text));
        State->SetActiveWidgetIndex(2);
        Line->AddChild(State);
        States.Add(State);
    }
    SetVisibility(ESlateVisibility::HitTestInvisible);
    return Super::RebuildWidget();
}

void UDashDebugOverlay::SetNumber(int32 Row, uint64 Value)
{
    Value = FMath::Min<uint64>(Value, 9999999);
    for (int32 Digit = 6; Digit >= 0; --Digit)
    {
        const int32 Index = static_cast<int32>(Value % 10);
        if (Digits[Row][Digit]->GetActiveWidgetIndex() != Index)
            Digits[Row][Digit]->SetActiveWidgetIndex(Index);
        Value /= 10;
    }
}

void UDashDebugOverlay::Advance(float Seconds, uint64 Resident, uint64 Texture,
    const UnRealDashCore::FFrameSnapshot& Snapshot)
{
    if (!Digits[0][0] || !FMath::IsFinite(Seconds) || Seconds <= 0) return;
    FrameSeconds[Next] = Seconds;
    Next = (Next + 1) % UE_ARRAY_COUNT(FrameSeconds);
    Count = FMath::Min(Count + 1, static_cast<int32>(UE_ARRAY_COUNT(FrameSeconds)));
    if (Seconds > 1.5 / 60.0) ++Missed;
    RefreshSeconds += Seconds;
    if (RefreshSeconds < 0.25) return;
    RefreshSeconds = 0;
    double Ordered[UE_ARRAY_COUNT(FrameSeconds)];
    double Total = 0;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Ordered[Index] = FrameSeconds[Index];
        Total += Ordered[Index];
    }
    std::sort(Ordered, Ordered + Count);
    SetNumber(0, static_cast<uint64>(10.0 * Count / Total + 0.5));
    SetNumber(1, static_cast<uint64>(Ordered[FMath::CeilToInt(Count * 0.95) - 1] * 10000 + 0.5));
    SetNumber(2, static_cast<uint64>(Ordered[FMath::CeilToInt(Count * 0.99) - 1] * 10000 + 0.5));
    SetNumber(3, Missed);
    SetNumber(4, Resident / (1024 * 1024));
    SetNumber(5, Texture / (1024 * 1024));
    for (int32 Index = 0; Index < States.Num(); ++Index)
    {
        int32 State = 2;
        const int32 Found = Snapshot.SignalIds.IndexOfByKey(SignalIds[Index]);
        if (Snapshot.Samples.IsValidIndex(Found))
        {
            const auto& Sample = Snapshot.Samples[Found];
            State = static_cast<int32>(Sample.Quality);
            if (State == 0 && Sample.AgeEvidence == UnRealDashCore::ESignalAge::Unknown) State = 4;
        }
        if (States[Index]->GetActiveWidgetIndex() != State) States[Index]->SetActiveWidgetIndex(State);
    }
}
