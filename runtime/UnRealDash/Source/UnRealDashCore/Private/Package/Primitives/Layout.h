#pragma once
#include "CoreMinimal.h"
#include "UnRealDashCore/ComponentRegistry.h"

class UPanelSlot;
class UWidget;

namespace UnRealDashCore
{
// Geometry read straight off a component node. Rects are in reference-viewport units: the screen
// scales the whole document from the reference viewport to the real one exactly once, so a
// component declared 320 by 180 covers 320 by 180 pixels whenever the two match. The screenshot
// gate's arithmetic depends on that and on nothing else in the render path adding a scale.
struct FDashRect
{
    FVector2D Position = FVector2D::ZeroVector;
    FVector2D Size = FVector2D::ZeroVector;
};

bool ReadRect(const FDashComponent& Component, const FDashPackage& Package, FDashRect& Out, FDashLoadError& OutError);

// The schema's nine anchors as a normalized point. top_left is (0,0) and bottom_right is (1,1).
FVector2D AnchorPoint(const FString& Anchor);

// Positions a child in its parent from rect and anchor, and applies clipping. Canvas slots are the
// only slot type this touches; any other slot type is left alone, because a primitive that chose a
// non-canvas panel is responsible for its own arrangement.
bool ApplyLayout(const FDashComponent& Component, UPanelSlot* Slot, const FDashPackage& Package,
    FDashLoadError& OutError);

// Applies the component's clipping flag to the widget itself.
void ApplyClipping(UWidget* Widget, const FDashComponent& Component);

// Resolves aspect_policy, following inherit up the parent chain. The semantic pass already rejects
// a circular gauge under an inherited stretch, so this only has to agree with it, not re-police it.
FString ResolveAspectPolicy(const FDashComponent& Component, const FDashPackage& Package);

// The component's declared presentation for the state in force, or an empty string when the state
// is Valid. Reading it never fails: the schema requires all four states on every component.
FString PresentationFor(const FDashComponent& Component, EDashSignalState State);
FString PresentationTokenFor(const FDashComponent& Component, EDashSignalState State);
FString PresentationPointerFor(const FDashComponent& Component, EDashSignalState State);

// The binding entry for this component, or an invalid value when the document binds nothing to it.
FDashValue BindingFor(const FDashComponent& Component, const FDashPackage& Package);
}
