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
    TArray<FString> AssetNames() const;
    bool Asset(const FString& NormalizedName, TConstArrayView<uint8>& Out, FDashLoadError& OutError) const;
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
}
