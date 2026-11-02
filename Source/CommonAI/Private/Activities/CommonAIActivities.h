#pragma once

#include "CoreMinimal.h"
#include "AthenaAIAction.h"
#include "AthenaAITask.h"
#include "AthenaAITask_Sequence.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "AITypes.h"
#include "CommonAIAgentComponent.h"
#include "CommonAIActivities.generated.h"

class UPathFollowingComponent;
struct FPathFollowingResult;

// Duration zero makes this an interruptible idle fallback without a finish/rescore loop.
UCLASS(EditInlineNew, Blueprintable)
class COMMONAI_API UCommonAIWaitTask : public UAthenaAITask
{
	GENERATED_BODY()

public:
	UCommonAIWaitTask(const FObjectInitializer& ObjectInitializer);
	virtual void Start(UAthenaAIDataManager* DataManager, AAIController* Controller) override;
	virtual void Stop(UAthenaAIDataManager* DataManager, AAIController* Controller, EAAITaskResult Result) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Common AI|Wait", meta = (ClampMin = "0.0", Units = "Seconds"))
	float DurationSeconds = 0.0f;

private:
	FTimerHandle WaitTimer;
	uint64 RunId = 0;
	bool bRunning = false;
};

// Common async action policy: one completion per start, deferred to avoid vendor sequence reentrancy.
UCLASS(Abstract, EditInlineNew, Blueprintable)
class COMMONAI_API UCommonAIAction : public UAthenaAIAction
{
	GENERATED_BODY()

public:
	virtual void StartAction(UAthenaAIDataManager* DataManager, AAIController* Controller) override;
	virtual void StopAction() override;

protected:
	virtual void ExecuteStep(AAIController* Controller) PURE_VIRTUAL(UCommonAIAction::ExecuteStep, );
	void CompleteStep(bool bSucceeded);
	bool IsStepCurrent(uint64 ExpectedEpoch) const;
	uint64 GetStepEpoch() const { return StepEpoch; }

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

protected:
	virtual void ExecuteStep(AAIController* Controller) override;
	virtual void ActionTick() override;

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

// The vendor sequence owns action order; this task owns the reservation and semantic result.
UCLASS(EditInlineNew, Blueprintable)
class COMMONAI_API UCommonAIWorkTask : public UAthenaAITask_Sequence
{
	GENERATED_BODY()

public:
	UCommonAIWorkTask(const FObjectInitializer& ObjectInitializer);
	virtual void Start(UAthenaAIDataManager* DataManager, AAIController* Controller) override;
	virtual void Stop(UAthenaAIDataManager* DataManager, AAIController* Controller, EAAITaskResult Result) override;
	virtual void Cleanup() override;

	void Configure(const UCommonAIProfile* Profile);
	void RequestCancellation(FGameplayTag Reason);
	bool IsRunning() const { return bRunning; }
	bool IsCancelling() const { return bCancelling; }
	const FCommonAIInteractionRequest& GetRequest() const { return Request; }
	AAIController* GetRunController() const { return RunController.Get(); }
	UObject* GetSite() const { return Site.Get(); }
	UCommonAIWorkProviderComponent* GetProvider() const { return Provider.Get(); }
	UCommonAIAgentComponent* GetAgent() const { return Agent.Get(); }
	void SetSite(UObject* NewSite);
	void MarkReserved() { bHasReservation = true; }
	void MarkPerformed() { bPerformed = true; }
	void MarkReleased() { bReleased = true; }
	void SetInteractionStarted() { bInteractionStarted = true; }
	void SetFailureReason(FGameplayTag Reason) { EndReason = Reason; }

protected:
	virtual void FinishTask() override;
	virtual void TaskTick() override;

private:
	TWeakObjectPtr<AAIController> RunController;
	TWeakObjectPtr<UCommonAIWorkProviderComponent> Provider;
	TWeakObjectPtr<UObject> Site;
	TWeakObjectPtr<UCommonAIAgentComponent> Agent;
	FCommonAIInteractionRequest Request;
	FGuid BusyContribution;
	FGameplayTag EndReason;
	bool bCancelling = false;
	bool bInteractionStarted = false;
	bool bRunning = false;
	bool bHasReservation = false;
	bool bPerformed = false;
	bool bReleased = false;
	bool bSequenceFinished = false;
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
	virtual void ActionTick() override;
};

UCLASS(EditInlineNew)
class COMMONAI_API UCommonAIReleaseWorkAction : public UCommonAIAction
{
	GENERATED_BODY()
protected:
	virtual void ExecuteStep(AAIController* Controller) override;
};
