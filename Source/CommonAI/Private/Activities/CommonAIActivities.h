#pragma once

#include "CoreMinimal.h"
#include "AthenaAIAction.h"
#include "AthenaAITask.h"
#include "AthenaAITask_Sequence.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "AITypes.h"
#include "CommonAIAgentComponent.h"
#include "Definitions/CommonAITaskDefinition.h"
#include "Execution/CommonAIExecution.h"
#include "CommonAIActivities.generated.h"

class UPathFollowingComponent;
struct FPathFollowingResult;

// Common async action policy: one completion per start, deferred to avoid vendor sequence reentrancy.
UCLASS(Abstract, EditInlineNew, Blueprintable)
class COMMONAI_API UCommonAIAction : public UAthenaAIAction
{
	GENERATED_BODY()

public:
	virtual void StartAction(UAthenaAIDataManager* DataManager, AAIController* Controller) override;
	virtual void StopAction() override;
	virtual void RequestSafeCancellation(FGameplayTag Reason) {}
	virtual bool HasReachedSafeEnd() const { return true; }
	void SetActionIdentity(FGameplayTag Id) { ActionTag = Id; }
	void SetTickCadence(float Seconds) { TickInterval = Seconds; }

protected:
	virtual void ExecuteStep(AAIController* Controller) PURE_VIRTUAL(UCommonAIAction::ExecuteStep, );
	virtual void ActionTick() override final;
	virtual void TickStep() {}
	void CompleteStep(bool bSucceeded);
	bool IsStepCurrent(uint64 ExpectedEpoch) const;

private:
	uint64 StepEpoch = 0;
	bool bStepActive = false;
	bool bCompletionQueued = false;
};

// Generic Move action. The owning sequence supplies a target when its opportunity is found.
UCLASS(EditInlineNew, Blueprintable)
class COMMONAI_API UCommonAIMoveAction : public UCommonAIAction
{
	GENERATED_BODY()

public:
	UCommonAIMoveAction();
	void SetTarget(AActor* Target) { MoveTarget = Target; }
	void SetGuardObject(UObject* Object) { GuardObject = Object; bHasGuardObject = true; }
	void SetAcceptanceRadius(float Radius) { AcceptanceRadius = Radius; }
	virtual void StopAction() override;
	virtual void RequestSafeCancellation(FGameplayTag Reason) override { StopAction(); }

protected:
	virtual void ExecuteStep(AAIController* Controller) override;
	virtual void TickStep() override;

	UPROPERTY(EditDefaultsOnly, Category = "Common AI|Move", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float AcceptanceRadius = 50.0f;

private:
	void OnMoveFinished(FAIRequestID FinishedRequest, const FPathFollowingResult& Result);
	void CompleteMove(bool bSucceeded);
	TWeakObjectPtr<AActor> MoveTarget;
	TWeakObjectPtr<UObject> GuardObject;
	bool bHasGuardObject = false;
	TWeakObjectPtr<UPathFollowingComponent> PathFollowing;
	FDelegateHandle MoveDelegateHandle;
	FAIRequestID MoveRequestId = FAIRequestID::InvalidRequest;
	TWeakObjectPtr<UObject> MovementAdapter;
	FCommonAIInteractionRequest AdapterRequest;
	bool bUsingAdapter = false;
};

// Vendor task/sequence lifecycle with one resource-safe execution contract for every definition.
UCLASS(EditInlineNew, NotBlueprintable)
class COMMONAI_API UCommonAITaskAdapter : public UAthenaAITask_Sequence
{
	GENERATED_BODY()

public:
	UCommonAITaskAdapter(const FObjectInitializer& ObjectInitializer);
	virtual void Start(UAthenaAIDataManager* DataManager, AAIController* Controller) override;
	virtual void Stop(UAthenaAIDataManager* DataManager, AAIController* Controller, EAAITaskResult Result) override;
	virtual void Cleanup() override;

	void Configure(const FCommonAITaskDefinition& InDefinition, bool bInFallback = false);
	const FCommonAITaskDefinition& GetDefinition() const { return Definition; }
	bool IsFallback() const { return bFallback; }
	void SetUtilityInterruptible(bool bValue) { Definition.bUtilityInterruptible = bValue; }
	void SetMomentumPolicy(int32 Maximum, int32 Reduction, float DecaySeconds)
	{
		MaxMomentum = static_cast<uint8>(Maximum);
		MomentumScoreReduction = static_cast<uint8>(Reduction);
		MomentumDecreaseTime = DecaySeconds;
	}
	void RequestCancellation(FGameplayTag Reason);
	bool TryFinishCancellation();
	bool IsRunning() const { return bRunning; }
	bool IsCancelling() const { return bCancelling; }
	const FCommonAIInteractionRequest& GetRequest() const { return Request; }
	AAIController* GetRunController() const { return RunController.Get(); }
	UObject* GetSite() const { return Site.Get(); }
	UCommonAIWorkProviderComponent* GetProvider() const { return Provider.Get(); }
	UCommonAIAgentComponent* GetAgent() const { return Agent.Get(); }
	bool SetSite(UObject* NewSite);
	bool PrepareReservation()
	{
		if (bHasReservation) return false;
		Request.Operation = FGuid::NewGuid();
		return true;
	}
	void MarkReserved();
	void MarkReleased() { bHasReservation = false; }
	void SetFailureReason(FGameplayTag Reason) { EndReason = Reason; }
	FCommonAIExecutionContext MakeExecutionContext(FGameplayTag Action, FGuid Entry) const;
	bool SignalExecutionComplete(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason);
	UCommonAIExecution* GetActiveExecution() const;

protected:
	virtual void FinishTask() override;
	virtual void TaskTick() override;

private:
	UPROPERTY() FCommonAITaskDefinition Definition;
	UCommonAIAction* CurrentCommonAction() const;
	TWeakObjectPtr<AAIController> RunController;
	TWeakObjectPtr<UCommonAIWorkProviderComponent> Provider;
	TWeakObjectPtr<UObject> Site;
	TWeakObjectPtr<UCommonAIAgentComponent> Agent;
	UPROPERTY(Transient) FCommonAIInteractionRequest Request;
	FGuid BusyContribution;
	FGameplayTag EndReason;
	bool bCancelling = false;
	UPROPERTY() bool bFallback = false;
	bool bRunning = false;
	bool bHasReservation = false;
	bool bSequenceFinished = false;
};

UCLASS(EditInlineNew, NotBlueprintable)
class UCommonAIConfiguredAction : public UCommonAIAction
{
	GENERATED_BODY()
public:
	void Configure(const FCommonAIActionDefinition& InDefinition, float IntervalSeconds);
	virtual void StopAction() override;
	virtual void RequestSafeCancellation(FGameplayTag Reason) override;
	virtual bool HasReachedSafeEnd() const override;
	bool SignalComplete(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason);
	UCommonAIExecution* GetExecution() const { return Execution; }
protected:
	virtual void ExecuteStep(AAIController* Controller) override;
	virtual void TickStep() override;
private:
	UPROPERTY() FCommonAIActionDefinition Definition;
	UPROPERTY(Instanced) TObjectPtr<UCommonAIExecution> Execution;
	void HandleFinished(FGuid Invocation, const FCommonAIInteractionResult& Result);
	FDelegateHandle CompletionHandle;
	double LastTickTime = 0.0;
};

UCLASS(EditInlineNew)
class COMMONAI_API UCommonAIFindWorkAction : public UCommonAIAction
{
	GENERATED_BODY()
protected:
	virtual void ExecuteStep(AAIController* Controller) override;
};

UCLASS(EditInlineNew)
class COMMONAI_API UCommonAIReserveWorkAction : public UCommonAIAction
{
	GENERATED_BODY()
protected:
	virtual void ExecuteStep(AAIController* Controller) override;
};

UCLASS(EditInlineNew)
class COMMONAI_API UCommonAIPerformWorkAction : public UCommonAIAction
{
	GENERATED_BODY()
public:
	UCommonAIPerformWorkAction();
protected:
	virtual void ExecuteStep(AAIController* Controller) override;
	virtual void TickStep() override;
};

UCLASS(EditInlineNew)
class COMMONAI_API UCommonAIReleaseWorkAction : public UCommonAIAction
{
	GENERATED_BODY()
protected:
	virtual void ExecuteStep(AAIController* Controller) override;
};


