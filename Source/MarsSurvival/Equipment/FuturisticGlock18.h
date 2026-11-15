#pragma once

#include "CoreMinimal.h"
#include "Equipment/MarsEquipment.h"
#include "FuturisticGlock18.generated.h"

class UPointLightComponent;
class UTextRenderComponent;
class UStaticMeshComponent;

/**
 * 2087 NASA-modified Glock-18 (select fire, ~1,100 rpm on auto, 33-round extended magazine).
 * Procedural viewmodel: angular matte-polymer frame, vented titanium slide (animated recoil),
 * cyan holographic reflex sight with amber dot, underbarrel red laser designator (live beam + dot),
 * rear digital ammo counter. Fires physical AMarsBullet projectiles.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AFuturisticGlock18 : public AMarsEquipment
{
	GENERATED_BODY()

public:
	AFuturisticGlock18();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnEquipped(AMarsCharacter* NewOwner) override;
	virtual void OnUnequipped() override;
	virtual void StartPrimary() override;
	virtual void StartSecondary() override;
	virtual void Reload() override;
	virtual FText GetStatusText() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MuzzleFlash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> LaserDot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> AmmoCounter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	int32 MagazineCapacity = 33;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	int32 RoundsInMagazine = 33;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	bool bFullAuto = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	float FireRateRpm = 1100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	float Damage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	float MuzzleVelocityCmS = 37500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	float SemiSpreadDegrees = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	float AutoSpreadDegrees = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	float RecoilPitch = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glock")
	bool bLaserEnabled = true;

private:
	void FireRound();
	void UpdateLaser();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LaserBeam;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SlideParts;

	TArray<FVector> SlideRestLocations;
	float TimeSinceLastShot = 1.0f;
	float SlideRecoil = 0.0f;
	float MuzzleFlashTime = 0.0f;
};
