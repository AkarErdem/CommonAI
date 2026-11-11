#pragma once

#include "CoreMinimal.h"
#include "Contracts/CommonAIWorkSite.h"
#include "GameFramework/Actor.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectRuntime.h"
#include "CommonAISmartObjectSite.generated.h"

class USceneComponent;
class USmartObjectComponent;
class USmartObjectSubsystem;

// Marks slots whose activity is executed through the CommonAI interaction lifecycle.
UCLASS(EditInlineNew, CollapseCategories, meta = (DisplayName = "Common AI Activity"))
class COMMONAI_API UCommonAISmartObjectBehaviorDefinition : public USmartObjectBehaviorDefinition
{
    GENERATED_BODY()
};

// One origin-aligned slot, with ownership retained until the sequence releases its interaction.
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAISmartObjectSite : public UCommonAIWorkSite
{
    GENERATED_BODY()
public:
    UCommonAISmartObjectSite();
    virtual FCommonAIInteractionOffer QueryInteraction_Implementation(const FCommonAIInteractionRequest& Request) const override;
    virtual bool ReserveInteraction_Implementation(const FCommonAIInteractionRequest& Request) override;
    virtual FCommonAIInteractionResult BeginInteraction_Implementation(const FCommonAIInteractionRequest& Request) override;
    virtual void RequestInteractionCancellation_Implementation(const FCommonAIInteractionRequest& Request, FGameplayTag Reason) override;
    virtual bool ReleaseInteraction_Implementation(const FCommonAIInteractionRequest& Request, bool bEmergencyTeardown) override;

    UFUNCTION(BlueprintPure, Category = "Common AI|Interaction")
    bool HasClaim() const;
    const FCommonAIInteractionRequest& GetReservation() const { return Reservation; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI|Interaction") FGuid SiteId;
    // If unset, the owner's Smart Object component is used. The slot must remain at the owner's origin.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI|Interaction") TObjectPtr<USmartObjectComponent> SmartObject;

protected:
    virtual void ClearExecution() override;

private:
    bool ResolvePoint(USmartObjectSubsystem*& OutSubsystem, FSmartObjectSlotHandle& OutSlot) const;
    bool IsSlotAvailable(const FCommonAIInteractionRequest& Request, USmartObjectSubsystem& Subsystem,
        const FSmartObjectSlotHandle& Slot) const;
    void OnSlotInvalidated(const FSmartObjectClaimHandle& InvalidatedClaim, ESmartObjectSlotState PreviousState);
    void HandleDisabledClaim(const FSmartObjectClaimHandle& DisabledClaim);
    FCommonAIInteractionRequest FailLostClaim();
    void RemoveSlotEventBinding();
    void ReleaseClaim();
    FSmartObjectClaimHandle Claim;
    TWeakObjectPtr<USmartObjectSubsystem> ClaimSubsystem;
    FDelegateHandle SlotEventBinding;
    bool bChangingClaim = false;
};

UCLASS(Blueprintable)
class COMMONAI_API ACommonAIActivityLocation : public AActor
{
    GENERATED_BODY()
public:
    ACommonAIActivityLocation();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") TObjectPtr<USmartObjectComponent> SmartObject;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") TObjectPtr<UCommonAISmartObjectSite> Site;
};
