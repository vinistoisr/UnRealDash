#pragma once
#include "Commandlets/Commandlet.h"
#include "DashPlayerConfigTestCommandlet.generated.h"
UCLASS()
class UDashPlayerConfigTestCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UDashPlayerConfigTestCommandlet();
    virtual int32 Main(const FString& Params) override;
};
