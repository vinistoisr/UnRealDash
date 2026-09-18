#include "DashPackageLoader.h"
#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"

namespace UnRealDashCore
{
namespace
{
FString EngineText(std::string_view Text)
{
    if (Text.empty()) return FString();
    const FUTF8ToTCHAR Converted(Text.data(), static_cast<int32>(Text.size()));
    return FString(Converted.Length(), Converted.Get());
}
}
struct FDashPackage::FImpl
{
    TSharedPtr<dashboard_spec::LoadedPackage> Payload;
    TArray<FDashComponent> Nodes;
    FString InputPath;
};
struct FDashValue::FImpl
{
    // The payload owns the document; component views do not own the node table.
    TSharedPtr<dashboard_spec::LoadedPackage> Owner;
    dashboard_spec::PropertyView View;
};
struct FPackageAccess
{
    static FDashValue Value(TSharedPtr<dashboard_spec::LoadedPackage> Owner, dashboard_spec::PropertyView View)
    {
        FDashValue Result;
        Result.Impl = MakeShared<FDashValue::FImpl>();
        Result.Impl->Owner = MoveTemp(Owner);
        Result.Impl->View = View;
        return Result;
    }
};
FDashValue::FDashValue() = default;
FDashValue::~FDashValue() = default;
FDashValue FDashValue::Member(const FString& Name) const
{
    if (!Impl) return {};
    const FTCHARToUTF8 Utf8(*Name);
    return FPackageAccess::Value(Impl->Owner, Impl->View.Member({Utf8.Get(), static_cast<size_t>(Utf8.Length())}));
}
FDashValue FDashValue::Element(int32 Index) const
{
    return Impl && Index >= 0 ? FPackageAccess::Value(Impl->Owner, Impl->View.Element(Index)) : FDashValue();
}
int32 FDashValue::Num() const { return Impl ? static_cast<int32>(Impl->View.Size()) : 0; }
FString FDashValue::Key(int32 Index) const { return Impl && Index >= 0 ? EngineText(Impl->View.Key(Index)) : FString(); }
bool FDashValue::String(FString& Out) const
{
    std::string_view Value;
    if (!Impl || !Impl->View.String(Value)) return false;
    Out = EngineText(Value);
    return true;
}
bool FDashValue::Number(double& Out) const { return Impl && Impl->View.Number(Out); }
bool FDashValue::Boolean(bool& Out) const { return Impl && Impl->View.Boolean(Out); }
bool FDashValue::Exists() const { return Impl && Impl->View.Exists(); }
FDashPackage::FDashPackage() = default;
FDashPackage::~FDashPackage() = default;
TConstArrayView<FDashComponent> FDashPackage::Components() const
{
    return Impl ? TConstArrayView<FDashComponent>(Impl->Nodes) : TConstArrayView<FDashComponent>();
}
FString FDashPackage::Path() const { return Impl ? Impl->InputPath : FString(); }
FString FDashLoadError::DisplayText() const
{
    return FString::Printf(TEXT("File: %s\nCode: %s\nNumber: %d\nPointer: %s\nMessage: %s"),
        *Path, *CodeName, Code, *Pointer, *Message);
}
void TranslateError(const dashboard_spec::Error& Error, const FString& Path, FDashLoadError& Out)
{
    Out = { UTF8_TO_TCHAR(dashboard_spec::CodeName(Error.code)), static_cast<int32>(Error.code),
        EngineText(Error.pointer.View()), EngineText(Error.message.View()), Path };
}
FString ResolveSchemaDirectory()
{
    return IPlatformFile::GetPlatformPhysical().ConvertToAbsolutePathForExternalAppForRead(
        *FPaths::Combine(FPaths::ProjectDir(), TEXT("Schema")));
}
FString ResolveDashSchemaDirectory() { return ResolveSchemaDirectory(); }
void InitializePackageLoader()
{
    UE_LOG(LogTemp, Display, TEXT("Dashboard schema directory: %s"), *ResolveSchemaDirectory());
}
EDashProfile DefaultDashProfile()
{
#if PLATFORM_ANDROID
    return EDashProfile::Mobile;
#else
    return EDashProfile::Desktop;
#endif
}
bool LoadPackage(const FString& Path, EDashProfile Profile, FDashPackage& Out, FDashLoadError& OutError)
{
    const FTCHARToUTF8 Schema(*ResolveSchemaDirectory());
    dashboard_spec::Validator Validator({Schema.Get(), static_cast<size_t>(Schema.Length())});
    const FTCHARToUTF8 Input(*Path);
    auto Data = MakeShared<FDashPackage::FImpl>();
    auto Owner = MakeShared<dashboard_spec::LoadedPackage>();
    const auto Error = dashboard_spec::PackageReader(Validator).Load(
        {Input.Get(), static_cast<size_t>(Input.Length())},
        Profile == EDashProfile::Mobile ? dashboard_spec::Profile::mobile : dashboard_spec::Profile::desktop, *Owner);
    if (!Error.Ok()) { TranslateError(Error, Path, OutError); return false; }
    Data->InputPath = Path;
    for (size_t Index = 0; Index < Owner->Doc().ComponentCount(); ++Index)
    {
        dashboard_spec::ComponentView View;
        if (!Owner->Doc().ComponentAt(Index, View))
        {
            OutError = { TEXT("E_SCHEMA"), 7, TEXT("/dashboard/components"), TEXT("Cannot read component"), Path };
            return false;
        }
        Data->Nodes.Add({EngineText(View.type), EngineText(View.id), EngineText(View.parent), EngineText(View.pointer.View()),
            FPackageAccess::Value(Owner, View.properties), FPackageAccess::Value(Owner, View.node)});
    }
    // Nodes and the theme keep the payload alive independently of the local reader.
    Data->Payload = Owner;
    Out.Impl = MoveTemp(Data);
    OutError = {};
    return true;
}
FDashValue FDashPackage::Theme() const
{
    return Impl ? FPackageAccess::Value(Impl->Payload, Impl->Payload->Doc().Theme()) : FDashValue();
}
FIntPoint FDashPackage::ReferenceViewport() const
{
    // The schema requires reference_viewport with integer width and height of at least 1, so a
    // loaded package always has one. The zero return is unreachable through LoadPackage and exists
    // so a default-constructed FDashPackage does not read through a null payload.
    if (!Impl) return FIntPoint::ZeroValue;
    const auto Root = Impl->Payload->Doc().Root();
    double Width = 0, Height = 0;
    Root.Member("reference_viewport").Member("width").Number(Width);
    Root.Member("reference_viewport").Member("height").Number(Height);
    return FIntPoint(static_cast<int32>(Width), static_cast<int32>(Height));
}
FDashValue FDashPackage::Bindings() const
{
    return Impl ? FPackageAccess::Value(Impl->Payload, Impl->Payload->Doc().Root().Member("bindings")) : FDashValue();
}
FDashValue FDashPackage::Signals() const
{
    return Impl ? FPackageAccess::Value(Impl->Payload, Impl->Payload->Doc().Signals()) : FDashValue();
}
FString FDashPackage::DefinitionPackText() const
{
    std::string Text;
    if (!Impl || !Impl->Payload->Doc().DefinitionPackText(Text)) return FString();
    return EngineText(Text);
}
FDashValue FDashPackage::Pages() const
{
    return Impl ? FPackageAccess::Value(Impl->Payload, Impl->Payload->Doc().Root().Member("pages")) : FDashValue();
}
TArray<FString> FDashPackage::AssetNames() const
{
    TArray<FString> Result;
    if (Impl) for (auto Name : Impl->Payload->AssetNames()) Result.Add(EngineText(Name));
    return Result;
}
bool FDashPackage::Asset(const FString& Reference, TConstArrayView<uint8>& Out, FDashLoadError& OutError) const
{
    Out = {};
    if (!Impl) { OutError = { TEXT("E_PKG_ASSET_NOT_IN_PACKAGE"), 44, TEXT(""), Reference, TEXT("") }; return false; }
    const FTCHARToUTF8 Utf8(*Reference);
    // The document's spelling is resolved to the name the entries were admitted under before it is
    // looked up. A document may write "assets//shared.png" or "assets/./shared.png" for the entry
    // stored as "assets/shared.png", and the normalization rules are the package path gate, so
    // they stay in the library rather than being restated here.
    std::string Normalized;
    const auto Resolved = Impl->Payload->ResolveAssetName({Utf8.Get(), static_cast<size_t>(Utf8.Length())}, Normalized);
    if (!Resolved.Ok()) { TranslateError(Resolved, Impl->InputPath, OutError); return false; }
    std::span<const std::uint8_t> Bytes;
    const auto Error = Impl->Payload->Asset(Normalized, Bytes);
    if (!Error.Ok()) { TranslateError(Error, Impl->InputPath, OutError); return false; }
    Out = TConstArrayView<uint8>(Bytes.data(), static_cast<int32>(Bytes.size()));
    OutError = {};
    return true;
}
}
