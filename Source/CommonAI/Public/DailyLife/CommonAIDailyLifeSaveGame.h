#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DailyLife/CommonAIDailyLifeClockSubsystem.h"
#include "DailyLife/CommonAIDailyLifeTypes.h"
#include "CommonAIDailyLifeSaveGame.generated.h"

/** Daily-life state only: no live actors, controller state or reservation handles. */
UCLASS(BlueprintType)
class COMMONAI_API UCommonAIDailyLifeSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    /** False for resident-only snapshots whose caller owns its own time authority. */
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Daily Life")
    bool bHasClock = false;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Daily Life")
    FCommonAIDailyLifeClockState Clock;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Daily Life")
    TArray<FCommonAIDailyLifeState> Residents;
};
