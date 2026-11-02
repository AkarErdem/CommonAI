#include "Activities/CommonAIActivities.h"

#include "AIController.h"
#include "CommonAIProfile.h"
#include "CommonAITags.h"
#include "Contracts/CommonAIMovement.h"
#include "AthenaAIComponent.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

namespace
{
	constexpr float TargetValidationIntervalSeconds = 0.25f;
	
}

UCommonAIWaitTask::UCommonAIWaitTask(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    TaskTag = CommonAITags::Activity_Wait;
    ReplaceResistance = 0.0f;
    MaxMomentum = 0;
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
			if (const UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(WeakThis->OwnerTask))
				if (Task->IsCancelling()) return;
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
	const UCommonAIWorkTask* Work = Cast<UCommonAIWorkTask>(OwnerTask);
	UCommonAIAgentComponent* Agent = Work ? Work->GetAgent() : nullptr;
	if (Agent && Agent->GetMovementAdapter() && IsValid(Target) && (!bHasGuardObject || GuardObject.IsValid()))
	{
		MovementAdapter = Agent->GetMovementAdapter();
		AdapterRequest = Work->GetRequest();
		bUsingAdapter = true;
		const ECommonAIMoveStatus Status = ICommonAIMovement::Execute_BeginMove(MovementAdapter.Get(), AdapterRequest, Target, AcceptanceRadius);
		if (Status != ECommonAIMoveStatus::Running) CompleteMove(Status == ECommonAIMoveStatus::Completed);
		return;
	}
	UPathFollowingComponent* Path = Controller ? Controller->GetPathFollowingComponent() : nullptr;
	if (!IsValid(Target) || (bHasGuardObject && !GuardObject.IsValid()) || !Path || !Controller->GetPawn())
	{
		CompleteMove(false);
		return;
	}
	PathFollowing = Path;
	MoveDelegateHandle = Path->OnRequestFinished.AddUObject(this, &UCommonAIMoveAction::OnMoveFinished);
	FAIMoveRequest Request(Target);
	Request.SetAcceptanceRadius(AcceptanceRadius);
	const FPathFollowingRequestResult Result = Controller->MoveTo(Request);
	if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		CompleteMove(true);
	}
	else if (Result.Code == EPathFollowingRequestResult::Failed)
	{
		CompleteMove(false);
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
		CompleteMove(IsValid(MoveTarget.Get()) && (!bHasGuardObject || GuardObject.IsValid()) && Result.IsSuccess());
	}
}

void UCommonAIMoveAction::ActionTick()
{
	if (bUsingAdapter)
	{
		if (!MovementAdapter.IsValid()) { CompleteMove(false); return; }
		const ECommonAIMoveStatus Status = ICommonAIMovement::Execute_GetMoveStatus(MovementAdapter.Get(), AdapterRequest);
		if (Status != ECommonAIMoveStatus::Running) CompleteMove(Status == ECommonAIMoveStatus::Completed);
	}
	if (!IsValid(MoveTarget.Get()) || (bHasGuardObject && !GuardObject.IsValid()))
	{
		CompleteMove(false);
	}
}

void UCommonAIMoveAction::StopAction()
{
	if (bUsingAdapter && MovementAdapter.IsValid()) ICommonAIMovement::Execute_CancelMove(MovementAdapter.Get(), AdapterRequest);
	MovementAdapter.Reset();
	bUsingAdapter = false;
	AdapterRequest = {};
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

void UCommonAIMoveAction::CompleteMove(bool bSucceeded)
{
    if (!bSucceeded)
        if (UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask))
            Task->SetFailureReason(MoveTarget.IsValid() ? CommonAITags::Reason_MovementFailed : CommonAITags::Reason_TargetLost);
    CompleteStep(bSucceeded);
}
UCommonAIWorkTask::UCommonAIWorkTask(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    Actions.Add(CreateDefaultSubobject<UCommonAIFindWorkAction>(TEXT("FindWork")));
    Actions.Add(CreateDefaultSubobject<UCommonAIReserveWorkAction>(TEXT("ReserveWork")));
    Actions.Add(CreateDefaultSubobject<UCommonAIMoveAction>(TEXT("MoveToWork")));
    Actions.Add(CreateDefaultSubobject<UCommonAIPerformWorkAction>(TEXT("PerformWork")));
    Actions.Add(CreateDefaultSubobject<UCommonAIReleaseWorkAction>(TEXT("ReleaseWork")));
    TaskTag = CommonAITags::Activity_Work;
    Considerations.AddTag(CommonAITags::Consideration_WorkAvailable);
    bInterruptible = false;
    bEnableTick = true;
    TickInterval = TargetValidationIntervalSeconds;
    MaxMomentum = 0;
}

void UCommonAIWorkTask::Configure(const UCommonAIProfile* Profile)
{
    TaskTag = Profile->WorkActivity;
    ScoreModifier = Profile->WorkScoreModifier;
    TickInterval = Profile->ObservationIntervalSeconds;
    for (UAthenaAIAction* Action : Actions)
        if (UCommonAIMoveAction* Move = Cast<UCommonAIMoveAction>(Action))
            Move->SetAcceptanceRadius(Profile->AcceptanceRadiusCentimeters);
}

void UCommonAIWorkTask::Start(UAthenaAIDataManager* DataManager, AAIController* Controller)
{
    // The backend never starts another run until this run has reached its safe terminal point.
    if (bRunning) return;
    bRunning = true;
    bHasReservation = bPerformed = bReleased = bSequenceFinished = bCancelling = bInteractionStarted = false;
    EndReason = {};
    Site.Reset();
    RunController = Controller;
    Agent = Controller ? Controller->FindComponentByClass<UCommonAIAgentComponent>() : nullptr;
    Provider = Agent.IsValid() ? Agent->GetProvider() : nullptr;
    Request.Execution = FGuid::NewGuid();
    Request.Agent = Controller;
    Request.Activity = TaskTag;
    if (Agent.IsValid())
    {
        BusyContribution = Agent->AddTagContribution(this, FGameplayTagContainer(CommonAITags::State_Busy_Working));
        Agent->SetExecutionSnapshot(ECommonAIPhase::Approaching, Request, nullptr);
    }
    Super::Start(DataManager, Controller);
}

void UCommonAIWorkTask::SetSite(UObject* NewSite)
{
    Site = NewSite;
    const FCommonAIInteractionOffer Offer = IsValid(NewSite)
        ? ICommonAIInteraction::Execute_QueryInteraction(NewSite, Request) : FCommonAIInteractionOffer{};
    for (UAthenaAIAction* Action : Actions)
        if (UCommonAIMoveAction* Move = Cast<UCommonAIMoveAction>(Action))
        {
            Move->SetTarget(Offer.ApproachActor);
            Move->SetGuardObject(NewSite);
        }
    if (Agent.IsValid()) Agent->SetExecutionSnapshot(ECommonAIPhase::Approaching, Request, NewSite);
}

void UCommonAIWorkTask::RequestCancellation(FGameplayTag Reason)
{
    if (!bRunning || bCancelling) return;
    bCancelling = true;
    EndReason = Reason;
    for (UAthenaAIAction* Action : Actions)
        if (UCommonAIMoveAction* Move = Cast<UCommonAIMoveAction>(Action)) Move->StopAction();
    if (Agent.IsValid()) Agent->SetExecutionSnapshot(ECommonAIPhase::Cancelling, Request, Site.Get(), Reason);
    if (bHasReservation && Site.IsValid())
        ICommonAIInteraction::Execute_RequestInteractionCancellation(Site.Get(), Request, Reason);
    // Do not stop from inside a scoring callback. The next task tick drains or waits.
}

void UCommonAIWorkTask::TaskTick()
{
    if (!bRunning) return;
    if (bCancelling)
    {
        FCommonAIInteractionResult Current;
        if (bHasReservation && Site.IsValid())
            Current = ICommonAIInteraction::Execute_GetInteractionResult(Site.Get(), Request);
        if (!bHasReservation || !Site.IsValid() || Current.IsTerminal())
        {
            const bool bFailed = bHasReservation && (!Site.IsValid()
                || Current.Status == ECommonAIInteractionStatus::Failed || Current.Status == ECommonAIInteractionStatus::Rejected);
            if (bFailed) EndReason = Site.IsValid() ? Current.Reason : CommonAITags::Reason_TargetLost;
            Stop(OwnerAthenaAIComp->GetDataManager(), RunController.Get(),
                bFailed ? EAAITaskResult::Aborted : EAAITaskResult::Successful);
        }
    }
    Super::TaskTick();
}

void UCommonAIWorkTask::Stop(UAthenaAIDataManager* DataManager, AAIController* Controller, EAAITaskResult Result)
{
    if (!bRunning) return;
    bRunning = false;
    FCommonAIInteractionResult Outcome;
    Outcome.Status = Result == EAAITaskResult::Successful && bCancelling ? ECommonAIInteractionStatus::Cancelled
        : Result == EAAITaskResult::Successful && bPerformed && bReleased && bSequenceFinished
        ? ECommonAIInteractionStatus::Completed : ECommonAIInteractionStatus::Failed;
    Outcome.Reason = EndReason;
    if (bHasReservation && Site.IsValid())
    {
        ICommonAIInteraction::Execute_RequestInteractionCancellation(Site.Get(), Request, EndReason);
        const FCommonAIInteractionResult Current = ICommonAIInteraction::Execute_GetInteractionResult(Site.Get(), Request);
        ICommonAIInteraction::Execute_ReleaseInteraction(Site.Get(), Request, !Current.IsTerminal());
    }
    Site.Reset();
    bHasReservation = false;
    if (Agent.IsValid())
    {
        Agent->RemoveTagContribution(this, BusyContribution);
        Agent->RecordExecutionResult(Outcome);
    }
    Super::Stop(DataManager, Controller, Result);
}

void UCommonAIWorkTask::FinishTask()
{
    bSequenceFinished = bPerformed && bReleased;
    Super::FinishTask();
}

void UCommonAIWorkTask::Cleanup()
{
    if (bRunning) Stop(OwnerAthenaAIComp->GetDataManager(), RunController.Get(), EAAITaskResult::Aborted);
    Super::Cleanup();
}

void UCommonAIFindWorkAction::ExecuteStep(AAIController* Controller)
{
    UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask);
    const UCommonAIAgentComponent* Agent = Task ? Task->GetAgent() : nullptr;
    UObject* Target = Task && Task->GetProvider() && Agent && Agent->Profile
        ? Task->GetProvider()->FindOpportunity(Task->GetRequest(), Agent->Profile->RequiredTargetCapabilities) : nullptr;
    if (Target) Task->SetSite(Target);
    else if (Task) Task->SetFailureReason(CommonAITags::Reason_NoOpportunity);
    CompleteStep(Target != nullptr);
}

void UCommonAIReserveWorkAction::ExecuteStep(AAIController* Controller)
{
    UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask);
    const bool bReserved = Task && IsValid(Task->GetSite()) && !Task->IsCancelling()
        && ICommonAIInteraction::Execute_ReserveInteraction(Task->GetSite(), Task->GetRequest());
    if (bReserved) Task->MarkReserved();
    else if (Task) Task->SetFailureReason(CommonAITags::Reason_ReservationFailed);
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
    if (!Task || !IsValid(Task->GetSite())) { CompleteStep(false); return; }
    if (Task->IsCancelling()) return;
    Task->SetInteractionStarted();
    if (UCommonAIAgentComponent* Agent = Task->GetAgent())
        Agent->SetExecutionSnapshot(ECommonAIPhase::Interacting, Task->GetRequest(), Task->GetSite());
    const FCommonAIInteractionResult Started = ICommonAIInteraction::Execute_BeginInteraction(Task->GetSite(), Task->GetRequest());
    if (Started.Status == ECommonAIInteractionStatus::Rejected || Started.Status == ECommonAIInteractionStatus::Failed)
    {
        Task->SetFailureReason(Started.Reason);
        CompleteStep(false);
        return;
    }
    ActionTick();
}

void UCommonAIPerformWorkAction::ActionTick()
{
    UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask);
    if (!Task || !IsValid(Task->GetSite()))
    {
        if (Task) Task->SetFailureReason(CommonAITags::Reason_TargetLost);
        CompleteStep(false);
        return;
    }
    if (Task->IsCancelling()) return;
    const FCommonAIInteractionResult Result = ICommonAIInteraction::Execute_GetInteractionResult(Task->GetSite(), Task->GetRequest());
    if (Result.IsTerminal())
    {
        const bool bSucceeded = Result.Status == ECommonAIInteractionStatus::Completed;
        if (bSucceeded) Task->MarkPerformed();
        else Task->SetFailureReason(Result.Reason);
        CompleteStep(bSucceeded);
    }
}

void UCommonAIReleaseWorkAction::ExecuteStep(AAIController* Controller)
{
    UCommonAIWorkTask* Task = Cast<UCommonAIWorkTask>(OwnerTask);
    const bool bCanRelease = Task && IsValid(Task->GetSite());
    if (bCanRelease)
    {
        ICommonAIInteraction::Execute_ReleaseInteraction(Task->GetSite(), Task->GetRequest(), false);
        Task->MarkReleased();
    }
    CompleteStep(bCanRelease);
}

