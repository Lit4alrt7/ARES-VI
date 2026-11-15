#pragma once

#include "CoreMinimal.h"
#include "Stations/MarsStations.h"
#include "MarsSaveTerminal.generated.h"

/**
 * Mission log terminal (Storage and Med-Bay): tap E to write a manual save of the whole station state,
 * the player's biometrics and inventory. Refuses while Stage 4 collapse is running.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsSaveTerminal : public AMarsStationBase
{
	GENERATED_BODY()

public:
	AMarsSaveTerminal();
	virtual void Tick(float DeltaTime) override;
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;

private:
	float Cooldown = 0.0f;
	float WrittenFlash = 0.0f;
	FString LastWrite;
};
