#pragma once

#include "CoreMinimal.h"
#include "DailyLife/CommonAIDailyLifeTypes.h"
#include "CommonAIDailyLifeContextFixture.generated.h"

/** Test-owned external time source with no movement or persistence implementation. */
UCLASS()
class UCommonAIDailyLifeContextFixture : public UObject, public ICommonAIDailyLifeContext
{
    GENERATED_BODY()
public:
    double TimeHours = 0.0;

    virtual double GetDailyLifeTimeHours_Implementation() const override { return TimeHours; }
    virtual FGameplayTag GetDailyLifeInterruptionReason_Implementation() const override { return {}; }
};
