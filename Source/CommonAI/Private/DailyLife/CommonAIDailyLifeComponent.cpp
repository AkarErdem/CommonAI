#include "DailyLife/CommonAIDailyLifeComponent.h"
#include "DailyLife/CommonAIDailyLifeProfile.h"
#include "CommonAIProfile.h"
#include "CommonAITags.h"
#include "Contracts/CommonAISmartObjectSite.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
    constexpr int32 NeedChannel = 0;
    constexpr int32 TimeChannel = 1;
    constexpr int32 AvailabilityChannel = 2;
    constexpr int32 DistanceChannel = 3;
    constexpr int32 InputChannelCount = 4;

    float GetScheduleOffset(FGuid Identity, float VariationHours)
    {
        FRandomStream Random(GetTypeHash(Identity));
        return Random.FRandRange(-VariationHours, VariationHours);
    }

    float GetTimePreference(const FCommonAIDailyActivityDefinition& Activity, double Now, float Offset)
    {
        return Activity.PreferredTimes.IsEmpty() || Activity.PreferredTimes.ContainsByPredicate(
            [=](const FCommonAITimePreference& Window) { return Window.Contains(Now, Offset); })
            ? 1.0f : Activity.OutsideWindowPreference;
    }

    bool IsOffCooldown(const FCommonAIDailyLifeState& State, FGameplayTag Activity, double Now)
    {
        const double* Cooldown = State.CooldownUntilHours.Find(Activity);
        return !Cooldown || Now >= *Cooldown;
    }

    bool IsNeedSatisfied(const FCommonAIDailyActivityDefinition& Activity, const FCommonAIDailyLifeState& State)
    {
        return (Activity.Kind == ECommonAIDailyActivity::Eat && State.Hunger <= Activity.SatisfiedNeed)
            || ((Activity.Kind == ECommonAIDailyActivity::Sleep || Activity.Kind == ECommonAIDailyActivity::Rest) && State.Fatigue <= Activity.SatisfiedNeed);
    }

    void IntegrateNeeds(FCommonAIDailyLifeState& State, const UCommonAIDailyLifeProfile& Profile, double Now,
        const FCommonAIDailyActivityDefinition* Executing)
    {
        if (!FMath::IsFinite(Now) || Now < 0.0) return;
        const double Delta = State.LastTimeHours >= 0.0 ? FMath::Max(0.0, Now - State.LastTimeHours) : 0.0;
        State.LastTimeHours = Now;
        double ActiveHours = Executing ? Delta : 0.0;
        // Routine-timed recovery stops at the authored duration even if observation is delayed by a hitch.
        if (Executing && !Executing->bCompleteOnSiteSignal)
            ActiveHours = FMath::Min(ActiveHours, FMath::Max(0.0, Executing->DurationHours - State.ProgressHours));
        State.Hunger = FMath::Clamp(static_cast<float>(State.Hunger + Delta * Profile.HungerPerHour
            - ActiveHours * (Executing ? Executing->HungerRecoveryPerHour : 0.0f)), 0.0f, 1.0f);
        State.Fatigue = FMath::Clamp(static_cast<float>(State.Fatigue + Delta * Profile.FatiguePerHour
            - ActiveHours * (Executing ? Executing->FatigueRecoveryPerHour : 0.0f)), 0.0f, 1.0f);
        State.ProgressHours += ActiveHours;
    }
}

UCommonAIDailyLifeComponent::UCommonAIDailyLifeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

UObject* UCommonAIDailyLifeComponent::ResolveRoutineContext() const
{
    return RoutineContext || bRequiresExternalContext ? RoutineContext.Get() : const_cast<UCommonAIDailyLifeComponent*>(this);
}

double UCommonAIDailyLifeComponent::GetCurrentTimeHours() const
{
    const UObject* Provider = ContextProvider ? ContextProvider.Get() : ResolveRoutineContext();
    return IsValid(Provider) && Provider->GetClass()->ImplementsInterface(UCommonAIDailyLifeContext::StaticClass())
        ? ICommonAIDailyLifeContext::Execute_GetDailyLifeTimeHours(Provider) : -1.0;
}

bool UCommonAIDailyLifeComponent::CanInitializeRoutine(const UCommonAIAgentComponent* InAgent) const
{
    if (bEndingPlay || IsBeingDestroyed() || !Profile || !Profile->ValidateProfile().IsEmpty() || !IsValid(InAgent) || !GetWorld()) return false;
    const APawn* Pawn = Cast<APawn>(GetOwner());
    const AAIController* Controller = Cast<AAIController>(InAgent->GetOwner());
    if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || !Controller || Controller->GetPawn() != Pawn
        || Controller != Pawn->GetController()) return false;
    if (const UObject* Source = InAgent->GetInputSource(CommonAIDailyLife::SourceTag()); Source && Source != this) return false;
    if (InAgent->Profile && InAgent->Profile != AgentProfile)
    {
        const UCommonAIDailyLifeComponent* PreviousRoutine = Cast<UCommonAIDailyLifeComponent>(InAgent->Profile->GetOuter());
        if (!PreviousRoutine || PreviousRoutine->GetRoutineAgent() == InAgent
            || IsValid(InAgent->GetInputSource(CommonAIDailyLife::SourceTag()))) return false;
    }
    return true;
}

bool UCommonAIDailyLifeComponent::PreparePersistentState()
{
    if (!Profile || !Profile->ValidateProfile().IsEmpty() || !GetOwner()) return false;
    if (!bStateInitialized)
    {
        State = InitialState;
        if (!State.Identity.IsValid()) State.Identity = FGuid::NewGuid();
        if (State.DisplayName.IsEmpty()) State.DisplayName = GetOwner()->GetName();
        if (!State.Occupation.IsValid()) State.Occupation = Profile->Occupation;
        if (Home && !State.HomeId.IsValid()) State.HomeId = Home->SiteId;
        if (Work && !State.WorkId.IsValid()) State.WorkId = Work->SiteId;
        bStateInitialized = true;
    }
    return CanRestoreState(State);
}

bool UCommonAIDailyLifeComponent::InitializeRoutine(UCommonAIAgentComponent* InAgent, UObject* InContext)
{
    if (bInitializingRoutine) return false;
    return InitializeRoutineInternal(InAgent, InContext, true);
}

bool UCommonAIDailyLifeComponent::InitializeRoutineInternal(UCommonAIAgentComponent* InAgent, UObject* InContext, bool bRegisterResident)
{
    TGuardValue<bool> InitializeGuard(bInitializingRoutine, true);
    if (!InContext) InContext = ResolveRoutineContext();
    if (!CanInitializeRoutine(InAgent) || !IsValid(InContext)
        || !InContext->GetClass()->ImplementsInterface(UCommonAIDailyLifeContext::StaticClass())) return false;
    if (Agent.Get() == InAgent && ContextProvider == InContext)
    {
        RoutineContext = InContext == this ? nullptr : InContext;
        bRequiresExternalContext = InContext != this;
        return (!bRegisterResident || RegisterPersistentState()) && Agent.Get() == InAgent
            && ContextProvider == InContext && CanInitializeRoutine(InAgent);
    }
    // Finish the previous clock domain before selecting the next one. Fresh state is prepared only afterward.
    ShutdownRoutine();
    if (!CanInitializeRoutine(InAgent) || !IsValid(InContext)) return false;
    RoutineContext = InContext == this ? nullptr : InContext;
    bRequiresExternalContext = InContext != this;
    if (!(bRegisterResident ? RegisterPersistentState() : PreparePersistentState())
        || !CanInitializeRoutine(InAgent) || !IsValid(InContext)) return false;
    bShuttingDown = false;
    if (!AgentProfile) AgentProfile = Profile->CreateAgentProfile(this);
    if (!AgentProfile || !InAgent->RegisterInputSource(CommonAIDailyLife::SourceTag(), this)) return false;
    Agent = InAgent;
    RetiredExecution = InAgent->GetSnapshot().Execution;
    ContextProvider = InContext;
    InAgent->bStartAutomatically = false;
    InAgent->SetComponentTickInterval(Profile->ExecutionIntervalSeconds);
    InAgent->SetComponentTickEnabled(true);
    InAgent->OnActivityStarted.AddUniqueDynamic(this, &UCommonAIDailyLifeComponent::HandleStarted);
    InAgent->OnActivityProgress.AddUniqueDynamic(this, &UCommonAIDailyLifeComponent::HandleProgress);
    InAgent->OnActivityEnded.AddUniqueDynamic(this, &UCommonAIDailyLifeComponent::HandleEnded);
    InAgent->OnInputsReset.AddUniqueDynamic(this, &UCommonAIDailyLifeComponent::HandleInputsReset);
    if (!InAgent->SetProfile(AgentProfile)) { ShutdownRoutine(); return false; }
    InAgent->AddTagContribution(this, FGameplayTagContainer(State.Occupation));
    RegisterDestinations();
    if (State.Activity.IsValid())
    {
        const FCommonAIDailyActivityDefinition* SavedActivity = Profile->FindActivity(State.Activity);
        const bool bAlreadyComplete = SavedActivity && !SavedActivity->bCompleteOnSiteSignal
            && (State.ProgressHours >= SavedActivity->DurationHours || IsNeedSatisfied(*SavedActivity, State));
        if (bAlreadyComplete)
        {
            State.ResumeActivity = {};
            State.ResumeProgressHours = 0.0;
            State.CooldownUntilHours.Add(State.Activity, FMath::Max(0.0, GetCurrentTimeHours()) + SavedActivity->CooldownHours);
        }
        else
        {
            State.ResumeActivity = State.Activity;
            State.ResumeProgressHours = State.ProgressHours;
        }
        State.Activity = {};
        State.ProgressHours = 0.0;
    }
    // Identity-staggered observation prevents the settlement from rescoring on one frame.
    FRandomStream Random(GetTypeHash(State.Identity));
    GetWorld()->GetTimerManager().SetTimer(ObservationTimer, this, &UCommonAIDailyLifeComponent::RefreshRoutine,
        Profile->ObservationIntervalSeconds, true, Random.FRandRange(Profile->ExecutionIntervalSeconds, Profile->ObservationIntervalSeconds));
    RefreshRoutine();
    return Agent.Get() == InAgent && ContextProvider == InContext && CanInitializeRoutine(InAgent);
}

void UCommonAIDailyLifeComponent::RegisterDestinations()
{
    if (!Agent.IsValid()) return;
    for (UCommonAISmartObjectSite* Site : Destinations) if (IsValid(Site)) Agent->AddInteractionTarget(Site);
    if (IsValid(Home)) Agent->AddInteractionTarget(Home);
    if (IsValid(Work)) Agent->AddInteractionTarget(Work);
}

void UCommonAIDailyLifeComponent::ReleaseCommitment()
{
    if (Agent.IsValid() && CommitmentHold.IsValid()) Agent->ResumeScoring(this, CommitmentHold);
    CommitmentHold.Invalidate();
}

void UCommonAIDailyLifeComponent::ShutdownRoutine()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ObservationTimer);
    if (bShuttingDown) return;
    bShuttingDown = true;
    // An unbound restored state has no retiring clock domain to sample.
    if (ContextProvider) AdvanceNeeds(GetCurrentTimeHours());
    if (Agent.IsValid())
    {
        if (State.Activity.IsValid())
        {
            State.ResumeActivity = State.Activity;
            State.ResumeProgressHours = State.ProgressHours;
        }
        Agent->OnActivityStarted.RemoveDynamic(this, &UCommonAIDailyLifeComponent::HandleStarted);
        Agent->OnActivityProgress.RemoveDynamic(this, &UCommonAIDailyLifeComponent::HandleProgress);
        Agent->OnActivityEnded.RemoveDynamic(this, &UCommonAIDailyLifeComponent::HandleEnded);
        Agent->OnInputsReset.RemoveDynamic(this, &UCommonAIDailyLifeComponent::HandleInputsReset);
        if (Agent->Profile == AgentProfile) Agent->StopAgent();
        ReleaseCommitment();
        if (InterruptionHold.IsValid()) Agent->ResumeScoring(this, InterruptionHold);
        Agent->ReleaseOwnedState(this);
        if (UCommonAIWorkProviderComponent* Provider = Agent->GetProvider(); Provider && AgentProfile)
            for (const FCommonAITaskDefinition& Task : AgentProfile->Tasks) Provider->ClearActivityTarget(Task.Activity);
    }
    InterruptionHold.Invalidate();
    ActiveExecution.Invalidate();
    bInteracting = false;
    Agent.Reset();
    ContextProvider = nullptr;
}

void UCommonAIDailyLifeComponent::AdvanceUnrepresentedNeeds(FCommonAIDailyLifeState& InOutState, const UCommonAIDailyLifeProfile& Definition, double ToHours)
{
    IntegrateNeeds(InOutState, Definition, ToHours, nullptr);
}

void UCommonAIDailyLifeComponent::AdvanceNeeds(double Now)
{
    if (Profile && bStateInitialized) IntegrateNeeds(State, *Profile, Now,
        bInteracting && !bCompletionRequested && !InterruptionReason.IsValid() ? Profile->FindActivity(State.Activity) : nullptr);
}

FCommonAIDailyLifeState UCommonAIDailyLifeComponent::ExportState() const
{
    FCommonAIDailyLifeState Result = bStateInitialized ? State : InitialState;
    if (Profile && !Result.Occupation.IsValid()) Result.Occupation = Profile->Occupation;
    if (Home && !Result.HomeId.IsValid()) Result.HomeId = Home->SiteId;
    if (Work && !Result.WorkId.IsValid()) Result.WorkId = Work->SiteId;
    if (Profile && bStateInitialized) IntegrateNeeds(Result, *Profile, GetCurrentTimeHours(),
        bInteracting && !bCompletionRequested && !InterruptionReason.IsValid() ? Profile->FindActivity(State.Activity) : nullptr);
    return Result;
}

bool UCommonAIDailyLifeComponent::CanRestoreState(const FCommonAIDailyLifeState& SavedState) const
{
    if (!SavedState.IsValid() || (Profile && SavedState.Occupation != Profile->Occupation)) return false;
    if (Profile && !Profile->ValidateProfile().IsEmpty()) return false;
    if (Agent.IsValid())
    {
        const APawn* Pawn = Cast<APawn>(GetOwner());
        if (!Pawn || Pawn->GetController() != Agent->GetOwner() || !IsValid(ContextProvider)) return false;
    }
    if (bStateInitialized && State.Identity.IsValid() && State.Identity != SavedState.Identity) return false;
    if (InitialState.Identity.IsValid() && InitialState.Identity != SavedState.Identity) return false;
    if (Profile && ((SavedState.Activity.IsValid() && SavedState.Activity != CommonAITags::Activity_Wait && !Profile->FindActivity(SavedState.Activity))
        || (SavedState.ResumeActivity.IsValid() && SavedState.ResumeActivity != CommonAITags::Activity_Wait && !Profile->FindActivity(SavedState.ResumeActivity)))) return false;
    return true;
}

bool UCommonAIDailyLifeComponent::RestoreState(const FCommonAIDailyLifeState& SavedState)
{
    if (!CanRestoreState(SavedState)) return false;
    const TWeakObjectPtr<UCommonAIAgentComponent> PreviousAgent = Agent;
    const TWeakObjectPtr<UObject> PreviousContext = ContextProvider.Get();
    const bool bWasInitialized = PreviousAgent.IsValid();
    const FCommonAIDailyLifeState Copy = SavedState;
    ShutdownRoutine();
    if (bEndingPlay || IsBeingDestroyed() || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
        || !CanRestoreState(Copy)) return false;
    State = Copy;
    bStateInitialized = true;
    bShuttingDown = false;
    // The save service can already own registration while applying a retained record.
    if (bWasInitialized) return PreviousAgent.IsValid() && PreviousContext.IsValid()
        && InitializeRoutineInternal(PreviousAgent.Get(), PreviousContext.Get(), false);
    return true;
}

UCommonAISmartObjectSite* UCommonAIDailyLifeComponent::ChooseDestination(const FCommonAIDailyActivityDefinition& Activity) const
{
    if (!Agent.IsValid() || Activity.Kind == ECommonAIDailyActivity::Idle) return nullptr;
    FCommonAIInteractionRequest Query = Agent->GetSnapshot().Interaction;
    Query.Agent = Cast<AAIController>(Agent->GetOwner());
    Query.Pawn = Cast<APawn>(GetOwner());
    Query.Activity = Activity.Activity;
    auto Available = [&](UCommonAISmartObjectSite* Site)
    {
        if (!IsValid(Site)) return false;
        const FCommonAIInteractionOffer Offer = ICommonAIInteraction::Execute_QueryInteraction(Site, Query);
        return Offer.bAvailable && IsValid(Offer.ApproachActor)
            && (Activity.RequiredCapabilities.IsEmpty() || Activity.RequiredCapabilities.Matches(Offer.Capabilities));
    };
    if (Activity.Kind == ECommonAIDailyActivity::Sleep || Activity.Kind == ECommonAIDailyActivity::ReturnHome)
        return Home && Home->SiteId == State.HomeId && Available(Home) ? Home.Get() : nullptr;
    if (Activity.Kind == ECommonAIDailyActivity::Work)
        return Work && Work->SiteId == State.WorkId && Available(Work) ? Work.Get() : nullptr;
    if (State.Activity == Query.Activity)
        if (UCommonAISmartObjectSite* Current = Cast<UCommonAISmartObjectSite>(Agent->GetSnapshot().Target.Get()))
            if (Available(Current)) return Current;
    TArray<UCommonAISmartObjectSite*> Eligible;
    for (UCommonAISmartObjectSite* Site : Destinations) if (Available(Site)) Eligible.Add(Site);
    if (Eligible.IsEmpty()) return nullptr;
    if (Activity.Kind == ECommonAIDailyActivity::Wander)
    {
        FRandomStream Random(HashCombine(GetTypeHash(State.Identity), GetTypeHash(State.CompletedActivities)));
        return Eligible[Random.RandRange(0, Eligible.Num() - 1)];
    }
    Eligible.Sort([this](const UCommonAISmartObjectSite& Left, const UCommonAISmartObjectSite& Right)
    {
        return FVector::DistSquared(GetOwner()->GetActorLocation(), Left.GetOwner()->GetActorLocation())
            < FVector::DistSquared(GetOwner()->GetActorLocation(), Right.GetOwner()->GetActorLocation());
    });
    return Eligible[0];
}

void UCommonAIDailyLifeComponent::PublishInputs()
{
    if (!Agent.IsValid() || !Profile || !Agent->GetProvider()) return;
    const double Now = GetCurrentTimeHours();
    if (!FMath::IsFinite(Now) || Now < 0.0) return;
    const float Offset = GetScheduleOffset(State.Identity, Profile->ScheduleVariationHours);
    TArray<FCommonAIInputUpdate> Updates;
    for (const FCommonAIDailyActivityDefinition& Entry : Profile->Activities)
    {
        const FGameplayTag Activity = Entry.Activity;
        UCommonAISmartObjectSite* Candidate = ChooseDestination(Entry);
        TArray<UObject*> PermittedTargets;
        if (Candidate) PermittedTargets.Add(Candidate);
        if (Entry.Kind != ECommonAIDailyActivity::Work && Entry.Kind != ECommonAIDailyActivity::Sleep
            && Entry.Kind != ECommonAIDailyActivity::ReturnHome && Entry.Kind != ECommonAIDailyActivity::Idle)
            for (UCommonAISmartObjectSite* Site : Destinations)
                if (IsValid(Site) && Site->Activities.HasTag(Activity)) PermittedTargets.AddUnique(Site);
        Agent->GetProvider()->SetActivityTargets(Activity, PermittedTargets);
        const bool bReady = IsOffCooldown(State, Activity, Now) && !InterruptionReason.IsValid();
        float Values[InputChannelCount] = { 1.0f, 1.0f, bReady && (Candidate || Entry.Kind == ECommonAIDailyActivity::Idle) ? 1.0f : 0.0f, 1.0f };
        if (Entry.Kind == ECommonAIDailyActivity::Eat)
            Values[NeedChannel] = FMath::Clamp((State.Hunger - Entry.NeedThreshold) / (1.0f - Entry.NeedThreshold), 0.0f, 1.0f);
        else if (Entry.Kind == ECommonAIDailyActivity::Sleep || Entry.Kind == ECommonAIDailyActivity::Rest)
            Values[NeedChannel] = FMath::Clamp((State.Fatigue - Entry.NeedThreshold) / (1.0f - Entry.NeedThreshold), 0.0f, 1.0f);
        else Values[NeedChannel] = 1.0f - FMath::Max(State.Hunger, State.Fatigue);
        // Keep an executing need-recovery activity eligible down to its completion threshold.
        const bool bOtherNeedUrgent = (Entry.Kind != ECommonAIDailyActivity::Eat && State.Hunger >= Profile->UrgentNeedThreshold)
            || (Entry.Kind == ECommonAIDailyActivity::Eat && State.Fatigue >= Profile->UrgentNeedThreshold);
        if (Activity == State.Activity && bInteracting && !bOtherNeedUrgent
            && (Entry.HungerRecoveryPerHour > 0.0f || Entry.FatigueRecoveryPerHour > 0.0f))
            Values[NeedChannel] = 1.0f;
        Values[TimeChannel] = GetTimePreference(Entry, Now, Offset);
        if (Candidate)
        {
            const double Distance = FVector::Distance(GetOwner()->GetActorLocation(), Candidate->GetOwner()->GetActorLocation());
            Values[DistanceChannel] = static_cast<float>(1.0 / (1.0 + Distance / Profile->TravelCostDistanceCentimeters));
        }
        const auto InputTags = Entry.GetInputTags();
        for (int32 Channel = 0; Channel < InputChannelCount; ++Channel)
        {
            FCommonAIInputUpdate Update;
            Update.Id = InputTags[Channel];
            Update.RawValue = Values[Channel];
            Updates.Add(Update);
        }
    }
    Agent->ApplyInputUpdates(this, Agent->GetInputGeneration(), Updates, true);
}

void UCommonAIDailyLifeComponent::RefreshRoutine()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CommonAI_DailyLifeObservation);
    if (bShuttingDown || !Agent.IsValid() || !Profile) return;
    const double Now = GetCurrentTimeHours();
    AdvanceNeeds(Now);
    const FGameplayTag Reason = !FMath::IsFinite(Now) || Now < 0.0 ? CommonAITags::Reason_InputUnavailable
        : ICommonAIDailyLifeContext::Execute_GetDailyLifeInterruptionReason(ContextProvider);
    if (Reason.IsValid())
    {
        if (!InterruptionHold.IsValid()) InterruptionHold = Agent->PauseScoring(this, Reason);
        InterruptionReason = Reason;
        ReleaseCommitment();
        Agent->RequestCancellation(Reason);
        return;
    }
    InterruptionReason = {};
    if (InterruptionHold.IsValid())
    {
        Agent->ResumeScoring(this, InterruptionHold);
        InterruptionHold.Invalidate();
    }
    if (!Agent->GetSnapshot().bEnabled) Agent->StartAgent();
    // A terminal interaction is draining its Release step. Keep selection paused until its result is published.
    if (bCompletionRequested) return;
    const FCommonAIDailyActivityDefinition* Current = Profile->FindActivity(State.Activity);
    if (Current)
    {
        const float Offset = GetScheduleOffset(State.Identity, Profile->ScheduleVariationHours);
        const auto HasAvailableRecovery = [&](ECommonAIDailyActivity Kind, float Need)
        {
            return Profile->Activities.ContainsByPredicate([&](const FCommonAIDailyActivityDefinition& Entry)
            {
                return Entry.Kind == Kind && Need > Entry.NeedThreshold && Entry.BaseWeight > 0.0f
                    && IsOffCooldown(State, Entry.Activity, Now) && GetTimePreference(Entry, Now, Offset) > 0.0f
                    && ChooseDestination(Entry);
            });
        };
        const bool bRecoveringNeed = Current->HungerRecoveryPerHour > Profile->HungerPerHour || Current->FatigueRecoveryPerHour > Profile->FatiguePerHour;
        const bool bUrgentHunger = !bRecoveringNeed && State.Hunger >= Profile->UrgentNeedThreshold && Current->Kind != ECommonAIDailyActivity::Eat
            && HasAvailableRecovery(ECommonAIDailyActivity::Eat, State.Hunger);
        const bool bUrgentFatigue = !bRecoveringNeed && State.Fatigue >= Profile->UrgentNeedThreshold
            && Current->Kind != ECommonAIDailyActivity::Sleep && Current->Kind != ECommonAIDailyActivity::Rest
            && (HasAvailableRecovery(ECommonAIDailyActivity::Sleep, State.Fatigue)
                || HasAvailableRecovery(ECommonAIDailyActivity::Rest, State.Fatigue));
        if (CommitmentHold.IsValid() && (State.ProgressHours - ProgressAtStartHours >= Current->MinimumCommitmentHours || bUrgentHunger || bUrgentFatigue)) ReleaseCommitment();
        if (bUrgentHunger || bUrgentFatigue)
        {
            // Yield to an available urgent need. Athena still chooses the next activity; two urgent needs do not repeatedly cancel each other's recovery.
            Agent->RequestCancellation(CommonAIDailyLife::NeedReason());
        }
        else Agent->SetTaskResistance(State.Activity, Current->ReplaceResistance);
        if (!bInteracting && Now - StartedAtHours >= Profile->TravelTimeoutHours)
        {
            ReleaseCommitment();
            Agent->RequestCancellation(CommonAITags::Reason_MovementFailed);
        }
        if (bInteracting && !Current->bCompleteOnSiteSignal)
        {
            const bool bNeedsSatisfied = IsNeedSatisfied(*Current, State);
            if (State.ProgressHours >= Current->DurationHours || (State.ProgressHours >= Current->MinimumCommitmentHours && bNeedsSatisfied))
            {
                const FCommonAIAgentSnapshot Snapshot = Agent->GetSnapshot();
                if (UCommonAISmartObjectSite* Site = Cast<UCommonAISmartObjectSite>(Snapshot.Target.Get())) Site->SignalCompletion(Snapshot.Interaction, true);
                else if (Current->Kind == ECommonAIDailyActivity::Idle)
                    Agent->CompleteExecution(Snapshot.Invocation, ECommonAIInteractionStatus::Completed, {});
                bCompletionRequested = true;
                if (!CommitmentHold.IsValid()) CommitmentHold = Agent->PauseScoring(this, CommonAITags::State_Busy);
            }
        }
    }
    PublishInputs();
}

void UCommonAIDailyLifeComponent::HandleInputsReset(FGuid Generation)
{
    if (!Agent.IsValid()) return;
    Agent->RegisterInputSource(CommonAIDailyLife::SourceTag(), this);
    RegisterDestinations();
    PublishInputs();
}

void UCommonAIDailyLifeComponent::HandleStarted(FCommonAIInteractionRequest Request)
{
    if (bShuttingDown || !Profile || !Agent.IsValid() || Request.Execution == RetiredExecution
        || Request.Pawn.Get() != GetOwner() || Request.Agent != Agent->GetOwner()
        || Request.Generation != Agent->GetInputGeneration()) return;
    const FCommonAIAgentSnapshot Snapshot = Agent->GetSnapshot();
    // Current-generation runs may start and finish between observations; their ordered terminal event still owns cooldown cleanup.
    if (Snapshot.bProfileChangePending) return;
    AdvanceNeeds(GetCurrentTimeHours());
    ReleaseCommitment();
    State.Activity = Request.Activity;
    State.DestinationId.Invalidate();
    State.ProgressHours = State.ResumeActivity == Request.Activity ? State.ResumeProgressHours : 0.0;
    const FCommonAIDailyActivityDefinition* Current = Profile->FindActivity(State.Activity);
    if (Current && !Current->bCompleteOnSiteSignal && State.ProgressHours >= Current->DurationHours) State.ProgressHours = 0.0;
    ProgressAtStartHours = State.ProgressHours;
    if (State.ResumeActivity == Request.Activity) { State.ResumeActivity = {}; State.ResumeProgressHours = 0.0; }
    ActiveExecution = Request.Execution;
    StartedAtHours = GetCurrentTimeHours();
    bCompletionRequested = false;
    bInteracting = Current && Current->Kind == ECommonAIDailyActivity::Idle;
    if (Current && Current->MinimumCommitmentHours > 0.0f)
        CommitmentHold = Agent->PauseScoring(this, CommonAITags::State_Busy);
}

void UCommonAIDailyLifeComponent::HandleProgress(FCommonAIActivityProgress Progress)
{
    if (Progress.Execution != ActiveExecution || bShuttingDown) return;
    AdvanceNeeds(GetCurrentTimeHours());
    if (const UCommonAISmartObjectSite* Site = Cast<UCommonAISmartObjectSite>(Progress.Target.Get())) State.DestinationId = Site->SiteId;
    const FCommonAIDailyActivityDefinition* Current = Profile ? Profile->FindActivity(State.Activity) : nullptr;
    bInteracting = Progress.Phase == ECommonAIPhase::Interacting || (Current && Current->Kind == ECommonAIDailyActivity::Idle);
}

void UCommonAIDailyLifeComponent::HandleEnded(FCommonAIInteractionRequest Request, FCommonAIInteractionResult Result)
{
    if (Request.Execution != ActiveExecution || bShuttingDown) return;
    AdvanceNeeds(GetCurrentTimeHours());
    const FCommonAIDailyActivityDefinition* Current = Profile ? Profile->FindActivity(Request.Activity) : nullptr;
    if (Current)
    {
        if (Result.Status == ECommonAIInteractionStatus::Completed) ++State.CompletedActivities;
        if (Result.Status == ECommonAIInteractionStatus::Cancelled && Current->bResumeAfterInterruption)
        {
            State.ResumeActivity = Request.Activity;
            State.ResumeProgressHours = State.ProgressHours;
        }
        State.CooldownUntilHours.Add(Request.Activity, FMath::Max(0.0, GetCurrentTimeHours()) + Current->CooldownHours);
    }
    LastTransition = FString::Printf(TEXT("%s: %s (%s)"), *Request.Activity.ToString(),
        *UEnum::GetValueAsString(Result.Status), Result.Reason.IsValid() ? *Result.Reason.ToString() : TEXT("duration/needs complete"));
    if (Current && Result.Status != ECommonAIInteractionStatus::Completed) LastIssue = LastTransition;
    State.Activity = {};
    State.DestinationId.Invalidate();
    State.ProgressHours = 0.0;
    ActiveExecution.Invalidate();
    bInteracting = false;
    bCompletionRequested = false;
    ReleaseCommitment();
    // Publish cooldown and post-activity need inputs before the backend processes a pending rescore.
    PublishInputs();
}

FString UCommonAIDailyLifeComponent::GetDebugText() const
{
    const FCommonAIDailyLifeState Current = ExportState();
    const FCommonAIAgentSnapshot Snapshot = Agent.IsValid() ? Agent->GetSnapshot() : FCommonAIAgentSnapshot();
    const FCommonAIDailyActivityDefinition* Definition = Profile ? Profile->FindActivity(Snapshot.Activity) : nullptr;
    FString Scores;
    for (const FCommonAITaskSnapshot& Task : Snapshot.Tasks)
    {
        const FCommonAIDailyActivityDefinition* Entry = Profile ? Profile->FindActivity(Task.Activity) : nullptr;
        if (Task.bFallback) continue;
        Scores += FString::Printf(TEXT("%s %.2f%s | "), Entry ? *Entry->Label.ToString() : *Task.Activity.ToString(),
            Task.Score, Task.bLocked ? TEXT(" locked") : TEXT(""));
    }
    const UCommonAISmartObjectSite* Site = Cast<UCommonAISmartObjectSite>(Snapshot.Target.Get());
    const FCommonAIInteractionRequest Lease = Site ? Site->GetReservation() : FCommonAIInteractionRequest();
    const FCommonAIInteractionRequest& Request = Snapshot.Interaction;
    const bool bClaimed = Site && Site->HasClaim();
    const bool bOwnsClaim = bClaimed && Request.Execution.IsValid() && Request.Operation.IsValid()
        && Lease.Execution == Request.Execution && Lease.Operation == Request.Operation && Lease.Generation == Request.Generation
        && Lease.Pawn == Request.Pawn && Lease.Agent == Request.Agent && Lease.Activity == Request.Activity;
    const TCHAR* Decision = Snapshot.Command.Execution.IsValid() && Snapshot.Command.Execution == Snapshot.Execution ? TEXT("explicit command")
        : InterruptionReason.IsValid() ? TEXT("suspended by context") : !Definition ? TEXT("no active scored activity")
        : CommitmentHold.IsValid() ? TEXT("utility selection; commitment held") : TEXT("utility selection / replacement resistance");
    const double Now = GetCurrentTimeHours();
    const double Hour = Now >= 0.0 ? FMath::Fmod(Now, CommonAIDailyLife::HoursPerDay) : -1.0;
    FString Windows;
    if (Definition) for (const FCommonAITimePreference& Window : Definition->PreferredTimes)
        Windows += FString::Printf(TEXT("%.1f-%.1f "), Window.StartHour, Window.EndHour);
    const FString Progress = Definition && Definition->bCompleteOnSiteSignal
        ? FString::Printf(TEXT("%.2f h | awaiting site result"), Current.ProgressHours)
        : FString::Printf(TEXT("%.2f / %.2f h"), Current.ProgressHours, Definition ? Definition->DurationHours : 0.0f);
    return FString::Printf(TEXT("%s | %s\n%s | %s\nHunger %.0f%% | Fatigue %.0f%% | Time %.2f\nDestination %s | Reservation %s\nProgress %s | Preferred %s\nDecision: %s\n%s\nLast: %s\nIssue: %s%s"),
        *Current.DisplayName, Profile ? *Profile->OccupationLabel.ToString() : TEXT("unconfigured"),
        Definition ? *Definition->Label.ToString() : *Snapshot.Activity.ToString(), *UEnum::GetValueAsString(Snapshot.Phase),
        Current.Hunger * 100.0f, Current.Fatigue * 100.0f, Hour,
        Site ? *Site->GetOwner()->GetName() : TEXT("unavailable"), bOwnsClaim ? TEXT("held") : bClaimed ? TEXT("occupied by another execution") : TEXT("none"),
        *Progress, Windows.IsEmpty() ? TEXT("any") : *Windows,
        Decision, *Scores, LastTransition.IsEmpty() ? TEXT("none") : *LastTransition, LastIssue.IsEmpty() ? TEXT("none") : *LastIssue,
        InterruptionReason.IsValid() ? *FString::Printf(TEXT(" | Interrupted: %s"), *InterruptionReason.ToString()) : TEXT(""));
}
