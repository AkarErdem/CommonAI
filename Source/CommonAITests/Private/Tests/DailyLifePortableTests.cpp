#if WITH_DEV_AUTOMATION_TESTS
#include "AIController.h"
#include "CommonAIAgentComponent.h"
#include "CommonAITags.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "Contracts/CommonAISmartObjectSite.h"
#include "DailyLife/CommonAIDailyLifeClockSubsystem.h"
#include "DailyLife/CommonAIDailyLifeComponent.h"
#include "DailyLife/CommonAIDailyLifeProfile.h"
#include "DailyLife/CommonAIDailyLifeSaveGame.h"
#include "DailyLife/CommonAIDailyLifeSaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Fixtures/CommonAIDailyLifeContextFixture.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace
{
constexpr float FrameSeconds = 1.0f / 60.0f;
constexpr float MaximumLifecycleSeconds = 5.0f;
constexpr float ObservationSeconds = 0.05f;
constexpr float ExecutionSeconds = 0.02f;
constexpr float ProgressObservationSeconds = 0.1f;
constexpr float InitialHunger = 0.9f;
constexpr float RestoredProbeHunger = 0.6f;
constexpr float SatisfiedHunger = 0.1f;
constexpr double DayLengthSeconds = 24.0;
constexpr double StartHours = 6.0;
constexpr double ClockProbeHours = 30.0;

struct FPortableDailyLifeWorld
{
    UWorld* World = nullptr;
    FPortableDailyLifeWorld()
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
    ~FPortableDailyLifeWorld()
    {
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    }
    void Tick(float Seconds)
    {
        for (int32 Frame = 0; Frame < FMath::CeilToInt(Seconds / FrameSeconds); ++Frame)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, FrameSeconds);
        }
    }
    bool Until(TFunctionRef<bool()> Condition)
    {
        for (int32 Frame = 0; Frame < FMath::CeilToInt(MaximumLifecycleSeconds / FrameSeconds); ++Frame)
        {
            if (Condition()) return true;
            Tick(FrameSeconds);
        }
        return Condition();
    }
    UCommonAIDailyLifeProfile* Profile()
    {
        UCommonAIDailyLifeProfile* Result = NewObject<UCommonAIDailyLifeProfile>(World);
        Result->Occupation = FGameplayTag::RequestGameplayTag(TEXT("CommonAITest.Occupation.Primary"));
        Result->OccupationLabel = FText::FromString(TEXT("Resident"));
        Result->HungerPerHour = 0.0f;
        Result->FatiguePerHour = 0.0f;
        Result->ScheduleVariationHours = 0.0f;
        Result->ObservationIntervalSeconds = ObservationSeconds;
        Result->ExecutionIntervalSeconds = ExecutionSeconds;
        FCommonAIDailyActivityDefinition& Eat = Result->Activities.AddDefaulted_GetRef();
        Eat.Kind = ECommonAIDailyActivity::Eat;
        Eat.Activity = CommonAITags::Activity_Eat;
        Eat.NeedInput = CommonAITags::DailyLife_Eat_Need;
        Eat.TimeInput = CommonAITags::DailyLife_Eat_Time;
        Eat.AvailabilityInput = CommonAITags::DailyLife_Eat_Available;
        Eat.DistanceInput = CommonAITags::DailyLife_Eat_Distance;
        Eat.Label = FText::FromString(TEXT("Eat"));
        Eat.HungerRecoveryPerHour = 1.0f;
        Eat.SatisfiedNeed = SatisfiedHunger;
        return Result;
    }
    ACommonAIActivityLocation* Location(FGameplayTag Activity = CommonAITags::Activity_Eat)
    {
        USmartObjectDefinition* Definition = NewObject<USmartObjectDefinition>(World);
        FSmartObjectSlotDefinition& Slot = Definition->DebugAddSlot();
        Slot.BehaviorDefinitions.Add(NewObject<UCommonAISmartObjectBehaviorDefinition>(Definition));
        Definition->Validate();
        ACommonAIActivityLocation* Result = World->SpawnActorDeferred<ACommonAIActivityLocation>(
            ACommonAIActivityLocation::StaticClass(), FTransform::Identity);
        Result->SmartObject->SetDefinition(Definition);
        Result->Site->SiteId = FGuid::NewGuid();
        Result->Site->Activities.Reset();
        Result->Site->Activities.AddTag(Activity);
        Result->FinishSpawning(FTransform::Identity);
        return Result;
    }
    ACharacter* Resident(UCommonAIDailyLifeProfile* Definition, FGuid Identity,
        ACommonAIActivityLocation* Destination = nullptr, ACommonAIActivityLocation* OtherDestination = nullptr)
    {
        ACharacter* Result = World->SpawnActorDeferred<ACharacter>(ACharacter::StaticClass(), FTransform::Identity,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Result->AutoPossessAI = EAutoPossessAI::Disabled;
        UCommonAIDailyLifeComponent* Routine = NewObject<UCommonAIDailyLifeComponent>(Result);
        Result->AddInstanceComponent(Routine);
        Routine->Profile = Definition;
        Routine->InitialState.Identity = Identity;
        Routine->InitialState.Hunger = InitialHunger;
        Routine->InitialState.Fatigue = 0.0f;
        if (Destination) Routine->Destinations.Add(Destination->Site);
        if (OtherDestination) Routine->Destinations.Add(OtherDestination->Site);
        Routine->RegisterComponent();
        Result->FinishSpawning(FTransform::Identity);
        AAIController* Controller = World->SpawnActor<AAIController>();
        Controller->Possess(Result);
        // Possession restarts the character and restores its movement mode. Keep this floorless fixture at the activity point afterward.
        Result->GetCharacterMovement()->DisableMovement();
        return Result;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIDailyLifeConsumerActivitiesTest, "CommonAI.DailyLife.ConsumerActivities",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAIDailyLifeConsumerActivitiesTest::RunTest(const FString& Parameters)
{
    FPortableDailyLifeWorld Fixture;
    UCommonAIDailyLifeClockSubsystem* Clock = Fixture.World->GetSubsystem<UCommonAIDailyLifeClockSubsystem>();
    if (!TestNotNull(TEXT("The common clock is available"), Clock)) return false;
    Clock->SetDayLengthSeconds(DayLengthSeconds);
    Clock->SetTimeHours(StartHours);
    UCommonAIDailyLifeProfile* Profile = Fixture.Profile();
    FCommonAIDailyActivityDefinition ClosedMeal = Profile->Activities[0];
    ClosedMeal.OutsideWindowPreference = 0.0f;
    ClosedMeal.PreferredTimes.Add({18.0f, 20.0f});
    Profile->Activities.Reset();
    const auto AddActivity = [Profile](const TCHAR* Name, float FromHour, float UntilHour)
    {
        const auto Consideration = [Name](const TCHAR* Channel)
        {
            return FGameplayTag::RequestGameplayTag(FName(*FString::Printf(
                TEXT("CommonAITest.Consideration.%s.%s"), Name, Channel)));
        };
        FCommonAIDailyActivityDefinition& Entry = Profile->Activities.AddDefaulted_GetRef();
        Entry.Activity = FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("CommonAITest.Activity.%s"), Name)));
        Entry.Label = FText::FromString(Name);
        Entry.Kind = ECommonAIDailyActivity::SiteActivity;
        Entry.NeedInput = Consideration(TEXT("Need"));
        Entry.TimeInput = Consideration(TEXT("Time"));
        Entry.AvailabilityInput = Consideration(TEXT("Available"));
        Entry.DistanceInput = Consideration(TEXT("Distance"));
        Entry.PreferredTimes.Add({FromHour, UntilHour});
        Entry.OutsideWindowPreference = 0.0f;
        Entry.bCompleteOnSiteSignal = true;
        Entry.DurationHours = 0.05f;
    };
    constexpr float SecondWindowStart = 12.0f;
    AddActivity(TEXT("First"), 6.0f, 9.0f);
    AddActivity(TEXT("Second"), SecondWindowStart, 16.0f);
    Profile->Activities.Add(ClosedMeal);
    if (!TestTrue(TEXT("Two consumer activity identities may use the same behavior"), Profile->ValidateProfile().IsEmpty())) return false;
    const FGameplayTag PortableActivityFirst = Profile->Activities[0].Activity;
    const FGameplayTag PortableActivitySecond = Profile->Activities[1].Activity;
    const FGameplayTag SecondTime = Profile->Activities[1].TimeInput;
    Profile->Activities[1].TimeInput = Profile->Activities[0].TimeInput;
    TestFalse(TEXT("The same scoring input cannot be authored for two activities"), Profile->ValidateProfile().IsEmpty());
    Profile->Activities[1].TimeInput = SecondTime;

    ACommonAIActivityLocation* First = Fixture.Location(PortableActivityFirst);
    ACommonAIActivityLocation* Second = Fixture.Location(PortableActivitySecond);
    First->Site->Activities.AddTag(ClosedMeal.Activity);
    Second->Site->Activities.AddTag(ClosedMeal.Activity);
    First->Site->bWaitForSafeCancellation = true;
    Second->Site->bWaitForSafeCancellation = true;
    ACharacter* Character = Fixture.Resident(Profile, FGuid::NewGuid(), First, Second);
    UCommonAIDailyLifeComponent* Routine = Character->FindComponentByClass<UCommonAIDailyLifeComponent>();
    const auto IsInteracting = [Routine](FGameplayTag Activity, const UCommonAISmartObjectSite* Site)
    {
        const UCommonAIAgentComponent* Agent = Routine->GetRoutineAgent();
        if (!Agent) return false;
        const FCommonAIAgentSnapshot Snapshot = Agent->GetSnapshot();
        return Snapshot.Phase == ECommonAIPhase::Interacting && Snapshot.Activity == Activity && Snapshot.Target.Get() == Site;
    };
    if (!TestTrue(TEXT("The first consumer activity is scored and reserves its own target"), Fixture.Until([&]
        { return IsInteracting(PortableActivityFirst, First->Site); }))) return false;
    Fixture.Tick(ProgressObservationSeconds);
    TestTrue(TEXT("Urgency cannot interrupt the activity for a meal outside its availability window"),
        IsInteracting(PortableActivityFirst, First->Site));
    TestTrue(TEXT("Site completion owns the outcome beyond the routine duration"),
        First->Site->HasClaim() && Routine->ExportState().ProgressHours > Profile->Activities[0].DurationHours);
    First->Site->SignalCompletion(First->Site->GetReservation(), true);
    if (!TestTrue(TEXT("The site result completes the activity and releases its slot"), Fixture.Until([&]
        { return Routine->ExportState().CompletedActivities == 1 && !First->Site->HasClaim(); }))) return false;
    TestTrue(TEXT("The first activity owns its cooldown"),
        Routine->ExportState().CooldownUntilHours.Contains(PortableActivityFirst)
        && !Routine->ExportState().CooldownUntilHours.Contains(PortableActivitySecond));
    Clock->SetTimeHours(SecondWindowStart);
    if (!TestTrue(TEXT("The second activity selects its distinct target in its own window"), Fixture.Until([&]
        { return IsInteracting(PortableActivitySecond, Second->Site); }))) return false;
    Fixture.Tick(ProgressObservationSeconds);
    Clock->SetPaused(true);
    const double ProgressBeforeCancellation = Routine->ExportState().ProgressHours;
    TestTrue(TEXT("The second effect records progress before cancellation"), ProgressBeforeCancellation > 0.0);
    const FCommonAIInteractionRequest Cancelled = Second->Site->GetReservation();
    Routine->GetRoutineAgent()->RequestCancellation(CommonAITags::Reason_Emergency);
    Fixture.Tick(FrameSeconds);
    TestTrue(TEXT("Cancellation retains the slot until the site acknowledges"), Second->Site->HasClaim());
    TestEqual(TEXT("The requested site is draining its cancellation"),
        ICommonAIInteraction::Execute_GetInteractionResult(Second->Site, Cancelled).Status, ECommonAIInteractionStatus::Cancelling);
    Second->Site->SignalSafeCancellation(Cancelled);
    if (!TestTrue(TEXT("The acknowledged cancellation releases the slot"), Fixture.Until([&] { return !Second->Site->HasClaim(); })))
    {
        AddInfo(Routine->GetDebugText());
        return false;
    }
    Fixture.Tick(ProgressObservationSeconds);
    const FCommonAIDailyLifeState AfterCancellation = Routine->ExportState();
    TestEqual(TEXT("The activity retains its own resume identity"), AfterCancellation.ResumeActivity, PortableActivitySecond);
    TestEqual(TEXT("The activity retains its own resumable progress"), AfterCancellation.ResumeProgressHours, ProgressBeforeCancellation);
    TestTrue(TEXT("Cancellation applies the second activity cooldown"), AfterCancellation.CooldownUntilHours.Contains(PortableActivitySecond));
    Clock->SetPaused(false);
    if (!TestTrue(TEXT("The same activity can obtain a new claim after its cooldown"), Fixture.Until([&]
        { return IsInteracting(PortableActivitySecond, Second->Site); }))) return false;
    const FCommonAIInteractionRequest Current = Second->Site->GetReservation();
    TestNotEqual(TEXT("The replay has a new execution identity"), Current.Execution, Cancelled.Execution);
    Second->Site->SignalCompletion(Cancelled, true);
    TestEqual(TEXT("A stale result leaves the current interaction running"),
        ICommonAIInteraction::Execute_GetInteractionResult(Second->Site, Current).Status, ECommonAIInteractionStatus::Running);
    Second->Site->SignalCompletion(Current, true);
    if (!TestTrue(TEXT("The replay completes through its own result"), Fixture.Until([&]
        { return Routine->ExportState().CompletedActivities == 2 && !Second->Site->HasClaim(); }))) return false;
    UCommonAIWorkProviderComponent* Provider = Routine->GetRoutineAgent()->GetProvider();
    if (!TestNotNull(TEXT("The routine has a shared target provider"), Provider)) return false;
    TestEqual(TEXT("The running profile binds an independent candidate list"), Provider->GetCandidateCount(PortableActivityFirst), 1);
    Routine->ShutdownRoutine();
    return TestEqual(TEXT("Shutdown removes the exact activity binding"), Provider->GetCandidateCount(PortableActivityFirst), 2);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIDailyLifePortableLifecycleTest, "CommonAI.DailyLife.PortableLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAIDailyLifePortableLifecycleTest::RunTest(const FString& Parameters)
{
    FPortableDailyLifeWorld Fixture;
    UCommonAIDailyLifeClockSubsystem* Clock = Fixture.World->GetSubsystem<UCommonAIDailyLifeClockSubsystem>();
    UCommonAIDailyLifeSaveSubsystem* Saves = Fixture.World->GetSubsystem<UCommonAIDailyLifeSaveSubsystem>();
    USmartObjectSubsystem* SmartObjects = Fixture.World->GetSubsystem<USmartObjectSubsystem>();
    if (!TestNotNull(TEXT("The neutral world has its shared clock"), Clock)
        || !TestNotNull(TEXT("The neutral world has its shared save service"), Saves)
        || !TestNotNull(TEXT("The real Smart Object subsystem is available"), SmartObjects)) return false;
    Clock->SetDayLengthSeconds(DayLengthSeconds);
    Clock->SetTimeHours(StartHours);
    UCommonAIDailyLifeProfile* Profile = Fixture.Profile();
    ACommonAIActivityLocation* Location = Fixture.Location();
    const FGuid Identity = FGuid::NewGuid();
    ACharacter* Character = Fixture.Resident(Profile, Identity, Location);
    UCommonAIDailyLifeComponent* Routine = Character->FindComponentByClass<UCommonAIDailyLifeComponent>();
    const FVector ResidentLocation = Character->GetActorLocation();
    TestTrue(TEXT("The resident is an unmodified engine character"), Character->GetClass() == ACharacter::StaticClass());
    TestTrue(TEXT("The controller is an unmodified engine AI controller"), Character->GetController()->GetClass() == AAIController::StaticClass());
    TestEqual(TEXT("The floorless fixture disables movement after possession"), Character->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
    if (!TestTrue(TEXT("Possession automatically binds the shared component"), Fixture.Until([&]
        { return Routine->IsRoutineInitialized(); }))) return false;
    UCommonAIAgentComponent* Agent = Routine->GetRoutineAgent();
    if (!TestTrue(TEXT("Ordinary scoring starts the logical eating activity"), Fixture.Until([&]
        { return Agent->GetSnapshot().Phase == ECommonAIPhase::Interacting; })))
    {
        AddInfo(Routine->GetDebugText());
        return false;
    }
    TestEqual(TEXT("The selected activity is Eat"), Agent->GetSnapshot().Activity, CommonAIDailyLife::ActivityTag(ECommonAIDailyActivity::Eat));
    TestTrue(TEXT("The production sequence holds a real claim"), Location->Site->HasClaim());
    TArray<FSmartObjectSlotHandle> Slots;
    SmartObjects->GetAllSlots(Location->SmartObject->GetRegisteredHandle(), Slots);
    if (!TestEqual(TEXT("The portable point has one registered engine slot"), Slots.Num(), 1)) return false;
    TestEqual(TEXT("The production sequence occupies the engine slot"), SmartObjects->GetSlotState(Slots[0]), ESmartObjectSlotState::Occupied);

    Fixture.Tick(ProgressObservationSeconds);
    Clock->SetPaused(true);
    const FCommonAIDailyLifeState Before = Routine->ExportState();
    TestTrue(TEXT("The saved execution has actual logical progress"), Before.ProgressHours > 0.0);
    TestTrue(TEXT("The fixture remains at the activity point before saving"), Character->GetActorLocation().Equals(ResidentLocation));
    const double SavedHours = Clock->GetTimeHours();
    const FString SaveSlot = TEXT("CommonAIPortable_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ON_SCOPE_EXIT
    {
        if (UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
            TestTrue(TEXT("The portable check removes its own save slot"), UGameplayStatics::DeleteGameInSlot(SaveSlot, 0));
    };
    if (!TestTrue(TEXT("The shared save service writes an engine save slot"), Saves->SaveToSlot(SaveSlot))) return false;
    TStrongObjectPtr<UCommonAIDailyLifeSaveGame> Saved(Cast<UCommonAIDailyLifeSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)));
    if (!TestNotNull(TEXT("The engine independently reopens the slot"), Saved.Get())
        || !TestEqual(TEXT("One registered resident is serialized"), Saved->Residents.Num(), 1)) return false;
    TestEqual(TEXT("The serialized key is the authored stable identity"), Saved->Residents[0].Identity, Identity);
    TestEqual(TEXT("Logical progress is serialized"), Saved->Residents[0].ProgressHours, Before.ProgressHours);
    TestEqual(TEXT("The destination identity is serialized"), Saved->Residents[0].DestinationId, Location->Site->SiteId);
    FCommonAIDailyLifeState Changed = Before;
    Changed.Hunger = RestoredProbeHunger;
    TestTrue(TEXT("A valid initial-condition change is accepted"), Routine->RestoreState(Changed));
    Clock->SetTimeHours(ClockProbeHours);
    if (!TestTrue(TEXT("The production slot load restores the resident"), Saves->LoadFromSlot(SaveSlot))) return false;
    TestEqual(TEXT("The clock round-trips with resident state"), Clock->GetTimeHours(), SavedHours);
    TestTrue(TEXT("Clock pause survives serialization"), Clock->GetState().bPaused);
    const FCommonAIDailyLifeState Restored = Routine->ExportState();
    TestEqual(TEXT("Resident hunger restores by stable identity"), Restored.Hunger, Before.Hunger);
    TestEqual(TEXT("Resident activity progress is retained for execution resumption"),
        Restored.ProgressHours + Restored.ResumeProgressHours, Before.ProgressHours);
    TestTrue(TEXT("The loaded logical activity is active or queued for resumption"),
        Restored.Activity == Before.Activity || Restored.ResumeActivity == Before.Activity);

    Clock->SetPaused(false);
    if (!TestTrue(TEXT("Logical eating completes without an animation"), Fixture.Until([&]
        { return Routine->ExportState().CompletedActivities > Before.CompletedActivities; })))
    {
        const FCommonAIDailyLifeClockState ClockState = Clock->GetState();
        const FCommonAIDailyLifeState Final = Routine->ExportState();
        AddInfo(FString::Printf(TEXT("Resume clock: saved=%.6f current=%.6f daySeconds=%.6f scale=%.6f paused=%d; stateAnchor=%.6f initialized=%d inputOwner=%d movementOwner=%d\nPosition: initial=%s current=%s navAgent=%s destination=%s velocity=%s movementMode=%d\nFinal routine:\n%s"),
            SavedHours, ClockState.TimeHours, ClockState.DayLengthSeconds, ClockState.TimeScale, ClockState.bPaused,
            Final.LastTimeHours, Routine->IsRoutineInitialized(), Agent->GetInputSource(CommonAIDailyLife::SourceTag()) == Routine,
            Agent->GetMovementAdapter() == Routine, *ResidentLocation.ToString(), *Character->GetActorLocation().ToString(),
            *Character->GetNavAgentLocation().ToString(), *Location->GetActorLocation().ToString(), *Character->GetVelocity().ToString(),
            static_cast<int32>(Character->GetCharacterMovement()->MovementMode), *Routine->GetDebugText()));
        return false;
    }
    const FCommonAIDailyLifeState Completed = Routine->ExportState();
    TestTrue(TEXT("The fixture remains at the activity point through post-load completion"), Character->GetActorLocation().Equals(ResidentLocation));
    TestTrue(TEXT("The production activity satisfies hunger"), Completed.Hunger <= SatisfiedHunger + KINDA_SMALL_NUMBER);
    TestFalse(TEXT("Completion releases the claim"), Location->Site->HasClaim());
    TestEqual(TEXT("Completion frees the actual engine slot"), SmartObjects->GetSlotState(Slots[0]), ESmartObjectSlotState::Free);

    Clock->SetPaused(true);
    AController* OriginalController = Character->GetController();
    Character->Destroy();
    if (IsValid(OriginalController)) OriginalController->Destroy();
    ACharacter* Replacement = Fixture.Resident(Profile, Identity, Location);
    UCommonAIDailyLifeComponent* ReplacementRoutine = Replacement->FindComponentByClass<UCommonAIDailyLifeComponent>();
    if (!TestTrue(TEXT("A replacement representation binds normally"), Fixture.Until([&]
        { return ReplacementRoutine->IsRoutineInitialized(); }))) return false;
    TestEqual(TEXT("Unload and registration preserve the stable identity"), ReplacementRoutine->ExportState().Identity, Identity);
    TestEqual(TEXT("Unload and registration restore satisfied hunger"), ReplacementRoutine->ExportState().Hunger, Completed.Hunger);
    TestEqual(TEXT("Unload and registration preserve completed work"), ReplacementRoutine->ExportState().CompletedActivities, Completed.CompletedActivities);
    TestTrue(TEXT("Repeated registration is idempotent"), Saves->RegisterResident(ReplacementRoutine));

    FCommonAIDailyLifeState HungryAgain = ReplacementRoutine->ExportState();
    HungryAgain.Hunger = InitialHunger;
    HungryAgain.CooldownUntilHours.Reset();
    TestTrue(TEXT("The teardown scenario establishes a new hungry state"), ReplacementRoutine->RestoreState(HungryAgain));
    if (!TestTrue(TEXT("The replacement can acquire its own live claim"), Fixture.Until([&]
        { return Location->Site->HasClaim(); }))) return false;
    Replacement->Destroy();
    TestFalse(TEXT("Actor teardown releases the replacement claim"), Location->Site->HasClaim());
    TestEqual(TEXT("Actor teardown leaves the engine slot free"), SmartObjects->GetSlotState(Slots[0]), ESmartObjectSlotState::Free);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIDailyLifePersistenceBoundariesTest, "CommonAI.DailyLife.PersistenceBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAIDailyLifePersistenceBoundariesTest::RunTest(const FString& Parameters)
{
    FPortableDailyLifeWorld Fixture;
    UCommonAIDailyLifeClockSubsystem* Clock = Fixture.World->GetSubsystem<UCommonAIDailyLifeClockSubsystem>();
    UCommonAIDailyLifeSaveSubsystem* Saves = Fixture.World->GetSubsystem<UCommonAIDailyLifeSaveSubsystem>();
    if (!TestNotNull(TEXT("Shared clock exists"), Clock) || !TestNotNull(TEXT("Shared persistence exists"), Saves)) return false;
    Clock->SetTimeHours(StartHours);
    Clock->SetDayLengthSeconds(DayLengthSeconds);
    constexpr double TimeScale = 2.0;
    constexpr float ClockObservationSeconds = 0.5f;
    Clock->SetTimeScale(TimeScale);
    const double WorldBefore = Fixture.World->GetTimeSeconds();
    Fixture.Tick(ClockObservationSeconds);
    const double ExpectedHours = StartHours + (Fixture.World->GetTimeSeconds() - WorldBefore) * TimeScale;
    TestTrue(TEXT("Clock scale advances from actual gameplay seconds"), FMath::IsNearlyEqual(Clock->GetTimeHours(), ExpectedHours, UE_DOUBLE_KINDA_SMALL_NUMBER));
    Clock->SetPaused(true);
    const double PausedHours = Clock->GetTimeHours();
    Fixture.Tick(ClockObservationSeconds);
    TestEqual(TEXT("A paused clock does not advance"), Clock->GetTimeHours(), PausedHours);
    TestFalse(TEXT("A zero day length is rejected"), Clock->SetDayLengthSeconds(0.0));
    TestFalse(TEXT("A negative scale is rejected"), Clock->SetTimeScale(-1.0));
    TestEqual(TEXT("Rejected clock edits preserve time"), Clock->GetTimeHours(), PausedHours);

    UCommonAIDailyLifeProfile* Profile = Fixture.Profile();
    const FGuid Identity = FGuid::NewGuid();
    ACharacter* Character = Fixture.Resident(Profile, Identity);
    UCommonAIDailyLifeComponent* Routine = Character->FindComponentByClass<UCommonAIDailyLifeComponent>();
    if (!TestTrue(TEXT("The original controller binding initializes"), Fixture.Until([&]
        { return Routine->IsRoutineInitialized(); }))) return false;
    UCommonAIDailyLifeComponent* Duplicate = NewObject<UCommonAIDailyLifeComponent>(Character);
    Duplicate->Profile = Profile;
    Duplicate->InitialState = Routine->ExportState();
    UCommonAIAgentComponent* Agent = Routine->GetRoutineAgent();
    if (!TestNotNull(TEXT("The original resident owns the controller binding"), Agent)) return false;
    TestFalse(TEXT("A second routine cannot take an active controller binding"), Duplicate->InitializeRoutine(Agent, nullptr));
    TestTrue(TEXT("The rejected routine preserves the original input producer"), Agent->GetInputSource(CommonAIDailyLife::SourceTag()) == Routine);
    TestTrue(TEXT("The rejected routine preserves the original movement adapter"), Agent->GetMovementAdapter() == Routine);
    TestFalse(TEXT("A second component cannot register the same stable identity"), Saves->RegisterResident(Duplicate));
    TestFalse(TEXT("An unregistered component cannot remember over the owner"), Saves->RememberResident(Duplicate));
    Saves->UnregisterResident(Duplicate);
    TestTrue(TEXT("Foreign unregister preserves the real owner"), Saves->RegisterResident(Routine));
    UCommonAIDailyLifeComponent* Invalid = NewObject<UCommonAIDailyLifeComponent>(Character);
    Invalid->Profile = Profile;
    Invalid->InitialState = Routine->ExportState();
    Invalid->InitialState.Identity = FGuid::NewGuid();
    Invalid->InitialState.Hunger = -1.0f;
    TestFalse(TEXT("Invalid resident state is rejected before registration"), Saves->RegisterResident(Invalid));
    {
        FPortableDailyLifeWorld OtherWorld;
        AActor* OtherOwner = OtherWorld.World->SpawnActor<AActor>();
        UCommonAIDailyLifeComponent* Foreign = NewObject<UCommonAIDailyLifeComponent>(OtherOwner);
        Foreign->Profile = Profile;
        Foreign->InitialState = Routine->ExportState();
        TestFalse(TEXT("Cross-world resident registration is rejected"), Saves->RegisterResident(Foreign));
    }

    TStrongObjectPtr<UCommonAIDailyLifeSaveGame> Snapshot(Saves->CreateSave());
    if (!TestNotNull(TEXT("The registered owner still produces a valid snapshot"), Snapshot.Get())
        || !TestEqual(TEXT("Only the registered identity is captured"), Snapshot->Residents.Num(), 1)) return false;
    TestEqual(TEXT("Clock scale is part of the saved state"), Snapshot->Clock.TimeScale, TimeScale);
    TStrongObjectPtr<UCommonAIDailyLifeSaveGame> Malformed(DuplicateObject<UCommonAIDailyLifeSaveGame>(Snapshot.Get(), GetTransientPackage()));
    const FCommonAIDailyLifeState DuplicateState = Malformed->Residents[0];
    Malformed->Residents.Add(DuplicateState);
    Malformed->Clock.TimeHours = ClockProbeHours;
    TestFalse(TEXT("Duplicate save identities are rejected"), Saves->RestoreSave(Malformed.Get()));
    TestEqual(TEXT("Duplicate saves leave the clock unchanged"), Clock->GetTimeHours(), PausedHours);
    Malformed->Residents.Pop();
    Malformed->Residents[0].Occupation = FGameplayTag::RequestGameplayTag(TEXT("CommonAITest.Occupation.Other"));
    TestFalse(TEXT("A valid but incompatible occupation is rejected"), Saves->RestoreSave(Malformed.Get()));
    TestEqual(TEXT("Incompatible saves leave the clock unchanged"), Clock->GetTimeHours(), PausedHours);
    Malformed->Residents[0].Occupation = {};
    TestFalse(TEXT("Malformed resident records are rejected"), Saves->RestoreSave(Malformed.Get()));
    TestEqual(TEXT("Malformed saves leave the clock unchanged"), Clock->GetTimeHours(), PausedHours);

    TStrongObjectPtr<UCommonAIDailyLifeSaveGame> ResidentOnly(Saves->CreateSave(false));
    if (!TestNotNull(TEXT("An external time authority can request resident-only state"), ResidentOnly.Get())) return false;
    TestFalse(TEXT("Resident-only state does not own the clock"), ResidentOnly->bHasClock);
    Clock->SetTimeHours(ClockProbeHours);
    TestTrue(TEXT("Resident-only state restores normally"), Saves->RestoreSave(ResidentOnly.Get()));
    TestEqual(TEXT("Resident-only restore preserves current time"), Clock->GetTimeHours(), ClockProbeHours);
    TestTrue(TEXT("The full snapshot restores clock state"), Saves->RestoreSave(Snapshot.Get()));
    TestEqual(TEXT("Full restore recovers time"), Clock->GetTimeHours(), PausedHours);
    TestEqual(TEXT("Full restore recovers scale"), Clock->GetState().TimeScale, TimeScale);
    TestTrue(TEXT("Full restore recovers pause"), Clock->GetState().bPaused);

    {
        FPortableDailyLifeWorld ExternalFixture;
        UCommonAIDailyLifeClockSubsystem* DefaultClock = ExternalFixture.World->GetSubsystem<UCommonAIDailyLifeClockSubsystem>();
        UCommonAIDailyLifeSaveSubsystem* ExternalSaves = ExternalFixture.World->GetSubsystem<UCommonAIDailyLifeSaveSubsystem>();
        if (!TestNotNull(TEXT("The external-context fixture has a default clock"), DefaultClock)
            || !TestNotNull(TEXT("The external-context fixture has shared persistence"), ExternalSaves)) return false;
        DefaultClock->SetTimeHours(StartHours);
        DefaultClock->SetPaused(true);
        const FGuid LateIdentity = FGuid::NewGuid();
        ACharacter* LateCharacter = ExternalFixture.Resident(nullptr, LateIdentity);
        UCommonAIDailyLifeComponent* LateRoutine = LateCharacter->FindComponentByClass<UCommonAIDailyLifeComponent>();
        TestTrue(TEXT("The unconfigured component has already begun play"), LateRoutine->HasBegunPlay());
        TestFalse(TEXT("An unconfigured component has no initialized routine"), LateRoutine->IsRoutineInitialized());
        UCommonAIAgentComponent* LateAgent = NewObject<UCommonAIAgentComponent>(LateCharacter->GetController());
        LateAgent->bStartAutomatically = false;
        LateAgent->RegisterComponent();
        LateRoutine->Profile = ExternalFixture.Profile();
        constexpr float ExternalHungerPerHour = 0.001f;
        LateRoutine->Profile->HungerPerHour = ExternalHungerPerHour;
        TStrongObjectPtr<UCommonAIDailyLifeContextFixture> ExternalContext(NewObject<UCommonAIDailyLifeContextFixture>());
        constexpr double ExternalTimeHours = 100.0;
        ExternalContext->TimeHours = ExternalTimeHours;
        if (!TestTrue(TEXT("A late-configured resident initializes through the explicit public entry point"),
            LateRoutine->InitializeRoutine(LateAgent, ExternalContext.Get()))) return false;
        const FCommonAIDailyLifeState ExternalBefore = LateRoutine->ExportState();
        TestEqual(TEXT("Explicit initialization uses external time"), ExternalBefore.LastTimeHours, ExternalTimeHours);
        TStrongObjectPtr<UCommonAIDailyLifeSaveGame> LateSave(ExternalSaves->CreateSave(false));
        if (!TestNotNull(TEXT("Explicit initialization registers the late-configured resident for saving"), LateSave.Get())) return false;
        const FCommonAIDailyLifeState* SavedLateResident = LateSave->Residents.FindByPredicate(
            [&](const FCommonAIDailyLifeState& State) { return State.Identity == LateIdentity; });
        if (!TestNotNull(TEXT("The resident-only snapshot contains the late-configured stable identity"), SavedLateResident)) return false;
        TestEqual(TEXT("The resident snapshot retains the external time anchor"), SavedLateResident->LastTimeHours, ExternalTimeHours);

        LateRoutine->ShutdownRoutine();
        const FCommonAIDailyLifeState ExternalStopped = LateRoutine->ExportState();
        TestEqual(TEXT("Shutdown preserves the external clock source"), LateRoutine->GetCurrentTimeHours(), ExternalTimeHours);
        TestEqual(TEXT("An unbound resident retains its external time anchor"), ExternalStopped.LastTimeHours, ExternalTimeHours);
        TestEqual(TEXT("Shutdown cannot integrate needs against the default clock"), ExternalStopped.Hunger, ExternalBefore.Hunger);
        LateRoutine->RefreshControllerBinding();
        if (!TestTrue(TEXT("Controller rebinding resumes the late-configured resident"), ExternalFixture.Until([&]
            { return LateRoutine->IsRoutineInitialized(); }))) return false;
        const FCommonAIDailyLifeState ExternalRebound = LateRoutine->ExportState();
        TestTrue(TEXT("Rebinding retains the same external context object"), LateRoutine->RoutineContext == ExternalContext.Get());
        TestEqual(TEXT("Rebinding keeps external time authoritative"), ExternalRebound.LastTimeHours, ExternalTimeHours);
        TestEqual(TEXT("Rebinding does not manufacture 94 hours of hunger growth"), ExternalRebound.Hunger, ExternalBefore.Hunger);
        TestEqual(TEXT("External-context lifecycle leaves the default clock untouched"), DefaultClock->GetTimeHours(), StartHours);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIDailyLifeSharedContentTest, "CommonAI.DailyLife.SharedContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCommonAIDailyLifeSharedContentTest::RunTest(const FString& Parameters)
{
    const USmartObjectDefinition* Definition = LoadObject<USmartObjectDefinition>(nullptr,
        TEXT("/CommonAI/DailyLife/SmartObjects/SO_DailyLifeSingleSlot.SO_DailyLifeSingleSlot"));
    if (!TestNotNull(TEXT("The shared single-slot Smart Object definition loads"), Definition)
        || !TestEqual(TEXT("The shared Smart Object has exactly one slot"), Definition->GetSlots().Num(), 1)) return false;
    TestTrue(TEXT("The shared Smart Object slot is aligned with its actor origin"), Definition->GetSlots()[0].Offset.IsNearlyZero());
    TestNotNull(TEXT("The shared Smart Object slot carries the CommonAI activity behavior"),
        Definition->GetBehaviorDefinition(0, UCommonAISmartObjectBehaviorDefinition::StaticClass()));
    return true;
}
#endif
