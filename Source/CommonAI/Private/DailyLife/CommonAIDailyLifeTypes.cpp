#include "DailyLife/CommonAIDailyLifeTypes.h"
#include "CommonAITags.h"

bool FCommonAITimePreference::Contains(double Hour, float OffsetHours) const
{
    const double LocalHour = FMath::Fmod(FMath::Fmod(Hour - OffsetHours, CommonAIDailyLife::HoursPerDay)
        + CommonAIDailyLife::HoursPerDay, CommonAIDailyLife::HoursPerDay);
    if (StartHour == EndHour) return true;
    return StartHour < EndHour ? LocalHour >= StartHour && LocalHour < EndHour : LocalHour >= StartHour || LocalHour < EndHour;
}

bool FCommonAIDailyLifeState::IsValid() const
{
    if (!Identity.IsValid() || !Occupation.IsValid() || !FMath::IsFinite(Hunger) || Hunger < 0.0f || Hunger > 1.0f
        || !FMath::IsFinite(Fatigue) || Fatigue < 0.0f || Fatigue > 1.0f
        || !FMath::IsFinite(ProgressHours) || ProgressHours < 0.0
        || !FMath::IsFinite(ResumeProgressHours) || ResumeProgressHours < 0.0
        || !FMath::IsFinite(LastTimeHours) || LastTimeHours < -1.0 || CompletedActivities < 0) return false;
    for (const auto& Pair : CooldownUntilHours)
        if (!Pair.Key.IsValid() || !FMath::IsFinite(Pair.Value) || Pair.Value < 0.0) return false;
    return true;
}

FGameplayTag CommonAIDailyLife::ActivityTag(ECommonAIDailyActivity Kind)
{
    switch (Kind)
    {
    case ECommonAIDailyActivity::Work: return CommonAITags::Activity_Work;
    case ECommonAIDailyActivity::Eat: return CommonAITags::Activity_Eat;
    case ECommonAIDailyActivity::Sleep: return CommonAITags::Activity_Sleep;
    case ECommonAIDailyActivity::Rest: return CommonAITags::Activity_Rest;
    case ECommonAIDailyActivity::Socialize: return CommonAITags::Activity_Socialize;
    case ECommonAIDailyActivity::Wander: return CommonAITags::Activity_Wander;
    case ECommonAIDailyActivity::ReturnHome: return CommonAITags::Activity_ReturnHome;
    case ECommonAIDailyActivity::Idle: return CommonAITags::Activity_Idle;
    case ECommonAIDailyActivity::SiteActivity: return {};
    }
    return {};
}

FGameplayTag CommonAIDailyLife::InputTag(ECommonAIDailyActivity Kind, int32 Channel)
{
#define DAILY_INPUT_ROW(Name) { CommonAITags::DailyLife_##Name##_Need, CommonAITags::DailyLife_##Name##_Time, CommonAITags::DailyLife_##Name##_Available, CommonAITags::DailyLife_##Name##_Distance }
    const FGameplayTag Inputs[][4] = {
        DAILY_INPUT_ROW(Work), DAILY_INPUT_ROW(Eat), DAILY_INPUT_ROW(Sleep), DAILY_INPUT_ROW(Rest),
        DAILY_INPUT_ROW(Socialize), DAILY_INPUT_ROW(Wander), DAILY_INPUT_ROW(ReturnHome), DAILY_INPUT_ROW(Idle)
    };
#undef DAILY_INPUT_ROW
    const int32 Index = static_cast<int32>(Kind);
    return Index >= 0 && Index < UE_ARRAY_COUNT(Inputs) && Channel >= 0 && Channel < 4 ? Inputs[Index][Channel] : FGameplayTag();
}
FGameplayTag CommonAIDailyLife::SourceTag() { return CommonAITags::Source_DailyLife; }
FGameplayTag CommonAIDailyLife::EmergencyReason() { return CommonAITags::Reason_Emergency; }
FGameplayTag CommonAIDailyLife::NeedReason() { return CommonAITags::Reason_UrgentNeed; }
