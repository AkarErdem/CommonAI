#include "Contracts/CommonAIWorkProviderComponent.h"

bool UCommonAIWorkProviderComponent::FindOpportunity(const FCommonAIExecutionContext& Context, FCommonAIWorkOpportunity& OutOpportunity) const
{
	OutOpportunity.Site = nullptr;
	if (!Context.Agent.IsValid() || Context.RunId == 0)
	{
		return false;
	}
	for (UCommonAIWorkSite* Site : Sites)
	{
		if (IsValid(Site) && Site->IsAvailable())
		{
			OutOpportunity.Site = Site;
			return true;
		}
	}
	return false;
}

void UCommonAIWorkProviderComponent::AddSite(UCommonAIWorkSite* Site)
{
	if (IsValid(Site))
	{
		Sites.AddUnique(Site);
	}
}

void UCommonAIWorkProviderComponent::RemoveSite(UCommonAIWorkSite* Site)
{
	Sites.Remove(Site);
}
