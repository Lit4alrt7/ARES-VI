#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MarsHUD.generated.h"

class UMarsPlayerHUD;

/** Hosts the UMG player HUD widget (all drawing lives in UMarsPlayerHUD). */
UCLASS()
class MARSSURVIVAL_API AMarsHUD : public AHUD
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetHUDVisible(bool bVisible);

	/** Toggle key: minimal UI (prompts, alarms and subtitles only). */
	void ToggleMinimalHUD();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Transient)
	TObjectPtr<UMarsPlayerHUD> HUDWidget;
};
