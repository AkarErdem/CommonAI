#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DailyLife/CommonAIDailyLifeTypes.h"
#include "CommonAIDailyLifeSaveSubsystem.generated.h"

class UCommonAIDailyLifeComponent;
class UCommonAIDailyLifeSaveGame;

/** Explicitly registered residents and retained records for unloaded representations. */
UCLASS()
class COMMONAI_API UCommonAIDailyLifeSaveSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** External time authorities use resident-only snapshots. No automatic disk saves are performed. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Common AI|Daily Life|Save")
    UCommonAIDailyLifeSaveGame* CreateSave(bool bIncludeClock = true);

    /**
     * Validates all records and loaded identities before changing clock or resident state.
     * Returns false if a lifecycle callback interrupts application; already applied state is not rolled back.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Common AI|Daily Life|Save")
    bool RestoreSave(const UCommonAIDailyLifeSaveGame* Save);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Common AI|Daily Life|Save")
    bool SaveToSlot(const FString& SlotName, int32 UserIndex = 0, bool bIncludeClock = true);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Common AI|Daily Life|Save")
    bool LoadFromSlot(const FString& SlotName, int32 UserIndex = 0);

    /** Registers one prepared stable identity and restores its retained state once. Repeated registration is idempotent. */
    bool RegisterResident(UCommonAIDailyLifeComponent* Resident);
    void UnregisterResident(UCommonAIDailyLifeComponent* Resident, bool bRemember = true);
    bool RestoreResident(UCommonAIDailyLifeComponent* Resident);
    bool RememberResident(const UCommonAIDailyLifeComponent* Resident);

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
    bool HasAuthority() const;
    bool IsRegisteredResident(const UCommonAIDailyLifeComponent* Resident, const FGuid& Identity) const;
    bool AreResidentsCurrent(const TMap<FGuid, TWeakObjectPtr<UCommonAIDailyLifeComponent>>& Residents) const;
    bool CollectResidents(TMap<FGuid, TWeakObjectPtr<UCommonAIDailyLifeComponent>>& OutResidents,
        TMap<FGuid, FCommonAIDailyLifeState>& OutStates);
    bool RestoreRegisteredResident(UCommonAIDailyLifeComponent* Resident, const FGuid& Identity);
    bool RememberRegisteredResident(const UCommonAIDailyLifeComponent* Resident);

    TMap<FGuid, TWeakObjectPtr<UCommonAIDailyLifeComponent>> RegisteredResidents;
    UPROPERTY(Transient)
    TMap<FGuid, FCommonAIDailyLifeState> RetainedResidents;
    bool bOperationInProgress = false;
};
