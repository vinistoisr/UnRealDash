#include "Primitives.h"
#include "Blueprint/WidgetTree.h"

namespace UnRealDashCore
{
// The whole registration surface for the Stage 0 primitive set. Adding a primitive later means
// adding one file and one line here, and touching neither FComponentRegistry nor FWidgetTreeBuilder.
//
// All nine schema types are registered as of PLAN 4.6. The three added there draw only the part
// that moves: the needle, the filled portion, the trace. Everything static around them is artwork,
// because the schema gives none of them an asset field and a primitive that drew its own ornament
// could not be restyled by a document.
void RegisterStage0Builders(FComponentRegistry& Registry, UWidgetTree& Tree)
{
    UWidgetTree* Owner = &Tree;
    Registry.Register(TEXT("container"), [Owner](const FComponentContext& Context, FDashLoadError& Error)
        { return BuildContainer(Context, Error, *Owner); });
    Registry.Register(TEXT("readout"), [Owner](const FComponentContext& Context, FDashLoadError& Error)
        { return BuildReadout(Context, Error, *Owner); });
    Registry.Register(TEXT("image"), [Owner](const FComponentContext& Context, FDashLoadError& Error)
        { return BuildImage(Context, Error, *Owner); });
    Registry.Register(TEXT("shape"), [Owner](const FComponentContext& Context, FDashLoadError& Error)
        { return BuildShape(Context, Error, *Owner); });
    Registry.Register(TEXT("indicator"), [Owner](const FComponentContext& Context, FDashLoadError& Error)
        { return BuildIndicator(Context, Error, *Owner); });
    Registry.Register(TEXT("page_switch"), [Owner](const FComponentContext& Context, FDashLoadError& Error)
        { return BuildPageSwitch(Context, Error, *Owner); }, &AdoptIntoPage);
    Registry.Register(TEXT("analog_dial"), [Owner](const FComponentContext& Context, FDashLoadError& Error)
        { return BuildAnalogDial(Context, Error, *Owner); });
    Registry.Register(TEXT("bar_gauge"), [Owner](const FComponentContext& Context, FDashLoadError& Error)
        { return BuildBarGauge(Context, Error, *Owner); });
    Registry.Register(TEXT("history_graph"), [Owner](const FComponentContext& Context, FDashLoadError& Error)
        { return BuildHistoryGraph(Context, Error, *Owner); });
}
}
