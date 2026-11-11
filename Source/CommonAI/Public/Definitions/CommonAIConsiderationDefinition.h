#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/CurveTable.h"
#include "CommonAIConsiderationDefinition.generated.h"

class UCurveTable;
class UCommonAIEvaluator;

UENUM(BlueprintType)
enum class ECommonAIInputRoute : uint8 { External, Opportunity, Evaluator };
UENUM(BlueprintType)
enum class ECommonAIValueStatus : uint8 { Unavailable, Value, Skipped };
UENUM(BlueprintType)
enum class ECommonAIRetentionPolicy : uint8 { Invalidate, HoldLast };

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIConsiderationDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "CommonAI.Consideration")) FGameplayTag Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") FName Units;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") ECommonAIInputRoute Route = ECommonAIInputRoute::External;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") FGameplayTag SourceId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") float Minimum = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") float Maximum = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") bool bHasInitialValue = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (EditCondition = "bHasInitialValue")) float InitialValue = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "CommonAI.Activity")) FGameplayTag OpportunityActivity;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Input") TObjectPtr<UCommonAIEvaluator> Evaluator;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced") ECommonAIRetentionPolicy UnavailablePolicy = ECommonAIRetentionPolicy::Invalidate;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAICurveBinding
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve", meta = (Categories = "CommonAI.Consideration")) FGameplayTag Consideration;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve") FCurveTableRowHandle Curve;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIInputUpdate
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI") FGameplayTag Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI") ECommonAIValueStatus Status = ECommonAIValueStatus::Value;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI") float RawValue = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI") bool bOverrideRange = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI") float Minimum = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI") float Maximum = 1.0f;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIConsiderationSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Id;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag SourceId;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") ECommonAIValueStatus Status = ECommonAIValueStatus::Unavailable;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float RawValue = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float NormalizedValue = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float Minimum = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float Maximum = 1.0f;
};
