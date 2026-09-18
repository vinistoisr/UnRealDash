#include "Primitives.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"

namespace UnRealDashCore
{
// A container positions its children by rect and anchor and clips them. It draws nothing itself:
// a container that painted a background would be ornament generated in code, which is the thing
// this chunk exists to keep out of the primitives. A panel behind a group of components is a
// shape, authored in the document, where a designer can restyle it.
UWidget* BuildContainer(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    UCanvasPanel* Panel = MakePanel(Tree);
    // Read the rect even though nothing here consumes it, so a malformed one is reported against
    // this component rather than surfacing later against whichever child happened to be first.
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return nullptr;
    ApplyClipping(Panel, Context.Component);
    return Panel;
}
}
