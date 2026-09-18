#include "Primitives.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"

namespace UnRealDashCore
{
// A needle, and nothing else.
//
// No bezel, no tick band, no numerals, no face. The schema gives an analog_dial only minimum,
// maximum and colour, with no asset field, so a face could never be named by a document, and a
// primitive that drew one could not be restyled by a document either. Those are image components
// the document positions behind this one. That is how the owner's own RealDash cluster for the
// Scirocco is built: the face is authored as SVG and rendered to PNG at 2x, and the only thing that
// moves at runtime is a rotating needle sprite.
FBuiltComponent BuildAnalogDial(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return {};
    if (!RefuseStretchedCircle(Context, OutError)) return {};

    FLinearColor Colour;
    if (!Context.Theme.ResolveColour(Context.Component.Properties.Member(TEXT("colour")),
            Context.Component.Pointer + TEXT("/properties/colour"), Colour, OutError))
        return {};
    float Deflection = 0.f;
    if (!GaugeDeflection(Context, Deflection, OutError)) return {};

    UCanvasPanel* Panel = MakePanel(Tree);
    // The working square, so a dial in a non-square rect sweeps a circle rather than an ellipse.
    const float Side = static_cast<float>(FMath::Min(Rect.Size.X, Rect.Size.Y));

    UImage* Needle = MakeFill(Tree, Colour);
    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Panel->AddChild(Needle)))
    {
        // Pinned to the panel centre by its own bottom edge, pointing straight up before rotation,
        // so the pivot and the sweep centre are the same point.
        Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        Slot->SetAlignment(FVector2D(0.5, 1.0));
        Slot->SetAutoSize(false);
        Slot->SetOffsets(FMargin(0.f, 0.f, Side * NeedleWidth, Side * NeedleLength));
    }
    // Rotation rather than a redraw: a render transform about the needle's bottom centre costs no
    // custom painting, and the pivot is expressed in the needle's own normalized space.
    Needle->SetRenderTransformPivot(FVector2D(0.5, 1.0));
    FWidgetTransform Transform;
    Transform.Angle = SweepStartDegrees + SweepDegrees * Deflection;
    Needle->SetRenderTransform(Transform);

    FMissingDataPresenter Presenter;
    if (!BuildMissingData(Context, Tree, Panel, Needle, nullptr, Presenter, OutError)) return {};
    ApplyClipping(Panel, Context.Component);
    if (!BindingFor(Context.Component, Context.Package).Exists()) return {Panel, {}};

    FGaugeRange Range;
    if (!ReadGaugeRange(Context, Range, OutError)) return {};
    // A render transform angle, not a redraw. The needle widget, its pivot and its geometry are
    // fixed at build time and only the angle moves, which is the whole point of drawing a needle
    // as a rotated rectangle rather than as a polyline recomputed per frame.
    return {Panel, [Needle, Range, Presenter](const FDashSignalValue& Reading)
    {
        FWidgetTransform Transform;
        Transform.Angle = SweepStartDegrees + SweepDegrees * Range.Deflection(Reading);
        Needle->SetRenderTransform(Transform);
        Presenter.Apply(Reading.State);
    }};
}
}
