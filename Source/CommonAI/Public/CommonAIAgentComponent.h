#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Contracts/CommonAIInteraction.h"
#include "CommonAIAgentComponent.generated.h"

class UCommonAIProfile;
class UCommonAIBackend;
class UCommonAIWorkProviderComponent;
class USkeletalMeshComponent;
class UAnimInstance;
class UAnimNotifyState;

UENUM(BlueprintType)
enum class ECommonAIPhase : uint8 { Idle, Approaching, Interacting, Cancelling };

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAITagContribution
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGuid Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") TWeakObjectPtr<UObject> Source;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTagContainer Tags;
};

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIAgentSnapshot
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") bool bEnabled = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") ECommonAIPhase Phase = ECommonAIPhase::Idle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTag Activity;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") TWeakObjectPtr<UObject> Target;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGuid Execution;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FCommonAIInteractionResult LastResult;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTag CancellationReason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") float CancellationElapsedSeconds = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") bool bCancellationOverdue = false;
};

// The consumer entry point. Runtime backend types are never required by its callers.
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAIAgentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCommonAIAgentComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI") TObjectPtr<UCommonAIProfile> Profile;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI") bool bStartAutomatically = true;
    UFUNCTION(BlueprintCallable, Category = "Common AI") bool StartAgent();
    UFUNCTION(BlueprintCallable, Category = "Common AI") void StopAgent();
    UFUNCTION(BlueprintCallable, Category = "Common AI") bool SetProfile(UCommonAIProfile* NewProfile);
    UFUNCTION(BlueprintCallable, Category = "Common AI") void RequestCancellation(FGameplayTag Reason);
    UFUNCTION(BlueprintCallable, Category = "Common AI") bool AddInteractionTarget(UObject* Target);
    UFUNCTION(BlueprintCallable, Category = "Common AI") void RemoveInteractionTarget(UObject* Target);
    UFUNCTION(BlueprintCallable, Category = "Common AI") bool SetMovementAdapter(UObject* Adapter);
    UObject* GetMovementAdapter() const { return MovementAdapter; }
    UFUNCTION(BlueprintCallable, Category = "Common AI|Tags") FGuid AddTagContribution(UObject* Source, const FGameplayTagContainer& Tags);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Tags") bool RemoveTagContribution(UObject* Source, FGuid Contribution);
    UFUNCTION(BlueprintPure, Category = "Common AI|Tags") FGameplayTagContainer GetEffectiveTags() const;
    UFUNCTION(BlueprintPure, Category = "Common AI") FCommonAIAgentSnapshot GetSnapshot() const { return Snapshot; }
    UFUNCTION(BlueprintPure, Category = "Common AI") static UCommonAIAgentComponent* FindAgent(AActor* Actor);

    // Notify bookkeeping belongs to this agent, never to a shared notify asset.
    void BeginMontageTagWindow(USkeletalMeshComponent* Mesh, UAnimNotifyState* Notify, int32 MontageInstanceId, const FGameplayTagContainer& Tags);
    void EndMontageTagWindow(USkeletalMeshComponent* Mesh, UAnimNotifyState* Notify, int32 MontageInstanceId);
    void SetExecutionSnapshot(ECommonAIPhase Phase, const FCommonAIInteractionRequest& Request, UObject* Target, FGameplayTag Reason = FGameplayTag());
    void RecordExecutionResult(const FCommonAIInteractionResult& Result);
    UCommonAIWorkProviderComponent* GetProvider() const { return Provider; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    struct FMontageWindow
    {
        TWeakObjectPtr<USkeletalMeshComponent> Mesh;
        TWeakObjectPtr<UAnimInstance> AnimInstance;
        TWeakObjectPtr<UAnimNotifyState> Notify;
        int32 MontageInstanceId = INDEX_NONE;
        FGuid Contribution;
    };
    bool EnsureBackend();
    void ApplyPendingOperation();
    UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
    UPROPERTY(Transient) TObjectPtr<UCommonAIBackend> Backend;
    UPROPERTY(Transient) TObjectPtr<UCommonAIWorkProviderComponent> Provider;
    UPROPERTY(Transient) TObjectPtr<UCommonAIProfile> PendingProfile;
    UPROPERTY(Transient) TObjectPtr<UObject> MovementAdapter;
    UPROPERTY(VisibleInstanceOnly, Category = "Common AI|State") FCommonAIAgentSnapshot Snapshot;
    UPROPERTY(VisibleInstanceOnly, Category = "Common AI|State") TArray<FCommonAITagContribution> TagContributions;
    TArray<FMontageWindow> MontageWindows;
    FGuid CancellingContribution;
    bool bStopPending = false;
    bool bRestartAfterDrain = false;
    float ObservationElapsedSeconds = 0.0f;
};
