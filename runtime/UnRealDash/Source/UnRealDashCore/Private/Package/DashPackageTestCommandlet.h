#pragma once
#include "Commandlets/Commandlet.h"
#include "DashPackageTestCommandlet.generated.h"
UCLASS()
class UDashPackageTestCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UDashPackageTestCommandlet();
    virtual int32 Main(const FString& Params) override;
};
