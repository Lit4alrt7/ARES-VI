#include "Equipment/MarsBullet.h"

#include "Equipment/MarsImpactFX.h"
#include "Audio/MarsAudioManager.h"
#include "Base/MarsBaseModule.h"
#include "Hallucination/HallucinationPhantom.h"
#include "EngineUtils.h"
#include "Player/MarsCharacter.h"
#include "Props/MarsPropBuilder.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

AMarsBullet::AMarsBullet()
{
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = 3.0f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ImpactFlash = CreateDefaultSubobject<UPointLightComponent>(TEXT("ImpactFlash"));
	ImpactFlash->SetupAttachment(Root);
	ImpactFlash->SetIntensityUnits(ELightUnits::Lumens);
	ImpactFlash->SetIntensity(1500.0f);
	ImpactFlash->SetAttenuationRadius(220.0f);
	ImpactFlash->SetLightColor(FLinearColor(1.0f, 0.7f, 0.35f));
	ImpactFlash->SetCastShadows(false);
	ImpactFlash->SetVisibility(false);
}

void AMarsBullet::BeginPlay()
{
	Super::BeginPlay();
	// Tracer streak.
	AMarsPropBuilder::AddPart(Root, EMarsPropShape::Box, FVector(-12, 0, 0), FVector(24, 0.5f, 0.5f), FRotator::ZeroRotator, FLinearColor(1.0f, 0.65f, 0.2f), 15.0f);
}

void AMarsBullet::Launch(const FVector& Direction, float SpeedCmPerSecond, float InDamage, AController* InInstigatorController, AActor* InIgnoredActor)
{
	Velocity = Direction.GetSafeNormal() * SpeedCmPerSecond;
	Damage = InDamage;
	InstigatorController = InInstigatorController;
	IgnoredActor = InIgnoredActor;
	SetActorRotation(Velocity.Rotation());
}

void AMarsBullet::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bHasHit)
	{
		HitTime += DeltaTime;
		ImpactFlash->SetIntensity(1500.0f * FMath::Max(1.0f - HitTime / 0.08f, 0.0f));
		if (HitTime > 0.08f)
		{
			Destroy();
		}
		return;
	}

	const FVector Start = GetActorLocation();
	Velocity.Z -= 371.0f * DeltaTime; // Mars gravity, cm/s^2
	const FVector End = Start + Velocity * DeltaTime;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MarsBullet), true);
	Params.AddIgnoredActor(this);
	if (IgnoredActor.IsValid())
	{
		Params.AddIgnoredActor(IgnoredActor.Get());
		if (AActor* WeaponOwner = IgnoredActor->GetOwner())
		{
			Params.AddIgnoredActor(WeaponOwner);
		}
	}

	// Phantoms have no collision: test their silhouettes explicitly. The round does NOT stop -
	// it carries on into whatever real structure is behind the "creature".
	for (TActorIterator<AHallucinationPhantom> It(GetWorld()); It; ++It)
	{
		if (It->TestSegment(Start, End))
		{
			AMarsImpactFX::Spawn(GetWorld(), It->GetActorLocation() + FVector(0, 0, 90.0f), -Velocity.GetSafeNormal(), EMarsImpactType::PhantomAsh);
			It->Awaken();
		}
	}

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		bHasHit = true;
		SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * 5.0f);
		ImpactFlash->SetVisibility(true);
		{
			const AActor* HitOwner = Hit.GetActor();
			const bool bMetal = HitOwner && (HitOwner->IsA<AMarsBaseModule>() || (HitOwner->GetAttachParentActor() && HitOwner->GetAttachParentActor()->IsA<AMarsBaseModule>())
				|| HitOwner->GetClass()->GetName().Contains(TEXT("Station")));
			AMarsImpactFX::Spawn(GetWorld(), Hit.ImpactPoint, Hit.ImpactNormal, bMetal ? EMarsImpactType::Metal : EMarsImpactType::Regolith);
		}

		if (AActor* HitActor = Hit.GetActor())
		{
			UGameplayStatics::ApplyPointDamage(HitActor, Damage, Velocity.GetSafeNormal(), Hit, InstigatorController.Get(), this, UDamageType::StaticClass());

			if (const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
			{
				if (UMarsAudioManager* Audio = Player->GetAudioManager())
				{
					Audio->PlayBulletImpact(Hit.ImpactPoint, Cast<AMarsBaseModule>(HitActor) != nullptr);
				}
			}
		}
		return;
	}

	SetActorLocationAndRotation(End, Velocity.Rotation());
}
