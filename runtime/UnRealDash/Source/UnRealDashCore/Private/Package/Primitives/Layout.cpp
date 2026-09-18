#include "Layout.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"

namespace UnRealDashCore
{
bool ReadRect(const FDashComponent& Component, const FDashPackage& Package, FDashRect& Out, FDashLoadError& OutError)
{
    const FDashValue Rect = Component.Node.Member(TEXT("rect"));
    double X = 0, Y = 0, Width = 0, Height = 0;
    const bool Read = Rect.Member(TEXT("x")).Number(X) && Rect.Member(TEXT("y")).Number(Y) &&
        Rect.Member(TEXT("width")).Number(Width) && Rect.Member(TEXT("height")).Number(Height);
    if (!Read)
    {
        // Unreachable through LoadPackage: the schema requires all four members as numbers. Kept
        // so a hand-built FDashComponent in a test cannot silently lay out at the origin.
        OutError = { TEXT("E_SCHEMA"), 7, Component.Pointer + TEXT("/rect"),
            TEXT("Rect needs numeric x, y, width and height"), Package.Path() };
        return false;
    }
    Out.Position = FVector2D(X, Y);
    Out.Size = FVector2D(Width, Height);
    return true;
}
FVector2D AnchorPoint(const FString& Anchor)
{
    if (Anchor == TEXT("top_left")) return FVector2D(0.0, 0.0);
    if (Anchor == TEXT("top")) return FVector2D(0.5, 0.0);
    if (Anchor == TEXT("top_right")) return FVector2D(1.0, 0.0);
    if (Anchor == TEXT("left")) return FVector2D(0.0, 0.5);
    if (Anchor == TEXT("right")) return FVector2D(1.0, 0.5);
    if (Anchor == TEXT("bottom_left")) return FVector2D(0.0, 1.0);
    if (Anchor == TEXT("bottom")) return FVector2D(0.5, 1.0);
    if (Anchor == TEXT("bottom_right")) return FVector2D(1.0, 1.0);
    return FVector2D(0.5, 0.5); // center, and the schema admits nothing else
}
void ApplyClipping(UWidget* Widget, const FDashComponent& Component)
{
    bool Clip = false;
    if (Widget && Component.Node.Member(TEXT("clipping")).Boolean(Clip))
        Widget->SetClipping(Clip ? EWidgetClipping::ClipToBounds : EWidgetClipping::Inherit);
}
bool ApplyLayout(const FDashComponent& Component, UPanelSlot* Slot, const FDashPackage& Package,
    FDashLoadError& OutError)
{
    UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Slot);
    if (!Canvas) return true;
    FDashRect Rect;
    if (!ReadRect(Component, Package, Rect, OutError)) return false;
    FString Anchor;
    Component.Node.Member(TEXT("anchor")).String(Anchor);
    const FVector2D Point = AnchorPoint(Anchor);
    // Anchor and alignment are the same point, so a component anchored bottom_right at rect
    // (-20, -20) sits 20 units in from the parent's bottom right corner rather than off the edge.
    Canvas->SetAnchors(FAnchors(Point.X, Point.Y, Point.X, Point.Y));
    Canvas->SetAlignment(Point);
    Canvas->SetAutoSize(false);
    Canvas->SetOffsets(FMargin(Rect.Position.X, Rect.Position.Y, Rect.Size.X, Rect.Size.Y));
    // Stacking comes from the document's z_order, and it has to: the order components are written
    // in is not recoverable at runtime. BoundedParse.cpp:118-132 sorts every object's members by
    // name, deliberately, so that schema diagnostics do not depend on the author's ordering, and
    // JSON object member order is not significant in the first place. Before z_order existed, two
    // overlapping siblings stacked by component id, which is deterministic but is layout by naming
    // convention: a dial named "dial" was painted under its own face image named "face" with no way
    // for the document to say otherwise.
    //
    // Equal z_order still ties by component id, so a document that does not care is unaffected and
    // every render is still deterministic.
    double ZOrder = 0;
    Component.Node.Member(TEXT("z_order")).Number(ZOrder);
    Canvas->SetZOrder(static_cast<float>(ZOrder));
    return true;
}
FString ResolveAspectPolicy(const FDashComponent& Component, const FDashPackage& Package)
{
    FString Policy;
    Component.Node.Member(TEXT("aspect_policy")).String(Policy);
    if (Policy != TEXT("inherit")) return Policy;
    // Walk up taking the policy from CONTAINER ancestors only, and keep walking past any other
    // type. That is exactly what the semantic pass does (SemanticPass.cpp:267-272), and the two
    // must agree: the pass is what rejects a circular gauge under an inherited stretch, so a
    // runtime that resolved inherit differently could non-uniformly scale a dial the validator
    // accepted. Taking the policy from any ancestor, as this did before, was such a divergence.
    FString ParentId = Component.ParentId;
    const auto Nodes = Package.Components();
    for (int32 Depth = 0; Depth < Nodes.Num() && !ParentId.IsEmpty(); ++Depth)
    {
        const FDashComponent* Parent = nullptr;
        for (const auto& Node : Nodes) if (Node.Id == ParentId) { Parent = &Node; break; }
        if (!Parent) break;
        if (Parent->Type == TEXT("container"))
        {
            FString ParentPolicy;
            Parent->Node.Member(TEXT("aspect_policy")).String(ParentPolicy);
            if (ParentPolicy != TEXT("inherit")) return ParentPolicy;
        }
        ParentId = Parent->ParentId;
    }
    // A chain that inherits all the way to the root resolves to preserve. The semantic pass leaves
    // it as inherit and its stretch check simply does not fire, so preserve is the reading that
    // agrees with it and never distorts artwork.
    return TEXT("preserve");
}
namespace
{
const TCHAR* StateKey(EDashSignalState State)
{
    switch (State)
    {
    case EDashSignalState::Stale: return TEXT("stale");
    case EDashSignalState::Unavailable: return TEXT("unavailable");
    case EDashSignalState::Invalid: return TEXT("invalid");
    case EDashSignalState::AgeUnknown: return TEXT("age_unknown");
    default: return nullptr;
    }
}
}
FString PresentationFor(const FDashComponent& Component, EDashSignalState State)
{
    const TCHAR* Key = StateKey(State);
    if (!Key) return FString();
    FString Presentation;
    Component.Node.Member(TEXT("missing_data")).Member(Key).Member(TEXT("presentation")).String(Presentation);
    return Presentation;
}
FString PresentationTokenFor(const FDashComponent& Component, EDashSignalState State)
{
    const TCHAR* Key = StateKey(State);
    if (!Key) return FString();
    FString Token;
    Component.Node.Member(TEXT("missing_data")).Member(Key).Member(TEXT("token")).String(Token);
    return Token;
}
FString PresentationPointerFor(const FDashComponent& Component, EDashSignalState State)
{
    const TCHAR* Key = StateKey(State);
    if (!Key) return Component.Pointer;
    return Component.Pointer + TEXT("/missing_data/") + Key + TEXT("/token");
}
FDashValue BindingFor(const FDashComponent& Component, const FDashPackage& Package)
{
    return Package.Bindings().Member(Component.Id);
}
}
