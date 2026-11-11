#include "Contracts/CommonAISmartObjectSite.h"
#include "CommonAIAgentComponent.h"
#include "CommonAITags.h"
#include "AIController.h"
#include "Async/Async.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "SmartObjectComponent.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"

namespace
{
bool IsClaimForRequest(const USmartObjectSubsystem* Subsystem, const FSmartObjectClaimHandle& Claim,
    const FCommonAIInteractionRequest& Request)
{
    if (!Claim.IsValid() || !Subsystem || !Subsystem->IsSmartObjectValid(Claim.SmartObjectHandle)) return false;
    bool bOwnsSlot = false;
    Subsystem->ReadSlotData(Claim.SlotHandle, [&Request, &bOwnsSlot](FConstSmartObjectSlotView Slot)
    {
        const FCommonAIInteractionRequest* User = Slot.GetUserData().GetPtr<FCommonAIInteractionRequest>();
        bOwnsSlot = User && User->Execution == Request.Execution && User->Operation == Request.Operation
            && User->Agent == Request.Agent && User->Generation == Request.Generation
            && User->Pawn == Request.Pawn && User->Activity == Request.Activity
            && (Slot.GetState() == ESmartObjectSlotState::Claimed || Slot.GetState() == ESmartObjectSlotState::Occupied);
    });
    return bOwnsSlot;
}
}

UCommonAISmartObjectSite::UCommonAISmartObjectSite()
{
    bWaitForSafeCancellation = false;
}

bool UCommonAISmartObjectSite::ResolvePoint(USmartObjectSubsystem*& OutSubsystem, FSmartObjectSlotHandle& OutSlot) const
{
    OutSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USmartObjectSubsystem>() : nullptr;
    const USmartObjectComponent* Component = SmartObject ? SmartObject.Get()
        : IsValid(GetOwner()) ? GetOwner()->FindComponentByClass<USmartObjectComponent>() : nullptr;
    if (!OutSubsystem || !IsValid(Component) || !IsValid(GetOwner()) || Component->GetOwner() != GetOwner()) return false;
    const USmartObjectDefinition* Definition = Component->GetDefinition();
    if (!Definition || Definition->GetSlots().Num() != 1
        || !Definition->GetBehaviorDefinition(0, UCommonAISmartObjectBehaviorDefinition::StaticClass())
        || !OutSubsystem->IsSmartObjectValid(Component->GetRegisteredHandle())) return false;
    TArray<FSmartObjectSlotHandle> Slots;
    OutSubsystem->GetAllSlots(Component->GetRegisteredHandle(), Slots);
    if (Slots.Num() != 1) return false;
    const TOptional<FTransform> Transform = OutSubsystem->GetSlotTransform(Slots[0]);
    if (!Transform.IsSet() || !Transform->GetLocation().Equals(GetOwner()->GetActorLocation(), UE_KINDA_SMALL_NUMBER)) return false;
    OutSlot = Slots[0];
    return true;
}

bool UCommonAISmartObjectSite::IsSlotAvailable(const FCommonAIInteractionRequest& Request,
    USmartObjectSubsystem& Subsystem, const FSmartObjectSlotHandle& Slot) const
{
    FSmartObjectRequestFilter Filter;
    Filter.BehaviorDefinitionClasses.Add(UCommonAISmartObjectBehaviorDefinition::StaticClass());
    if (const UCommonAIAgentComponent* Agent = Request.Agent->FindComponentByClass<UCommonAIAgentComponent>())
        Filter.UserTags = Agent->GetEffectiveTags();
    TArray<FSmartObjectSlotHandle> Slots;
    const AActor* UserActor = Request.Pawn.IsValid() ? static_cast<AActor*>(Request.Pawn.Get()) : Request.Agent.Get();
    const FSmartObjectActorUserData UserData(UserActor);
    Subsystem.FindSlots(Slot.GetSmartObjectHandle(), Filter, Slots, FConstStructView::Make(UserData));
    return Slots.Num() == 1 && Slots[0] == Slot;
}

FCommonAIInteractionOffer UCommonAISmartObjectSite::QueryInteraction_Implementation(const FCommonAIInteractionRequest& Request) const
{
    FCommonAIInteractionOffer Offer = Super::QueryInteraction_Implementation(Request);
    const bool bOwnsRequest = Owns(Request);
    if (!Offer.bAvailable && !bOwnsRequest) return Offer;
    USmartObjectSubsystem* Subsystem = nullptr;
    FSmartObjectSlotHandle Slot;
    if (bChangingClaim || bTearingDown || !SiteId.IsValid() || !ResolvePoint(Subsystem, Slot))
    {
        Offer.bAvailable = false;
        return Offer;
    }
    // A reservation is deliberately hidden from other agents, but its own approach must remain eligible.
    if (bOwnsRequest)
    {
        Offer.bAvailable = HasClaim() && Claim.SlotHandle == Slot;
        return Offer;
    }
    Offer.bAvailable = Offer.bAvailable && IsSlotAvailable(Request, *Subsystem, Slot);
    return Offer;
}

bool UCommonAISmartObjectSite::ReserveInteraction_Implementation(const FCommonAIInteractionRequest& Request)
{
    const FCommonAIInteractionRequest Lease = Request;
    if (bChangingClaim || Owns(Lease) || !Super::ReserveInteraction_Implementation(Lease)) return false;
    TGuardValue<bool> ClaimGuard(bChangingClaim, true);
    USmartObjectSubsystem* Subsystem = nullptr;
    FSmartObjectSlotHandle Slot;
    if (!ResolvePoint(Subsystem, Slot) || !IsSlotAvailable(Lease, *Subsystem, Slot))
    {
        ClearExecution();
        return false;
    }
    ClaimSubsystem = Subsystem;
    // Store the interaction identity in engine-owned slot data, allowing inspection to verify ownership.
    Claim = Subsystem->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal, FConstStructView::Make(Lease));
    if (bTearingDown || !Owns(Lease))
    {
        // Component destruction may occur in the engine's claim notification, before the handle is returned.
        const FSmartObjectClaimHandle UnfinishedClaim = Claim;
        Claim.Invalidate();
        ClaimSubsystem.Reset();
        if (IsClaimForRequest(Subsystem, UnfinishedClaim, Lease)) Subsystem->MarkSlotAsFree(UnfinishedClaim);
        return false;
    }
    if (!HasClaim())
    {
        ClearExecution();
        return false;
    }
    Subsystem->RegisterSlotInvalidationCallback(Claim, FOnSlotInvalidated::CreateUObject(this, &ThisClass::OnSlotInvalidated));
    FOnSmartObjectEvent* Events = Subsystem->GetSlotEventDelegate(Claim.SlotHandle);
    if (!Events) { ClearExecution(); return false; }
    const FSmartObjectClaimHandle ExpectedClaim = Claim;
    TWeakObjectPtr<UCommonAISmartObjectSite> WeakThis(this);
    SlotEventBinding = Events->AddWeakLambda(this, [WeakThis, ExpectedClaim](const FSmartObjectEventData& Event)
    {
        // Disabling does not abort engine claims. Treat it as target loss for this interaction.
        if (Event.SmartObjectHandle != ExpectedClaim.SmartObjectHandle
            || (Event.Reason != ESmartObjectChangeReason::OnObjectDisabled
                && (Event.Reason != ESmartObjectChangeReason::OnSlotDisabled || Event.SlotHandle != ExpectedClaim.SlotHandle))) return;
        auto HandleDisabled = [WeakThis, ExpectedClaim]()
        {
            if (WeakThis.IsValid()) WeakThis->HandleDisabledClaim(ExpectedClaim);
        };
        // Smart Object event delegates may broadcast off the game thread; UObject state remains game-thread owned.
        if (IsInGameThread()) HandleDisabled();
        else AsyncTask(ENamedThreads::GameThread, MoveTemp(HandleDisabled));
    });
    bool bEnabled = false;
    Subsystem->ReadSlotData(Claim.SlotHandle, [&bEnabled](FConstSmartObjectSlotView View) { bEnabled = View.IsEnabled(); });
    // A claim notification may have disabled the point before our event subscription was installed.
    if (!bEnabled) { ClearExecution(); return false; }
    return true;
}

FCommonAIInteractionResult UCommonAISmartObjectSite::BeginInteraction_Implementation(const FCommonAIInteractionRequest& Request)
{
    if (bChangingClaim || !Owns(Request) || Result.Status != ECommonAIInteractionStatus::Reserved) return {};
    if (!HasClaim() || !QueryInteraction_Implementation(Request).bAvailable)
    {
        Result.Status = ECommonAIInteractionStatus::Failed;
        Result.Reason = CommonAITags::Reason_TargetLost;
        return Result;
    }
    {
        TGuardValue<bool> ClaimGuard(bChangingClaim, true);
        if (!ClaimSubsystem->MarkSlotAsOccupied<UCommonAISmartObjectBehaviorDefinition>(Claim))
        {
            Result.Status = ECommonAIInteractionStatus::Failed;
            Result.Reason = CommonAITags::Reason_InteractionFailed;
            return Result;
        }
    }
    // Engine callbacks can invalidate a point during occupation.
    if (!HasClaim() || Result.Status != ECommonAIInteractionStatus::Reserved) return GetInteractionResult_Implementation(Request);
    return Super::BeginInteraction_Implementation(Request);
}

void UCommonAISmartObjectSite::RequestInteractionCancellation_Implementation(
    const FCommonAIInteractionRequest& Request, FGameplayTag Reason)
{
    if (bChangingClaim) return;
    Super::RequestInteractionCancellation_Implementation(Request, Reason);
}

bool UCommonAISmartObjectSite::ReleaseInteraction_Implementation(const FCommonAIInteractionRequest& Request, bool bEmergencyTeardown)
{
    return !bChangingClaim && Super::ReleaseInteraction_Implementation(Request, bEmergencyTeardown);
}

bool UCommonAISmartObjectSite::HasClaim() const
{
    return IsClaimForRequest(ClaimSubsystem.Get(), Claim, Reservation);
}

void UCommonAISmartObjectSite::OnSlotInvalidated(const FSmartObjectClaimHandle& InvalidatedClaim, ESmartObjectSlotState PreviousState)
{
    if (InvalidatedClaim != Claim) return;
    TGuardValue<bool> ClaimGuard(bChangingClaim, true);
    const FCommonAIInteractionRequest Abandoned = FailLostClaim();
    // The engine is already releasing this slot. Do not release it recursively or unbind a future owner's callback.
    RemoveSlotEventBinding();
    Claim.Invalidate();
    ClaimSubsystem.Reset();
    if (Abandoned.Execution.IsValid()) OnInteractionAbandoned.Broadcast(Abandoned);
}

void UCommonAISmartObjectSite::HandleDisabledClaim(const FSmartObjectClaimHandle& DisabledClaim)
{
    if (DisabledClaim != Claim) return;
    TGuardValue<bool> ClaimGuard(bChangingClaim, true);
    const FCommonAIInteractionRequest Abandoned = FailLostClaim();
    ReleaseClaim();
    if (Abandoned.Execution.IsValid()) OnInteractionAbandoned.Broadcast(Abandoned);
}

FCommonAIInteractionRequest UCommonAISmartObjectSite::FailLostClaim()
{
    if (Result.IsTerminal()) return {};
    const bool bEffectStarted = Result.Status == ECommonAIInteractionStatus::Running
        || Result.Status == ECommonAIInteractionStatus::Cancelling;
    Result.Status = ECommonAIInteractionStatus::Failed;
    Result.Reason = CommonAITags::Reason_TargetLost;
    return bEffectStarted ? Reservation : FCommonAIInteractionRequest{};
}

void UCommonAISmartObjectSite::RemoveSlotEventBinding()
{
    if (SlotEventBinding.IsValid())
    {
        if (USmartObjectSubsystem* Subsystem = ClaimSubsystem.Get();
            Subsystem && Subsystem->IsSmartObjectValid(Claim.SmartObjectHandle))
            if (FOnSmartObjectEvent* Events = Subsystem->GetSlotEventDelegate(Claim.SlotHandle)) Events->Remove(SlotEventBinding);
        SlotEventBinding.Reset();
    }
}

void UCommonAISmartObjectSite::ReleaseClaim()
{
    TGuardValue<bool> ClaimGuard(bChangingClaim, true);
    USmartObjectSubsystem* Subsystem = ClaimSubsystem.Get();
    const FSmartObjectClaimHandle PreviousClaim = Claim;
    const bool bOwnedClaim = HasClaim();
    RemoveSlotEventBinding();
    Claim.Invalidate();
    ClaimSubsystem.Reset();
    if (bOwnedClaim && Subsystem)
    {
        Subsystem->UnregisterSlotInvalidationCallback(PreviousClaim);
        Subsystem->MarkSlotAsFree(PreviousClaim);
    }
}

void UCommonAISmartObjectSite::ClearExecution()
{
    ReleaseClaim();
    Super::ClearExecution();
}

ACommonAIActivityLocation::ACommonAIActivityLocation()
{
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SmartObject = CreateDefaultSubobject<USmartObjectComponent>(TEXT("SmartObject"));
    SmartObject->SetupAttachment(SceneRoot);
    Site = CreateDefaultSubobject<UCommonAISmartObjectSite>(TEXT("Site"));
    Site->SmartObject = SmartObject;
}
