#include "GaugeCluster3D.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnRealDashLog.h"

namespace
{
constexpr float kSweepDegrees = 250.f;      // A physical gauge sweeps about this much.
constexpr float kSweepStartDegrees = -125.f;

// BasicShapeMaterial exposes a "Color" vector parameter. Setting a parameter a material does not
// have is a no-op rather than an error, so a rename upstream costs colour, never a crash.
UMaterialInstanceDynamic* MakeColour(UObject* Outer, UMeshComponent* Target, const FLinearColor& Colour, bool bEmissive)
{
    UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Base) { return nullptr; }
    UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Outer);
    if (!Instance) { return nullptr; }
    // Confirmed by dumping the material's parameters: it exposes Color and Roughness and is LIT.
    // Base colour clamps at 1, so multiplying it does not make anything glow. The bright parts are
    // made bright by being near white and very smooth, and by the lights below being strong.
    Instance->SetVectorParameterValue(TEXT("Color"), Colour);
    Instance->SetScalarParameterValue(TEXT("Roughness"), bEmissive ? 0.12f : 0.65f);
    Target->SetMaterial(0, Instance);
    return Instance;
}
}

AGaugeCluster3D::AGaugeCluster3D()
{
    PrimaryActorTick.bCanEverTick = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

UStaticMeshComponent* AGaugeCluster3D::AddMesh(USceneComponent* Parent, const TCHAR* MeshPath, FVector Location,
                                               FRotator Rotation, FVector Scale, FLinearColor Colour, bool bEmissive)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
    if (!Mesh) { UE_LOG(LogUnRealDash, Error, TEXT("Missing mesh %s"), MeshPath); return nullptr; }
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
    Component->SetStaticMesh(Mesh);
    Component->SetupAttachment(Parent);
    Component->SetRelativeLocation(Location);
    Component->SetRelativeRotation(Rotation);
    Component->SetRelativeScale3D(Scale);
    Component->SetCastShadow(false);
    Component->RegisterComponent();
    MakeColour(this, Component, Colour, bEmissive);
    return Component;
}

void AGaugeCluster3D::BuildDial(FDial& Dial, USceneComponent* Parent, const FVector& Location, float Radius,
                                const FLinearColor& Accent, int32 TickCount)
{
    Dial.Radius = Radius;

    USceneComponent* Pivot = NewObject<USceneComponent>(this);
    Pivot->SetupAttachment(Parent);
    Pivot->SetRelativeLocation(Location);
    Pivot->RegisterComponent();
    Dial.Pivot = Pivot;

    const float R = Radius / 50.f;   // The engine cylinder is 100 units across.

    // Bezel, then a darker face slightly proud of it, so the dial reads as recessed.
    AddMesh(Pivot, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0, 0, 0), FRotator(0, 0, 90.f),
            FVector(R * 1.08f, R * 1.08f, 0.06f), FLinearColor(0.06f, 0.07f, 0.09f), false);
    AddMesh(Pivot, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0, -3.f, 0), FRotator(0, 0, 90.f),
            FVector(R, R, 0.05f), FLinearColor(0.015f, 0.018f, 0.022f), false);

    // Tick marks around the sweep. The last few glow, which is what a redline looks like before
    // anyone has written a rule engine binding for it.
    for (int32 Index = 0; Index < TickCount; ++Index)
    {
        const float T = static_cast<float>(Index) / FMath::Max(1, TickCount - 1);
        const float Angle = FMath::DegreesToRadians(kSweepStartDegrees + kSweepDegrees * T);
        const bool bRed = T > 0.78f;
        const bool bMajor = (Index % 2) == 0;
        const FVector Position(FMath::Sin(Angle) * Radius * 0.84f, -6.f, FMath::Cos(Angle) * Radius * 0.84f);
        UStaticMeshComponent* Tick = AddMesh(Pivot, TEXT("/Engine/BasicShapes/Cube.Cube"), Position,
            FRotator(-FMath::RadiansToDegrees(Angle), 0, 0),
            FVector(0.035f, 0.035f, bMajor ? 0.16f : 0.10f) * R * 1.6f,
            bRed ? FLinearColor(1.f, 0.15f, 0.12f) : FLinearColor(0.75f, 0.80f, 0.88f), true);
        if (Tick) { Dial.TickMaterials.Add(Cast<UMaterialInstanceDynamic>(Tick->GetMaterial(0))); }
    }

    // Needle: a slim tapered bar offset so it pivots about the hub rather than its own centre.
    USceneComponent* NeedlePivot = NewObject<USceneComponent>(this);
    NeedlePivot->SetupAttachment(Pivot);
    NeedlePivot->SetRelativeLocation(FVector(0, -16.f, 0));
    NeedlePivot->RegisterComponent();

    UStaticMeshComponent* Needle = AddMesh(NeedlePivot, TEXT("/Engine/BasicShapes/Cone.Cone"),
        FVector(0, 0, Radius * 0.44f), FRotator(0, 0, 0),   // cone +Z is up; pivot rolls it
        FVector(0.075f * R, 0.075f * R, Radius / 100.f * 1.25f), FLinearColor(1.f, 0.30f, 0.12f), true);
    Dial.Needle = NeedlePivot;
    (void)Needle;

    // Hub cap over the needle root.
    AddMesh(Pivot, TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(0, -11.f, 0), FRotator::ZeroRotator,
            FVector(0.07f * R), FLinearColor(0.85f, 0.88f, 0.95f), true);

    UStaticMeshComponent* Arc = AddMesh(Pivot, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
        FVector(0, -4.f, 0), FRotator(0, 0, 90.f), FVector(R * 0.30f, R * 0.30f, 0.055f), Accent, true);
    if (Arc) { Dial.ArcMaterial = Cast<UMaterialInstanceDynamic>(Arc->GetMaterial(0)); }
}

void AGaugeCluster3D::BeginPlay()
{
    Super::BeginPlay();

    // Backing plate, so the cluster sits on something rather than floating in void.
    AddMesh(Root, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0, 22.f, 0), FRotator::ZeroRotator,
            FVector(6.6f, 0.22f, 2.0f), FLinearColor(0.012f, 0.014f, 0.018f), false);

    Dials.SetNum(3);
    BuildDial(Dials[0], Root, FVector(-215.f, 0, 0), 66.f, FLinearColor(0.15f, 0.65f, 0.95f), 11);  // left
    BuildDial(Dials[1], Root, FVector(0, 0, 6.f), 94.f, FLinearColor(0.20f, 0.80f, 1.00f), 15);    // centre
    BuildDial(Dials[2], Root, FVector(215.f, 0, 0), 66.f, FLinearColor(0.95f, 0.55f, 0.15f), 11);   // right

    // A segmented bar under the cluster, the cheapest way to show a second channel moving.
    for (int32 Index = 0; Index < 16; ++Index)
    {
        UStaticMeshComponent* Segment = AddMesh(Root,
            TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(-120.f + Index * 16.f, -6.f, -118.f),
            FRotator::ZeroRotator, FVector(0.11f, 0.06f, 0.30f), FLinearColor(0.10f, 0.12f, 0.16f), false);
        if (Segment) { BarSegments.Add(Cast<UMaterialInstanceDynamic>(Segment->GetMaterial(0))); }
    }

    UWorld* World = GetWorld();
    if (World)
    {
        ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, -400.f, 400.f), FRotator(-35.f, 60.f, 0));
        if (Sun)
        {
            Sun->SetMobility(EComponentMobility::Movable);
            Sun->GetComponent()->SetIntensity(0.9f);
            Sun->GetComponent()->SetLightColor(FLinearColor(1.f, 0.97f, 0.92f));
        }
        ASkyLight* Sky = World->SpawnActor<ASkyLight>();
        if (Sky)
        {
            Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            Sky->GetLightComponent()->SetIntensity(0.6f);
            Sky->GetLightComponent()->SetLightColor(FLinearColor(0.40f, 0.52f, 0.78f));
        }
        // Key light in front of the cluster. Without it the dial faces point at the camera and away
        // from every other light, which is what made the first render read as flat grey.
        APointLight* Key = World->SpawnActor<APointLight>(FVector(0, -260.f, 60.f), FRotator::ZeroRotator);
        if (Key)
        {
            Key->SetMobility(EComponentMobility::Movable);
            Key->PointLightComponent->SetIntensity(9000.f);
            Key->PointLightComponent->SetAttenuationRadius(1400.f);
            Key->PointLightComponent->SetLightColor(FLinearColor(0.80f, 0.90f, 1.f));
        }
        APointLight* Warm = World->SpawnActor<APointLight>(FVector(-260.f, -180.f, -90.f), FRotator::ZeroRotator);
        if (Warm)
        {
            Warm->SetMobility(EComponentMobility::Movable);
            Warm->PointLightComponent->SetIntensity(22000.f);
            Warm->PointLightComponent->SetAttenuationRadius(1200.f);
            Warm->PointLightComponent->SetLightColor(FLinearColor(1.f, 0.55f, 0.25f));
        }
        // Fixed exposure. Auto exposure hunts on a mostly black frame and blew the first lit render
        // to pure white; a cluster should look the same every time it is photographed anyway.
        APostProcessVolume* Post = World->SpawnActor<APostProcessVolume>();
        if (Post)
        {
            Post->bUnbound = true;
            // Pin exposure by clamping the histogram range to a single value. AEM_Manual derives
            // exposure from physical camera settings, which rendered the scene black; clamping min
            // and max is predictable and needs no aperture or ISO maths.
            Post->Settings.bOverride_AutoExposureMinBrightness = true;
            Post->Settings.bOverride_AutoExposureMaxBrightness = true;
            Post->Settings.AutoExposureMinBrightness = 1.f;
            Post->Settings.AutoExposureMaxBrightness = 1.f;
            Post->Settings.bOverride_AutoExposureBias = true;
            Post->Settings.AutoExposureBias = 0.f;
            Post->Settings.bOverride_BloomIntensity = true;
            Post->Settings.BloomIntensity = 0.55f;
        }
        // One coloured light per dial. The engine's basic material will not tint, so the colour in
        // this scene is lighting rather than surface. It is a spike; a real dashboard gets its
        // colour from theme tokens in the document.
        const FLinearColor DialColours[3] = { FLinearColor(0.25f, 0.65f, 1.f), FLinearColor(0.30f, 0.95f, 1.f), FLinearColor(1.f, 0.55f, 0.18f) };
        const float DialX[3] = { -215.f, 0.f, 215.f };
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (APointLight* Rim = World->SpawnActor<APointLight>(FVector(DialX[Index], -230.f, 10.f), FRotator::ZeroRotator))
            {
                Rim->SetMobility(EComponentMobility::Movable);
                Rim->PointLightComponent->SetIntensity(16000.f);
                Rim->PointLightComponent->SetAttenuationRadius(520.f);
                Rim->PointLightComponent->SetLightColor(DialColours[Index]);
            }
        }
        UE_LOG(LogUnRealDash, Display, TEXT("Spike 3D lights: sun=%d sky=%d key=%d warm=%d"),
            Sun ? 1 : 0, Sky ? 1 : 0, Key ? 1 : 0, Warm ? 1 : 0);
    }

    // Ask the material what it actually exposes rather than guessing a parameter name. The first
    // attempt set "Color", which this material does not have, and a missing parameter is silently
    // ignored, so everything rendered in default grey with no error anywhere.
    if (UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        TArray<FMaterialParameterInfo> Infos;
        TArray<FGuid> Guids;
        Base->GetAllVectorParameterInfo(Infos, Guids);
        for (const FMaterialParameterInfo& Info : Infos) { UE_LOG(LogUnRealDash, Display, TEXT("vector param: %s"), *Info.Name.ToString()); }
        Base->GetAllScalarParameterInfo(Infos, Guids);
        for (const FMaterialParameterInfo& Info : Infos) { UE_LOG(LogUnRealDash, Display, TEXT("scalar param: %s"), *Info.Name.ToString()); }
        UE_LOG(LogUnRealDash, Display, TEXT("BasicShapeMaterial shading model lit=%d"), Base->GetShadingModels().IsLit() ? 1 : 0);
    }
    Simulator = MakeUnique<UnRealDashCore::FSmokeSimulator>(UnRealDashCore::MakePlatformClock());
    UnRealDashCore::FSmokeSimulatorOptions Options;
    Options.Scenario = TEXT("acceleration");
    Options.Seed = 1;
    Options.DurationSeconds = 600.0;
    Options.IntervalMilliseconds = 40;
    const FString Error = Simulator->Start(Options);
    if (!Error.IsEmpty()) { UE_LOG(LogUnRealDash, Error, TEXT("%s"), *Error); return; }

    Dials[0].FullScale = 8000.0;
    Dials[1].FullScale = 30.0;
    Dials[2].FullScale = 130.0;
    bReady = true;
    UE_LOG(LogUnRealDash, Display, TEXT("Spike 3D cluster built: %d dials, %d bar segments"), Dials.Num(), BarSegments.Num());
}

void AGaugeCluster3D::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bReady) { return; }
    Elapsed += DeltaSeconds;

    UnRealDashCore::FSignalSample Sample;
    const FString Error = Simulator->Tick(Sample);
    if (Error.IsEmpty()) { Observed = Sample.Value; }

    // One scenario channel drives the centre dial directly; the flanking two are derived from it so
    // the cluster moves as a set. A real dashboard binds each dial to its own signal, which is what
    // the document model is for. This is a spike and says so.
    Dials[1].Value = Observed;
    Dials[0].Value = Observed * 260.0 + 800.0 * (1.0 + FMath::Sin(Elapsed * 1.7));
    Dials[2].Value = 80.0 + Observed * 1.4 + 6.0 * FMath::Sin(Elapsed * 0.6);

    for (FDial& Dial : Dials)
    {
        // Ease toward the target so the needle settles instead of snapping, which is what makes it
        // read as an instrument rather than a slider.
        Dial.Displayed = FMath::FInterpTo(Dial.Displayed, Dial.Value, DeltaSeconds, 6.0);
        const float Fraction = FMath::Clamp(static_cast<float>(Dial.Displayed / Dial.FullScale), 0.f, 1.f);
        if (Dial.Needle)
        {
            Dial.Needle->SetRelativeRotation(FRotator(-(kSweepStartDegrees + kSweepDegrees * Fraction), 0, 0));
        }
        if (Dial.ArcMaterial)
        {
            const FLinearColor Hot(1.f, 0.25f, 0.12f);
            const FLinearColor Cool(0.15f, 0.75f, 1.f);
            Dial.ArcMaterial->SetVectorParameterValue(TEXT("Color"),
                FMath::Lerp(Cool, Hot, FMath::Clamp((Fraction - 0.55f) / 0.45f, 0.f, 1.f)) * 3.f);
        }
        for (int32 Index = 0; Index < Dial.TickMaterials.Num(); ++Index)
        {
            if (!Dial.TickMaterials[Index]) { continue; }
            const float T = static_cast<float>(Index) / FMath::Max(1, Dial.TickMaterials.Num() - 1);
            const bool bLit = T <= Fraction;
            const FLinearColor Base = T > 0.78f ? FLinearColor(1.f, 0.15f, 0.12f) : FLinearColor(0.75f, 0.80f, 0.88f);
            Dial.TickMaterials[Index]->SetVectorParameterValue(TEXT("Color"), bLit ? Base * 4.f : Base * 0.25f);
        }
    }

    const float BarFraction = FMath::Clamp(static_cast<float>(Dials[1].Displayed / Dials[1].FullScale), 0.f, 1.f);
    for (int32 Index = 0; Index < BarSegments.Num(); ++Index)
    {
        if (!BarSegments[Index]) { continue; }
        const bool bLit = (static_cast<float>(Index) / BarSegments.Num()) < BarFraction;
        const FLinearColor Lit = Index > BarSegments.Num() * 3 / 4 ? FLinearColor(1.f, 0.35f, 0.12f) : FLinearColor(0.20f, 0.85f, 1.f);
        BarSegments[Index]->SetVectorParameterValue(TEXT("Color"), bLit ? Lit * 3.5f : FLinearColor(0.07f, 0.08f, 0.11f));
    }

    // Slow orbit, so a still screenshot still shows it is a 3D scene and not a flat image.
    const float Orbit = 14.f * FMath::Sin(Elapsed * 0.25f);
    SetActorRotation(FRotator(0, Orbit, 0));
}

AGaugeCluster3DGameMode::AGaugeCluster3DGameMode()
{
    DefaultPawnClass = nullptr;
}

void AGaugeCluster3DGameMode::StartPlay()
{
    Super::StartPlay();
    UWorld* World = GetWorld();
    if (!World) { return; }
    World->SpawnActor<AGaugeCluster3D>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (ACameraActor* Camera = World->SpawnActor<ACameraActor>(FVector(0, -640.f, 40.f), FRotator(-3.f, 90.f, 0)))
    {
        Camera->GetCameraComponent()->SetFieldOfView(60.f);
        if (APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0))
        {
            Controller->SetViewTarget(Camera);
        }
    }
}
