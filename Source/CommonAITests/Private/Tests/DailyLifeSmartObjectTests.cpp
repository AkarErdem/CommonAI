#if WITH_DEV_AUTOMATION_TESTS
#include "Contracts/CommonAISmartObjectSite.h"
#include "CommonAITags.h"
#include "Fixtures/CommonAIFoundationObserver.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"

namespace
{
struct FDailyLifeSmartObjectWorld
{
    UWorld* World = nullptr;
    FDailyLifeSmartObjectWorld()
    {
        const UWorld::InitializationValues Settings = UWorld::InitializationValues()
            .AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
            .CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        World->BeginPlay();
        World->GetWorldSettings()->NotifyBeginPlay();
    }
    ~FDailyLifeSmartObjectWorld()
    {
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    }
    ACommonAIActivityLocation* Location(int32 SlotCount = 1, FVector3f Offset = FVector3f::ZeroVector)
    {
        USmartObjectDefinition* Definition = NewObject<USmartObjectDefinition>(World);
        for (int32 Index = 0; Index < SlotCount; ++Index)
        {
            FSmartObjectSlotDefinition& Slot = Definition->DebugAddSlot();
            Slot.Offset = Offset;
            Slot.BehaviorDefinitions.Add(NewObject<UCommonAISmartObjectBehaviorDefinition>(Definition));
        }
        Definition->Validate();
        ACommonAIActivityLocation* Location = World->SpawnActorDeferred<ACommonAIActivityLocation>(
            ACommonAIActivityLocation::StaticClass(), FTransform::Identity);
        Location->SmartObject->SetDefinition(Definition);
        Location->Site->SiteId = FGuid::NewGuid();
        Location->FinishSpawning(FTransform::Identity);
        return Location;
    }
    FCommonAIInteractionRequest Request()
    {
        FCommonAIInteractionRequest Request;
        Request.Agent = World->SpawnActor<AAIController>();
        Request.Execution = FGuid::NewGuid();
        Request.Operation = FGuid::NewGuid();
        Request.Generation = FGuid::NewGuid();
        Request.Activity = CommonAITags::Activity_Work;
        return Request;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDailyLifeSmartObjectLeaseTest, "CommonAI.DailyLife.SmartObjectLease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDailyLifeSmartObjectLeaseTest::RunTest(const FString& Parameters)
{
    FDailyLifeSmartObjectWorld Fixture;
    USmartObjectSubsystem* Subsystem = Fixture.World->GetSubsystem<USmartObjectSubsystem>();
    if (!TestNotNull(TEXT("Real Smart Object subsystem is available"), Subsystem)) return false;
    ACommonAIActivityLocation* Location = Fixture.Location();
    UCommonAISmartObjectSite* Site = Location->Site;
    UCommonAIFoundationObserver* EffectObserver = NewObject<UCommonAIFoundationObserver>(Site);
    Site->OnInteractionAbandoned.AddDynamic(EffectObserver, &UCommonAIFoundationObserver::OnAbandoned);
    const FCommonAIInteractionRequest First = Fixture.Request();
    const FCommonAIInteractionRequest Second = Fixture.Request();
    TArray<FSmartObjectSlotHandle> Slots;
    Subsystem->GetAllSlots(Location->SmartObject->GetRegisteredHandle(), Slots);
    if (!TestEqual(TEXT("One real engine slot is registered"), Slots.Num(), 1)) return false;
    const FSmartObjectSlotHandle Slot = Slots[0];
    bool bReentrantClaimRejected = false;
    bool bReentrantReleaseRejected = false;
    FOnSmartObjectEvent* Events = Subsystem->GetSlotEventDelegate(Slot);
    const FDelegateHandle ReentryObserver = Events->AddLambda([&](const FSmartObjectEventData& Event)
    {
        if (Event.Reason == ESmartObjectChangeReason::OnClaimed)
            bReentrantClaimRejected = !ICommonAIInteraction::Execute_ReserveInteraction(Site, Second);
        if (Event.Reason == ESmartObjectChangeReason::OnReleased)
            bReentrantReleaseRejected = !ICommonAIInteraction::Execute_ReserveInteraction(Site, Second);
    });

    TestTrue(TEXT("First execution acquires the engine claim"), ICommonAIInteraction::Execute_ReserveInteraction(Site, First));
    TestTrue(TEXT("Engine claim callbacks cannot replace the pending lease"), bReentrantClaimRejected);
    TestTrue(TEXT("The site reports its real claim"), Site->HasClaim());
    TestEqual(TEXT("Engine state is claimed"), Subsystem->GetSlotState(Slot), ESmartObjectSlotState::Claimed);
    TestTrue(TEXT("The current lease remains available while approaching"), ICommonAIInteraction::Execute_QueryInteraction(Site, First).bAvailable);
    FCommonAIInteractionRequest DifferentActivity = First;
    DifferentActivity.Activity = CommonAITags::Activity_Wait;
    TestFalse(TEXT("Another activity cannot reuse the current lease's availability"),
        ICommonAIInteraction::Execute_QueryInteraction(Site, DifferentActivity).bAvailable);
    FCommonAIInteractionRequest DifferentGeneration = First;
    DifferentGeneration.Generation = FGuid::NewGuid();
    TestFalse(TEXT("A stale generation cannot reuse the current lease's availability"),
        ICommonAIInteraction::Execute_QueryInteraction(Site, DifferentGeneration).bAvailable);
    TestEqual(TEXT("A stale generation cannot begin the reserved interaction"),
        ICommonAIInteraction::Execute_BeginInteraction(Site, DifferentGeneration).Status, ECommonAIInteractionStatus::Rejected);
    TestEqual(TEXT("A rejected begin preserves the valid reservation"),
        ICommonAIInteraction::Execute_GetInteractionResult(Site, First).Status, ECommonAIInteractionStatus::Reserved);
    TestFalse(TEXT("A stale generation cannot release a reserved claim"),
        ICommonAIInteraction::Execute_ReleaseInteraction(Site, DifferentGeneration, false));
    TestFalse(TEXT("Emergency release still rejects a stale generation"),
        ICommonAIInteraction::Execute_ReleaseInteraction(Site, DifferentGeneration, true));
    TestFalse(TEXT("Another execution cannot reserve"), ICommonAIInteraction::Execute_ReserveInteraction(Site, Second));
    TestEqual(TEXT("Beginning keeps the logical activity running"), ICommonAIInteraction::Execute_BeginInteraction(Site, First).Status,
        ECommonAIInteractionStatus::Running);
    TestEqual(TEXT("Engine state is occupied"), Subsystem->GetSlotState(Slot), ESmartObjectSlotState::Occupied);
    ICommonAIInteraction::Execute_RequestInteractionCancellation(Site, DifferentGeneration, CommonAITags::Reason_Disabled);
    Site->SignalCompletion(DifferentGeneration, true);
    TestEqual(TEXT("A stale generation cannot cancel or complete the current interaction"),
        ICommonAIInteraction::Execute_GetInteractionResult(Site, First).Status, ECommonAIInteractionStatus::Running);
    Site->SignalCompletion(Second, true);
    TestEqual(TEXT("A stale completion cannot finish this execution"), ICommonAIInteraction::Execute_GetInteractionResult(Site, First).Status,
        ECommonAIInteractionStatus::Running);
    ICommonAIInteraction::Execute_RequestInteractionCancellation(Site, First, CommonAITags::Reason_Disabled);
    TestEqual(TEXT("Logic-only cancellation completes without animation acknowledgement"),
        ICommonAIInteraction::Execute_GetInteractionResult(Site, First).Status, ECommonAIInteractionStatus::Cancelled);
    TestTrue(TEXT("Sequence cleanup releases cancellation"), ICommonAIInteraction::Execute_ReleaseInteraction(Site, First, false));
    TestFalse(TEXT("Cancellation leaves no claim"), Site->HasClaim());
    TestEqual(TEXT("Engine slot is free after cancellation cleanup"), Subsystem->GetSlotState(Slot), ESmartObjectSlotState::Free);
    TestTrue(TEXT("Engine release callbacks cannot interleave a new claim"), bReentrantReleaseRejected);
    Events->Remove(ReentryObserver);

    TestTrue(TEXT("Next execution reserves the released slot"), ICommonAIInteraction::Execute_ReserveInteraction(Site, Second));
    ICommonAIInteraction::Execute_BeginInteraction(Site, Second);
    TestFalse(TEXT("Stale release cannot free the next execution"), ICommonAIInteraction::Execute_ReleaseInteraction(Site, First, true));
    Site->SignalCompletion(Second, true);
    TestEqual(TEXT("Completion is explicit"), ICommonAIInteraction::Execute_GetInteractionResult(Site, Second).Status,
        ECommonAIInteractionStatus::Completed);
    TestTrue(TEXT("Sequence cleanup releases success"), ICommonAIInteraction::Execute_ReleaseInteraction(Site, Second, false));
    TestEqual(TEXT("Engine slot is free after success cleanup"), Subsystem->GetSlotState(Slot), ESmartObjectSlotState::Free);

    TestTrue(TEXT("A fresh claim is available for invalidation"), ICommonAIInteraction::Execute_ReserveInteraction(Site, First));
    TestEqual(TEXT("The effect is active before object invalidation"),
        ICommonAIInteraction::Execute_BeginInteraction(Site, First).Status, ECommonAIInteractionStatus::Running);
    Subsystem->SetEnabled(Location->SmartObject->GetRegisteredHandle(), false);
    TestFalse(TEXT("Engine invalidation clears the owned claim"), Site->HasClaim());
    TestEqual(TEXT("Disabling the object releases the actual engine slot"), Subsystem->GetSlotState(Slot), ESmartObjectSlotState::Free);
    TestFalse(TEXT("A disabled point is unavailable even to its old lease"), ICommonAIInteraction::Execute_QueryInteraction(Site, First).bAvailable);
    TestEqual(TEXT("Invalidation reports failure to the sequence"), ICommonAIInteraction::Execute_GetInteractionResult(Site, First).Status,
        ECommonAIInteractionStatus::Failed);
    TestEqual(TEXT("Invalidation reports target loss"), ICommonAIInteraction::Execute_GetInteractionResult(Site, First).Reason,
        CommonAITags::Reason_TargetLost.GetTag());
    TestTrue(TEXT("Invalidated execution can drain normally"), ICommonAIInteraction::Execute_ReleaseInteraction(Site, First, false));
    TestEqual(TEXT("Invalidation notifies the active effect owner once"), EffectObserver->AbandonmentCount, 1);
    TestEqual(TEXT("The invalidated effect notification carries the original operation"), EffectObserver->LastAbandoned.Operation, First.Operation);
    Subsystem->SetEnabled(Location->SmartObject->GetRegisteredHandle(), true);
    TestTrue(TEXT("The re-enabled point is reusable"), ICommonAIInteraction::Execute_ReserveInteraction(Site, Second));
    ICommonAIInteraction::Execute_BeginInteraction(Site, Second);
    Subsystem->SetSlotEnabled(Slot, false);
    TestFalse(TEXT("Disabling just the occupied slot clears the owned claim"), Site->HasClaim());
    TestEqual(TEXT("Slot disablement releases actual engine occupancy"), Subsystem->GetSlotState(Slot), ESmartObjectSlotState::Free);
    TestEqual(TEXT("Slot disablement reports failure"), ICommonAIInteraction::Execute_GetInteractionResult(Site, Second).Status,
        ECommonAIInteractionStatus::Failed);
    TestTrue(TEXT("The slot-disabled interaction drains"), ICommonAIInteraction::Execute_ReleaseInteraction(Site, Second, false));
    TestEqual(TEXT("Slot disablement notifies the next effect owner once"), EffectObserver->AbandonmentCount, 2);
    TestEqual(TEXT("The disabled effect notification carries the original operation"), EffectObserver->LastAbandoned.Operation, Second.Operation);
    Subsystem->SetSlotEnabled(Slot, true);

    TestTrue(TEXT("A free slot can be claimed before engine preemption"), ICommonAIInteraction::Execute_ReserveInteraction(Site, First));
    const FSmartObjectClaimHandle HigherPriorityClaim = Subsystem->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::High);
    if (!TestTrue(TEXT("The engine can preempt a reserved claim with higher priority"), HigherPriorityClaim.IsValid())) return false;
    TestFalse(TEXT("Real engine invalidation clears the preempted claim"), Site->HasClaim());
    TestEqual(TEXT("Preemption reports failure"), ICommonAIInteraction::Execute_GetInteractionResult(Site, First).Status,
        ECommonAIInteractionStatus::Failed);
    TestTrue(TEXT("The preempted interaction drains"), ICommonAIInteraction::Execute_ReleaseInteraction(Site, First, false));
    TestEqual(TEXT("Preemption before interaction starts has no effect to detach"), EffectObserver->AbandonmentCount, 2);
    TestEqual(TEXT("Old cleanup cannot release the new engine owner's claim"), Subsystem->GetSlotState(Slot), ESmartObjectSlotState::Claimed);
    TestTrue(TEXT("The engine owner can release its own claim"), Subsystem->MarkSlotAsFree(HigherPriorityClaim));

    TestTrue(TEXT("A released point can reserve before site destruction"), ICommonAIInteraction::Execute_ReserveInteraction(Site, Second));
    ICommonAIInteraction::Execute_BeginInteraction(Site, Second);
    Site->DestroyComponent();
    TestEqual(TEXT("Destroying the site releases its occupied engine slot"), Subsystem->GetSlotState(Slot), ESmartObjectSlotState::Free);

    ACommonAIActivityLocation* DestroyedLocation = Fixture.Location();
    UCommonAISmartObjectSite* DestroyedSite = DestroyedLocation->Site;
    TestTrue(TEXT("Another point reserves before actor destruction"), ICommonAIInteraction::Execute_ReserveInteraction(DestroyedSite, First));
    DestroyedLocation->Destroy();
    TestFalse(TEXT("Actor destruction leaves no claim"), DestroyedSite->HasClaim());

    ACommonAIActivityLocation* ReentrantLocation = Fixture.Location();
    UCommonAISmartObjectSite* ReentrantSite = ReentrantLocation->Site;
    Slots.Reset();
    Subsystem->GetAllSlots(ReentrantLocation->SmartObject->GetRegisteredHandle(), Slots);
    if (!TestEqual(TEXT("Reentrant teardown fixture has a slot"), Slots.Num(), 1)) return false;
    const FSmartObjectSlotHandle ReentrantSlot = Slots[0];
    FOnSmartObjectEvent* ReentrantEvents = Subsystem->GetSlotEventDelegate(ReentrantSlot);
    const FDelegateHandle TeardownObserver = ReentrantEvents->AddLambda([ReentrantSite](const FSmartObjectEventData& Event)
    {
        if (Event.Reason == ESmartObjectChangeReason::OnClaimed) ReentrantSite->DestroyComponent();
    });
    TestFalse(TEXT("Destruction during engine claim notification rejects the pending reservation"),
        ICommonAIInteraction::Execute_ReserveInteraction(ReentrantSite, First));
    TestEqual(TEXT("Destruction before the claim handle returns does not leak the slot"),
        Subsystem->GetSlotState(ReentrantSlot), ESmartObjectSlotState::Free);
    ReentrantEvents->Remove(TeardownObserver);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDailyLifeSmartObjectPointContractTest, "CommonAI.DailyLife.SmartObjectPointContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDailyLifeSmartObjectPointContractTest::RunTest(const FString& Parameters)
{
    FDailyLifeSmartObjectWorld Fixture;
    const FCommonAIInteractionRequest Request = Fixture.Request();
    TestFalse(TEXT("Multiple slots are outside the single-capacity contract"),
        ICommonAIInteraction::Execute_QueryInteraction(Fixture.Location(2)->Site, Request).bAvailable);
    constexpr float OffsetFromApproachActorCentimeters = 100.0f;
    TestFalse(TEXT("A displaced slot cannot use actor-origin movement"),
        ICommonAIInteraction::Execute_QueryInteraction(Fixture.Location(1, FVector3f(OffsetFromApproachActorCentimeters, 0.0f, 0.0f))->Site,
            Request).bAvailable);
    return true;
}
#endif
