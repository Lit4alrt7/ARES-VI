#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerFatigueComponent.generated.h"

/**
 * Sleep / fatigue. Fatigue rises with game hours awake (faster with exertion) and is pushed into the
 * biometrics as SleepDeprivation: slower stamina recovery and, past ~55 %, a growing hallucination
 * contribution. Sleeping in a crew bunk resets it and fast-forwards to the next morning.
 */
UCLASS(ClassGroup = (Mars), meta = (BlueprintSpawnableComponent))
class MARSSURVIVAL_API UPlayerFatigueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerFatigueComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True when tired enough, or it is evening / night. */
	bool CanSleep() const;

	/** Hours until 07:00 next morning (min 4). */
	float GetHoursUntilMorning() const;

	/** Hours from now until the given local hour (wraps past midnight). */
	float GetHoursUntil(float TargetHour) const;

	/** Called once the screen is black: skips the clock. bSleep = proper sleep (fatigue reset, reports "Slept"), otherwise a rest/wait. */
	void PerformRest(float Hours, bool bSleep);
	void PerformSleep();

	/** 0 = fresh, 1 = exhausted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fatigue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Fatigue = 0.05f;

	/** Game hours awake until fully exhausted at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fatigue")
	float HoursToExhaustion = 20.0f;

private:
	float WarnCooldown = 0.0f;
};
