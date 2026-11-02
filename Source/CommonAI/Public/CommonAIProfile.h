#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CommonAIProfile.generated.h"

class UCurveTable;

UCLASS(BlueprintType)
class COMMONAI_API UCommonAIProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UCommonAIProfile();
    bool IsUsable() const;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTag WorkActivity;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTagQuery RequiredAgentTags;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTagQuery RequiredTargetCapabilities;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI") TObjectPtr<UCurveTable> ConsiderationCurves;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI") FName AvailabilityCurveRow;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI", meta = (ClampMin = "0.01", Units = "Seconds"))
    float ObservationIntervalSeconds = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI", meta = (ClampMin = "0.0", Units = "Centimeters"))
    float AcceptanceRadiusCentimeters = 50.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI", meta = (ClampMin = "0.0"))
    float WorkScoreModifier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI", meta = (ClampMin = "0.0", Units = "Seconds"))
    float CancellationWarningSeconds = 10.0f;
};
