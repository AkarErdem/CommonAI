#include "Athena/CommonAIBackend.h"
#include "Activities/CommonAIActivities.h"
#include "CommonAIAgentComponent.h"
#include "CommonAIProfile.h"
#include "CommonAITags.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "AthenaAIAgentPreset.h"
#include "AIController.h"
#include "Engine/CurveTable.h"

bool UCommonAIDataManager::CanReplaceCurrentTask(const UAthenaAITask* NewTask)
{
    if (!Super::CanReplaceCurrentTask(NewTask)) return false;
    UCommonAIWorkTask* Current = Cast<UCommonAIWorkTask>(OwnerAAIComponent->GetCurrentTask());
    if (Current && Current->IsRunning() && Current != NewTask)
    {
        Current->RequestCancellation(CommonAITags::Reason_Replaced);
        return false;
    }
    return true;
}

void UCommonAIDataManager::CalculateTasksScore()
{
    if (IsScoringPaused()) return;
    Super::CalculateTasksScore();
    // The vendor fallback path bypasses CanReplaceCurrentTask. The running Work is
    // non-interruptible until its safe stop, so observe that no candidate scored above zero.
    UCommonAIWorkTask* Current = Cast<UCommonAIWorkTask>(OwnerAAIComponent->GetCurrentTask());
    if (Current && Current->IsRunning())
    {
        const bool bHasCandidate = OwnerAAIComponent->GetTasks().ContainsByPredicate(
            [](const UAthenaAITask* Task) { return Task && Task->GetScore() > 0.0f; });
        if (!bHasCandidate) Current->RequestCancellation(CommonAITags::Reason_Replaced);
    }
}

UCommonAIBackend::UCommonAIBackend()
{
    bEnableSystemOnPawnPossessed = false;
    bResumeScoringOnTaskTerminated = false;
    bCalculateScoresOnTaskTerminated = false;
}

bool UCommonAIBackend::Prepare(UCommonAIAgentComponent* InAgent, const UCommonAIProfile* Profile)
{
    if (!InAgent || !Profile || !Profile->IsUsable()) return false;
    Agent = InAgent;
    UAthenaAIAgentPreset* Preset = NewObject<UAthenaAIAgentPreset>(this);
    Preset->TaskNameShortener = Preset->ConsiderationNameShortener = Preset->ActionNameShortener = 0;
    Preset->Name = TEXT("CommonAI Work");
    UCommonAIWorkTask* Work = NewObject<UCommonAIWorkTask>(Preset);
    Work->Configure(Profile);
    Preset->Tasks.Add(Work);
    Preset->FallbackTask = NewObject<UCommonAIWaitTask>(Preset);
    FAAIPresetConsiderationEntry Entry;
    Entry.ConsiderationTag = CommonAITags::Consideration_WorkAvailable;
    Preset->Considerations.Add(Entry);
    // Project the authored availability curve onto this activity's vendor row identity.
    // A specialized activity tag can reuse the same profile curve without renaming asset rows.
    const FRealCurve* Curve = Profile->ConsiderationCurves->FindCurveUnchecked(Profile->AvailabilityCurveRow);
    Preset->ConsdrCurveTable = NewObject<UCurveTable>(Preset);
    const FName Row = Preset->GetTableRowNameFast(Profile->WorkActivity, Entry.ConsiderationTag);
    if (Profile->ConsiderationCurves->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
        Preset->ConsdrCurveTable->AddSimpleCurve(Row) = *static_cast<const FSimpleCurve*>(Curve);
    else
        Preset->ConsdrCurveTable->AddRichCurve(Row) = *static_cast<const FRichCurve*>(Curve);
    AIPreset = Preset;
    return true;
}

void UCommonAIBackend::InitializeComponent()
{
    Super::InitializeComponent();
    // The public component coordinates possession changes through the same drain as Stop/Profile.
    if (AgentController)
        AgentController->OnPossessedPawnChanged.RemoveDynamic(this, &UCommonAIBackend::HandlePawnChanged);
}

void UCommonAIBackend::CreateDataManager()
{
    AIDataManager = NewObject<UCommonAIDataManager>(this);
}

UCommonAIWorkTask* UCommonAIBackend::GetRunningWork() const
{
    UCommonAIWorkTask* Work = Cast<UCommonAIWorkTask>(GetCurrentTask());
    return Work && Work->IsRunning() ? Work : nullptr;
}

void UCommonAIBackend::Observe()
{
    if (!Agent.IsValid() || !Agent->Profile || !AIDataManager || !IsEnabled()) return;
    FCommonAIInteractionRequest Query;
    Query.Agent = AgentController;
    Query.Activity = Agent->Profile->WorkActivity;
    const bool bOwnsOpportunity = GetRunningWork() != nullptr;
    const bool bEligible = Agent->Profile->RequiredAgentTags.IsEmpty()
        || Agent->Profile->RequiredAgentTags.Matches(Agent->GetEffectiveTags());
    const bool bAvailable = bEligible && (bOwnsOpportunity || (Agent->GetProvider()
        && Agent->GetProvider()->FindOpportunity(Query, Agent->Profile->RequiredTargetCapabilities)));
    AIDataManager->SetConsiderationValue(CommonAITags::Consideration_WorkAvailable, bAvailable ? 1.0f : 0.0f, false);
    AIDataManager->ForceTaskScoring();
}
