#include "Primitives.h"
#include "DashLines.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"

namespace UnRealDashCore
{
// The trace, and nothing else. Axes, grid and labels are artwork.
//
// The y range is the buffer's own minimum and maximum, padded, because nothing declares one: a
// history_graph carries only history_samples and colour, and a signal declares a unit but no range.
// Auto scaling is therefore not a choice this primitive makes among several, it is the only thing
// it can do with the information the document gives it.
UWidget* BuildHistoryGraph(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return nullptr;

    double Samples = 0;
    if (!Context.Component.Properties.Member(TEXT("history_samples")).Number(Samples) || Samples < 1)
    {
        // Unreachable through LoadPackage: the schema requires an integer of at least 1, and the
        // semantic pass caps it at limits::history_samples, which is 4096.
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties/history_samples"),
            TEXT("History graph needs a positive history_samples"), Context.Package.Path() };
        return nullptr;
    }
    FLinearColor Colour;
    if (!Context.Theme.ResolveColour(Context.Component.Properties.Member(TEXT("colour")),
            Context.Component.Pointer + TEXT("/properties/colour"), Colour, OutError))
        return nullptr;

    const int32 Count = FMath::Clamp(static_cast<int32>(Samples), 1, 4096);
    UCanvasPanel* Panel = MakePanel(Tree);
    UDashLines* Trace = Tree.ConstructWidget<UDashLines>();

    // Until a connector exists, the buffer is the gate switch's synthetic ramp: a straight line from
    // zero to the forced fraction across the whole window. It is not data and the report says so.
    // PLAN 4.9 replaces it with a real series, and only then does a changing trace mean anything.
    TArray<double> Buffer;
    Buffer.Reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index)
        Buffer.Add(Count > 1 ? static_cast<double>(Context.Fraction) * Index / (Count - 1) : 0.0);

    TArray<FVector2D> Points;
    if (Buffer.Num() >= 2)
    {
        double Low = Buffer[0], High = Buffer[0];
        for (double Value : Buffer) { Low = FMath::Min(Low, Value); High = FMath::Max(High, Value); }
        const double Span = High - Low;
        // A buffer whose values are all equal has no span to scale against. Drawing it across the
        // vertical centre is the reading that neither divides by zero nor invents a slope.
        const double Padding = Span > 0.0 ? Span * 0.05 : 0.0;
        Low -= Padding;
        High += Padding;
        const double Scale = High - Low;
        Points.Reserve(Buffer.Num());
        for (int32 Index = 0; Index < Buffer.Num(); ++Index)
        {
            const double X = static_cast<double>(Index) / (Buffer.Num() - 1);
            const double Y = Scale > 0.0 ? 1.0 - (Buffer[Index] - Low) / Scale : 0.5;
            Points.Add(FVector2D(X, Y));
        }
    }
    // Fewer than two samples is not a trace. The widget draws nothing and the component presents its
    // missing-data state, rather than a single dot that would read as a reading.
    Trace->SetLines(Points, Colour, FMath::Max(1.f, static_cast<float>(Rect.Size.Y) * 0.02f));
    AddFilling(Tree, Panel, Trace);

    if (!ApplyMissingData(Context, Tree, Panel, Trace, nullptr, OutError)) return nullptr;
    ApplyClipping(Panel, Context.Component);
    return Panel;
}
}
