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

// PLAN 4.6. Each draws only the part that moves: the needle, the filled portion, the trace. The
// face, bezel, tick band, numerals, track, axes and labels are image components the document
// positions behind them, because the schema gives none of these three an asset field and a
// primitive that drew its own ornament could not be restyled by a document.
UWidget* BuildAnalogDial(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree);
UWidget* BuildBarGauge(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree);
UWidget* BuildHistoryGraph(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree);

// The automotive sweep: 270 degrees, starting at 225 degrees clockwise from twelve o'clock, so a
// gauge at minimum points to the lower left and at maximum to the lower right. It is not a document
// field because the schema has none, and adding one would change a schema this chunk must not.
inline constexpr float SweepStartDegrees = 225.f;
inline constexpr float SweepDegrees = 270.f;

// Fraction of the dial's working square: how long and how wide the needle is, and where the arc of
// a circular bar gauge sits. Pinned here because the capture gate measures them.
inline constexpr float NeedleLength = 0.42f;
inline constexpr float NeedleWidth = 0.03f;
inline constexpr float ArcRadius = 0.42f;
inline constexpr float ArcThickness = 0.06f;

// Where a gauge sits in its own declared range, 0 to 1. Reads minimum and maximum off the component
// and applies the context's fraction. A range of zero width yields 0 rather than a division by zero.
bool GaugeDeflection(const FComponentContext& Context, float& Out, FDashLoadError& OutError);

// The largest square centred in the component's rect, added as a canvas child. A dial in a
// non-square rect keeps a circular sweep instead of tracing an ellipse, and SDashLines normalizes
// to its own allotted size, so an arc is only circular inside a square.
UCanvasPanelSlot* AddCentredSquare(UWidgetTree& Tree, UCanvasPanel* Panel, UWidget* Child, const FDashRect& Rect);

// Refuses a component whose resolved aspect policy is stretch. The semantic pass already rejects a
// dial and a circular bar gauge under one, so this cannot fire through LoadPackage; it is kept
// because a caller can build a component the validator never saw, and a silently distorted circular
// gauge is the exact defect PLAN 4.6's gate exists to catch.
bool RefuseStretchedCircle(const FComponentContext& Context, FDashLoadError& OutError);

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
