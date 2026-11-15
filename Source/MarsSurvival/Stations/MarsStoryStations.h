#pragma once

#include "CoreMinimal.h"
#include "Items/MarsItemTypes.h"
#include "Stations/MarsStations.h"
#include "MarsStoryStations.generated.h"

/**
 * Wall locker / medical cabinet / magnetised cargo rack. Holds neatly stored items (and optionally an
 * encrypted data log). E opens the doors and moves the contents into your inventory; the log is shown
 * on the visor panel and archived. Opening reports "Locker_<Tag>" / StoryEvent to the narrative director.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsStorageLocker : public AMarsStationBase
{
	GENERATED_BODY()

public:
	AMarsStorageLocker();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locker")
	TArray<FMarsInventorySlot> Contents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locker")
	bool bOpened = false;

	/** Event reported to UMarsNarrativeDirector on first opening (e.g. "Log_Relay"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locker")
	FName StoryEvent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locker")
	FString LogTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locker", meta = (MultiLine = true))
	FString LogText;

	/** Extra bio-agent exposure when opened (biologically active samples). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locker")
	float BioExposure = 0.0f;

protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;

private:
	float DoorAngle = 0.0f;
};

/** Crew bunk: sleep until the next morning (07:00), resetting fatigue. Advances the Sol. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsBunkStation : public AMarsStationBase
{
	GENERATED_BODY()

public:
	AMarsBunkStation();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

protected:
	virtual FString GetScreenText() const override;
};

/** Engineering terminal: reroute RTG power between sectors (E cycles the routing profile). */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsPowerRoutingTerminal : public AMarsStationBase
{
	GENERATED_BODY()

public:
	AMarsPowerRoutingTerminal();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual void InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;

private:
	float HoldTime = 0.0f;
	double LastHold = 0.0;
	bool bHoldConsumed = false;
};

/** Rest / chrono terminal: tap E to pick a target time (06:00, 12:00, 18:00, 22:00), hold E to rest until then. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsRestTerminal : public AMarsStationBase
{
	GENERATED_BODY()

public:
	AMarsRestTerminal();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual void InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

protected:
	virtual FString GetScreenText() const override;

private:
	int32 TargetIndex = 2;
	float HoldTime = 0.0f;
	double LastHoldTime = 0.0;
};
