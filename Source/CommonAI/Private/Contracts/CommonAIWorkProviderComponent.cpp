#include "Contracts/CommonAIWorkProviderComponent.h"

UObject* UCommonAIWorkProviderComponent::FindOpportunity(const FCommonAIInteractionRequest& Request,
    const FGameplayTagQuery& RequiredCapabilities, const TSet<UObject*>* Excluded) const
{
    const TArray<TWeakObjectPtr<UObject>>* Bound = ActivityTargets.Find(Request.Activity);
    // Queries may invoke authored callbacks; keep this traversal stable if a callback updates the bindings.
    const TArray<TWeakObjectPtr<UObject>> Candidates = Bound ? *Bound : Targets;
    for (const TWeakObjectPtr<UObject>& Entry : Candidates)
    {
        UObject* Target = Entry.Get();
        if (!IsValid(Target) || (Excluded && Excluded->Contains(Target))) continue;
        const FCommonAIInteractionOffer Offer = ICommonAIInteraction::Execute_QueryInteraction(Target, Request);
        if (IsValid(Target) && Offer.bAvailable && IsValid(Offer.ApproachActor)
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

void UCommonAIWorkProviderComponent::SetActivityTarget(FGameplayTag Activity, UObject* Target)
{
    SetActivityTargets(Activity, { Target });
}

void UCommonAIWorkProviderComponent::SetActivityTargets(FGameplayTag Activity, const TArray<UObject*>& OrderedTargets)
{
    if (!Activity.IsValid()) return;
    TArray<TWeakObjectPtr<UObject>>& Bound = ActivityTargets.FindOrAdd(Activity);
    Bound.Reset();
    for (UObject* Target : OrderedTargets)
        if (IsValid(Target) && Target->GetClass()->ImplementsInterface(UCommonAIInteraction::StaticClass())) Bound.AddUnique(Target);
}

void UCommonAIWorkProviderComponent::ClearActivityTarget(FGameplayTag Activity)
{
    ActivityTargets.Remove(Activity);
}

int32 UCommonAIWorkProviderComponent::GetCandidateCount(FGameplayTag Activity) const
{
    const TArray<TWeakObjectPtr<UObject>>* Bound = ActivityTargets.Find(Activity);
    return Bound ? Bound->Num() : Targets.Num();
}
