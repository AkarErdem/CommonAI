#pragma once
#include "CoreMinimal.h"
#include "Contracts/CommonAIWorkSite.h"
#include "CommonAIFoundationObserver.generated.h"

// Test-owned observer of production interaction events, including synchronous reentry.
UCLASS()
class UCommonAIFoundationObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UCommonAIWorkSite> Target;
    UPROPERTY() FCommonAIInteractionRequest Replacement;
    UPROPERTY() FCommonAIInteractionRequest LastAbandoned;
    int32 AbandonmentCount = 0;
    bool bReserveOnAbandonment = false;
    UFUNCTION() void OnAbandoned(FCommonAIInteractionRequest Request)
    {
        ++AbandonmentCount;
        LastAbandoned = Request;
        if (bReserveOnAbandonment)
        {
            ICommonAIInteraction::Execute_ReleaseInteraction(Target, Request, false);
            ICommonAIInteraction::Execute_ReserveInteraction(Target, Replacement);
        }
    }
};
