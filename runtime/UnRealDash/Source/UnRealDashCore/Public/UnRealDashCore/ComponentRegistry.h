#pragma once
#include "CoreMinimal.h"
#include "UnRealDashCore/DashPackageLoader.h"
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
};
using FComponentBuilder = TFunction<UWidget*(const FComponentContext&, FDashLoadError&)>;
class UNREALDASHCORE_API FComponentRegistry
{
public:
    void Register(const FString& Type, FComponentBuilder Builder);
    UWidget* Build(const FComponentContext& Context, FDashLoadError& OutError) const;
private:
    TMap<FString, FComponentBuilder> Builders;
};
class UNREALDASHCORE_API FWidgetTreeBuilder
{
public:
    bool Build(const FDashPackage& Package, EDashProfile Profile, const FComponentRegistry& Registry,
        UWidget*& OutRoot, FDashLoadError& OutError);
    // Exact document IDs remain attached to their widgets, independent of traversal order.
    const TMap<FString, UWidget*>& WidgetsById() const { return Widgets; }
private:
    TMap<FString, UWidget*> Widgets;
};
UNREALDASHCORE_API void RegisterPlaceholderBuilders(FComponentRegistry& Registry, UWidgetTree& Tree);
}
