#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Contracts/CommonAIInteraction.h"
#include "CommonAIWorkSite.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonAIInteractionStarted, FCommonAIInteractionRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommonAIInteractionCancelRequested, FCommonAIInteractionRequest, Request, FGameplayTag, Reason);

// One target grants one execution at a time. Signals must carry the original execution identity.
UCLASS(Blueprintable, ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAIWorkSite : public UActorComponent, public ICommonAIInteraction
{
    GENERATED_BODY()
public:
    UCommonAIWorkSite();
    virtual FCommonAIInteractionOffer QueryInteraction_Implementation(const FCommonAIInteractionRequest& Request) const override;
    virtual bool ReserveInteraction_Implementation(const FCommonAIInteractionRequest& Request) override;
    virtual FCommonAIInteractionResult BeginInteraction_Implementation(const FCommonAIInteractionRequest& Request) override;
    virtual void RequestInteractionCancellation_Implementation(const FCommonAIInteractionRequest& Request, FGameplayTag Reason) override;
    virtual FCommonAIInteractionResult GetInteractionResult_Implementation(const FCommonAIInteractionRequest& Request) const override;
    virtual void ReleaseInteraction_Implementation(const FCommonAIInteractionRequest& Request, bool bEmergencyTeardown) override;

    UFUNCTION(BlueprintCallable, Category = "Common AI|Interaction")
    void SignalCompletion(FGuid Execution, bool bSucceeded);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Interaction")
    void SignalSafeCancellation(FGuid Execution);
    UPROPERTY(BlueprintAssignable, Category = "Common AI|Interaction") FCommonAIInteractionStarted OnInteractionStarted;
    UPROPERTY(BlueprintAssignable, Category = "Common AI|Interaction") FCommonAIInteractionCancelRequested OnCancellationRequested;
    // Emergency teardown cannot wait for animation; effect owners must detach their pending work.
    UPROPERTY(BlueprintAssignable, Category = "Common AI|Interaction") FCommonAIInteractionStarted OnInteractionAbandoned;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI|Interaction") FGameplayTagContainer Activities;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI|Interaction") FGameplayTagContainer Capabilities;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI|Interaction") FGameplayTagQuery RequiredAgentTags;
    // When enabled, the effect owner acknowledges a safe stop or completes naturally.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI|Interaction") bool bWaitForSafeCancellation = true;
protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
    virtual void ClearExecution();
    void AbandonCurrentExecution();
    bool Owns(const FCommonAIInteractionRequest& Request) const;
    FCommonAIInteractionResult Result;
    FGuid Execution;
    TWeakObjectPtr<AAIController> ReservationOwner;
    FGameplayTag ReservedActivity;
    bool bTearingDown = false;
};

UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAITimedWorkSite : public UCommonAIWorkSite
{
    GENERATED_BODY()
public:
    virtual FCommonAIInteractionResult BeginInteraction_Implementation(const FCommonAIInteractionRequest& Request) override;
    UPROPERTY(EditAnywhere, Category = "Common AI|Work", meta = (ClampMin = "0.001", Units = "Seconds"))
    float WorkDurationSeconds = 2.0f;
protected:
    virtual void ClearExecution() override;
private:
    FTimerHandle WorkTimer;
};

UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAISignaledWorkSite : public UCommonAIWorkSite
{
    GENERATED_BODY()
};
