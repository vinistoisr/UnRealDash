#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Widgets/SLeafWidget.h"
#include "DashLines.generated.h"

// One polyline painter, shared by the circular bar gauge's arc and the history graph's trace.
//
// Slate has no arc widget and no polyline widget a document can position, and both of those shapes
// reduce to a point list, so they get one painter between them. Two custom painters for the same
// operation would be two places for the same bug.
//
// Points are in the widget's own local space, normalized 0 to 1 on each axis, so a caller does not
// have to know the allotted geometry and the same point list draws correctly at any size. That is
// what keeps the arc circular when the widget is square, and it is the caller's job to keep it
// square; see the dial and gauge builders for how the largest centred square is chosen.
class SDashLines : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDashLines) {}
        SLATE_ARGUMENT(TArray<FVector2D>, Points)
        SLATE_ARGUMENT(FLinearColor, Colour)
        SLATE_ARGUMENT(float, Thickness)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetLines(const TArray<FVector2D>& InPoints, const FLinearColor& InColour, float InThickness);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

private:
    TArray<FVector2D> Points;
    FLinearColor Colour = FLinearColor::White;
    float Thickness = 1.f;
};

UCLASS()
class UDashLines : public UWidget
{
    GENERATED_BODY()
public:
    void SetLines(const TArray<FVector2D>& InPoints, const FLinearColor& InColour, float InThickness);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    TSharedPtr<SDashLines> Line;
    TArray<FVector2D> Points;
    FLinearColor Colour = FLinearColor::White;
    float Thickness = 1.f;
};
