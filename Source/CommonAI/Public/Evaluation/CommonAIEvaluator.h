#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Definitions/CommonAIConsiderationDefinition.h"
#include "UObject/Interface.h"
#include "CommonAIEvaluator.generated.h"

class UCommonAIAgentComponent;

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAINumericContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") float Value = 0.0f;
};
USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIActorContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") TObjectPtr<AActor> Source;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") TObjectPtr<AActor> Target;
};
USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAILocationContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") FVector Source = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") FVector Target = FVector::ZeroVector;
};
USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAITagContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") FGameplayTagContainer Tags;
};
USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIEventContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") FGameplayTag Event;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") TObjectPtr<AActor> Source;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") TObjectPtr<AActor> Target;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") FVector Direction = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context") float Magnitude = 0.0f;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIEvaluationResult
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation") ECommonAIValueStatus Status = ECommonAIValueStatus::Unavailable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation") float RawValue = 0.0f;
};

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class COMMONAI_API UCommonAIEvaluator : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, Category = "Common AI|Evaluation")
    FCommonAIEvaluationResult Evaluate(const FInstancedStruct& Context) const;
    virtual FCommonAIEvaluationResult Evaluate_Implementation(const FInstancedStruct& Context) const { return {}; }
};

UCLASS(DisplayName = "Numeric or Event Magnitude")
class COMMONAI_API UCommonAINumericEvaluator : public UCommonAIEvaluator
{
    GENERATED_BODY()
public:
    virtual FCommonAIEvaluationResult Evaluate_Implementation(const FInstancedStruct& Context) const override;
};
UCLASS(DisplayName = "Actor or Location Distance")
class COMMONAI_API UCommonAIDistanceEvaluator : public UCommonAIEvaluator
{
    GENERATED_BODY()
public:
    virtual FCommonAIEvaluationResult Evaluate_Implementation(const FInstancedStruct& Context) const override;
};
UCLASS(DisplayName = "Tag Query")
class COMMONAI_API UCommonAITagQueryEvaluator : public UCommonAIEvaluator
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Evaluation") FGameplayTagQuery Query;
    virtual FCommonAIEvaluationResult Evaluate_Implementation(const FInstancedStruct& Context) const override;
};

UINTERFACE(BlueprintType)
class COMMONAI_API UCommonAIObservationProvider : public UInterface { GENERATED_BODY() };

class COMMONAI_API ICommonAIObservationProvider
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Evaluation")
    FInstancedStruct ReadObservation(UCommonAIAgentComponent* Agent, FGameplayTag Consideration) const;
};
