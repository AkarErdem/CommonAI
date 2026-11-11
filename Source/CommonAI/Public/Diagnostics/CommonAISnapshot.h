#pragma once
#include "CoreMinimal.h"
#include "Contracts/CommonAIInteraction.h"
#include "Definitions/CommonAIConsiderationDefinition.h"
#include "Definitions/CommonAIServiceDefinition.h"
#include "CommonAISnapshot.generated.h"

UENUM(BlueprintType)
enum class ECommonAITransitionMode : uint8 { CancelAtSafePoint, AfterNaturalCompletion };
UENUM(BlueprintType)
enum class ECommonAICommandRetention : uint8 { HoldUntilCompletion, AllowUtilityReplacement };
UENUM(BlueprintType)
enum class ECommonAICommandStatus : uint8 { Rejected, Queued, Running, Completed, Failed, Cancelled, Superseded };

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAICommandSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGuid Id;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGuid Execution;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Activity;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") ECommonAICommandStatus Status = ECommonAICommandStatus::Rejected;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Reason;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAICurveSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Consideration;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float NormalizedInput = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float Contribution = 0.0f;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAITaskSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Activity;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float Score = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float ScoreModifier = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float ReplaceResistance = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") int32 Priority = 1;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") int32 Momentum = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") int32 MaxMomentum = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") bool bLocked = false;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") bool bInterruptible = true;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") bool bActive = false;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") bool bFallback = false;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Reason;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") TArray<FCommonAICurveSnapshot> Curves;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIServiceConditionSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FCommonAIServiceCondition Definition;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") bool bMet = false;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIServiceSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Id;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") bool bRunning = false;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") float IntervalSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") TArray<FCommonAIServiceConditionSnapshot> Conditions;
};
