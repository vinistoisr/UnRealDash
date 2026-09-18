#include "Primitives.h"
#include "DashLines.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"

namespace UnRealDashCore
{
// The filled portion, and nothing else. The track behind it is artwork, for the same reason a dial
// draws no face: the schema gives a bar_gauge only minimum, maximum, circular and colour.
FBuiltComponent BuildBarGauge(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return {};

    bool bCircular = false;
    if (!Context.Component.Properties.Member(TEXT("circular")).Boolean(bCircular))
    {
        // Unreachable through LoadPackage: the schema makes circular required on a bar gauge.
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties/circular"),
            TEXT("Bar gauge needs a circular property"), Context.Package.Path() };
        return {};
    }
    // Only the circular form is a circle, so only it is refused under a stretch. The semantic pass
    // draws the same distinction at SemanticPass.cpp:273, and the two must agree.
    if (bCircular && !RefuseStretchedCircle(Context, OutError)) return {};

    FLinearColor Colour;
    if (!Context.Theme.ResolveColour(Context.Component.Properties.Member(TEXT("colour")),
            Context.Component.Pointer + TEXT("/properties/colour"), Colour, OutError))
        return {};
    float Deflection = 0.f;
    if (!GaugeDeflection(Context, Deflection, OutError)) return {};

    UCanvasPanel* Panel = MakePanel(Tree);
    UWidget* Content = nullptr;
    if (bCircular)
    {
        // An arc on the same sweep as the dial, so a dial and a circular gauge on the same face
        // agree about where minimum and maximum are. One point per degree swept: at 270 degrees
        // that is at most 271 points, which is enough that the step is invisible at any size this
        // runs at and few enough that the point list costs nothing.
        const int32 Steps = FMath::Max(1, FMath::RoundToInt(SweepDegrees * Deflection));
        TArray<FVector2D> Points;
        Points.Reserve(Steps + 1);
        for (int32 Index = 0; Index <= Steps; ++Index)
        {
            const double Degrees = SweepStartDegrees + static_cast<double>(Index);
            const double Radians = FMath::DegreesToRadians(Degrees);
            // Zero degrees is twelve o'clock and degrees increase clockwise, matching the needle's
            // render transform, so sine drives x and negative cosine drives y.
            Points.Add(FVector2D(0.5 + ArcRadius * FMath::Sin(Radians), 0.5 - ArcRadius * FMath::Cos(Radians)));
        }
        UDashLines* Arc = Tree.ConstructWidget<UDashLines>();
        const float Side = static_cast<float>(FMath::Min(Rect.Size.X, Rect.Size.Y));
        Arc->SetLines(Points, Colour, Side * ArcThickness);
        AddCentredSquare(Tree, Panel, Arc, Rect);
        Content = Arc;
    }
    else
    {
        // A filled rectangle along the rect's longer axis, growing from the low edge: left to right
        // when it is wider than tall, bottom to top when it is taller than wide.
        const bool bHorizontal = Rect.Size.X >= Rect.Size.Y;
        UImage* Fill = MakeFill(Tree, Colour);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Panel->AddChild(Fill)))
        {
            Slot->SetAutoSize(false);
            if (bHorizontal)
            {
                Slot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
                Slot->SetAlignment(FVector2D(0.0, 0.0));
                Slot->SetOffsets(FMargin(0.f, 0.f, static_cast<float>(Rect.Size.X) * Deflection, 0.f));
            }
            else
            {
                Slot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
                Slot->SetAlignment(FVector2D(0.0, 1.0));
                Slot->SetOffsets(FMargin(0.f, 0.f, 0.f, static_cast<float>(Rect.Size.Y) * Deflection));
            }
        }
        Content = Fill;
    }
    FMissingDataPresenter Presenter;
    if (!BuildMissingData(Context, Tree, Panel, Content, nullptr, Presenter, OutError)) return {};
    ApplyClipping(Panel, Context.Component);
    if (!BindingFor(Context.Component, Context.Package).Exists()) return {Panel, {}};

    FGaugeRange Range;
    if (!ReadGaugeRange(Context, Range, OutError)) return {};
    const double Width = Rect.Size.X, Height = Rect.Size.Y;
    const bool bHorizontal = Width >= Height;
    const float Side = static_cast<float>(FMath::Min(Width, Height));
    UCanvasPanelSlot* FillSlot = bCircular ? nullptr : Cast<UCanvasPanelSlot>(Content->Slot);
    UDashLines* Arc = bCircular ? Cast<UDashLines>(Content) : nullptr;

    return {Panel, [Arc, FillSlot, Range, Colour, Side, Width, Height, bHorizontal, Presenter]
        (const FDashSignalValue& Reading)
    {
        const float Deflection = Range.Deflection(Reading);
        if (Arc)
        {
            // The arc's point list is the only thing here that is rebuilt per update, because an
            // arc's length is its geometry. It is bounded at 271 points by the 270 degree sweep.
            const int32 Steps = FMath::Max(1, FMath::RoundToInt(SweepDegrees * Deflection));
            TArray<FVector2D> Points;
            Points.Reserve(Steps + 1);
            for (int32 Index = 0; Index <= Steps; ++Index)
            {
                const double Radians = FMath::DegreesToRadians(SweepStartDegrees + static_cast<double>(Index));
                Points.Add(FVector2D(0.5 + ArcRadius * FMath::Sin(Radians), 0.5 - ArcRadius * FMath::Cos(Radians)));
            }
            Arc->SetLines(Points, Colour, Side * ArcThickness);
        }
        else if (FillSlot)
        {
            // A resize, not a rebuild: the fill widget and its anchors are fixed and only the
            // offset that carries its length changes.
            FillSlot->SetOffsets(bHorizontal
                ? FMargin(0.f, 0.f, static_cast<float>(Width) * Deflection, 0.f)
                : FMargin(0.f, 0.f, 0.f, static_cast<float>(Height) * Deflection));
        }
        Presenter.Apply(Reading.State);
    }};
}
}
