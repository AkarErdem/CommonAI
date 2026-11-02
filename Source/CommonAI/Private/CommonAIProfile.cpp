#include "CommonAIProfile.h"
#include "CommonAITags.h"
#include "Engine/CurveTable.h"

UCommonAIProfile::UCommonAIProfile()
{
    WorkActivity = CommonAITags::Activity_Work;
    // CSV table import removes spaces from row identifiers. Backend projection supplies vendor formatting.
    AvailabilityCurveRow = FName(*FString::Printf(TEXT("%s-%s"), *WorkActivity.ToString(), *CommonAITags::Consideration_WorkAvailable.GetTag().ToString()));
}

bool UCommonAIProfile::IsUsable() const
{
    return WorkActivity.IsValid() && ConsiderationCurves && ConsiderationCurves->FindCurveUnchecked(AvailabilityCurveRow)
        && FMath::IsFinite(ObservationIntervalSeconds) && ObservationIntervalSeconds > 0.0f
        && FMath::IsFinite(AcceptanceRadiusCentimeters) && AcceptanceRadiusCentimeters >= 0.0f
        && FMath::IsFinite(WorkScoreModifier) && WorkScoreModifier >= 0.0f
        && FMath::IsFinite(CancellationWarningSeconds) && CancellationWarningSeconds >= 0.0f;
}
