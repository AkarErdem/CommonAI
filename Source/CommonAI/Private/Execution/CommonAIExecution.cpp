#include "Execution/CommonAIExecution.h"
#include "CommonAIAgentComponent.h"

UWorld* UCommonAIExecution::GetWorld() const
{
    return IsValid(Context.Agent) ? Context.Agent->GetWorld() : nullptr;
}

bool UCommonAIExecution::BeginExecution(const FCommonAIExecutionContext& InContext)
{
    if (!Result.IsTerminal() || !InContext.Invocation.IsValid() || !IsValid(InContext.Agent)) return false;
    Context = InContext;
    Result = {};
    Result.Status = ECommonAIInteractionStatus::Running;
    OnStarted(Context);
    return true;
}

void UCommonAIExecution::TickExecution(float DeltaSeconds)
{
    if (!Result.IsTerminal() && RequiresTick()) OnTick(DeltaSeconds);
}

void UCommonAIExecution::CancelExecution(FGameplayTag Reason)
{
    if (Result.IsTerminal() || Result.Status == ECommonAIInteractionStatus::Cancelling) return;
    Result.Status = ECommonAIInteractionStatus::Cancelling;
    Result.Reason = Reason;
    OnCancellationRequested(Reason);
}

void UCommonAIExecution::AbandonExecution(FGameplayTag Reason)
{
    if (Result.IsTerminal()) return;
    Result.Status = ECommonAIInteractionStatus::Failed;
    Result.Reason = Reason;
    OnAbandoned(Reason);
    OnFinished.Broadcast(Context.Invocation, Result);
}

void UCommonAIExecution::ReleaseContext()
{
    if (Result.IsTerminal())
    {
        if (IsValid(Context.Agent)) Context.Agent->ReleaseOwnedState(this);
        Context = {};
    }
}

bool UCommonAIExecution::CompleteExecution(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason)
{
    if (Invocation != Context.Invocation || Result.IsTerminal()
        || (Status != ECommonAIInteractionStatus::Completed && Status != ECommonAIInteractionStatus::Failed
            && Status != ECommonAIInteractionStatus::Cancelled)) return false;
    const ECommonAICompletionDisposition Disposition = PrepareCompletion(Status, Reason);
    if (Disposition == ECommonAICompletionDisposition::Rejected) return false;
    if (Disposition == ECommonAICompletionDisposition::Deferred) return true;
    return CommitCompletion(Invocation, Status, Reason);
}

bool UCommonAIExecution::CommitCompletion(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason)
{
    if (Invocation != Context.Invocation || Result.IsTerminal()) return false;
    const bool bCancelled = Result.Status == ECommonAIInteractionStatus::Cancelling;
    Result.Status = bCancelled && Status == ECommonAIInteractionStatus::Completed ? ECommonAIInteractionStatus::Cancelled : Status;
    if (Reason.IsValid()) Result.Reason = Reason;
    OnFinished.Broadcast(Context.Invocation, Result);
    return true;
}

void UCommonAIWaitExecution::OnStarted_Implementation(const FCommonAIExecutionContext& ExecutionContext)
{
    ElapsedSeconds = 0.0f;
}

void UCommonAIWaitExecution::OnTick_Implementation(float DeltaSeconds)
{
    ElapsedSeconds += DeltaSeconds;
    if (DurationSeconds > 0.0f && ElapsedSeconds >= DurationSeconds)
        CompleteExecution(GetContext().Invocation, ECommonAIInteractionStatus::Completed, {});
}

void UCommonAIWaitExecution::OnCancellationRequested_Implementation(FGameplayTag Reason)
{
    CompleteExecution(GetContext().Invocation, ECommonAIInteractionStatus::Cancelled, Reason);
}
