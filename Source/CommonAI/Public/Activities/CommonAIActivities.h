#pragma once

#include "CoreMinimal.h"
#include "AthenaAIAction.h"
#include "AthenaAITask.h"
#include "AthenaAITask_Sequence.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "AITypes.h"
#include "CommonAIActivities.generated.h"

class UPathFollowingComponent;
struct FPathFollowingResult;

UENUM(BlueprintType)
enum class ECommonAIActivityOutcome : uint8
{
	Completed,
	Failed,
	Cancelled
};

// Duration zero makes this an interruptible idle fallback without a finish/rescore loop.
UCLASS(EditInlineNew, Blueprintable)
class COMMONAI_API UCommonAIWaitTask : public UAthenaAITask
{
	GENERATED_BODY()

public:
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
	virtual void StopAction() override;

protected:
	virtual void ExecuteStep(AAIController* Controller) override;
	virtual void ActionTick() override;

	UPROPERTY(EditDefaultsOnly, Category = "Common AI|Move", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float AcceptanceRadius = 50.0f;

private:
	void OnMoveFinished(FAIRequestID FinishedRequest, const FPathFollowingResult& Result);
	TWeakObjectPtr<AActor> MoveTarget;
	TWeakObjectPtr<UObject> GuardObject;
	bool bHasGuardObject = false;
	TWeakObjectPtr<UPathFollowingComponent> PathFollowing;
	FDelegateHandle MoveDelegateHandle;
	FAIRequestID MoveRequestId = FAIRequestID::InvalidRequest;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FCommonAIWorkEnded, uint64 /* RunId */, ECommonAIActivityOutcome /* Outcome */);

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

	uint64 GetRunId() const { return RunId; }
	AAIController* GetRunController() const { return RunController.Get(); }
	UCommonAIWorkSite* GetSite() const { return Site.Get(); }
	UCommonAIWorkProviderComponent* GetProvider() const { return Provider.Get(); }
	void SetSite(UCommonAIWorkSite* NewSite);
	void MarkReserved() { bHasReservation = true; }
	void MarkPerformed() { bPerformed = true; }
	void MarkReleased() { bReleased = true; }
	FCommonAIWorkEnded OnWorkEnded;

protected:
	virtual void FinishTask() override;

private:
	TWeakObjectPtr<AAIController> RunController;
	TWeakObjectPtr<UCommonAIWorkProviderComponent> Provider;
	TWeakObjectPtr<UCommonAIWorkSite> Site;
	uint64 RunId = 0;
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
