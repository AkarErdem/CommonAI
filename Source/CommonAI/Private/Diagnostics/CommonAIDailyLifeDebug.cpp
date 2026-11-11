#include "Diagnostics/CommonAIDailyLifeDebug.h"
#include "DailyLife/CommonAIDailyLifeComponent.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
#if !UE_BUILD_SHIPPING
    constexpr float LabelHeightCentimeters = 210.0f;
    constexpr double RefreshSeconds = 0.25;
    constexpr float DefaultRangeCentimeters = 5000.0f;
    constexpr int32 DefaultLabelLimit = 12;
    constexpr float LabelGapPixels = 8.0f;
    FDelegateHandle DrawHandle;
    IConsoleVariable* Enabled = nullptr;
    IConsoleVariable* Range = nullptr;
    IConsoleVariable* Limit = nullptr;
    struct FCachedLabel { FString Text; double Updated = -1.0; };
    TMap<TWeakObjectPtr<UCommonAIDailyLifeComponent>, FCachedLabel> Labels;

    void Draw(UCanvas* Canvas, APlayerController* Controller)
    {
        if (!Canvas || !GEngine || !Enabled || !Enabled->GetBool()) return;
        TRACE_CPUPROFILER_EVENT_SCOPE(CommonAI_DailyLifeDebugDraw);
        UWorld* World = Controller ? Controller->GetWorld() : GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
        if (!World) return;
        if (!Controller) Controller = World->GetFirstPlayerController();
        if (!Controller) return;
        FVector ViewLocation;
        FRotator ViewRotation;
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
        const float RangeSquared = FMath::Square(FMath::Max(0.0f, Range->GetFloat()));
        TArray<UCommonAIDailyLifeComponent*> Visible;
        for (TObjectIterator<UCommonAIDailyLifeComponent> It; It; ++It)
        {
            if (It->GetWorld() == World && It->IsRoutineInitialized() && IsValid(It->GetOwner())
                && FVector::DistSquared(ViewLocation, It->GetOwner()->GetActorLocation()) <= RangeSquared) Visible.Add(*It);
        }
        Visible.Sort([&](const UCommonAIDailyLifeComponent& Left, const UCommonAIDailyLifeComponent& Right)
        {
            return FVector::DistSquared(ViewLocation, Left.GetOwner()->GetActorLocation()) < FVector::DistSquared(ViewLocation, Right.GetOwner()->GetActorLocation());
        });
        for (auto It = Labels.CreateIterator(); It; ++It) if (!It.Key().IsValid() || !It.Key()->IsRoutineInitialized()) It.RemoveCurrent();
        TArray<FBox2D> Occupied;
        const double Now = World->GetRealTimeSeconds();
        const int32 Maximum = FMath::Max(0, Limit->GetInt());
        for (UCommonAIDailyLifeComponent* Routine : Visible)
        {
            if (Occupied.Num() >= Maximum) break;
            const FVector Position = Routine->GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, LabelHeightCentimeters);
            if (FVector::DotProduct(Position - ViewLocation, ViewRotation.Vector()) <= 0.0) continue;
            const FVector Screen = Canvas->Project(Position);
            if (Screen.X < 0.0 || Screen.Y < 0.0 || Screen.X >= Canvas->SizeX || Screen.Y >= Canvas->SizeY) continue;
            FCachedLabel& Label = Labels.FindOrAdd(Routine);
            if (Label.Updated < 0.0 || Now < Label.Updated || Now - Label.Updated >= RefreshSeconds)
            {
                Label.Text = Routine->GetDebugText();
                Label.Updated = Now;
            }
            float Width = 0.0f, Height = 0.0f;
            Canvas->StrLen(GEngine->GetSmallFont(), Label.Text, Width, Height);
            const FVector2D Origin(FMath::Clamp(Screen.X - Width * 0.5, 0.0, FMath::Max(0.0, Canvas->SizeX - static_cast<double>(Width))),
                FMath::Clamp(Screen.Y - Height, 0.0, FMath::Max(0.0, Canvas->SizeY - static_cast<double>(Height))));
            const FBox2D Bounds(Origin, Origin + FVector2D(Width + LabelGapPixels, Height + LabelGapPixels));
            if (Occupied.ContainsByPredicate([&](const FBox2D& Other) { return Bounds.Intersect(Other); })) continue;
            Occupied.Add(Bounds);
            Canvas->K2_DrawText(GEngine->GetSmallFont(), Label.Text, Origin, FVector2D::UnitVector,
                FLinearColor::White, 0.0f, FLinearColor::Black, FVector2D::UnitVector, false, false, true);
        }
    }

    void Changed(IConsoleVariable* Variable)
    {
        if (Variable->GetBool() && !DrawHandle.IsValid()) DrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateStatic(&Draw));
        if (!Variable->GetBool())
        {
            if (DrawHandle.IsValid()) UDebugDrawService::Unregister(DrawHandle);
            DrawHandle.Reset();
            Labels.Reset();
        }
    }
#endif
}

void CommonAIDailyLifeDebug::Startup()
{
#if !UE_BUILD_SHIPPING
    Enabled = IConsoleManager::Get().RegisterConsoleVariable(TEXT("CommonAI.Debug.All"), 0,
        TEXT("Show daily-life labels above nearby agents. 1 enables; 0 removes rendering and cached labels."), ECVF_Cheat);
    Range = IConsoleManager::Get().RegisterConsoleVariable(TEXT("CommonAI.Debug.Distance"), DefaultRangeCentimeters,
        TEXT("Maximum daily-life label distance in centimeters."), ECVF_Cheat);
    Limit = IConsoleManager::Get().RegisterConsoleVariable(TEXT("CommonAI.Debug.MaxLabels"), DefaultLabelLimit,
        TEXT("Maximum non-overlapping daily-life labels in a viewport."), ECVF_Cheat);
    Enabled->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&Changed));
#endif
}

void CommonAIDailyLifeDebug::Shutdown()
{
#if !UE_BUILD_SHIPPING
    if (DrawHandle.IsValid()) UDebugDrawService::Unregister(DrawHandle);
    DrawHandle.Reset();
    Labels.Reset();
    IConsoleManager::Get().UnregisterConsoleObject(TEXT("CommonAI.Debug.All"), false);
    IConsoleManager::Get().UnregisterConsoleObject(TEXT("CommonAI.Debug.Distance"), false);
    IConsoleManager::Get().UnregisterConsoleObject(TEXT("CommonAI.Debug.MaxLabels"), false);
    Enabled = Range = Limit = nullptr;
#endif
}
