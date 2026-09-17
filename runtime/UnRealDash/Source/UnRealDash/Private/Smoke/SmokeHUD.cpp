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
#include "HAL/PlatformApplicationMisc.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"
#include "DynamicRHI.h"
#include "RHI.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

ASmokeGameMode::ASmokeGameMode()
{
    HUDClass = ASmokeHUD::StaticClass();
    DefaultPawnClass = nullptr;
}

// SMOKE SPIKE ONLY. Everything between here and the next marker exists so a human can watch the
// device and judge whether it feels smooth. PLAN 4.5 and 4.6 own the real dial, bar and graph,
// built from the document through the component registry. None of this is their starting point.
namespace
{
constexpr float kTau = 6.28318530718f;

// A thick line is the most portable filled rectangle UCanvas offers. K2_DrawBox strokes an
// outline, and DrawTile needs a texture this spike has no reason to carry.
void FillBar(UCanvas* Canvas, const FVector2D& Origin, float Width, float Height, const FLinearColor& Colour)
{
    if (Width <= 0.f || Height <= 0.f) { return; }
    const float Middle = Origin.Y + Height * 0.5f;
    Canvas->K2_DrawLine(FVector2D(Origin.X, Middle), FVector2D(Origin.X + Width, Middle), Height, Colour);
}

void DrawArc(UCanvas* Canvas, const FVector2D& Centre, float Radius, float FromTurns, float ToTurns,
             float Thickness, const FLinearColor& Colour, int32 Segments = 48)
{
    FVector2D Previous = FVector2D::ZeroVector;
    for (int32 Index = 0; Index <= Segments; ++Index)
    {
        const float Turns = FMath::Lerp(FromTurns, ToTurns, static_cast<float>(Index) / Segments);
        const FVector2D Point(Centre.X + Radius * FMath::Cos(Turns * kTau),
                              Centre.Y + Radius * FMath::Sin(Turns * kTau));
        if (Index > 0) { Canvas->K2_DrawLine(Previous, Point, Thickness, Colour); }
        Previous = Point;
    }
}

// The needle sweeps the same 240 degree span a physical gauge does, starting bottom-left.
constexpr float kSweepStart = 0.375f;
constexpr float kSweepEnd = 1.0f;

void DrawDial(UCanvas* Canvas, UFont* Font, const FVector2D& Centre, float Radius, float Fraction,
              const FLinearColor& Accent, const FString& Label)
{
    DrawArc(Canvas, Centre, Radius, kSweepStart, kSweepEnd, 3.f, FLinearColor(0.18f, 0.20f, 0.24f));
    DrawArc(Canvas, Centre, Radius, kSweepStart, FMath::Lerp(kSweepStart, kSweepEnd, Fraction), 6.f, Accent);
    for (int32 Tick = 0; Tick <= 8; ++Tick)
    {
        const float Turns = FMath::Lerp(kSweepStart, kSweepEnd, static_cast<float>(Tick) / 8.f);
        const FVector2D Direction(FMath::Cos(Turns * kTau), FMath::Sin(Turns * kTau));
        Canvas->K2_DrawLine(Centre + Direction * (Radius - 14.f), Centre + Direction * (Radius - 2.f),
                            2.f, FLinearColor(0.45f, 0.48f, 0.55f));
    }
    const float NeedleTurns = FMath::Lerp(kSweepStart, kSweepEnd, Fraction);
    const FVector2D Needle(FMath::Cos(NeedleTurns * kTau), FMath::Sin(NeedleTurns * kTau));
    Canvas->K2_DrawLine(Centre - Needle * 12.f, Centre + Needle * (Radius - 18.f), 4.f, FLinearColor::White);
    DrawArc(Canvas, Centre, 7.f, 0.f, 1.f, 5.f, FLinearColor::White, 20);
    if (Font) { Canvas->K2_DrawText(Font, Label, Centre + FVector2D(-Radius * 0.35f, Radius * 0.45f)); }
}

// Takes raw values and scales them against the window's own range, so a climbing signal shows a
// ramp rather than a flat line pinned to the top.
void DrawHistory(UCanvas* Canvas, const FVector2D& Origin, float Width, float Height,
                 const TArray<float>& Samples, const FLinearColor& Accent)
{
    Canvas->K2_DrawBox(Origin, FVector2D(Width, Height), 1.5f, FLinearColor(0.20f, 0.22f, 0.27f));
    if (Samples.Num() < 2) { return; }
    float Low = Samples[0], High = Samples[0];
    for (float Value : Samples) { Low = FMath::Min(Low, Value); High = FMath::Max(High, Value); }
    const float Span = High - Low;
    const float Step = Width / static_cast<float>(Samples.Num() - 1);
    auto Y = [&](float Value) {
        const float Normalized = Span > 0.f ? (Value - Low) / Span : 0.5f;
        // Inset so a flat trace at either extreme is still visibly inside the box.
        return Origin.Y + Height - 4.f - Normalized * (Height - 8.f);
    };
    for (int32 Index = 1; Index < Samples.Num(); ++Index)
    {
        Canvas->K2_DrawLine(FVector2D(Origin.X + Step * (Index - 1), Y(Samples[Index - 1])),
                            FVector2D(Origin.X + Step * Index, Y(Samples[Index])), 2.f, Accent);
    }
}

float Percentile(TArray<float> Values, float Fraction)
{
    if (Values.Num() == 0) { return 0.f; }
    Values.Sort();
    const int32 Index = FMath::Clamp(static_cast<int32>(Fraction * (Values.Num() - 1)), 0, Values.Num() - 1);
    return Values[Index];
}
} // namespace
// End SMOKE SPIKE ONLY block.
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
    // Games hold the screen awake while they are foreground. Without this the device locks
    // mid-run, which on the first device pass turned a live capture into a lock screen photo.
    FPlatformApplicationMisc::ControlScreensaver(FPlatformApplicationMisc::Disable);
    ProbeStandardFileApis(Config.Image, Saved);
    bReady = true;
}

// SPIKE PROBE for PLAN 4.4. dashboard-spec reads every file through std::ifstream over a
// std::filesystem::path, and its traversal, symlink, hard-link and case rejections are built on
// symlink_status, hard_link_count and canonical. If any of that is inert under Epic's Android
// toolchain then porting that library into the engine needs a different plan, and finding out
// mid-build is the expensive way. This answers it against a real pushed file on the device.
// Delete this with the rest of the smoke spike.
void ASmokeHUD::ProbeStandardFileApis(const FString& ImagePath, const FString& SavedDirectory)
{
    const std::string Image(TCHAR_TO_UTF8(*ImagePath));
    const std::string Directory(TCHAR_TO_UTF8(*SavedDirectory));
    std::error_code Code;

    std::ifstream Stream(Image, std::ios::binary);
    std::string Bytes;
    if (Stream)
    {
        Bytes.resize(4096);
        Stream.read(Bytes.data(), static_cast<std::streamsize>(Bytes.size()));
        Bytes.resize(static_cast<std::size_t>(Stream.gcount()));
    }
    UE_LOG(LogUnRealDash, Display, TEXT("probe ifstream open=%s read_bytes=%d path=%s"),
        Stream ? TEXT("true") : TEXT("false"), static_cast<int32>(Bytes.size()), *ImagePath);

    const bool bIsDirectory = std::filesystem::is_directory(Directory, Code);
    UE_LOG(LogUnRealDash, Display, TEXT("probe is_directory=%s error=%d"),
        bIsDirectory ? TEXT("true") : TEXT("false"), Code.value());

    const auto Status = std::filesystem::symlink_status(Image, Code);
    UE_LOG(LogUnRealDash, Display, TEXT("probe symlink_status is_symlink=%s error=%d"),
        std::filesystem::is_symlink(Status) ? TEXT("true") : TEXT("false"), Code.value());

    const auto Links = std::filesystem::hard_link_count(Image, Code);
    UE_LOG(LogUnRealDash, Display, TEXT("probe hard_link_count=%llu error=%d"),
        static_cast<uint64>(Code ? 0 : Links), Code.value());

    const auto Canonical = std::filesystem::canonical(Image, Code);
    UE_LOG(LogUnRealDash, Display, TEXT("probe canonical error=%d path=%s"),
        Code.value(), UTF8_TO_TCHAR(Canonical.string().c_str()));

    const auto Size = std::filesystem::file_size(Image, Code);
    UE_LOG(LogUnRealDash, Display, TEXT("probe file_size=%llu error=%d"),
        static_cast<uint64>(Code ? 0 : Size), Code.value());

    int32 Entries = 0;
    for (std::filesystem::recursive_directory_iterator It(Directory, std::filesystem::directory_options::none, Code), End;
         It != End && !Code; ++It) { ++Entries; if (Entries > 64) { break; } }
    UE_LOG(LogUnRealDash, Display, TEXT("probe recursive_directory_iterator entries=%d error=%d"), Entries, Code.value());
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
    const float FrameMs = static_cast<float>((Now - PreviousTime) * 1000.0);

    // SMOKE SPIKE ONLY: a visual read on the device. See the note above the helpers.
    if (!bRangeSeen) { ObservedMinimum = ObservedMaximum = Sample.Value; bRangeSeen = true; }
    ObservedMinimum = FMath::Min(ObservedMinimum, Sample.Value);
    ObservedMaximum = FMath::Max(ObservedMaximum, Sample.Value);

    // A real gauge has a fixed full scale, and the needle sits somewhere inside it. Normalizing
    // against the running maximum instead pins a monotonically climbing signal at 1.0 forever,
    // which is what the first device capture showed: a full bar and a flat history line. Full
    // scale grows in steps and keeps headroom, so the needle moves and never leaves the dial.
    FullScale = FMath::Max(FullScale, FMath::GridSnap(ObservedMaximum * 1.3, 5.0));
    const float Fraction = FullScale > 0.0 ? static_cast<float>(Sample.Value / FullScale) : 0.f;

    // Raw values, normalized at draw time, so the trace keeps its shape as the scale changes.
    if (ValueHistory.Num() >= HistorySize) { ValueHistory.RemoveAt(0, 1, EAllowShrinking::No); }
    ValueHistory.Add(static_cast<float>(Sample.Value));
    if (Frame > 0)
    {
        if (FrameMilliseconds.Num() >= HistorySize) { FrameMilliseconds.RemoveAt(0, 1, EAllowShrinking::No); }
        FrameMilliseconds.Add(FrameMs);
    }

    const float W = static_cast<float>(Canvas->SizeX);
    const float H = static_cast<float>(Canvas->SizeY);
    // Compared as the adapter's string rather than signal_core::Quality::valid on purpose: the
    // game module is not allowed to name signal-core, and chunk 08 adds a check that enforces it.
    const FLinearColor Accent = Quality == TEXT("valid")
        ? FLinearColor(0.25f, 0.78f, 0.95f) : FLinearColor(0.95f, 0.62f, 0.20f);

    Canvas->K2_DrawText(Font, FString::Printf(TEXT("%.4f  quality=%s  age_evidence=%s"), Sample.Value, *Quality, *Age), FVector2D(24, 24));

    float Average = 0.f;
    for (float Value : FrameMilliseconds) { Average += Value; }
    Average = FrameMilliseconds.Num() ? Average / FrameMilliseconds.Num() : 0.f;
    const float P95 = Percentile(FrameMilliseconds, 0.95f);
    const float P99 = Percentile(FrameMilliseconds, 0.99f);
    Canvas->K2_DrawText(Font, FString::Printf(
        TEXT("%.1f fps   frame %.2f ms   avg %.2f   p95 %.2f   p99 %.2f   rhi=%s   %dx%d"),
        Average > 0.f ? 1000.f / Average : 0.f, FrameMs, Average, P95, P99,
        GDynamicRHI ? GDynamicRHI->GetName() : TEXT("?"), Canvas->SizeX, Canvas->SizeY), FVector2D(24, 52));

    const float Radius = FMath::Clamp(FMath::Min(W * 0.16f, H * 0.30f), 40.f, 220.f);
    DrawDial(Canvas, Font, FVector2D(W * 0.22f, H * 0.52f), Radius, FMath::Clamp(Fraction, 0.f, 1.f), Accent,
             FString::Printf(TEXT("%.2f"), Sample.Value));

    const float BarX = W * 0.42f, BarY = H * 0.40f, BarW = W * 0.50f, BarH = 34.f;
    Canvas->K2_DrawBox(FVector2D(BarX, BarY), FVector2D(BarW, BarH), 1.5f, FLinearColor(0.20f, 0.22f, 0.27f));
    FillBar(Canvas, FVector2D(BarX + 2.f, BarY + 2.f), (BarW - 4.f) * FMath::Clamp(Fraction, 0.f, 1.f), BarH - 4.f, Accent);

    DrawHistory(Canvas, FVector2D(BarX, BarY + BarH + 24.f), BarW, H * 0.26f, ValueHistory, Accent);
    Canvas->K2_DrawText(Font, FString::Printf(TEXT("last %d samples   seen %.2f to %.2f   full scale %.0f"),
        ValueHistory.Num(), ObservedMinimum, ObservedMaximum, FullScale), FVector2D(BarX, BarY + BarH + 28.f + H * 0.26f));

    // The imported PNG and the material quad stay: they are what 4.0 actually proved.
    const float Size = FMath::Max(1.f, FMath::Min(W * 0.10f, H * 0.18f));
    Canvas->K2_DrawTexture(Texture, FVector2D(W - Size - 24.f, 24.f), FVector2D(Size, Size), FVector2D::ZeroVector);
    Material->SetScalarParameterValue(TEXT("Value"), static_cast<float>(Sample.Value));
    Canvas->K2_DrawMaterial(Material, FVector2D(W - Size - 24.f, 32.f + Size), FVector2D(Size, Size), FVector2D::ZeroVector);
    // End SMOKE SPIKE ONLY.

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
        // bShowUI must be true. Everything this spike draws is HUD, so capturing without the UI
        // writes a black PNG of an empty scene, which is what the first device run produced.
        FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
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

