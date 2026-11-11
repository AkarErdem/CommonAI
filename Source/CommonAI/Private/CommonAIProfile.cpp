#include "CommonAIProfile.h"
#include "CommonAITags.h"
#include "Engine/CurveTable.h"
#include "Evaluation/CommonAIEvaluator.h"
#include "Evaluation/CommonAIServiceBehavior.h"
#include "Execution/CommonAIExecution.h"

UCommonAIProfile::UCommonAIProfile()
{
    Fallback.Activity = CommonAITags::Activity_Wait;
    Fallback.Kind = ECommonAITaskKind::Wait;
    Fallback.ReplaceResistance = 0.0f;
}

bool UCommonAIProfile::IsUsable() const
{
    return ValidateProfile().IsEmpty();
}

const FCommonAITaskDefinition* UCommonAIProfile::FindTask(FGameplayTag Activity) const
{
    if (Fallback.Activity == Activity) return &Fallback;
    return Tasks.FindByPredicate([Activity](const FCommonAITaskDefinition& Task) { return Task.Activity == Activity; });
}

const FCommonAIConsiderationDefinition* UCommonAIProfile::FindConsideration(FGameplayTag Id) const
{
    return Considerations.FindByPredicate([Id](const FCommonAIConsiderationDefinition& Input) { return Input.Id == Id; });
}

TArray<FString> UCommonAIProfile::ValidateProfile() const
{
    TArray<FString> Errors;
    auto Error = [&Errors](FGameplayTag Id, const TCHAR* Message) { Errors.Add(FString::Printf(TEXT("%s: %s"), *Id.ToString(), Message)); };
    auto FiniteNonnegative = [](float Value) { return FMath::IsFinite(Value) && Value >= 0.0f; };
    const bool bNeedsObservation = ScoringMode == ECommonAIScoringMode::Interval
        || Considerations.ContainsByPredicate([](const FCommonAIConsiderationDefinition& Input) { return Input.Route == ECommonAIInputRoute::Opportunity; });
    if (!FMath::IsFinite(ObservationIntervalSeconds) || ObservationIntervalSeconds < 0.0f || (bNeedsObservation && ObservationIntervalSeconds == 0.0f))
        Errors.Add(TEXT("A positive observation interval is required for periodic scoring or opportunity observation."));
    if (!FiniteNonnegative(CancellationWarningSeconds)) Errors.Add(TEXT("Cancellation warning duration must be finite and nonnegative."));
    if (Tasks.IsEmpty()) Errors.Add(TEXT("At least one task is required."));
    TSet<FGameplayTag> InputIds;
    TSet<FGameplayTag> ServiceIds;
    for (const FCommonAIServiceDefinition& Service : Services)
    {
        if (!Service.Id.IsValid() || ServiceIds.Contains(Service.Id)) Error(Service.Id, TEXT("Invalid or duplicate service identity."));
        ServiceIds.Add(Service.Id);
        if (!FiniteNonnegative(Service.IntervalSeconds)) Error(Service.Id, TEXT("Invalid service interval."));
        if (Service.EvaluatedInputs.IsEmpty() && !Service.Behavior) Error(Service.Id, TEXT("Service has no evaluator inputs or behavior."));
    }
    for (const FCommonAIConsiderationDefinition& Input : Considerations)
    {
        if (!Input.Id.IsValid() || InputIds.Contains(Input.Id)) Error(Input.Id, TEXT("Invalid or duplicate consideration identity."));
        InputIds.Add(Input.Id);
        if (!FMath::IsFinite(Input.Minimum) || !FMath::IsFinite(Input.Maximum) || Input.Minimum >= Input.Maximum)
            Error(Input.Id, TEXT("Input range must be finite with minimum below maximum."));
        if (Input.bHasInitialValue && !FMath::IsFinite(Input.InitialValue)) Error(Input.Id, TEXT("Initial value must be finite."));
        if (Input.Route == ECommonAIInputRoute::Opportunity && !Input.OpportunityActivity.IsValid())
            Error(Input.Id, TEXT("Opportunity input requires an activity identity."));
        if (Input.Route != ECommonAIInputRoute::Opportunity && !Input.SourceId.IsValid())
            Error(Input.Id, TEXT("Input requires one authoritative source identity."));
        if (Input.Route == ECommonAIInputRoute::Evaluator && !Input.Evaluator)
            Error(Input.Id, TEXT("Evaluator input requires an evaluator."));
    }
    TSet<FGameplayTag> TaskIds;
    auto ValidateTask = [&](const FCommonAITaskDefinition& Task, bool bFallback)
    {
        if (!Task.Activity.IsValid() || TaskIds.Contains(Task.Activity)) Error(Task.Activity, TEXT("Invalid or duplicate task identity."));
        TaskIds.Add(Task.Activity);
        if (Task.Priority < 1 || Task.Priority > TNumericLimits<int8>::Max()) Error(Task.Activity, TEXT("Priority must fit the vendor's positive signed range."));
        if (!FiniteNonnegative(Task.ScoreModifier) || !FiniteNonnegative(Task.ReplaceResistance) || Task.ReplaceResistance > 1.0f
            || !FiniteNonnegative(Task.UnlockDelaySeconds) || !FiniteNonnegative(Task.MomentumDecaySeconds)
            || !FiniteNonnegative(Task.WaitDurationSeconds) || !FMath::IsFinite(Task.TickIntervalSeconds) || Task.TickIntervalSeconds <= 0.0f)
            Error(Task.Activity, TEXT("Task policy values are invalid."));
        if (Task.MaxMomentum < 0 || Task.MaxMomentum > 50 || Task.MomentumReductionPercent < 0 || Task.MomentumReductionPercent > 100)
            Error(Task.Activity, TEXT("Momentum settings are outside supported ranges."));
        if (bFallback && !Task.bUtilityInterruptible) Error(Task.Activity, TEXT("Fallback must permit utility replacement."));
        if (!bFallback && !Task.bCommandOnly && Task.Considerations.IsEmpty()) Error(Task.Activity, TEXT("Scored task requires consideration bindings."));
        if (Task.Considerations.Num() > TNumericLimits<uint8>::Max()) Error(Task.Activity, TEXT("Consideration count exceeds the vendor task limit."));
        TSet<FGameplayTag> BoundInputs;
        for (const FCommonAICurveBinding& Binding : Task.Considerations)
        {
            if (!InputIds.Contains(Binding.Consideration) || BoundInputs.Contains(Binding.Consideration))
                Error(Task.Activity, TEXT("Unknown or duplicate consideration binding."));
            BoundInputs.Add(Binding.Consideration);
            if (!Binding.Curve.CurveTable || Binding.Curve.RowName.IsNone() || !Binding.Curve.CurveTable->FindCurveUnchecked(Binding.Curve.RowName))
                Error(Task.Activity, TEXT("Missing source curve table/row."));
        }
        if (Task.Kind == ECommonAITaskKind::Sequence && Task.Actions.IsEmpty()) Error(Task.Activity, TEXT("Sequence requires at least one action."));
        if (Task.Kind == ECommonAITaskKind::Custom && !Task.Execution) Error(Task.Activity, TEXT("Custom task requires an execution definition."));
        if (Task.Kind == ECommonAITaskKind::BehaviorTree && !Task.BehaviorTree) Error(Task.Activity, TEXT("Behavior Tree task requires a tree."));
        TSet<FGuid> Entries;
        bool bTargetSelected = false;
        bool bReservationOwned = false;
        for (const FCommonAIActionDefinition& Action : Task.Actions)
        {
            if (!Action.Id.IsValid() || !Action.EntryId.IsValid() || Entries.Contains(Action.EntryId))
                Error(Task.Activity, TEXT("Actions require valid tags and unique occurrence identities."));
            Entries.Add(Action.EntryId);
            if (!FiniteNonnegative(Action.DurationSeconds) || !FiniteNonnegative(Action.AcceptanceRadiusCentimeters))
                Error(Action.Id, TEXT("Action timing or movement radius is invalid."));
            if (!FMath::IsFinite(Action.TickIntervalSeconds) || Action.TickIntervalSeconds <= 0.0f)
                Error(Action.Id, TEXT("Action tick interval must be finite and positive."));
            if (Action.Kind == ECommonAIActionKind::Custom && !Action.Execution) Error(Action.Id, TEXT("Custom action requires an execution definition."));
            if (Action.Kind == ECommonAIActionKind::ResolveBinding && !Action.BindingId.IsValid()) Error(Action.Id, TEXT("Binding action requires a binding identity."));
            if (Action.Kind == ECommonAIActionKind::FindOpportunity || Action.Kind == ECommonAIActionKind::ResolveBinding)
            {
                if (bReservationOwned) Error(Action.Id, TEXT("Release the current reservation before selecting another target."));
                bTargetSelected = true;
            }
            else if (Action.Kind == ECommonAIActionKind::Reserve)
            {
                if (!bTargetSelected || bReservationOwned) Error(Action.Id, TEXT("Reserve requires a selected target and no outstanding reservation."));
                bReservationOwned = true;
            }
            else if (Action.Kind == ECommonAIActionKind::Move && !bTargetSelected)
                Error(Action.Id, TEXT("Move requires a selected target."));
            else if (Action.Kind == ECommonAIActionKind::Interact && !bReservationOwned)
                Error(Action.Id, TEXT("Interaction requires a reservation."));
            else if (Action.Kind == ECommonAIActionKind::Release)
            {
                if (!bReservationOwned) Error(Action.Id, TEXT("Release requires a reservation."));
                bReservationOwned = false;
            }
        }
        if (Task.Kind == ECommonAITaskKind::Sequence && bReservationOwned) Error(Task.Activity, TEXT("Sequence must release its final reservation."));
    };
    for (const FCommonAITaskDefinition& Task : Tasks) ValidateTask(Task, false);
    ValidateTask(Fallback, true);
    TSet<FGameplayTag> Evaluated;
    for (const FCommonAIServiceDefinition& Service : Services)
    {
        TSet<FGameplayTag> Conditions;
        for (const FCommonAIServiceCondition& Condition : Service.Conditions)
        {
            if (Conditions.Contains(Condition.Consideration)) Error(Service.Id, TEXT("A service may define only one comparison per input; use a derived range evaluator for compound constraints."));
            Conditions.Add(Condition.Consideration);
            if (!InputIds.Contains(Condition.Consideration) || !FiniteNonnegative(Condition.NormalizedThreshold) || Condition.NormalizedThreshold > 1.0f)
                Error(Service.Id, TEXT("Unknown condition input or invalid normalized threshold."));
        }
        for (const FGameplayTag InputId : Service.EvaluatedInputs)
        {
            const FCommonAIConsiderationDefinition* Input = FindConsideration(InputId);
            if (!Input || Input->Route != ECommonAIInputRoute::Evaluator || Evaluated.Contains(InputId))
                Error(Service.Id, TEXT("Evaluated input must designate one evaluator service."));
            Evaluated.Add(InputId);
        }
    }
    return Errors;
}

