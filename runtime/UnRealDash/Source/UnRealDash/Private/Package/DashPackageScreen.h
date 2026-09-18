#pragma once
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "UnRealDashCore/ComponentRegistry.h"
#include "DashPackageScreen.generated.h"

UCLASS()
class UDashPackageScreen : public UUserWidget
{
    GENERATED_BODY()
public:
    void Open(const FString& Path, UnRealDashCore::EDashProfile Profile,
        UnRealDashCore::EDashSignalState State = UnRealDashCore::EDashSignalState::Valid,
        float Fraction = 0.f);
    // Read by the HUD to emit one machine-readable verdict line per run; the device half of the
    // PLAN 4.4 gate parses it out of logcat, where an accepted case otherwise logs nothing at all.
    bool WasAccepted() const { return Accepted; }
    const UnRealDashCore::FDashLoadError& LastError() const { return Error; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UWidget* BuildDocumentRoot(UWidget* Content);
    UWidget* BuildErrorRoot();

    UnRealDashCore::FDashPackage Package;
    UnRealDashCore::FDashTheme Theme;
    UnRealDashCore::FDashLoadError Error;
    UnRealDashCore::EDashProfile Profile = UnRealDashCore::EDashProfile::Desktop;
    UnRealDashCore::EDashSignalState State = UnRealDashCore::EDashSignalState::Valid;
    float Fraction = 0.f;
    UnRealDashCore::FWidgetTreeBuilder Builder;
    bool Accepted = false;
};
UCLASS()
class ADashPackageHUD : public AHUD
{
    GENERATED_BODY()
    // Gate-only; see the notes on the definitions. Neither is part of the PLAN 4.7 runtime flag
    // surface, and both exist because a gate has to be runnable without a person at the keyboard.
    void RunBatch(const FString& Directory);
    void ScheduleShot();
    void TickShot();
public:
    virtual void BeginPlay() override;
private:
    UPROPERTY(Transient) TObjectPtr<UDashPackageScreen> Screen;
    FString ShotPath;
    FTimerHandle ShotTimer;
    // Frames to let the scene settle before capturing. The capture conditions switch off temporal
    // antialiasing, bloom, auto exposure and motion blur, so nothing here is converging over time;
    // the wait is for the widget tree to have been laid out and drawn at least once.
    int32 ShotFramesRemaining = 0;
};
UCLASS()
class ADashPackageGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADashPackageGameMode();
};
