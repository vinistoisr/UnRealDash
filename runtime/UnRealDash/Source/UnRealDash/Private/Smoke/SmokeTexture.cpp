#include "SmokeTexture.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Paths.h"

FSmokeTextureResult LoadSmokeTexture(const FString& AbsolutePath, const FSmokeReadBytes& ReadBytes,
    IImageWrapperModule& Images, const FSmokeLog& Log)
{
    Log(TEXT("Smoke PNG path=") + AbsolutePath);
    auto Failure = [&AbsolutePath](const FString& Message) { return FSmokeTextureResult{nullptr, AbsolutePath + TEXT(": ") + Message}; };
    if (FPaths::IsRelative(AbsolutePath)) { return Failure(TEXT("PNG path must be absolute")); }
    TArray<uint8> Bytes;
    FString Error;
    if (!ReadBytes(AbsolutePath, Bytes, Error)) { return Failure(Error); }
    const auto Wrapper = Images.CreateImageWrapper(EImageFormat::PNG);
    if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num())) { return Failure(TEXT("Malformed PNG")); }
    const int64 Width = Wrapper->GetWidth();
    const int64 Height = Wrapper->GetHeight();
    // Bound decoded memory before asking the decoder to allocate it.
    if (Width <= 0 || Height <= 0 || Width > 4096 || Height > 4096) { return Failure(TEXT("PNG dimensions must be 1 through 4096")); }
    TArray64<uint8> Raw;
    if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw) || Raw.Num() != Width * Height * 4) { return Failure(TEXT("PNG decode failed")); }
    UTexture2D* Texture = UTexture2D::CreateTransient(static_cast<int32>(Width), static_cast<int32>(Height), PF_B8G8R8A8);
    if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0) { return Failure(TEXT("Texture allocation failed")); }
    auto& Bulk = Texture->GetPlatformData()->Mips[0].BulkData;
    void* Destination = Bulk.Lock(LOCK_READ_WRITE);
    if (!Destination) { Bulk.Unlock(); return Failure(TEXT("Texture storage unavailable")); }
    FMemory::Memcpy(Destination, Raw.GetData(), Raw.Num());
    Bulk.Unlock();
    Texture->SRGB = true;
    Texture->UpdateResource();
    return {Texture, {}};
}
