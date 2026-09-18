#include "Primitives.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"

namespace UnRealDashCore
{
UCanvasPanel* MakePanel(UWidgetTree& Tree)
{
    return Tree.ConstructWidget<UCanvasPanel>();
}
UImage* MakeFill(UWidgetTree& Tree, const FLinearColor& Colour)
{
    UImage* Fill = Tree.ConstructWidget<UImage>();
    // FSlateColorBrush is a NoImage brush, which Slate renders as a solid colour over the whole
    // allotted geometry. No resource, no margins, no tiling and nothing from the style.
    Fill->SetBrush(FSlateColorBrush(FLinearColor::White));
    Fill->SetColorAndOpacity(Colour);
    return Fill;
}
UCanvasPanelSlot* AddFilling(UWidgetTree& Tree, UCanvasPanel* Panel, UWidget* Child, float Inset)
{
    UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Panel->AddChild(Child));
    if (!Slot) return nullptr;
    Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    Slot->SetAlignment(FVector2D::ZeroVector);
    Slot->SetAutoSize(false);
    Slot->SetOffsets(FMargin(Inset, Inset, Inset, Inset));
    return Slot;
}
UCanvasPanelSlot* AddBottomBand(UWidgetTree& Tree, UCanvasPanel* Panel, UWidget* Child, float Height)
{
    UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Panel->AddChild(Child));
    if (!Slot) return nullptr;
    // Stretched across the width, pinned to the bottom edge, of a fixed height. Left and top in the
    // offsets are the left and bottom insets; right and bottom are width and height, and width is
    // ignored because the anchors already span the full width.
    Slot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
    Slot->SetAlignment(FVector2D(0.f, 1.f));
    Slot->SetAutoSize(false);
    Slot->SetOffsets(FMargin(0.f, 0.f, 0.f, Height));
    return Slot;
}
float TextSizeForHeight(double Height)
{
    return static_cast<float>(FMath::Clamp(Height * 0.28, 12.0, 96.0));
}
namespace
{
// The badge drawn beside a value for the icon presentation. The schema has no icon asset field on
// a missing_data state, so this chunk cannot consume artwork for it the way the image primitive
// consumes a dial face; a glyph is what the document actually offers. Recorded in chunk-11
// revision 4 alongside the other schema gaps rather than presented as a design choice.
const TCHAR* BadgeGlyph = TEXT("?");
}
bool ApplyMissingData(const FComponentContext& Context, UWidgetTree& Tree, UCanvasPanel* Panel,
    UWidget* Content, UTextBlock* ValueText, FDashLoadError& OutError)
{
    if (Context.State == EDashSignalState::Valid) return true;
    // A component the document binds to no signal has no signal state to present. Missing data is a
    // property of a signal, and a component bound to nothing has no signal that could be stale.
    //
    // This is also what keeps the PLAN 4.5 criterion 4 gate honest. Without it, forcing a state
    // changed every signal-capable primitive in the fixture at once, and the image and the lamp
    // between them moved 66,816 pixels of a 921,600 pixel frame. The three states would then have
    // been distinguishable even if the readout the criterion names had rendered identically in all
    // three, so the gate could not have failed for the thing it measures.
    if (!BindingFor(Context.Component, Context.Package).Exists()) return true;

    const FString Presentation = PresentationFor(Context.Component, Context.State);
    const FString Token = PresentationTokenFor(Context.Component, Context.State);
    const FString Pointer = PresentationPointerFor(Context.Component, Context.State);
    FLinearColor StateColour;
    if (!Context.Theme.Resolve(Token, Pointer, StateColour, OutError)) return false;

    if (Presentation == TEXT("hidden"))
    {
        if (Content) Content->SetVisibility(ESlateVisibility::Hidden);
    }
    else if (Presentation == TEXT("last_value_dimmed"))
    {
        // Dimmed, not removed: the last reading is still the best information available, and
        // hiding it would read as a fault in the display rather than in the signal.
        if (Content) Content->SetRenderOpacity(0.35f);
    }
    else if (Presentation == TEXT("dash"))
    {
        // A primitive with value text replaces it. One with no text has nothing to dash, so its
        // content is hidden and the band alone carries the state; see revision 4 C2.
        if (ValueText) { ValueText->SetText(FText::FromString(TEXT("--"))); ValueText->SetColorAndOpacity(FSlateColor(StateColour)); }
        else if (Content) Content->SetVisibility(ESlateVisibility::Hidden);
    }
    else if (Presentation == TEXT("icon"))
    {
        // The value stays readable and gains a badge, which is what makes age_unknown structurally
        // distinct from valid rather than merely dimmer. Criterion 4 measures glyph coverage and
        // band colour, not opacity, precisely so the difference cannot be a judgement call.
        //
        // The badge is its own text block so it can carry the state token colour. Appending it to
        // the value text instead would draw it in the value's colour, and the state's colour would
        // then appear nowhere but the band.
        if (ValueText)
        {
            if (UOverlay* Host = Cast<UOverlay>(ValueText->GetParent()))
            {
                UTextBlock* Badge = Tree.ConstructWidget<UTextBlock>();
                Badge->SetText(FText::FromString(BadgeGlyph));
                Badge->SetColorAndOpacity(FSlateColor(StateColour));
                Badge->SetFont(ValueText->GetFont());
                if (UOverlaySlot* Slot = Cast<UOverlaySlot>(Host->AddChild(Badge)))
                {
                    Slot->SetHorizontalAlignment(HAlign_Right);
                    Slot->SetVerticalAlignment(VAlign_Center);
                    // Clear of the edge. Flush against it the glyph reads as clipped rather than
                    // as a badge, which is the opposite of what a state marker is for.
                    Slot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
                }
            }
        }
    }

    AddBottomBand(Tree, Panel, MakeFill(Tree, StateColour), StatusBandHeight);
    return true;
}
}
