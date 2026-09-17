#include "SmokeHUD.h"
#include "HAL/PlatformFileManager.h"
#include "SmokeTexture.h"
#include "UnRealDashLog.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"
#include "DynamicRHI.h"
#include "RHI.h"

ASmokeGameMode::ASmokeGameMode()
{
    HUDClass = ASmokeHUD::StaticClass();
    DefaultPawnClass = nullptr;
}
void ASmokeHUD::BeginPlay()
{
    Super::BeginPlay();
    const FSmokeLog Log = [](const FString& Message) { UE_LOG(LogUnRealDash, Display, TEXT("%s"), *Message); };
    // ConvertRelativePathToFull is not enough on Android, where ProjectSavedDir is a virtual path
    // like ../../../UnRealDash/Saved/ that the platform file layer resolves. Asking the platform
    // file for the external-app path is what turns it into the real directory adb can reach, which
    // is also the directory this run has to report so files can be pushed and pulled.
    const FString Saved = FPaths::ConvertRelativePathToFull(
        IPlatformFile::GetPlatformPhysical().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::ProjectSavedDir()));
    // This actor is the engine composition boundary. Helpers receive these services as arguments.
    const auto Resolved = ResolveSmokeConfig(Saved, FCommandLine::Get(), [](const FString& Path) {
        FSmokeFileRead Read;
        Read.bExists = IFileManager::Get().FileExists(*Path);
        if (!Read.bExists || !FFileHelper::LoadFileToString(Read.Contents, *Path)) { Read.Error = TEXT("Cannot read file"); }
        return Read;
    }, Log);
    if (!Resolved.bRunnable) { FPlatformMisc::RequestExit(false); return; }
    Config = Resolved.Config;
    StartedAt = PreviousTime = GetWorld()->GetRealTimeSeconds();
    Simulator = MakeUnique<UnRealDashCore::FSmokeSimulator>(signal_core::Clock{GetWorld(), [](void* Context) {
        return signal_core::Time(static_cast<int64>(static_cast<UWorld*>(Context)->GetRealTimeSeconds() * 1.e9));
    }});
    const UnRealDashCore::FSmokeSimulatorOptions Options{
        Config.Scenario, Config.Seed, Config.DurationSeconds, Config.IntervalMilliseconds};
    const FString SimulationError = Simulator->Start(Options);
    if (!SimulationError.IsEmpty()) { Log(SimulationError); FPlatformMisc::RequestExit(false); return; }
    IImageWrapperModule* Images = FModuleManager::LoadModulePtr<IImageWrapperModule>(TEXT("ImageWrapper"));
    if (!Images) { Log(TEXT("ImageWrapper module unavailable")); FPlatformMisc::RequestExit(false); return; }
    const auto Imported = LoadSmokeTexture(Config.Image, [](const FString& Path, TArray<uint8>& Bytes, FString& Error) {
        if (!FFileHelper::LoadFileToArray(Bytes, *Path)) { Error = TEXT("Cannot read PNG"); return false; }
        return true;
    }, *Images, Log);
    if (!Imported.Texture) { Log(Imported.Error); FPlatformMisc::RequestExit(false); return; }
    Texture = Imported.Texture;
    UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Game/Smoke/M_Smoke.M_Smoke"));
    Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    if (!Base || !Font) { Log(TEXT("Smoke material or font missing; run MakeSmokeAssets before packaging")); FPlatformMisc::RequestExit(false); return; }
    Material = UMaterialInstanceDynamic::Create(Base, this);
    if (!Material) { Log(TEXT("Smoke material instance creation failed")); FPlatformMisc::RequestExit(false); return; }
    Material->SetTextureParameterValue(TEXT("BaseTexture"), Texture);
    UE_LOG(LogUnRealDash, Display, TEXT("Smoke RHI=%s GPU=%s driver=%s internal_driver=%s"),
        GDynamicRHI ? GDynamicRHI->GetName() : TEXT("unavailable"), *GRHIAdapterName,
        *GRHIAdapterUserDriverVersion, *GRHIAdapterInternalDriverVersion);
    bReady = true;
}
void ASmokeHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!bReady || !Canvas) { return; }
    const double Now = GetWorld()->GetRealTimeSeconds();
    const FString Error = Simulator->Tick(Sample);
    if (!Error.IsEmpty()) { UE_LOG(LogUnRealDash, Error, TEXT("%s"), *Error); bReady = false; FPlatformMisc::RequestExit(false); return; }
    const FString Quality = UnRealDashCore::ToString(Sample.Quality);
    const FString Age = UnRealDashCore::ToString(Sample.AgeEvidence);
    Canvas->K2_DrawText(Font, FString::Printf(TEXT("%.6f quality=%s age_evidence=%s"), Sample.Value, *Quality, *Age), FVector2D(24, 24));
    const float Size = FMath::Max(1.f, FMath::Min(Canvas->SizeX * 0.4f, Canvas->SizeY * 0.6f));
    Canvas->K2_DrawTexture(Texture, FVector2D(24, 80), FVector2D(Size, Size), FVector2D::ZeroVector);
    Material->SetScalarParameterValue(TEXT("Value"), static_cast<float>(Sample.Value));
    Canvas->K2_DrawMaterial(Material, FVector2D(48 + Size, 80), FVector2D(Size, Size), FVector2D::ZeroVector);
    Csv += FString::Printf(TEXT("1,%llu,%.9f,%.6f,%.17g,%s,%s\n"), Frame++, Now, (Now - PreviousTime) * 1000, Sample.Value, *Quality, *Age);
    PreviousTime = Now;
    if (!bExportRequested && Now - StartedAt >= Config.RunSeconds)
    {
        if (!IFileManager::Get().MakeDirectory(*Config.OutputDirectory, true))
        {
            UE_LOG(LogUnRealDash, Error, TEXT("Cannot create output_directory=%s"), *Config.OutputDirectory);
            bReady = false; FPlatformMisc::RequestExit(false); return;
        }
        const FString MetricsPath = Config.OutputDirectory / TEXT("metrics.csv");
        if (!FFileHelper::SaveStringToFile(Csv, *MetricsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            UE_LOG(LogUnRealDash, Error, TEXT("Cannot write metrics path=%s"), *MetricsPath);
            bReady = false; FPlatformMisc::RequestExit(false); return;
        }
        UE_LOG(LogUnRealDash, Display, TEXT("Smoke metrics path=%s"), *MetricsPath);
        ScreenshotPath = Config.OutputDirectory / TEXT("screenshot.png");
        // A weak pointer, not a raw this: EndPlay removes the handle, but a teardown path that
        // does not run EndPlay before the screenshot completes would otherwise write through a
        // dangling pointer. On a device that is a crash after the run has already succeeded.
        TWeakObjectPtr<ASmokeHUD> WeakThis(this);
        ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([WeakThis]() {
            if (ASmokeHUD* Hud = WeakThis.Get()) { Hud->bScreenshotProcessed = true; }
        });
        FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
        bExportRequested = true;
        UE_LOG(LogUnRealDash, Display, TEXT("Smoke screenshot requested path=%s"), *ScreenshotPath);
    }
    // The callback runs after the screenshot frame. Exit on the following draw, never the request frame.
    if (bScreenshotProcessed)
    {
        const FString MetricsPath = Config.OutputDirectory / TEXT("metrics.csv");
        const bool bSaved = FFileHelper::SaveStringToFile(Csv, *MetricsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        const bool bScreenshotExists = IFileManager::Get().FileExists(*ScreenshotPath);
        UE_LOG(LogUnRealDash, Display, TEXT("Smoke metrics path=%s written=%s screenshot path=%s exists=%s"),
            *MetricsPath, bSaved ? TEXT("true") : TEXT("false"), *ScreenshotPath,
            bScreenshotExists ? TEXT("true") : TEXT("false"));
        // The first write reports a failure and the final one used to exit as though it had
        // succeeded, which would hide a device-specific write failure behind a clean exit.
        if (!bSaved || !bScreenshotExists)
        {
            UE_LOG(LogUnRealDash, Error, TEXT("Smoke export incomplete: metrics written=%s screenshot exists=%s"),
                bSaved ? TEXT("true") : TEXT("false"), bScreenshotExists ? TEXT("true") : TEXT("false"));
        }
        bReady = false;
        FPlatformMisc::RequestExit(false);
    }
}
void ASmokeHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ScreenshotHandle.IsValid()) { FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle); }
    Super::EndPlay(Reason);
}

