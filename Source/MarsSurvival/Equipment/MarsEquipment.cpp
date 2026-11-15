#include "Equipment/MarsEquipment.h"

#include "Audio/MarsAudioManager.h"
#include "Base/MarsBaseModule.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Props/MarsPropBuilder.h"

AMarsEquipment::AMarsEquipment()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ViewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewRoot"));
	SetRootComponent(ViewRoot);
}

void AMarsEquipment::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AMarsPropBuilder::BuildProp(ViewRoot, ViewModelKind, 1.0f, false, 0.22f);
	if (bShowHand)
	{
		BuildSuitHand();
	}
	// No PS1 vertex snapping on view models: at 30 cm from the lens it shreds small parts.
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* M : Meshes)
	{
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
		{
			MID->SetScalarParameterValue(TEXT("JitterAmount"), 0.0f);
		}
		M->SetCastShadow(false);
		M->bCastDynamicShadow = false;
	}
}

void AMarsEquipment::BuildSuitHand()
{
	// MCP glove + ribbed cuff + suit sleeve running back out of frame toward the shoulder.
	USceneComponent* Grip = AMarsPropBuilder::AddPivot(ViewRoot, GripLocation, GripRotation, TEXT("Hand"));
	if (!Grip)
	{
		return;
	}
	using S = EMarsPropShape;
	const FLinearColor Glove(0.2f, 0.21f, 0.23f);
	const FLinearColor GloveGrip(0.1f, 0.1f, 0.11f);
	const FLinearColor Fabric(0.8f, 0.8f, 0.77f);
	const FLinearColor Ring(0.5f, 0.52f, 0.55f);
	const FLinearColor Accent(0.9f, 0.38f, 0.08f);
	const float W = GripWidth;
	auto Add = [Grip](S Shape, const FVector& L, const FVector& Sz, const FRotator& R, const FLinearColor& C, float E = 0.15f, float Rough = 0.9f, float Metallic = 0.0f)
	{
		AMarsPropBuilder::SetSurface(AMarsPropBuilder::AddPart(Grip, Shape, L, Sz, R, C, E), Rough, Metallic);
	};
	const FLinearColor Rubber(0.05f, 0.05f, 0.055f);
	// Palm on the right side of the grip, web of the hand on the backstrap.
	Add(S::Box, FVector(-0.4f, W * 0.5f + 0.7f, -0.3f), FVector(4.6f, 1.4f, 7.6f), FRotator::ZeroRotator, Glove);
	Add(S::Box, FVector(-2.5f, 0.4f, 2.6f), FVector(1.6f, W + 1.4f, 3.0f), FRotator::ZeroRotator, Glove);
	// Fingers wrapped around the front strap (index finger rests on the frame, above).
	for (int32 i = 0; i < 3; ++i)
	{
		const float Z = 1.0f - i * 1.9f;
		Add(S::Box, FVector(2.4f, 0.0f, Z), FVector(1.5f, W + 1.6f, 1.7f), FRotator::ZeroRotator, Glove);
		Add(S::Box, FVector(1.2f, -(W * 0.5f + 0.6f), Z), FVector(2.4f, 1.1f, 1.6f), FRotator::ZeroRotator, GloveGrip);
	}
	Add(S::Box, FVector(1.6f, W * 0.5f + 0.4f, 3.6f), FVector(4.2f, 1.1f, 1.3f), FRotator(0, 0, 0), Glove);          // index finger along frame
	Add(S::Box, FVector(0.8f, -(W * 0.5f + 0.7f), 3.9f), FVector(4.4f, 1.2f, 1.4f), FRotator(0, -6, 0), Glove);     // thumb
	Add(S::Box, FVector(-0.2f, W * 0.5f + 1.45f, 0.0f), FVector(3.0f, 0.2f, 2.0f), FRotator::ZeroRotator, Accent, 1.0f); // glove patch

	// Wrist + forearm direction: back toward the camera, down and slightly right.
	const FVector WristPos(-3.6f, 1.2f, -2.6f);
	const FVector Dir = FVector(-1.0f, 0.18f, -0.42f).GetSafeNormal();
	const FRotator Along = FRotationMatrix::MakeFromZ(Dir).Rotator();
	Add(S::Cylinder, WristPos + Dir * 1.0f, FVector(5.6f, 5.6f, 2.2f), Along, Ring, 0.25f, 0.28f, 1.0f);   // bearing ring
	for (int32 i = 0; i < 5; ++i)
	{
		const float D = i % 2 ? 5.8f : 6.5f;
		Add(S::Cylinder, WristPos + Dir * (2.8f + i * 1.2f), FVector(D, D, 0.9f), Along, Rubber, 0.05f, 0.98f); // ribbed rubber wrist seal
	}
	Add(S::Cylinder, WristPos + Dir * 8.8f, FVector(6.9f, 6.9f, 0.6f), Along, Ring, 0.25f, 0.28f, 1.0f);      // clamp ring
	Add(S::Cylinder, WristPos + Dir * 22.0f, FVector(7.6f, 7.6f, 26.0f), Along, Fabric);                      // sleeve
	Add(S::Cylinder, WristPos + Dir * 12.5f, FVector(7.8f, 7.8f, 1.2f), Along, Accent, 0.6f);                 // ID stripe
	Add(S::Box, WristPos + Dir * 16.0f + FVector(0.0f, 3.5f, 1.0f), FVector(5.0f, 0.6f, 2.4f), Along, FLinearColor(0.05f, 0.3f, 0.12f), 1.5f); // cuff status LED panel
}

void AMarsEquipment::OnEquipped(AMarsCharacter* NewOwner)
{
	OwnerCharacter = NewOwner;
	SetOwner(NewOwner);
	if (NewOwner && NewOwner->GetFirstPersonCamera())
	{
		AttachToComponent(NewOwner->GetFirstPersonCamera(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(HeldOffset);
		SetActorRelativeRotation(HeldRotation);
	}
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
}

void AMarsEquipment::OnUnequipped()
{
	StopPrimary();
	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);
}

void AMarsEquipment::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float AMarsEquipment::GetTremorDegrees() const
{
	const AMarsCharacter* Character = OwnerCharacter.Get();
	const UPlayerBiometricsComponent* Bio = Character ? Character->GetBiometrics() : nullptr;
	if (!Bio)
	{
		return 0.0f;
	}
	// Stage 2+ micro-tremor (hypoxia, adrenergic drive) degrades precision.
	return FMath::Max(Bio->GetStageIndex() - 1, 0) * 1.2f + Bio->PanicLevel * 1.5f;
}

bool AMarsEquipment::TraceFromView(float Range, FHitResult& OutHit, float SpreadDegrees) const
{
	const AMarsCharacter* Character = OwnerCharacter.Get();
	if (!Character || !Character->GetFirstPersonCamera())
	{
		return false;
	}
	const UCameraComponent* Camera = Character->GetFirstPersonCamera();
	FVector Direction = Camera->GetForwardVector();
	const float Spread = SpreadDegrees + GetTremorDegrees();
	if (Spread > 0.0f)
	{
		Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(Spread));
	}

	const FVector Start = Camera->GetComponentLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MarsEquipmentTrace), true);
	Params.AddIgnoredActor(Character);
	Params.AddIgnoredActor(this);
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, Start + Direction * Range, ECC_Visibility, Params);
}

AMarsBaseModule* AMarsEquipment::GetHitModule(const FHitResult& Hit)
{
	return Cast<AMarsBaseModule>(Hit.GetActor());
}

UMarsAudioManager* AMarsEquipment::GetAudio() const
{
	const AMarsCharacter* Character = OwnerCharacter.Get();
	return Character ? Character->GetAudioManager() : nullptr;
}

TArray<UStaticMeshComponent*> AMarsEquipment::FindTaggedParts(FName Tag) const
{
	TArray<UStaticMeshComponent*> Result;
	TArray<UStaticMeshComponent*> All;
	GetComponents<UStaticMeshComponent>(All);
	for (UStaticMeshComponent* C : All)
	{
		if (C->ComponentHasTag(Tag))
		{
			Result.Add(C);
		}
	}
	return Result;
}
