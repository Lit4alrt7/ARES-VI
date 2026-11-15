#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/MarsItemTypes.h"
#include "MarsSaveGame.generated.h"

/** Every saveable UPROPERTY of one object, exported as text (object references are skipped). */
USTRUCT()
struct FMarsSavedObject
{
	GENERATED_BODY()

	UPROPERTY()
	FString Key;

	UPROPERTY()
	TMap<FString, FString> Props;
};

/**
 * Mission save: player transform, biometrics, suit, inventory, every module (pressure, thermal, seals,
 * hull leaks, CO2/O2, dust, transistors) and every station's state, the remaining comm window, and which
 * pickups are still lying around. Two slots: "MarsCheckpoint" (automatic) and "MarsManual" (save terminals).
 */
UCLASS()
class MARSSURVIVAL_API UMarsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString CheckpointSlot;
	static const FString ManualSlot;

	static bool HasAnySave();
	/** Newest existing slot, or empty. */
	static FString MostRecentSlot();
	static UMarsSaveGame* LoadSlot(const FString& Slot);

	/** Capture the live world and write it to Slot. */
	static bool SaveWorld(UWorld* World, const FString& Slot, const FString& Label);
	/** Restore into the live world (call once the player pawn exists). */
	void ApplyToWorld(UWorld* World) const;

	UPROPERTY()
	FString Label;

	UPROPERTY()
	FDateTime Timestamp;

	UPROPERTY()
	FString LocationName;

	UPROPERTY()
	FTransform PlayerTransform;

	UPROPERTY()
	FRotator ControlRotation;

	UPROPERTY()
	float RemainingSeconds = 0.0f;

	UPROPERTY()
	float MissionElapsedSeconds = 0.0f;

	UPROPERTY()
	TArray<FMarsInventorySlot> Inventory;

	UPROPERTY()
	TArray<FMarsSavedObject> Objects;

	UPROPERTY()
	TArray<FString> RemainingPickups;

	UPROPERTY()
	bool bBioAgentCharacterized = false;

	UPROPERTY()
	TArray<EMarsItemType> InspectedTypes;

	// Headline stats for the load screen.
	UPROPERTY()
	int32 Sol = 1;

	UPROPERTY()
	float SolHours = 7.5f;

	UPROPERTY()
	int32 StoryStep = 0;

	UPROPERTY()
	TArray<FString> StoryLogs;

	UPROPERTY()
	int32 PowerRoute = 0;

	UPROPERTY()
	int32 MissionsCompleted = 0;

	UPROPERTY()
	bool bHasShuttle = false;

	UPROPERTY()
	FTransform ShuttleTransform;

	UPROPERTY()
	float ShuttleBatteryWh = 9000.0f;

	UPROPERTY()
	float ShuttleO2Liters = 4000.0f;

	UPROPERTY()
	bool bEmShield = false;

	UPROPERTY()
	float SuitO2Percent = 0.0f;

	UPROPERTY()
	int32 TransistorsInstalled = 0;
};
