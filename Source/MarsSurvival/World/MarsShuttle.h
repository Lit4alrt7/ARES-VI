#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarsShuttle.generated.h"

class UPointLightComponent;

DECLARE_MULTICAST_DELEGATE(FOnShuttleDeparted);

/**
 * The supply / extraction shuttle parked outside the airlock viewport. StartDeparture() plays the
 * opening hook: engines spool, lift-off, pitch-over and climb into the dust storm - stranding the crew.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsShuttle : public AActor
{
	GENERATED_BODY()

public:
	AMarsShuttle();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Shuttle")
	void StartDeparture();

	/** Fires once the shuttle has cleared the pad (A.R.I.A.'s first transmission hooks onto this). */
	FOnShuttleDeparted OnDeparted;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> EngineGlow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> BeaconLight;

private:
	bool bDeparting = false;
	bool bAnnounced = false;
	float Time = 0.0f;
	FVector StartLocation = FVector::ZeroVector;
	FRotator StartRotation = FRotator::ZeroRotator;
	float Speed = 0.0f;
	float DustAccumulator = 0.0f;
	float LastYaw = 0.0f;
	float Bank = 0.0f;
};
