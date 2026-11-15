#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Items/MarsItemTypes.h"
#include "MarsMissionDirector.generated.h"

class AMarsReconShuttle;

UENUM()
enum class EMarsMalfunction : uint8
{
	None,
	ThrusterStuck,   // RCS valve stuck open: the craft yaws on its own
	NavFailure,      // star tracker down: no compass / waypoint
	PowerFault,      // bus short: battery drains 3x
	CoolantLeak      // cabin O2 reserve venting
};

enum class EMarsObjectiveKind : uint8 { Scan, Reach, Hold, Dock };

struct FMarsMissionObjective
{
	EMarsObjectiveKind Kind = EMarsObjectiveKind::Reach;
	FString Text;
	FVector Offset = FVector::ZeroVector;   // from the orbit origin, cm
	uint8 PropKind = 0;
	float Radius = 15000.0f;                // cm
	float Duration = 3.0f;                  // seconds for Scan / Hold / Dock
	float Scale = 1.0f;
	FLinearColor Tint = FLinearColor(1.0f, 0.3f, 0.1f);
};

struct FMarsMission
{
	FString Code;
	FString Name;
	int32 Sol = 2;
	FString Briefing;
	TArray<FMarsMissionObjective> Objectives;
	TArray<FMarsInventorySlot> Reward;
	FString LogTitle;
	FString LogText;
	FString AriaDone;
};

/**
 * Off-planet operations (6 missions across 14 Sols). Fly the RS-2 above 1.2 km and hold SPACE to burn for
 * orbit: the craft is moved to the orbital operations zone (120 km up, above the atmosphere) where the
 * day's mission targets are spawned. Objectives: scan (keep the target in the nose cone), reach, hold
 * position (signal interception) and dock (approach slowly). Random in-flight malfunctions need a manual
 * override (hold R in the cockpit). Hold C to de-orbit: you return above Pad 2, rewards are stowed and a
 * checkpoint is written. Running out of power in orbit forces an emergency return that damages the base.
 */
UCLASS()
class MARSSURVIVAL_API UMarsMissionDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMarsMissionDirector* Get(const UObject* WorldContext);
	static const FVector OrbitOrigin;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMarsMissionDirector, STATGROUP_Tickables); }

	/** Mission index available today (lowest uncompleted with Sol <= today), or INDEX_NONE. */
	int32 GetAvailableMission() const;
	FString GetStatusLine() const;

	bool IsInOrbit() const { return bInOrbit; }
	void RequestOrbitInsertion(AMarsReconShuttle* Shuttle);
	/** Dev capture (-AutoOrbit): start mission 0 regardless of Sol / altitude. */
	void DevForceOrbit(AMarsReconShuttle* Shuttle);
	void RequestDeorbit(AMarsReconShuttle* Shuttle, bool bEmergency = false);
	float GetTransitionFade() const { return Fade; }

	AActor* GetObjectiveTarget() const;
	FString GetObjectiveText() const;
	float GetObjectiveProgress01() const;
	const FMarsMission* GetActiveMission() const { return Missions.IsValidIndex(ActiveMission) ? &Missions[ActiveMission] : nullptr; }
	bool IsActiveMissionComplete() const;

	EMarsMalfunction GetMalfunction() const { return Malfunction; }
	FString GetMalfunctionText() const;
	float GetRepairProgress01() const { return RepairProgress / 3.0f; }

	int32 GetCompletedMask() const { return CompletedMask; }
	void SetCompletedMask(int32 Mask) { CompletedMask = Mask; }
	int32 GetMissionCount() const { return Missions.Num(); }

private:
	void BuildMissions();
	void SpawnTargets();
	void ClearTargets();
	void FinishTransition();
	void UpdateObjectives(float DeltaTime);
	void UpdateMalfunctions(float DeltaTime);

	TArray<FMarsMission> Missions;
	int32 CompletedMask = 0;
	int32 ActiveMission = INDEX_NONE;
	int32 ObjectiveIndex = 0;
	float ObjectiveProgress = 0.0f;
	bool bInOrbit = false;

	// Fade-out -> teleport -> fade-in.
	enum class ETransition : uint8 { None, ToOrbit, ToSurface };
	ETransition Transition = ETransition::None;
	bool bEmergencyReturn = false;
	float Fade = 0.0f;
	float TransitionTime = 0.0f;
	TWeakObjectPtr<AMarsReconShuttle> TransitShuttle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Targets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Scenery;

	EMarsMalfunction Malfunction = EMarsMalfunction::None;
	float MalfunctionTimer = 40.0f;
	float RepairProgress = 0.0f;
	float PostReturnHazardTime = 0.0f;
	float DeadBatteryTime = 0.0f;
	float DenyCooldown = 0.0f;
};
