#include "DashLines.h"
#include "Rendering/DrawElements.h"

void SDashLines::Construct(const FArguments& InArgs)
{
    Points = InArgs._Points;
    Colour = InArgs._Colour;
    Thickness = InArgs._Thickness;
    SetCanTick(false);
}
void SDashLines::SetLines(const TArray<FVector2D>& InPoints, const FLinearColor& InColour, float InThickness)
{
    Points = InPoints;
    Colour = InColour;
    Thickness = InThickness;
    Invalidate(EInvalidateWidgetReason::Paint);
}
int32 SDashLines::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& CullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const
{
    // Fewer than two points is not a line. Drawing nothing is the honest result, and the callers
    // that can reach this state present their missing-data rendering instead.
    if (Points.Num() < 2) return LayerId;

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    TArray<FVector2f> Local;
    Local.Reserve(Points.Num());
    for (const FVector2D& Point : Points)
        Local.Add(FVector2f(static_cast<float>(Point.X * Size.X), static_cast<float>(Point.Y * Size.Y)));

    // Antialiasing off. The screenshot gate measures pixel extents, and an antialiased endpoint
    // spreads the measured extent by a fraction of a pixel differently between driver versions,
    // which would show up as a change nobody made.
    FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Local,
        ESlateDrawEffect::None, Colour, /*bAntialias=*/false, Thickness);
    return LayerId + 1;
}

void UDashLines::SetLines(const TArray<FVector2D>& InPoints, const FLinearColor& InColour, float InThickness)
{
    Points = InPoints;
    Colour = InColour;
    Thickness = InThickness;
    if (Line.IsValid()) Line->SetLines(Points, Colour, Thickness);
}
TSharedRef<SWidget> UDashLines::RebuildWidget()
{
    Line = SNew(SDashLines).Points(Points).Colour(Colour).Thickness(Thickness);
    return Line.ToSharedRef();
}
void UDashLines::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Line.Reset();
}
