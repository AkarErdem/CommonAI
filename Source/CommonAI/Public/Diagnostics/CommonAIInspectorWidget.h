#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommonAIAgentComponent.h"
#include "CommonAIInspectorWidget.generated.h"

class UTextBlock;

UCLASS(Blueprintable)
class COMMONAI_API UCommonAIInspectorWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Common AI|Debug") void InspectActor(AActor* Actor);
    UFUNCTION(BlueprintPure, Category = "Common AI|Debug") FCommonAIAgentSnapshot GetInspection() const;
    UFUNCTION(BlueprintPure, Category = "Common AI|Debug") FString GetInspectionText() const;
    UFUNCTION(BlueprintCallable, Category = "Common AI|Debug") void ToggleScoringPause();
    UFUNCTION(BlueprintCallable, Category = "Common AI|Debug") void RescoreSelected();
    UFUNCTION(BlueprintCallable, Category = "Common AI|Debug") void ReleasePause();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common AI|Debug", meta = (ClampMin = "0.05", Units = "Seconds")) float RefreshIntervalSeconds = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI|Layout") FVector2D PanelSizePixels = FVector2D(650.0f, 650.0f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI|Layout", meta = (ClampMin = "0.0")) float PaddingPixels = 16.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI|Layout", meta = (ClampMin = "8")) int32 FontSize = 14;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common AI|Layout") FLinearColor BackgroundColor = FLinearColor(0.025f, 0.035f, 0.05f, 0.96f);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SummaryText;
    TWeakObjectPtr<UCommonAIAgentComponent> InspectedAgent;
    FGuid PauseHold;
    float RefreshElapsed = 0.0f;
};
