#pragma once
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "Components/TextRenderComponent.h"
#include "UnRealDashCore/SmokeSimulator.h"
#include "GaugeCluster3D.generated.h"

class UProceduralMeshComponent;
class UTextRenderComponent;

// SPIKE ONLY, and deliberately not on the PLAN 4.5 or 4.6 path.
//
// A hand-built 3D instrument cluster. The geometry is generated in C++ rather than assembled from
// engine primitives: engine cylinders and cones are low poly, so circles faceted visibly and tick
// marks were chunky boxes. Everything here is built from triangles at whatever tessellation it
// needs, with per-vertex colour and an unlit material, which also sidesteps BasicShapeMaterial
// refusing to tint.
//
// It is NOT the showcase scene PLAN's Stage 0 goal 1 describes, and not a prototype of the runtime
// primitives, which are built from the document through the component registry. This one hardcodes
// its geometry, which is exactly what those must never do.
//
// Needle values come from signal-core's own scenario replay through FSmokeSimulator, so the data is
// deterministic rather than random.
UCLASS()
class AGaugeCluster3D : public AActor
{
    GENERATED_BODY()
public:
    AGaugeCluster3D();
    void SetCamera(AActor* InCamera) { Camera = InCamera; }
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    // One mesh buffer that every static part of the cluster writes into, so the whole cluster is a
    // single draw. Moving parts get their own buffers because they are rebuilt each frame.
    struct FMeshBuffer
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UVs;
        TArray<FColor> Colours;
        void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FColor& Colour);
        void Ring(const FVector& Centre, float Inner, float Outer, float FromTurn, float ToTurn,
                  int32 Segments, const FColor& Colour);
        void Reset();
    };

    struct FDial
    {
        FVector Centre = FVector::ZeroVector;
        float Radius = 60.f;
        double FullScale = 1.0;
        double Value = 0.0;
        double Displayed = 0.0;      // eased toward Value so the needle settles rather than snaps
        FLinearColor Accent = FLinearColor::White;
        TObjectPtr<UProceduralMeshComponent> Dynamic;   // needle and value arc, rebuilt each frame
        TObjectPtr<UTextRenderComponent> Readout;
    };

    void BuildStatic();
    void BuildDialStatic(FMeshBuffer& Buffer, const FDial& Dial, int32 MajorTicks, int32 MinorPerMajor);
    void RebuildDynamic(FDial& Dial);
    UTextRenderComponent* AddText(const FVector& Location, float Size, const FLinearColor& Colour,
                                  const FString& Text, EHorizTextAligment Align);

    UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> StaticMeshPart;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextRenderComponent>> Labels;
    TArray<FDial> Dials;
    TUniquePtr<UnRealDashCore::FSmokeSimulator> Simulator;
    // Touch and mouse control. One finger orbits, two fingers pinch to zoom. The idle orbit stops
    // the moment the user takes hold of it and does not come back, because a view that keeps
    // drifting under your finger feels broken rather than alive.
    void UpdateInput(float DeltaSeconds);
    TWeakObjectPtr<AActor> Camera;
    FVector2D LastTouch = FVector2D::ZeroVector;
    float LastPinch = 0.f;
    float Yaw = 0.f;
    float Pitch = 0.f;
    float Distance = 700.f;
    bool bWasTouching = false;
    bool bWasPinching = false;
    bool bUserTookControl = false;
    double Elapsed = 0.0;
    double Observed = 0.0;
    bool bReady = false;
};

UCLASS()
class AGaugeCluster3DGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AGaugeCluster3DGameMode();
    virtual void StartPlay() override;
};
