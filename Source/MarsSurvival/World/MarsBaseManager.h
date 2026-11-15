#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MarsBaseManager.generated.h"

/** Snapshot of the outpost's health (refreshed once a second). */
struct FMarsBaseStatus
{
	float PowerOutputW = 0.0f;
	float PowerDemandW = 0.0f;
	float PowerAvailability = 1.0f;
	float AvgO2Percent = 21.0f;
	float AvgIntegrity = 1.0f;
	FString WorstModule;
	float WorstIntegrity = 1.0f;
	float WaterLiters = 0.0f;
	int32 FoodRations = 0;
	int32 O2Canisters = 0;
	int32 SpareParts = 0;
	bool bCraftingLocked = false;
	FString CraftingLockReason;
};

/**
 * Base integrity / resources manager. Aggregates generator output, module oxygen, structural integrity
 * and stockpiles, and applies the consequences:
 *  - modules below 50 % integrity bleed oxygen (micro-fractures), worse below 30 %;
 *  - a damaged Med-Bay (< 50 %) or a weak grid (< 40 %) locks the bioprinter (crafting);
 *  - a badly damaged Bio-Dome stops the Chlorella culture recovering.
 * External damage (solar storms, hull punctures, a failed orbital op) feeds straight into these numbers.
 */
UCLASS()
class MARSSURVIVAL_API UMarsBaseManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMarsBaseManager* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMarsBaseManager, STATGROUP_Tickables); }

	const FMarsBaseStatus& GetStatus() const { return Status; }
	bool IsCraftingLocked() const { return Status.bCraftingLocked; }

	/** Spread structural damage over the station (e.g. a failed mission, a storm). */
	void ApplyBaseDamage(float Amount);

private:
	void Refresh();
	FMarsBaseStatus Status;
	float RefreshTimer = 0.0f;
};
