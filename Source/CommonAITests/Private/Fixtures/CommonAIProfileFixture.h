#pragma once
#include "CommonAIProfile.h"
#include "CommonAITags.h"
#include "Engine/CurveTable.h"

namespace CommonAITestProfiles
{
    inline UCommonAIProfile* MakeWork(UObject* Outer, UCurveTable* SourceCurves = nullptr, FName SourceRow = NAME_None)
    {
        UCommonAIProfile* Profile = NewObject<UCommonAIProfile>(Outer);
        Profile->ProfileId = CommonAITags::Profile_Worker;
        if (!SourceCurves)
        {
            SourceCurves = NewObject<UCurveTable>(Profile);
            SourceRow = TEXT("Availability");
            FRichCurve& Curve = SourceCurves->AddRichCurve(SourceRow);
            Curve.AddKey(0.0f, 0.0f);
            Curve.AddKey(1.0f, 1.0f);
        }
        FCommonAIConsiderationDefinition Input;
        Input.Id = CommonAITags::Consideration_WorkAvailable;
        Input.Route = ECommonAIInputRoute::Opportunity;
        Input.OpportunityActivity = CommonAITags::Activity_Work;
        Input.bHasInitialValue = true;
        Profile->Considerations.Add(Input);
        FCommonAITaskDefinition Work;
        Work.Activity = CommonAITags::Activity_Work;
        Work.ActiveStateTags.AddTag(CommonAITags::State_Busy_Working);
        FCommonAICurveBinding Binding;
        Binding.Consideration = Input.Id;
        Binding.Curve.CurveTable = SourceCurves;
        Binding.Curve.RowName = SourceRow;
        Work.Considerations.Add(Binding);
        auto AddAction = [&Work](ECommonAIActionKind Kind, FGameplayTag Id)
        {
            FCommonAIActionDefinition& Action = Work.Actions.AddDefaulted_GetRef();
            Action.Kind = Kind;
            Action.Id = Id;
        };
        AddAction(ECommonAIActionKind::FindOpportunity, CommonAITags::Action_Find);
        AddAction(ECommonAIActionKind::Reserve, CommonAITags::Action_Reserve);
        AddAction(ECommonAIActionKind::Move, CommonAITags::Action_Move);
        AddAction(ECommonAIActionKind::Interact, CommonAITags::Action_Interact);
        AddAction(ECommonAIActionKind::Release, CommonAITags::Action_Release);
        Profile->Tasks.Add(Work);
        return Profile;
    }

    inline UCommonAIProfile* MakeSelection(UObject* Outer)
    {
        UCommonAIProfile* Profile = NewObject<UCommonAIProfile>(Outer);
        UCurveTable* Curves = NewObject<UCurveTable>(Profile);
        FRichCurve& Curve = Curves->AddRichCurve(TEXT("Linear"));
        Curve.AddKey(0.0f, 0.0f);
        Curve.AddKey(1.0f, 1.0f);
        for (const FGameplayTag Id : { CommonAITags::Consideration_WorkAvailable.GetTag(), CommonAITags::Consideration_DistanceTarget.GetTag() })
        {
            FCommonAIConsiderationDefinition Input;
            Input.Id = Id;
            Input.SourceId = CommonAITags::Source_External;
            Input.bHasInitialValue = true;
            Input.InitialValue = 0.5f;
            Profile->Considerations.Add(Input);
        }
        for (const FGameplayTag Id : { CommonAITags::Activity_Work_Farming.GetTag(), CommonAITags::Activity_Work_Crafting.GetTag() })
        {
            FCommonAITaskDefinition Task;
            Task.Activity = Id;
            Task.Kind = ECommonAITaskKind::Wait;
            Task.WaitDurationSeconds = 2.0f;
            Task.ReplaceResistance = 0.0f;
            for (const FCommonAIConsiderationDefinition& Input : Profile->Considerations)
            {
                if (Id == CommonAITags::Activity_Work_Crafting && Input.Id == CommonAITags::Consideration_DistanceTarget) continue;
                FCommonAICurveBinding Binding;
                Binding.Consideration = Input.Id;
                Binding.Curve.CurveTable = Curves;
                Binding.Curve.RowName = TEXT("Linear");
                Task.Considerations.Add(Binding);
            }
            Profile->Tasks.Add(Task);
        }
        return Profile;
    }
}

