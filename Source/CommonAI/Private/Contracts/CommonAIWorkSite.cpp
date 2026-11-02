#include "Contracts/CommonAIWorkSite.h"
#include "CommonAIAgentComponent.h"
#include "CommonAITags.h"
#include "AIController.h"
#include "Engine/World.h"
#include "TimerManager.h"

UCommonAIWorkSite::UCommonAIWorkSite()
{
    Activities.AddTag(CommonAITags::Activity_Work);
    Capabilities.AddTag(CommonAITags::Capability_Work);
}

bool UCommonAIWorkSite::Owns(const FCommonAIInteractionRequest& Request) const
{
    return Request.Execution.IsValid() && Request.Execution == Execution && IsValid(Request.Agent)
        && ReservationOwner.Get() == Request.Agent;
}

FCommonAIInteractionOffer UCommonAIWorkSite::QueryInteraction_Implementation(const FCommonAIInteractionRequest& Request) const
{
    FCommonAIInteractionOffer Offer;
    Offer.ApproachActor = GetOwner();
    Offer.Capabilities = Capabilities;
    const UCommonAIAgentComponent* Agent = IsValid(Request.Agent)
        ? Request.Agent->FindComponentByClass<UCommonAIAgentComponent>() : nullptr;
    Offer.bAvailable = !bTearingDown && IsValid(GetOwner()) && IsValid(Request.Agent) && !ReservationOwner.IsValid()
        && Activities.HasTag(Request.Activity)
        && (RequiredAgentTags.IsEmpty() || (Agent && RequiredAgentTags.Matches(Agent->GetEffectiveTags())));
    return Offer;
}

bool UCommonAIWorkSite::ReserveInteraction_Implementation(const FCommonAIInteractionRequest& Request)
{
    if (!Request.Execution.IsValid() || !ICommonAIInteraction::Execute_QueryInteraction(this, Request).bAvailable)
        return false;
    ClearExecution();
    Execution = Request.Execution;
    ReservationOwner = Request.Agent;
    ReservedActivity = Request.Activity;
    Result.Status = ECommonAIInteractionStatus::Reserved;
    return true;
}

FCommonAIInteractionResult UCommonAIWorkSite::BeginInteraction_Implementation(const FCommonAIInteractionRequest& Request)
{
    if (!Owns(Request) || Result.Status != ECommonAIInteractionStatus::Reserved) return {};
    Result.Status = ECommonAIInteractionStatus::Running;
    OnInteractionStarted.Broadcast(Request);
    return GetInteractionResult_Implementation(Request);
}

void UCommonAIWorkSite::RequestInteractionCancellation_Implementation(const FCommonAIInteractionRequest& Request, FGameplayTag Reason)
{
    if (!Owns(Request) || Result.IsTerminal() || Result.Status == ECommonAIInteractionStatus::Cancelling) return;
    const bool bWasReserved = Result.Status == ECommonAIInteractionStatus::Reserved;
    Result.Status = ECommonAIInteractionStatus::Cancelling;
    Result.Reason = Reason;
    OnCancellationRequested.Broadcast(Request, Reason);
    if (bWasReserved || !bWaitForSafeCancellation) SignalSafeCancellation(Request.Execution);
}

FCommonAIInteractionResult UCommonAIWorkSite::GetInteractionResult_Implementation(const FCommonAIInteractionRequest& Request) const
{
    return Owns(Request) ? Result : FCommonAIInteractionResult{};
}

void UCommonAIWorkSite::SignalCompletion(FGuid ExpectedExecution, bool bSucceeded)
{
    if (ExpectedExecution != Execution || !ReservationOwner.IsValid()
        || (Result.Status != ECommonAIInteractionStatus::Running && Result.Status != ECommonAIInteractionStatus::Cancelling)) return;
    const bool bCancelling = Result.Status == ECommonAIInteractionStatus::Cancelling;
    Result.Status = !bSucceeded ? ECommonAIInteractionStatus::Failed
        : bCancelling ? ECommonAIInteractionStatus::Cancelled : ECommonAIInteractionStatus::Completed;
    if (!bSucceeded) Result.Reason = CommonAITags::Reason_InteractionFailed;
}

void UCommonAIWorkSite::SignalSafeCancellation(FGuid ExpectedExecution)
{
    if (ExpectedExecution == Execution && ReservationOwner.IsValid() && Result.Status == ECommonAIInteractionStatus::Cancelling)
        Result.Status = ECommonAIInteractionStatus::Cancelled;
}

void UCommonAIWorkSite::ReleaseInteraction_Implementation(const FCommonAIInteractionRequest& Request, bool bEmergencyTeardown)
{
    if (Owns(Request) && (bEmergencyTeardown || Result.IsTerminal() || Result.Status == ECommonAIInteractionStatus::Reserved))
    {
        const bool bNotifyAbandonment = bEmergencyTeardown && !Result.IsTerminal();
        ClearExecution();
        // Clear before notifying: a listener may synchronously reserve a new execution.
        if (bNotifyAbandonment) OnInteractionAbandoned.Broadcast(Request);
    }
}

void UCommonAIWorkSite::ClearExecution()
{
    Execution.Invalidate();
    ReservationOwner.Reset();
    ReservedActivity = {};
    Result = {};
}

void UCommonAIWorkSite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    AbandonCurrentExecution();
    Super::EndPlay(EndPlayReason);
}

void UCommonAIWorkSite::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    AbandonCurrentExecution();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UCommonAIWorkSite::AbandonCurrentExecution()
{
    bTearingDown = true;
    FCommonAIInteractionRequest Previous;
    Previous.Execution = Execution;
    Previous.Agent = ReservationOwner.Get();
    Previous.Activity = ReservedActivity;
    const bool bNotify = Execution.IsValid() && !Result.IsTerminal();
    ClearExecution();
    if (bNotify) OnInteractionAbandoned.Broadcast(Previous);
}

FCommonAIInteractionResult UCommonAITimedWorkSite::BeginInteraction_Implementation(const FCommonAIInteractionRequest& Request)
{
    if (!GetWorld() || WorkDurationSeconds <= 0.0f) return {};
    const FCommonAIInteractionResult Started = Super::BeginInteraction_Implementation(Request);
    if (Started.Status == ECommonAIInteractionStatus::Running || Started.Status == ECommonAIInteractionStatus::Cancelling)
    {
        TWeakObjectPtr<UCommonAITimedWorkSite> WeakThis(this);
        const FGuid ExpectedExecution = Request.Execution;
        GetWorld()->GetTimerManager().SetTimer(WorkTimer, [WeakThis, ExpectedExecution]()
        {
            if (WeakThis.IsValid()) WeakThis->SignalCompletion(ExpectedExecution, true);
        }, WorkDurationSeconds, false);
    }
    return Started;
}

void UCommonAITimedWorkSite::ClearExecution()
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(WorkTimer);
    Super::ClearExecution();
}
