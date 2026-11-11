#include "Diagnostics/CommonAIDebugComponent.h"
#include "Diagnostics/CommonAIInspectorWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "AIController.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

UCommonAIDebugComponent::UCommonAIDebugComponent()
{
    WidgetClass = UCommonAIInspectorWidget::StaticClass();
}

APlayerController* UCommonAIDebugComponent::PlayerController() const
{
    if (APlayerController* Controller = Cast<APlayerController>(GetOwner())) return Controller;
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

bool UCommonAIDebugComponent::EnsureWidget()
{
    if (Widget) return true;
    if (!WidgetClass) return false;
    if (bWorldPanel)
    {
        AActor* Anchor = GetOwner();
        if (const AAIController* Controller = Cast<AAIController>(Anchor)) Anchor = Controller->GetPawn();
        if (!Anchor || !Anchor->GetRootComponent()) return false;
        WorldWidget = NewObject<UWidgetComponent>(Anchor);
        WorldWidget->SetWidgetClass(WidgetClass);
        WorldWidget->SetWidgetSpace(EWidgetSpace::Screen);
        WorldWidget->SetDrawSize(WidgetClass->GetDefaultObject<UCommonAIInspectorWidget>()->PanelSizePixels);
        WorldWidget->SetupAttachment(Anchor->GetRootComponent());
        WorldWidget->SetRelativeLocation(WorldPanelOffset);
        WorldWidget->RegisterComponent();
        WorldWidget->InitWidget();
        Widget = Cast<UCommonAIInspectorWidget>(WorldWidget->GetUserWidgetObject());
    }
    else if (APlayerController* Controller = PlayerController())
    {
        Widget = CreateWidget<UCommonAIInspectorWidget>(Controller, WidgetClass);
        if (Widget)
        {
            Widget->AddToViewport();
            Widget->SetDesiredSizeInViewport(Widget->PanelSizePixels);
            Widget->SetPositionInViewport(FVector2D(Widget->PaddingPixels));
        }
    }
    return Widget != nullptr;
}

bool UCommonAIDebugComponent::InspectActor(AActor* Actor)
{
    if (!UCommonAIAgentComponent::FindAgent(Actor) || !EnsureWidget()) return false;
    Widget->InspectActor(Actor);
    SetPanelVisible(true);
    return true;
}

bool UCommonAIDebugComponent::InspectFromView()
{
    APlayerController* Controller = PlayerController();
    if (!Controller || !GetWorld()) return false;
    FVector Location;
    FRotator Rotation;
    Controller->GetPlayerViewPoint(Location, Rotation);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CommonAIDebugAim), false);
    Params.AddIgnoredActor(Controller->GetPawn());
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByObjectType(Hits, Location, Location + Rotation.Vector() * AimRangeCentimeters,
        FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(AimRadiusCentimeters), Params);
    for (const FHitResult& Hit : Hits)
    {
        AActor* Actor = Hit.GetActor();
        if (!UCommonAIAgentComponent::FindAgent(Actor)) continue;
        FHitResult Occlusion;
        if (GetWorld()->LineTraceSingleByChannel(Occlusion, Location, Actor->GetActorLocation(), ECC_Visibility, Params)
            && Occlusion.GetActor() != Actor) continue;
        return InspectActor(Actor);
    }
    return false;
}

void UCommonAIDebugComponent::SetPanelVisible(bool bVisible)
{
    if (Widget) Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (WorldWidget) WorldWidget->SetVisibility(bVisible);
}

void UCommonAIDebugComponent::SetDebugEnabled(bool bEnabled)
{
    if (bEnabled) { EnsureWidget(); SetPanelVisible(true); }
    else
    {
        if (Widget) { Widget->ReleasePause(); if (!WorldWidget) Widget->RemoveFromParent(); }
        if (IsValid(WorldWidget)) WorldWidget->DestroyComponent();
        Widget = nullptr;
        WorldWidget = nullptr;
    }
}

void UCommonAIDebugComponent::SetAllWorldPanelsVisible(bool bVisible)
{
    for (TObjectIterator<UCommonAIDebugComponent> It; It; ++It)
        if (It->GetWorld() == GetWorld() && It->bWorldPanel) It->SetPanelVisible(bVisible);
}

void UCommonAIDebugComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bWorldPanel)
        if (AController* Controller = Cast<AController>(GetOwner()))
            Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &UCommonAIDebugComponent::HandlePawnChanged);
    if (bShowOnBeginPlay) InspectActor(InitialTarget ? InitialTarget.Get() : GetOwner());
}

void UCommonAIDebugComponent::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    const bool bWasVisible = Widget && Widget->GetVisibility() != ESlateVisibility::Collapsed;
    SetDebugEnabled(false);
    if (NewPawn && bWasVisible) InspectActor(NewPawn);
}

void UCommonAIDebugComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AController* Controller = Cast<AController>(GetOwner()))
        Controller->OnPossessedPawnChanged.RemoveDynamic(this, &UCommonAIDebugComponent::HandlePawnChanged);
    SetDebugEnabled(false);
    Super::EndPlay(EndPlayReason);
}
