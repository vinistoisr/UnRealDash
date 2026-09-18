#include "Primitives.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"

namespace UnRealDashCore
{
// A lamp driven by a rule result. The schema gives an indicator only `colour`, with no on and off
// artwork to resolve, so the two states are drawn from that one colour rather than sampled: on is
// the colour as authored, off is the same hue at 18 percent value. Off stays visibly present, so a
// dark cluster reads as a cluster with nothing lit rather than as a display that failed to draw.
// See chunk-11 revision 4 C6; artwork-backed lamps arrive with the PLAN 4.6 primitives, which have
// asset fields in the schema.
FBuiltComponent BuildIndicator(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return {};

    FLinearColor Colour;
    if (!Context.Theme.ResolveColour(Context.Component.Properties.Member(TEXT("colour")),
            Context.Component.Pointer + TEXT("/properties/colour"), Colour, OutError))
        return {};

    // No rule is evaluated in this chunk: PLAN 4.5 renders a frozen document, and wiring a lamp to
    // a live rule result is 4.9's connector work. Off is the honest state to draw for a lamp whose
    // rule has not been evaluated, because a lamp that lights without a rule behind it would be
    // reporting a condition nothing measured.
    const bool bLit = false;
    const FLinearColor Drawn = bLit ? Colour
        : FLinearColor(Colour.R * 0.18f, Colour.G * 0.18f, Colour.B * 0.18f, Colour.A);

    UCanvasPanel* Panel = MakePanel(Tree);
    UImage* Lamp = MakeFill(Tree, Drawn);
    AddFilling(Tree, Panel, Lamp);

    FMissingDataPresenter Presenter;
    if (!BuildMissingData(Context, Tree, Panel, Lamp, nullptr, Presenter, OutError)) return {};
    ApplyClipping(Panel, Context.Component);
    if (!BindingFor(Context.Component, Context.Package).Exists()) return {Panel, {}};

    const FLinearColor Lit = Colour;
    const FLinearColor Dim = Drawn;
    // A bound lamp lights on a non-zero reading. Chunk 12 drew it permanently off because nothing
    // could tell it otherwise; a binding now can. A rule result driving it is PLAN 5.x.
    return {Panel, [Lamp, Lit, Dim, Presenter](const FDashSignalValue& Reading)
    {
        const bool bOn = Reading.bHasValue && Reading.State == EDashSignalState::Valid && Reading.Value != 0.0;
        Lamp->SetColorAndOpacity(bOn ? Lit : Dim);
        Presenter.Apply(Reading.State);
    }};
}
}
