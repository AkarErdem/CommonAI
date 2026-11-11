#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Contracts/CommonAIInteraction.h"
#include "Diagnostics/CommonAISnapshot.h"
#include "CommonAIAgentComponent.generated.h"

class UCommonAIProfile;
class UCommonAIBackend;
class UCommonAIWorkProviderComponent;
class USkeletalMeshComponent;
class UAnimInstance;
class UAnimNotifyState;
class UBehaviorTreeComponent;

UENUM(BlueprintType)
enum class ECommonAIPhase : uint8 { Idle, Approaching, Interacting, Cancelling, Executing, Waiting };

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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FCommonAIInteractionRequest Interaction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FCommonAIInteractionResult LastResult;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTag CancellationReason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") float CancellationElapsedSeconds = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") bool bCancellationOverdue = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTag CurrentAction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGuid Invocation;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") bool bScoringPaused = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") bool bScoresDirty = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTag ProfileId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FGuid InputGeneration;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") bool bProfileChangePending = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") ECommonAITransitionMode ProfileTransition = ECommonAITransitionMode::CancelAtSafePoint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") TArray<FCommonAITaskSnapshot> Tasks;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") TArray<FCommonAIConsiderationSnapshot> Considerations;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") TArray<FCommonAIServiceSnapshot> Services;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Common AI") FCommonAICommandSnapshot Command;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonAIActivityStarted, FCommonAIInteractionRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommonAIActivityEnded, FCommonAIInteractionRequest, Request, FCommonAIInteractionResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonAICommandChanged, FCommonAICommandSnapshot, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonAIInputsReset, FGuid, Generation);

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIActivityProgress
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGuid Execution;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGuid Operation;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGuid Invocation;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Activity;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Action;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") ECommonAIPhase Phase = ECommonAIPhase::Idle;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") TWeakObjectPtr<UObject> Target;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonAIActivityProgressed, FCommonAIActivityProgress, Progress);

UENUM()
enum class ECommonAIActivityEventKind : uint8 { Started, Progress, Ended };

USTRUCT()
struct FCommonAIActivityEventRecord
{
    GENERATED_BODY()
    UPROPERTY() FCommonAIInteractionRequest Request;
    UPROPERTY() FCommonAIInteractionResult Result;
    UPROPERTY() FCommonAIActivityProgress Progress;
    UPROPERTY() ECommonAIActivityEventKind Kind = ECommonAIActivityEventKind::Ended;
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
    UFUNCTION(BlueprintCallable, Category = "Common AI") bool ChangeProfile(UCommonAIProfile* NewProfile, ECommonAITransitionMode Transition, bool bPreserveInputs = true);
    UFUNCTION(BlueprintCallable, Category = "Common AI") void RequestCancellation(FGameplayTag Reason);
    UFUNCTION(BlueprintCallable, Category = "Common AI") bool CancelActivity(FGuid Execution, FGameplayTag Reason);
    UFUNCTION(BlueprintCallable, Category = "Common AI") bool AddInteractionTarget(UObject* Target);
    UFUNCTION(BlueprintCallable, Category = "Common AI") void RemoveInteractionTarget(UObject* Target);
    UFUNCTION(BlueprintCallable, Category = "Common AI") bool SetMovementAdapter(UObject* Adapter);
    UObject* GetMovementAdapter() const { return MovementAdapter; }
    UFUNCTION(BlueprintCallable, Category = "Common AI|Tags") FGuid AddTagContribution(UObject* Source, const FGameplayTagContainer& Tags);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Tags") bool RemoveTagContribution(UObject* Source, FGuid Contribution);
    UFUNCTION(BlueprintPure, Category = "Common AI|Tags") FGameplayTagContainer GetEffectiveTags() const;
    UFUNCTION(BlueprintPure, Category = "Common AI|Tags") TArray<FCommonAITagContribution> GetTagContributions() const { return TagContributions; }
    UFUNCTION(BlueprintPure, Category = "Common AI") FCommonAIAgentSnapshot GetSnapshot() const;
    UFUNCTION(BlueprintPure, Category = "Common AI") static UCommonAIAgentComponent* FindAgent(AActor* Actor);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Inputs") bool RegisterInputSource(FGameplayTag SourceId, UObject* Source);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Inputs") bool UnregisterInputSource(FGameplayTag SourceId, UObject* Source);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Inputs") bool ApplyInputUpdates(UObject* Source, FGuid Generation, const TArray<FCommonAIInputUpdate>& Updates, bool bRescore = true);
    UFUNCTION(BlueprintPure, Category = "Common AI|Inputs") FGuid GetInputGeneration() const;
    UFUNCTION(BlueprintPure, Category = "Common AI|Inputs") bool GetConsideration(FGameplayTag Id, FCommonAIConsiderationSnapshot& Value) const;
    UFUNCTION(BlueprintCallable, Category = "Common AI|Inputs", meta = (AutoCreateRefTerm = "Context"))
    bool EvaluateInput(UObject* Source, FGuid Generation, FGameplayTag Id, const FInstancedStruct& Context, bool bRescore = true);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") FGuid RequestActivity(FGameplayTag Activity,
        ECommonAITransitionMode Transition = ECommonAITransitionMode::CancelAtSafePoint,
        ECommonAICommandRetention Retention = ECommonAICommandRetention::HoldUntilCompletion, bool bUnlockRetry = false);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control", meta = (AutoCreateRefTerm = "Payload"))
    FGuid RequestActivityWithContext(FGameplayTag Activity, const FInstancedStruct& Payload,
        ECommonAITransitionMode Transition = ECommonAITransitionMode::CancelAtSafePoint,
        ECommonAICommandRetention Retention = ECommonAICommandRetention::HoldUntilCompletion, bool bUnlockRetry = false);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") FGuid PauseScoring(UObject* Source, FGameplayTag Reason);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") bool ResumeScoring(UObject* Source, FGuid Hold);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") bool Rescore();
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") FGuid LockTask(UObject* Source, FGameplayTag Activity, FGameplayTag Reason);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") bool UnlockTask(UObject* Source, FGuid Hold);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") bool SetTaskScoreModifier(FGameplayTag Activity, float Value);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") bool SetTaskPriority(FGameplayTag Activity, int32 Value);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") bool SetTaskResistance(FGameplayTag Activity, float Value);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") bool SetTaskInterruptible(FGameplayTag Activity, bool bInterruptible);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Control") bool SetTaskMomentum(FGameplayTag Activity, int32 Maximum, int32 ReductionPercent, float DecaySeconds);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Execution") bool RegisterExecutionBinding(FGameplayTag Id, UObject* Binding);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Execution") bool UpdateExecutionBinding(FGameplayTag Id, UObject* ExpectedBinding, UObject* NewBinding);
    UFUNCTION(BlueprintPure, Category = "Common AI|Execution") UObject* GetExecutionBinding(FGameplayTag Id) const;
    UFUNCTION(BlueprintCallable, Category = "Common AI|Execution") bool CompleteExecution(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Execution") bool CompleteBehaviorTree(UBehaviorTreeComponent* Tree, bool bSucceeded, FGameplayTag Reason);
    UPROPERTY(BlueprintAssignable, Category = "Common AI|Events") FCommonAIActivityStarted OnActivityStarted;
    UPROPERTY(BlueprintAssignable, Category = "Common AI|Events") FCommonAIActivityEnded OnActivityEnded;
    UPROPERTY(BlueprintAssignable, Category = "Common AI|Events") FCommonAIActivityProgressed OnActivityProgress;
    UPROPERTY(BlueprintAssignable, Category = "Common AI|Events") FCommonAICommandChanged OnCommandChanged;
    UPROPERTY(BlueprintAssignable, Category = "Common AI|Events") FCommonAIInputsReset OnInputsReset;

    // Notify bookkeeping belongs to this agent, never to a shared notify asset.
    void BeginMontageTagWindow(USkeletalMeshComponent* Mesh, UAnimNotifyState* Notify, int32 MontageInstanceId, const FGameplayTagContainer& Tags);
    void EndMontageTagWindow(USkeletalMeshComponent* Mesh, UAnimNotifyState* Notify, int32 MontageInstanceId);
    void SetExecutionSnapshot(ECommonAIPhase Phase, const FCommonAIInteractionRequest& Request, UObject* Target, FGameplayTag Reason = FGameplayTag());
    void RecordExecutionResult(const FCommonAIInteractionRequest& Request, const FCommonAIInteractionResult& Result);
    void QueueActivityStarted(const FCommonAIInteractionRequest& Request);
    void QueueActivityProgress();
    void QueueCommandEvent(const FCommonAICommandSnapshot& Command);
    void SetCurrentAction(FGameplayTag Action, FGuid Invocation);
    UObject* GetInputSource(FGameplayTag Id) const;
    FGuid GetInputSourceRevision(FGameplayTag Id) const;
    bool HasScoringHolds() const;
    bool HasTaskLock(FGameplayTag Activity) const;
    void ReleaseOwnedState(UObject* Source, bool bReleaseInputs = true);
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
    struct FControlHold { FGuid Id; TWeakObjectPtr<UObject> Source; FGameplayTag Activity; FGameplayTag Reason; };
    TArray<FControlHold> ScoringHolds;
    TArray<FControlHold> TaskHolds;
    UPROPERTY(Transient) TArray<FCommonAIActivityEventRecord> ActivityEvents;
    TArray<FCommonAICommandSnapshot> CommandEvents;
    UPROPERTY(Transient) TMap<FGameplayTag, TWeakObjectPtr<UObject>> InputSources;
    TMap<FGameplayTag, FGuid> InputSourceRevisions;
    UPROPERTY(Transient) TMap<FGameplayTag, TWeakObjectPtr<UObject>> ExecutionBindings;
    ECommonAITransitionMode PendingProfileMode = ECommonAITransitionMode::CancelAtSafePoint;
    bool bPreserveProfileInputs = true;
    bool bResetInputsForPawn = false;
    bool bEndingPlay = false;
    bool bStopPending = false;
    bool bRestartAfterDrain = false;
    float ObservationElapsedSeconds = 0.0f;
};
