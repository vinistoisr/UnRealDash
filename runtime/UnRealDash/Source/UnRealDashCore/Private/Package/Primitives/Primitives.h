#pragma once
#include "Layout.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UTextBlock;
class UWidgetTree;

namespace UnRealDashCore
{
// Every primitive returns a UCanvasPanel as its outer widget, with its own visuals added as canvas
// children. That is what lets ApplyLayout position document children uniformly whatever primitive
// they sit under, instead of each primitive inventing an arrangement for them.
UCanvasPanel* MakePanel(UWidgetTree& Tree);

// A flat rectangle of one colour, filling whatever slot it is given.
//
// UImage over a colour brush rather than UBorder: a UBorder draws its default brush as a
// nine-slice box, so the painted area is the slot inset by the brush margins and the corners
// come from the style rather than from the colour. The screenshot gate measures exact pixel
// counts, so a fill has to be the size it was asked for, uniformly, with no style in it.
UImage* MakeFill(UWidgetTree& Tree, const FLinearColor& Colour);

// Adds Child to Panel filling it exactly, or inset by Inset units on each side.
UCanvasPanelSlot* AddFilling(UWidgetTree& Tree, UCanvasPanel* Panel, UWidget* Child, float Inset = 0.f);

// Adds Child as a band of the given height across the full width, against the bottom edge.
UCanvasPanelSlot* AddBottomBand(UWidgetTree& Tree, UCanvasPanel* Panel, UWidget* Child, float Height);

// The status band height in document units, pinned by the chunk 11 gate arithmetic: on the gate
// fixture's 320 unit wide readout this is 15,360 pixels, over three times the 4,608 pixels that
// criterion 4 requires two states to differ by. Changing it invalidates that arithmetic.
inline constexpr float StatusBandHeight = 48.f;

// Text size derived from the component's own height, so a readout's glyphs scale with its box and
// two captures of the same document always agree. No document field controls it: the schema gives
// a readout only text and colour.
float TextSizeForHeight(double Height);

UWidget* BuildContainer(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree);
UWidget* BuildReadout(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree);
UWidget* BuildImage(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree);
UWidget* BuildShape(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree);
UWidget* BuildIndicator(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree);
UWidget* BuildPageSwitch(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree);

// page_switch takes its children into the panel for the page that lists them, and hides every page
// but the initial one. A child no page lists is an error naming its pointer.
bool AdoptIntoPage(const FAdoptContext& Context, FDashLoadError& OutError);

// Shared missing-data rendering for the three primitives that bind a signal. Applies the declared
// presentation to Content and paints the state band on Panel. Returns false only on a theme error.
//
// container, shape and page_switch bind no signal and never call this. The schema requires
// missing_data on them anyway, so their declaration is parsed and ignored; see chunk-11 revision 4
// C1 for why treating its presence as an error is unbuildable.
bool ApplyMissingData(const FComponentContext& Context, UWidgetTree& Tree, UCanvasPanel* Panel,
    UWidget* Content, UTextBlock* ValueText, FDashLoadError& OutError);
}
