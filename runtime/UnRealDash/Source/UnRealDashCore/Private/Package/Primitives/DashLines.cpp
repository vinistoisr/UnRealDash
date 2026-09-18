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

    // Antialiased, which also gets the joins.
    //
    // Chunk 12 turned it off so the screenshot gate would measure a stable extent. That was the
    // wrong trade for a thick polyline: Slate's non-antialiased path draws each segment as its own
    // quad with no join, so every turn leaves an uncovered notch on the outer edge, and a circular
    // gauge came out visibly serrated. Raising the point density made the notches thinner without
    // closing them, because the quads still do not overlap.
    //
    // Nothing measured depends on the difference. The gates that touch these two primitives use
    // pixel-count floors with wide margins, an arc floor of 3,000 against about 8,000 measured,
    // rather than an exact bounding box, so a fraction of a pixel at an edge changes nothing.
    FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Local,
        ESlateDrawEffect::None, Colour, /*bAntialias=*/true, Thickness);
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
