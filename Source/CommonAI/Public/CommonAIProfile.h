#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Definitions/CommonAITaskDefinition.h"
#include "Definitions/CommonAIServiceDefinition.h"
#include "CommonAIProfile.generated.h"

class UCurveTable;

UENUM(BlueprintType)
enum class ECommonAIScoringMode : uint8 { Interval, OnInputChange, Manual };

UCLASS(BlueprintType)
class COMMONAI_API UCommonAIProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UCommonAIProfile();
    bool IsUsable() const;
    UFUNCTION(BlueprintPure, Category = "Common AI") TArray<FString> ValidateProfile() const;
    const FCommonAITaskDefinition* FindTask(FGameplayTag Activity) const;
    const FCommonAIConsiderationDefinition* FindConsideration(FGameplayTag Id) const;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (Categories = "CommonAI.Profile")) FGameplayTag ProfileId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tasks", meta = (TitleProperty = "Activity")) TArray<FCommonAITaskDefinition> Tasks;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Considerations", meta = (TitleProperty = "Id")) TArray<FCommonAIConsiderationDefinition> Considerations;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Services", meta = (TitleProperty = "Id")) TArray<FCommonAIServiceDefinition> Services;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fallback") FCommonAITaskDefinition Fallback;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced") bool bPauseAffectsServices = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced") ECommonAIScoringMode ScoringMode = ECommonAIScoringMode::Interval;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced") bool bRescoreAfterCompletion = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Authoring") TObjectPtr<UCurveTable> AuthoringCurves;
    UPROPERTY(VisibleAnywhere, Transient, Category = "Validation") TArray<FString> ValidationMessages;
#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Authoring") void GenerateMissingCurves();
    UFUNCTION(CallInEditor, Category = "Authoring") void CreateGeneralExample();
    UFUNCTION(CallInEditor, Category = "Validation") void RefreshValidation();
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
    virtual void PostLoad() override;
#endif
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI", meta = (ClampMin = "0.0", Units = "Seconds"))
    float ObservationIntervalSeconds = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI", meta = (ClampMin = "0.0", Units = "Seconds"))
    float CancellationWarningSeconds = 10.0f;
};
