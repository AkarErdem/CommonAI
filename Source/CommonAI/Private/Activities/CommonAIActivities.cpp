#include "Activities/CommonAIActivities.h"

#include "AIController.h"
#include "AthenaAIComponent.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

namespace
{
	constexpr float TargetValidationIntervalSeconds = 0.25f;
	uint64 NextWorkExecutionId = 0;
}

void UCommonAIWaitTask::Start(UAthenaAIDataManager* DataManager, AAIController* Controller)
{
	++RunId;
	bRunning = true;
	Super::Start(DataManager, Controller);
	if (DurationSeconds <= 0.0f || !GetWorld())
	{
		return;
	}
	const uint64 ExpectedRun = RunId;
	TWeakObjectPtr<UCommonAIWaitTask> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimer(WaitTimer, [WeakThis, ExpectedRun]()
	{
		if (WeakThis.IsValid() && WeakThis->bRunning && WeakThis->RunId == ExpectedRun)
		{
			WeakThis->FinishTask();
		}
	}, DurationSeconds, false);
}

void UCommonAIWaitTask::Stop(UAthenaAIDataManager* DataManager, AAIController* Controller, EAAITaskResult Result)
{
	bRunning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaitTimer);
	}
	Super::Stop(DataManager, Controller, Result);
}

void UCommonAIAction::StartAction(UAthenaAIDataManager* DataManager, AAIController* Controller)
{
	++StepEpoch;
	bStepActive = true;
	bCompletionQueued = false;
	Super::StartAction(DataManager, Controller);
	if (bStepActive)
	{
		ExecuteStep(Controller);
	}
}

void UCommonAIAction::StopAction()
{
	if (!bStepActive)
	{
		return;
	}
	bStepActive = false;
	bCompletionQueued = false;
	Super::StopAction();
}

bool UCommonAIAction::IsStepCurrent(uint64 ExpectedEpoch) const
{
	return bStepActive && StepEpoch == ExpectedEpoch;
}

void UCommonAIAction::CompleteStep(bool bSucceeded)
{
	if (!bStepActive || bCompletionQueued || !GetWorld())
	{
		return;
	}
	bCompletionQueued = true;
	const uint64 ExpectedEpoch = StepEpoch;
	TWeakObjectPtr<UCommonAIAction> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis, ExpectedEpoch, bSucceeded]()
	{
		if (WeakThis.IsValid() && WeakThis->IsStepCurrent(ExpectedEpoch))
		{
			WeakThis->FinishAction(!bSucceeded);
		}
	});
}

UCommonAIMoveAction::UCommonAIMoveAction()
{
	bEnableTick = true;
	TickInterval = TargetValidationIntervalSeconds;
}

void UCommonAIMoveAction::ExecuteStep(AAIController* Controller)
{
	AActor* Target = MoveTarget.Get();
	UPathFollowingComponent* Path = Controller ? Controller->GetPathFollowingComponent() : nullptr;
	if (!IsValid(Target) || (bHasGuardObject && !GuardObject.IsValid()) || !Path || !Controller->GetPawn())
	{
		CompleteStep(false);
		return;
	}
	PathFollowing = Path;
	MoveDelegateHandle = Path->OnRequestFinished.AddUObject(this, &UCommonAIMoveAction::OnMoveFinished);
	FAIMoveRequest Request(Target);
	Request.SetAcceptanceRadius(AcceptanceRadius);
	const FPathFollowingRequestResult Result = Controller->MoveTo(Request);
	if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		CompleteStep(true);
	}
	else if (Result.Code == EPathFollowingRequestResult::Failed)
	{
		CompleteStep(false);
	}
	else
	{
		MoveRequestId = Result.MoveId;
	}
}

void UCommonAIMoveAction::OnMoveFinished(FAIRequestID FinishedRequest, const FPathFollowingResult& Result)
{
	if (MoveRequestId == FinishedRequest && MoveRequestId.IsValid())
	{
		CompleteStep(IsValid(MoveTarget.Get()) && (!bHasGuardObject || GuardObject.IsValid()) && Result.IsSuccess());
	}
}

void UCommonAIMoveAction::ActionTick()
{
	if (!IsValid(MoveTarget.Get()) || (bHasGuardObject && !GuardObject.IsValid()))
	{
		CompleteStep(false);
	}
}

void UCommonAIMoveAction::StopAction()
{
	if (UPathFollowingComponent* Path = PathFollowing.Get())
	{
		Path->OnRequestFinished.Remove(MoveDelegateHandle);
		if (MoveRequestId.IsValid() && Path->GetCurrentRequestId() == MoveRequestId)
		{
			Path->AbortMove(*this, FPathFollowingResultFlags::UserAbort, MoveRequestId);
		}
	}
	MoveRequestId = FAIRequestID::InvalidRequest;
	PathFollowing.Reset();
	MoveTarget.Reset();
	GuardObject.Reset();
	bHasGuardObject = false;
	Super::StopAction();
}

UCommonAIWorkTask::UCommonAIWorkTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Actions.Add(CreateDefaultSubobject<UCommonAIFindWorkAction>(TEXT("FindWork")));
	Actions.Add(CreateDefaultSubobject<UCommonAIReserveWorkAction>(TEXT("ReserveWork")));
	Actions.Add(CreateDefaultSubobject<UCommonAIMoveAction>(TEXT("MoveToWork")));
	Actions.Add(CreateDefaultSubobject<UCommonAIPerformWorkAction>(TEXT("PerformWork")));
	Actions.Add(CreateDefaultSubobject<UCommonAIReleaseWorkAction>(TEXT("ReleaseWork")));
}

void UCommonAIWorkTask::Start(UAthenaAIDataManager* DataManager, AAIController* Controller)
{
	if (bHasReservation && Site.IsValid())
	{
		Site->Release(RunController.Get(), RunId);
	}
	RunId = ++NextWorkExecutionId;
	bRunning = true;
	bHasReservation = false;
	bPerformed = false;
	bReleased = false;
	bSequenceFinished = false;
	Site.Reset();
	RunController = Controller;
	Provider = Controller ? Controller->FindComponentByClass<UCommonAIWorkProviderComponent>() : nullptr;
	if (Actions.Num() != 5 || !Actions[0] || !Actions[0]->IsA<UCommonAIFindWorkAction>()
		|| !Actions[1] || !Actions[1]->IsA<UCommonAIReserveWorkAction>()
		|| !Actions[2] || !Actions[2]->IsA<UCommonAIMoveAction>()
		|| !Actions[3] || !Actions[3]->IsA<UCommonAIPerformWorkAction>()
		|| !Actions[4] || !Actions[4]->IsA<UCommonAIReleaseWorkAction>())
	{
		Stop(DataManager, Controller, EAAITaskResult::Failed);
		return;
	}
	Super::Start(DataManager, Controller);
}

void UCommonAIWorkTask::SetSite(UCommonAIWorkSite* NewSite)
{
	Site = NewSite;
	for (UAthenaAIAction* Action : Actions)
	{
		if (UCommonAIMoveAction* Move = Cast<UCommonAIMoveAction>(Action))
		{
			Move->SetTarget(IsValid(NewSite) ? NewSite->GetOwner() : nullptr);
			Move->SetGuardObject(NewSite);
		}
	}
}

void UCommonAIWorkTask::Stop(UAthenaAIDataManager* DataManager, AAIController* Controller, EAAITaskResult Result)
{
	if (!bRunning)
	{
		return;
	}
	bRunning = false;
	const uint64 EndedRun = RunId;
	const ECommonAIActivityOutcome Outcome = bPerformed && bReleased && bSequenceFinished && Result == EAAITaskResult::Successful
		? ECommonAIActivityOutcome::Completed
		: (Result == EAAITaskResult::Replaced || (Result == EAAITaskResult::Successful && !bReleased)
			? ECommonAIActivityOutcome::Cancelled : ECommonAIActivityOutcome::Failed);
	if (bHasReservation && Site.IsValid())
	{
		Site->Release(RunController.Get(), RunId);
	}
	Site.Reset();
	bHasReservation = false;
	Super::Stop(DataManager, Controller, Result);
	OnWorkEnded.Broadcast(EndedRun, Outcome);
}

void UCommonAIWorkTask::FinishTask()
{
	bSequenceFinished = bPerformed && bReleased;
	Super::FinishTask();
}

void UCommonAIWorkTask::Cleanup()
{
	if (bHasReservation && Site.IsValid())
	{
		Site->Release(RunController.Get(), RunId);
	}
	Site.Reset();
	bHasReservation = false;
	bRunning = false;
	Super::Cleanup();
}

void UCommonAIFindWorkAction::ExecuteStep(AAIController* Controller)
{
	UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask);
	FCommonAIWorkOpportunity Opportunity;
	const FCommonAIExecutionContext Context { Controller, Task ? Task->GetRunId() : 0 };
	const bool bFound = Task && Task->GetProvider() && Task->GetProvider()->FindOpportunity(Context, Opportunity)
		&& IsValid(Opportunity.Site);
	if (bFound)
	{
		Task->SetSite(Opportunity.Site);
	}
	CompleteStep(bFound);
}

void UCommonAIReserveWorkAction::ExecuteStep(AAIController* Controller)
{
	UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask);
	const bool bReserved = Task && Task->GetSite() && Task->GetSite()->TryReserve(Controller, Task->GetRunId());
	if (bReserved)
	{
		Task->MarkReserved();
	}
	CompleteStep(bReserved);
}

UCommonAIPerformWorkAction::UCommonAIPerformWorkAction()
{
	bEnableTick = true;
	TickInterval = TargetValidationIntervalSeconds;
}

void UCommonAIPerformWorkAction::ExecuteStep(AAIController* Controller)
{
	UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask);
	UCommonAIWorkSite* Site = Task ? Task->GetSite() : nullptr;
	if (!IsValid(Site) || !Task || !Site->IsReservedBy(Controller, Task->GetRunId()))
	{
		CompleteStep(false);
		return;
	}
	const uint64 ExpectedEpoch = GetStepEpoch();
	TWeakObjectPtr<UCommonAIPerformWorkAction> WeakThis(this);
	const bool bAccepted = Site->BeginWork(Controller, Task->GetRunId(), [WeakThis, ExpectedEpoch](bool bSucceeded)
	{
		if (WeakThis.IsValid() && WeakThis->IsStepCurrent(ExpectedEpoch))
		{
			if (bSucceeded)
			{
				if (UCommonAIWorkTask* CurrentTask = Cast<UCommonAIWorkTask>(WeakThis->OwnerTask))
				{
					CurrentTask->MarkPerformed();
				}
			}
			WeakThis->CompleteStep(bSucceeded);
		}
	});
	if (!bAccepted)
	{
		CompleteStep(false);
	}
}

void UCommonAIPerformWorkAction::ActionTick()
{
	const UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask);
	if (!Task || !IsValid(Task->GetSite()) || !Task->GetSite()->IsReservedBy(Task->GetRunController(), Task->GetRunId()))
	{
		CompleteStep(false);
	}
}

void UCommonAIReleaseWorkAction::ExecuteStep(AAIController* Controller)
{
	UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask);
	const bool bCanRelease = Task && IsValid(Task->GetSite())
		&& Task->GetSite()->IsReservedBy(Controller, Task->GetRunId());
	if (bCanRelease)
	{
		Task->GetSite()->Release(Controller, Task->GetRunId());
		Task->MarkReleased();
	}
	CompleteStep(bCanRelease);
}
