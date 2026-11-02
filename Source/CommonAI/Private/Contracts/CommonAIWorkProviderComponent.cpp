#include "Contracts/CommonAIWorkProviderComponent.h"

UObject* UCommonAIWorkProviderComponent::FindOpportunity(const FCommonAIInteractionRequest& Request, const FGameplayTagQuery& RequiredCapabilities) const
{
    for (const TWeakObjectPtr<UObject>& Entry : Targets)
    {
        UObject* Target = Entry.Get();
        if (!IsValid(Target)) continue;
        const FCommonAIInteractionOffer Offer = ICommonAIInteraction::Execute_QueryInteraction(Target, Request);
        if (Offer.bAvailable && IsValid(Offer.ApproachActor)
            && (RequiredCapabilities.IsEmpty() || RequiredCapabilities.Matches(Offer.Capabilities))) return Target;
    }
    return nullptr;
}

bool UCommonAIWorkProviderComponent::AddTarget(UObject* Target)
{
    if (!IsValid(Target) || !Target->GetClass()->ImplementsInterface(UCommonAIInteraction::StaticClass())) return false;
    Targets.AddUnique(Target);
    return true;
}

void UCommonAIWorkProviderComponent::RemoveTarget(UObject* Target)
{
    Targets.Remove(Target);
}
