#include "UnRealDashCore/ComponentRegistry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

namespace UnRealDashCore
{
void FComponentRegistry::Register(const FString& Type, FComponentBuilder Builder)
{
    Builders.Add(Type, MoveTemp(Builder));
}
UWidget* FComponentRegistry::Build(const FComponentContext& Context, FDashLoadError& OutError) const
{
    const auto* Builder = Builders.Find(Context.Component.Type);
    if (!Builder)
    {
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer,
            FString::Printf(TEXT("Unknown component type: %s"), *Context.Component.Type), Context.Package.Path() };
        return nullptr;
    }
    UWidget* Widget = (*Builder)(Context, OutError);
    if (!Widget && OutError.CodeName.IsEmpty())
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer,
            FString::Printf(TEXT("Builder failed for type: %s"), *Context.Component.Type), Context.Package.Path() };
    return Widget;
}
bool FWidgetTreeBuilder::Build(const FDashPackage& Package, EDashProfile Profile, const FComponentRegistry& Registry,
    UWidget*& OutRoot, FDashLoadError& OutError)
{
    Widgets.Reset();
    OutRoot = nullptr;
    OutError = {};
    const auto Nodes = Package.Components();
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
            UWidget* Built = Registry.Build({Node, Package, Profile, Parent}, OutError);
            if (!Built) { Widgets.Reset(); return false; }
            Built->SetToolTipText(FText::FromString(Node.Id));
            if (Parent)
            {
                UPanelWidget* Panel = Cast<UPanelWidget>(Parent);
                if (!Panel || !Panel->AddChild(Built))
                {
                    OutError = { TEXT("E_SCHEMA"), 7, Node.Pointer, TEXT("Parent cannot contain this component"), Package.Path() };
                    Widgets.Reset();
                    return false;
                }
            }
            else
            {
                if (Root)
                {
                    OutError = { TEXT("E_MULTIPLE_ROOTS"), 16, Node.Pointer, TEXT("Multiple root widgets"), Package.Path() };
                    Widgets.Reset();
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
void RegisterPlaceholderBuilders(FComponentRegistry& Registry, UWidgetTree& Tree)
{
    // One placeholder implementation, explicitly registered for the schema's type vocabulary.
    const FComponentBuilder Placeholder = [&Tree](const FComponentContext& Context, FDashLoadError&) -> UWidget*
    {
        auto* Box = Tree.ConstructWidget<UVerticalBox>();
        auto* Border = Tree.ConstructWidget<UBorder>();
        auto* Label = Tree.ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Context.Component.Type + TEXT(" : ") + Context.Component.Id));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        auto Font = Label->GetFont();
        Font.Size = 24;
        Label->SetFont(Font);
        Label->SetAutoWrapText(true);
        Border->SetBrushColor(FLinearColor(0.08f, 0.12f, 0.17f, 1.f));
        Border->SetPadding(FMargin(12.f));
        Border->SetContent(Label);
        Box->AddChild(Border);
        return Box;
    };
    for (const TCHAR* Type : { TEXT("readout"), TEXT("image"), TEXT("shape"), TEXT("analog_dial"),
        TEXT("bar_gauge"), TEXT("indicator"), TEXT("history_graph"), TEXT("container"), TEXT("page_switch") })
        Registry.Register(Type, Placeholder);
}
}
