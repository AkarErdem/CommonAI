#pragma once
#include "CoreMinimal.h"
#include "Definitions/CommonAIActionDefinition.h"
#include "Definitions/CommonAIConsiderationDefinition.h"
#include "CommonAITaskDefinition.generated.h"

class UBehaviorTree;

UENUM(BlueprintType)
enum class ECommonAITaskKind : uint8 { Sequence, Wait, BehaviorTree, Custom };

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAITaskDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task", meta = (Categories = "CommonAI.Activity")) FGameplayTag Activity;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task") FText Label;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task") ECommonAITaskKind Kind = ECommonAITaskKind::Sequence;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task") FGameplayTagQuery RequiredAgentTags;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task") FGameplayTagQuery RequiredTargetCapabilities;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task", meta = (Categories = "CommonAI.State")) FGameplayTagContainer ActiveStateTags;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task") bool bCommandOnly = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task") TArray<FCommonAICurveBinding> Considerations;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task") TArray<FCommonAIActionDefinition> Actions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Task") TObjectPtr<UCommonAIExecution> Execution;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task") TObjectPtr<UBehaviorTree> BehaviorTree;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task", meta = (ClampMin = "0.0", Units = "Seconds")) float WaitDurationSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy", meta = (ClampMin = "1", ClampMax = "127")) int32 Priority = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy", meta = (ClampMin = "0.0")) float ScoreModifier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy", meta = (ClampMin = "0.0", ClampMax = "1.0")) float ReplaceResistance = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy") bool bUtilityInterruptible = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced", meta = (ClampMin = "0.01", Units = "Seconds")) float TickIntervalSeconds = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced", meta = (ClampMin = "0.0", Units = "Seconds")) float UnlockDelaySeconds = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced", meta = (ClampMin = "0", ClampMax = "50")) int32 MaxMomentum = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced", meta = (ClampMin = "0", ClampMax = "100")) int32 MomentumReductionPercent = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced", meta = (ClampMin = "0.0", Units = "Seconds")) float MomentumDecaySeconds = 0.0f;
};
