#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CommonAIDebugComponent.generated.h"

class UCommonAIInspectorWidget;
class UWidgetComponent;
class APlayerController;
class APawn;

// Optional debug presentation only. Production decisions do not depend on this component.
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class COMMONAI_API UCommonAIDebugComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCommonAIDebugComponent();
    UPROPERTY(EditAnywhere, Category = "Common AI|Debug") TSubclassOf<UCommonAIInspectorWidget> WidgetClass;
    UPROPERTY(EditAnywhere, Category = "Common AI|Debug") bool bWorldPanel = false;
    UPROPERTY(EditAnywhere, Category = "Common AI|Debug") bool bShowOnBeginPlay = false;
    UPROPERTY(EditInstanceOnly, Category = "Common AI|Debug") TObjectPtr<AActor> InitialTarget;
    UPROPERTY(EditAnywhere, Category = "Common AI|Debug", meta = (Units = "Centimeters")) FVector WorldPanelOffset = FVector(0.0f, 0.0f, 200.0f);
    UPROPERTY(EditAnywhere, Category = "Common AI|Debug", meta = (ClampMin = "1.0", Units = "Centimeters")) float AimRangeCentimeters = 10000.0f;
    UPROPERTY(EditAnywhere, Category = "Common AI|Debug", meta = (ClampMin = "0.0", Units = "Centimeters")) float AimRadiusCentimeters = 50.0f;
    UFUNCTION(BlueprintCallable, Category = "Common AI|Debug") bool InspectActor(AActor* Actor);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Debug") bool InspectFromView();
    UFUNCTION(BlueprintCallable, Category = "Common AI|Debug") void SetPanelVisible(bool bVisible);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Debug") void SetDebugEnabled(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category = "Common AI|Debug") void SetAllWorldPanelsVisible(bool bVisible);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    APlayerController* PlayerController() const;
    bool EnsureWidget();
    UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
    UPROPERTY(Transient) TObjectPtr<UCommonAIInspectorWidget> Widget;
    UPROPERTY(Transient) TObjectPtr<UWidgetComponent> WorldWidget;
};
