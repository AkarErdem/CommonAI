#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CommonAIServiceDefinition.generated.h"

class UCommonAIServiceBehavior;

UENUM(BlueprintType)
enum class ECommonAIComparison : uint8 { Less, LessEqual, Equal, GreaterEqual, Greater };

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIServiceCondition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition") FGameplayTag Consideration;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition") ECommonAIComparison Comparison = ECommonAIComparison::GreaterEqual;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0.0", ClampMax = "1.0")) float NormalizedThreshold = 0.5f;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIServiceDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Service", meta = (Categories = "CommonAI.Service")) FGameplayTag Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Service") TArray<FCommonAIServiceCondition> Conditions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Service", meta = (ClampMin = "0.0", Units = "Seconds")) float IntervalSeconds = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Service") FGameplayTagContainer EvaluatedInputs;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Service") TObjectPtr<UCommonAIServiceBehavior> Behavior;
};
