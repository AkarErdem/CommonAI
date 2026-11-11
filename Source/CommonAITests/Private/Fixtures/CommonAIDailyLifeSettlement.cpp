#include "Fixtures/CommonAIDailyLifeSettlement.h"
#include "AIController.h"
#include "Contracts/CommonAISmartObjectSite.h"
#include "DailyLife/CommonAIDailyLifeClockSubsystem.h"
#include "DailyLife/CommonAIDailyLifeComponent.h"
#include "DailyLife/CommonAIDailyLifeProfile.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpectatorPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogCommonAISettlement, Log, All);

ACommonAIDailyLifeSettlement::ACommonAIDailyLifeSettlement()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ACommonAIDailyLifeSettlement::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority()) return;
    UCommonAIDailyLifeClockSubsystem* Clock = GetWorld()->GetSubsystem<UCommonAIDailyLifeClockSubsystem>();
    if (!CharacterClass || !Clock || !Clock->SetDayLengthSeconds(DayLengthSeconds) || !Clock->SetTimeHours(StartHour))
    {
        StartupError = TEXT("Missing character reference or invalid settlement clock configuration.");
        UE_LOG(LogCommonAISettlement, Error, TEXT("%s"), *StartupError);
        return;
    }
    TSet<FGuid> Identities;
    for (const FCommonAIDailyLifeResident& Definition : Residents)
    {
        if (!Definition.Identity.IsValid() || Identities.Contains(Definition.Identity) ||
            !Definition.Profile || !Definition.Home || !Definition.Home->Site || (Definition.Work && !Definition.Work->Site))
        {
            StartupError = TEXT("Every resident requires a unique identity, profile and home.");
            break;
        }
        Identities.Add(Definition.Identity);
        ACharacter* Character = GetWorld()->SpawnActorDeferred<ACharacter>(CharacterClass, Definition.SpawnTransform,
            this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Character) { StartupError = TEXT("An authored resident could not be spawned."); break; }
        UCommonAIDailyLifeComponent* Routine = Character->FindComponentByClass<UCommonAIDailyLifeComponent>();
        if (!Routine)
        {
            Routine = NewObject<UCommonAIDailyLifeComponent>(Character);
            Character->AddInstanceComponent(Routine);
        }
        Routine->Profile = Definition.Profile;
        Routine->InitialState.Identity = Definition.Identity;
        Routine->InitialState.DisplayName = Definition.Name.ToString();
        Routine->InitialState.Hunger = Definition.InitialHunger;
        Routine->InitialState.Fatigue = Definition.InitialFatigue;
        Routine->InitialState.HomeId = Definition.Home->Site->SiteId;
        Routine->InitialState.WorkId = Definition.Work ? Definition.Work->Site->SiteId : FGuid();
        Routine->Home = Definition.Home->Site;
        Routine->Work = Definition.Work ? Definition.Work->Site : nullptr;
        for (ACommonAIActivityLocation* Location : SharedLocations)
            if (Location && Location->Site) Routine->Destinations.AddUnique(Location->Site);
        if (!Routine->IsRegistered()) Routine->RegisterComponent();
        if (!Character->AIControllerClass) Character->AIControllerClass = AAIController::StaticClass();
        Character->FinishSpawning(Definition.SpawnTransform);
        if (!Character->GetController()) Character->SpawnDefaultController();
        Character->GetCharacterMovement()->SetAvoidanceEnabled(Definition.bUseRVOAvoidance);
        Population.Add(Character);
    }
    if (!StartupError.IsEmpty()) UE_LOG(LogCommonAISettlement, Error, TEXT("%s"), *StartupError);
}

void ACommonAIDailyLifeSettlement::EndPlay(EEndPlayReason::Type Reason)
{
    for (ACharacter* Resident : Population) if (IsValid(Resident)) Resident->Destroy();
    Population.Reset();
    Super::EndPlay(Reason);
}

ACommonAIDailyLifeSettlementGameMode::ACommonAIDailyLifeSettlementGameMode()
{
    DefaultPawnClass = ASpectatorPawn::StaticClass();
}
