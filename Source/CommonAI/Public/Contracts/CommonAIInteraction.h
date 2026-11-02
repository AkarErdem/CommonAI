#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "CommonAIInteraction.generated.h"

class AAIController;

UENUM(BlueprintType)
enum class ECommonAIInteractionStatus : uint8
{
    Rejected, Reserved, Running, Cancelling, Completed, Failed, Cancelled
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIInteractionRequest
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGuid Execution;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") TObjectPtr<AAIController> Agent = nullptr;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Activity;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIInteractionOffer
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI") bool bAvailable = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI") TObjectPtr<AActor> ApproachActor = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI") FGameplayTagContainer Capabilities;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIInteractionResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") ECommonAIInteractionStatus Status = ECommonAIInteractionStatus::Rejected;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Reason;
    bool IsTerminal() const
    {
        return Status == ECommonAIInteractionStatus::Completed || Status == ECommonAIInteractionStatus::Failed
            || Status == ECommonAIInteractionStatus::Cancelled || Status == ECommonAIInteractionStatus::Rejected;
    }
};

UINTERFACE(BlueprintType)
class COMMONAI_API UCommonAIInteraction : public UInterface
{
    GENERATED_BODY()
};

// Targets may be actors or components. Every operation is scoped to the original request.
class COMMONAI_API ICommonAIInteraction
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Interaction")
    FCommonAIInteractionOffer QueryInteraction(const FCommonAIInteractionRequest& Request) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Interaction")
    bool ReserveInteraction(const FCommonAIInteractionRequest& Request);
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Interaction")
    FCommonAIInteractionResult BeginInteraction(const FCommonAIInteractionRequest& Request);
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Interaction")
    void RequestInteractionCancellation(const FCommonAIInteractionRequest& Request, FGameplayTag Reason);
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Interaction")
    FCommonAIInteractionResult GetInteractionResult(const FCommonAIInteractionRequest& Request) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Interaction")
    void ReleaseInteraction(const FCommonAIInteractionRequest& Request, bool bEmergencyTeardown = false);
};
