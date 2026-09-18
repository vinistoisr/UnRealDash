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
FBuiltComponent BuildHistoryGraph(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return {};

    double Samples = 0;
    if (!Context.Component.Properties.Member(TEXT("history_samples")).Number(Samples) || Samples < 1)
    {
        // Unreachable through LoadPackage: the schema requires an integer of at least 1, and the
        // semantic pass caps it at limits::history_samples, which is 4096.
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties/history_samples"),
            TEXT("History graph needs a positive history_samples"), Context.Package.Path() };
        return {};
    }
    FLinearColor Colour;
    if (!Context.Theme.ResolveColour(Context.Component.Properties.Member(TEXT("colour")),
            Context.Component.Pointer + TEXT("/properties/colour"), Colour, OutError))
        return {};

    const int32 Count = FMath::Clamp(static_cast<int32>(Samples), 1, 4096);
    UCanvasPanel* Panel = MakePanel(Tree);
    UDashLines* Trace = Tree.ConstructWidget<UDashLines>();

    // Nothing has arrived yet, so the trace starts empty. Fewer than two samples is not a trace:
    // the widget draws nothing and the component presents its missing-data state, rather than a
    // single dot that would read as a reading.
    //
    // Chunk 12 filled this at build time from the -udash-fraction gate switch, because there was no
    // way to tell a widget a new number. There is now, and a synthetic ramp standing in for a
    // series would be inventing data.
    Trace->SetLines({}, Colour, FMath::Max(1.f, static_cast<float>(Rect.Size.Y) * 0.02f));
    AddFilling(Tree, Panel, Trace);

    FMissingDataPresenter Presenter;
    if (!BuildMissingData(Context, Tree, Panel, Trace, nullptr, Presenter, OutError)) return {};
    ApplyClipping(Panel, Context.Component);
    if (!BindingFor(Context.Component, Context.Package).Exists()) return {Panel, {}};

    // A ring of the last Count readings, allocated once here and only written afterwards. An
    // updater that allocated per frame would be a stutter in a moving car.
    const auto Buffer = MakeShared<TArray<double>>();
    Buffer->Reserve(Count);
    const float Thickness = FMath::Max(1.f, static_cast<float>(Rect.Size.Y) * 0.02f);
    return {Panel, [Trace, Buffer, Count, Colour, Thickness, Presenter](const FDashSignalValue& Reading)
    {
        if (Reading.bHasValue)
        {
            if (Buffer->Num() == Count) Buffer->RemoveAt(0, 1, EAllowShrinking::No);
            Buffer->Add(Reading.Value);
        }
        TArray<FVector2D> Points;
        if (Buffer->Num() >= 2)
        {
            double Low = (*Buffer)[0], High = (*Buffer)[0];
            for (double V : *Buffer) { Low = FMath::Min(Low, V); High = FMath::Max(High, V); }
            const double Span = High - Low;
            const double Padding = Span > 0.0 ? Span * 0.05 : 0.0;
            Low -= Padding;
            High += Padding;
            const double Scale = High - Low;
            Points.Reserve(Buffer->Num());
            for (int32 Index = 0; Index < Buffer->Num(); ++Index)
            {
                const double X = static_cast<double>(Index) / (Buffer->Num() - 1);
                const double Y = Scale > 0.0 ? 1.0 - ((*Buffer)[Index] - Low) / Scale : 0.5;
                Points.Add(FVector2D(X, Y));
            }
        }
        Trace->SetLines(Points, Colour, Thickness);
        Presenter.Apply(Reading.State);
    }};
}
}
