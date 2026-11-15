#include "World/MarsWaypointTrail.h"

#include "Core/MarsNarrativeDirector.h"
#include "Player/MarsCharacter.h"
#include "Props/MarsPropBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

AMarsWaypointTrail::AMarsWaypointTrail()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AMarsWaypointTrail::BeginPlay()
{
	Super::BeginPlay();
	const FLinearColor Holo(0.25f, 0.85f, 1.0f, 0.75f);
	for (int32 i = 0; i < NumChevrons; ++i)
	{
		USceneComponent* Pivot = AMarsPropBuilder::AddPivot(GetRootComponent(), FVector::ZeroVector, FRotator::ZeroRotator, NAME_None);
		if (!Pivot)
		{
			continue;
		}
		Pivot->SetUsingAbsoluteLocation(true);
		Pivot->SetUsingAbsoluteRotation(true);
		TArray<TObjectPtr<UStaticMeshComponent>> Parts;
		// ">" made of two thin glowing bars lying on the floor, pointing along +X.
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			if (UStaticMeshComponent* Bar = AMarsPropBuilder::AddPart(Pivot, EMarsPropShape::Box, FVector(-12.0f, Side * 14.0f, 1.0f), FVector(44.0f, 7.0f, 1.0f), FRotator(0.0f, -Side * 38.0f, 0.0f), Holo, 6.0f))
			{
				Bar->SetCastShadow(false);
				Parts.Add(Bar);
			}
		}
		Chevrons.Add(Pivot);
		ChevronParts.Add(Parts);
	}
}

void AMarsWaypointTrail::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Time += DeltaTime;

	const UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this);
	const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const AActor* Target = Story ? Story->GetObjectiveTarget() : nullptr;
	const bool bShow = Story && Story->AreWaypointsEnabled() && Player && Target && !Player->IsDead() && !Player->IsSleeping();

	int32 Visible = 0;
	if (bShow)
	{
		const FVector Feet = Player->GetActorLocation() - FVector(0.0f, 0.0f, 88.0f);
		FVector ToTarget = Target->GetActorLocation() - Feet;
		ToTarget.Z = 0.0f;
		const float Dist = ToTarget.Size();
		const FVector Dir = ToTarget.GetSafeNormal();
		const float Yaw = Dir.Rotation().Yaw;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(WaypointTrail), false, Player);
		Params.AddIgnoredActor(this);
		for (int32 i = 0; i < Chevrons.Num(); ++i)
		{
			const float Along = 140.0f + i * 130.0f;
			if (Along > Dist - 60.0f)
			{
				break;
			}
			const FVector Probe = Feet + Dir * Along + FVector(0.0f, 0.0f, 60.0f);
			// Stop at walls: the trail ends at the bulkhead / doorway.
			FHitResult Block;
			if (GetWorld()->LineTraceSingleByChannel(Block, Player->GetActorLocation(), Probe, ECC_Visibility, Params) && Block.GetActor() != Target)
			{
				break;
			}
			// Hug the floor.
			FHitResult Floor;
			float Z = Feet.Z;
			if (GetWorld()->LineTraceSingleByChannel(Floor, Probe, Probe - FVector(0.0f, 0.0f, 400.0f), ECC_Visibility, Params))
			{
				Z = Floor.ImpactPoint.Z;
			}
			Chevrons[i]->SetWorldLocationAndRotation(FVector(Probe.X, Probe.Y, Z + 1.5f), FRotator(0.0f, Yaw, 0.0f));
			Chevrons[i]->SetVisibility(true, true);

			// Pulse running toward the goal.
			const float Wave = 0.35f + 0.65f * FMath::Pow(FMath::Max(FMath::Sin(Time * 4.0f - i * 0.7f), 0.0f), 3.0f);
			for (UStaticMeshComponent* Bar : ChevronParts[i])
			{
				if (UMaterialInstanceDynamic* MID = Bar ? Cast<UMaterialInstanceDynamic>(Bar->GetMaterial(0)) : nullptr)
				{
					MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.25f, 0.85f, 1.0f) * 7.0f * Wave);
				}
			}
			++Visible;
		}
	}
	for (int32 i = Visible; i < Chevrons.Num(); ++i)
	{
		Chevrons[i]->SetVisibility(false, true);
	}
}
