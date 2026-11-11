#include "DailyLife/CommonAIDailyLifeProfile.h"
#include "CommonAIProfile.h"
#include "CommonAITags.h"
#include "Execution/CommonAIExecution.h"
#include "Engine/CurveTable.h"

namespace { constexpr float WeightNormalization = 100.0f; }

TArray<FString> UCommonAIDailyLifeProfile::ValidateProfile() const
{
    TArray<FString> Errors;
    auto Nonnegative = [](double Value) { return FMath::IsFinite(Value) && Value >= 0.0; };
    if (!Occupation.IsValid() || Activities.IsEmpty()) Errors.Add(TEXT("Occupation and at least one activity are required."));
    if (!Nonnegative(HungerPerHour) || !Nonnegative(FatiguePerHour) || !Nonnegative(ScheduleVariationHours)
        || !Nonnegative(UrgentNeedThreshold) || UrgentNeedThreshold > 1.0f
        || !FMath::IsFinite(ObservationIntervalSeconds) || ObservationIntervalSeconds <= 0.0f
        || !FMath::IsFinite(ExecutionIntervalSeconds) || ExecutionIntervalSeconds <= 0.0f
        || !FMath::IsFinite(TravelCostDistanceCentimeters) || TravelCostDistanceCentimeters <= 0.0f
        || !Nonnegative(AcceptanceRadiusCentimeters) || !FMath::IsFinite(TravelTimeoutHours) || TravelTimeoutHours <= 0.0f)
        Errors.Add(TEXT("Needs, cadence, schedule variation and movement settings must be finite and in range."));
    TSet<FGameplayTag> ActivityIds;
    TSet<FGameplayTag> InputIds;
    for (const FCommonAIDailyActivityDefinition& Entry : Activities)
    {
        if (!Entry.Activity.IsValid() || Entry.Activity == CommonAITags::Activity_Wait || ActivityIds.Contains(Entry.Activity))
            Errors.Add(TEXT("Activity tags must be valid, unique and distinct from the fallback."));
        ActivityIds.Add(Entry.Activity);
        if (Entry.Kind > ECommonAIDailyActivity::SiteActivity
            || (Entry.Kind == ECommonAIDailyActivity::Idle && Entry.bCompleteOnSiteSignal))
            Errors.Add(TEXT("The activity behavior and completion mode must be compatible."));
        for (const FGameplayTag Input : Entry.GetInputTags())
        {
            if (!Input.IsValid() || InputIds.Contains(Input)) Errors.Add(TEXT("Consideration tags must be valid and unique across the profile."));
            InputIds.Add(Input);
        }
        if (!Nonnegative(Entry.BaseWeight) || !Nonnegative(Entry.OutsideWindowPreference) || Entry.OutsideWindowPreference > 1.0f
            || !Nonnegative(Entry.NeedThreshold) || Entry.NeedThreshold >= 1.0f || !Nonnegative(Entry.SatisfiedNeed) || Entry.SatisfiedNeed > 1.0f
            || !Nonnegative(Entry.HungerRecoveryPerHour) || !Nonnegative(Entry.FatigueRecoveryPerHour)
            || !FMath::IsFinite(Entry.DurationHours) || Entry.DurationHours <= 0.0f
            || !Nonnegative(Entry.MinimumCommitmentHours) || (!Entry.bCompleteOnSiteSignal && Entry.MinimumCommitmentHours > Entry.DurationHours)
            || !Nonnegative(Entry.CooldownHours) || !Nonnegative(Entry.ReplaceResistance) || Entry.ReplaceResistance > 1.0f)
            Errors.Add(TEXT("Activity weights, needs and timing settings are outside supported ranges."));
        for (const FCommonAITimePreference& Time : Entry.PreferredTimes)
            if (!Nonnegative(Time.StartHour) || Time.StartHour > CommonAIDailyLife::HoursPerDay
                || !Nonnegative(Time.EndHour) || Time.EndHour > CommonAIDailyLife::HoursPerDay) Errors.Add(TEXT("Preferred hours must be in [0,24]."));
    }
    return Errors;
}

const FCommonAIDailyActivityDefinition* UCommonAIDailyLifeProfile::FindActivity(FGameplayTag Activity) const
{
    return Activities.FindByPredicate([Activity](const FCommonAIDailyActivityDefinition& Entry) { return Entry.Activity == Activity; });
}

UCommonAIProfile* UCommonAIDailyLifeProfile::CreateAgentProfile(UObject* Outer) const
{
    if (!Outer || !ValidateProfile().IsEmpty()) return nullptr;
    UCommonAIProfile* Result = NewObject<UCommonAIProfile>(Outer);
    Result->DisplayName = OccupationLabel;
    Result->ScoringMode = ECommonAIScoringMode::Manual;
    Result->ObservationIntervalSeconds = 0.0f;
    Result->bRescoreAfterCompletion = false;
    UCurveTable* Curves = NewObject<UCurveTable>(Result);
    FRichCurve& Identity = Curves->AddRichCurve(TEXT("Identity"));
    Identity.AddKey(0.0f, 0.0f);
    Identity.AddKey(1.0f, 1.0f);
    Result->AuthoringCurves = Curves;
    Result->Fallback.TickIntervalSeconds = ExecutionIntervalSeconds;
    for (const FCommonAIDailyActivityDefinition& Entry : Activities)
    {
        FCommonAITaskDefinition Task;
        Task.Activity = Entry.Activity;
        Task.Label = Entry.Label;
        Task.ScoreModifier = Entry.BaseWeight / WeightNormalization;
        Task.ReplaceResistance = Entry.ReplaceResistance;
        Task.RequiredTargetCapabilities = Entry.RequiredCapabilities;
        Task.TickIntervalSeconds = ExecutionIntervalSeconds;
        Task.UnlockDelaySeconds = ObservationIntervalSeconds;
        for (const FGameplayTag Id : Entry.GetInputTags())
        {
            FCommonAIConsiderationDefinition Input;
            Input.Id = Id;
            Input.SourceId = CommonAIDailyLife::SourceTag();
            Result->Considerations.Add(Input);
            FCommonAICurveBinding Binding;
            Binding.Consideration = Input.Id;
            Binding.Curve.CurveTable = Curves;
            Binding.Curve.RowName = TEXT("Identity");
            Task.Considerations.Add(Binding);
        }
        if (Entry.Kind == ECommonAIDailyActivity::Idle)
        {
            Task.Kind = ECommonAITaskKind::Custom;
            // Routine simulation time completes this wait; no animation or real-time timer is needed.
            Task.Execution = NewObject<UCommonAIWaitExecution>(Result);
        }
        else
        {
            const ECommonAIActionKind Kinds[] = { ECommonAIActionKind::FindOpportunity, ECommonAIActionKind::Reserve,
                ECommonAIActionKind::Move, ECommonAIActionKind::Interact, ECommonAIActionKind::Release };
            const FGameplayTag Tags[] = { CommonAITags::Action_Find, CommonAITags::Action_Reserve, CommonAITags::Action_Move,
                CommonAITags::Action_Interact, CommonAITags::Action_Release };
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Kinds); ++Index)
            {
                FCommonAIActionDefinition Action;
                Action.Kind = Kinds[Index];
                Action.Id = Tags[Index];
                Action.TickIntervalSeconds = ExecutionIntervalSeconds;
                Action.AcceptanceRadiusCentimeters = AcceptanceRadiusCentimeters;
                Task.Actions.Add(Action);
            }
        }
        Result->Tasks.Add(Task);
    }
    return Result->IsUsable() ? Result : nullptr;
}
