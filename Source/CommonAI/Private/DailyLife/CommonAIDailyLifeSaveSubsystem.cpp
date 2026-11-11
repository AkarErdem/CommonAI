#include "DailyLife/CommonAIDailyLifeSaveSubsystem.h"
#include "DailyLife/CommonAIDailyLifeComponent.h"
#include "DailyLife/CommonAIDailyLifeSaveGame.h"
#include "CommonAILog.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Templates/UnrealTemplate.h"

namespace
{
    bool IsResidentAvailable(const UCommonAIDailyLifeComponent* Resident, const UWorld* World)
    {
        return IsValid(Resident) && Resident->GetWorld() == World
            && IsValid(Resident->GetOwner()) && !Resident->GetOwner()->IsActorBeingDestroyed();
    }
}

bool UCommonAIDailyLifeSaveSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UCommonAIDailyLifeSaveSubsystem::HasAuthority() const
{
    return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

bool UCommonAIDailyLifeSaveSubsystem::IsRegisteredResident(const UCommonAIDailyLifeComponent* Resident, const FGuid& Identity) const
{
    const TWeakObjectPtr<UCommonAIDailyLifeComponent>* Registered = RegisteredResidents.Find(Identity);
    return IsValid(Resident) && Resident->GetWorld() == GetWorld() && Registered && Registered->Get() == Resident;
}

bool UCommonAIDailyLifeSaveSubsystem::AreResidentsCurrent(const TMap<FGuid, TWeakObjectPtr<UCommonAIDailyLifeComponent>>& Residents) const
{
    if (Residents.Num() != RegisteredResidents.Num()) return false;
    for (const auto& Entry : Residents)
    {
        if (!IsResidentAvailable(Entry.Value.Get(), GetWorld()) || !IsRegisteredResident(Entry.Value.Get(), Entry.Key)) return false;
    }
    return true;
}

bool UCommonAIDailyLifeSaveSubsystem::CollectResidents(TMap<FGuid, TWeakObjectPtr<UCommonAIDailyLifeComponent>>& OutResidents,
    TMap<FGuid, FCommonAIDailyLifeState>& OutStates)
{
    for (auto It = RegisteredResidents.CreateIterator(); It; ++It)
    {
        if (!IsResidentAvailable(It.Value().Get(), GetWorld())) It.RemoveCurrent();
    }
    OutResidents = RegisteredResidents;
    OutStates.Reset();
    for (const auto& Entry : OutResidents)
    {
        UCommonAIDailyLifeComponent* Resident = Entry.Value.Get();
        if (!IsResidentAvailable(Resident, GetWorld()) || !IsRegisteredResident(Resident, Entry.Key)) return false;
        const FCommonAIDailyLifeState State = Resident->ExportState();
        // Context time providers can execute user callbacks; never keep iterators into the live registry.
        if (!IsResidentAvailable(Resident, GetWorld()) || !IsRegisteredResident(Resident, Entry.Key)
            || !State.IsValid() || State.Identity != Entry.Key || !Resident->CanRestoreState(State))
        {
            UE_LOG(LogCommonAI, Warning, TEXT("Daily-life snapshot rejected: resident identity, state or ownership changed."));
            return false;
        }
        OutStates.Add(Entry.Key, State);
    }
    return AreResidentsCurrent(OutResidents);
}

bool UCommonAIDailyLifeSaveSubsystem::RegisterResident(UCommonAIDailyLifeComponent* Resident)
{
    if (!HasAuthority() || bOperationInProgress || !IsResidentAvailable(Resident, GetWorld())) return false;
    TGuardValue<bool> OperationGuard(bOperationInProgress, true);
    const FCommonAIDailyLifeState State = Resident->ExportState();
    if (!IsResidentAvailable(Resident, GetWorld()) || !State.IsValid() || !Resident->CanRestoreState(State)) return false;
    for (auto It = RegisteredResidents.CreateIterator(); It; ++It)
    {
        if (!IsResidentAvailable(It.Value().Get(), GetWorld())) { It.RemoveCurrent(); continue; }
        if (It.Value().Get() == Resident) return It.Key() == State.Identity;
        if (It.Key() == State.Identity)
        {
            UE_LOG(LogCommonAI, Warning, TEXT("Daily-life registration rejected: duplicate resident identity."));
            return false;
        }
    }
    RegisteredResidents.Add(State.Identity, Resident);
    if (RestoreRegisteredResident(Resident, State.Identity)) return true;
    if (IsRegisteredResident(Resident, State.Identity)) RegisteredResidents.Remove(State.Identity);
    return false;
}

void UCommonAIDailyLifeSaveSubsystem::UnregisterResident(UCommonAIDailyLifeComponent* Resident, bool bRemember)
{
    if (!HasAuthority() || !Resident) return;
    FGuid Identity;
    for (const auto& Entry : RegisteredResidents)
    {
        if (Entry.Value.Get() == Resident) { Identity = Entry.Key; break; }
    }
    if (!Identity.IsValid()) return;
    if (bRemember && !bOperationInProgress)
    {
        TGuardValue<bool> OperationGuard(bOperationInProgress, true);
        if (!RememberRegisteredResident(Resident))
        {
            UE_LOG(LogCommonAI, Warning, TEXT("Daily-life unload could not capture resident state; the previous retained record is preserved."));
        }
    }
    // Teardown must release ownership even during restore callbacks. Keep the in-flight retained snapshot intact.
    if (IsRegisteredResident(Resident, Identity)) RegisteredResidents.Remove(Identity);
}

bool UCommonAIDailyLifeSaveSubsystem::RestoreRegisteredResident(UCommonAIDailyLifeComponent* Resident, const FGuid& Identity)
{
    if (!IsResidentAvailable(Resident, GetWorld()) || !IsRegisteredResident(Resident, Identity)) return false;
    const FCommonAIDailyLifeState* Found = RetainedResidents.Find(Identity);
    if (!Found) return true;
    // Restore can stop and restart execution, so callbacks must not invalidate the record being applied.
    const FCommonAIDailyLifeState State = *Found;
    if (!Resident->CanRestoreState(State)) return false;
    const bool bRestored = Resident->RestoreState(State);
    return bRestored && IsResidentAvailable(Resident, GetWorld()) && IsRegisteredResident(Resident, Identity);
}

bool UCommonAIDailyLifeSaveSubsystem::RestoreResident(UCommonAIDailyLifeComponent* Resident)
{
    if (!HasAuthority() || bOperationInProgress || !IsResidentAvailable(Resident, GetWorld())) return false;
    TGuardValue<bool> OperationGuard(bOperationInProgress, true);
    const FCommonAIDailyLifeState State = Resident->ExportState();
    return State.IsValid() && RestoreRegisteredResident(Resident, State.Identity);
}

bool UCommonAIDailyLifeSaveSubsystem::RememberRegisteredResident(const UCommonAIDailyLifeComponent* Resident)
{
    if (!IsValid(Resident) || Resident->GetWorld() != GetWorld()) return false;
    const FCommonAIDailyLifeState State = Resident->ExportState();
    // Remembering during EndPlay must not require a still-possessed execution context.
    if (!State.IsValid() || !IsRegisteredResident(Resident, State.Identity)) return false;
    RetainedResidents.Add(State.Identity, State);
    return true;
}

bool UCommonAIDailyLifeSaveSubsystem::RememberResident(const UCommonAIDailyLifeComponent* Resident)
{
    if (!HasAuthority() || bOperationInProgress) return false;
    TGuardValue<bool> OperationGuard(bOperationInProgress, true);
    return RememberRegisteredResident(Resident);
}

UCommonAIDailyLifeSaveGame* UCommonAIDailyLifeSaveSubsystem::CreateSave(bool bIncludeClock)
{
    if (!HasAuthority() || bOperationInProgress) return nullptr;
    TGuardValue<bool> OperationGuard(bOperationInProgress, true);
    UCommonAIDailyLifeClockSubsystem* Clock = bIncludeClock ? GetWorld()->GetSubsystem<UCommonAIDailyLifeClockSubsystem>() : nullptr;
    if (bIncludeClock && !Clock) return nullptr;
    TMap<FGuid, TWeakObjectPtr<UCommonAIDailyLifeComponent>> Residents;
    TMap<FGuid, FCommonAIDailyLifeState> States;
    if (!CollectResidents(Residents, States)) return nullptr;
    for (const auto& Entry : RetainedResidents)
    {
        if (!Entry.Value.IsValid() || Entry.Key != Entry.Value.Identity) return nullptr;
        if (!States.Contains(Entry.Key)) States.Add(Entry.Key, Entry.Value);
    }
    UCommonAIDailyLifeSaveGame* Save = Cast<UCommonAIDailyLifeSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UCommonAIDailyLifeSaveGame::StaticClass()));
    if (!Save) return nullptr;
    Save->bHasClock = bIncludeClock;
    if (Clock)
    {
        Save->Clock = Clock->GetState();
        if (!Save->Clock.IsValid()) return nullptr;
    }
    States.GenerateValueArray(Save->Residents);
    Save->Residents.Sort([](const FCommonAIDailyLifeState& Left, const FCommonAIDailyLifeState& Right)
    {
        return Left.Identity < Right.Identity;
    });
    return Save;
}

bool UCommonAIDailyLifeSaveSubsystem::RestoreSave(const UCommonAIDailyLifeSaveGame* Save)
{
    if (!HasAuthority() || bOperationInProgress || !IsValid(Save)) return false;
    TGuardValue<bool> OperationGuard(bOperationInProgress, true);
    const bool bRestoreClock = Save->bHasClock;
    const FCommonAIDailyLifeClockState ClockState = Save->Clock;
    UCommonAIDailyLifeClockSubsystem* Clock = bRestoreClock ? GetWorld()->GetSubsystem<UCommonAIDailyLifeClockSubsystem>() : nullptr;
    if (bRestoreClock && (!Clock || !ClockState.IsValid())) return false;
    TMap<FGuid, FCommonAIDailyLifeState> Incoming;
    for (const FCommonAIDailyLifeState& State : Save->Residents)
    {
        if (!State.IsValid() || Incoming.Contains(State.Identity))
        {
            UE_LOG(LogCommonAI, Warning, TEXT("Daily-life load rejected: invalid or duplicate saved identity."));
            return false;
        }
        Incoming.Add(State.Identity, State);
    }
    TMap<FGuid, TWeakObjectPtr<UCommonAIDailyLifeComponent>> Residents;
    TMap<FGuid, FCommonAIDailyLifeState> CurrentStates;
    if (!CollectResidents(Residents, CurrentStates)) return false;
    for (const auto& Entry : Residents)
    {
        if (const FCommonAIDailyLifeState* State = Incoming.Find(Entry.Key); State && !Entry.Value->CanRestoreState(*State))
        {
            UE_LOG(LogCommonAI, Warning, TEXT("Daily-life load rejected: saved state is incompatible with a registered resident."));
            return false;
        }
    }
    // Data validation is complete before writes. Lifecycle callbacks can still invalidate later applications.
    if (Clock && !Clock->RestoreState(ClockState)) return false;
    RetainedResidents = MoveTemp(Incoming);
    for (const auto& Entry : Residents)
    {
        if (!RestoreRegisteredResident(Entry.Value.Get(), Entry.Key) || !AreResidentsCurrent(Residents))
        {
            UE_LOG(LogCommonAI, Warning, TEXT("Daily-life load interrupted by resident lifecycle changes; some state may already be applied."));
            return false;
        }
    }
    return true;
}

bool UCommonAIDailyLifeSaveSubsystem::SaveToSlot(const FString& SlotName, int32 UserIndex, bool bIncludeClock)
{
    if (SlotName.IsEmpty() || UserIndex < 0) return false;
    UCommonAIDailyLifeSaveGame* Save = CreateSave(bIncludeClock);
    return Save && UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex);
}

bool UCommonAIDailyLifeSaveSubsystem::LoadFromSlot(const FString& SlotName, int32 UserIndex)
{
    if (!HasAuthority() || bOperationInProgress || SlotName.IsEmpty() || UserIndex < 0) return false;
    return RestoreSave(Cast<UCommonAIDailyLifeSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex)));
}
