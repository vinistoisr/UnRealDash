#pragma once
#include "CoreMinimal.h"
#include "SmokeConfig.h"
class UTexture2D;
class IImageWrapperModule;
struct FSmokeTextureResult
{
    UTexture2D* Texture = nullptr;
    FString Error;
};
using FSmokeReadBytes = TFunction<bool(const FString&, TArray<uint8>&, FString&)>;
FSmokeTextureResult LoadSmokeTexture(const FString& AbsolutePath, const FSmokeReadBytes& ReadBytes,
    IImageWrapperModule& Images, const FSmokeLog& Log);
