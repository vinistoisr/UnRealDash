#include "Primitives.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

namespace UnRealDashCore
{
namespace
{
// Decode is the path PLAN 4.0 already proved on the device: IImageWrapperModule, CreateTransient,
// UpdateResource. The bounds here duplicate the package reader's, which has already checked the
// declared dimensions against the manifest and the profile texture budget. They are kept because
// this function allocates from numbers a decoder reports, and a decoder is the wrong place to take
// a size on trust.
bool DecodePng(TConstArrayView<uint8> Bytes, UTexture2D*& Out, FString& Error)
{
    IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    const auto Wrapper = Images.CreateImageWrapper(EImageFormat::PNG);
    if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
    { Error = TEXT("Malformed PNG"); return false; }
    const int64 Width = Wrapper->GetWidth(), Height = Wrapper->GetHeight();
    if (Width <= 0 || Height <= 0 || Width > 4096 || Height > 4096)
    { Error = TEXT("PNG dimensions must be 1 through 4096"); return false; }
    TArray64<uint8> Raw;
    if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw) || Raw.Num() != Width * Height * 4)
    { Error = TEXT("PNG decode failed"); return false; }
    Out = UTexture2D::CreateTransient(static_cast<int32>(Width), static_cast<int32>(Height), PF_B8G8R8A8);
    if (!Out || !Out->GetPlatformData() || Out->GetPlatformData()->Mips.Num() == 0)
    { Error = TEXT("Texture allocation failed"); return false; }
    auto& Bulk = Out->GetPlatformData()->Mips[0].BulkData;
    void* Destination = Bulk.Lock(LOCK_READ_WRITE);
    if (!Destination) { Bulk.Unlock(); Error = TEXT("Texture storage unavailable"); return false; }
    FMemory::Memcpy(Destination, Raw.GetData(), Raw.Num());
    Bulk.Unlock();
    Out->SRGB = true;
    // Nearest filtering keeps a capture identical between runs and between drivers. Bilinear
    // filtering of the same artwork differs in the last bit or two across driver versions, which
    // the screenshot gate would report as a change nobody made.
    Out->Filter = TF_Nearest;
    Out->UpdateResource();
    return true;
}
}
// The workhorse of the set: a dial face, a bezel, an indicator icon and a background are all
// images positioned by the document. It samples artwork and never generates ornament.
//
// It is also never tinted. The schema gives an image only `asset`, so there is no per-component
// opt-in to multiply a theme colour over artwork, and applying one by default is exactly the
// RealDash trap the owner's own notes record: every image gauge there must be forced to white or
// the artwork renders tinted. See chunk-11 revision 4 C5.
UWidget* BuildImage(const FComponentContext& Context, FDashLoadError& OutError, UWidgetTree& Tree)
{
    FDashRect Rect;
    if (!ReadRect(Context.Component, Context.Package, Rect, OutError)) return nullptr;

    FString Name;
    if (!Context.Component.Properties.Member(TEXT("asset")).String(Name))
    {
        // Unreachable through LoadPackage: the schema makes `asset` required on an image.
        OutError = { TEXT("E_SCHEMA"), 7, Context.Component.Pointer + TEXT("/properties/asset"),
            TEXT("Image needs an asset property"), Context.Package.Path() };
        return nullptr;
    }
    TConstArrayView<uint8> Bytes;
    // A missing asset is a readable error naming the component, not a blank space. A blank space
    // in a cluster reads as a design with a gap in it rather than as a package that is wrong.
    if (!Context.Package.Asset(Name, Bytes, OutError))
    {
        OutError.Pointer = Context.Component.Pointer + TEXT("/properties/asset");
        return nullptr;
    }
    UTexture2D* Texture = nullptr;
    FString Error;
    if (!DecodePng(Bytes, Texture, Error))
    {
        OutError = { TEXT("E_PKG_ASSET_UNREADABLE"), 45, Context.Component.Pointer + TEXT("/properties/asset"),
            Name + TEXT(": ") + Error, Context.Package.Path() };
        return nullptr;
    }
    UCanvasPanel* Panel = MakePanel(Tree);
    UImage* Picture = Tree.ConstructWidget<UImage>();
    Picture->SetBrushFromTexture(Texture, true);
    Picture->SetColorAndOpacity(FLinearColor::White);

    // aspect_policy decides the fit, and preserve must never scale non-uniformly. inherit is
    // resolved up the parent chain rather than guessed at, so a face inside a container that
    // declares preserve stays circular.
    const FString Policy = ResolveAspectPolicy(Context.Component, Context.Package);
    UScaleBox* Fit = Tree.ConstructWidget<UScaleBox>();
    Fit->SetStretch(Policy == TEXT("stretch") ? EStretch::Fill : EStretch::ScaleToFit);
    Fit->SetStretchDirection(EStretchDirection::Both);
    if (UScaleBoxSlot* Slot = Cast<UScaleBoxSlot>(Fit->AddChild(Picture)))
    {
        Slot->SetHorizontalAlignment(HAlign_Center);
        Slot->SetVerticalAlignment(VAlign_Center);
    }
    AddFilling(Tree, Panel, Fit);

    if (!ApplyMissingData(Context, Tree, Panel, Fit, nullptr, OutError)) return nullptr;
    ApplyClipping(Panel, Context.Component);
    return Panel;
}
}
