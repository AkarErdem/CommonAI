#include "Evaluation/CommonAIEvaluator.h"
#include "GameFramework/Actor.h"

namespace
{
    FCommonAIEvaluationResult Value(float Raw)
    {
        FCommonAIEvaluationResult Result;
        if (FMath::IsFinite(Raw)) { Result.Status = ECommonAIValueStatus::Value; Result.RawValue = Raw; }
        return Result;
    }
}

FCommonAIEvaluationResult UCommonAINumericEvaluator::Evaluate_Implementation(const FInstancedStruct& Context) const
{
    if (const auto* Numeric = Context.GetPtr<FCommonAINumericContext>()) return Value(Numeric->Value);
    if (const auto* Event = Context.GetPtr<FCommonAIEventContext>()) return Value(Event->Magnitude);
    return {};
}

FCommonAIEvaluationResult UCommonAIDistanceEvaluator::Evaluate_Implementation(const FInstancedStruct& Context) const
{
    if (const auto* Actors = Context.GetPtr<FCommonAIActorContext>())
        if (IsValid(Actors->Source) && IsValid(Actors->Target))
            return Value(FVector::Distance(Actors->Source->GetActorLocation(), Actors->Target->GetActorLocation()));
    if (const auto* Locations = Context.GetPtr<FCommonAILocationContext>())
        return Value(FVector::Distance(Locations->Source, Locations->Target));
    return {};
}

FCommonAIEvaluationResult UCommonAITagQueryEvaluator::Evaluate_Implementation(const FInstancedStruct& Context) const
{
    if (const auto* Tags = Context.GetPtr<FCommonAITagContext>()) return Value(Query.IsEmpty() || Query.Matches(Tags->Tags) ? 1.0f : 0.0f);
    return {};
}
