#include "Athena/CommonAIBackend.h"
#include "Activities/CommonAIActivities.h"
#include "CommonAIAgentComponent.h"
#include "CommonAITags.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "Evaluation/CommonAIEvaluator.h"
#include "Evaluation/CommonAIServiceBehavior.h"
#include "AthenaAIAgentPreset.h"
#include "AIController.h"
#include "Engine/CurveTable.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "VisualLogger/VisualLogger.h"

namespace
{
    UCommonAIBackend* BackendOf(const UAthenaAIDataManager* Data)
    {
        return Cast<UCommonAIBackend>(Data->GetOwnerAAIComponent());
    }

    EAServiceConditionType VendorComparison(ECommonAIComparison Comparison)
    {
        switch (Comparison)
        {
        case ECommonAIComparison::Less: return EAServiceConditionType::Less;
        case ECommonAIComparison::LessEqual: return EAServiceConditionType::LessEqual;
        case ECommonAIComparison::Equal: return EAServiceConditionType::Equal;
        case ECommonAIComparison::Greater: return EAServiceConditionType::Greater;
        default: return EAServiceConditionType::GreaterEqual;
        }
    }
}

float UCommonAIDataManager::CalculateConsiderationScore(const FGameplayTag& TaskTag, const FGameplayTag& ConsiderationTag,
    UCurveTable* CurveTable, float ModificationFactor) const
{
    const UAthenaAITask* Task = OwnerAAIComponent->GetTaskByTag(TaskTag);
    const UCommonAIBackend* Backend = BackendOf(this);
    const int32 Count = Task ? Task->GetConsiderations().Num() : 0;
    const float CorrectedFactor = Count > 0 ? 1.0f - 1.0f / static_cast<float>(Count) : 0.0f;
    const float Score = Count > 0 && Backend && Backend->IsTaskEligible(TaskTag)
        ? Super::CalculateConsiderationScore(TaskTag, ConsiderationTag, CurveTable, CorrectedFactor) : 0.0f;
    FCommonAICurveSnapshot Snapshot;
    Snapshot.Consideration = ConsiderationTag;
    const FAAIConsideration* Value = ConsdrsContainer.Find(ConsiderationTag);
    Snapshot.NormalizedInput = Value ? Value->Value : 0.0f;
    Snapshot.Contribution = Score;
    Contributions.FindOrAdd(TaskTag).Add(Snapshot);
    return Score;
}

bool UCommonAIDataManager::CanReplaceCurrentTask(const UAthenaAITask* NewTask)
{
    UCommonAIBackend* Backend = BackendOf(this);
    if (!Backend || !Backend->AllowsUtilityReplacement() || NewTask == OwnerAAIComponent->GetCurrentTask()) return false;
    if (!Super::CanReplaceCurrentTask(NewTask)) return false;
    if (UCommonAITaskAdapter* Current = Backend->GetRunningTask())
    {
        if (Current->GetDefinition().bUtilityInterruptible) Current->RequestCancellation(CommonAITags::Reason_Replaced);
        return false;
    }
    return true;
}

void UCommonAIDataManager::CalculateTasksScore()
{
    UCommonAIBackend* Backend = BackendOf(this);
    if (!Backend || IsScoringPaused() || Backend->bInCallback || Backend->bInScoring
        || (!Backend->GetRunningTask() && !Backend->AllowsUtilityReplacement())) return;
    TRACE_CPUPROFILER_EVENT_SCOPE(CommonAI_ScoreTasks);
    TGuardValue<bool> Guard(Backend->bInScoring, true);
    Contributions.Reset();
    Super::CalculateTasksScore();
    Backend->bScoresDirty = false;
    UCommonAITaskAdapter* Current = Backend->GetRunningTask();
    if (Current && !Current->IsFallback() && Current->GetDefinition().bUtilityInterruptible && Backend->AllowsUtilityReplacement())
    {
        const bool bCandidate = OwnerAAIComponent->GetTasks().ContainsByPredicate(
            [](const UAthenaAITask* Task) { return Task && Task->GetScore() > 0.0f; });
        if (!bCandidate) Current->RequestCancellation(CommonAITags::Reason_Replaced);
    }
}

void UCommonAIDataManager::BeginInputBatch()
{
    ++BatchDepth;
}

void UCommonAIDataManager::UpdateServices(const FGameplayTag& Id, float Value) const
{
    if (BatchDepth > 0) { ChangedInputs.Add(Id); return; }
    Super::UpdateServices(Id, Value);
}

void UCommonAIDataManager::EndInputBatch()
{
    check(BatchDepth > 0);
    if (--BatchDepth > 0) return;
    UCommonAIBackend* Backend = BackendOf(this);
    if (!Backend) return;
    {
        TGuardValue<bool> Guard(Backend->bPublishingInputs, true);
        for (const FGameplayTag Id : ChangedInputs)
            if (const FAAIConsideration* Value = ConsdrsContainer.Find(Id)) Super::UpdateServices(Id, Value->Value);
        ChangedInputs.Reset();
    }
    Backend->CommitServiceTransitions();
    Backend->ReconcileServices();
}

void UCommonAIServiceAdapter::Configure(const FCommonAIServiceDefinition& InDefinition)
{
    Definition = InDefinition;
    TickInterval = Definition.IntervalSeconds;
    Conditions.Reset();
    for (const FCommonAIServiceCondition& Entry : Definition.Conditions)
    {
        FAServiceCondition Condition;
        Condition.ConditionType = VendorComparison(Entry.Comparison);
        Condition.Value = Entry.NormalizedThreshold;
        Conditions.Add(Entry.Consideration, Condition);
    }
    if (Definition.Behavior) Behavior = DuplicateObject<UCommonAIServiceBehavior>(Definition.Behavior, this);
}

void UCommonAIServiceAdapter::Initialize(UAthenaAIDataManager* DataManager, AAIController* InController)
{
    Super::Initialize(DataManager, InController);
    UCommonAIBackend* Backend = BackendOf(DataManager);
    Agent = Backend ? Backend->GetAgent() : nullptr;
    Generation = Backend ? Backend->GetInputGeneration() : FGuid();
    if (Agent.IsValid() && Behavior) Agent->RegisterInputSource(Definition.Id, Behavior);
}

void UCommonAIServiceAdapter::StartService()
{
    UCommonAIBackend* Backend = BackendOf(AAIDataManager);
    if (!Backend || !Agent.IsValid() || !Backend->IsEnabled()) return;
    if (Backend->bPublishingInputs) { bTransitionPending = true; bDesiredRunning = true; return; }
    if (!Backend->AreConditionsAvailable(Definition)) { StopService(); return; }
    const bool bWasRunning = IsRunning();
    Super::StartService();
    if (!bWasRunning && IsRunning())
    {
        Generation = Backend->GetInputGeneration();
        LastTickTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        TGuardValue<bool> Guard(Backend->bInCallback, true);
        if (Behavior) Behavior->OnStarted(Agent.Get(), Definition.Id, Generation);
        Backend->EvaluateInputs(Definition.EvaluatedInputs);
    }
}

void UCommonAIServiceAdapter::StopService()
{
    UCommonAIBackend* Backend = AAIDataManager ? BackendOf(AAIDataManager) : nullptr;
    if (Backend && Backend->bPublishingInputs) { bTransitionPending = true; bDesiredRunning = false; return; }
    const bool bWasRunning = IsRunning();
    Super::StopService();
    if (bWasRunning && Backend && Agent.IsValid() && Behavior)
    {
        TGuardValue<bool> Guard(Backend->bInCallback, true);
        Behavior->OnStopped(Agent.Get(), Definition.Id, Generation);
        Agent->ReleaseOwnedState(Behavior, false);
    }
}

void UCommonAIServiceAdapter::ServiceTick()
{
    UCommonAIBackend* Backend = BackendOf(AAIDataManager);
    if (!Backend || !Agent.IsValid() || !Backend->IsEnabled() || Generation != Backend->GetInputGeneration()) { StopService(); return; }
    if (!Backend->AreConditionsAvailable(Definition)) { StopService(); return; }
    TRACE_CPUPROFILER_EVENT_SCOPE(CommonAI_Service);
    TGuardValue<bool> Guard(Backend->bInCallback, true);
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : LastTickTime;
    if (Behavior) Behavior->OnTick(Agent.Get(), Definition.Id, Generation, static_cast<float>(FMath::Max(0.0, Now - LastTickTime)));
    LastTickTime = Now;
    Backend->EvaluateInputs(Definition.EvaluatedInputs);
}

void UCommonAIServiceAdapter::CommitTransition()
{
    if (!bTransitionPending) return;
    bTransitionPending = false;
    if (bDesiredRunning) StartService();
    else StopService();
}

void UCommonAIServiceAdapter::Reconcile()
{
    UCommonAIBackend* Backend = BackendOf(AAIDataManager);
    if (!Backend || !Backend->IsEnabled()) return;
    if (!Backend->AreConditionsAvailable(Definition)) StopService();
    else if (AAIDataManager->IsScoringPaused() && Backend->GetProfile()->bPauseAffectsServices) return;
    else if (Conditions.IsEmpty()) StartService();
}

void UCommonAIServiceAdapter::Cleanup()
{
    bTransitionPending = false;
    StopService();
    if (Agent.IsValid() && Behavior) Agent->UnregisterInputSource(Definition.Id, Behavior);
    Super::Cleanup();
}

FCommonAIServiceSnapshot UCommonAIServiceAdapter::Snapshot() const
{
    FCommonAIServiceSnapshot Result;
    Result.Id = Definition.Id;
    Result.bRunning = IsRunning();
    Result.IntervalSeconds = TickInterval;
    for (const FCommonAIServiceCondition& Entry : Definition.Conditions)
    {
        FCommonAIServiceConditionSnapshot State;
        State.Definition = Entry;
        if (const FAServiceCondition* Condition = Conditions.Find(Entry.Consideration)) State.bMet = Condition->bConditionMet;
        Result.Conditions.Add(State);
    }
    return Result;
}

UCommonAIBackend::UCommonAIBackend()
{
    bEnableSystemOnPawnPossessed = false;
    bResumeScoringOnTaskTerminated = false;
    bCalculateScoresOnTaskTerminated = false;
}

bool UCommonAIBackend::Prepare(UCommonAIAgentComponent* InAgent, const UCommonAIProfile* Profile)
{
    if (!InAgent || !Profile || !Profile->ValidateProfile().IsEmpty()) return false;
    Agent = InAgent;
    RuntimeProfile = DuplicateObject<UCommonAIProfile>(Profile, this);
    bScorePausingAffectsServices = RuntimeProfile->bPauseAffectsServices;
    InputGeneration = FGuid::NewGuid();
    UAthenaAIAgentPreset* Preset = NewObject<UAthenaAIAgentPreset>(this);
    Preset->TaskNameShortener = Preset->ConsiderationNameShortener = Preset->ActionNameShortener = 0;
    Preset->Name = RuntimeProfile->ProfileId.GetTagName();
    Preset->ConsdrCurveTable = NewObject<UCurveTable>(Preset);
    for (const FCommonAIConsiderationDefinition& Input : RuntimeProfile->Considerations)
    {
        FAAIPresetConsiderationEntry Entry;
        Entry.ConsiderationTag = Input.Id;
        Preset->Considerations.Add(Entry);
    }
    for (const FCommonAITaskDefinition& Definition : RuntimeProfile->Tasks)
    {
        UCommonAITaskAdapter* Task = NewObject<UCommonAITaskAdapter>(Preset);
        Task->Configure(Definition);
        Preset->Tasks.Add(Task);
        for (const FCommonAICurveBinding& Binding : Definition.Considerations)
        {
            const FName Row = Preset->GetTableRowNameFast(Definition.Activity, Binding.Consideration);
            const FRealCurve* Curve = Binding.Curve.CurveTable->FindCurveUnchecked(Binding.Curve.RowName);
            // Backend uses rich curves consistently; a simple source is copied without changing its interpolation.
            FRichCurve& Target = Preset->ConsdrCurveTable->AddRichCurve(Row);
            if (Binding.Curve.CurveTable->GetCurveTableMode() == ECurveTableMode::RichCurves)
                Target = *static_cast<const FRichCurve*>(Curve);
            else
            {
                const FSimpleCurve* Simple = static_cast<const FSimpleCurve*>(Curve);
                for (const FSimpleCurveKey& Key : Simple->GetConstRefOfKeys())
                {
                    const FKeyHandle Handle = Target.AddKey(Key.Time, Key.Value);
                    Target.SetKeyInterpMode(Handle, Simple->GetKeyInterpMode());
                }
                Target.PreInfinityExtrap = Simple->PreInfinityExtrap;
                Target.PostInfinityExtrap = Simple->PostInfinityExtrap;
                Target.SetDefaultValue(Simple->GetDefaultValue());
            }
        }
    }
    UCommonAITaskAdapter* Fallback = NewObject<UCommonAITaskAdapter>(Preset);
    Fallback->Configure(RuntimeProfile->Fallback, true);
    Preset->FallbackTask = Fallback;
    for (const FCommonAIServiceDefinition& Definition : RuntimeProfile->Services)
    {
        UCommonAIServiceAdapter* Service = NewObject<UCommonAIServiceAdapter>(Preset);
        Service->Configure(Definition);
        Preset->Services.Add(Service);
    }
    AIPreset = Preset;
    return true;
}

void UCommonAIBackend::InitializeComponent()
{
    Super::InitializeComponent();
    if (AgentController) AgentController->OnPossessedPawnChanged.RemoveDynamic(this, &UCommonAIBackend::HandlePawnChanged);
    ResetInputs();
}

void UCommonAIBackend::CreateDataManager()
{
    AIDataManager = NewObject<UCommonAIDataManager>(this);
}

void UCommonAIBackend::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    if (AgentController) DisableAthenaAI(AgentController->GetPawn());
    // The vendor clears scored tasks/services but leaves its separately duplicated fallback out of that loop.
    if (FallbackTask)
    {
        FallbackTask->Cleanup();
        if (GetWorld()) GetWorld()->GetTimerManager().ClearAllTimersForObject(FallbackTask);
        FallbackTask->MarkAsGarbage();
        FallbackTask = nullptr;
    }
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UCommonAIBackend::ResetInputs()
{
    if (!AIDataManager || !RuntimeProfile) return;
    InputGeneration = FGuid::NewGuid();
    InputStates.Reset();
    QueuedInputs.Reset();
    for (const FCommonAIConsiderationDefinition& Input : RuntimeProfile->Considerations)
    {
        AIDataManager->SetConsiderationDefaults(Input.Id, Input.Maximum, Input.Minimum, Input.InitialValue);
        InputStates.FindOrAdd(Input.Id).Status = Input.bHasInitialValue ? ECommonAIValueStatus::Value : ECommonAIValueStatus::Unavailable;
    }
    bScoresDirty = true;
    bRescorePending = RuntimeProfile->ScoringMode != ECommonAIScoringMode::Manual;
}

bool UCommonAIBackend::ActivateAgent()
{
    bTransitionPending = false;
    if (!IsEnabled() && !EnableAthenaAI(false)) return false;
    RefreshScoringHold();
    Observe();
    return true;
}

void UCommonAIBackend::RefreshScoringHold()
{
    if (!AIDataManager) return;
    const bool bWasPaused = AIDataManager->IsScoringPaused();
    const bool bHeld = !IsEnabled() || (Agent.IsValid() && Agent->HasScoringHolds())
        || (ActiveCommand.Snapshot.Status == ECommonAICommandStatus::Running
            && ActiveCommand.Retention == ECommonAICommandRetention::HoldUntilCompletion);
    if (bHeld) AIDataManager->PauseScoring();
    else AIDataManager->ResumeScoring();
    if (bWasPaused && !bHeld) ReconcileServices(true);
}

bool UCommonAIBackend::AllowsUtilityReplacement() const
{
    return !bTransitionPending
        && !(PendingCommand.Snapshot.Status == ECommonAICommandStatus::Queued
            && PendingCommand.Transition == ECommonAITransitionMode::AfterNaturalCompletion);
}

UCommonAITaskAdapter* UCommonAIBackend::GetRunningTask() const
{
    UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(GetCurrentTask());
    return Task && Task->IsRunning() ? Task : nullptr;
}

UCommonAITaskAdapter* UCommonAIBackend::FindTask(FGameplayTag Activity) const
{
    for (UAthenaAITask* Task : GetTasks())
        if (Task && Task->GetTaskTag() == Activity) return Cast<UCommonAITaskAdapter>(Task);
    if (GetFallbackTask() && GetFallbackTask()->GetTaskTag() == Activity) return Cast<UCommonAITaskAdapter>(GetFallbackTask());
    return nullptr;
}

bool UCommonAIBackend::IsInputAvailable(FGameplayTag Id) const
{
    const FInputState* State = InputStates.Find(Id);
    if (!State || State->Status != ECommonAIValueStatus::Value) return false;
    const FCommonAIConsiderationDefinition* Input = RuntimeProfile ? RuntimeProfile->FindConsideration(Id) : nullptr;
    if (Input && Input->UnavailablePolicy == ECommonAIRetentionPolicy::Invalidate && Agent.IsValid()
        && Agent->GetInputSourceRevision(Input->SourceId).IsValid() && !Agent->GetInputSource(Input->SourceId)) return false;
    return true;
}

bool UCommonAIBackend::GetInput(FGameplayTag Id, FCommonAIConsiderationSnapshot& Snapshot) const
{
    const FCommonAIConsiderationDefinition* Definition = RuntimeProfile ? RuntimeProfile->FindConsideration(Id) : nullptr;
    FAAIConsideration Value;
    if (!Definition || !AIDataManager || !AIDataManager->GetConsiderationBP(Id, Value)) return false;
    Snapshot.Id = Id;
    Snapshot.SourceId = Definition->SourceId;
    Snapshot.RawValue = Value.RawValue;
    Snapshot.NormalizedValue = Value.Value;
    Snapshot.Minimum = Value.MinValue;
    Snapshot.Maximum = Value.MaxValue;
    const FInputState* State = InputStates.Find(Id);
    Snapshot.Status = State ? State->Status : ECommonAIValueStatus::Unavailable;
    if (Snapshot.Status == ECommonAIValueStatus::Value && !IsInputAvailable(Id)) Snapshot.Status = ECommonAIValueStatus::Unavailable;
    return true;
}

bool UCommonAIBackend::IsTaskEligible(FGameplayTag Activity) const
{
    const FCommonAITaskDefinition* Task = RuntimeProfile ? RuntimeProfile->FindTask(Activity) : nullptr;
    if (!Task || !Agent.IsValid() || Agent->HasTaskLock(Activity)) return false;
    if (!Task->RequiredAgentTags.IsEmpty() && !Task->RequiredAgentTags.Matches(Agent->GetEffectiveTags())) return false;
    for (const FCommonAICurveBinding& Binding : Task->Considerations)
        if (!IsInputAvailable(Binding.Consideration)) return false;
    return true;
}

bool UCommonAIBackend::AreConditionsAvailable(const FCommonAIServiceDefinition& Definition) const
{
    for (const FCommonAIServiceCondition& Condition : Definition.Conditions)
        if (!IsInputAvailable(Condition.Consideration)) return false;
    return true;
}

bool UCommonAIBackend::ValidateUpdates(UObject* Source, FGuid Generation, const TArray<FCommonAIInputUpdate>& Updates, bool bInternal) const
{
    if (Generation != InputGeneration || !RuntimeProfile || Updates.IsEmpty()) return false;
    TSet<FGameplayTag> Ids;
    for (const FCommonAIInputUpdate& Update : Updates)
    {
        const FCommonAIConsiderationDefinition* Input = RuntimeProfile->FindConsideration(Update.Id);
        if (!Input || Ids.Contains(Update.Id)) return false;
        Ids.Add(Update.Id);
        if (!bInternal && (Input->Route != ECommonAIInputRoute::External || !IsValid(Source)
            || !Agent.IsValid() || Agent->GetInputSource(Input->SourceId) != Source)) return false;
        if (Update.Status == ECommonAIValueStatus::Value && !FMath::IsFinite(Update.RawValue)) return false;
        if (Update.bOverrideRange && (!FMath::IsFinite(Update.Minimum) || !FMath::IsFinite(Update.Maximum) || Update.Minimum >= Update.Maximum))
            return false;
    }
    return true;
}

bool UCommonAIBackend::ApplyInputs(UObject* Source, FGuid Generation, const TArray<FCommonAIInputUpdate>& Updates, bool bRescore)
{
    if (!ValidateUpdates(Source, Generation, Updates, false)) return false;
    if (bInCallback || bInScoring || bPublishingInputs)
    {
        FQueuedInput& Queued = QueuedInputs.AddDefaulted_GetRef();
        Queued.Source = Source;
        Queued.Generation = Generation;
        Queued.Updates = Updates;
        Queued.bRescore = bRescore;
        for (const FCommonAIInputUpdate& Update : Updates)
        {
            const FGameplayTag Id = RuntimeProfile->FindConsideration(Update.Id)->SourceId;
            Queued.Producers.Add({ Id, Source, Agent->GetInputSourceRevision(Id), true });
        }
        return true;
    }
    PublishUpdates(Updates, bRescore);
    return true;
}

void UCommonAIBackend::PublishUpdates(const TArray<FCommonAIInputUpdate>& Updates, bool bRescore, bool bAutomatic)
{
    if (!AIDataManager) return;
    TRACE_CPUPROFILER_EVENT_SCOPE(CommonAI_PublishInputs);
    UCommonAIDataManager* Data = CastChecked<UCommonAIDataManager>(AIDataManager);
    bool bChanged = false;
    Data->BeginInputBatch();
    for (const FCommonAIInputUpdate& Update : Updates)
    {
        const FCommonAIConsiderationDefinition* Input = RuntimeProfile->FindConsideration(Update.Id);
        if (!Input) continue;
        FInputState& State = InputStates.FindOrAdd(Update.Id);
        if (Update.Status != ECommonAIValueStatus::Value)
        {
            if (Input->UnavailablePolicy != ECommonAIRetentionPolicy::HoldLast)
            {
                bChanged |= State.Status != Update.Status;
                State.Status = Update.Status;
            }
            continue;
        }
        FAAIConsideration Current;
        Data->GetConsiderationBP(Update.Id, Current);
        const bool bValueChanged = State.Status != ECommonAIValueStatus::Value || Current.RawValue != Update.RawValue
            || (Update.bOverrideRange && (Current.MinValue != Update.Minimum || Current.MaxValue != Update.Maximum));
        if (!bValueChanged) continue;
        bChanged = true;
        State.Status = ECommonAIValueStatus::Value;
        if (Update.bOverrideRange)
            Data->SetConsiderationValueNorm(Update.Id, Update.RawValue, Update.Maximum, Update.Minimum, false, true);
        else Data->SetConsiderationValue(Update.Id, Update.RawValue, false);
    }
    Data->EndInputBatch();
    bScoresDirty |= bChanged;
    if (bRescore && (!bAutomatic || (bChanged && RuntimeProfile->ScoringMode == ECommonAIScoringMode::OnInputChange))) bRescorePending = true;
}

bool UCommonAIBackend::EvaluateInput(UObject* Source, FGuid Generation, FGameplayTag Id, const FInstancedStruct& Context, bool bRescore)
{
    const FCommonAIConsiderationDefinition* Input = RuntimeProfile ? RuntimeProfile->FindConsideration(Id) : nullptr;
    if (!Input || Input->Route != ECommonAIInputRoute::Evaluator || !Input->Evaluator || Generation != InputGeneration
        || !Agent.IsValid() || !IsValid(Source) || Agent->GetInputSource(Input->SourceId) != Source) return false;
    FProducerStamp Stamp { Input->SourceId, Source, Agent->GetInputSourceRevision(Input->SourceId), true };
    FCommonAIEvaluationResult Result;
    {
        TGuardValue<bool> Guard(bInCallback, true);
        Result = Input->Evaluator->Evaluate(Context);
    }
    FCommonAIInputUpdate Update;
    Update.Id = Id;
    Update.Status = FMath::IsFinite(Result.RawValue) ? Result.Status : ECommonAIValueStatus::Unavailable;
    Update.RawValue = Result.RawValue;
    FQueuedInput& Queued = QueuedInputs.AddDefaulted_GetRef();
    Queued.Source = Source;
    Queued.Generation = Generation;
    Queued.Updates.Add(Update);
    Queued.Producers.Add(Stamp);
    Queued.bInternal = true;
    Queued.bRescore = bRescore;
    return true;
}

void UCommonAIBackend::NotifyEligibilityChanged()
{
    bScoresDirty = true;
    if (RuntimeProfile && RuntimeProfile->ScoringMode == ECommonAIScoringMode::OnInputChange) bRescorePending = true;
}

void UCommonAIBackend::EvaluateInputs(const FGameplayTagContainer& Inputs)
{
    if (!Agent.IsValid() || !RuntimeProfile) return;
    TArray<FCommonAIInputUpdate> Updates;
    TArray<FProducerStamp> Producers;
    for (const FGameplayTag Id : Inputs)
    {
        const FCommonAIConsiderationDefinition* Input = RuntimeProfile->FindConsideration(Id);
        if (!Input || !Input->Evaluator) continue;
        FCommonAIEvaluationResult Result;
        UObject* Provider = Agent->GetInputSource(Input->SourceId);
        Producers.Add({ Input->SourceId, Provider, Agent->GetInputSourceRevision(Input->SourceId), IsValid(Provider) });
        if (IsValid(Provider) && Provider->GetClass()->ImplementsInterface(UCommonAIObservationProvider::StaticClass()))
        {
            const FInstancedStruct Context = ICommonAIObservationProvider::Execute_ReadObservation(Provider, Agent.Get(), Id);
            Result = Input->Evaluator->Evaluate(Context);
        }
        FCommonAIInputUpdate& Update = Updates.AddDefaulted_GetRef();
        Update.Id = Id;
        Update.Status = Result.Status;
        Update.RawValue = Result.RawValue;
        if (!FMath::IsFinite(Update.RawValue)) Update.Status = ECommonAIValueStatus::Unavailable;
    }
    if (!Updates.IsEmpty())
    {
        FQueuedInput& Queued = QueuedInputs.AddDefaulted_GetRef();
        Queued.Source = this;
        Queued.Generation = InputGeneration;
        Queued.Updates = MoveTemp(Updates);
        Queued.Producers = MoveTemp(Producers);
        Queued.bInternal = true;
        Queued.bAutomatic = true;
    }
}

void UCommonAIBackend::Observe()
{
    if (!Agent.IsValid() || !RuntimeProfile || !AIDataManager || !IsEnabled()) return;
    TArray<FCommonAIInputUpdate> Updates;
    for (const FCommonAIConsiderationDefinition& Input : RuntimeProfile->Considerations)
    {
        if (Input.Route != ECommonAIInputRoute::Opportunity) continue;
        FCommonAIInteractionRequest Query;
        Query.Agent = AgentController;
        Query.Pawn = AgentController ? AgentController->GetPawn() : nullptr;
        Query.Activity = Input.OpportunityActivity;
        const FCommonAITaskDefinition* Task = RuntimeProfile->FindTask(Input.OpportunityActivity);
        UCommonAITaskAdapter* Current = GetRunningTask();
        const bool bOwns = Current && Current->GetRequest().Activity == Query.Activity && IsValid(Current->GetSite());
        const bool bAvailable = Task && (bOwns || (Agent->GetProvider()
            && Agent->GetProvider()->FindOpportunity(Query, Task->RequiredTargetCapabilities)));
        FCommonAIInputUpdate& Update = Updates.AddDefaulted_GetRef();
        Update.Id = Input.Id;
        Update.RawValue = bAvailable ? 1.0f : 0.0f;
    }
    if (!Updates.IsEmpty()) PublishUpdates(Updates, true, true);
    if (RuntimeProfile->ScoringMode == ECommonAIScoringMode::Interval) bRescorePending = true;
    Pump();
}

bool UCommonAIBackend::Rescore()
{
    RefreshScoringHold();
    if (!AIDataManager || AIDataManager->IsScoringPaused() || bInCallback || bInScoring || bPublishingInputs)
    {
        return false;
    }
    AIDataManager->ForceTaskScoring();
    return true;
}

void UCommonAIBackend::Pump()
{
    if (bInCallback || bInScoring || bPublishingInputs) return;
    TArray<FQueuedInput> Inputs = MoveTemp(QueuedInputs);
    QueuedInputs.Reset();
    for (const FQueuedInput& Input : Inputs)
    {
        const bool bCurrentProducers = Agent.IsValid() && Input.Producers.ContainsByPredicate([this](const FProducerStamp& Stamp)
        {
            return (Stamp.bExpectedAlive && !Stamp.Source.IsValid()) || Agent->GetInputSource(Stamp.Id) != Stamp.Source.Get()
                || Agent->GetInputSourceRevision(Stamp.Id) != Stamp.Revision;
        }) == false;
        if (bCurrentProducers && ValidateUpdates(Input.Source.Get(), Input.Generation, Input.Updates, Input.bInternal))
            PublishUpdates(Input.Updates, Input.bRescore, Input.bAutomatic);
    }
    if (bServiceReconcilePending) { bServiceReconcilePending = false; ReconcileServices(); }
    RefreshScoringHold();
    if (UCommonAITaskAdapter* Task = GetRunningTask())
        if (Task->IsCancelling()) Task->TryFinishCancellation();
    PumpCommand();
    if (bRescorePending && IsEnabled() && AllowsUtilityReplacement() && AIDataManager && !AIDataManager->IsScoringPaused())
    {
        bRescorePending = false;
        Rescore();
    }
}

void UCommonAIBackend::ReconcileServices(bool bRefreshConditions)
{
    if (bRefreshConditions && AIDataManager && (!AIDataManager->IsScoringPaused() || !RuntimeProfile->bPauseAffectsServices))
    {
        {
            TGuardValue<bool> Guard(bPublishingInputs, true);
            for (UAthenaAIService* Service : GetServices())
                for (const auto& Value : AIDataManager->GetAllConsiderations())
                    Service->UpdateServiceCondition(Value.Key, Value.Value.Value);
        }
        CommitServiceTransitions();
    }
    for (UAthenaAIService* Service : GetServices())
        if (UCommonAIServiceAdapter* Adapter = Cast<UCommonAIServiceAdapter>(Service)) Adapter->Reconcile();
}

void UCommonAIBackend::InvalidateSource(FGameplayTag SourceId)
{
    if (!RuntimeProfile) return;
    bool bChanged = false;
    for (const FCommonAIConsiderationDefinition& Input : RuntimeProfile->Considerations)
        if (Input.SourceId == SourceId && Input.UnavailablePolicy == ECommonAIRetentionPolicy::Invalidate)
        {
            FInputState& State = InputStates.FindOrAdd(Input.Id);
            bChanged |= State.Status != ECommonAIValueStatus::Unavailable;
            State.Status = ECommonAIValueStatus::Unavailable;
        }
    if (!bChanged) return;
    bScoresDirty = true;
    bRescorePending |= RuntimeProfile->ScoringMode == ECommonAIScoringMode::OnInputChange;
    if (bInCallback || bInScoring || bPublishingInputs) bServiceReconcilePending = true;
    else ReconcileServices();
}
void UCommonAIBackend::RestoreInputs(const TArray<FCommonAIConsiderationSnapshot>& Values)
{
    TArray<FCommonAIInputUpdate> Updates;
    for (const FCommonAIConsiderationSnapshot& Value : Values)
    {
        const FCommonAIConsiderationDefinition* Definition = RuntimeProfile->FindConsideration(Value.Id);
        if (!Definition || Definition->Route != ECommonAIInputRoute::External || Definition->SourceId != Value.SourceId) continue;
        FCommonAIInputUpdate& Update = Updates.AddDefaulted_GetRef();
        Update.Id = Value.Id;
        Update.Status = Value.Status;
        Update.RawValue = Value.RawValue;
    }
    if (!Updates.IsEmpty()) PublishUpdates(Updates, true);
}

void UCommonAIBackend::CommitServiceTransitions()
{
    for (UAthenaAIService* Service : GetServices())
        if (UCommonAIServiceAdapter* Adapter = Cast<UCommonAIServiceAdapter>(Service)) Adapter->CommitTransition();
}

void UCommonAIBackend::CancelCurrent(FGameplayTag Reason)
{
    if (UCommonAITaskAdapter* Task = GetRunningTask())
    {
        {
            TGuardValue<bool> Guard(bInCallback, true);
            Task->RequestCancellation(Reason);
        }
        if (!bInCallback && !bInScoring && !bPublishingInputs) Task->TryFinishCancellation();
    }
}

void UCommonAIBackend::HoldForTransition()
{
    bTransitionPending = true;
    CancelCommands(CommonAITags::Reason_ProfileChanged);
}

void UCommonAIBackend::EndCommand(FCommand& Command, ECommonAICommandStatus Status, FGameplayTag Reason)
{
    if (!Command.Snapshot.Id.IsValid()) return;
    Command.Snapshot.Status = Status;
    Command.Snapshot.Reason = Reason;
    LastCommand = Command.Snapshot;
    if (Agent.IsValid()) Agent->QueueCommandEvent(Command.Snapshot);
    CommandPayloads.Remove(Command.Snapshot.Id);
    Command = {};
}

void UCommonAIBackend::OnExecutionEnded(FGuid Execution, const FCommonAIInteractionResult& Result)
{
    if (ActiveCommand.Snapshot.Execution == Execution)
    {
        const ECommonAICommandStatus Status = Result.Status == ECommonAIInteractionStatus::Completed ? ECommonAICommandStatus::Completed
            : Result.Status == ECommonAIInteractionStatus::Cancelled ? ECommonAICommandStatus::Cancelled : ECommonAICommandStatus::Failed;
        EndCommand(ActiveCommand, Status, Result.Reason);
    }
    bRescorePending |= (RuntimeProfile->ScoringMode != ECommonAIScoringMode::Manual && RuntimeProfile->bRescoreAfterCompletion)
        || (Result.Status == ECommonAIInteractionStatus::Cancelled && Result.Reason == CommonAITags::Reason_Replaced);
    const UCommonAITaskAdapter* Finished = Cast<UCommonAITaskAdapter>(LastTask);
    if (IsEnabled() && Finished && Finished->IsFallback() && Result.Status == ECommonAIInteractionStatus::Failed && Agent.IsValid())
        Agent->StopAgent();
}

FGuid UCommonAIBackend::RequestCommand(FGameplayTag Activity, ECommonAITransitionMode Transition, ECommonAICommandRetention Retention, bool bUnlockRetry,
    const FInstancedStruct& Payload)
{
    FCommand Command;
    Command.Snapshot.Id = FGuid::NewGuid();
    Command.Snapshot.Activity = Activity;
    Command.Transition = Transition;
    Command.Retention = Retention;
    Command.bUnlockRetry = bUnlockRetry;
    UCommonAITaskAdapter* Task = FindTask(Activity);
    const FGuid Id = Command.Snapshot.Id;
    if (!IsEnabled() || bTransitionPending || !Task || !IsTaskEligible(Activity) || (Task->IsLocked() && !bUnlockRetry))
    {
        EndCommand(Command, ECommonAICommandStatus::Rejected, Task && Task->IsLocked() ? CommonAITags::Reason_TaskLocked : CommonAITags::Reason_InputUnavailable);
        return Id;
    }
    EndCommand(PendingCommand, ECommonAICommandStatus::Superseded, CommonAITags::Reason_Superseded);
    Command.Snapshot.Status = ECommonAICommandStatus::Queued;
    PendingCommand = Command;
    if (Payload.IsValid()) CommandPayloads.Add(Id, Payload);
    if (Agent.IsValid()) Agent->QueueCommandEvent(Command.Snapshot);
    if (Transition == ECommonAITransitionMode::CancelAtSafePoint) CancelCurrent(CommonAITags::Reason_Replaced);
    if (!bInCallback && !bInScoring) PumpCommand();
    return Id;
}

void UCommonAIBackend::PumpCommand()
{
    if (PendingCommand.Snapshot.Status != ECommonAICommandStatus::Queued || GetRunningTask()
        || bTransitionPending || bInCallback || bInScoring || !IsEnabled()) return;
    UCommonAITaskAdapter* Task = FindTask(PendingCommand.Snapshot.Activity);
    if (!Task || !IsTaskEligible(PendingCommand.Snapshot.Activity) || (Task->IsLocked() && !PendingCommand.bUnlockRetry))
    {
        EndCommand(PendingCommand, ECommonAICommandStatus::Rejected, CommonAITags::Reason_InputUnavailable);
        return;
    }
    if (PendingCommand.bUnlockRetry) { Task->SetLocked(false); LockedTasks.Remove(Task); }
    ActiveCommand = PendingCommand;
    PendingCommand = {};
    ActiveCommand.Snapshot.Status = ECommonAICommandStatus::Running;
    RefreshScoringHold();
    {
        TGuardValue<bool> Guard(bInCallback, true);
        StartTask(Task);
    }
    ActiveCommand.Snapshot.Execution = Task->GetRequest().Execution;
    if (!Task->IsRunning())
    {
        EndCommand(ActiveCommand, ECommonAICommandStatus::Failed, CommonAITags::Reason_InvalidConfiguration);
        return;
    }
    if (Agent.IsValid()) Agent->QueueCommandEvent(ActiveCommand.Snapshot);
}

FInstancedStruct UCommonAIBackend::GetStartPayload(FGameplayTag Activity) const
{
    if (ActiveCommand.Snapshot.Status != ECommonAICommandStatus::Running || ActiveCommand.Snapshot.Activity != Activity) return {};
    const FInstancedStruct* Payload = CommandPayloads.Find(ActiveCommand.Snapshot.Id);
    return Payload ? *Payload : FInstancedStruct();
}

void UCommonAIBackend::CancelCommands(FGameplayTag Reason)
{
    EndCommand(PendingCommand, ECommonAICommandStatus::Cancelled, Reason);
    // Active execution owns its hold until its real terminal acknowledgement.
}

FCommonAICommandSnapshot UCommonAIBackend::GetCommand() const
{
    if (PendingCommand.Snapshot.Id.IsValid()) return PendingCommand.Snapshot;
    if (ActiveCommand.Snapshot.Id.IsValid()) return ActiveCommand.Snapshot;
    return LastCommand;
}

TArray<FCommonAITaskSnapshot> UCommonAIBackend::TaskSnapshots() const
{
    TArray<FCommonAITaskSnapshot> Result;
    TArray<UAthenaAITask*> All = GetTasks();
    All.Add(GetFallbackTask());
    const UCommonAIDataManager* Data = Cast<UCommonAIDataManager>(AIDataManager);
    for (UAthenaAITask* Native : All)
    {
        const UCommonAITaskAdapter* Task = Cast<UCommonAITaskAdapter>(Native);
        if (!Task) continue;
        FCommonAITaskSnapshot Snapshot;
        Snapshot.Activity = Task->GetTaskTag();
        Snapshot.Score = Task->GetScore();
        Snapshot.ScoreModifier = Task->GetScoreModifier();
        Snapshot.ReplaceResistance = Task->GetReplaceResistance();
        Snapshot.Priority = Task->GetPriority();
        Snapshot.Momentum = Task->GetCurrentMomentum();
        Snapshot.MaxMomentum = Task->GetMaxMomentum();
        Snapshot.bLocked = Task->IsLocked() || (Agent.IsValid() && Agent->HasTaskLock(Snapshot.Activity));
        Snapshot.bInterruptible = Task->GetDefinition().bUtilityInterruptible;
        Snapshot.bActive = Task->IsRunning();
        Snapshot.bFallback = Task->IsFallback();
        if (Snapshot.bLocked) Snapshot.Reason = CommonAITags::Reason_TaskLocked;
        else if (!IsTaskEligible(Snapshot.Activity)) Snapshot.Reason = CommonAITags::Reason_InputUnavailable;
        if (Data)
            if (const TArray<FCommonAICurveSnapshot>* Curves = Data->FindContributions(Snapshot.Activity)) Snapshot.Curves = *Curves;
        Result.Add(Snapshot);
    }
    return Result;
}

TArray<FCommonAIConsiderationSnapshot> UCommonAIBackend::InputSnapshots() const
{
    TArray<FCommonAIConsiderationSnapshot> Result;
    if (RuntimeProfile)
        for (const FCommonAIConsiderationDefinition& Definition : RuntimeProfile->Considerations)
        {
            FCommonAIConsiderationSnapshot Snapshot;
            if (GetInput(Definition.Id, Snapshot)) Result.Add(Snapshot);
        }
    return Result;
}

TArray<FCommonAIServiceSnapshot> UCommonAIBackend::ServiceSnapshots() const
{
    TArray<FCommonAIServiceSnapshot> Result;
    for (UAthenaAIService* Service : GetServices())
        if (const UCommonAIServiceAdapter* Adapter = Cast<UCommonAIServiceAdapter>(Service)) Result.Add(Adapter->Snapshot());
    return Result;
}

#if ENABLE_VISUAL_LOG
void UCommonAIBackend::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
    Super::GrabDebugSnapshot(Snapshot);
    if (!Agent.IsValid()) return;
    const FCommonAIAgentSnapshot State = Agent->GetSnapshot();
    FVisualLogStatusCategory Category(TEXT("CommonAI"));
    Category.Add(TEXT("Activity"), State.Activity.ToString());
    Category.Add(TEXT("Cancellation"), State.CancellationReason.ToString());
    Category.Add(TEXT("Execution"), State.Execution.ToString());
    Category.Add(TEXT("Dirty scores"), State.bScoresDirty ? TEXT("true") : TEXT("false"));
    Snapshot->Status.Add(Category);
}
#endif



