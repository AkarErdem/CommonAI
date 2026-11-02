#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Contracts/CommonAIInteraction.h"
#include "CommonAIWorkProviderComponent.generated.h"

// Registered targets avoid global world scans. Targets can implement the contract on actors or components.
UCLASS(Blueprintable, ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAIWorkProviderComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UObject* FindOpportunity(const FCommonAIInteractionRequest& Request, const FGameplayTagQuery& RequiredCapabilities) const;
    UFUNCTION(BlueprintCallable, Category = "Common AI|Work") bool AddTarget(UObject* Target);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Work") void RemoveTarget(UObject* Target);
private:
    UPROPERTY(Transient) TArray<TWeakObjectPtr<UObject>> Targets;
};
