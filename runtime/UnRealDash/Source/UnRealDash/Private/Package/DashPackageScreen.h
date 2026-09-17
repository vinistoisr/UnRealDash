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
    void Open(const FString& Path, UnRealDashCore::EDashProfile Profile);
    // Read by the HUD to emit one machine-readable verdict line per run; the device half of the
    // PLAN 4.4 gate parses it out of logcat, where an accepted case otherwise logs nothing at all.
    bool WasAccepted() const { return Accepted; }
    const UnRealDashCore::FDashLoadError& LastError() const { return Error; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UnRealDashCore::FDashPackage Package;
    UnRealDashCore::FDashLoadError Error;
    UnRealDashCore::EDashProfile Profile = UnRealDashCore::EDashProfile::Desktop;
    UnRealDashCore::FWidgetTreeBuilder Builder;
    bool Accepted = false;
};
UCLASS()
class ADashPackageHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
private:
    UPROPERTY(Transient) TObjectPtr<UDashPackageScreen> Screen;
};
UCLASS()
class ADashPackageGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADashPackageGameMode();
};
