#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MarsSolarStormManager.generated.h"

class AMarsBaseModule;

UENUM()
enum class ESolarStormPhase : uint8
{
	Idle,
	Warning,   // 90 s: shield the base or get to the shelter
	Active,    // ~4 min: zero visibility, radiation, electronics failures, progressive hull damage
	Subsiding
};

/**
 * Catastrophic Solar Storm (coronal mass ejection + global dust storm). Rare: from Sol 3 a ~15 % chance per
 * Sol, and at least once during chapter III. While active:
 *  - weather is forced to a zero-visibility dust storm, the surface dose rate spikes;
 *  - unshielded electronics fail: station grid output drops (lights brown out and flicker), HUD/visor
 *    glitches, the RS-2 shuttle loses thrust and drains its battery;
 *  - modules take progressive structural damage.
 * Countermeasures: raise the EM shield at the Engineering terminal (hold E, costs 4 kW) or shelter in the
 * Med-Bay storm shelter (blast-proof, fully shielded compartment).
 * Dev: "-SolarStormAt=N" starts the warning N real seconds after begin play.
 */
UCLASS()
class MARSSURVIVAL_API UMarsSolarStormManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMarsSolarStormManager* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMarsSolarStormManager, STATGROUP_Tickables); }

	void TriggerStorm();
	ESolarStormPhase GetPhase() const { return Phase; }
	bool IsActive() const { return Phase == ESolarStormPhase::Active; }
	bool IsThreat() const { return Phase == ESolarStormPhase::Warning || Phase == ESolarStormPhase::Active; }
	float GetPhaseSecondsRemaining() const { return PhaseRemaining; }

	/** Fraction of the surface dose that reaches someone in this module (null = outside). */
	float GetIndoorDoseFactor(const AMarsBaseModule* Module) const;

	/** True when unshielded electronics are failing right now (HUD glitches, shuttle faults). */
	bool AreElectronicsFailing() const;

	static bool IsShelter(const AMarsBaseModule* Module);

	int32 GetStormsSoFar() const { return StormsSoFar; }

private:
	void SetPhase(ESolarStormPhase NewPhase, float Seconds);
	void DamageTick();

	ESolarStormPhase Phase = ESolarStormPhase::Idle;
	float PhaseRemaining = 0.0f;
	float DamageTimer = 0.0f;
	float DevTimer = -1.0f;
	bool bDevChecked = false;
	int32 LastSolChecked = 0;
	int32 StormsSoFar = 0;
	float ScheduledHour = -1.0f;
};
