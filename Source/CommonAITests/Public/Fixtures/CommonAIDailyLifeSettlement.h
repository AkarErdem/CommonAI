#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "CommonAIDailyLifeSettlement.generated.h"

class ACharacter;
class UCommonAIDailyLifeProfile;
class ACommonAIActivityLocation;

/** Authored test population data. Production components own all routine behavior. */
USTRUCT(BlueprintType)
struct COMMONAITESTS_API FCommonAIDailyLifeResident
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Settlement") FGuid Identity;
    UPROPERTY(EditAnywhere, Category="Settlement") FName Name;
    UPROPERTY(EditAnywhere, Category="Settlement") TObjectPtr<UCommonAIDailyLifeProfile> Profile;
    UPROPERTY(EditAnywhere, Category="Settlement") TObjectPtr<ACommonAIActivityLocation> Home;
    UPROPERTY(EditAnywhere, Category="Settlement") TObjectPtr<ACommonAIActivityLocation> Work;
    UPROPERTY(EditAnywhere, Category="Settlement") FTransform SpawnTransform;
    UPROPERTY(EditAnywhere, Category="Settlement") bool bUseRVOAvoidance = true;
    UPROPERTY(EditAnywhere, Category="Settlement", meta=(ClampMin="0", ClampMax="1")) float InitialHunger = 0.2f;
    UPROPERTY(EditAnywhere, Category="Settlement", meta=(ClampMin="0", ClampMax="1")) float InitialFatigue = 0.2f;
};

/** Development population with authored character classes and explicit initial conditions. */
UCLASS()
class COMMONAITESTS_API ACommonAIDailyLifeSettlement : public AActor
{
    GENERATED_BODY()
public:
    ACommonAIDailyLifeSettlement();
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, Category="Settlement") TSubclassOf<ACharacter> CharacterClass;
    UPROPERTY(EditAnywhere, Category="Settlement") TArray<FCommonAIDailyLifeResident> Residents;
    UPROPERTY(EditAnywhere, Category="Settlement") TArray<TObjectPtr<ACommonAIActivityLocation>> SharedLocations;
    UPROPERTY(EditAnywhere, Category="Settlement", meta=(ClampMin="1", Units="s")) double DayLengthSeconds = 480.;
    UPROPERTY(EditAnywhere, Category="Settlement", meta=(ClampMin="0", ClampMax="24")) double StartHour = 6.;
    UPROPERTY(Transient) TArray<TObjectPtr<ACharacter>> Population;
    UPROPERTY(VisibleInstanceOnly, Category="Settlement") FString StartupError;
};

/** Fly camera uses the engine's ordinary spectator input and developer console. */
UCLASS()
class COMMONAITESTS_API ACommonAIDailyLifeSettlementGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACommonAIDailyLifeSettlementGameMode();
};
