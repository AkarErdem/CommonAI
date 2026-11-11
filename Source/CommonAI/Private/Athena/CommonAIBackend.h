#pragma once
#include "CoreMinimal.h"
#include "AthenaAIComponent.h"
#include "AthenaAIDataManager.h"
#include "AthenaAIService.h"
#include "CommonAIProfile.h"
#include "Diagnostics/CommonAISnapshot.h"
#include "CommonAIBackend.generated.h"

class UCommonAIAgentComponent;
class UCommonAITaskAdapter;

UCLASS(NotBlueprintable)
class UCommonAIDataManager : public UAthenaAIDataManager
{
    GENERATED_BODY()
public:
    void BeginInputBatch();
    void EndInputBatch();
    const TArray<FCommonAICurveSnapshot>* FindContributions(FGameplayTag Activity) const { return Contributions.Find(Activity); }
protected:
    virtual bool CanReplaceCurrentTask(const UAthenaAITask* NewTask) override;
    virtual void CalculateTasksScore() override;
    virtual float CalculateConsiderationScore(const FGameplayTag& TaskTag, const FGameplayTag& ConsiderationTag,
        UCurveTable* CurveTable, float ModificationFactor) const override;
    virtual void UpdateServices(const FGameplayTag& ConsiderationTag, float NewValue) const override;
private:
    int32 BatchDepth = 0;
    mutable TSet<FGameplayTag> ChangedInputs;
    mutable TMap<FGameplayTag, TArray<FCommonAICurveSnapshot>> Contributions;
};

UCLASS(NotBlueprintable)
class UCommonAIServiceAdapter : public UAthenaAIService
{
    GENERATED_BODY()
public:
    void Configure(const FCommonAIServiceDefinition& InDefinition);
    virtual void Initialize(UAthenaAIDataManager* DataManager, AAIController* Controller) override;
    virtual void Cleanup() override;
    void Reconcile();
    void CommitTransition();
    FCommonAIServiceSnapshot Snapshot() const;
    FGameplayTag GetId() const { return Definition.Id; }
protected:
    virtual void StartService() override;
    virtual void StopService() override;
    virtual void ServiceTick() override;
private:
    UPROPERTY() FCommonAIServiceDefinition Definition;
    UPROPERTY(Instanced) TObjectPtr<UCommonAIServiceBehavior> Behavior;
    TWeakObjectPtr<UCommonAIAgentComponent> Agent;
    FGuid Generation;
    bool bTransitionPending = false;
    bool bDesiredRunning = false;
    double LastTickTime = 0.0;
};

UCLASS(NotBlueprintable)
class UCommonAIBackend : public UAthenaAIComponent
{
    GENERATED_BODY()
public:
    UCommonAIBackend();
    bool Prepare(UCommonAIAgentComponent* Agent, const UCommonAIProfile* Profile);
    virtual void InitializeComponent() override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
    void Observe();
    void Pump();
    bool ActivateAgent();
    bool Rescore();
    void RefreshScoringHold();
    UCommonAITaskAdapter* GetRunningTask() const;
    UCommonAITaskAdapter* FindTask(FGameplayTag Activity) const;
    UCommonAIAgentComponent* GetAgent() const { return Agent.Get(); }
    const UCommonAIProfile* GetProfile() const { return RuntimeProfile; }
    FGuid GetInputGeneration() const { return InputGeneration; }
    void ResetInputs();
    void RestoreInputs(const TArray<FCommonAIConsiderationSnapshot>& Values);
    void InvalidateSource(FGameplayTag SourceId);
    bool ApplyInputs(UObject* Source, FGuid Generation, const TArray<FCommonAIInputUpdate>& Updates, bool bRescore);
    void EvaluateInputs(const FGameplayTagContainer& Inputs);
    bool EvaluateInput(UObject* Source, FGuid Generation, FGameplayTag Id, const FInstancedStruct& Context, bool bRescore);
    void NotifyEligibilityChanged();
    bool GetInput(FGameplayTag Id, FCommonAIConsiderationSnapshot& Value) const;
    bool IsInputAvailable(FGameplayTag Id) const;
    bool IsTaskEligible(FGameplayTag Activity) const;
    bool AreConditionsAvailable(const FCommonAIServiceDefinition& Definition) const;
    void ReconcileServices(bool bRefreshConditions = false);
    void CommitServiceTransitions();
    void CancelCurrent(FGameplayTag Reason);
    void HoldForTransition();
    bool AllowsUtilityReplacement() const;
    void OnExecutionEnded(FGuid Execution, const FCommonAIInteractionResult& Result);
    FGuid RequestCommand(FGameplayTag Activity, ECommonAITransitionMode Transition, ECommonAICommandRetention Retention, bool bUnlockRetry,
        const FInstancedStruct& Payload = FInstancedStruct());
    FInstancedStruct GetStartPayload(FGameplayTag Activity) const;
    void CancelCommands(FGameplayTag Reason);
    FCommonAICommandSnapshot GetCommand() const;
    TArray<FCommonAITaskSnapshot> TaskSnapshots() const;
    TArray<FCommonAIConsiderationSnapshot> InputSnapshots() const;
    TArray<FCommonAIServiceSnapshot> ServiceSnapshots() const;
    bool bInCallback = false;
    bool bInScoring = false;
    bool bPublishingInputs = false;
    bool bScoresDirty = true;
#if ENABLE_VISUAL_LOG
    virtual void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const override;
#endif
protected:
    virtual void CreateDataManager() override;
private:
    struct FInputState { ECommonAIValueStatus Status = ECommonAIValueStatus::Unavailable; };
    struct FProducerStamp { FGameplayTag Id; TWeakObjectPtr<UObject> Source; FGuid Revision; bool bExpectedAlive = false; };
    struct FQueuedInput
    {
        TWeakObjectPtr<UObject> Source;
        FGuid Generation;
        TArray<FCommonAIInputUpdate> Updates;
        TArray<FProducerStamp> Producers;
        bool bInternal = false;
        bool bRescore = true;
        bool bAutomatic = false;
    };
    struct FCommand
    {
        FCommonAICommandSnapshot Snapshot;
        ECommonAITransitionMode Transition = ECommonAITransitionMode::CancelAtSafePoint;
        ECommonAICommandRetention Retention = ECommonAICommandRetention::HoldUntilCompletion;
        bool bUnlockRetry = false;
    };
    bool ValidateUpdates(UObject* Source, FGuid Generation, const TArray<FCommonAIInputUpdate>& Updates, bool bInternal) const;
    void PublishUpdates(const TArray<FCommonAIInputUpdate>& Updates, bool bRescore, bool bAutomatic = false);
    void PumpCommand();
    void EndCommand(FCommand& Command, ECommonAICommandStatus Status, FGameplayTag Reason);
    UPROPERTY(Transient) TObjectPtr<UCommonAIProfile> RuntimeProfile;
    UPROPERTY(Transient) TMap<FGuid, FInstancedStruct> CommandPayloads;
    TWeakObjectPtr<UCommonAIAgentComponent> Agent;
    TMap<FGameplayTag, FInputState> InputStates;
    TArray<FQueuedInput> QueuedInputs;
    FGuid InputGeneration;
    FCommand PendingCommand;
    FCommand ActiveCommand;
    FCommonAICommandSnapshot LastCommand;
    bool bTransitionPending = false;
    bool bRescorePending = false;
    bool bServiceReconcilePending = false;
};

