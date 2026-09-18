#pragma once
#include "CoreMinimal.h"
#include "UnRealDashCore/DashPackageLoader.h"
#include "UnRealDashCore/DashTheme.h"
class UWidget;
class UWidgetTree;
namespace UnRealDashCore
{
struct FComponentContext
{
    const FDashComponent& Component;
    const FDashPackage& Package;
    EDashProfile Profile;
    UWidget* Parent;
    // Chunk 10 pinned the four fields above. Chunk 11 adds two, because a builder cannot resolve a
    // colour without the theme and cannot render a missing-data state without knowing which one is
    // in force. Both refer to state the caller owns for the whole build.
    const FDashTheme& Theme;
    EDashSignalState State;
    // Where a bound gauge sits in its declared range, 0 to 1. Chunk 12 adds it for the same reason
    // chunk 11 added State: the connectors that supply real values are PLAN 4.9, and a dial cannot
    // be captured at a known deflection before one exists. It is a gate input, never data.
    float Fraction = 0.f;
};
using FComponentBuilder = TFunction<UWidget*(const FComponentContext&, FDashLoadError&)>;

// How a built parent takes a child. It is registered beside the builder, keyed by the PARENT's
// type, so the tree builder never learns that a page_switch routes children into per-page panels
// while a container simply adds them. Without this the page rule would have to be a type check in
// FWidgetTreeBuilder, which the chunk 11 constraint "nothing switches on a component type string
// outside the registry" forbids.
struct FAdoptContext
{
    UWidget* Parent;
    const FDashComponent& ParentComponent;
    UWidget* Child;
    const FDashComponent& ChildComponent;
    const FDashPackage& Package;
};
using FComponentAdopter = TFunction<bool(const FAdoptContext&, FDashLoadError&)>;

// The default: cast the parent to a panel and add the child. A parent whose type registers nothing
// else gets this, which is what container and every future panel primitive wants.
UNREALDASHCORE_API bool AdoptIntoPanel(const FAdoptContext& Context, FDashLoadError& OutError);

class UNREALDASHCORE_API FComponentRegistry
{
public:
    void Register(const FString& Type, FComponentBuilder Builder, FComponentAdopter Adopter = nullptr);
    UWidget* Build(const FComponentContext& Context, FDashLoadError& OutError) const;
    // Adds Child to Parent using the adopter registered for the parent component's type.
    bool Adopt(const FAdoptContext& Context, FDashLoadError& OutError) const;
    bool Knows(const FString& Type) const { return Builders.Contains(Type); }
private:
    struct FEntry { FComponentBuilder Builder; FComponentAdopter Adopter; };
    TMap<FString, FEntry> Builders;
};
class UNREALDASHCORE_API FWidgetTreeBuilder
{
public:
    bool Build(const FDashPackage& Package, EDashProfile Profile, const FComponentRegistry& Registry,
        const FDashTheme& Theme, EDashSignalState State, float Fraction, UWidget*& OutRoot,
        FDashLoadError& OutError);
    // Exact document IDs remain attached to their widgets, independent of traversal order.
    const TMap<FString, UWidget*>& WidgetsById() const { return Widgets; }
private:
    TMap<FString, UWidget*> Widgets;
};
// The Stage 0 primitive set: container, readout, image, shape, indicator and page_switch. The three
// the schema also defines, analog_dial, bar_gauge and history_graph, belong to PLAN 4.6 and are
// deliberately not registered here, so a document using one fails with a named error rather than
// rendering a placeholder that looks finished.
UNREALDASHCORE_API void RegisterStage0Builders(FComponentRegistry& Registry, UWidgetTree& Tree);
}
