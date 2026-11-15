#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MarsSuitViewmodel.generated.h"

class UTextRenderComponent;

/**
 * First-person MCP suit (AMCPPlayerSuit equivalent, lives on the camera): the left forearm with ribbed
 * pressure joints, metallic wrist bearing, glove and an articulated wrist telemetry computer
 * (O2, battery, external temperature, dose rate, Sol / local time). It rests at the lower-left edge of
 * the view and is raised to read with the WristDisplay key. Visible only while the suit is worn.
 */
UCLASS(ClassGroup = (Mars), meta = (BlueprintSpawnableComponent))
class MARSSURVIVAL_API UMarsSuitViewmodel : public USceneComponent
{
	GENERATED_BODY()

public:
	UMarsSuitViewmodel();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void ToggleRaised() { bRaised = !bRaised; }
	bool IsRaised() const { return bRaised; }

private:
	void Build();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Arm;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> WristText;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CollarRoot;

	bool bRaised = false;
	float Blend = 0.0f;
	float TextTimer = 0.0f;
};
