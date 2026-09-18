#pragma once
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "UnRealDashCore/SmokeSimulator.h"
#include "GaugeCluster3D.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

// SPIKE ONLY, and deliberately not on the PLAN 4.5 or 4.6 path.
//
// A hand-built 3D instrument cluster, assembled from engine primitives in C++ so it needs no
// editor work and no cooked assets of its own. It exists to make the project visible: the owner
// asked to see something moving.
//
// It is NOT the showcase scene PLAN's Stage 0 goal 1 describes, and it is not a prototype of the
// runtime primitives. Those are built from the document through the component registry. This one
// hardcodes its geometry, which is exactly what the real ones must never do.
//
// The needle values come from signal-core's own scenario replay through FSmokeSimulator, so the
// data is deterministic rather than random. A gauge driven by rand() would look the same and prove
// nothing about the pipeline.
UCLASS()
class AGaugeCluster3D : public AActor
{
    GENERATED_BODY()
public:
    AGaugeCluster3D();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    struct FDial
    {
        TObjectPtr<USceneComponent> Pivot;
        TObjectPtr<USceneComponent> Needle;   // pivot the needle mesh hangs off
        TObjectPtr<UMaterialInstanceDynamic> ArcMaterial;
        TArray<TObjectPtr<UMaterialInstanceDynamic>> TickMaterials;
        double FullScale = 1.0;
        double Value = 0.0;
        double Displayed = 0.0;   // eased toward Value so the needle does not snap
        float Radius = 60.f;
    };

    UStaticMeshComponent* AddMesh(USceneComponent* Parent, const TCHAR* MeshPath, FVector Location,
                                  FRotator Rotation, FVector Scale, FLinearColor Colour, bool bEmissive);
    void BuildDial(FDial& Dial, USceneComponent* Parent, const FVector& Location, float Radius,
                   const FLinearColor& Accent, int32 TickCount);

    UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> BarSegments;
    TArray<FDial> Dials;
    TUniquePtr<UnRealDashCore::FSmokeSimulator> Simulator;
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
