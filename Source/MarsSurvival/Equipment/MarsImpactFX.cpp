#include "Equipment/MarsImpactFX.h"

#include "Props/MarsPropBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr int32 MaxBulletHoles = 48;
}

AMarsImpactFX::AMarsImpactFX()
{
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = 3.0f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AMarsImpactFX::Spawn(UWorld* World, const FVector& Location, const FVector& Normal, EMarsImpactType Type)
{
	if (!World)
	{
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AMarsImpactFX* FX = World->SpawnActor<AMarsImpactFX>(AMarsImpactFX::StaticClass(), Location + Normal * 2.0f, FRotator::ZeroRotator, P))
	{
		FX->Build(Normal, Type);
	}

	if (Type == EMarsImpactType::PhantomAsh || Type == EMarsImpactType::ThrusterDust)
	{
		return;
	}

	// Recycle the oldest hole once the cap is reached.
	TArray<AMarsBulletHole*> Holes;
	for (TActorIterator<AMarsBulletHole> It(World); It; ++It)
	{
		Holes.Add(*It);
	}
	if (Holes.Num() >= MaxBulletHoles)
	{
		Holes.Sort([](const AMarsBulletHole& A, const AMarsBulletHole& B) { return A.GetGameTimeSinceCreation() > B.GetGameTimeSinceCreation(); });
		Holes[0]->Destroy();
	}
	if (AMarsBulletHole* Hole = World->SpawnActor<AMarsBulletHole>(AMarsBulletHole::StaticClass(), Location + Normal * 0.4f, Normal.Rotation(), P))
	{
		Hole->Init(Normal, Type == EMarsImpactType::Metal);
	}
}

void AMarsImpactFX::Build(const FVector& Normal, EMarsImpactType Type)
{
	ImpactType = Type;
	if (Type == EMarsImpactType::ThrusterDust)
	{
		SetLifeSpan(3.0f);
	}
	int32 Count = 14;
	FLinearColor Color(1.0f, 0.55f, 0.15f);
	float Emissive = 6.0f;
	float Speed = 520.0f;
	float Size = 1.0f;
	float Life = 0.45f;
	switch (Type)
	{
	case EMarsImpactType::Metal:
		break;
	case EMarsImpactType::Regolith:
		Count = 10; Color = FLinearColor(0.45f, 0.24f, 0.13f); Emissive = 0.0f; Speed = 260.0f; Size = 1.8f; Life = 0.9f;
		break;
	case EMarsImpactType::PhantomAsh:
		Count = 26; Color = FLinearColor(0.16f, 0.16f, 0.17f); Emissive = 0.0f; Speed = 90.0f; Size = 2.4f; Life = 1.6f;
		Gravity = 60.0f; // ash drifts upward and dissipates
		break;
	case EMarsImpactType::ThrusterDust:
		Count = 18; Color = FLinearColor(0.55f, 0.30f, 0.17f); Emissive = 0.0f; Speed = 900.0f; Size = 22.0f; Life = 2.6f;
		Gravity = -40.0f;
		break;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Dir = (Normal + FMath::VRand() * (Type == EMarsImpactType::PhantomAsh ? 1.4f : 0.75f)).GetSafeNormal();
		const float S = Size * FMath::FRandRange(0.5f, 1.2f);
		const FVector Extent = Type == EMarsImpactType::Metal ? FVector(S * 2.2f, S * 0.5f, S * 0.5f) : FVector(S);
		UStaticMeshComponent* Mesh = AMarsPropBuilder::AddPart(GetRootComponent(), EMarsPropShape::Box, FVector::ZeroVector, Extent,
			FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), 0.0f), Color * FMath::FRandRange(0.7f, 1.1f), Emissive);
		if (!Mesh)
		{
			continue;
		}
		Mesh->SetCastShadow(false);
		FChip C;
		C.Mesh = Mesh;
		C.Velocity = Dir * Speed * FMath::FRandRange(0.4f, 1.0f);
		C.MaxLife = C.Life = Life * FMath::FRandRange(0.6f, 1.0f);
		C.BaseScale = Mesh->GetRelativeScale3D();
		Chips.Add(C);
	}
}

void AMarsImpactFX::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	for (FChip& C : Chips)
	{
		if (!C.Mesh || C.Life <= 0.0f)
		{
			continue;
		}
		C.Life -= DeltaTime;
		C.Velocity.Z += Gravity * DeltaTime;
		C.Velocity *= (ImpactType == EMarsImpactType::PhantomAsh || ImpactType == EMarsImpactType::ThrusterDust ? 0.97f : 0.995f);
		C.Mesh->AddRelativeLocation(C.Velocity * DeltaTime);
		if (ImpactType == EMarsImpactType::Metal)
		{
			// Sparks stretch along their velocity.
			C.Mesh->SetRelativeRotation(C.Velocity.Rotation());
		}
		const float K = FMath::Clamp(C.Life / C.MaxLife, 0.0f, 1.0f);
		C.Mesh->SetRelativeScale3D(C.BaseScale * K);
		if (C.Life <= 0.0f)
		{
			C.Mesh->SetVisibility(false);
		}
	}
}

// ----------------------------------------------------------------------------

AMarsBulletHole::AMarsBulletHole()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AMarsBulletHole::Init(const FVector& Normal, bool bMetal)
{
	bMetalHole = bMetal;
	USceneComponent* Root = GetRootComponent();
	// Root X axis = surface normal. Thin discs lie in the surface plane.
	const FRotator Flat(90.0f, 0.0f, 0.0f);
	AMarsPropBuilder::AddPart(Root, EMarsPropShape::Cylinder, FVector::ZeroVector, FVector(bMetal ? 5.5f : 7.0f, bMetal ? 5.5f : 7.0f, 0.2f), Flat,
		bMetal ? FLinearColor(0.09f, 0.08f, 0.07f) : FLinearColor(0.12f, 0.07f, 0.05f));
	AMarsPropBuilder::AddPart(Root, EMarsPropShape::Cylinder, FVector(0.15f, 0, 0), FVector(1.6f, 1.6f, 0.2f), Flat, FLinearColor(0.005f, 0.005f, 0.005f));
	if (bMetal)
	{
		// Hot rim that cools over a few seconds.
		Glow = AMarsPropBuilder::AddPart(Root, EMarsPropShape::Cylinder, FVector(0.1f, 0, 0), FVector(2.6f, 2.6f, 0.2f), Flat, FLinearColor(1.0f, 0.35f, 0.05f), 8.0f);
	}
	for (UActorComponent* C : GetComponents())
	{
		if (UStaticMeshComponent* M = Cast<UStaticMeshComponent>(C))
		{
			M->SetCastShadow(false);
		}
	}
}

void AMarsBulletHole::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Age += DeltaTime;
	if (Glow)
	{
		const float Heat = FMath::Clamp(1.0f - Age / 4.0f, 0.0f, 1.0f);
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Glow->GetMaterial(0)))
		{
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(1.0f, 0.35f, 0.05f) * 8.0f * Heat * Heat);
		}
		if (Heat <= 0.0f)
		{
			Glow->DestroyComponent();
			Glow = nullptr;
			SetActorTickEnabled(false);
		}
	}
	else
	{
		SetActorTickEnabled(false);
	}
}
