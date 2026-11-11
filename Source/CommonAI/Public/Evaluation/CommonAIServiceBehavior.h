#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CommonAIServiceBehavior.generated.h"

class UCommonAIAgentComponent;

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class COMMONAI_API UCommonAIServiceBehavior : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, Category = "Common AI|Service")
    void OnStarted(UCommonAIAgentComponent* Agent, FGameplayTag Service, FGuid InputGeneration);
    virtual void OnStarted_Implementation(UCommonAIAgentComponent* Agent, FGameplayTag Service, FGuid InputGeneration) {}
    UFUNCTION(BlueprintNativeEvent, Category = "Common AI|Service")
    void OnTick(UCommonAIAgentComponent* Agent, FGameplayTag Service, FGuid InputGeneration, float DeltaSeconds);
    virtual void OnTick_Implementation(UCommonAIAgentComponent* Agent, FGameplayTag Service, FGuid InputGeneration, float DeltaSeconds) {}
    UFUNCTION(BlueprintNativeEvent, Category = "Common AI|Service")
    void OnStopped(UCommonAIAgentComponent* Agent, FGameplayTag Service, FGuid InputGeneration);
    virtual void OnStopped_Implementation(UCommonAIAgentComponent* Agent, FGameplayTag Service, FGuid InputGeneration) {}
};
