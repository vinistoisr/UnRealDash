#include "GaugeCluster3D.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UnRealDashLog.h"

namespace
{
// A 250 degree sweep starting low on the left, which is what a car instrument does. Measured in
// turns clockwise from straight up, so the maths below stays free of degree conversions.
constexpr float kSweepStart = 0.375f;
constexpr float kSweepSpan = -0.6944f;

// The cluster is built in the XZ plane and viewed down +Y, so every face points at -Y.
FVector Polar(const FVector& Centre, float Radius, float Turn)
{
    const float Angle = Turn * 2.f * PI;
    return Centre + FVector(FMath::Sin(Angle) * Radius, 0.f, FMath::Cos(Angle) * Radius);
}

FColor Mix(const FLinearColor& Colour, float Scale)
{
    return (Colour * Scale).ToFColor(false);
}
}

void AGaugeCluster3D::FMeshBuffer::Reset()
{
    Vertices.Reset(); Triangles.Reset(); Normals.Reset(); UVs.Reset(); Colours.Reset();
}

void AGaugeCluster3D::FMeshBuffer::Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FColor& Colour)
{
    const int32 Base = Vertices.Num();
    for (const FVector& V : { A, B, C, D })
    {
        Vertices.Add(V);
        Normals.Add(FVector(0.f, -1.f, 0.f));
        UVs.Add(FVector2D::ZeroVector);
        Colours.Add(Colour);
    }
    Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
    Triangles.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
}

void AGaugeCluster3D::FMeshBuffer::Ring(const FVector& Centre, float Inner, float Outer, float FromTurn,
                                        float ToTurn, int32 Segments, const FColor& Colour)
{
    for (int32 Index = 0; Index < Segments; ++Index)
    {
        const float T0 = FMath::Lerp(FromTurn, ToTurn, static_cast<float>(Index) / Segments);
        const float T1 = FMath::Lerp(FromTurn, ToTurn, static_cast<float>(Index + 1) / Segments);
        Quad(Polar(Centre, Outer, T0), Polar(Centre, Outer, T1), Polar(Centre, Inner, T1), Polar(Centre, Inner, T0), Colour);
    }
}

AGaugeCluster3D::AGaugeCluster3D()
{
    PrimaryActorTick.bCanEverTick = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

UTextRenderComponent* AGaugeCluster3D::AddText(const FVector& Location, float Size, const FLinearColor& Colour,
                                               const FString& Text, EHorizTextAligment Align)
{
    UTextRenderComponent* Component = NewObject<UTextRenderComponent>(this);
    Component->SetupAttachment(Root);
    Component->SetRelativeLocation(Location);
    // Text faces +X by default; yaw it round to face the camera at -Y.
    Component->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
    Component->SetHorizontalAlignment(Align);
    Component->SetVerticalAlignment(EVRTA_TextCenter);
    Component->SetWorldSize(Size);
    Component->SetTextRenderColor(Colour.ToFColor(false));
    Component->SetText(FText::FromString(Text));
    if (UMaterialInterface* TextMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/EngineMaterials/DefaultTextMaterialTranslucent.DefaultTextMaterialTranslucent")))
    {
        Component->SetTextMaterial(TextMaterial);
    }
    if (UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")))
    {
        Component->SetFont(Font);
    }
    Component->RegisterComponent();
    return Component;
}

void AGaugeCluster3D::BuildDialStatic(FMeshBuffer& Buffer, const FDial& Dial, int32 MajorTicks, int32 MinorPerMajor)
{
    const float R = Dial.Radius;
    const FVector Face(Dial.Centre.X, 0.f, Dial.Centre.Z);
    const FVector Track(Dial.Centre.X, -1.2f, Dial.Centre.Z);
    const FVector Ticks(Dial.Centre.X, -2.4f, Dial.Centre.Z);
    const FVector& C = Face;

    // 192 segments so the silhouette reads as a circle. The engine cylinder faceted visibly, which
    // is most of what made the first version look cheap.
    Buffer.Ring(C, 0.f, R * 0.995f, 0.f, 1.f, 192, FColor(7, 9, 12, 255));
    Buffer.Ring(C, R * 0.995f, R * 1.055f, 0.f, 1.f, 192, Mix(FLinearColor(0.14f, 0.17f, 0.22f), 1.f));
    Buffer.Ring(C, R * 1.055f, R * 1.075f, 0.f, 1.f, 192, Mix(FLinearColor(0.32f, 0.40f, 0.54f), 1.f));

    // Unlit track that the value arc is drawn over.
    Buffer.Ring(Track, R * 0.80f, R * 0.872f, kSweepStart, kSweepStart + kSweepSpan, 160, FColor(17, 21, 28, 255));

    const int32 TotalTicks = MajorTicks * MinorPerMajor;
    for (int32 Index = 0; Index <= TotalTicks; ++Index)
    {
        const float T = static_cast<float>(Index) / TotalTicks;
        const float Turn = kSweepStart + kSweepSpan * T;
        const bool bMajor = (Index % MinorPerMajor) == 0;
        const bool bRed = T > 0.80f;
        const FLinearColor Base = bRed ? FLinearColor(1.f, 0.20f, 0.14f) : FLinearColor(0.80f, 0.87f, 0.96f);
        const float Inner = bMajor ? R * 0.63f : R * 0.71f;
        const float Half = bMajor ? 0.0040f : 0.0018f;   // in turns, so ticks stay radial
        Buffer.Quad(Polar(Ticks, R * 0.775f, Turn - Half), Polar(Ticks, R * 0.775f, Turn + Half),
                    Polar(Ticks, Inner, Turn + Half), Polar(Ticks, Inner, Turn - Half), Mix(Base, bMajor ? 1.f : 0.5f));
    }
}

void AGaugeCluster3D::RebuildDynamic(FDial& Dial)
{
    const float Fraction = FMath::Clamp(static_cast<float>(Dial.Displayed / Dial.FullScale), 0.f, 1.f);
    const FVector ArcAt(Dial.Centre.X, -3.6f, Dial.Centre.Z);
    const FVector C(Dial.Centre.X, -4.8f, Dial.Centre.Z);
    const float R = Dial.Radius;

    FMeshBuffer B;

    const FLinearColor Arc = FMath::Lerp(Dial.Accent, FLinearColor(1.f, 0.26f, 0.12f),
                                         FMath::Clamp((Fraction - 0.6f) / 0.4f, 0.f, 1.f));
    if (Fraction > 0.002f)
    {
        B.Ring(ArcAt, R * 0.80f, R * 0.872f, kSweepStart, kSweepStart + kSweepSpan * Fraction,
               FMath::Max(2, FMath::RoundToInt(160.f * Fraction)), Mix(Arc, 1.7f));
    }

    // A long tapered blade with a short counterweight, which is what stops it reading as a cone
    // stuck on a disc.
    const float Turn = kSweepStart + kSweepSpan * Fraction;
    const FColor Needle = Mix(FLinearColor(1.f, 0.44f, 0.14f), 2.0f);
    B.Quad(Polar(C, R * 0.735f, Turn), Polar(C, R * 0.14f, Turn + 0.016f),
           Polar(C, -R * 0.12f, Turn), Polar(C, R * 0.14f, Turn - 0.016f), Needle);

    B.Ring(C, 0.f, R * 0.085f, 0.f, 1.f, 48, Mix(FLinearColor(0.09f, 0.11f, 0.15f), 1.f));
    B.Ring(C, R * 0.085f, R * 0.104f, 0.f, 1.f, 48, Mix(Dial.Accent, 1.5f));

    Dial.Dynamic->CreateMeshSection(0, B.Vertices, B.Triangles, B.Normals, B.UVs, B.Colours, TArray<FProcMeshTangent>(), false);

    if (Dial.Readout)
    {
        Dial.Readout->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(Dial.Displayed))));
        Dial.Readout->SetTextRenderColor(Mix(Arc, 1.f));
    }
}

void AGaugeCluster3D::BuildStatic()
{
    // EngineDebugMaterials is editor-only content and is not cooked into a package unless
    // DefaultGame.ini asks for it, which it now does. The fallback exists because the failure mode
    // without a material is a completely black screen with only a warning in the log, which looks
    // like a dead app rather than a missing asset.
    UMaterialInterface* Unlit = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly"));
    if (!Unlit)
    {
        UE_LOG(LogUnRealDash, Error, TEXT("Vertex colour material missing; falling back to a lit material, colours will be wrong"));
        Unlit = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }
    if (!Unlit) { UE_LOG(LogUnRealDash, Error, TEXT("No usable material at all; the cluster will not be visible")); }

    StaticMeshPart = NewObject<UProceduralMeshComponent>(this);
    StaticMeshPart->SetupAttachment(Root);
    StaticMeshPart->bUseAsyncCooking = false;
    StaticMeshPart->SetCastShadow(false);
    StaticMeshPart->RegisterComponent();

    FMeshBuffer B;
    B.Quad(FVector(-430.f, 6.f, 215.f), FVector(430.f, 6.f, 215.f), FVector(430.f, 6.f, 0.f), FVector(-430.f, 6.f, 0.f), FColor(9, 11, 15, 255));
    B.Quad(FVector(-430.f, 6.f, 0.f), FVector(430.f, 6.f, 0.f), FVector(430.f, 6.f, -215.f), FVector(-430.f, 6.f, -215.f), FColor(5, 6, 9, 255));

    for (const FDial& Dial : Dials) { BuildDialStatic(B, Dial, Dial.Radius > 100.f ? 8 : 6, 5); }

    for (int32 Index = 0; Index < 24; ++Index)
    {
        const float X = -228.f + Index * 19.f;
        B.Quad(FVector(X, 4.f, -152.f), FVector(X + 13.f, 4.f, -152.f), FVector(X + 13.f, 4.f, -174.f), FVector(X, 4.f, -174.f), FColor(15, 18, 24, 255));
    }

    StaticMeshPart->CreateMeshSection(0, B.Vertices, B.Triangles, B.Normals, B.UVs, B.Colours, TArray<FProcMeshTangent>(), false);
    if (Unlit) { StaticMeshPart->SetMaterial(0, Unlit); }

    for (FDial& Dial : Dials)
    {
        Dial.Dynamic = NewObject<UProceduralMeshComponent>(this);
        Dial.Dynamic->SetupAttachment(Root);
        Dial.Dynamic->bUseAsyncCooking = false;
        Dial.Dynamic->SetCastShadow(false);
        Dial.Dynamic->RegisterComponent();
        if (Unlit) { Dial.Dynamic->SetMaterial(0, Unlit); }

        // Numerals are what make it read as an instrument rather than a diagram, and they cost one
        // component each.
        const int32 Majors = Dial.Radius > 100.f ? 8 : 6;
        for (int32 Index = 0; Index <= Majors; ++Index)
        {
            const float T = static_cast<float>(Index) / Majors;
            const FVector At = Polar(FVector(Dial.Centre.X, -7.f, Dial.Centre.Z), Dial.Radius * 0.52f, kSweepStart + kSweepSpan * T);
            Labels.Add(AddText(At, Dial.Radius * 0.20f, FLinearColor(0.72f, 0.80f, 0.92f),
                               FString::Printf(TEXT("%d"), FMath::RoundToInt(Dial.FullScale * T)), EHTA_Center));
        }
        Dial.Readout = AddText(FVector(Dial.Centre.X, -7.f, Dial.Centre.Z - Dial.Radius * 0.46f),
                               Dial.Radius * 0.26f, Dial.Accent, TEXT("0"), EHTA_Center);
        Labels.Add(Dial.Readout);
    }
}

void AGaugeCluster3D::BeginPlay()
{
    Super::BeginPlay();

    Dials.SetNum(3);
    Dials[0].Centre = FVector(-250.f, 0.f, 12.f); Dials[0].Radius = 86.f;  Dials[0].FullScale = 8000.0;
    Dials[0].Accent = FLinearColor(0.35f, 0.72f, 1.f);
    Dials[1].Centre = FVector(0.f, 0.f, 12.f);    Dials[1].Radius = 120.f; Dials[1].FullScale = 240.0;
    Dials[1].Accent = FLinearColor(0.30f, 0.92f, 1.f);
    Dials[2].Centre = FVector(250.f, 0.f, 12.f);  Dials[2].Radius = 86.f;  Dials[2].FullScale = 130.0;
    Dials[2].Accent = FLinearColor(1.f, 0.62f, 0.20f);

    BuildStatic();
    Labels.Add(AddText(FVector(0.f, -7.f, -198.f), 20.f, FLinearColor(0.38f, 0.48f, 0.64f), TEXT("UNREALDASH   SPIKE"), EHTA_Center));

    if (UWorld* World = GetWorld())
    {
        // Unlit vertex colour needs no lights at all, which also removes the hotspots and blowouts
        // the lit version kept producing. Bloom supplies the glow.
        if (APostProcessVolume* Post = World->SpawnActor<APostProcessVolume>())
        {
            Post->bUnbound = true;
            Post->Settings.bOverride_AutoExposureMinBrightness = true;
            Post->Settings.bOverride_AutoExposureMaxBrightness = true;
            Post->Settings.AutoExposureMinBrightness = 1.f;
            Post->Settings.AutoExposureMaxBrightness = 1.f;
            Post->Settings.bOverride_BloomIntensity = true;
            Post->Settings.BloomIntensity = 0.75f;
        }
    }

    Simulator = MakeUnique<UnRealDashCore::FSmokeSimulator>(UnRealDashCore::MakePlatformClock());
    UnRealDashCore::FSmokeSimulatorOptions Options;
    Options.Scenario = TEXT("acceleration");
    Options.Seed = 1;
    Options.DurationSeconds = 600.0;
    Options.IntervalMilliseconds = 40;
    const FString Error = Simulator->Start(Options);
    if (!Error.IsEmpty()) { UE_LOG(LogUnRealDash, Error, TEXT("%s"), *Error); return; }

    FPlatformApplicationMisc::ControlScreensaver(FPlatformApplicationMisc::Disable);
    bReady = true;
    UE_LOG(LogUnRealDash, Display, TEXT("Spike 3D cluster: %d dials, %d labels"), Dials.Num(), Labels.Num());
}

void AGaugeCluster3D::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bReady) { return; }
    Elapsed += DeltaSeconds;

    UnRealDashCore::FSignalSample Sample;
    if (Simulator->Tick(Sample).IsEmpty()) { Observed = Sample.Value; }

    // One scenario channel drives the centre dial; the others are derived so the cluster moves as a
    // set. A real dashboard binds each dial to its own signal through the document.
    Dials[1].Value = Observed * 9.0;
    Dials[0].Value = 900.0 + Observed * 250.0 + 700.0 * (1.0 + FMath::Sin(Elapsed * 1.9));
    Dials[2].Value = 78.0 + Observed * 1.9 + 7.0 * FMath::Sin(Elapsed * 0.55);

    for (FDial& Dial : Dials)
    {
        Dial.Displayed = FMath::FInterpTo(Dial.Displayed, Dial.Value, DeltaSeconds, 5.5);
        RebuildDynamic(Dial);
    }

    UpdateInput(DeltaSeconds);
}

void AGaugeCluster3D::UpdateInput(float DeltaSeconds)
{
    APlayerController* Controller = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!Controller) { return; }

    float X1 = 0.f, Y1 = 0.f, X2 = 0.f, Y2 = 0.f;
    bool bTouch1 = false, bTouch2 = false;
    Controller->GetInputTouchState(ETouchIndex::Touch1, X1, Y1, bTouch1);
    Controller->GetInputTouchState(ETouchIndex::Touch2, X2, Y2, bTouch2);

    // Mouse drag stands in for one finger so this is testable on the desktop.
    if (!bTouch1 && Controller->IsInputKeyDown(EKeys::LeftMouseButton))
    {
        float MouseX = 0.f, MouseY = 0.f;
        if (Controller->GetMousePosition(MouseX, MouseY)) { X1 = MouseX; Y1 = MouseY; bTouch1 = true; }
    }

    if (bTouch1 && bTouch2)
    {
        const float Pinch = FVector2D::Distance(FVector2D(X1, Y1), FVector2D(X2, Y2));
        if (bWasPinching)
        {
            // Scale the step by the current distance so zooming feels the same far away as close.
            Distance = FMath::Clamp(Distance - (Pinch - LastPinch) * (Distance / 420.f), 260.f, 2200.f);
            bUserTookControl = true;
        }
        LastPinch = Pinch;
        bWasPinching = true;
        bWasTouching = false;
    }
    else if (bTouch1)
    {
        const FVector2D Now(X1, Y1);
        if (bWasTouching)
        {
            const FVector2D Delta = Now - LastTouch;
            Yaw = FMath::Clamp(Yaw + Delta.X * 0.28f, -75.f, 75.f);
            Pitch = FMath::Clamp(Pitch - Delta.Y * 0.20f, -45.f, 45.f);
            bUserTookControl = true;
        }
        LastTouch = Now;
        bWasTouching = true;
        bWasPinching = false;
    }
    else
    {
        bWasTouching = false;
        bWasPinching = false;
    }

    // Mouse wheel zoom, again so the desktop behaves like the device.
    const float Wheel = Controller->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
    if (!FMath::IsNearlyZero(Wheel))
    {
        Distance = FMath::Clamp(Distance - Wheel * 90.f, 260.f, 2200.f);
        bUserTookControl = true;
    }

    if (!bUserTookControl)
    {
        Yaw = 9.f * FMath::Sin(Elapsed * 0.22f);
    }
    SetActorRotation(FRotator(Pitch, Yaw, 0.f));

    if (AActor* Cam = Camera.Get())
    {
        const FVector Target(0.f, -Distance, 10.f);
        Cam->SetActorLocation(FMath::VInterpTo(Cam->GetActorLocation(), Target, DeltaSeconds, 9.f));
    }
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
    AGaugeCluster3D* Cluster = World->SpawnActor<AGaugeCluster3D>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (ACameraActor* Camera = World->SpawnActor<ACameraActor>(FVector(0.f, -700.f, 10.f), FRotator(0.f, 90.f, 0.f)))
    {
        Camera->GetCameraComponent()->SetFieldOfView(62.f);
        if (Cluster) { Cluster->SetCamera(Camera); }
        if (APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0))
        {
            Controller->SetViewTarget(Camera);
            // Touch state is polled rather than bound, so these have to be on or every finger is
            // dropped before it reaches the game.
            Controller->bEnableTouchEvents = true;
            Controller->bEnableTouchOverEvents = true;
            Controller->bShowMouseCursor = false;
        }
    }
}
