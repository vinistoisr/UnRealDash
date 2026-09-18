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
UWidget* BuildIndicator(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return nullptr;

    FLinearColor Colour;
    if (!Context.Theme.ResolveColour(Context.Component.Properties.Member(TEXT("colour")),
            Context.Component.Pointer + TEXT("/properties/colour"), Colour, OutError))
        return nullptr;

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

    if (!ApplyMissingData(Context, Tree, Panel, Lamp, nullptr, OutError)) return nullptr;
    ApplyClipping(Panel, Context.Component);
    return Panel;
}
}
