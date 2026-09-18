#include "Primitives.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"

namespace UnRealDashCore
{
namespace
{
// The schema gives a readout `text` and an optional `colour`, and nothing else; format lives on the
// binding as `.1f` and the unit on the signal definition. See chunk-11 revision 4 C4, which
// withdrew the claim that a readout carries its own format, units and precision.
//
// Only the printf conversions the binding format can name are honoured, and a format this cannot
// read leaves the text alone rather than printing a guess.
FString ApplyBindingFormat(const FString& Text, const FString& Format)
{
    if (Format.IsEmpty()) return Text;
    const TCHAR Conversion = Format[Format.Len() - 1];
    if (Conversion != TEXT('f') && Conversion != TEXT('e') && Conversion != TEXT('g') && Conversion != TEXT('d'))
        return Text;
    // A frozen document carries a constant in `text`. If it is not a number there is nothing to
    // format, which is the ordinary case for a label.
    if (!Text.IsNumeric()) return Text;
    const double Value = FCString::Atod(*Text);
    if (Conversion == TEXT('d')) return FString::Printf(TEXT("%lld"), static_cast<int64>(Value));
    int32 Precision = 6;
    int32 Dot = INDEX_NONE;
    if (Format.FindChar(TEXT('.'), Dot))
        Precision = FCString::Atoi(*Format.Mid(Dot + 1, Format.Len() - Dot - 2));
    Precision = FMath::Clamp(Precision, 0, 17);
    return FString::Printf(TEXT("%.*f"), Precision, Value);
}
}
UWidget* BuildReadout(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return nullptr;

    FString Text;
    if (!Context.Component.Properties.Member(TEXT("text")).String(Text))
    {
        // Unreachable through LoadPackage: the schema makes `text` required on a readout.
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties/text"),
            TEXT("Readout needs a text property"), Context.Package.Path() };
        return nullptr;
    }
    FString Format;
    BindingFor(Context.Component, Context.Package).Member(TEXT("format")).String(Format);

    FLinearColor Colour = FLinearColor::White;
    const FDashValue Declared = Context.Component.Properties.Member(TEXT("colour"));
    if (Declared.Exists() && !Context.Theme.ResolveColour(Declared,
            Context.Component.Pointer + TEXT("/properties/colour"), Colour, OutError))
        return nullptr;

    UCanvasPanel* Panel = MakePanel(Tree);
    UTextBlock* Value = Tree.ConstructWidget<UTextBlock>();
    Value->SetText(FText::FromString(ApplyBindingFormat(Text, Format)));
    Value->SetColorAndOpacity(FSlateColor(Colour));
    FSlateFontInfo Font = Value->GetFont();
    Font.Size = static_cast<int32>(TextSizeForHeight(Rect.Size.Y));
    Value->SetFont(Font);
    Value->SetJustification(ETextJustify::Center);
    // Centred in the area above the status band rather than in the whole box, so that painting a
    // band does not move the glyphs. Criterion 4 measures the state, not a layout shift the state
    // happens to cause, and a shifted glyph would move pixels for the wrong reason.
    UOverlay* Centre = Tree.ConstructWidget<UOverlay>();
    if (UOverlaySlot* Inner = Cast<UOverlaySlot>(Centre->AddChild(Value)))
    {
        Inner->SetHorizontalAlignment(HAlign_Center);
        Inner->SetVerticalAlignment(VAlign_Center);
    }
    if (UCanvasPanelSlot* Slot = AddFilling(Tree, Panel, Centre))
        Slot->SetOffsets(FMargin(0.f, 0.f, 0.f, StatusBandHeight));

    if (!ApplyMissingData(Context, Tree, Panel, Centre, Value, OutError)) return nullptr;
    ApplyClipping(Panel, Context.Component);
    return Panel;
}
}
