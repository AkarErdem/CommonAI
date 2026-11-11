#include "DailyLife/CommonAIDailyLifeClockSubsystem.h"
#include "DailyLife/CommonAIDailyLifeTypes.h"
#include "CommonAILog.h"
#include "Engine/World.h"

bool FCommonAIDailyLifeClockState::IsValid() const
{
    return FMath::IsFinite(TimeHours) && TimeHours >= 0.0
        && FMath::IsFinite(DayLengthSeconds) && DayLengthSeconds > 0.0
        && FMath::IsFinite(TimeScale) && TimeScale >= 0.0;
}

void UCommonAIDailyLifeClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (!InitialState.IsValid())
    {
        UE_LOG(LogCommonAI, Warning, TEXT("Invalid initial daily-life clock settings; using the default clock settings."));
    }
    CurrentState = InitialState.IsValid() ? InitialState : FCommonAIDailyLifeClockState();
    AnchorWorldSeconds = GetWorld()->GetTimeSeconds();
}

void UCommonAIDailyLifeClockSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    AnchorWorldSeconds = InWorld.GetTimeSeconds();
}

bool UCommonAIDailyLifeClockSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UCommonAIDailyLifeClockSubsystem::CanModifyClock() const
{
    return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

double UCommonAIDailyLifeClockSubsystem::GetTimeHours() const
{
    const UWorld* World = GetWorld();
    const double ElapsedSeconds = World && HasCalledBeginPlay()
        ? FMath::Max(0.0, World->GetTimeSeconds() - AnchorWorldSeconds) : 0.0;
    return CurrentState.TimeHours + (CurrentState.bPaused ? 0.0
        : ElapsedSeconds * CurrentState.TimeScale * CommonAIDailyLife::HoursPerDay / CurrentState.DayLengthSeconds);
}

FCommonAIDailyLifeClockState UCommonAIDailyLifeClockSubsystem::GetState() const
{
    FCommonAIDailyLifeClockState State = CurrentState;
    State.TimeHours = GetTimeHours();
    return State;
}

bool UCommonAIDailyLifeClockSubsystem::RestoreState(const FCommonAIDailyLifeClockState& State)
{
    if (!CanModifyClock() || !State.IsValid()) return false;
    CurrentState = State;
    AnchorWorldSeconds = GetWorld()->GetTimeSeconds();
    return true;
}

bool UCommonAIDailyLifeClockSubsystem::SetTimeHours(double TimeHours)
{
    FCommonAIDailyLifeClockState State = GetState();
    State.TimeHours = TimeHours;
    return RestoreState(State);
}

bool UCommonAIDailyLifeClockSubsystem::SetDayLengthSeconds(double DayLengthSeconds)
{
    FCommonAIDailyLifeClockState State = GetState();
    State.DayLengthSeconds = DayLengthSeconds;
    return RestoreState(State);
}

bool UCommonAIDailyLifeClockSubsystem::SetTimeScale(double TimeScale)
{
    FCommonAIDailyLifeClockState State = GetState();
    State.TimeScale = TimeScale;
    return RestoreState(State);
}

bool UCommonAIDailyLifeClockSubsystem::SetPaused(bool bPaused)
{
    FCommonAIDailyLifeClockState State = GetState();
    State.bPaused = bPaused;
    return RestoreState(State);
}
