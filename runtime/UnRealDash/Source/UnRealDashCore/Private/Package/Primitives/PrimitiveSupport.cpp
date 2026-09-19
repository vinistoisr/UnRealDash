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
bool GaugeDeflection(const FComponentContext& Context, float& Out, FDashLoadError& OutError)
{
    double Minimum = 0, Maximum = 0;
    if (!Context.Component.Properties.Member(TEXT("minimum")).Number(Minimum) ||
        !Context.Component.Properties.Member(TEXT("maximum")).Number(Maximum))
    {
        // Unreachable through LoadPackage: the schema makes both required on a dial and a bar gauge.
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties"),
            TEXT("Gauge needs numeric minimum and maximum"), Context.Package.Path() };
        return false;
    }
    const double Range = Maximum - Minimum;
    // A zero or inverted range has no deflection to compute. Resting at the low end is the honest
    // rendering, and it is not a silent fallback because there is no other defensible answer.
    const double Value = Minimum + FMath::Clamp(static_cast<double>(Context.Fraction), 0.0, 1.0) * Range;
    Out = Range > 0.0 ? static_cast<float>(FMath::Clamp((Value - Minimum) / Range, 0.0, 1.0)) : 0.f;
    return true;
}
bool ReadGaugeRange(const FComponentContext& Context, FGaugeRange& Out, FDashLoadError& OutError)
{
    if (!Context.Component.Properties.Member(TEXT("minimum")).Number(Out.Minimum) ||
        !Context.Component.Properties.Member(TEXT("maximum")).Number(Out.Maximum))
    {
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties"),
            TEXT("Gauge needs numeric minimum and maximum"), Context.Package.Path() };
        return false;
    }
    return true;
}
float FGaugeRange::Deflection(const FDashSignalValue& Reading) const
{
    const double Range = Maximum - Minimum;
    if (!Reading.bHasValue || Range <= 0.0) return 0.f;
    return static_cast<float>(FMath::Clamp((Reading.Value - Minimum) / Range, 0.0, 1.0));
}
UCanvasPanelSlot* AddCentredSquare(UWidgetTree& Tree, UCanvasPanel* Panel, UWidget* Child, const FDashRect& Rect)
{
    UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Panel->AddChild(Child));
    if (!Slot) return nullptr;
    const float Side = static_cast<float>(FMath::Min(Rect.Size.X, Rect.Size.Y));
    Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
    Slot->SetAlignment(FVector2D(0.5, 0.5));
    Slot->SetAutoSize(false);
    // On a point-anchored axis the offsets are position then size, so this is centred at the panel
    // centre with a side of Side. See SConstraintCanvas.cpp lines 246 to 281.
    Slot->SetOffsets(FMargin(0.f, 0.f, Side, Side));
    return Slot;
}
bool RefuseStretchedCircle(const FComponentContext& Context, FDashLoadError& OutError)
{
    if (ResolveAspectPolicy(Context.Component, Context.Package) != TEXT("stretch")) return true;
    OutError = { TEXT("E_ASPECT_POLICY_FORBIDDEN"), 18, Context.Component.Pointer + TEXT("/aspect_policy"),
        TEXT("A circular gauge cannot be non-uniformly scaled"), Context.Package.Path() };
    return false;
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
namespace
{
// The four schema states in a fixed order, so a resolved colour can be indexed rather than looked
// up by string every frame.
constexpr EDashSignalState StateOrder[FMissingDataPresenter::StateCount] = {
    EDashSignalState::Stale, EDashSignalState::Unavailable,
    EDashSignalState::Invalid, EDashSignalState::AgeUnknown};

int32 IndexOf(EDashSignalState State)
{
    for (int32 Index = 0; Index < FMissingDataPresenter::StateCount; ++Index)
        if (StateOrder[Index] == State) return Index;
    return INDEX_NONE;
}
}

void FMissingDataPresenter::Apply(EDashSignalState State, const FText* Live) const
{
    const int32 Index = IndexOf(State);

    // Start from the valid rendering every time, so a return to Valid restores what was there and
    // a change between two non-valid states does not leave the previous one's marks behind.
    if (Content)
    {
        Content->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Content->SetRenderOpacity(1.f);
    }
    if (ValueText)
    {
        // The live reading where there is one, so a state presentation is applied ON TOP of the
        // current value rather than instead of it. dash still replaces it below, which is what
        // dash means.
        ValueText->SetText(Live ? *Live : ValidText);
        ValueText->SetColorAndOpacity(FSlateColor(ValidColour));
    }
    if (Badge) Badge->SetVisibility(ESlateVisibility::Collapsed);
    if (Band) Band->SetVisibility(ESlateVisibility::Collapsed);
    if (Index == INDEX_NONE) return;

    const FString& Shown = Presentation[Index];
    const FLinearColor& Colour = StateColour[Index];

    if (Shown == TEXT("hidden"))
    {
        if (Content) Content->SetVisibility(ESlateVisibility::Hidden);
    }
    else if (Shown == TEXT("last_value_dimmed"))
    {
        // Dimmed, not removed: the last reading is still the best information available, and hiding
        // it would read as a fault in the display rather than in the signal.
        if (Content) Content->SetRenderOpacity(0.35f);
    }
    else if (Shown == TEXT("dash"))
    {
        // A primitive with value text replaces it. One with no text has nothing to dash, so its
        // content is hidden and the band alone carries the state; see chunk-11 revision 4 C2.
        if (ValueText)
        {
            ValueText->SetText(FText::FromString(TEXT("--")));
            ValueText->SetColorAndOpacity(FSlateColor(Colour));
        }
        else if (Content) Content->SetVisibility(ESlateVisibility::Hidden);
    }
    else if (Shown == TEXT("icon"))
    {
        // The value stays readable and gains a badge, which is what makes age_unknown structurally
        // distinct from valid rather than merely dimmer. The chunk 11 gate measures glyph coverage
        // and band colour, not opacity, precisely so the difference cannot be a judgement call.
        if (Badge)
        {
            Badge->SetColorAndOpacity(FSlateColor(Colour));
            Badge->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
    }

    if (Band)
    {
        Band->SetColorAndOpacity(Colour);
        Band->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}

bool BuildMissingData(const FComponentContext& Context, UWidgetTree& Tree, UCanvasPanel* Panel,
    UWidget* Content, UTextBlock* ValueText, FMissingDataPresenter& Out, FDashLoadError& OutError)
{
    Out = {};
    Out.Content = Content;
    Out.ValueText = ValueText;
    if (ValueText)
    {
        Out.ValidText = ValueText->GetText();
        Out.ValidColour = ValueText->GetColorAndOpacity().GetSpecifiedColor();
    }

    // A component the document binds to no signal has no signal state to present. Missing data is a
    // property of a signal, and a component bound to nothing has no signal that could be stale.
    //
    // This is also what keeps the PLAN 4.5 criterion 4 gate honest. Without it, forcing a state
    // changed every signal-capable primitive in the fixture at once, and the image and the lamp
    // between them moved 66,816 pixels of a 921,600 pixel frame. The three states would then have
    // been distinguishable even if the readout the criterion names had rendered identically in all
    // three, so the gate could not have failed for the thing it measures.
    if (!BindingFor(Context.Component, Context.Package).Exists()) return true;

    // Every state's token is resolved here rather than when it first fires, so a theme error
    // surfaces at load instead of the first time a signal happens to go stale on a device.
    for (int32 Index = 0; Index < FMissingDataPresenter::StateCount; ++Index)
    {
        const EDashSignalState State = StateOrder[Index];
        Out.Presentation[Index] = PresentationFor(Context.Component, State);
        if (!Context.Theme.Resolve(PresentationTokenFor(Context.Component, State),
                PresentationPointerFor(Context.Component, State), Out.StateColour[Index], OutError))
            return false;
    }

    // Built once, collapsed, and only ever shown or hidden afterwards.
    if (ValueText)
    {
        if (UOverlay* Host = Cast<UOverlay>(ValueText->GetParent()))
        {
            Out.Badge = Tree.ConstructWidget<UTextBlock>();
            Out.Badge->SetText(FText::FromString(BadgeGlyph));
            Out.Badge->SetFont(ValueText->GetFont());
            Out.Badge->SetVisibility(ESlateVisibility::Collapsed);
            if (UOverlaySlot* Slot = Cast<UOverlaySlot>(Host->AddChild(Out.Badge)))
            {
                Slot->SetHorizontalAlignment(HAlign_Right);
                Slot->SetVerticalAlignment(VAlign_Center);
                // Clear of the edge. Flush against it the glyph reads as clipped rather than as a
                // badge, which is the opposite of what a state marker is for.
                Slot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
            }
        }
    }
    Out.Band = MakeFill(Tree, FLinearColor::White);
    Out.Band->SetVisibility(ESlateVisibility::Collapsed);
    AddBottomBand(Tree, Panel, Out.Band, StatusBandHeight);

    // The initial render. Before the presenter existed this happened as a side effect of building,
    // and moving the rendering into an updater silently stopped it: the widgets were built in their
    // valid appearance and nothing applied the state until a signal arrived. With -udash-state that
    // meant all three states captured identically, which the chunk 11 gate caught immediately.
    Out.Apply(Context.State);
    return true;
}
}
