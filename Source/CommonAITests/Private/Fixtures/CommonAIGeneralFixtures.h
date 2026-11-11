#pragma once
#include "CoreMinimal.h"
#include "Evaluation/CommonAIEvaluator.h"
#include "Evaluation/CommonAIServiceBehavior.h"
#include "CommonAIAgentComponent.h"
#include "CommonAITags.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Contracts/CommonAIWorkSite.h"
#include "CommonAIGeneralFixtures.generated.h"

UCLASS()
class UCommonAIProbeObservations : public UObject, public ICommonAIObservationProvider
{
    GENERATED_BODY()
public:
    UPROPERTY() TMap<FGameplayTag, FInstancedStruct> Values;
    bool bUnregisterDuringRead = false;
    virtual FInstancedStruct ReadObservation_Implementation(UCommonAIAgentComponent* Agent, FGameplayTag Consideration) const override
    {
        if (bUnregisterDuringRead) Agent->UnregisterInputSource(CommonAITags::Source_External, const_cast<UCommonAIProbeObservations*>(this));
        const FInstancedStruct* Value = Values.Find(Consideration);
        return Value ? *Value : FInstancedStruct();
    }
};

UCLASS()
class UCommonAIProbeReentrantSite : public UCommonAISignaledWorkSite
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UCommonAIAgentComponent> Agent;
    virtual bool ReserveInteraction_Implementation(const FCommonAIInteractionRequest& Request) override
    {
        const bool bReserved = Super::ReserveInteraction_Implementation(Request);
        if (bReserved && Agent) Agent->StopAgent();
        return bReserved;
    }
};

UCLASS()
class UCommonAIProbeServiceRecorder : public UObject
{
    GENERATED_BODY()
public:
    int32 Starts = 0;
    int32 Stops = 0;
    int32 Ticks = 0;
    float ValueOnStart = -1.0f;
};

UCLASS()
class UCommonAIProbeInteractionRecorder : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() FCommonAIInteractionRequest Request;
    int32 Starts = 0;
    int32 ProgressCount = 0;
    UFUNCTION() void OnProgress(FCommonAIActivityProgress Progress) { ++ProgressCount; }
    UFUNCTION() void OnStarted(FCommonAIInteractionRequest InRequest)
    {
        Request = InRequest;
        ++Starts;
    }
};

UCLASS()
class UCommonAIProbeServiceBehavior : public UCommonAIServiceBehavior
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UCommonAIProbeServiceRecorder> Recorder;
    UPROPERTY() FGameplayTag WatchedInput;
    virtual void OnStarted_Implementation(UCommonAIAgentComponent* Agent, FGameplayTag Service, FGuid Generation) override
    {
        if (!Recorder) return;
        ++Recorder->Starts;
        FCommonAIConsiderationSnapshot Value;
        if (Agent->GetConsideration(WatchedInput, Value)) Recorder->ValueOnStart = Value.RawValue;
    }
    virtual void OnTick_Implementation(UCommonAIAgentComponent* Agent, FGameplayTag Service, FGuid Generation, float DeltaSeconds) override
    {
        if (Recorder) ++Recorder->Ticks;
    }
    virtual void OnStopped_Implementation(UCommonAIAgentComponent* Agent, FGameplayTag Service, FGuid Generation) override
    {
        if (Recorder) ++Recorder->Stops;
    }
};

UCLASS()
class UBTTask_CommonAIProbeLatentAbort : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_CommonAIProbeLatentAbort()
    {
        bCreateNodeInstance = true;
        bNotifyTaskFinished = true;
    }
    UPROPERTY() float AbortDelaySeconds = 0.3f;
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override
    {
        return EBTNodeResult::InProgress;
    }
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override
    {
        Owner = &OwnerComp;
        OwnerComp.GetWorld()->GetTimerManager().SetTimer(AbortTimer, this, &UBTTask_CommonAIProbeLatentAbort::FinishAbort, AbortDelaySeconds, false);
        return EBTNodeResult::InProgress;
    }
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type Result) override
    {
        OwnerComp.GetWorld()->GetTimerManager().ClearTimer(AbortTimer);
        Owner.Reset();
        Super::OnTaskFinished(OwnerComp, NodeMemory, Result);
    }
private:
    void FinishAbort()
    {
        if (Owner.IsValid()) FinishLatentAbort(*Owner.Get());
    }
    TWeakObjectPtr<UBehaviorTreeComponent> Owner;
    FTimerHandle AbortTimer;
};
