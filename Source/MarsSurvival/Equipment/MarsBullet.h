#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarsBullet.generated.h"

class UPointLightComponent;

/**
 * Physical 9x19mm projectile: ~375 m/s muzzle velocity, Mars gravity (3.71 m/s^2), swept line
 * traces every frame. Passes through phantoms (no collision) and deals point damage to whatever
 * it hits - usually the station's own pressure hull.
 */
UCLASS()
class MARSSURVIVAL_API AMarsBullet : public AActor
{
	GENERATED_BODY()

public:
	AMarsBullet();

	virtual void Tick(float DeltaTime) override;

	void Launch(const FVector& Direction, float SpeedCmPerSecond, float InDamage, AController* InInstigatorController, AActor* InIgnoredActor);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> ImpactFlash;

protected:
	virtual void BeginPlay() override;

private:
	FVector Velocity = FVector::ZeroVector;
	float Damage = 20.0f;
	bool bHasHit = false;
	float HitTime = 0.0f;
	TWeakObjectPtr<AController> InstigatorController;
	TWeakObjectPtr<AActor> IgnoredActor;
};
