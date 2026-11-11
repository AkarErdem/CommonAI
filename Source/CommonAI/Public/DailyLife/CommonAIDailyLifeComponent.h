#pragma once
#include "CoreMinimal.h"
#include "AITypes.h"
#include "Components/ActorComponent.h"
#include "Contracts/CommonAIMovement.h"
#include "DailyLife/CommonAIDailyLifeTypes.h"
#include "CommonAIAgentComponent.h"
#include "CommonAIDailyLifeComponent.generated.h"

class UCommonAIDailyLifeProfile;
struct FCommonAIDailyActivityDefinition;
class UCommonAISmartObjectSite;
class UPathFollowingComponent;
class AController;
class AAIController;
class APawn;
struct FPathFollowingResult;

UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class COMMONAI_API UCommonAIDailyLifeComponent : public UActorComponent,
    public ICommonAIDailyLifeContext, public ICommonAIMovement
{
    GENERATED_BODY()
public:
    UCommonAIDailyLifeComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daily Life") TObjectPtr<UCommonAIDailyLifeProfile> Profile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daily Life") FCommonAIDailyLifeState InitialState;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Daily Life") TObjectPtr<UCommonAISmartObjectSite> Home;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Daily Life") TObjectPtr<UCommonAISmartObjectSite> Work;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Daily Life") TArray<TObjectPtr<UCommonAISmartObjectSite>> Destinations;
    // Initially unset uses the shared clock. Explicit initialization retains its context across shutdown.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daily Life", meta=(MustImplement="/Script/CommonAI.CommonAIDailyLifeContext")) TObjectPtr<UObject> RoutineContext;
    UFUNCTION(BlueprintCallable, Category="Common AI|Daily Life") bool InitializeRoutine(UCommonAIAgentComponent* InAgent, UObject* InContext);
    UFUNCTION(BlueprintCallable, Category="Common AI|Daily Life") void ShutdownRoutine();
    UFUNCTION(BlueprintPure, Category="Common AI|Daily Life") FCommonAIDailyLifeState ExportState() const;
    UFUNCTION(BlueprintCallable, Category="Common AI|Daily Life") bool RestoreState(const FCommonAIDailyLifeState& SavedState);
    bool CanRestoreState(const FCommonAIDailyLifeState& SavedState) const;
    UFUNCTION(BlueprintCallable, Category="Common AI|Daily Life") void RefreshRoutine();
    UFUNCTION(BlueprintPure, Category="Common AI|Daily Life") bool IsRoutineInitialized() const { return Agent.IsValid(); }
    UFUNCTION(BlueprintPure, Category="Common AI|Daily Life") FString GetDebugText() const;
    UCommonAIAgentComponent* GetRoutineAgent() const { return Agent.Get(); }
    FGameplayTag GetInterruptionReason() const { return InterruptionReason; }
    double GetCurrentTimeHours() const;
    bool PreparePersistentState();
    UFUNCTION(BlueprintCallable, Category="Common AI|Daily Life") void RefreshControllerBinding();
    void ReleaseControllerBinding();
    bool GetRoutineMoveFacingLocation(FVector& OutLocation) const;
    virtual double GetDailyLifeTimeHours_Implementation() const override;
    virtual FGameplayTag GetDailyLifeInterruptionReason_Implementation() const override;
    virtual ECommonAIMoveStatus BeginMove_Implementation(const FCommonAIInteractionRequest& Request, AActor* Target,
        float AcceptanceRadiusCentimeters) override;
    virtual ECommonAIMoveStatus GetMoveStatus_Implementation(const FCommonAIInteractionRequest& Request) const override;
    virtual void CancelMove_Implementation(const FCommonAIInteractionRequest& Request) override;
    // Representation handoff preserves state; the new representation supplies its own executor.
    static void AdvanceUnrepresentedNeeds(FCommonAIDailyLifeState& InOutState, const UCommonAIDailyLifeProfile& Definition, double ToHours);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual UPathFollowingComponent* GetNavigationAuthority(AAIController* Controller) const;
    virtual FAIRequestID RequestNavigationMove(AAIController* Controller, const FVector& Destination, float AcceptanceRadius);
    virtual void OnControllerBound(AAIController* Controller) {}
    virtual void OnControllerUnbinding(AAIController* Controller) {}
    bool HasNavigationConflict() const;
private:
    UFUNCTION() void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
    bool RegisterPersistentState();
    UObject* ResolveRoutineContext() const;
    bool InitializeRoutineInternal(UCommonAIAgentComponent* InAgent, UObject* InContext, bool bRegisterResident);
    bool CanInitializeRoutine(const UCommonAIAgentComponent* InAgent) const;
    void TryBindController(AAIController* Controller);
    void ClearControllerBinding();
    bool IsControllerBindingDraining(const AAIController* Controller) const;
    void QueueControllerBindingRetry(AAIController* Controller);
    void RetryControllerBinding();
    void CancelControllerBindingRetry();
    void HandleMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result);
    bool MatchesMove(const FCommonAIInteractionRequest& Request) const;
    void ReleaseOwnedMove();
    UFUNCTION() void HandleStarted(FCommonAIInteractionRequest Request);
    UFUNCTION() void HandleProgress(FCommonAIActivityProgress Progress);
    UFUNCTION() void HandleEnded(FCommonAIInteractionRequest Request, FCommonAIInteractionResult Result);
    UFUNCTION() void HandleInputsReset(FGuid Generation);
    void PublishInputs();
    void AdvanceNeeds(double Now);
    void RegisterDestinations();
    void ReleaseCommitment();
    UCommonAISmartObjectSite* ChooseDestination(const FCommonAIDailyActivityDefinition& Activity) const;
    UPROPERTY(Transient) FCommonAIDailyLifeState State;
    UPROPERTY(Transient) TObjectPtr<UObject> ContextProvider;
    UPROPERTY(Transient) TObjectPtr<UCommonAIProfile> AgentProfile;
    TWeakObjectPtr<UCommonAIAgentComponent> Agent;
    FTimerHandle ObservationTimer;
    FGuid CommitmentHold;
    FGuid InterruptionHold;
    FGuid ActiveExecution;
    FGuid RetiredExecution;
    FGameplayTag InterruptionReason;
    FString LastTransition;
    FString LastIssue;
    double StartedAtHours = 0.0;
    double ProgressAtStartHours = 0.0;
    bool bInteracting = false;
    bool bCompletionRequested = false;
    bool bStateInitialized = false;
    // A destroyed external provider must report unavailable time instead of changing clock domains.
    bool bRequiresExternalContext = false;
    bool bShuttingDown = false;
    bool bInitializingRoutine = false;
    bool bEndingPlay = false;
    bool bReleasingControllerBinding = false;
    TWeakObjectPtr<AAIController> BoundController;
    TWeakObjectPtr<UCommonAIAgentComponent> BoundAgent;
    TWeakObjectPtr<AAIController> PendingController;
    TWeakObjectPtr<UCommonAIDailyLifeProfile> PendingProfile;
    FTimerHandle BindingRetryTimer;
    double BindingRetryDeadlineSeconds = 0.0;
    TWeakObjectPtr<UPathFollowingComponent> BoundNavigation;
    FDelegateHandle MoveFinishedHandle;
    UPROPERTY(Transient) FCommonAIInteractionRequest OwnedMove;
    FAIRequestID OwnedMoveId;
    ECommonAIMoveStatus MoveStatus = ECommonAIMoveStatus::Failed;
    FVector MoveDestination = FVector::ZeroVector;
    float MoveAcceptanceRadius = 0.0f;
};
