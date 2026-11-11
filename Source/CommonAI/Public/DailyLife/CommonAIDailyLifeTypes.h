#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "CommonAIDailyLifeTypes.generated.h"

UENUM(BlueprintType)
enum class ECommonAIDailyActivity : uint8 { Work, Eat, Sleep, Rest, Socialize, Wander, ReturnHome, Idle, SiteActivity };

USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAITimePreference
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time", meta = (ClampMin="0", ClampMax="24")) float StartHour = 8.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time", meta = (ClampMin="0", ClampMax="24")) float EndHour = 17.0f;
    bool Contains(double Hour, float OffsetHours = 0.0f) const;
};

// Serializable state contains identities and progress, never live controller or claim handles.
USTRUCT(BlueprintType)
struct COMMONAI_API FCommonAIDailyLifeState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Identity") FGuid Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Identity") FString DisplayName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Identity") FGameplayTag Occupation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Needs", meta=(ClampMin="0", ClampMax="1")) float Hunger = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Needs", meta=(ClampMin="0", ClampMax="1")) float Fatigue = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Assignments") FGuid HomeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Assignments") FGuid WorkId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Activity") FGameplayTag Activity;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Activity") FGuid DestinationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Activity") double ProgressHours = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Activity") FGameplayTag ResumeActivity;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Activity") double ResumeProgressHours = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Activity") double LastTimeHours = -1.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Activity") TMap<FGameplayTag, double> CooldownUntilHours;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Activity") int32 CompletedActivities = 0;
    bool IsValid() const;
};

UINTERFACE(BlueprintType)
class COMMONAI_API UCommonAIDailyLifeContext : public UInterface { GENERATED_BODY() };

class COMMONAI_API ICommonAIDailyLifeContext
{
    GENERATED_BODY()
public:
    // Monotonic accumulated simulation hours; return a negative value when unavailable.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Daily Life") double GetDailyLifeTimeHours() const;
    // An empty reason grants ordinary routine execution. The consumer retains emergency authority.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Common AI|Daily Life") FGameplayTag GetDailyLifeInterruptionReason() const;
};

namespace CommonAIDailyLife
{
    constexpr double HoursPerDay = 24.0;
    COMMONAI_API FGameplayTag ActivityTag(ECommonAIDailyActivity Kind);
    COMMONAI_API FGameplayTag InputTag(ECommonAIDailyActivity Kind, int32 Channel);
    COMMONAI_API FGameplayTag SourceTag();
    COMMONAI_API FGameplayTag EmergencyReason();
    COMMONAI_API FGameplayTag NeedReason();
}
