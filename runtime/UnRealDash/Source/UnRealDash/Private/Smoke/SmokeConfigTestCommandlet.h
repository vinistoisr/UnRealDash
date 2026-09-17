#pragma once
#include "Commandlets/Commandlet.h"
#include "SmokeConfigTestCommandlet.generated.h"
UCLASS()
class USmokeConfigTestCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    USmokeConfigTestCommandlet();
    virtual int32 Main(const FString& Params) override;
};
