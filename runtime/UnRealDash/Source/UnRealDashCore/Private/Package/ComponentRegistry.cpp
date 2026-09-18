#include "UnRealDashCore/ComponentRegistry.h"
#include "Primitives/Layout.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"

namespace UnRealDashCore
{
void FComponentRegistry::Register(const FString& Type, FComponentBuilder Builder, FComponentAdopter Adopter)
{
    Builders.Add(Type, FEntry{ MoveTemp(Builder), Adopter ? MoveTemp(Adopter) : FComponentAdopter(&AdoptIntoPanel) });
}
FBuiltComponent FComponentRegistry::Build(const FComponentContext& Context, FDashLoadError& OutError) const
{
    const FEntry* Entry = Builders.Find(Context.Component.Type);
    if (!Entry)
    {
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer,
            FString::Printf(TEXT("Unknown component type: %s"), *Context.Component.Type), Context.Package.Path() };
        return {};
    }
    FBuiltComponent Built = Entry->Builder(Context, OutError);
    if (!Built.Widget && OutError.CodeName.IsEmpty())
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer,
            FString::Printf(TEXT("Builder failed for type: %s"), *Context.Component.Type), Context.Package.Path() };
    return Built;
}
bool FComponentRegistry::Adopt(const FAdoptContext& Context, FDashLoadError& OutError) const
{
    const FEntry* Entry = Builders.Find(Context.ParentComponent.Type);
    if (!Entry)
    {
        OutError = { TEXT("E_SCHEMA"), 7, Context.ParentComponent.Pointer,
            FString::Printf(TEXT("Unknown component type: %s"), *Context.ParentComponent.Type), Context.Package.Path() };
        return false;
    }
    return Entry->Adopter(Context, OutError);
}
bool AdoptIntoPanel(const FAdoptContext& Context, FDashLoadError& OutError)
{
    UPanelWidget* Panel = Cast<UPanelWidget>(Context.Parent);
    UPanelSlot* Slot = Panel ? Panel->AddChild(Context.Child) : nullptr;
    if (!Slot)
    {
        OutError = { TEXT("E_SCHEMA"), 7, Context.ChildComponent.Pointer,
            TEXT("Parent cannot contain this component"), Context.Package.Path() };
        return false;
    }
    return ApplyLayout(Context.ChildComponent, Slot, Context.Package, OutError);
}
bool FWidgetTreeBuilder::Build(const FDashPackage& Package, EDashProfile Profile, const FComponentRegistry& Registry,
    const FDashTheme& Theme, EDashSignalState State, float Fraction, UWidget*& OutRoot,
    FDashLoadError& OutError)
{
    Widgets.Reset();
    Updaters.Reset();
    OutRoot = nullptr;
    OutError = {};
    const auto Nodes = Package.Components();
    // Parent lookup by id, so an adopter can name the parent component rather than only its widget.
    TMap<FString, const FDashComponent*> ById;
    for (const auto& Node : Nodes) ById.Add(Node.Id, &Node);
    UWidget* Root = nullptr;
    while (Widgets.Num() < Nodes.Num())
    {
        bool Progress = false;
        for (const auto& Node : Nodes)
        {
            if (Widgets.Contains(Node.Id)) continue;
            UWidget* Parent = nullptr;
            if (!Node.ParentId.IsEmpty())
            {
                UWidget** Found = Widgets.Find(Node.ParentId);
                if (!Found) continue;
                Parent = *Found;
            }
            const FBuiltComponent Component = Registry.Build({Node, Package, Profile, Parent, Theme, State, Fraction}, OutError);
            UWidget* Built = Component.Widget;
            if (!Built) { Widgets.Reset(); Updaters.Reset(); return false; }
            Built->SetToolTipText(FText::FromString(Node.Id));
            // Only the components that bind a signal produce one, which is most of them not.
            if (Component.Updater) Updaters.Add(Node.Id, Component.Updater);
            if (Parent)
            {
                const FDashComponent* const* ParentNode = ById.Find(Node.ParentId);
                if (!ParentNode)
                {
                    OutError = { TEXT("E_UNRESOLVED_PARENT"), 12, Node.Pointer,
                        TEXT("Parent component is not in the document"), Package.Path() };
                    Widgets.Reset();
                    Updaters.Reset();
                    return false;
                }
                if (!Registry.Adopt({Parent, **ParentNode, Built, Node, Package}, OutError))
                { Widgets.Reset(); Updaters.Reset(); return false; }
            }
            else
            {
                if (Root)
                {
                    OutError = { TEXT("E_MULTIPLE_ROOTS"), 16, Node.Pointer, TEXT("Multiple root widgets"), Package.Path() };
                    Widgets.Reset();
                    Updaters.Reset();
                    return false;
                }
                Root = Built;
            }
            Widgets.Add(Node.Id, Built);
            Progress = true;
        }
        if (!Progress)
        {
            OutError = { TEXT("E_UNRESOLVED_PARENT"), 12, TEXT("/dashboard/components"), TEXT("Cannot resolve component parents"), Package.Path() };
            Widgets.Reset();
            return false;
        }
    }
    if (!Root)
    {
        OutError = { TEXT("E_NO_ROOT"), 15, TEXT("/dashboard/components"), TEXT("No root widget"), Package.Path() };
        return false;
    }
    OutRoot = Root;
    return true;
}
}
