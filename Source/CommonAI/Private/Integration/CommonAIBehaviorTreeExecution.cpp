#include "Integration/CommonAIBehaviorTreeExecution.h"
#include "CommonAIAgentComponent.h"
#include "CommonAITags.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

void UCommonAIBehaviorTreeExecution::OnStarted_Implementation(const FCommonAIExecutionContext& ExecutionContext)
{
    Component.Reset();
    bFinishRequested = bStopRequested = false;
    PendingResult = ECommonAIInteractionStatus::Completed;
    PendingReason = {};
    AAIController* Controller = ExecutionContext.Request.Agent;
    if (!IsValid(Controller) || !Tree || !Tree->RootNode)
    {
        CommitCompletion(ExecutionContext.Invocation, ECommonAIInteractionStatus::Failed, CommonAITags::Reason_InvalidConfiguration);
        return;
    }
    UBehaviorTreeComponent* Existing = Cast<UBehaviorTreeComponent>(Controller->GetBrainComponent());
    if ((Controller->GetBrainComponent() && !Existing)
        || (Existing && (Existing->TreeHasBeenStarted() || Existing->IsAbortPending() || Existing->IsPaused())))
    {
        CommitCompletion(ExecutionContext.Invocation, ECommonAIInteractionStatus::Failed, CommonAITags::Reason_BrainBusy);
        return;
    }
    UBlackboardComponent* Blackboard = Controller->GetBlackboardComponent();
    if (Tree->BlackboardAsset && Blackboard && !Blackboard->IsCompatibleWith(Tree->BlackboardAsset))
    {
        CommitCompletion(ExecutionContext.Invocation, ECommonAIInteractionStatus::Failed, CommonAITags::Reason_BrainBusy);
        return;
    }
    if (Tree->BlackboardAsset && !Controller->UseBlackboard(Tree->BlackboardAsset, Blackboard))
    {
        CommitCompletion(ExecutionContext.Invocation, ECommonAIInteractionStatus::Failed, CommonAITags::Reason_InvalidConfiguration);
        return;
    }
    if (!Existing)
    {
        Existing = NewObject<UBehaviorTreeComponent>(Controller);
        Existing->RegisterComponent();
        Controller->BrainComponent = Existing;
    }
    Component = Existing;
    Existing->StartTree(*Tree, EBTExecutionMode::Looped);
}

bool UCommonAIBehaviorTreeExecution::OwnsTree(const UBehaviorTreeComponent* InComponent) const
{
    return Component.IsValid() && Component.Get() == InComponent && Component->GetRootTree() == Tree && !GetResult().IsTerminal();
}

ECommonAICompletionDisposition UCommonAIBehaviorTreeExecution::PrepareCompletion(ECommonAIInteractionStatus Status, FGameplayTag Reason)
{
    if (bFinishRequested) return ECommonAICompletionDisposition::Rejected;
    PendingResult = Status;
    PendingReason = Reason;
    bFinishRequested = true;
    return ECommonAICompletionDisposition::Deferred;
}

void UCommonAIBehaviorTreeExecution::OnCancellationRequested_Implementation(FGameplayTag Reason)
{
    PendingResult = ECommonAIInteractionStatus::Cancelled;
    PendingReason = Reason;
    bFinishRequested = true;
}

void UCommonAIBehaviorTreeExecution::OnTick_Implementation(float DeltaSeconds)
{
    UBehaviorTreeComponent* Brain = Component.Get();
    if (!Brain || (Brain->TreeHasBeenStarted() && Brain->GetRootTree() != Tree))
    {
        CommitCompletion(GetContext().Invocation, ECommonAIInteractionStatus::Failed, CommonAITags::Reason_TargetLost);
        return;
    }
    if (bFinishRequested && !bStopRequested)
    {
        bStopRequested = true;
        Brain->StopTree(EBTStopMode::Safe);
    }
    // A sleeping tree with a latent abort needs its own engine tick to observe the acknowledgement.
    if (bStopRequested && Brain->IsAbortPending()) Brain->ScheduleNextTick(0.0f);
    if (!Brain->TreeHasBeenStarted() && !Brain->IsAbortPending())
        CommitCompletion(GetContext().Invocation, bStopRequested ? PendingResult : ECommonAIInteractionStatus::Failed,
            bStopRequested ? PendingReason : CommonAITags::Reason_TargetLost);
}

void UCommonAIBehaviorTreeExecution::OnAbandoned_Implementation(FGameplayTag Reason)
{
    if (UBehaviorTreeComponent* Brain = Component.Get())
        if (Brain->GetRootTree() == Tree) Brain->StopTree(EBTStopMode::Forced);
    Component.Reset();
}

UBTTask_CommonAICompleteActivity::UBTTask_CommonAICompleteActivity()
{
    NodeName = TEXT("Finish CommonAI Activity");
}

EBTNodeResult::Type UBTTask_CommonAICompleteActivity::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UCommonAIAgentComponent* Agent = UCommonAIAgentComponent::FindAgent(OwnerComp.GetAIOwner());
    if (!Agent || !Agent->CompleteBehaviorTree(&OwnerComp, bSucceeded, Reason)) return EBTNodeResult::Failed;
    return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_CommonAICompleteActivity::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    return EBTNodeResult::Aborted;
}

