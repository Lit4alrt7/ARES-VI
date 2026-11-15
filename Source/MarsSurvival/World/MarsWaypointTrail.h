#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarsWaypointTrail.generated.h"

class UStaticMeshComponent;

/**
 * Diegetic wayfinding: a line of holographic floor chevrons projected from the suit toward the current
 * story objective (UMarsNarrativeDirector target). The trail hugs the floor, stops at walls (so it never
 * points through bulkheads - follow it to the doorway signage), and pulses toward the goal.
 * Toggled with the Waypoints key (N).
 */
UCLASS()
class MARSSURVIVAL_API AMarsWaypointTrail : public AActor
{
	GENERATED_BODY()

public:
	AMarsWaypointTrail();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	static constexpr int32 NumChevrons = 9;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Chevrons;

	TArray<TArray<TObjectPtr<UStaticMeshComponent>>> ChevronParts;
	float Time = 0.0f;
};
