#include "CommonAIAgentComponent.h"
#include "CommonAIProfile.h"
#include "CommonAITags.h"
#include "Athena/CommonAIBackend.h"
#include "Activities/CommonAIActivities.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "Contracts/CommonAIMovement.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

UCommonAIAgentComponent::UCommonAIAgentComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

UCommonAIAgentComponent* UCommonAIAgentComponent::FindAgent(AActor* Actor)
{
    if (!IsValid(Actor)) return nullptr;
    if (UCommonAIAgentComponent* Direct = Actor->FindComponentByClass<UCommonAIAgentComponent>()) return Direct;
    if (const APawn* Pawn = Cast<APawn>(Actor))
        return Pawn->GetController() ? Pawn->GetController()->FindComponentByClass<UCommonAIAgentComponent>() : nullptr;
    return nullptr;
}

bool UCommonAIAgentComponent::EnsureBackend()
{
    if (Backend) return true;
    AAIController* Controller = Cast<AAIController>(GetOwner());
    if (!Controller || !Profile) return false;
    if (!Provider)
    {
        Provider = Controller->FindComponentByClass<UCommonAIWorkProviderComponent>();
        if (!Provider)
        {
            Provider = NewObject<UCommonAIWorkProviderComponent>(Controller);
            Provider->RegisterComponent();
        }
    }
    UCommonAIBackend* Created = NewObject<UCommonAIBackend>(Controller);
    if (!Created->Prepare(this, Profile)) return false;
    Backend = Created;
    Backend->RegisterComponent();
    return Backend->GetDataManager() != nullptr;
}

void UCommonAIAgentComponent::BeginPlay()
{
    Super::BeginPlay();
    if (AAIController* Controller = Cast<AAIController>(GetOwner()))
        Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &UCommonAIAgentComponent::HandlePawnChanged);
    if (bStartAutomatically) StartAgent();
}

bool UCommonAIAgentComponent::StartAgent()
{
    if (bStopPending || PendingProfile || !EnsureBackend()) return false;
    if (!Backend->IsEnabled() && !Backend->EnableAthenaAI(false)) return false;
    Snapshot.bEnabled = true;
    Backend->GetDataManager()->ResumeScoring();
    Backend->Observe();
    return true;
}

void UCommonAIAgentComponent::StopAgent()
{
    bRestartAfterDrain = false;
    bStopPending = true;
    RequestCancellation(CommonAITags::Reason_Disabled);
    ApplyPendingOperation();
}

bool UCommonAIAgentComponent::SetProfile(UCommonAIProfile* NewProfile)
{
    if (!IsValid(NewProfile) || !NewProfile->IsUsable()) return false;
    PendingProfile = NewProfile;
    RequestCancellation(CommonAITags::Reason_ProfileChanged);
    ApplyPendingOperation();
    return true;
}

void UCommonAIAgentComponent::RequestCancellation(FGameplayTag Reason)
{
    if (Backend)
        if (UCommonAIWorkTask* Work = Backend->GetRunningWork())
        {
            Snapshot.CancellationReason = Reason;
            Work->RequestCancellation(Reason);
        }
}

void UCommonAIAgentComponent::ApplyPendingOperation()
{
    if (Backend && Backend->GetRunningWork()) return;
    if (!bStopPending && !PendingProfile) return;
    const AAIController* Controller = Cast<AAIController>(GetOwner());
    const bool bRestart = (Snapshot.bEnabled && !bStopPending)
        || (bRestartAfterDrain && Controller && Controller->GetPawn());
    if (Backend) Backend->DisableAthenaAI();
    Snapshot.bEnabled = false;
    if (PendingProfile)
    {
        if (Backend) Backend->DestroyComponent();
        Backend = nullptr;
        Profile = PendingProfile;
        PendingProfile = nullptr;
    }
    bStopPending = false;
    bRestartAfterDrain = false;
    if (bRestart) StartAgent();
}

void UCommonAIAgentComponent::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    if (OldPawn)
    {
        StopAgent();
        if (OldPawn->IsActorBeingDestroyed() && Backend)
        {
            if (UCommonAIWorkTask* Work = Backend->GetRunningWork())
            {
                Work->SetFailureReason(CommonAITags::Reason_TargetLost);
                Work->Stop(Backend->GetDataManager(), Cast<AAIController>(GetOwner()), EAAITaskResult::Aborted);
            }
            ApplyPendingOperation();
        }
        for (int32 Index = MontageWindows.Num() - 1; Index >= 0; --Index)
        {
            const FMontageWindow& Window = MontageWindows[Index];
            if (Window.Mesh.IsValid() && Window.Mesh->GetOwner() == OldPawn)
            {
                RemoveTagContribution(Window.Mesh.Get(), Window.Contribution);
                MontageWindows.RemoveAt(Index);
            }
        }
    }
    // A replacement pawn cannot acquire execution while the old pawn is draining.
    if (NewPawn && bStartAutomatically)
    {
        if (bStopPending) bRestartAfterDrain = true;
        else StartAgent();
    }
}

bool UCommonAIAgentComponent::AddInteractionTarget(UObject* Target)
{
    if (!Provider)
    {
        if (!Cast<AAIController>(GetOwner())) return false;
        Provider = GetOwner()->FindComponentByClass<UCommonAIWorkProviderComponent>();
        if (!Provider)
        {
            Provider = NewObject<UCommonAIWorkProviderComponent>(GetOwner());
            Provider->RegisterComponent();
        }
    }
    return Provider->AddTarget(Target);
}

void UCommonAIAgentComponent::RemoveInteractionTarget(UObject* Target)
{
    if (Provider) Provider->RemoveTarget(Target);
}

bool UCommonAIAgentComponent::SetMovementAdapter(UObject* Adapter)
{
    if (Backend && Backend->GetRunningWork()) return false;
    if (Adapter && (!IsValid(Adapter) || !Adapter->GetClass()->ImplementsInterface(UCommonAIMovement::StaticClass()))) return false;
    MovementAdapter = Adapter;
    return true;
}

FGuid UCommonAIAgentComponent::AddTagContribution(UObject* Source, const FGameplayTagContainer& Tags)
{
    if (!IsValid(Source) || Tags.IsEmpty()) return {};
    FCommonAITagContribution& Entry = TagContributions.AddDefaulted_GetRef();
    Entry.Id = FGuid::NewGuid();
    Entry.Source = Source;
    Entry.Tags = Tags;
    return Entry.Id;
}

bool UCommonAIAgentComponent::RemoveTagContribution(UObject* Source, FGuid Contribution)
{
    return TagContributions.RemoveAll([Source, Contribution](const FCommonAITagContribution& Entry)
    { return Entry.Id == Contribution && Entry.Source.Get() == Source; }) > 0;
}

FGameplayTagContainer UCommonAIAgentComponent::GetEffectiveTags() const
{
    FGameplayTagContainer Result;
    for (const FCommonAITagContribution& Entry : TagContributions)
        if (Entry.Source.IsValid()) Result.AppendTags(Entry.Tags);
    return Result;
}

void UCommonAIAgentComponent::BeginMontageTagWindow(USkeletalMeshComponent* Mesh, UAnimNotifyState* Notify,
    int32 MontageInstanceId, const FGameplayTagContainer& Tags)
{
    if (!Mesh || !Notify || MontageInstanceId == INDEX_NONE) return;
    EndMontageTagWindow(Mesh, Notify, MontageInstanceId);
    FMontageWindow Window;
    Window.Mesh = Mesh;
    Window.AnimInstance = Mesh->GetAnimInstance();
    Window.Notify = Notify;
    Window.MontageInstanceId = MontageInstanceId;
    Window.Contribution = AddTagContribution(Mesh, Tags);
    MontageWindows.Add(Window);
}

void UCommonAIAgentComponent::EndMontageTagWindow(USkeletalMeshComponent* Mesh, UAnimNotifyState* Notify, int32 MontageInstanceId)
{
    for (int32 Index = MontageWindows.Num() - 1; Index >= 0; --Index)
    {
        const FMontageWindow& Window = MontageWindows[Index];
        if (Window.Mesh.Get() == Mesh && Window.Notify.Get() == Notify && Window.MontageInstanceId == MontageInstanceId)
        {
            RemoveTagContribution(Mesh, Window.Contribution);
            MontageWindows.RemoveAt(Index);
        }
    }
}

void UCommonAIAgentComponent::SetExecutionSnapshot(ECommonAIPhase Phase, const FCommonAIInteractionRequest& Request, UObject* Target, FGameplayTag Reason)
{
    if (Snapshot.Phase != ECommonAIPhase::Cancelling && Phase == ECommonAIPhase::Cancelling)
    {
        Snapshot.CancellationElapsedSeconds = 0.0f;
        Snapshot.CancellationReason = Reason;
        CancellingContribution = AddTagContribution(this, FGameplayTagContainer(CommonAITags::State_Busy_Cancelling));
    }
    Snapshot.Phase = Phase;
    Snapshot.Activity = Request.Activity;
    Snapshot.Execution = Request.Execution;
    Snapshot.Target = Target;
}

void UCommonAIAgentComponent::RecordExecutionResult(const FCommonAIInteractionResult& Result)
{
    Snapshot.LastResult = Result;
    Snapshot.Phase = ECommonAIPhase::Idle;
    Snapshot.Target.Reset();
    Snapshot.Execution.Invalidate();
    Snapshot.Activity = CommonAITags::Activity_Wait;
    Snapshot.bCancellationOverdue = false;
    RemoveTagContribution(this, CancellingContribution);
    CancellingContribution.Invalidate();
}

void UCommonAIAgentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    for (int32 Index = MontageWindows.Num() - 1; Index >= 0; --Index)
    {
        const FMontageWindow& Window = MontageWindows[Index];
        if (!Window.Mesh.IsValid() || !Window.AnimInstance.IsValid()
            || Window.Mesh->GetAnimInstance() != Window.AnimInstance.Get()
            || FindAgent(Window.Mesh->GetOwner()) != this
            || !Window.AnimInstance->GetMontageInstanceForID(Window.MontageInstanceId))
        {
            RemoveTagContribution(Window.Mesh.Get(), Window.Contribution);
            MontageWindows.RemoveAt(Index);
        }
    }
    TagContributions.RemoveAll([](const FCommonAITagContribution& Entry) { return !Entry.Source.IsValid(); });
    if (Snapshot.Phase == ECommonAIPhase::Cancelling)
    {
        Snapshot.CancellationElapsedSeconds += DeltaTime;
        Snapshot.bCancellationOverdue = Profile && Profile->CancellationWarningSeconds > 0.0f
            && Snapshot.CancellationElapsedSeconds >= Profile->CancellationWarningSeconds;
    }
    ApplyPendingOperation();
    ObservationElapsedSeconds += DeltaTime;
    if (Backend && Profile && Snapshot.bEnabled && !bStopPending && !PendingProfile
        && ObservationElapsedSeconds >= Profile->ObservationIntervalSeconds)
    {
        ObservationElapsedSeconds = 0.0f;
        Backend->Observe();
    }
}

void UCommonAIAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AAIController* Controller = Cast<AAIController>(GetOwner()))
        Controller->OnPossessedPawnChanged.RemoveDynamic(this, &UCommonAIAgentComponent::HandlePawnChanged);
    if (Backend) Backend->DestroyComponent();
    Backend = nullptr;
    MontageWindows.Empty();
    TagContributions.Empty();
    Snapshot.bEnabled = false;
    Snapshot.Phase = ECommonAIPhase::Idle;
    Super::EndPlay(EndPlayReason);
}
