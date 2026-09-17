#include "MakeSmokeAssetsCommandlet.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "MaterialShared.h"
#include "AssetCompilingManager.h"
#include "RHI.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"

UMakeSmokeAssetsCommandlet::UMakeSmokeAssetsCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}
int32 UMakeSmokeAssetsCommandlet::Main(const FString& Params)
{
    (void)Params;
    // The commandlet is the editor composition boundary for compilation and package I/O.
    const auto Save = [](UPackage* Package, UObject* Asset, const FString& Extension) {
        const FString Path = FPackageName::LongPackageNameToFilename(Package->GetName(), Extension);
        if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true)) { return false; }
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *Path, Args);
    };
    UPackage* MaterialPackage = CreatePackage(TEXT("/Game/Smoke/M_Smoke"));
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Cast<UMaterial>(Factory->FactoryCreateNew(UMaterial::StaticClass(), MaterialPackage,
        TEXT("M_Smoke"), RF_Public | RF_Standalone, nullptr, nullptr));
    if (!Material) { UE_LOG(LogTemp, Error, TEXT("Cannot create /Game/Smoke/M_Smoke")); return 1; }
    UTexture2D* DefaultTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
    if (!DefaultTexture) { UE_LOG(LogTemp, Error, TEXT("Cannot load material default texture")); return 1; }
    {
        FMaterialUpdateContext Update;
        Update.AddMaterial(Material);
        Material->SetShadingModel(MSM_Unlit);
        Material->BlendMode = BLEND_Opaque;
        auto* Texture = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
        Texture->ParameterName = TEXT("BaseTexture");
        Texture->Texture = DefaultTexture;
        Texture->SamplerType = SAMPLERTYPE_Color;
        auto* Value = NewObject<UMaterialExpressionScalarParameter>(Material);
        Value->ParameterName = TEXT("Value");
        Value->DefaultValue = 1;
        auto* Multiply = NewObject<UMaterialExpressionMultiply>(Material);
        Multiply->A.Connect(0, Texture);
        Multiply->B.Connect(0, Value);
        Material->GetExpressionCollection().AddExpression(Texture);
        Material->GetExpressionCollection().AddExpression(Value);
        Material->GetExpressionCollection().AddExpression(Multiply);
        Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Multiply);
        Material->PostEditChange();
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
    if (!Resource || !Resource->GetCompileErrors().IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Smoke material compilation did not produce an error-free resource"));
        if (Resource)
        {
            for (const FString& Error : Resource->GetCompileErrors()) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); }
        }
        return 1;
    }
    if (!Save(MaterialPackage, Material, FPackageName::GetAssetPackageExtension()))
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot save /Game/Smoke/M_Smoke")); return 1;
    }
    UPackage* LevelPackage = CreatePackage(TEXT("/Game/Smoke/L_Smoke"));
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("L_Smoke"), LevelPackage, false);
    if (!World) { UE_LOG(LogTemp, Error, TEXT("Cannot create /Game/Smoke/L_Smoke")); return 1; }
    // CreateWorld does not apply the asset flags SavePackage requires of a top level object, so the
    // save refuses the package rather than writing one that would lose the world on load.
    World->SetFlags(RF_Public | RF_Standalone);
    LevelPackage->MarkPackageDirty();
    const bool bSaved = Save(LevelPackage, World, FPackageName::GetMapPackageExtension());
    World->DestroyWorld(false);
    if (!bSaved) { UE_LOG(LogTemp, Error, TEXT("Cannot save /Game/Smoke/L_Smoke")); return 1; }
    UE_LOG(LogTemp, Display, TEXT("Saved /Game/Smoke/M_Smoke and /Game/Smoke/L_Smoke"));
    return 0;
}

