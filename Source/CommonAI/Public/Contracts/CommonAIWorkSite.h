#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CommonAIWorkSite.generated.h"

class AAIController;
class UCommonAIWorkSite;

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIWorkOpportunity
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Common AI|Work")
	TObjectPtr<UCommonAIWorkSite> Site = nullptr;
};

// One site grants one execution at a time. The run ID prevents an old run from releasing a new claim.
UCLASS(Abstract, Blueprintable, ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAIWorkSite : public UActorComponent
{
	GENERATED_BODY()

public:
	bool IsAvailable() const;
	bool IsPerforming() const { return static_cast<bool>(WorkCompletion); }
	bool TryReserve(AAIController* Agent, uint64 RunId);
	bool IsReservedBy(const AAIController* Agent, uint64 RunId) const;
	void Release(AAIController* Agent, uint64 RunId);
	virtual bool BeginWork(AAIController* Agent, uint64 RunId, TFunction<void(bool)> Completion);

protected:
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void CancelWork();
	void CompleteWork(AAIController* Agent, uint64 RunId, bool bSucceeded);
	AAIController* GetReservationOwner() const { return ReservationOwner.Get(); }
	uint64 GetReservationRunId() const { return ReservationRunId; }

private:
	TWeakObjectPtr<AAIController> ReservationOwner;
	uint64 ReservationRunId = 0;
	TFunction<void(bool)> WorkCompletion;
};

UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAITimedWorkSite : public UCommonAIWorkSite
{
	GENERATED_BODY()

public:
	virtual bool BeginWork(AAIController* Agent, uint64 RunId, TFunction<void(bool)> Completion) override;

protected:
	virtual void CancelWork() override;

	UPROPERTY(EditAnywhere, Category = "Common AI|Work", meta = (ClampMin = "0.001", Units = "Seconds"))
	float WorkDurationSeconds = 2.0f;

private:
	FTimerHandle WorkTimer;
};

// An external neutral signal completes this site; game-specific effects stay in the consumer.
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAISignaledWorkSite : public UCommonAIWorkSite
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Common AI|Work")
	void SignalCompletion(bool bSucceeded);
};
