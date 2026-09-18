#include "Primitives.h"
#include "Blueprint/WidgetTree.h"

namespace UnRealDashCore
{
// The whole registration surface for the Stage 0 primitive set. Adding a primitive later means
// adding one file and one line here, and touching neither FComponentRegistry nor FWidgetTreeBuilder.
//
// analog_dial, bar_gauge and history_graph are deliberately absent. They belong to PLAN 4.6, and a
// placeholder registered for them would render something that looks finished, which is worse than
// a named failure: the gap would stop being visible to anyone reading a screen.
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
}
}
