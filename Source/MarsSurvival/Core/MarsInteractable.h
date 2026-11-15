#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MarsInteractable.generated.h"

class AMarsCharacter;

UINTERFACE(MinimalAPI, Blueprintable)
class UMarsInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Anything the player can press (E) or hold (E) on: stations, terminals, pickups, beacons. */
class MARSSURVIVAL_API IMarsInteractable
{
	GENERATED_BODY()

public:
	/** Single press. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AMarsCharacter* InteractingCharacter);
	virtual void Interact_Implementation(AMarsCharacter* InteractingCharacter) {}

	/** Called every frame while the interact key is held on this target. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void InteractHold(AMarsCharacter* InteractingCharacter, float DeltaSeconds);
	virtual void InteractHold_Implementation(AMarsCharacter* InteractingCharacter, float DeltaSeconds) {}

	/** HUD prompt, e.g. "[E] Cycle airlock". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt(AMarsCharacter* InteractingCharacter) const;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* InteractingCharacter) const { return FText::GetEmpty(); }
};
