#include "Contracts/CommonAIWorkSite.h"

#include "AIController.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool UCommonAIWorkSite::IsAvailable() const
{
	return IsValid(GetOwner()) && !ReservationOwner.IsValid();
}

bool UCommonAIWorkSite::TryReserve(AAIController* Agent, uint64 RunId)
{
	if (!IsValid(Agent) || RunId == 0 || !IsAvailable())
	{
		return false;
	}
	if (ReservationRunId != 0)
	{
		CancelWork();
		WorkCompletion = nullptr;
	}
	ReservationOwner = Agent;
	ReservationRunId = RunId;
	return true;
}

bool UCommonAIWorkSite::IsReservedBy(const AAIController* Agent, uint64 RunId) const
{
	return Agent && ReservationOwner.Get() == Agent && ReservationRunId == RunId;
}

void UCommonAIWorkSite::Release(AAIController* Agent, uint64 RunId)
{
	if (!IsReservedBy(Agent, RunId))
	{
		return;
	}
	CancelWork();
	WorkCompletion = nullptr;
	ReservationOwner.Reset();
	ReservationRunId = 0;
}

bool UCommonAIWorkSite::BeginWork(AAIController* Agent, uint64 RunId, TFunction<void(bool)> Completion)
{
	if (!IsReservedBy(Agent, RunId) || WorkCompletion)
	{
		return false;
	}
	WorkCompletion = MoveTemp(Completion);
	return true;
}

void UCommonAIWorkSite::CompleteWork(AAIController* Agent, uint64 RunId, bool bSucceeded)
{
	if (IsReservedBy(Agent, RunId) && WorkCompletion)
	{
		TFunction<void(bool)> Completion = MoveTemp(WorkCompletion);
		Completion(bSucceeded);
	}
}

void UCommonAIWorkSite::CancelWork()
{
}

void UCommonAIWorkSite::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	CancelWork();
	WorkCompletion = nullptr;
	ReservationOwner.Reset();
	ReservationRunId = 0;
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UCommonAITimedWorkSite::BeginWork(AAIController* Agent, uint64 RunId, TFunction<void(bool)> Completion)
{
	if (!GetWorld() || WorkDurationSeconds <= 0.0f || !Super::BeginWork(Agent, RunId, MoveTemp(Completion)))
	{
		return false;
	}
	TWeakObjectPtr<UCommonAITimedWorkSite> WeakThis(this);
	TWeakObjectPtr<AAIController> WeakAgent(Agent);
	GetWorld()->GetTimerManager().SetTimer(WorkTimer, [WeakThis, WeakAgent, RunId]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->CompleteWork(WeakAgent.Get(), RunId, true);
		}
	}, WorkDurationSeconds, false);
	return true;
}

void UCommonAITimedWorkSite::CancelWork()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorkTimer);
	}
}

void UCommonAISignaledWorkSite::SignalCompletion(bool bSucceeded)
{
	// Only the current reservation can receive the signal.
	if (AAIController* Agent = GetReservationOwner())
	{
		CompleteWork(Agent, GetReservationRunId(), bSucceeded);
	}
}
