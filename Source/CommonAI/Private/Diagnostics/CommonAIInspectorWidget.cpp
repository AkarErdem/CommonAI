#include "Diagnostics/CommonAIInspectorWidget.h"
#include "CommonAITags.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

TSharedRef<SWidget> UCommonAIInspectorWidget::RebuildWidget()
{
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InspectorBackground"));
        Border->SetBrushColor(BackgroundColor);
        Border->SetPadding(FMargin(PaddingPixels));
        WidgetTree->RootWidget = Border;
        USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
        Size->SetWidthOverride(PanelSizePixels.X);
        Size->SetHeightOverride(PanelSizePixels.Y);
        Border->SetContent(Size);
        UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
        Size->SetContent(Content);
        UHorizontalBox* Controls = WidgetTree->ConstructWidget<UHorizontalBox>();
        Content->AddChildToVerticalBox(Controls);
        auto AddButton = [this, Controls](const TCHAR* Name, const TCHAR* Label)
        {
            UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
            UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
            Text->SetText(FText::FromString(Label));
            Button->SetContent(Text);
            Controls->AddChildToHorizontalBox(Button);
            return Button;
        };
        AddButton(TEXT("PauseButton"), TEXT("Pause / Resume scoring"))->OnClicked.AddDynamic(this, &UCommonAIInspectorWidget::ToggleScoringPause);
        AddButton(TEXT("RescoreButton"), TEXT("Rescore"))->OnClicked.AddDynamic(this, &UCommonAIInspectorWidget::RescoreSelected);
        UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
        Content->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        SummaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InspectionText"));
        FSlateFontInfo Font = SummaryText->GetFont();
        Font.Size = FontSize;
        SummaryText->SetFont(Font);
        SummaryText->SetAutoWrapText(true);
        SummaryText->SetText(FText::FromString(GetInspectionText()));
        Scroll->AddChild(SummaryText);
    }
    return Super::RebuildWidget();
}

void UCommonAIInspectorWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (SummaryText) SummaryText->SetText(FText::FromString(GetInspectionText()));
}

void UCommonAIInspectorWidget::InspectActor(AActor* Actor)
{
    ReleasePause();
    InspectedAgent = UCommonAIAgentComponent::FindAgent(Actor);
    if (SummaryText) SummaryText->SetText(FText::FromString(GetInspectionText()));
}

FCommonAIAgentSnapshot UCommonAIInspectorWidget::GetInspection() const
{
    return InspectedAgent.IsValid() ? InspectedAgent->GetSnapshot() : FCommonAIAgentSnapshot{};
}

FString UCommonAIInspectorWidget::GetInspectionText() const
{
    if (!InspectedAgent.IsValid()) return TEXT("CommonAI\nNo agent selected.");
    const FCommonAIAgentSnapshot State = GetInspection();
    FString Text = FString::Printf(TEXT("COMMON AI — %s\n%s | %s\nActivity: %s\nAction: %s\n"),
        *GetNameSafe(InspectedAgent->GetOwner()), State.bEnabled ? TEXT("Enabled") : TEXT("Disabled"),
        State.bScoringPaused ? TEXT("Scoring paused") : TEXT("Scoring active"), *State.Activity.ToString(), *State.CurrentAction.ToString());
    Text += TEXT("Profile: ") + State.ProfileId.ToString() + TEXT("\n");
    if (State.bProfileChangePending)
        Text += State.ProfileTransition == ECommonAITransitionMode::AfterNaturalCompletion
            ? TEXT("Profile change: waiting for natural completion (an idle fallback may wait indefinitely).\n")
            : TEXT("Profile change: waiting for safe cancellation.\n");
    if (State.Phase == ECommonAIPhase::Cancelling)
        Text += FString::Printf(TEXT("Waiting for safe cancellation: %.2f s%s\nReason: %s\n"), State.CancellationElapsedSeconds,
            State.bCancellationOverdue ? TEXT(" (overdue)") : TEXT(""), *State.CancellationReason.ToString());
    Text += FString::Printf(TEXT("Last result: %s — %s\n"), *StaticEnum<ECommonAIInteractionStatus>()->GetNameStringByValue(int64(State.LastResult.Status)), *State.LastResult.Reason.ToString());
    if (State.Command.Id.IsValid())
        Text += FString::Printf(TEXT("Command: %s — %s\n"), *State.Command.Activity.ToString(), *StaticEnum<ECommonAICommandStatus>()->GetNameStringByValue(int64(State.Command.Status)));
    Text += TEXT("\nTASKS");
    if (State.bScoresDirty) Text += TEXT(" (pending rescore)");
    Text += TEXT("\n");
    for (const FCommonAITaskSnapshot& Task : State.Tasks)
    {
        Text += FString::Printf(TEXT("%s%s  %.3f  priority %d%s%s\n  weight %.2f | resistance %.2f | momentum %d/%d\n"),
            Task.bActive ? TEXT("> ") : TEXT("  "), *Task.Activity.ToString(), Task.Score, Task.Priority,
            Task.bLocked ? TEXT(" | locked") : TEXT(""), Task.bFallback ? TEXT(" | fallback") : TEXT(""),
            Task.ScoreModifier, Task.ReplaceResistance, Task.Momentum, Task.MaxMomentum);
        if (Task.Reason.IsValid()) Text += TEXT("  ") + Task.Reason.ToString() + TEXT("\n");
        for (const FCommonAICurveSnapshot& Curve : Task.Curves)
            Text += FString::Printf(TEXT("  %s: %.3f -> %.3f\n"), *Curve.Consideration.ToString(), Curve.NormalizedInput, Curve.Contribution);
    }
    Text += TEXT("\nCONSIDERATIONS\n");
    for (const FCommonAIConsiderationSnapshot& Input : State.Considerations)
        Text += FString::Printf(TEXT("%s\n  raw %.3f | range %.3f..%.3f | normalized %.3f | %s\n"),
            *Input.Id.ToString(), Input.RawValue, Input.Minimum, Input.Maximum, Input.NormalizedValue,
            *StaticEnum<ECommonAIValueStatus>()->GetNameStringByValue(int64(Input.Status)));
    Text += TEXT("\nSERVICES\n");
    for (const FCommonAIServiceSnapshot& Service : State.Services)
    {
        Text += FString::Printf(TEXT("%s — %s (%.2f s)\n"), *Service.Id.ToString(), Service.bRunning ? TEXT("running") : TEXT("stopped"), Service.IntervalSeconds);
        for (const FCommonAIServiceConditionSnapshot& Condition : Service.Conditions)
            Text += FString::Printf(TEXT("  %s %s %.3f: %s\n"), *Condition.Definition.Consideration.ToString(),
                *StaticEnum<ECommonAIComparison>()->GetNameStringByValue(int64(Condition.Definition.Comparison)),
                Condition.Definition.NormalizedThreshold, Condition.bMet ? TEXT("met") : TEXT("not met"));
    }
    Text += TEXT("\nSTATE CONTRIBUTIONS\n");
    for (const FCommonAITagContribution& Contribution : InspectedAgent->GetTagContributions())
        if (Contribution.Source.IsValid()) Text += Contribution.Tags.ToStringSimple() + TEXT(" — ") + GetNameSafe(Contribution.Source.Get()) + TEXT("\n");
    return Text;
}

void UCommonAIInspectorWidget::ToggleScoringPause()
{
    if (!InspectedAgent.IsValid()) return;
    if (PauseHold.IsValid()) ReleasePause();
    else PauseHold = InspectedAgent->PauseScoring(this, CommonAITags::Reason_DebugPause);
    if (SummaryText) SummaryText->SetText(FText::FromString(GetInspectionText()));
}

void UCommonAIInspectorWidget::ReleasePause()
{
    if (InspectedAgent.IsValid() && PauseHold.IsValid()) InspectedAgent->ResumeScoring(this, PauseHold);
    PauseHold.Invalidate();
}

void UCommonAIInspectorWidget::RescoreSelected()
{
    if (InspectedAgent.IsValid()) InspectedAgent->Rescore();
}

void UCommonAIInspectorWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    RefreshElapsed += DeltaSeconds;
    if (SummaryText && RefreshElapsed >= RefreshIntervalSeconds)
    {
        RefreshElapsed = 0.0f;
        SummaryText->SetText(FText::FromString(GetInspectionText()));
    }
}

void UCommonAIInspectorWidget::NativeDestruct()
{
    ReleasePause();
    Super::NativeDestruct();
}
