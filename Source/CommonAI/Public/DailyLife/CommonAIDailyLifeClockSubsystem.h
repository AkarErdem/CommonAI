#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CommonAIDailyLifeClockSubsystem.generated.h"

/** Continuous simulation time. Hours are not wrapped, so elapsed days survive save/load. */
USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIDailyLifeClockState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Clock", meta = (ClampMin = "0"))
    double TimeHours = 6.0;

    /** World gameplay seconds per simulation day at time scale one. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Clock", meta = (ClampMin = "1", Units = "s"))
    double DayLengthSeconds = 1440.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Clock", meta = (ClampMin = "0"))
    double TimeScale = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Clock")
    bool bPaused = false;

    bool IsValid() const;
};

/** Optional server-owned clock derived from world gameplay time; no per-frame clock tick. */
UCLASS(Config = Game)
class COMMONAI_API UCommonAIDailyLifeClockSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    UPROPERTY(Config, EditAnywhere, Category = "Clock")
    FCommonAIDailyLifeClockState InitialState;

    UFUNCTION(BlueprintPure, Category = "Common AI|Daily Life|Clock")
    double GetTimeHours() const;

    UFUNCTION(BlueprintPure, Category = "Common AI|Daily Life|Clock")
    FCommonAIDailyLifeClockState GetState() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Common AI|Daily Life|Clock")
    bool SetTimeHours(double TimeHours);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Common AI|Daily Life|Clock")
    bool SetDayLengthSeconds(double DayLengthSeconds);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Common AI|Daily Life|Clock")
    bool SetTimeScale(double TimeScale);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Common AI|Daily Life|Clock")
    bool SetPaused(bool bPaused);

    /** Reanchors saved time to this world; time spent outside gameplay does not advance it. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Common AI|Daily Life|Clock")
    bool RestoreState(const FCommonAIDailyLifeClockState& State);

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
    FCommonAIDailyLifeClockState CurrentState;
    double AnchorWorldSeconds = 0.0;
    bool CanModifyClock() const;
};
