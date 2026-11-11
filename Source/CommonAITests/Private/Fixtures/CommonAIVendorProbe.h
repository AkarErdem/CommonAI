#pragma once

#include "CoreMinimal.h"
#include "AthenaAIComponent.h"
#include "AthenaAIDataManager.h"
#include "AthenaAIAgentPreset.h"
#include "AthenaAITask.h"
#include "AthenaAIService.h"
#include "CommonAIVendorProbe.generated.h"

// Developer-only characterization fixtures. The production consumer API remains vendor-free.
UCLASS()
class UCommonAIVendorProbeTask : public UAthenaAITask
{
    GENERATED_BODY()
public:
    void Configure(FGameplayTag Id, const FGameplayTagContainer& Inputs, int8 InPriority = 1)
    {
        TaskTag = Id;
        Considerations = Inputs;
        Priority = InPriority;
        ReplaceResistance = 0.0f;
        MaxMomentum = 0;
    }
    void ConfigureMomentum(uint8 Maximum, uint8 ReductionPercent, float DecaySeconds)
    {
        MaxMomentum = Maximum;
        MomentumScoreReduction = ReductionPercent;
        MomentumDecreaseTime = DecaySeconds;
    }
    void ConfigureUnlock(float Seconds) { UnlockTaskTime = Seconds; }
    void CompleteNaturally() { FinishTask(); }
    void FailAction() { AbortTask(); }
    virtual void Start(UAthenaAIDataManager* Manager, AAIController* Controller) override
    {
        ++StartCount;
        Super::Start(Manager, Controller);
    }
    int32 StartCount = 0;
};

UCLASS()
class UCommonAIVendorProbeFallback : public UCommonAIVendorProbeTask
{
    GENERATED_BODY()
};

UCLASS()
class UCommonAIVendorProbeService : public UAthenaAIService
{
    GENERATED_BODY()
public:
    void Configure(FGameplayTag Input, EAServiceConditionType Comparison, float Threshold, float IntervalSeconds)
    {
        FAServiceCondition Condition;
        Condition.ConditionType = Comparison;
        Condition.Value = Threshold;
        Conditions.Add(Input, Condition);
        WatchedInput = Input;
        TickInterval = IntervalSeconds;
    }
    int32 Starts = 0;
    int32 Stops = 0;
    int32 Ticks = 0;
    float ValueObservedOnStart = -1.0f;
protected:
    virtual void StartService() override
    {
        const bool bWasRunning = IsRunning();
        Super::StartService();
        if (!bWasRunning && IsRunning())
        {
            ++Starts;
            FAAIConsideration Snapshot;
            if (AAIDataManager->GetConsiderationBP(WatchedInput, Snapshot))
                ValueObservedOnStart = Snapshot.Value;
        }
    }
    virtual void StopService() override
    {
        if (IsRunning()) ++Stops;
        Super::StopService();
    }
    virtual void ServiceTick() override
    {
        ++Ticks;
        Super::ServiceTick();
    }
private:
    UPROPERTY()
    FGameplayTag WatchedInput;
};

UCLASS()
class UCommonAIVendorProbeComponent : public UAthenaAIComponent
{
    GENERATED_BODY()
public:
    void Prepare(UAthenaAIAgentPreset* Preset)
    {
        AIPreset = Preset;
        bEnableSystemOnPawnPossessed = false;
        bResumeScoringOnTaskTerminated = false;
        bCalculateScoresOnTaskTerminated = false;
    }
    void ConfigureAutoResume(bool bResume, bool bRescore)
    {
        bResumeScoringOnTaskTerminated = bResume;
        bCalculateScoresOnTaskTerminated = bRescore;
    }
    void ConfigureAutoPossession(bool bEnable) { bEnableSystemOnPawnPossessed = bEnable; }
    void ConfigureServicePause(bool bPauseServices) { bScorePausingAffectsServices = bPauseServices; }
};

UCLASS()
class UCommonAIVendorEventRecorder : public UObject
{
    GENERATED_BODY()
public:
    int32 EndCount = 0;
    UFUNCTION() void OnEnded(TSubclassOf<UAthenaAITask> TaskClass, float Score, EAAITaskResult Result)
    {
        ++EndCount;
    }
};
