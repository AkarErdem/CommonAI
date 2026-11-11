#pragma once
#include "CoreMinimal.h"
#include "Execution/CommonAIExecution.h"
#include "BehaviorTree/BTTaskNode.h"
#include "CommonAIBehaviorTreeExecution.generated.h"

class UBehaviorTree;
class UBehaviorTreeComponent;

// Opt-in engine BT binding. It never replaces an unrelated controller brain.
UCLASS(DisplayName = "Behavior Tree")
class COMMONAI_API UCommonAIBehaviorTreeExecution : public UCommonAIExecution
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Behavior Tree") TObjectPtr<UBehaviorTree> Tree;
    bool OwnsTree(const UBehaviorTreeComponent* Component) const;
    virtual bool RequiresTick() const override { return true; }
protected:
    virtual void OnStarted_Implementation(const FCommonAIExecutionContext& Context) override;
    virtual void OnTick_Implementation(float DeltaSeconds) override;
    virtual void OnCancellationRequested_Implementation(FGameplayTag Reason) override;
    virtual void OnAbandoned_Implementation(FGameplayTag Reason) override;
    virtual ECommonAICompletionDisposition PrepareCompletion(ECommonAIInteractionStatus Status, FGameplayTag Reason) override;
private:
    TWeakObjectPtr<UBehaviorTreeComponent> Component;
    ECommonAIInteractionStatus PendingResult = ECommonAIInteractionStatus::Completed;
    FGameplayTag PendingReason;
    bool bFinishRequested = false;
    bool bStopRequested = false;
};

// Use at the terminal point of a CommonAI-owned tree; resource cleanup finishes before the activity does.
UCLASS(meta = (DisplayName = "Finish CommonAI Activity"))
class COMMONAI_API UBTTask_CommonAICompleteActivity : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_CommonAICompleteActivity();
    UPROPERTY(EditAnywhere, Category = "Result") bool bSucceeded = true;
    UPROPERTY(EditAnywhere, Category = "Result") FGameplayTag Reason;
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
