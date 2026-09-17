#pragma once
#include "Commandlets/Commandlet.h"
#include "MakeSmokeAssetsCommandlet.generated.h"
UCLASS()
class UMakeSmokeAssetsCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UMakeSmokeAssetsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
