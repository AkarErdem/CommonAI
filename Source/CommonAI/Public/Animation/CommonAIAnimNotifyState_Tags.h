#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "CommonAIAnimNotifyState_Tags.generated.h"

// Native branching points supply the montage playback identity even during interruption.
UCLASS(meta = (DisplayName = "CommonAI Tag Window"))
class COMMONAI_API UCommonAIAnimNotifyState_Tags : public UAnimNotifyState
{
    GENERATED_BODY()
public:
    UCommonAIAnimNotifyState_Tags();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI") FGameplayTagContainer Tags;
    virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& Payload) override;
    virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& Payload) override;
#if WITH_EDITOR
    virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
#endif
};
