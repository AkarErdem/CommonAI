#include "CommonAIAgentComponent.h"
#include "CommonAIProfile.h"
#include "CommonAITags.h"
#include "Athena/CommonAIBackend.h"
#include "Activities/CommonAIActivities.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "Contracts/CommonAIMovement.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Integration/CommonAIBehaviorTreeExecution.h"

UCommonAIAgentComponent::UCommonAIAgentComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

UCommonAIAgentComponent* UCommonAIAgentComponent::FindAgent(AActor* Actor)
{
    if (!IsValid(Actor)) return nullptr;
    if (UCommonAIAgentComponent* Direct = Actor->FindComponentByClass<UCommonAIAgentComponent>()) return Direct;
    if (const APawn* Pawn = Cast<APawn>(Actor))
        return Pawn->GetController() ? Pawn->GetController()->FindComponentByClass<UCommonAIAgentComponent>() : nullptr;
    return nullptr;
}

bool UCommonAIAgentComponent::EnsureBackend()
{
    if (Backend) return true;
    AAIController* Controller = Cast<AAIController>(GetOwner());
    if (bEndingPlay || !Controller || !Profile || !Profile->ValidateProfile().IsEmpty()) return false;
    if (!Provider)
    {
        Provider = Controller->FindComponentByClass<UCommonAIWorkProviderComponent>();
        if (!Provider)
        {
            Provider = NewObject<UCommonAIWorkProviderComponent>(Controller);
            Provider->RegisterComponent();
        }
    }
    UCommonAIBackend* Created = NewObject<UCommonAIBackend>(Controller);
    if (!Created->Prepare(this, Profile)) return false;
    Backend = Created;
    Backend->RegisterComponent();
    return Backend->GetDataManager() != nullptr;
}

void UCommonAIAgentComponent::BeginPlay()
{
    Super::BeginPlay();
    if (AAIController* Controller = Cast<AAIController>(GetOwner()))
        Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &UCommonAIAgentComponent::HandlePawnChanged);
    if (bStartAutomatically) StartAgent();
}

bool UCommonAIAgentComponent::StartAgent()
{
    if (bEndingPlay || bStopPending || PendingProfile || !EnsureBackend()) return false;
    if (Backend->bInCallback || Backend->bInScoring) return false;
    if (bResetInputsForPawn) { Backend->ResetInputs(); bResetInputsForPawn = false; }
    if (!Backend->IsEnabled())
    {
        TGuardValue<bool> Guard(Backend->bInCallback, true);
        OnInputsReset.Broadcast(Backend->GetInputGeneration());
    }
    if (bStopPending || PendingProfile) { ApplyPendingOperation(); return Snapshot.bEnabled; }
    Snapshot.bEnabled = Backend->ActivateAgent();
    return Snapshot.bEnabled;
}

void UCommonAIAgentComponent::StopAgent()
{
    bRestartAfterDrain = false;
    bStopPending = true;
    if (Backend)
    {
        Backend->HoldForTransition();
        Backend->CancelCommands(CommonAITags::Reason_Disabled);
    }
    RequestCancellation(CommonAITags::Reason_Disabled);
    ApplyPendingOperation();
}

bool UCommonAIAgentComponent::SetProfile(UCommonAIProfile* NewProfile)
{
    return ChangeProfile(NewProfile, ECommonAITransitionMode::CancelAtSafePoint, true);
}

bool UCommonAIAgentComponent::ChangeProfile(UCommonAIProfile* NewProfile, ECommonAITransitionMode Transition, bool bPreserveInputs)
{
    if (bEndingPlay || !IsValid(NewProfile) || !NewProfile->ValidateProfile().IsEmpty()) return false;
    PendingProfile = NewProfile;
    PendingProfileMode = Transition;
    bPreserveProfileInputs = bPreserveInputs;
    if (Backend) Backend->HoldForTransition();
    if (Transition == ECommonAITransitionMode::CancelAtSafePoint) RequestCancellation(CommonAITags::Reason_ProfileChanged);
    ApplyPendingOperation();
    return true;
}

void UCommonAIAgentComponent::RequestCancellation(FGameplayTag Reason)
{
    if (Backend && !bEndingPlay) Backend->CancelCurrent(Reason);
}

bool UCommonAIAgentComponent::CancelActivity(FGuid Execution, FGameplayTag Reason)
{
    UCommonAITaskAdapter* Task = Backend ? Backend->GetRunningTask() : nullptr;
    if (!Task || Task->GetRequest().Execution != Execution) return false;
    Backend->CancelCurrent(Reason);
    return true;
}

void UCommonAIAgentComponent::ApplyPendingOperation()
{
    if (Backend && (Backend->bInCallback || Backend->bInScoring || Backend->bPublishingInputs || Backend->GetRunningTask())) return;
    if (!bStopPending && !PendingProfile) return;
    const AAIController* Controller = Cast<AAIController>(GetOwner());
    const bool bRestart = (Snapshot.bEnabled && !bStopPending)
        || (bRestartAfterDrain && Controller && Controller->GetPawn());
    TArray<FCommonAIConsiderationSnapshot> Preserved;
    if (Backend)
    {
        if (PendingProfile && bPreserveProfileInputs && !bResetInputsForPawn) Preserved = Backend->InputSnapshots();
        Backend->DisableAthenaAI();
    }
    Snapshot.bEnabled = false;
    if (PendingProfile)
    {
        if (Backend) Backend->DestroyComponent();
        Backend = nullptr;
        Profile = PendingProfile;
        PendingProfile = nullptr;
        if (EnsureBackend() && !Preserved.IsEmpty()) Backend->RestoreInputs(Preserved);
    }
    bStopPending = false;
    bRestartAfterDrain = false;
    if (bRestart) StartAgent();
}

void UCommonAIAgentComponent::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    if (OldPawn)
    {
        StopAgent();
        bResetInputsForPawn = true;
        InputSources.Reset();
        InputSourceRevisions.Reset();
        auto IsOldPawnSource = [OldPawn](UObject* Source)
        {
            if (!Source) return false;
            if (Source == OldPawn || Source->IsIn(OldPawn)) return true;
            const UActorComponent* Component = Cast<UActorComponent>(Source);
            return Component && Component->GetOwner() == OldPawn;
        };
        TagContributions.RemoveAll([&](const FCommonAITagContribution& Entry) { return IsOldPawnSource(Entry.Source.Get()); });
        ScoringHolds.RemoveAll([&](const FControlHold& Entry) { return IsOldPawnSource(Entry.Source.Get()); });
        TaskHolds.RemoveAll([&](const FControlHold& Entry) { return IsOldPawnSource(Entry.Source.Get()); });
        if (OldPawn->IsActorBeingDestroyed() && Backend)
        {
            if (UCommonAITaskAdapter* Task = Backend->GetRunningTask())
            {
                Task->SetFailureReason(CommonAITags::Reason_TargetLost);
                Task->Stop(Backend->GetDataManager(), Cast<AAIController>(GetOwner()), EAAITaskResult::Failed);
            }
            ApplyPendingOperation();
        }
        for (int32 Index = MontageWindows.Num() - 1; Index >= 0; --Index)
        {
            const FMontageWindow& Window = MontageWindows[Index];
            if (Window.Mesh.IsValid() && Window.Mesh->GetOwner() == OldPawn)
            {
                RemoveTagContribution(Window.Mesh.Get(), Window.Contribution);
                MontageWindows.RemoveAt(Index);
            }
        }
    }
    if (NewPawn && bStartAutomatically)
    {
        if (bStopPending) bRestartAfterDrain = true;
        else StartAgent();
    }
}

bool UCommonAIAgentComponent::AddInteractionTarget(UObject* Target)
{
    if (!Provider)
    {
        if (!Cast<AAIController>(GetOwner())) return false;
        Provider = GetOwner()->FindComponentByClass<UCommonAIWorkProviderComponent>();
        if (!Provider)
        {
            Provider = NewObject<UCommonAIWorkProviderComponent>(GetOwner());
            Provider->RegisterComponent();
        }
    }
    return Provider->AddTarget(Target);
}

void UCommonAIAgentComponent::RemoveInteractionTarget(UObject* Target)
{
    if (Provider) Provider->RemoveTarget(Target);
}

bool UCommonAIAgentComponent::SetMovementAdapter(UObject* Adapter)
{
    if (Backend && Backend->GetRunningTask() && !Backend->GetRunningTask()->IsFallback()) return false;
    if (Adapter && (!IsValid(Adapter) || !Adapter->GetClass()->ImplementsInterface(UCommonAIMovement::StaticClass()))) return false;
    MovementAdapter = Adapter;
    return true;
}

bool UCommonAIAgentComponent::RegisterInputSource(FGameplayTag SourceId, UObject* Source)
{
    if (bEndingPlay || !SourceId.IsValid() || !IsValid(Source)) return false;
    const TWeakObjectPtr<UObject>* Existing = InputSources.Find(SourceId);
    if (Existing && Existing->IsValid() && Existing->Get() != Source) return false;
    if (Existing && Existing->Get() == Source) return true;
    const bool bReplacingExpiredSource = Existing != nullptr;
    InputSources.Add(SourceId, Source);
    InputSourceRevisions.Add(SourceId, FGuid::NewGuid());
    if (bReplacingExpiredSource && Backend) Backend->InvalidateSource(SourceId);
    return true;
}

bool UCommonAIAgentComponent::UnregisterInputSource(FGameplayTag SourceId, UObject* Source)
{
    const TWeakObjectPtr<UObject>* Existing = InputSources.Find(SourceId);
    if (!Existing || Existing->Get() != Source) return false;
    InputSources.Remove(SourceId);
    InputSourceRevisions.Remove(SourceId);
    if (Backend) Backend->InvalidateSource(SourceId);
    return true;
}

UObject* UCommonAIAgentComponent::GetInputSource(FGameplayTag Id) const
{
    const TWeakObjectPtr<UObject>* Source = InputSources.Find(Id);
    return Source ? Source->Get() : nullptr;
}

FGuid UCommonAIAgentComponent::GetInputSourceRevision(FGameplayTag Id) const
{
    const FGuid* Revision = InputSourceRevisions.Find(Id);
    return Revision ? *Revision : FGuid();
}

FGuid UCommonAIAgentComponent::GetInputGeneration() const
{
    return Backend ? Backend->GetInputGeneration() : FGuid();
}

bool UCommonAIAgentComponent::ApplyInputUpdates(UObject* Source, FGuid Generation, const TArray<FCommonAIInputUpdate>& Updates, bool bRescore)
{
    return !bEndingPlay && Backend && Backend->ApplyInputs(Source, Generation, Updates, bRescore);
}

bool UCommonAIAgentComponent::GetConsideration(FGameplayTag Id, FCommonAIConsiderationSnapshot& Value) const
{
    return Backend && Backend->GetInput(Id, Value);
}

bool UCommonAIAgentComponent::EvaluateInput(UObject* Source, FGuid Generation, FGameplayTag Id, const FInstancedStruct& Context, bool bRescore)
{
    return !bEndingPlay && Backend && Backend->EvaluateInput(Source, Generation, Id, Context, bRescore);
}

bool UCommonAIAgentComponent::RegisterExecutionBinding(FGameplayTag Id, UObject* Binding)
{
    if (!Id.IsValid() || !IsValid(Binding) || !Binding->GetClass()->ImplementsInterface(UCommonAIInteraction::StaticClass())) return false;
    const TWeakObjectPtr<UObject>* Existing = ExecutionBindings.Find(Id);
    if (Existing && Existing->IsValid() && Existing->Get() != Binding) return false;
    ExecutionBindings.Add(Id, Binding);
    return true;
}

bool UCommonAIAgentComponent::UpdateExecutionBinding(FGameplayTag Id, UObject* ExpectedBinding, UObject* NewBinding)
{
    if (!Id.IsValid() || GetExecutionBinding(Id) != ExpectedBinding) return false;
    if (NewBinding && (!IsValid(NewBinding) || !NewBinding->GetClass()->ImplementsInterface(UCommonAIInteraction::StaticClass()))) return false;
    if (NewBinding) ExecutionBindings.Add(Id, NewBinding);
    else ExecutionBindings.Remove(Id);
    return true;
}

UObject* UCommonAIAgentComponent::GetExecutionBinding(FGameplayTag Id) const
{
    const TWeakObjectPtr<UObject>* Binding = ExecutionBindings.Find(Id);
    return Binding ? Binding->Get() : nullptr;
}

bool UCommonAIAgentComponent::CompleteExecution(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason)
{
    UCommonAITaskAdapter* Task = Backend ? Backend->GetRunningTask() : nullptr;
    return Task && Task->SignalExecutionComplete(Invocation, Status, Reason);
}

bool UCommonAIAgentComponent::CompleteBehaviorTree(UBehaviorTreeComponent* Tree, bool bSucceeded, FGameplayTag Reason)
{
    UCommonAITaskAdapter* Task = Backend ? Backend->GetRunningTask() : nullptr;
    UCommonAIBehaviorTreeExecution* Execution = Task ? Cast<UCommonAIBehaviorTreeExecution>(Task->GetActiveExecution()) : nullptr;
    return Execution && Execution->OwnsTree(Tree)
        && Execution->CompleteExecution(Execution->GetContext().Invocation,
            bSucceeded ? ECommonAIInteractionStatus::Completed : ECommonAIInteractionStatus::Failed, Reason);
}

FGuid UCommonAIAgentComponent::RequestActivity(FGameplayTag Activity, ECommonAITransitionMode Transition,
    ECommonAICommandRetention Retention, bool bUnlockRetry)
{
    return !bEndingPlay && !bStopPending && !PendingProfile && Backend
        ? Backend->RequestCommand(Activity, Transition, Retention, bUnlockRetry) : FGuid();
}

FGuid UCommonAIAgentComponent::RequestActivityWithContext(FGameplayTag Activity, const FInstancedStruct& Payload,
    ECommonAITransitionMode Transition, ECommonAICommandRetention Retention, bool bUnlockRetry)
{
    return !bEndingPlay && !bStopPending && !PendingProfile && Backend
        ? Backend->RequestCommand(Activity, Transition, Retention, bUnlockRetry, Payload) : FGuid();
}

FGuid UCommonAIAgentComponent::PauseScoring(UObject* Source, FGameplayTag Reason)
{
    if (!IsValid(Source)) return {};
    FControlHold& Hold = ScoringHolds.AddDefaulted_GetRef();
    Hold.Id = FGuid::NewGuid();
    Hold.Source = Source;
    Hold.Reason = Reason;
    if (Backend) Backend->RefreshScoringHold();
    return Hold.Id;
}

bool UCommonAIAgentComponent::ResumeScoring(UObject* Source, FGuid Hold)
{
    const bool bRemoved = ScoringHolds.RemoveAll([Source, Hold](const FControlHold& Entry)
    { return Entry.Id == Hold && Entry.Source.Get() == Source; }) > 0;
    if (Backend) Backend->RefreshScoringHold();
    return bRemoved;
}

bool UCommonAIAgentComponent::HasScoringHolds() const
{
    return ScoringHolds.ContainsByPredicate([](const FControlHold& Hold) { return Hold.Source.IsValid(); });
}

bool UCommonAIAgentComponent::Rescore()
{
    return Backend && !bEndingPlay && Backend->Rescore();
}

FGuid UCommonAIAgentComponent::LockTask(UObject* Source, FGameplayTag Activity, FGameplayTag Reason)
{
    UCommonAITaskAdapter* Task = Backend ? Backend->FindTask(Activity) : nullptr;
    if (!IsValid(Source) || !Task || Task->IsFallback()) return {};
    FControlHold& Hold = TaskHolds.AddDefaulted_GetRef();
    Hold.Id = FGuid::NewGuid();
    Hold.Source = Source;
    Hold.Activity = Activity;
    Hold.Reason = Reason;
    Backend->NotifyEligibilityChanged();
    return Hold.Id;
}

bool UCommonAIAgentComponent::UnlockTask(UObject* Source, FGuid Hold)
{
    const bool bRemoved = TaskHolds.RemoveAll([Source, Hold](const FControlHold& Entry)
    { return Entry.Id == Hold && Entry.Source.Get() == Source; }) > 0;
    if (Backend) Backend->NotifyEligibilityChanged();
    return bRemoved;
}

bool UCommonAIAgentComponent::HasTaskLock(FGameplayTag Activity) const
{
    return TaskHolds.ContainsByPredicate([Activity](const FControlHold& Hold) { return Hold.Activity == Activity && Hold.Source.IsValid(); });
}

void UCommonAIAgentComponent::ReleaseOwnedState(UObject* Source, bool bReleaseInputs)
{
    if (!Source) return;
    const int32 RemovedTags = TagContributions.RemoveAll([Source](const FCommonAITagContribution& Entry) { return Entry.Source.Get() == Source; });
    ScoringHolds.RemoveAll([Source](const FControlHold& Entry) { return Entry.Source.Get() == Source; });
    const int32 RemovedLocks = TaskHolds.RemoveAll([Source](const FControlHold& Entry) { return Entry.Source.Get() == Source; });
    if (bReleaseInputs)
    {
        TArray<FGameplayTag> OwnedInputs;
        for (const auto& Entry : InputSources) if (Entry.Value.Get() == Source) OwnedInputs.Add(Entry.Key);
        for (const FGameplayTag Id : OwnedInputs) UnregisterInputSource(Id, Source);
    }
    if (Backend && (RemovedTags || RemovedLocks)) Backend->NotifyEligibilityChanged();
}

bool UCommonAIAgentComponent::SetTaskScoreModifier(FGameplayTag Activity, float Value)
{
    UCommonAITaskAdapter* Task = Backend ? Backend->FindTask(Activity) : nullptr;
    if (!Task || !FMath::IsFinite(Value) || Value < 0.0f) return false;
    Task->SetScoreModifier(Value);
    Backend->bScoresDirty = true;
    return true;
}

bool UCommonAIAgentComponent::SetTaskPriority(FGameplayTag Activity, int32 Value)
{
    UCommonAITaskAdapter* Task = Backend ? Backend->FindTask(Activity) : nullptr;
    if (!Task || Value < 1 || Value > TNumericLimits<int8>::Max()) return false;
    Task->SetPriority(static_cast<uint8>(Value));
    Backend->bScoresDirty = true;
    return true;
}

bool UCommonAIAgentComponent::SetTaskResistance(FGameplayTag Activity, float Value)
{
    UCommonAITaskAdapter* Task = Backend ? Backend->FindTask(Activity) : nullptr;
    if (!Task || !FMath::IsFinite(Value) || Value < 0.0f || Value > 1.0f) return false;
    Task->SetReplaceResistance(Value);
    Backend->bScoresDirty = true;
    return true;
}

bool UCommonAIAgentComponent::SetTaskInterruptible(FGameplayTag Activity, bool bInterruptible)
{
    UCommonAITaskAdapter* Task = Backend ? Backend->FindTask(Activity) : nullptr;
    if (!Task || (Task->IsFallback() && !bInterruptible)) return false;
    Task->SetUtilityInterruptible(bInterruptible);
    return true;
}

bool UCommonAIAgentComponent::SetTaskMomentum(FGameplayTag Activity, int32 Maximum, int32 ReductionPercent, float DecaySeconds)
{
    UCommonAITaskAdapter* Task = Backend ? Backend->FindTask(Activity) : nullptr;
    if (!Task || Maximum < 0 || Maximum > 50 || ReductionPercent < 0 || ReductionPercent > 100
        || !FMath::IsFinite(DecaySeconds) || DecaySeconds < 0.0f) return false;
    Task->SetMomentumPolicy(Maximum, ReductionPercent, DecaySeconds);
    Backend->bScoresDirty = true;
    return true;
}
FGuid UCommonAIAgentComponent::AddTagContribution(UObject* Source, const FGameplayTagContainer& Tags)
{
    if (!IsValid(Source) || Tags.IsEmpty()) return {};
    const FGameplayTagContainer Previous = GetEffectiveTags();
    FCommonAITagContribution& Entry = TagContributions.AddDefaulted_GetRef();
    Entry.Id = FGuid::NewGuid();
    Entry.Source = Source;
    Entry.Tags = Tags;
    if (Backend && Previous != GetEffectiveTags()) Backend->NotifyEligibilityChanged();
    return Entry.Id;
}

bool UCommonAIAgentComponent::RemoveTagContribution(UObject* Source, FGuid Contribution)
{
    const FGameplayTagContainer Previous = GetEffectiveTags();
    const bool bRemoved = TagContributions.RemoveAll([Source, Contribution](const FCommonAITagContribution& Entry)
    { return Entry.Id == Contribution && Entry.Source.Get() == Source; }) > 0;
    if (Backend && bRemoved && Previous != GetEffectiveTags()) Backend->NotifyEligibilityChanged();
    return bRemoved;
}

FGameplayTagContainer UCommonAIAgentComponent::GetEffectiveTags() const
{
    FGameplayTagContainer Result;
    for (const FCommonAITagContribution& Entry : TagContributions)
        if (Entry.Source.IsValid()) Result.AppendTags(Entry.Tags);
    return Result;
}

void UCommonAIAgentComponent::BeginMontageTagWindow(USkeletalMeshComponent* Mesh, UAnimNotifyState* Notify,
    int32 MontageInstanceId, const FGameplayTagContainer& Tags)
{
    if (!Mesh || !Notify || MontageInstanceId == INDEX_NONE) return;
    EndMontageTagWindow(Mesh, Notify, MontageInstanceId);
    FMontageWindow Window;
    Window.Mesh = Mesh;
    Window.AnimInstance = Mesh->GetAnimInstance();
    Window.Notify = Notify;
    Window.MontageInstanceId = MontageInstanceId;
    Window.Contribution = AddTagContribution(Mesh, Tags);
    MontageWindows.Add(Window);
}

void UCommonAIAgentComponent::EndMontageTagWindow(USkeletalMeshComponent* Mesh, UAnimNotifyState* Notify, int32 MontageInstanceId)
{
    for (int32 Index = MontageWindows.Num() - 1; Index >= 0; --Index)
    {
        const FMontageWindow& Window = MontageWindows[Index];
        if (Window.Mesh.Get() == Mesh && Window.Notify.Get() == Notify && Window.MontageInstanceId == MontageInstanceId)
        {
            RemoveTagContribution(Mesh, Window.Contribution);
            MontageWindows.RemoveAt(Index);
        }
    }
}

void UCommonAIAgentComponent::SetExecutionSnapshot(ECommonAIPhase Phase, const FCommonAIInteractionRequest& Request, UObject* Target, FGameplayTag Reason)
{
    if (Snapshot.Phase != ECommonAIPhase::Cancelling && Phase == ECommonAIPhase::Cancelling)
    {
        Snapshot.CancellationElapsedSeconds = 0.0f;
        Snapshot.CancellationReason = Reason;
        CancellingContribution = AddTagContribution(this, FGameplayTagContainer(CommonAITags::State_Busy_Cancelling));
    }
    Snapshot.Phase = Phase;
    Snapshot.Activity = Request.Activity;
    Snapshot.Execution = Request.Execution;
    Snapshot.Interaction = Request;
    Snapshot.Target = Target;
    QueueActivityProgress();
}

void UCommonAIAgentComponent::SetCurrentAction(FGameplayTag Action, FGuid Invocation)
{
    Snapshot.CurrentAction = Action;
    Snapshot.Invocation = Invocation;
}

void UCommonAIAgentComponent::QueueActivityStarted(const FCommonAIInteractionRequest& Request)
{
    FCommonAIActivityEventRecord& Event = ActivityEvents.AddDefaulted_GetRef();
    Event.Request = Request;
    Event.Result.Status = ECommonAIInteractionStatus::Running;
    Event.Kind = ECommonAIActivityEventKind::Started;
}

void UCommonAIAgentComponent::QueueActivityProgress()
{
    if (!Snapshot.Execution.IsValid() || !Snapshot.CurrentAction.IsValid()) return;
    FCommonAIActivityProgress Progress;
    Progress.Execution = Snapshot.Execution;
    Progress.Operation = Snapshot.Interaction.Operation;
    Progress.Invocation = Snapshot.Invocation;
    Progress.Activity = Snapshot.Activity;
    Progress.Action = Snapshot.CurrentAction;
    Progress.Phase = Snapshot.Phase;
    Progress.Target = Snapshot.Target;
    if (!ActivityEvents.IsEmpty() && ActivityEvents.Last().Kind == ECommonAIActivityEventKind::Progress)
    {
        const FCommonAIActivityProgress& Previous = ActivityEvents.Last().Progress;
        if (Previous.Execution == Progress.Execution && Previous.Operation == Progress.Operation
            && Previous.Invocation == Progress.Invocation && Previous.Action == Progress.Action
            && Previous.Phase == Progress.Phase && Previous.Target == Progress.Target) return;
    }
    FCommonAIActivityEventRecord& Event = ActivityEvents.AddDefaulted_GetRef();
    Event.Kind = ECommonAIActivityEventKind::Progress;
    Event.Progress = Progress;
}

void UCommonAIAgentComponent::QueueCommandEvent(const FCommonAICommandSnapshot& Command)
{
    CommandEvents.Add(Command);
}

void UCommonAIAgentComponent::RecordExecutionResult(const FCommonAIInteractionRequest& Request, const FCommonAIInteractionResult& Result)
{
    FCommonAIActivityEventRecord& Event = ActivityEvents.AddDefaulted_GetRef();
    Event.Request = Request;
    Event.Result = Result;
    if (Snapshot.Execution != Request.Execution) return;
    Snapshot.LastResult = Result;
    Snapshot.Phase = ECommonAIPhase::Idle;
    Snapshot.Target.Reset();
    Snapshot.Execution.Invalidate();
    Snapshot.Interaction = {};
    Snapshot.Invocation.Invalidate();
    Snapshot.CurrentAction = {};
    Snapshot.Activity = {};
    Snapshot.bCancellationOverdue = false;
    RemoveTagContribution(this, CancellingContribution);
    CancellingContribution.Invalidate();
}

FCommonAIAgentSnapshot UCommonAIAgentComponent::GetSnapshot() const
{
    FCommonAIAgentSnapshot Result = Snapshot;
    Result.bProfileChangePending = PendingProfile != nullptr;
    Result.ProfileTransition = PendingProfileMode;
    if (Backend)
    {
        Result.ProfileId = Backend->GetProfile() ? Backend->GetProfile()->ProfileId : FGameplayTag();
        Result.InputGeneration = Backend->GetInputGeneration();
        Result.Tasks = Backend->TaskSnapshots();
        Result.Considerations = Backend->InputSnapshots();
        Result.Services = Backend->ServiceSnapshots();
        Result.Command = Backend->GetCommand();
        Result.bScoringPaused = Backend->GetDataManager() && Backend->GetDataManager()->IsScoringPaused();
        Result.bScoresDirty = Backend->bScoresDirty;
    }
    return Result;
}

void UCommonAIAgentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bEndingPlay) return;
    for (int32 Index = MontageWindows.Num() - 1; Index >= 0; --Index)
    {
        const FMontageWindow& Window = MontageWindows[Index];
        if (!Window.Mesh.IsValid() || !Window.AnimInstance.IsValid()
            || Window.Mesh->GetAnimInstance() != Window.AnimInstance.Get()
            || FindAgent(Window.Mesh->GetOwner()) != this
            || !Window.AnimInstance->GetMontageInstanceForID(Window.MontageInstanceId))
        {
            RemoveTagContribution(Window.Mesh.Get(), Window.Contribution);
            MontageWindows.RemoveAt(Index);
        }
    }
    const int32 RemovedTags = TagContributions.RemoveAll([](const FCommonAITagContribution& Entry) { return !Entry.Source.IsValid(); });
    ScoringHolds.RemoveAll([](const FControlHold& Entry) { return !Entry.Source.IsValid(); });
    const int32 RemovedLocks = TaskHolds.RemoveAll([](const FControlHold& Entry) { return !Entry.Source.IsValid(); });
    if (Backend && (RemovedTags || RemovedLocks)) Backend->NotifyEligibilityChanged();
    TArray<FGameplayTag> LostSources;
    for (const auto& Source : InputSources) if (!Source.Value.IsValid()) LostSources.Add(Source.Key);
    for (const FGameplayTag Source : LostSources)
    {
        InputSources.Remove(Source);
        InputSourceRevisions.Remove(Source);
        if (Backend) Backend->InvalidateSource(Source);
    }
    if (Snapshot.Phase == ECommonAIPhase::Cancelling)
    {
        Snapshot.CancellationElapsedSeconds += DeltaTime;
        Snapshot.bCancellationOverdue = Profile && Profile->CancellationWarningSeconds > 0.0f
            && Snapshot.CancellationElapsedSeconds >= Profile->CancellationWarningSeconds;
    }
    const int32 EventCount = ActivityEvents.Num();
    TArray<FCommonAICommandSnapshot> Commands = MoveTemp(CommandEvents);
    CommandEvents.Reset();
    for (int32 Index = 0; Index < EventCount; ++Index)
    {
        if (bEndingPlay) break;
        const FCommonAIActivityEventRecord Event = ActivityEvents[Index];
        if (Event.Kind == ECommonAIActivityEventKind::Started) OnActivityStarted.Broadcast(Event.Request);
        else if (Event.Kind == ECommonAIActivityEventKind::Progress) OnActivityProgress.Broadcast(Event.Progress);
        else OnActivityEnded.Broadcast(Event.Request, Event.Result);
    }
    if (!bEndingPlay) ActivityEvents.RemoveAt(0, EventCount);
    for (const FCommonAICommandSnapshot& Command : Commands)
        if (!bEndingPlay) OnCommandChanged.Broadcast(Command);
    if (bEndingPlay) return;
    if (Backend) Backend->Pump();
    ApplyPendingOperation();
    ObservationElapsedSeconds += DeltaTime;
    if (Backend && Backend->GetProfile() && Snapshot.bEnabled
        && Backend->GetProfile()->ObservationIntervalSeconds > 0.0f
        && ObservationElapsedSeconds >= Backend->GetProfile()->ObservationIntervalSeconds)
    {
        ObservationElapsedSeconds = 0.0f;
        Backend->Observe();
        Snapshot = GetSnapshot();
    }

}

void UCommonAIAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    if (AAIController* Controller = Cast<AAIController>(GetOwner()))
        Controller->OnPossessedPawnChanged.RemoveDynamic(this, &UCommonAIAgentComponent::HandlePawnChanged);
    if (Backend) Backend->DestroyComponent();
    Backend = nullptr;
    MontageWindows.Empty();
    TagContributions.Empty();
    ScoringHolds.Empty();
    TaskHolds.Empty();
    InputSources.Empty();
    InputSourceRevisions.Empty();
    ExecutionBindings.Empty();
    ActivityEvents.Empty();
    CommandEvents.Empty();
    Snapshot.bEnabled = false;
    Snapshot.Phase = ECommonAIPhase::Idle;
    Super::EndPlay(EndPlayReason);
}


