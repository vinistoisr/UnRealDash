#pragma once
#include "Blueprint/UserWidget.h"
#include "UnRealDashCore/DashAcquisition.h"
#include "DashDebugOverlay.generated.h"

class UWidgetSwitcher;

UCLASS()
class UDashDebugOverlay : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeSignals(const TArray<FString>& Names, const TMap<FString, uint32>& Ids);
    void Advance(float Seconds, uint64 Resident, uint64 Texture,
        const UnRealDashCore::FFrameSnapshot& Snapshot);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    // Digits and labels are constructed once. Updating text every frame would allocate FText data.
    UWidgetSwitcher* Digits[6][7]{};
    TArray<UWidgetSwitcher*> States;
    TArray<FString> SignalNames;
    TArray<uint32> SignalIds;
    double FrameSeconds[240]{};
    int32 Next = 0, Count = 0;
    uint64 Missed = 0;
    double RefreshSeconds = 0;
    void SetNumber(int32 Row, uint64 Value);
};
