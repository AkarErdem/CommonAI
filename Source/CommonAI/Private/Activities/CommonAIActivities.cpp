#include "Activities/CommonAIActivities.h"

#include "AIController.h"
#include "CommonAIProfile.h"
#include "Athena/CommonAIBackend.h"
#include "Integration/CommonAIBehaviorTreeExecution.h"
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

void UCommonAIAction::StartAction(UAthenaAIDataManager* DataManager, AAIController* Controller)
{
	UCommonAIBackend* Backend = CastChecked<UCommonAIBackend>(OwnerTask->GetOwnerAthenaAIComp());
	TGuardValue<bool> Guard(Backend->bInCallback, true);
	if (UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask))
		if (Task->GetAgent()) Task->GetAgent()->SetCurrentAction(ActionTag, {});
	++StepEpoch;
	bStepActive = true;
	bCompletionQueued = false;
	Super::StartAction(DataManager, Controller);
	if (bStepActive)
	{
		ExecuteStep(Controller);
		if (UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask))
			if (Task->GetAgent()) Task->GetAgent()->QueueActivityProgress();
	}
}

void UCommonAIAction::ActionTick()
{
	if (!bStepActive) return;
	UCommonAIBackend* Backend = CastChecked<UCommonAIBackend>(OwnerTask->GetOwnerAthenaAIComp());
	TGuardValue<bool> Guard(Backend->bInCallback, true);
	TickStep();
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
			UCommonAIBackend* Backend = CastChecked<UCommonAIBackend>(WeakThis->OwnerTask->GetOwnerAthenaAIComp());
			TGuardValue<bool> Guard(Backend->bInCallback, true);
			if (const UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(WeakThis->OwnerTask))
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
	const UCommonAITaskAdapter* Work = Cast<UCommonAITaskAdapter>(OwnerTask);
	UCommonAIAgentComponent* Agent = Work ? Work->GetAgent() : nullptr;
	if (Agent && Agent->GetMovementAdapter() && IsValid(Target) && (!bHasGuardObject || GuardObject.IsValid()))
	{
		if (!IsValid(Agent->GetMovementAdapter())) { CompleteMove(false); return; }
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

void UCommonAIMoveAction::TickStep()
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
        if (UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask))
            Task->SetFailureReason(MoveTarget.IsValid() ? CommonAITags::Reason_MovementFailed : CommonAITags::Reason_TargetLost);
    CompleteStep(bSucceeded);
}
UCommonAITaskAdapter::UCommonAITaskAdapter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    bInterruptible = false;
    bEnableTick = false;
    TickInterval = TargetValidationIntervalSeconds;
    MaxMomentum = 0;
}

void UCommonAITaskAdapter::Configure(const FCommonAITaskDefinition& InDefinition, bool bInFallback)
{
    Definition = InDefinition;
    bFallback = bInFallback;
    TaskTag = Definition.Activity;
    Priority = static_cast<int8>(Definition.Priority);
    ScoreModifier = Definition.ScoreModifier;
    ReplaceResistance = Definition.ReplaceResistance;
    TickInterval = Definition.TickIntervalSeconds;
    UnlockTaskTime = Definition.UnlockDelaySeconds;
    MaxMomentum = static_cast<uint8>(Definition.MaxMomentum);
    MomentumScoreReduction = static_cast<uint8>(Definition.MomentumReductionPercent);
    MomentumDecreaseTime = Definition.MomentumDecaySeconds;
    Considerations.Reset();
    if (!bFallback && !Definition.bCommandOnly)
        for (const FCommonAICurveBinding& Binding : Definition.Considerations) Considerations.AddTag(Binding.Consideration);
    Actions.Reset();
    TArray<FCommonAIActionDefinition> Entries = Definition.Actions;
    if (Definition.Kind != ECommonAITaskKind::Sequence)
    {
        FCommonAIActionDefinition Entry;
        Entry.Id = Definition.Kind == ECommonAITaskKind::Wait ? CommonAITags::Action_Wait
            : Definition.Kind == ECommonAITaskKind::BehaviorTree ? CommonAITags::Action_BehaviorTree : CommonAITags::Action_Execute;
        Entry.Kind = ECommonAIActionKind::Custom;
        Entry.TickIntervalSeconds = Definition.TickIntervalSeconds;
        if (Definition.Kind == ECommonAITaskKind::Wait)
        {
            Entry.Kind = ECommonAIActionKind::Wait;
            Entry.DurationSeconds = Definition.WaitDurationSeconds;
        }
        else if (Definition.Kind == ECommonAITaskKind::BehaviorTree)
        {
            UCommonAIBehaviorTreeExecution* Tree = NewObject<UCommonAIBehaviorTreeExecution>(this);
            Tree->Tree = Definition.BehaviorTree;
            Entry.Execution = Tree;
        }
        else Entry.Execution = Definition.Execution;
        Entries = { Entry };
    }
    for (const FCommonAIActionDefinition& Entry : Entries)
    {
        UCommonAIAction* Action = nullptr;
        switch (Entry.Kind)
        {
        case ECommonAIActionKind::FindOpportunity: Action = NewObject<UCommonAIFindWorkAction>(this); break;
        case ECommonAIActionKind::Reserve: Action = NewObject<UCommonAIReserveWorkAction>(this); break;
        case ECommonAIActionKind::Move:
        {
            UCommonAIMoveAction* Move = NewObject<UCommonAIMoveAction>(this);
            Move->SetAcceptanceRadius(Entry.AcceptanceRadiusCentimeters);
            Action = Move;
            break;
        }
        case ECommonAIActionKind::Interact: Action = NewObject<UCommonAIPerformWorkAction>(this); break;
        case ECommonAIActionKind::Release: Action = NewObject<UCommonAIReleaseWorkAction>(this); break;
        default:
        {
            UCommonAIConfiguredAction* Configured = NewObject<UCommonAIConfiguredAction>(this);
            Configured->Configure(Entry, Entry.TickIntervalSeconds);
            Action = Configured;
            break;
        }
        }
        Action->SetActionIdentity(Entry.Id);
        Action->SetTickCadence(Entry.TickIntervalSeconds);
        Actions.Add(Action);
    }
}

UCommonAIAction* UCommonAITaskAdapter::CurrentCommonAction() const
{
    return Actions.IsValidIndex(CurrentActionIndex) ? Cast<UCommonAIAction>(Actions[CurrentActionIndex]) : nullptr;
}

FCommonAIExecutionContext UCommonAITaskAdapter::MakeExecutionContext(FGameplayTag Action, FGuid Entry) const
{
    FCommonAIExecutionContext Context;
    Context.Agent = Agent.Get();
    Context.Request = Request;
    Context.Pawn = Request.Pawn;
    Context.Target = Site;
    Context.Action = Action;
    Context.Entry = Entry;
    Context.Invocation = FGuid::NewGuid();
    return Context;
}

void UCommonAITaskAdapter::Start(UAthenaAIDataManager* DataManager, AAIController* Controller)
{
    if (bRunning) return;
    UCommonAIBackend* Backend = CastChecked<UCommonAIBackend>(OwnerAthenaAIComp);
    TGuardValue<bool> Guard(Backend->bInCallback, true);
    bRunning = true;
    bHasReservation = bSequenceFinished = bCancelling = false;
    EndReason = {};
    Site.Reset();
    RunController = Controller;
    Agent = Controller ? Controller->FindComponentByClass<UCommonAIAgentComponent>() : nullptr;
    Provider = Agent.IsValid() ? Agent->GetProvider() : nullptr;
    Request.Execution = FGuid::NewGuid();
    Request.Operation.Invalidate();
    Request.Generation = Agent.IsValid() ? Agent->GetInputGeneration() : FGuid();
    Request.Agent = Controller;
    Request.Pawn = Controller ? Controller->GetPawn() : nullptr;
    Request.Activity = TaskTag;
    Request.Payload = Backend->GetStartPayload(TaskTag);
    if (Agent.IsValid())
    {
        BusyContribution = Agent->AddTagContribution(this, Definition.ActiveStateTags);
        Agent->QueueActivityStarted(Request);
        Agent->SetExecutionSnapshot(bFallback ? ECommonAIPhase::Idle : ECommonAIPhase::Executing, Request, nullptr);
    }
    Super::Start(DataManager, Controller);
}

bool UCommonAITaskAdapter::SetSite(UObject* NewSite)
{
    if (bHasReservation || !IsValid(NewSite) || !NewSite->GetClass()->ImplementsInterface(UCommonAIInteraction::StaticClass())) return false;
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
    return true;
}

void UCommonAITaskAdapter::MarkReserved()
{
    bHasReservation = true;
    if (Agent.IsValid()) Agent->SetExecutionSnapshot(bCancelling ? ECommonAIPhase::Cancelling : ECommonAIPhase::Approaching, Request, Site.Get(), EndReason);
    if (bCancelling && Site.IsValid())
        ICommonAIInteraction::Execute_RequestInteractionCancellation(Site.Get(), Request, EndReason);
}

void UCommonAITaskAdapter::RequestCancellation(FGameplayTag Reason)
{
    if (!bRunning || bCancelling) return;
    bCancelling = true;
    EndReason = Reason;
    if (Agent.IsValid()) Agent->SetExecutionSnapshot(ECommonAIPhase::Cancelling, Request, Site.Get(), Reason);
    if (UCommonAIAction* Action = CurrentCommonAction()) Action->RequestSafeCancellation(Reason);
    if (bHasReservation && Site.IsValid())
        ICommonAIInteraction::Execute_RequestInteractionCancellation(Site.Get(), Request, Reason);
}

bool UCommonAITaskAdapter::TryFinishCancellation()
{
    if (!bRunning || !bCancelling) return false;
    if (const UCommonAIAction* Action = CurrentCommonAction())
        if (!Action->HasReachedSafeEnd()) return false;
    FCommonAIInteractionResult TargetResult;
    if (bHasReservation && Site.IsValid())
    {
        TargetResult = ICommonAIInteraction::Execute_GetInteractionResult(Site.Get(), Request);
        if (!TargetResult.IsTerminal()) return false;
    }
    const bool bTargetFailed = bHasReservation && (!Site.IsValid() || TargetResult.Status == ECommonAIInteractionStatus::Failed
            || TargetResult.Status == ECommonAIInteractionStatus::Rejected);
    const UCommonAIExecution* Active = GetActiveExecution();
    const bool bStepFailed = Active && (Active->GetResult().Status == ECommonAIInteractionStatus::Failed
        || Active->GetResult().Status == ECommonAIInteractionStatus::Rejected);
    const bool bFailed = bTargetFailed || bStepFailed;
    if (bTargetFailed) EndReason = Site.IsValid() ? TargetResult.Reason : CommonAITags::Reason_TargetLost;
    else if (bStepFailed) EndReason = Active->GetResult().Reason.IsValid() ? Active->GetResult().Reason : CommonAITags::Reason_InteractionFailed;
    Stop(OwnerAthenaAIComp->GetDataManager(), RunController.Get(), bFailed ? EAAITaskResult::Aborted : EAAITaskResult::Replaced);
    return true;
}

void UCommonAITaskAdapter::TaskTick()
{
    if (!bRunning) return;
    UCommonAIBackend* Backend = CastChecked<UCommonAIBackend>(OwnerAthenaAIComp);
    TGuardValue<bool> Guard(Backend->bInCallback, true);
    if (bCancelling) { TryFinishCancellation(); return; }
    Super::TaskTick();
}

void UCommonAITaskAdapter::Stop(UAthenaAIDataManager* DataManager, AAIController* Controller, EAAITaskResult Result)
{
    if (!bRunning) return;
    UCommonAIBackend* Backend = CastChecked<UCommonAIBackend>(OwnerAthenaAIComp);
    TGuardValue<bool> Guard(Backend->bInCallback, true);
    bRunning = false;
    FCommonAIInteractionResult Outcome;
    Outcome.Status = bCancelling && Result == EAAITaskResult::Replaced ? ECommonAIInteractionStatus::Cancelled
        : Result == EAAITaskResult::Successful && bSequenceFinished
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
    if (Agent.IsValid()) Agent->RemoveTagContribution(this, BusyContribution);
    const EAAITaskResult VendorResult = Outcome.Status == ECommonAIInteractionStatus::Completed ? EAAITaskResult::Successful
        : Outcome.Status == ECommonAIInteractionStatus::Cancelled ? EAAITaskResult::Replaced
        : Result == EAAITaskResult::Failed ? EAAITaskResult::Failed : EAAITaskResult::Aborted;
    Super::Stop(DataManager, Controller, VendorResult);
    if (Agent.IsValid()) Agent->RecordExecutionResult(Request, Outcome);
    Backend->OnExecutionEnded(Request.Execution, Outcome);
    Request.Payload.Reset();
}

void UCommonAITaskAdapter::FinishTask()
{
    bSequenceFinished = !bHasReservation;
    Super::FinishTask();
}

void UCommonAITaskAdapter::Cleanup()
{
    if (bRunning) Stop(OwnerAthenaAIComp->GetDataManager(), RunController.Get(), EAAITaskResult::Failed);
    Super::Cleanup();
}

bool UCommonAITaskAdapter::SignalExecutionComplete(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason)
{
    if (UCommonAIConfiguredAction* Action = Cast<UCommonAIConfiguredAction>(CurrentCommonAction()))
        return Action->SignalComplete(Invocation, Status, Reason);
    return false;
}

UCommonAIExecution* UCommonAITaskAdapter::GetActiveExecution() const
{
    const UCommonAIConfiguredAction* Action = Cast<UCommonAIConfiguredAction>(CurrentCommonAction());
    return Action ? Action->GetExecution() : nullptr;
}

void UCommonAIConfiguredAction::Configure(const FCommonAIActionDefinition& InDefinition, float IntervalSeconds)
{
    Definition = InDefinition;
    ActionTag = Definition.Id;
    TickInterval = IntervalSeconds;
    if (Definition.Kind == ECommonAIActionKind::Wait)
    {
        UCommonAIWaitExecution* Wait = NewObject<UCommonAIWaitExecution>(this);
        Wait->DurationSeconds = Definition.DurationSeconds;
        Execution = Wait;
    }
    else if (Definition.Execution) Execution = DuplicateObject<UCommonAIExecution>(Definition.Execution, this);
    bEnableTick = Execution && Execution->RequiresTick();
}

void UCommonAIConfiguredAction::ExecuteStep(AAIController* Controller)
{
    UCommonAITaskAdapter* Task = CastChecked<UCommonAITaskAdapter>(OwnerTask);
    if (Definition.Kind == ECommonAIActionKind::ResolveBinding)
    {
        UObject* Target = Task->GetAgent() ? Task->GetAgent()->GetExecutionBinding(Definition.BindingId) : nullptr;
        CompleteStep(Target && Task->SetSite(Target));
        return;
    }
    LastTickTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const FCommonAIExecutionContext Context = Task->MakeExecutionContext(Definition.Id, Definition.EntryId);
    if (Task->GetAgent()) Task->GetAgent()->SetCurrentAction(Definition.Id, Context.Invocation);
    if (Execution) CompletionHandle = Execution->OnFinished.AddUObject(this, &UCommonAIConfiguredAction::HandleFinished);
    if (!Execution || !Execution->BeginExecution(Context))
    {
        CompleteStep(false);
        return;
    }
}

void UCommonAIConfiguredAction::TickStep()
{
    if (!Execution) { CompleteStep(false); return; }
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : LastTickTime;
    Execution->TickExecution(static_cast<float>(FMath::Max(0.0, Now - LastTickTime)));
    LastTickTime = Now;
}

void UCommonAIConfiguredAction::HandleFinished(FGuid Invocation, const FCommonAIInteractionResult& Outcome)
{
    if (!Execution || Invocation != Execution->GetContext().Invocation) return;
    UCommonAIBackend* Backend = CastChecked<UCommonAIBackend>(OwnerTask->GetOwnerAthenaAIComp());
    TGuardValue<bool> Guard(Backend->bInCallback, true);
    if (Outcome.IsTerminal())
    {
        if (Outcome.Status == ECommonAIInteractionStatus::Cancelled)
        {
            if (UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask)) Task->RequestCancellation(Outcome.Reason);
            return;
        }
        if (UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask))
            if (Outcome.Status != ECommonAIInteractionStatus::Completed) Task->SetFailureReason(Outcome.Reason);
        CompleteStep(Outcome.Status == ECommonAIInteractionStatus::Completed);
    }
}

void UCommonAIConfiguredAction::RequestSafeCancellation(FGameplayTag Reason)
{
    if (Execution) Execution->CancelExecution(Reason);
}

bool UCommonAIConfiguredAction::HasReachedSafeEnd() const
{
    return !Execution || Execution->GetResult().IsTerminal();
}

bool UCommonAIConfiguredAction::SignalComplete(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason)
{
    return Execution && Execution->CompleteExecution(Invocation, Status, Reason);
}

void UCommonAIConfiguredAction::StopAction()
{
    if (Execution)
    {
        Execution->OnFinished.Remove(CompletionHandle);
        Execution->AbandonExecution(CommonAITags::Reason_Disabled);
        Execution->ReleaseContext();
    }
    Super::StopAction();
}
void UCommonAIFindWorkAction::ExecuteStep(AAIController* Controller)
{
    UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask);
    const UCommonAIAgentComponent* Agent = Task ? Task->GetAgent() : nullptr;
    UObject* Target = Task && Task->GetProvider() && Agent && Agent->Profile
        ? Task->GetProvider()->FindOpportunity(Task->GetRequest(), Task->GetDefinition().RequiredTargetCapabilities) : nullptr;
    const bool bSelected = Target && Task->SetSite(Target);
    if (!bSelected && Task) Task->SetFailureReason(CommonAITags::Reason_NoOpportunity);
    CompleteStep(bSelected);
}

void UCommonAIReserveWorkAction::ExecuteStep(AAIController* Controller)
{
    UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask);
    TSet<UObject*> AttemptedTargets;
    UObject* Candidate = Task ? Task->GetSite() : nullptr;
    const UCommonAIWorkProviderComponent* InitialProvider = Task ? Task->GetProvider() : nullptr;
    // Capture the bound before invoking authored callbacks, which may grow the provider's target list.
    const int32 MaximumAttempts = 1 + (InitialProvider ? InitialProvider->GetCandidateCount(Task->GetRequest().Activity) : 0);
    bool bReserved = false;
    while (Task && Task->IsRunning() && !Task->IsCancelling() && AttemptedTargets.Num() < MaximumAttempts)
    {
        if (!IsValid(Candidate))
        {
            UCommonAIWorkProviderComponent* Provider = Task->GetProvider();
            Candidate = Provider ? Provider->FindOpportunity(Task->GetRequest(),
                Task->GetDefinition().RequiredTargetCapabilities, &AttemptedTargets) : nullptr;
            if (!IsValid(Candidate) || AttemptedTargets.Contains(Candidate) || !Task->IsRunning()
                || Task->IsCancelling() || !Task->SetSite(Candidate)) break;
        }
        if (AttemptedTargets.Contains(Candidate) || !Task->IsRunning() || Task->IsCancelling()
            || !Task->PrepareReservation()) break;
        AttemptedTargets.Add(Candidate);
        bReserved = ICommonAIInteraction::Execute_ReserveInteraction(Candidate, Task->GetRequest());
        if (bReserved)
        {
            // A successful reserve can synchronously request cancellation; retain ownership so normal cleanup releases it.
            Task->MarkReserved();
            break;
        }
        Candidate = nullptr;
    }
    if (!bReserved && Task && !Task->IsCancelling()) Task->SetFailureReason(CommonAITags::Reason_ReservationFailed);
    CompleteStep(bReserved);
}

UCommonAIPerformWorkAction::UCommonAIPerformWorkAction()
{
    bEnableTick = true;
    TickInterval = TargetValidationIntervalSeconds;
}

void UCommonAIPerformWorkAction::ExecuteStep(AAIController* Controller)
{
    UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask);
    if (!Task || !IsValid(Task->GetSite())) { CompleteStep(false); return; }
    if (Task->IsCancelling()) return;

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

void UCommonAIPerformWorkAction::TickStep()
{
    UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask);
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
        if (!bSucceeded) Task->SetFailureReason(Result.Reason);
        CompleteStep(bSucceeded);
    }
}

void UCommonAIReleaseWorkAction::ExecuteStep(AAIController* Controller)
{
    UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(OwnerTask);
    const bool bReleased = Task && IsValid(Task->GetSite())
        && ICommonAIInteraction::Execute_ReleaseInteraction(Task->GetSite(), Task->GetRequest(), false);
    if (bReleased) Task->MarkReleased();
    else if (Task) Task->SetFailureReason(CommonAITags::Reason_ReservationFailed);
    CompleteStep(bReleased);
}






