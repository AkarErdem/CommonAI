#include "DailyLife/CommonAIDailyLifeComponent.h"
#include "DailyLife/CommonAIDailyLifeClockSubsystem.h"
#include "DailyLife/CommonAIDailyLifeProfile.h"
#include "DailyLife/CommonAIDailyLifeSaveSubsystem.h"
#include "CommonAILog.h"
#include "CommonAITags.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

namespace
{
    constexpr double ControllerBindingTimeoutSeconds = 5.0;
}

void UCommonAIDailyLifeComponent::BeginPlay()
{
    bEndingPlay = false;
    Super::BeginPlay();
    APawn* Pawn = Cast<APawn>(GetOwner());
    if (!Pawn || !Pawn->HasAuthority()) return;
    Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UCommonAIDailyLifeComponent::HandleControllerChanged);
    RefreshControllerBinding();
}

void UCommonAIDailyLifeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    if (APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UCommonAIDailyLifeComponent::HandleControllerChanged);
        if (Pawn->HasAuthority() && GetWorld())
            if (UCommonAIDailyLifeSaveSubsystem* Saves = GetWorld()->GetSubsystem<UCommonAIDailyLifeSaveSubsystem>())
                Saves->UnregisterResident(this, true);
    }
    ReleaseControllerBinding();
    Super::EndPlay(Reason);
}

void UCommonAIDailyLifeComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
    if (bEndingPlay || Pawn != GetOwner() || !Pawn->HasAuthority()) return;
    ReleaseControllerBinding();
    // The pawn event precedes the controller's possession and navigation setup.
    if (AAIController* Controller = Cast<AAIController>(NewController); Profile && Controller)
        QueueControllerBindingRetry(Controller);
}

void UCommonAIDailyLifeComponent::RefreshControllerBinding()
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    if (bEndingPlay || !HasBegunPlay() || !Pawn || !Pawn->HasAuthority()) return;
    AAIController* Controller = Cast<AAIController>(Pawn->GetController());
    if (Profile && Controller && BoundController == Controller && IsRoutineInitialized()) return;
    ReleaseControllerBinding();
    if (!Profile) return;
    if (Controller) TryBindController(Controller);
    else RegisterPersistentState();
}

bool UCommonAIDailyLifeComponent::RegisterPersistentState()
{
    if (!PreparePersistentState()) return false;
    if (UCommonAIDailyLifeSaveSubsystem* Saves = GetWorld()->GetSubsystem<UCommonAIDailyLifeSaveSubsystem>();
        Saves && !Saves->RegisterResident(this))
    {
        UE_LOG(LogCommonAI, Warning, TEXT("Daily life resident registration failed for %s."), *GetNameSafe(GetOwner()));
        return false;
    }
    return !bEndingPlay && !IsBeingDestroyed() && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed();
}

bool UCommonAIDailyLifeComponent::IsControllerBindingDraining(const AAIController* Controller) const
{
    if (!Controller || Controller->GetPawn() != GetOwner()) return false;
    const UCommonAIAgentComponent* RoutineAgent = Controller->FindComponentByClass<UCommonAIAgentComponent>();
    if (!RoutineAgent) return false;
    const FCommonAIAgentSnapshot Snapshot = RoutineAgent->GetSnapshot();
    if (Snapshot.Phase != ECommonAIPhase::Cancelling && !Snapshot.bProfileChangePending) return false;
    if (const UObject* Source = RoutineAgent->GetInputSource(CommonAIDailyLife::SourceTag()); Source && Source != this)
    {
        const UCommonAIDailyLifeComponent* PreviousRoutine = Cast<UCommonAIDailyLifeComponent>(Source);
        if (!PreviousRoutine || PreviousRoutine->GetRoutineAgent() == RoutineAgent) return false;
    }
    return true;
}

void UCommonAIDailyLifeComponent::TryBindController(AAIController* Controller)
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (bEndingPlay || bReleasingControllerBinding || !Pawn || !Pawn->HasAuthority() || !Controller || Pawn->GetController() != Controller || !Profile
        || !Profile->ValidateProfile().IsEmpty())
    {
        CancelControllerBindingRetry();
        return;
    }
    if (Controller->GetPawn() != Pawn || IsControllerBindingDraining(Controller))
    {
        QueueControllerBindingRetry(Controller);
        return;
    }
    UObject* Context = ResolveRoutineContext();
    if (!IsValid(Context) || !Context->GetClass()->ImplementsInterface(UCommonAIDailyLifeContext::StaticClass())
        || !RegisterPersistentState() || Pawn->GetController() != Controller || Controller->GetPawn() != Pawn)
    {
        CancelControllerBindingRetry();
        return;
    }
    UCommonAIAgentComponent* RoutineAgent = Controller->FindComponentByClass<UCommonAIAgentComponent>();
    if (!RoutineAgent)
    {
        RoutineAgent = NewObject<UCommonAIAgentComponent>(Controller);
        RoutineAgent->bStartAutomatically = false;
        RoutineAgent->PrimaryComponentTick.bStartWithTickEnabled = false;
        RoutineAgent->RegisterComponent();
    }
    // Check profile and producer ownership before changing any execution adapter.
    if (!CanInitializeRoutine(RoutineAgent)
        || (RoutineAgent->GetMovementAdapter() && RoutineAgent->GetMovementAdapter() != this))
    {
        CancelControllerBindingRetry();
        UE_LOG(LogCommonAI, Warning, TEXT("Daily life controller binding rejected for %s: an existing agent owns its profile, inputs or movement."), *GetNameSafe(Pawn));
        return;
    }
    UPathFollowingComponent* Navigation = GetNavigationAuthority(Controller);
    if (!Navigation || !RoutineAgent->SetMovementAdapter(this))
    {
        CancelControllerBindingRetry();
        UE_LOG(LogCommonAI, Warning, TEXT("Daily life controller binding rejected for %s: navigation is unavailable or an activity still owns movement."), *GetNameSafe(Pawn));
        return;
    }
    BoundController = Controller;
    BoundAgent = RoutineAgent;
    BoundNavigation = Navigation;
    MoveFinishedHandle = Navigation->OnRequestFinished.AddUObject(this, &UCommonAIDailyLifeComponent::HandleMoveFinished);
    OnControllerBound(Controller);
    if (!InitializeRoutineInternal(RoutineAgent, Context, false))
    {
        const bool bDraining = IsControllerBindingDraining(Controller);
        ClearControllerBinding();
        if (bDraining) QueueControllerBindingRetry(Controller);
        else
        {
            CancelControllerBindingRetry();
            UE_LOG(LogCommonAI, Warning, TEXT("Daily life initialization failed for %s."), *GetNameSafe(Pawn));
        }
        return;
    }
    CancelControllerBindingRetry();
}

void UCommonAIDailyLifeComponent::ReleaseControllerBinding()
{
    CancelControllerBindingRetry();
    ClearControllerBinding();
}

void UCommonAIDailyLifeComponent::ClearControllerBinding()
{
    if (bReleasingControllerBinding) return;
    TGuardValue<bool> ReleaseGuard(bReleasingControllerBinding, true);
    ShutdownRoutine();
    ReleaseOwnedMove();
    if (UPathFollowingComponent* Navigation = BoundNavigation.Get())
        Navigation->OnRequestFinished.Remove(MoveFinishedHandle);
    MoveFinishedHandle.Reset();
    BoundNavigation.Reset();
    if (AAIController* Controller = BoundController.Get()) OnControllerUnbinding(Controller);
    if (UCommonAIAgentComponent* RoutineAgent = BoundAgent.Get(); RoutineAgent && RoutineAgent->GetMovementAdapter() == this)
        RoutineAgent->SetMovementAdapter(nullptr);
    BoundAgent.Reset();
    BoundController.Reset();
}

void UCommonAIDailyLifeComponent::QueueControllerBindingRetry(AAIController* Controller)
{
    if (!GetWorld() || !Profile || !Profile->ValidateProfile().IsEmpty()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (PendingController != Controller || PendingProfile.Get() != Profile.Get())
    {
        PendingController = Controller;
        PendingProfile = Profile.Get();
        BindingRetryDeadlineSeconds = Now + ControllerBindingTimeoutSeconds;
    }
    if (Now >= BindingRetryDeadlineSeconds)
    {
        UE_LOG(LogCommonAI, Warning, TEXT("Daily life controller binding timed out for %s while possession or an activity drained."), *GetNameSafe(GetOwner()));
        CancelControllerBindingRetry();
        return;
    }
    const float Delay = static_cast<float>(FMath::Min(static_cast<double>(Profile->ExecutionIntervalSeconds), BindingRetryDeadlineSeconds - Now));
    GetWorld()->GetTimerManager().SetTimer(BindingRetryTimer, this, &UCommonAIDailyLifeComponent::RetryControllerBinding, Delay, false);
}

void UCommonAIDailyLifeComponent::RetryControllerBinding()
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    AAIController* Controller = PendingController.Get();
    if (bEndingPlay || !HasBegunPlay() || !Pawn || !Pawn->HasAuthority() || !Controller
        || Pawn->GetController() != Controller || Profile != PendingProfile.Get())
    {
        CancelControllerBindingRetry();
        return;
    }
    TryBindController(Controller);
}

void UCommonAIDailyLifeComponent::CancelControllerBindingRetry()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BindingRetryTimer);
    PendingController.Reset();
    PendingProfile.Reset();
    BindingRetryDeadlineSeconds = 0.0;
}

double UCommonAIDailyLifeComponent::GetDailyLifeTimeHours_Implementation() const
{
    const UCommonAIDailyLifeClockSubsystem* Clock = GetWorld() ? GetWorld()->GetSubsystem<UCommonAIDailyLifeClockSubsystem>() : nullptr;
    return Clock ? Clock->GetTimeHours() : -1.0;
}

FGameplayTag UCommonAIDailyLifeComponent::GetDailyLifeInterruptionReason_Implementation() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    const AAIController* Controller = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
    if (!Pawn || !Pawn->HasAuthority() || !Controller) return CommonAITags::Reason_Disabled;
    if (const UBrainComponent* Brain = Controller->GetBrainComponent(); Brain && Brain->IsRunning())
        return CommonAITags::Reason_BrainBusy;
    return HasNavigationConflict() ? CommonAITags::Reason_MovementBusy : FGameplayTag();
}

UPathFollowingComponent* UCommonAIDailyLifeComponent::GetNavigationAuthority(AAIController* Controller) const
{
    return Controller ? Controller->GetPathFollowingComponent() : nullptr;
}

FAIRequestID UCommonAIDailyLifeComponent::RequestNavigationMove(AAIController* Controller, const FVector& Destination, float AcceptanceRadius)
{
    FAIMoveRequest Request(Destination);
    Request.SetAcceptanceRadius(AcceptanceRadius);
    return Controller ? Controller->MoveTo(Request).MoveId : FAIRequestID::InvalidRequest;
}

bool UCommonAIDailyLifeComponent::HasNavigationConflict() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    AAIController* Controller = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
    const UPathFollowingComponent* Navigation = GetNavigationAuthority(Controller);
    return Navigation && Navigation->GetStatus() != EPathFollowingStatus::Idle
        && (!OwnedMoveId.IsValid() || Navigation != BoundNavigation.Get() || Navigation->GetCurrentRequestId() != OwnedMoveId);
}

bool UCommonAIDailyLifeComponent::GetRoutineMoveFacingLocation(FVector& OutLocation) const
{
    const UPathFollowingComponent* Navigation = BoundNavigation.Get();
    if (MoveStatus != ECommonAIMoveStatus::Running || !OwnedMoveId.IsValid() || !Navigation
        || Navigation->GetCurrentRequestId() != OwnedMoveId) return false;
    const FVector Direction = Navigation->GetCurrentDirection().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) return false;
    OutLocation = GetOwner()->GetActorLocation() + Direction;
    return true;
}

bool UCommonAIDailyLifeComponent::MatchesMove(const FCommonAIInteractionRequest& Request) const
{
    return Request.Execution.IsValid() && Request.Execution == OwnedMove.Execution
        && Request.Operation == OwnedMove.Operation && Request.Generation == OwnedMove.Generation
        && Request.Agent == OwnedMove.Agent && Request.Pawn == OwnedMove.Pawn;
}

ECommonAIMoveStatus UCommonAIDailyLifeComponent::BeginMove_Implementation(const FCommonAIInteractionRequest& Request,
    AActor* Target, float AcceptanceRadiusCentimeters)
{
    if (MatchesMove(Request)) return GetMoveStatus_Implementation(Request);
    APawn* Pawn = Cast<APawn>(GetOwner());
    UPathFollowingComponent* Navigation = BoundNavigation.Get();
    if (!Pawn || Request.Pawn.Get() != Pawn || Request.Agent != BoundController.Get() || !Navigation
        || !Request.Execution.IsValid() || !Request.Operation.IsValid() || !Request.Generation.IsValid()
        || !IsValid(Target) || !FMath::IsFinite(AcceptanceRadiusCentimeters)
        || AcceptanceRadiusCentimeters < 0.0f || !BoundAgent.IsValid()
        || Request.Generation != BoundAgent->GetInputGeneration()
        || !ContextProvider || ICommonAIDailyLifeContext::Execute_GetDailyLifeInterruptionReason(ContextProvider).IsValid()
        || HasNavigationConflict() || MoveStatus == ECommonAIMoveStatus::Running)
        return ECommonAIMoveStatus::Failed;
    OwnedMove = Request;
    MoveDestination = Target->GetActorLocation();
    MoveAcceptanceRadius = AcceptanceRadiusCentimeters;
    OwnedMoveId = FAIRequestID::InvalidRequest;
    Navigation->Initialize();
    if (Navigation->HasReached(MoveDestination, EPathFollowingReachMode::OverlapAgent, MoveAcceptanceRadius))
    {
        MoveStatus = ECommonAIMoveStatus::Completed;
        return MoveStatus;
    }
    OwnedMoveId = RequestNavigationMove(BoundController.Get(), MoveDestination, MoveAcceptanceRadius);
    MoveStatus = OwnedMoveId.IsValid() ? ECommonAIMoveStatus::Running : ECommonAIMoveStatus::Failed;
    // MoveTo may finish synchronously when path projection resolves an already reached goal.
    if (OwnedMoveId.IsValid() && Navigation->GetStatus() == EPathFollowingStatus::Idle
        && Navigation->HasReached(MoveDestination, EPathFollowingReachMode::OverlapAgent, MoveAcceptanceRadius))
    {
        OwnedMoveId = FAIRequestID::InvalidRequest;
        MoveStatus = ECommonAIMoveStatus::Completed;
    }
    return GetMoveStatus_Implementation(Request);
}

ECommonAIMoveStatus UCommonAIDailyLifeComponent::GetMoveStatus_Implementation(const FCommonAIInteractionRequest& Request) const
{
    if (!MatchesMove(Request) || !BoundAgent.IsValid() || Request.Generation != BoundAgent->GetInputGeneration())
        return ECommonAIMoveStatus::Failed;
    const UPathFollowingComponent* Navigation = BoundNavigation.Get();
    if (MoveStatus == ECommonAIMoveStatus::Running && (!Navigation
        || Navigation->GetCurrentRequestId() != OwnedMoveId || Navigation->GetStatus() == EPathFollowingStatus::Idle))
        return ECommonAIMoveStatus::Failed;
    return MoveStatus;
}

void UCommonAIDailyLifeComponent::HandleMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result)
{
    if (!OwnedMoveId.IsValid() || OwnedMoveId != RequestId) return;
    const UPathFollowingComponent* Navigation = BoundNavigation.Get();
    const bool bReached = Result.IsSuccess() && Navigation
        && Navigation->HasReached(MoveDestination, EPathFollowingReachMode::OverlapAgent, MoveAcceptanceRadius);
    MoveStatus = bReached ? ECommonAIMoveStatus::Completed : ECommonAIMoveStatus::Failed;
    OwnedMoveId = FAIRequestID::InvalidRequest;
}

void UCommonAIDailyLifeComponent::CancelMove_Implementation(const FCommonAIInteractionRequest& Request)
{
    if (MatchesMove(Request)) ReleaseOwnedMove();
}

void UCommonAIDailyLifeComponent::ReleaseOwnedMove()
{
    const FAIRequestID RequestId = OwnedMoveId;
    OwnedMoveId = FAIRequestID::InvalidRequest;
    MoveStatus = ECommonAIMoveStatus::Failed;
    OwnedMove = {};
    if (UPathFollowingComponent* Navigation = BoundNavigation.Get(); Navigation
        && RequestId.IsValid() && Navigation->GetCurrentRequestId() == RequestId)
        Navigation->AbortMove(*this, FPathFollowingResultFlags::UserAbort, RequestId);
}
