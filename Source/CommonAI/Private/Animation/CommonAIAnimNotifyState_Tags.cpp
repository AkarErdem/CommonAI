#include "Animation/CommonAIAnimNotifyState_Tags.h"
#include "CommonAIAgentComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"

UCommonAIAnimNotifyState_Tags::UCommonAIAnimNotifyState_Tags()
{
    bIsNativeBranchingPoint = true;
#if WITH_EDITORONLY_DATA
    bShouldFireInEditor = false;
#endif
}

void UCommonAIAnimNotifyState_Tags::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& Payload)
{
    if (Payload.SkelMeshComponent)
        if (UCommonAIAgentComponent* Agent = UCommonAIAgentComponent::FindAgent(Payload.SkelMeshComponent->GetOwner()))
            Agent->BeginMontageTagWindow(Payload.SkelMeshComponent, this, Payload.MontageInstanceID, Tags);
}

void UCommonAIAnimNotifyState_Tags::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& Payload)
{
    if (Payload.SkelMeshComponent)
        if (UCommonAIAgentComponent* Agent = UCommonAIAgentComponent::FindAgent(Payload.SkelMeshComponent->GetOwner()))
            Agent->EndMontageTagWindow(Payload.SkelMeshComponent, this, Payload.MontageInstanceID);
}

#if WITH_EDITOR
bool UCommonAIAnimNotifyState_Tags::CanBePlaced(UAnimSequenceBase* Animation) const
{
    return Animation && Animation->IsA<UAnimMontage>();
}
#endif
