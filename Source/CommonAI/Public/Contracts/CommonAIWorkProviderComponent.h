#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CommonAIWorkSite.h"
#include "CommonAIWorkProviderComponent.generated.h"

class AAIController;

struct FCommonAIExecutionContext
{
	TWeakObjectPtr<AAIController> Agent;
	uint64 RunId = 0;
};

// Attach to an AIController; authored sites may be supplied by a consumer without changing the controller class.
UCLASS(Blueprintable, ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAIWorkProviderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	virtual bool FindOpportunity(const FCommonAIExecutionContext& Context, FCommonAIWorkOpportunity& OutOpportunity) const;

	UFUNCTION(BlueprintCallable, Category = "Common AI|Work")
	void AddSite(UCommonAIWorkSite* Site);

	UFUNCTION(BlueprintCallable, Category = "Common AI|Work")
	void RemoveSite(UCommonAIWorkSite* Site);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCommonAIWorkSite>> Sites;
};
