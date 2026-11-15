#include "World/RadiationHazardZone.h"

#include "Player/MarsCharacter.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

ARadiationHazardZone::ARadiationHazardZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;

	Zone = CreateDefaultSubobject<USphereComponent>(TEXT("Zone"));
	SetRootComponent(Zone);
	Zone->SetSphereRadius(4000.0f);
	Zone->SetCollisionProfileName(TEXT("Trigger"));
	Zone->SetGenerateOverlapEvents(true);
}

float ARadiationHazardZone::GetDoseRateAt(const FVector& WorldLocation) const
{
	const float Radius = FMath::Max(Zone->GetScaledSphereRadius(), 1.0f);
	const float Normalised = FMath::Clamp(FVector::Dist(WorldLocation, GetActorLocation()) / Radius, 0.0f, 1.0f);
	// Smooth falloff toward the edge of the hotspot.
	const float Falloff = FMath::Square(1.0f - Normalised);

	float Storm = 0.0f;
	if (const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		Storm = Env->GetStormIntensity();
	}
	return PeakDoseRateMsvPerHour * Falloff * FMath::Lerp(1.0f, StormMultiplier, Storm);
}

void ARadiationHazardZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TArray<AActor*> Overlapping;
	Zone->GetOverlappingActors(Overlapping, AMarsCharacter::StaticClass());

	TMap<TWeakObjectPtr<AMarsCharacter>, float> NewRates;
	for (AActor* Actor : Overlapping)
	{
		if (AMarsCharacter* Character = Cast<AMarsCharacter>(Actor))
		{
			NewRates.Add(Character, GetDoseRateAt(Character->GetActorLocation()));
		}
	}

	// Apply the delta between last tick's contribution and this tick's.
	for (const TPair<TWeakObjectPtr<AMarsCharacter>, float>& Old : AppliedRates)
	{
		if (AMarsCharacter* Character = Old.Key.Get())
		{
			const float* New = NewRates.Find(Old.Key);
			Character->AddHazardDoseRate((New ? *New : 0.0f) - Old.Value);
		}
	}
	for (const TPair<TWeakObjectPtr<AMarsCharacter>, float>& New : NewRates)
	{
		if (!AppliedRates.Contains(New.Key))
		{
			if (AMarsCharacter* Character = New.Key.Get())
			{
				Character->AddHazardDoseRate(New.Value);
			}
		}
	}
	AppliedRates = MoveTemp(NewRates);
}

void ARadiationHazardZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TPair<TWeakObjectPtr<AMarsCharacter>, float>& Old : AppliedRates)
	{
		if (AMarsCharacter* Character = Old.Key.Get())
		{
			Character->AddHazardDoseRate(-Old.Value);
		}
	}
	AppliedRates.Reset();
	Super::EndPlay(EndPlayReason);
}
