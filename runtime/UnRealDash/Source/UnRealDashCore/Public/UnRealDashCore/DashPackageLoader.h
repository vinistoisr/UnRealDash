#pragma once
#include "CoreMinimal.h"

namespace UnRealDashCore
{
enum class EDashProfile : uint8 { Mobile, Desktop };
struct UNREALDASHCORE_API FDashLoadError
{
    FString CodeName;
    int32 Code = 0;
    FString Pointer;
    FString Message;
    FString Path;
    FString DisplayText() const;
};
// Typed, immutable view that retains its owning package.
class UNREALDASHCORE_API FDashValue
{
public:
    FDashValue();
    ~FDashValue();
    FDashValue Member(const FString& Name) const;
    FDashValue Element(int32 Index) const;
    int32 Num() const;
    FString Key(int32 Index) const;
    bool String(FString& Out) const;
    bool Number(double& Out) const;
    bool Boolean(bool& Out) const;
    bool Exists() const;
private:
    struct FImpl;
    TSharedPtr<FImpl> Impl;
    friend class FDashPackage;
    friend struct FPackageAccess;
};
struct UNREALDASHCORE_API FDashComponent
{
    FString Type;
    FString Id;
    FString ParentId;
    FString Pointer;
    FDashValue Properties;
    FDashValue Node;
};
class FDashPackage;
// Declared before the class so the friend declaration below refers to this one. Without it the
// friend declares a second LoadPackage with no dll linkage, which the monolithic game target
// tolerates because the API macro is empty there, and the modular editor target rejects with
// "redefinition; different linkage".
UNREALDASHCORE_API bool LoadPackage(const FString& Path, EDashProfile Profile, FDashPackage& Out, FDashLoadError& OutError);

class UNREALDASHCORE_API FDashPackage
{
public:
    FDashPackage();
    ~FDashPackage();
    TConstArrayView<FDashComponent> Components() const;
    FDashValue Theme() const;
    // Document units. Component rects are expressed in these, so a document whose reference
    // viewport matches the capture resolution maps one document unit to one pixel. The
    // screenshot gate's pixel arithmetic depends on that mapping.
    FIntPoint ReferenceViewport() const;
    // Keyed by component id. Each entry carries property, signal and an optional format.
    FDashValue Bindings() const;
    // Keyed by page name, each an array of component ids.
    FDashValue Pages() const;
    // The package's signals document, if it ships one. A binding names a signal by string and the
    // registry works in numbers; this is the table that joins them.
    FDashValue Signals() const;
    // The package's definition pack as JSON text, for DefinitionPackBuilder. Empty when the
    // package ships none, which is every package that is not driven by a binary telemetry stream.
    FString DefinitionPackText() const;
    TArray<FString> AssetNames() const;
    // Takes an asset reference exactly as the document writes it and normalizes it through the
    // package path rules before looking it up.
    bool Asset(const FString& Reference, TConstArrayView<uint8>& Out, FDashLoadError& OutError) const;
    FString Path() const;
private:
    struct FImpl;
    TSharedPtr<FImpl> Impl;
    friend struct FPackageAccess;
    // The API macro is required here too. Without it the friend declaration introduces a second
    // LoadPackage with no dll linkage, which a modular build rejects as inconsistent.
    friend UNREALDASHCORE_API bool LoadPackage(const FString&, EDashProfile, FDashPackage&, FDashLoadError&);
};
UNREALDASHCORE_API EDashProfile DefaultDashProfile();
UNREALDASHCORE_API bool LoadPackage(const FString& Path, EDashProfile Profile, FDashPackage& Out, FDashLoadError& OutError);
UNREALDASHCORE_API void InitializePackageLoader();
// The absolute schema directory the loader validates against. Exposed so a connector can
// validate a definition pack with the same schemas the document was validated with.
UNREALDASHCORE_API FString ResolveDashSchemaDirectory();
}
