#include "CommonAIProfile.h"

#if WITH_EDITOR
#include "Engine/CurveTable.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "CommonAITags.h"
#include "Evaluation/CommonAIEvaluator.h"

void UCommonAIProfile::RefreshValidation()
{
    ValidationMessages = ValidateProfile();
}

void UCommonAIProfile::PostLoad()
{
    Super::PostLoad();
    RefreshValidation();
}

void UCommonAIProfile::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    auto AssignOccurrenceIds = [](FCommonAITaskDefinition& Task)
    {
        TSet<FGuid> Entries;
        for (FCommonAIActionDefinition& Action : Task.Actions)
        {
            if (!Action.EntryId.IsValid() || Entries.Contains(Action.EntryId)) Action.EntryId = FGuid::NewGuid();
            Entries.Add(Action.EntryId);
        }
    };
    for (FCommonAITaskDefinition& Task : Tasks) AssignOccurrenceIds(Task);
    AssignOccurrenceIds(Fallback);
    Super::PostEditChangeProperty(Event);
    RefreshValidation();
}

void UCommonAIProfile::GenerateMissingCurves()
{
    ValidationMessages.Reset();
    if (!AuthoringCurves)
    {
        ValidationMessages.Add(TEXT("Select an authoring Curve Table. Existing referenced tables remain read-only."));
        return;
    }
    const FName Mount = FPackageName::GetPackageMountPoint(AuthoringCurves->GetOutermost()->GetName());
    if (Mount != FName(TEXT("CommonAI")) && Mount != FName(TEXT("Game")))
    {
        ValidationMessages.Add(TEXT("Curve generation is restricted to shared or consumer-owned content; vendor and engine tables are read-only."));
        return;
    }
    // Unreal's CallInEditor button provides the surrounding transaction. Register both edited objects.
    Modify();
    AuthoringCurves->Modify();
    bool bChanged = false;
    for (FCommonAITaskDefinition& Task : Tasks)
    {
        for (FCommonAICurveBinding& Binding : Task.Considerations)
        {
            if (!Task.Activity.IsValid() || !Binding.Consideration.IsValid()) continue;
            if (!Binding.Curve.CurveTable) Binding.Curve.CurveTable = AuthoringCurves.Get();
            if (Binding.Curve.CurveTable != AuthoringCurves.Get()) continue;
            if (Binding.Curve.RowName.IsNone())
                Binding.Curve.RowName = FName(*FString::Printf(TEXT("%s - %s"), *Task.Activity.ToString(), *Binding.Consideration.ToString()));
            if (AuthoringCurves->FindCurveUnchecked(Binding.Curve.RowName)) continue;
            if (AuthoringCurves->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
            {
                FSimpleCurve& Curve = AuthoringCurves->AddSimpleCurve(Binding.Curve.RowName);
                Curve.AddKey(0.0f, 0.0f);
                Curve.AddKey(1.0f, 1.0f);
            }
            else
            {
                FRichCurve& Curve = AuthoringCurves->AddRichCurve(Binding.Curve.RowName);
                Curve.AddKey(0.0f, 0.0f);
                Curve.AddKey(1.0f, 1.0f);
            }
            bChanged = true;
        }
    }
    if (bChanged) AuthoringCurves->MarkPackageDirty();
    MarkPackageDirty();
    RefreshValidation();
}

void UCommonAIProfile::CreateGeneralExample()
{
    if (!Tasks.IsEmpty() || !Considerations.IsEmpty() || !Services.IsEmpty() || !AuthoringCurves)
    {
        ValidationMessages = { TEXT("Use an empty profile with an assigned authoring table. Existing definitions are never overwritten.") };
        return;
    }
    const FName Mount = FPackageName::GetPackageMountPoint(AuthoringCurves->GetOutermost()->GetName());
    if (Mount != FName(TEXT("CommonAI")) && Mount != FName(TEXT("Game")))
    {
        ValidationMessages = { TEXT("The example requires a shared or consumer-owned authoring table.") };
        return;
    }
    Modify();
    AuthoringCurves->Modify();
    auto AddCurve = [this](FName Row, float StartValue, float EndValue)
    {
        if (AuthoringCurves->FindCurveUnchecked(Row)) return;
        if (AuthoringCurves->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
        {
            FSimpleCurve& Curve = AuthoringCurves->AddSimpleCurve(Row);
            Curve.AddKey(0.0f, StartValue); Curve.AddKey(1.0f, EndValue);
        }
        else
        {
            FRichCurve& Curve = AuthoringCurves->AddRichCurve(Row);
            Curve.AddKey(0.0f, StartValue); Curve.AddKey(1.0f, EndValue);
        }
    };
    const FName LinearRow(TEXT("Linear"));
    const FName DistanceRow(TEXT("DistancePreference"));
    AddCurve(LinearRow, 0.0f, 1.0f);
    AddCurve(DistanceRow, 1.0f, 0.0f);
    if (!ProfileId.IsValid()) ProfileId = CommonAITags::Profile_GeneralWorker;
    if (DisplayName.IsEmpty()) DisplayName = FText::FromString(TEXT("General worker"));
    FCommonAIConsiderationDefinition Preference;
    Preference.Id = CommonAITags::Consideration_WorkPreference;
    Preference.Description = FText::FromString(TEXT("Authoritative work preference, normalized from zero to one."));
    Preference.SourceId = CommonAITags::Source_External;
    Preference.bHasInitialValue = true;
    Preference.InitialValue = 0.7f;
    Considerations.Add(Preference);
    FCommonAIConsiderationDefinition Distance;
    Distance.Id = CommonAITags::Consideration_DistanceTarget;
    Distance.Description = FText::FromString(TEXT("Distance supplied by the registered observation provider."));
    Distance.Units = TEXT("cm");
    Distance.Route = ECommonAIInputRoute::Evaluator;
    Distance.SourceId = CommonAITags::Source_External;
    Distance.Maximum = 1000.0f;
    Distance.Evaluator = NewObject<UCommonAIDistanceEvaluator>(this, TEXT("DistanceEvaluator"));
    Considerations.Add(Distance);
    auto Binding = [this](FGameplayTag Id, FName Row)
    {
        FCommonAICurveBinding Result;
        Result.Consideration = Id;
        Result.Curve.CurveTable = AuthoringCurves.Get();
        Result.Curve.RowName = Row;
        return Result;
    };
    FCommonAITaskDefinition Work;
    Work.Activity = CommonAITags::Activity_Work_Farming;
    Work.Label = FText::FromString(TEXT("Station work"));
    Work.ActiveStateTags.AddTag(CommonAITags::State_Busy_Working);
    Work.Considerations = { Binding(Preference.Id, LinearRow), Binding(Distance.Id, DistanceRow) };
    auto Action = [&Work](ECommonAIActionKind Kind, FGameplayTag Id, const TCHAR* Label)
    {
        FCommonAIActionDefinition& Entry = Work.Actions.AddDefaulted_GetRef();
        Entry.Kind = Kind;
        Entry.Id = Id;
        Entry.Label = FText::FromString(Label);
        Entry.BindingId = CommonAITags::Binding_Interaction;
    };
    Action(ECommonAIActionKind::ResolveBinding, CommonAITags::Action_ResolveBinding, TEXT("Resolve station"));
    Action(ECommonAIActionKind::Reserve, CommonAITags::Action_Reserve, TEXT("Reserve"));
    Action(ECommonAIActionKind::Interact, CommonAITags::Action_Interact, TEXT("Interact"));
    Action(ECommonAIActionKind::Release, CommonAITags::Action_Release, TEXT("Release"));
    Tasks.Add(Work);
    FCommonAITaskDefinition Standby;
    Standby.Activity = CommonAITags::Activity_Standby;
    Standby.Label = FText::FromString(TEXT("Standby"));
    Standby.Kind = ECommonAITaskKind::Wait;
    Standby.ReplaceResistance = 0.0f;
    Standby.Considerations.Add(Binding(Preference.Id, LinearRow));
    Tasks.Add(Standby);
    FCommonAIServiceDefinition Observe;
    Observe.Id = CommonAITags::Service_Observe;
    Observe.EvaluatedInputs.AddTag(Distance.Id);
    Services.Add(Observe);
    AuthoringCurves->MarkPackageDirty();
    MarkPackageDirty();
    RefreshValidation();
}
#endif
