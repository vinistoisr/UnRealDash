#pragma once
#include "GameFramework/HUD.h"
#include "GameFramework/GameModeBase.h"
#include "SmokeConfig.h"
#include "UnRealDashCore/SmokeSimulator.h"
#include "SmokeHUD.generated.h"

class UMaterialInstanceDynamic;
class UTexture2D;
class UFont;

UCLASS()
class ASmokeHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void DrawHUD() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Texture;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
    UPROPERTY(Transient) TObjectPtr<UFont> Font;
    FSmokeConfig Config;
    TUniquePtr<UnRealDashCore::FSmokeSimulator> Simulator;
    UnRealDashCore::FSignalSample Sample = UnRealDashCore::ToEngineSample(signal_core::Sample{});
    FString Csv = TEXT("schema_version,frame_index,engine_time_seconds,frame_time_milliseconds,value,quality,age_evidence\n");
    FString ScreenshotPath;
    FDelegateHandle ScreenshotHandle;
    double StartedAt = 0;
    double PreviousTime = 0;
    uint64 Frame = 0;
    bool bReady = false;
    bool bExportRequested = false;
    bool bScreenshotProcessed = false;
};

UCLASS()
class ASmokeGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASmokeGameMode();
};

