#pragma once
#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Engine/DataAsset.h"
#include "DailyLife/CommonAIDailyLifeTypes.h"
#include "CommonAIDailyLifeProfile.generated.h"

class UCommonAIProfile;

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIDailyActivityDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity") FGameplayTag Activity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity") ECommonAIDailyActivity Kind = ECommonAIDailyActivity::Work;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity") FText Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity", meta=(ClampMin="0")) float BaseWeight = 85.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity") FGameplayTagQuery RequiredCapabilities;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") FGameplayTag NeedInput;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") FGameplayTag TimeInput;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") FGameplayTag AvailabilityInput;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") FGameplayTag DistanceInput;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time") TArray<FCommonAITimePreference> PreferredTimes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time", meta=(ClampMin="0", ClampMax="1")) float OutsideWindowPreference = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs", meta=(ClampMin="0", ClampMax="0.99")) float NeedThreshold = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs", meta=(ClampMin="0", ClampMax="1")) float SatisfiedNeed = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs", meta=(ClampMin="0")) float HungerRecoveryPerHour = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs", meta=(ClampMin="0")) float FatigueRecoveryPerHour = 0.0f;
    // The site must acknowledge completion or safe cancellation when it owns the effect.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing") bool bCompleteOnSiteSignal = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta=(ClampMin="0.001", EditCondition="!bCompleteOnSiteSignal")) float DurationHours = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta=(ClampMin="0")) float MinimumCommitmentHours = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta=(ClampMin="0")) float CooldownHours = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta=(ClampMin="0", ClampMax="1")) float ReplaceResistance = 0.08f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing") bool bResumeAfterInterruption = true;

    TStaticArray<FGameplayTag, 4> GetInputTags() const { return { NeedInput, TimeInput, AvailabilityInput, DistanceInput }; }
};

// Immutable authoring data. The component projects it into the existing Athena-backed profile.
UCLASS(BlueprintType)
class COMMONAI_API UCommonAIDailyLifeProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity") FGameplayTag Occupation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity") FText OccupationLabel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activities", meta=(TitleProperty="Label")) TArray<FCommonAIDailyActivityDefinition> Activities;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs", meta=(ClampMin="0")) float HungerPerHour = 0.06f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs", meta=(ClampMin="0")) float FatiguePerHour = 0.045f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs", meta=(ClampMin="0", ClampMax="1")) float UrgentNeedThreshold = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta=(ClampMin="0.01", Units="Seconds")) float ObservationIntervalSeconds = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta=(ClampMin="0.01", Units="Seconds")) float ExecutionIntervalSeconds = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta=(ClampMin="0")) float ScheduleVariationHours = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta=(ClampMin="1", Units="Centimeters")) float TravelCostDistanceCentimeters = 10000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta=(ClampMin="0", Units="Centimeters")) float AcceptanceRadiusCentimeters = 45.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta=(ClampMin="0.001")) float TravelTimeoutHours = 2.0f;
    UFUNCTION(BlueprintPure, Category = "Common AI|Daily Life") TArray<FString> ValidateProfile() const;
    const FCommonAIDailyActivityDefinition* FindActivity(FGameplayTag Activity) const;
    UCommonAIProfile* CreateAgentProfile(UObject* Outer) const;
};
