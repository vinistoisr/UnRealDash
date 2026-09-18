#include "Primitives.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

namespace UnRealDashCore
{
namespace
{
// Page membership lives in the document's top-level `pages` map, not on the child, so a child
// cannot say which page it belongs to and the page_switch has to look it up. The semantic pass has
// already resolved every page name and every component id it lists, so this is routing rather than
// validation. Returns the index of the child's page within the switch's declared `pages` array, or INDEX_NONE.
// The index, not a name, is what connects a child to its panel: the builder adds one panel per
// declared page in declaration order, so the Nth page is the Nth canvas child. Naming the panels
// instead would collide between two page switches that declare the same page name, because both
// panels share the one widget tree as their outer.
int32 PageIndexOfChild(const FDashComponent& Switch, const FDashComponent& Child, const FDashPackage& Package)
{
    const FDashValue Declared = Switch.Properties.Member(TEXT("pages"));
    const FDashValue Pages = Package.Pages();
    for (int32 Index = 0; Index < Declared.Num(); ++Index)
    {
        FString Name;
        if (!Declared.Element(Index).String(Name)) continue;
        const FDashValue Members = Pages.Member(Name);
        for (int32 Member = 0; Member < Members.Num(); ++Member)
        {
            FString Id;
            if (Members.Element(Member).String(Id) && Id == Child.Id) return Index;
        }
    }
    return INDEX_NONE;
}
}
// Swaps the visible page. Every declared page gets its own panel, filling the switch, and all but
// the initial one are collapsed. Collapsed rather than hidden, so a page that is not showing costs
// no layout and cannot contribute a pixel to a capture.
UWidget* BuildPageSwitch(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return nullptr;

    FString Initial;
    if (!Context.Component.Properties.Member(TEXT("initial_page")).String(Initial))
    {
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties/initial_page"),
            TEXT("Page switch needs an initial_page property"), Context.Package.Path() };
        return nullptr;
    }
    UCanvasPanel* Panel = MakePanel(Tree);
    const FDashValue Declared = Context.Component.Properties.Member(TEXT("pages"));
    bool bSawInitial = false;
    for (int32 Index = 0; Index < Declared.Num(); ++Index)
    {
        // One panel per declared element, with no skipping, so the Nth page is always the Nth
        // canvas child and PageIndexOfChild's index is a position the adopter can take directly.
        // Skipping a malformed element instead would silently shift every later page by one.
        UCanvasPanel* Page = MakePanel(Tree);
        AddFilling(Tree, Panel, Page);
        FString Name;
        const bool bActive = Declared.Element(Index).String(Name) && Name == Initial;
        bSawInitial |= bActive;
        Page->SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (!bSawInitial)
    {
        // Unreachable through LoadPackage: the semantic pass requires initial_page to be one of the
        // declared pages. Kept so the switch cannot render every page at once if that ever changes.
        OutError = { TEXT("E_UNRESOLVED_PAGE"), 11, Context.Component.Pointer + TEXT("/properties/initial_page"),
            FString::Printf(TEXT("initial_page %s is not one of the declared pages"), *Initial), Context.Package.Path() };
        return nullptr;
    }
    // A page_switch binds no signal, so it presents no missing-data state; see revision 4 C1.
    ApplyClipping(Panel, Context.Component);
    return Panel;
}
bool AdoptIntoPage(const FAdoptContext& Context, FDashLoadError& OutError)
{
    const int32 Page = PageIndexOfChild(Context.ParentComponent, Context.ChildComponent, Context.Package);
    if (Page == INDEX_NONE)
    {
        OutError = { TEXT("E_UNRESOLVED_PAGE"), 11, Context.ChildComponent.Pointer,
            FString::Printf(TEXT("Component %s is under page switch %s but no page it declares lists it"),
                *Context.ChildComponent.Id, *Context.ParentComponent.Id), Context.Package.Path() };
        return false;
    }
    UCanvasPanel* Switch = Cast<UCanvasPanel>(Context.Parent);
    UCanvasPanel* Target = Switch && Page < Switch->GetChildrenCount()
        ? Cast<UCanvasPanel>(Switch->GetChildAt(Page)) : nullptr;
    if (!Target)
    {
        OutError = { TEXT("E_UNRESOLVED_PAGE"), 11, Context.ChildComponent.Pointer,
            FString::Printf(TEXT("Page switch %s has no panel for page %d"), *Context.ParentComponent.Id, Page),
            Context.Package.Path() };
        return false;
    }
    UPanelSlot* Slot = Target->AddChild(Context.Child);
    if (!Slot)
    {
        OutError = { TEXT("E_SCHEMA"), 7, Context.ChildComponent.Pointer,
            TEXT("Parent cannot contain this component"), Context.Package.Path() };
        return false;
    }
    return ApplyLayout(Context.ChildComponent, Slot, Context.Package, OutError);
}
}
