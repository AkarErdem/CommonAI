#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CommonAIActionDefinition.generated.h"

class UCommonAIExecution;

UENUM(BlueprintType)
enum class ECommonAIActionKind : uint8 { FindOpportunity, ResolveBinding, Reserve, Move, Interact, Release, Wait, Custom };

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIActionDefinition
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, Category = "Identity", AdvancedDisplay, meta = (IgnoreForMemberInitializationTest)) FGuid EntryId = FGuid::NewGuid();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action") FText Label;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (Categories = "CommonAI.Action")) FGameplayTag Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action") ECommonAIActionKind Kind = ECommonAIActionKind::Wait;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action") FGameplayTag BindingId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.0", Units = "Seconds")) float DurationSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced", meta = (ClampMin = "0.01", Units = "Seconds")) float TickIntervalSeconds = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.0", Units = "Centimeters")) float AcceptanceRadiusCentimeters = 50.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Action") TObjectPtr<UCommonAIExecution> Execution;
};
