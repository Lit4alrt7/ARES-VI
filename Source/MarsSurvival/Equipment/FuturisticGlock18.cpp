#include "Equipment/FuturisticGlock18.h"

#include "Audio/MarsAudioManager.h"
#include "Equipment/MarsBullet.h"
#include "Items/MarsInventoryComponent.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "World/MarsStationSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"

namespace
{
	const FVector LaserEmitter(10.5f, 0.0f, -1.5f);
	const FVector MuzzlePoint(13.2f, 0.0f, 2.6f);
}

AFuturisticGlock18::AFuturisticGlock18()
{
	ItemType = EMarsItemType::Glock18;
	ViewModelKind = EMarsPropKind::GlockModel;
	HeldOffset = FVector(42.0f, 13.0f, -15.0f);

	MuzzleFlash = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlash"));
	MuzzleFlash->SetupAttachment(ViewRoot);
	MuzzleFlash->SetRelativeLocation(MuzzlePoint + FVector(4, 0, 0));
	MuzzleFlash->SetIntensityUnits(ELightUnits::Lumens);
	MuzzleFlash->SetIntensity(3500.0f);
	MuzzleFlash->SetAttenuationRadius(700.0f);
	MuzzleFlash->SetLightColor(FLinearColor(1.0f, 0.78f, 0.45f));
	MuzzleFlash->SetVisibility(false);
	MuzzleFlash->SetCastShadows(false);

	LaserDot = CreateDefaultSubobject<UPointLightComponent>(TEXT("LaserDot"));
	LaserDot->SetupAttachment(ViewRoot);
	LaserDot->SetUsingAbsoluteLocation(true);
	LaserDot->SetIntensityUnits(ELightUnits::Lumens);
	LaserDot->SetIntensity(18.0f);
	LaserDot->SetAttenuationRadius(15.0f);
	LaserDot->SetLightColor(FLinearColor(1.0f, 0.05f, 0.02f));
	LaserDot->SetCastShadows(false);

	AmmoCounter = CreateDefaultSubobject<UTextRenderComponent>(TEXT("AmmoCounter"));
	AmmoCounter->SetupAttachment(ViewRoot);
	AmmoCounter->SetRelativeLocationAndRotation(FVector(-7.45f, 0.0f, 2.6f), FRotator(0.0f, 180.0f, 0.0f));
	AmmoCounter->SetHorizontalAlignment(EHTA_Center);
	AmmoCounter->SetVerticalAlignment(EVRTA_TextCenter);
	AmmoCounter->SetWorldSize(1.1f);
	AmmoCounter->SetTextRenderColor(FColor(90, 255, 140));
	AmmoCounter->SetCastShadow(false);
}

void AFuturisticGlock18::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SlideParts.Reset();
	SlideRestLocations.Reset();
	for (UStaticMeshComponent* Part : FindTaggedParts(TEXT("Slide")))
	{
		SlideParts.Add(Part);
		SlideRestLocations.Add(Part->GetRelativeLocation());
	}

	// Laser beam: a 0.15 cm emissive filament stretched every frame to the hit point.
	LaserBeam = AMarsPropBuilder::AddPart(ViewRoot, EMarsPropShape::Box, LaserEmitter, FVector(1, 0.15f, 0.15f), FRotator::ZeroRotator,
		FLinearColor(1.0f, 0.05f, 0.02f), 25.0f);
}

void AFuturisticGlock18::OnEquipped(AMarsCharacter* NewOwner)
{
	Super::OnEquipped(NewOwner);
	LaserDot->SetVisibility(bLaserEnabled);
}

void AFuturisticGlock18::OnUnequipped()
{
	Super::OnUnequipped();
	LaserDot->SetVisibility(false);
}

void AFuturisticGlock18::UpdateLaser()
{
	if (!LaserBeam)
	{
		return;
	}
	LaserBeam->SetVisibility(bLaserEnabled);
	LaserDot->SetVisibility(bLaserEnabled);
	if (!bLaserEnabled)
	{
		return;
	}

	const FVector Start = ViewRoot->GetComponentTransform().TransformPosition(LaserEmitter);
	const FVector Dir = GetActorForwardVector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GlockLaser), false, this);
	if (OwnerCharacter.IsValid())
	{
		Params.AddIgnoredActor(OwnerCharacter.Get());
	}
	FHitResult Hit;
	const float MaxRange = 4000.0f;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * MaxRange, ECC_Visibility, Params);
	const float Length = bHit ? Hit.Distance : MaxRange;

	LaserBeam->SetRelativeLocation(LaserEmitter + FVector(Length * 0.5f, 0.0f, 0.0f));
	LaserBeam->SetRelativeScale3D(FVector(Length / 100.0f, 0.0015f, 0.0015f));
	LaserDot->SetWorldLocation(bHit ? Hit.ImpactPoint + Hit.ImpactNormal * 2.0f : Start + Dir * MaxRange);
	LaserDot->SetVisibility(bHit);
}

void AFuturisticGlock18::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TimeSinceLastShot += DeltaTime;

	if (bPrimaryHeld && bFullAuto && TimeSinceLastShot >= 60.0f / FMath::Max(FireRateRpm, 1.0f))
	{
		FireRound();
	}

	// Slide cycles back ~2 cm and returns.
	SlideRecoil = FMath::FInterpTo(SlideRecoil, 0.0f, DeltaTime, 25.0f);
	for (int32 i = 0; i < SlideParts.Num(); ++i)
	{
		if (SlideParts[i])
		{
			SlideParts[i]->SetRelativeLocation(SlideRestLocations[i] - FVector(SlideRecoil * 2.2f, 0, 0));
		}
	}

	MuzzleFlashTime -= DeltaTime;
	MuzzleFlash->SetVisibility(MuzzleFlashTime > 0.0f);

	AmmoCounter->SetText(FText::FromString(FString::Printf(TEXT("%02d"), RoundsInMagazine)));
	AmmoCounter->SetTextRenderColor(RoundsInMagazine <= 5 ? FColor(255, 70, 40) : FColor(90, 255, 140));
	UpdateLaser();
}

void AFuturisticGlock18::StartPrimary()
{
	Super::StartPrimary();
	if (!bFullAuto && TimeSinceLastShot >= 0.11f)
	{
		FireRound();
	}
}

void AFuturisticGlock18::StartSecondary()
{
	bFullAuto = !bFullAuto;
	if (UMarsAudioManager* Audio = GetAudio())
	{
		Audio->PlayUIBeep(bFullAuto ? 1.4f : 1.0f);
	}
	if (AMarsCharacter* Character = OwnerCharacter.Get())
	{
		Character->ShowNotification(FText::FromString(bFullAuto ? TEXT("SELECTOR: FULL AUTO") : TEXT("SELECTOR: SEMI")), 1.5f);
	}
}

void AFuturisticGlock18::Reload()
{
	AMarsCharacter* Character = OwnerCharacter.Get();
	UMarsInventoryComponent* Inventory = Character ? Character->GetInventory() : nullptr;
	if (!Inventory)
	{
		return;
	}
	const int32 Loaded = FMath::Min(MagazineCapacity - RoundsInMagazine, Inventory->GetItemCount(EMarsItemType::Ammo9mm));
	if (Loaded > 0 && Inventory->RemoveItem(EMarsItemType::Ammo9mm, Loaded))
	{
		RoundsInMagazine += Loaded;
		if (UMarsAudioManager* Audio = GetAudio())
		{
			Audio->PlayRatchet();
		}
	}
}

void AFuturisticGlock18::FireRound()
{
	AMarsCharacter* Character = OwnerCharacter.Get();
	if (!Character || !Character->GetFirstPersonCamera())
	{
		return;
	}
	TimeSinceLastShot = 0.0f;

	if (RoundsInMagazine <= 0)
	{
		if (UMarsAudioManager* Audio = GetAudio())
		{
			Audio->PlayDryFire();
		}
		bPrimaryHeld = false;
		return;
	}
	--RoundsInMagazine;

	// Aim from the eye (where the reticle is), launch from the muzzle.
	const UCameraComponent* Camera = Character->GetFirstPersonCamera();
	const float Spread = (bFullAuto ? AutoSpreadDegrees : SemiSpreadDegrees) + GetTremorDegrees();
	const FVector AimDir = FMath::VRandCone(Camera->GetForwardVector(), FMath::DegreesToRadians(Spread));
	FHitResult AimHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GlockAim), true, Character);
	Params.AddIgnoredActor(this);
	const FVector Eye = Camera->GetComponentLocation();
	const FVector AimPoint = GetWorld()->LineTraceSingleByChannel(AimHit, Eye, Eye + AimDir * 20000.0f, ECC_Visibility, Params)
		? AimHit.ImpactPoint : Eye + AimDir * 20000.0f;

	const FVector Muzzle = ViewRoot->GetComponentTransform().TransformPosition(MuzzlePoint);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AMarsBullet* Bullet = GetWorld()->SpawnActor<AMarsBullet>(AMarsBullet::StaticClass(), Muzzle, (AimPoint - Muzzle).Rotation(), SpawnParams))
	{
		Bullet->Launch(AimPoint - Muzzle, MuzzleVelocityCmS, Damage, Character->GetController(), this);
	}

	if (UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>())
	{
		Station->ReportWeaponDischarge(Character, Muzzle, false);
	}
	if (UPlayerBiometricsComponent* Bio = Character->GetBiometrics())
	{
		Bio->NotifyWeaponDischarged();
	}
	if (UMarsAudioManager* Audio = GetAudio())
	{
		Audio->PlayGunshot();
	}

	SlideRecoil = 1.0f;
	MuzzleFlashTime = 0.04f;
	Character->AddRecoil(RecoilPitch * FMath::FRandRange(0.7f, 1.2f), FMath::FRandRange(-0.25f, 0.25f) * RecoilPitch);
	Character->AddControllerYawInput(FMath::FRandRange(-0.25f, 0.25f));
}

FText AFuturisticGlock18::GetStatusText() const
{
	const AMarsCharacter* Character = OwnerCharacter.Get();
	const int32 Reserve = Character && Character->GetInventory() ? Character->GetInventory()->GetItemCount(EMarsItemType::Ammo9mm) : 0;
	return FText::FromString(FString::Printf(TEXT("G18 %s  %02d / %d"), bFullAuto ? TEXT("AUTO") : TEXT("SEMI"), RoundsInMagazine, Reserve));
}
