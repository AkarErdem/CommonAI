#pragma once
#include "CoreMinimal.h"
#include "Contracts/CommonAIInteraction.h"
#include "CommonAIExecution.generated.h"

class UCommonAIAgentComponent;
class APawn;
enum class ECommonAICompletionDisposition : uint8 { Commit, Deferred, Rejected };
DECLARE_MULTICAST_DELEGATE_TwoParams(FCommonAIExecutionFinished, FGuid, const FCommonAIInteractionResult&);

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIExecutionContext
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") TObjectPtr<UCommonAIAgentComponent> Agent;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FCommonAIInteractionRequest Request;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") TWeakObjectPtr<APawn> Pawn;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") TWeakObjectPtr<UObject> Target;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGameplayTag Action;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGuid Entry;
    UPROPERTY(BlueprintReadOnly, Category = "Common AI") FGuid Invocation;
};

// Definitions are duplicated for execution. Native entry points retain identity and terminal ownership.
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class COMMONAI_API UCommonAIExecution : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Execution Tick") bool bTickEnabled = false;
    virtual bool RequiresTick() const { return bTickEnabled; }
    FCommonAIExecutionFinished OnFinished;
    virtual UWorld* GetWorld() const override;
    bool BeginExecution(const FCommonAIExecutionContext& Context);
    void TickExecution(float DeltaSeconds);
    void CancelExecution(FGameplayTag Reason);
    void AbandonExecution(FGameplayTag Reason);
    void ReleaseContext();
    UFUNCTION(BlueprintCallable, Category = "Common AI|Execution")
    bool CompleteExecution(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason);
    UFUNCTION(BlueprintPure, Category = "Common AI|Execution") FCommonAIExecutionContext GetContext() const { return Context; }
    UFUNCTION(BlueprintPure, Category = "Common AI|Execution") FCommonAIInteractionResult GetResult() const { return Result; }
protected:
    virtual ECommonAICompletionDisposition PrepareCompletion(ECommonAIInteractionStatus Status, FGameplayTag Reason)
    { return ECommonAICompletionDisposition::Commit; }
    bool CommitCompletion(FGuid Invocation, ECommonAIInteractionStatus Status, FGameplayTag Reason);
    UFUNCTION(BlueprintNativeEvent, Category = "Common AI|Execution") void OnStarted(const FCommonAIExecutionContext& ExecutionContext);
    virtual void OnStarted_Implementation(const FCommonAIExecutionContext& ExecutionContext) {}
    UFUNCTION(BlueprintNativeEvent, Category = "Common AI|Execution") void OnTick(float DeltaSeconds);
    virtual void OnTick_Implementation(float DeltaSeconds) {}
    UFUNCTION(BlueprintNativeEvent, Category = "Common AI|Execution") void OnCancellationRequested(FGameplayTag Reason);
    virtual void OnCancellationRequested_Implementation(FGameplayTag Reason) {}
    UFUNCTION(BlueprintNativeEvent, Category = "Common AI|Execution") void OnAbandoned(FGameplayTag Reason);
    virtual void OnAbandoned_Implementation(FGameplayTag Reason) {}
private:
    UPROPERTY(Transient) FCommonAIExecutionContext Context;
    UPROPERTY(Transient) FCommonAIInteractionResult Result;
};

UCLASS(DisplayName = "Wait")
class COMMONAI_API UCommonAIWaitExecution : public UCommonAIExecution
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Wait", meta = (ClampMin = "0.0", Units = "Seconds")) float DurationSeconds = 0.0f;
    virtual bool RequiresTick() const override { return DurationSeconds > 0.0f; }
protected:
    virtual void OnStarted_Implementation(const FCommonAIExecutionContext& ExecutionContext) override;
    virtual void OnTick_Implementation(float DeltaSeconds) override;
    virtual void OnCancellationRequested_Implementation(FGameplayTag Reason) override;
private:
    float ElapsedSeconds = 0.0f;
};
