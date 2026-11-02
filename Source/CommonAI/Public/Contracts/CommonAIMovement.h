#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Contracts/CommonAIInteraction.h"
#include "CommonAIMovement.generated.h"

UENUM(BlueprintType)
enum class ECommonAIMoveStatus : uint8 { Running, Completed, Failed };

UINTERFACE(BlueprintType)
class COMMONAI_API UCommonAIMovement : public UInterface { GENERATED_BODY() };

// Optional binding to an existing locomotion owner. Cancellation must relinquish this
// movement request synchronously; animation-bound activities use the interaction protocol.
class COMMONAI_API ICommonAIMovement
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Movement")
    ECommonAIMoveStatus BeginMove(const FCommonAIInteractionRequest& Request, AActor* Target, float AcceptanceRadiusCentimeters);
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Movement")
    ECommonAIMoveStatus GetMoveStatus(const FCommonAIInteractionRequest& Request) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Movement")
    void CancelMove(const FCommonAIInteractionRequest& Request);
};
