#include "Primitives.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"

namespace UnRealDashCore
{
// The one primitive allowed to draw rather than sample artwork, and the boundary is operational
// rather than a matter of taste: axis-aligned rectangles and straight lines, one uniform fill.
//
// The schema's kind enum also admits `ellipse`. This runtime refuses it. A curve that is not a
// corner radius is artwork, and drawing it in code produces ornament a document cannot restyle,
// which is the defect this chunk exists to prevent. Refusing it by name is the third error case of
// criterion 6 and the only one of the three that reaches a builder at all: an unknown component
// type is rejected by schema validation and an unresolved theme token by the semantic pass, both
// before a package finishes loading. See chunk-11 revision 4, C3 and C7.
UWidget* BuildShape(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return nullptr;

    FString Kind;
    if (!Context.Component.Properties.Member(TEXT("kind")).String(Kind))
    {
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties/kind"),
            TEXT("Shape needs a kind property"), Context.Package.Path() };
        return nullptr;
    }
    if (Kind == TEXT("ellipse"))
    {
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties/kind"),
            TEXT("Shape kind ellipse is not drawn: a curve that is not a corner radius is artwork "
                 "and belongs in an image asset"), Context.Package.Path() };
        return nullptr;
    }
    FLinearColor Colour;
    if (!Context.Theme.ResolveColour(Context.Component.Properties.Member(TEXT("colour")),
            Context.Component.Pointer + TEXT("/properties/colour"), Colour, OutError))
        return nullptr;

    UCanvasPanel* Panel = MakePanel(Tree);
    UImage* Fill = MakeFill(Tree, Colour);
    if (Kind == TEXT("line"))
    {
        // A line is a rectangle one unit thick on its shorter axis, drawn against the top or left
        // edge of the component's rect. Slate has no stroke primitive that a document can position,
        // and a degenerate rectangle is the same pixels with none of the machinery.
        const bool Horizontal = Rect.Size.X >= Rect.Size.Y;
        UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Panel->AddChild(Fill));
        if (Slot)
        {
            Slot->SetAnchors(Horizontal ? FAnchors(0.f, 0.f, 1.f, 0.f) : FAnchors(0.f, 0.f, 0.f, 1.f));
            Slot->SetAlignment(FVector2D::ZeroVector);
            Slot->SetAutoSize(false);
            Slot->SetOffsets(Horizontal ? FMargin(0.f, 0.f, 0.f, Rect.Size.Y)
                                        : FMargin(0.f, 0.f, Rect.Size.X, 0.f));
        }
    }
    else
    {
        AddFilling(Tree, Panel, Fill);
    }
    // A shape binds no signal, so it has no missing-data state to present. The schema requires the
    // missing_data object on it anyway, and that declaration is parsed and ignored; revision 4 C1
    // records why treating its presence as an error is unbuildable.
    ApplyClipping(Panel, Context.Component);
    return Panel;
}
}
