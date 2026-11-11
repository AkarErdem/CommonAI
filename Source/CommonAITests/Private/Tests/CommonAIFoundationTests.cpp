#if WITH_DEV_AUTOMATION_TESTS
#include "CommonAIAgentComponent.h"
#include "CommonAITags.h"
#include "Contracts/CommonAIWorkSite.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAITagOwnershipTest, "CommonAI.Foundation.TagOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAITagOwnershipTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UCommonAIAgentComponent> Agent(NewObject<UCommonAIAgentComponent>());
    TStrongObjectPtr<UCommonAIAgentComponent> First(NewObject<UCommonAIAgentComponent>());
    TStrongObjectPtr<UCommonAIAgentComponent> Second(NewObject<UCommonAIAgentComponent>());
    FGameplayTagContainer Tags(CommonAITags::State_Busy_Working);
    const FGuid A = Agent->AddTagContribution(First.Get(), Tags);
    const FGuid B = Agent->AddTagContribution(Second.Get(), Tags);
    TestTrue(TEXT("A child satisfies the parent query"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Agent->RemoveTagContribution(First.Get(), B);
    Agent->RemoveTagContribution(First.Get(), A);
    TestTrue(TEXT("Another source retains its contribution"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Agent->RemoveTagContribution(Second.Get(), B);
    TestFalse(TEXT("Last release removes the parent match"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    return true;
}
#include "CommonAIProfile.h"
#include "Fixtures/CommonAIProfileFixture.h"
#include "AIController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/CurveTable.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
struct FCommonAIWorld
{
    UWorld* World = nullptr;
    FCommonAIWorld()
    {
        const UWorld::InitializationValues Settings = UWorld::InitializationValues()
            .AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
            .CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        World->GetWorldSettings()->NotifyBeginPlay();
    }
    ~FCommonAIWorld()
    {
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    }
    void Tick(float Seconds)
    {
        constexpr float FrameSeconds = 1.0f / 60.0f;
        for (int32 Frame = 0; Frame < FMath::CeilToInt(Seconds / FrameSeconds); ++Frame)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, FrameSeconds);
        }
    }
    UCommonAISignaledWorkSite* Site()
    {
        AActor* Owner = World->SpawnActor<AActor>();
        USceneComponent* Root = NewObject<USceneComponent>(Owner);
        Owner->SetRootComponent(Root);
        Root->RegisterComponent();
        UCommonAISignaledWorkSite* Result = NewObject<UCommonAISignaledWorkSite>(Owner);
        Result->RegisterComponent();
        return Result;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIInteractionOwnershipTest, "CommonAI.Foundation.InteractionOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAIInteractionOwnershipTest::RunTest(const FString& Parameters)
{
    FCommonAIWorld Fixture;
    UCommonAISignaledWorkSite* Site = Fixture.Site();
    FCommonAIInteractionRequest First;
    First.Agent = Fixture.World->SpawnActor<AAIController>();
    First.Activity = CommonAITags::Activity_Work;
    First.Execution = FGuid::NewGuid();
    First.Operation = FGuid::NewGuid();
    FCommonAIInteractionRequest Second = First;
    Second.Agent = Fixture.World->SpawnActor<AAIController>();
    Second.Execution = FGuid::NewGuid();
    Second.Operation = FGuid::NewGuid();
    TestTrue(TEXT("First agent reserves"), ICommonAIInteraction::Execute_ReserveInteraction(Site, First));
    TestFalse(TEXT("Second agent cannot reserve"), ICommonAIInteraction::Execute_ReserveInteraction(Site, Second));
    TestEqual(TEXT("Begin runs"), ICommonAIInteraction::Execute_BeginInteraction(Site, First).Status, ECommonAIInteractionStatus::Running);
    ICommonAIInteraction::Execute_RequestInteractionCancellation(Site, First, CommonAITags::Reason_Disabled);
    ICommonAIInteraction::Execute_ReleaseInteraction(Site, First, false);
    TestEqual(TEXT("Cancellation retains the reservation"), ICommonAIInteraction::Execute_GetInteractionResult(Site, First).Status, ECommonAIInteractionStatus::Cancelling);
    TestFalse(TEXT("Another agent waits for safe end"), ICommonAIInteraction::Execute_ReserveInteraction(Site, Second));
    Site->SignalCompletion(First, true);
    TestEqual(TEXT("Natural end after cancellation is cancelled"), ICommonAIInteraction::Execute_GetInteractionResult(Site, First).Status, ECommonAIInteractionStatus::Cancelled);
    ICommonAIInteraction::Execute_ReleaseInteraction(Site, First, false);
    TestTrue(TEXT("Released target can be reused"), ICommonAIInteraction::Execute_ReserveInteraction(Site, Second));
    ICommonAIInteraction::Execute_BeginInteraction(Site, Second);
    Site->SignalCompletion(First, true);
    TestEqual(TEXT("Old signal cannot complete the new run"), ICommonAIInteraction::Execute_GetInteractionResult(Site, Second).Status, ECommonAIInteractionStatus::Running);
    Site->SignalCompletion(Second, true);
    TestEqual(TEXT("Current signal completes"), ICommonAIInteraction::Execute_GetInteractionResult(Site, Second).Status, ECommonAIInteractionStatus::Completed);
    ICommonAIInteraction::Execute_ReleaseInteraction(Site, Second, false);
    FCommonAIInteractionRequest Repeated = Second;
    Repeated.Operation = FGuid::NewGuid();
    TestTrue(TEXT("Same activity can acquire a fresh interaction operation"), ICommonAIInteraction::Execute_ReserveInteraction(Site, Repeated));
    ICommonAIInteraction::Execute_BeginInteraction(Site, Repeated);
    Site->SignalCompletion(Second, true);
    TestEqual(TEXT("Earlier operation cannot complete a repeated step"), ICommonAIInteraction::Execute_GetInteractionResult(Site, Repeated).Status, ECommonAIInteractionStatus::Running);
    ICommonAIInteraction::Execute_RequestInteractionCancellation(Site, Repeated, CommonAITags::Reason_Disabled);
    Site->SignalSafeCancellation(Second);
    TestEqual(TEXT("Earlier operation cannot acknowledge a newer cancellation"), ICommonAIInteraction::Execute_GetInteractionResult(Site, Repeated).Status, ECommonAIInteractionStatus::Cancelling);
    Site->SignalSafeCancellation(Repeated);
    ICommonAIInteraction::Execute_ReleaseInteraction(Site, Repeated, false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIScoringDrainTest, "CommonAI.Foundation.ScoringAndDrain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAIScoringDrainTest::RunTest(const FString& Parameters)
{
    FCommonAIWorld Fixture;
    AAIController* Controller = Fixture.World->SpawnActor<AAIController>();
    ACharacter* Pawn = Fixture.World->SpawnActor<ACharacter>();
    Controller->Possess(Pawn);
    Pawn->GetCharacterMovement()->DisableMovement();
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Controller);
    Agent->bStartAutomatically = false;
    UCommonAIProfile* Profile = CommonAITestProfiles::MakeWork(Agent);
    Agent->Profile = Profile;
    Agent->RegisterComponent();
    UCommonAISignaledWorkSite* Site = Fixture.Site();
    Agent->AddInteractionTarget(Site);
    TestTrue(TEXT("Public API starts the agent"), Agent->StartAgent());
    Fixture.Tick(1.0f);
    const FCommonAIAgentSnapshot Active = Agent->GetSnapshot();
    TestEqual(TEXT("Normal scoring reaches interaction"), Active.Phase, ECommonAIPhase::Interacting);
    if (Active.Phase != ECommonAIPhase::Interacting) return false;
    Agent->StopAgent();
    Fixture.Tick(0.5f);
    TestEqual(TEXT("Stop waits for acknowledgement"), Agent->GetSnapshot().Phase, ECommonAIPhase::Cancelling);
    TestTrue(TEXT("Busy is retained while draining"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Site->SignalSafeCancellation(Active.Interaction);
    Fixture.Tick(0.5f);
    TestFalse(TEXT("Agent disables after drain"), Agent->GetSnapshot().bEnabled);
    TestEqual(TEXT("Stop is not normal success"), Agent->GetSnapshot().LastResult.Status, ECommonAIInteractionStatus::Cancelled);
    TestFalse(TEXT("Owned busy state is released"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    UCommonAIProfile* InvalidProfile = NewObject<UCommonAIProfile>(Agent);
    TestFalse(TEXT("An invalid profile is rejected before draining"), Agent->SetProfile(InvalidProfile));
    TestTrue(TEXT("Rejected profile preserves the configured profile"), Agent->Profile == Profile);
    Agent->StartAgent();
    Fixture.Tick(1.0f);
    const FCommonAIInteractionRequest ProfileRun = Agent->GetSnapshot().Interaction;
    UCommonAIProfile* ReplacementProfile = DuplicateObject<UCommonAIProfile>(Profile, Agent);
    TestTrue(TEXT("Valid profile change is accepted"), Agent->SetProfile(ReplacementProfile));
    TestTrue(TEXT("Old profile remains while draining"), Agent->Profile == Profile);
    Site->SignalSafeCancellation(ProfileRun);
    Fixture.Tick(1.0f);
    TestTrue(TEXT("New profile applies after safe completion"), Agent->Profile == ReplacementProfile);
    TestEqual(TEXT("New profile resumes ordinary scoring"), Agent->GetSnapshot().Phase, ECommonAIPhase::Interacting);
    const FCommonAIInteractionRequest PawnRun = Agent->GetSnapshot().Interaction;
    Agent->bStartAutomatically = true;
    ACharacter* ReplacementPawn = Fixture.World->SpawnActor<ACharacter>();
    ReplacementPawn->GetCharacterMovement()->DisableMovement();
    Controller->Possess(ReplacementPawn);
    Fixture.Tick(0.1f);
    TestEqual(TEXT("Pawn replacement respects drain"), Agent->GetSnapshot().Phase, ECommonAIPhase::Cancelling);
    Site->SignalSafeCancellation(PawnRun);
    Fixture.Tick(0.5f);
    TestTrue(TEXT("Already possessed replacement restarts after drain"), Agent->GetSnapshot().bEnabled);
    return true;
}
#include "Fixtures/CommonAIFoundationObserver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAITeardownReentryTest, "CommonAI.Foundation.TeardownReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAITeardownReentryTest::RunTest(const FString& Parameters)
{
    FCommonAIWorld Fixture;
    UCommonAISignaledWorkSite* Site = Fixture.Site();
    TStrongObjectPtr<UCommonAIFoundationObserver> Observer(NewObject<UCommonAIFoundationObserver>());
    Observer->Target = Site;
    Site->OnInteractionAbandoned.AddDynamic(Observer.Get(), &UCommonAIFoundationObserver::OnAbandoned);
    FCommonAIInteractionRequest First;
    First.Agent = Fixture.World->SpawnActor<AAIController>();
    First.Activity = CommonAITags::Activity_Work;
    First.Execution = FGuid::NewGuid();
    First.Operation = FGuid::NewGuid();
    Observer->Replacement = First;
    Observer->Replacement.Execution = FGuid::NewGuid();
    Observer->Replacement.Operation = FGuid::NewGuid();
    ICommonAIInteraction::Execute_ReserveInteraction(Site, First);
    ICommonAIInteraction::Execute_BeginInteraction(Site, First);
    Observer->bReserveOnAbandonment = true;
    ICommonAIInteraction::Execute_ReleaseInteraction(Site, First, true);
    TestEqual(TEXT("Reentrant replacement survives old teardown"),
        ICommonAIInteraction::Execute_GetInteractionResult(Site, Observer->Replacement).Status, ECommonAIInteractionStatus::Reserved);
    Observer->bReserveOnAbandonment = false;
    ICommonAIInteraction::Execute_BeginInteraction(Site, Observer->Replacement);
    Site->DestroyComponent();
    TestEqual(TEXT("Target destruction notifies the surviving effect owner"), Observer->AbandonmentCount, 2);
    return true;
}
#include "Animation/CommonAIAnimNotifyState_Tags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#include "AnimationBlueprintLibrary.h"
#endif
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIMontageWindowTest, "CommonAI.Foundation.MontageWindows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAIMontageWindowTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
    FCommonAIWorld Fixture;
    AAIController* Controller = Fixture.World->SpawnActor<AAIController>();
    ACharacter* Pawn = Fixture.World->SpawnActor<ACharacter>();
    Controller->Possess(Pawn);
    Pawn->GetCharacterMovement()->DisableMovement();
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Controller);
    Agent->bStartAutomatically = false;
    Agent->RegisterComponent();
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
    if (!TestNotNull(TEXT("Neutral engine mesh is available"), Mesh)) return false;
    Pawn->GetMesh()->SetSkeletalMesh(Mesh);
    Pawn->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Pawn->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());
    UAnimInstance* Anim = Pawn->GetMesh()->GetAnimInstance();
    if (!TestNotNull(TEXT("Native animation instance"), Anim)) return false;
    UAnimSequence* Sequence = NewObject<UAnimSequence>();
    Sequence->SetSkeleton(Mesh->GetSkeleton());
    IAnimationDataController& Data = Sequence->GetController();
    Data.InitializeModel();
    Data.OpenBracket(FText::FromString(TEXT("Neutral tag window fixture")), false);
    Data.SetFrameRate(FFrameRate(30, 1), false);
    Data.SetNumberOfFrames(FFrameNumber(30), false);
    Data.NotifyPopulated();
    Data.CloseBracket(false);
    UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.0f, 0.0f);
    if (!TestNotNull(TEXT("Transient montage"), Montage)) return false;
    UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Montage, TEXT("Tags"), FLinearColor::White);
    auto* First = Cast<UCommonAIAnimNotifyState_Tags>(UAnimationBlueprintLibrary::AddAnimationNotifyStateEvent(
        Montage, TEXT("Tags"), 0.1f, 0.3f, UCommonAIAnimNotifyState_Tags::StaticClass()));
    auto* Second = Cast<UCommonAIAnimNotifyState_Tags>(UAnimationBlueprintLibrary::AddAnimationNotifyStateEvent(
        Montage, TEXT("Tags"), 0.2f, 0.6f, UCommonAIAnimNotifyState_Tags::StaticClass()));
    if (!First || !Second) return false;
    First->Tags.AddTag(CommonAITags::State_Busy_Working);
    Second->Tags = First->Tags;
    Montage->RefreshCacheData();
    TestTrue(TEXT("Montage starts"), Anim->Montage_Play(Montage) > 0.0f);
    Fixture.Tick(0.3f);
    TestTrue(TEXT("Notify begins through real playback"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Fixture.Tick(0.25f);
    TestTrue(TEXT("Second interval retains shared tag after first ends"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Fixture.Tick(0.3f);
    TestFalse(TEXT("Last interval ends before montage termination"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Anim->Montage_Play(Montage);
    Fixture.Tick(0.3f);
    TestTrue(TEXT("Replay obtains a fresh contribution"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Anim->Montage_Stop(0.0f, Montage);
    Fixture.Tick(0.1f);
    TestFalse(TEXT("Interrupted montage releases its windows"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Anim->Montage_Play(Montage);
    Fixture.Tick(0.3f);
    Agent->AddTagContribution(Pawn, FGameplayTagContainer(CommonAITags::State_Busy_Working));
    Controller->UnPossess();
    TestFalse(TEXT("Possession transfer releases original agent windows"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
#endif
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIFailurePathsTest, "CommonAI.Foundation.FailurePaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAIFailurePathsTest::RunTest(const FString& Parameters)
{
    FCommonAIWorld Fixture;
    AAIController* Controller = Fixture.World->SpawnActor<AAIController>();
    ACharacter* Pawn = Fixture.World->SpawnActor<ACharacter>();
    Controller->Possess(Pawn);
    Pawn->GetCharacterMovement()->DisableMovement();
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = LoadObject<UCommonAIProfile>(nullptr, TEXT("/CommonAI/Profiles/DA_Work.DA_Work"));
    Agent->RegisterComponent();
    if (!TestTrue(TEXT("Saved profile starts"), Agent->StartAgent())) return false;
    Fixture.Tick(0.5f);
    TestEqual(TEXT("No opportunity stays in fallback"), Agent->GetSnapshot().Phase, ECommonAIPhase::Idle);
    UCommonAISignaledWorkSite* FarSite = Fixture.Site();
    constexpr float UnreachableDistanceCentimeters = 10000.0f;
    FarSite->GetOwner()->SetActorLocation(FVector(UnreachableDistanceCentimeters, 0.0f, 0.0f));
    Agent->AddInteractionTarget(FarSite);
    Fixture.Tick(1.0f);
    TestEqual(TEXT("Navigation failure is failure"), Agent->GetSnapshot().LastResult.Status, ECommonAIInteractionStatus::Failed);
    TestEqual(TEXT("Navigation failure has a reason"), Agent->GetSnapshot().LastResult.Reason, CommonAITags::Reason_MovementFailed.GetTag());
    TestFalse(TEXT("Failed movement releases busy state"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Agent->StopAgent();
    Agent->RemoveInteractionTarget(FarSite);
    UCommonAISignaledWorkSite* NearSite = Fixture.Site();
    NearSite->GetOwner()->SetActorLocation(Pawn->GetActorLocation());
    Agent->AddInteractionTarget(NearSite);
    Agent->StartAgent();
    Fixture.Tick(1.0f);
    AddInfo(FString::Printf(TEXT("Restart phase=%d result=%d reason=%s target=%s pawn=%s"),
        int32(Agent->GetSnapshot().Phase), int32(Agent->GetSnapshot().LastResult.Status),
        *Agent->GetSnapshot().LastResult.Reason.ToString(), *GetNameSafe(Agent->GetSnapshot().Target.Get()),
        *Pawn->GetActorLocation().ToString()));
    TestEqual(TEXT("Fresh execution reaches target"), Agent->GetSnapshot().Phase, ECommonAIPhase::Interacting);
    NearSite->GetOwner()->Destroy();
    Fixture.Tick(0.5f);
    TestEqual(TEXT("Target loss is never success"), Agent->GetSnapshot().LastResult.Status, ECommonAIInteractionStatus::Failed);
    Agent->StopAgent();
    UCommonAISignaledWorkSite* LastSite = Fixture.Site();
    LastSite->GetOwner()->SetActorLocation(Pawn->GetActorLocation());
    Agent->AddInteractionTarget(LastSite);
    Agent->StartAgent();
    Fixture.Tick(1.0f);
    TestEqual(TEXT("Final interaction starts"), Agent->GetSnapshot().Phase, ECommonAIPhase::Interacting);
    const FCommonAIInteractionRequest LastExecution = Agent->GetSnapshot().Interaction;
    Pawn->Destroy();
    Fixture.Tick(0.5f);
    TestFalse(TEXT("Destroyed pawn cannot leave a pending drain"), Agent->GetSnapshot().bEnabled);
    TestFalse(TEXT("Destroyed pawn releases state"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    FCommonAIInteractionRequest Query;
    Query.Agent = Fixture.World->SpawnActor<AAIController>();
    Query.Activity = CommonAITags::Activity_Work;
    TestTrue(TEXT("Surviving target is released on pawn destruction"), ICommonAIInteraction::Execute_QueryInteraction(LastSite, Query).bAvailable);
    LastSite->SignalCompletion(LastExecution, true);
    TestTrue(TEXT("Late completion cannot recreate a reservation"), ICommonAIInteraction::Execute_QueryInteraction(LastSite, Query).bAvailable);
    return true;
}
#endif


