#pragma once
#include "Commandlets/Commandlet.h"
#include "SignalCoreSmokeCommandlet.generated.h"

UCLASS()
class USignalCoreSmokeCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    USignalCoreSmokeCommandlet();
    virtual int32 Main(const FString& Params) override;
};
