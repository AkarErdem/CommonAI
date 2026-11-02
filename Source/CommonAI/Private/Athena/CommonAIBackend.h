#pragma once
#include "CoreMinimal.h"
#include "AthenaAIComponent.h"
#include "AthenaAIDataManager.h"
#include "CommonAIBackend.generated.h"

class UCommonAIAgentComponent;
class UCommonAIProfile;
class UCommonAIWorkTask;

UCLASS(NotBlueprintable)
class UCommonAIDataManager : public UAthenaAIDataManager
{
    GENERATED_BODY()
protected:
    virtual bool CanReplaceCurrentTask(const UAthenaAITask* NewTask) override;
    virtual void CalculateTasksScore() override;
};

UCLASS(NotBlueprintable)
class UCommonAIBackend : public UAthenaAIComponent
{
    GENERATED_BODY()
public:
    UCommonAIBackend();
    bool Prepare(UCommonAIAgentComponent* Agent, const UCommonAIProfile* Profile);
    virtual void InitializeComponent() override;
    void Observe();
    UCommonAIWorkTask* GetRunningWork() const;
protected:
    virtual void CreateDataManager() override;
private:
    TWeakObjectPtr<UCommonAIAgentComponent> Agent;
};
