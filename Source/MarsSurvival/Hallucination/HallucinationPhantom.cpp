#include "Hallucination/HallucinationPhantom.h"

#include "Audio/MarsAudioManager.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "Props/MarsPropBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FLinearColor Skin(0.02f, 0.018f, 0.025f);
	const FLinearColor EyeColor(1.0f, 0.85f, 0.7f);
	constexpr float CapsuleRadius = 45.0f;
	constexpr float CapsuleHeight = 230.0f;
}

AHallucinationPhantom::AHallucinationPhantom()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AHallucinationPhantom::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildCreature();
}

void AHallucinationPhantom::BuildCreature()
{
	using S = EMarsPropShape;
	EyeParts.Reset();
	BodyParts.Reset();
	USceneComponent* Root = GetRootComponent();

	auto Part = [&](USceneComponent* Parent, S Shape, const FVector& Loc, const FVector& Size, const FRotator& Rot, const FLinearColor& Col, float Emissive)
	{
		UStaticMeshComponent* C = AMarsPropBuilder::AddPart(Parent, Shape, Loc, Size, Rot, Col, Emissive, false);
		if (C)
		{
			C->SetCastShadow(false);
			BodyParts.Add(C);
		}
		return C;
	};

	// Pivot at the feet so the whole body can topple over when shot.
	BodyPivot = AMarsPropBuilder::AddPivot(Root, FVector::ZeroVector, FRotator::ZeroRotator, NAME_None);

	// Digitigrade legs (thigh forward, shin back, long feet).
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		Part(BodyPivot, S::Cylinder, FVector(8, Side * 16.0f, 95), FVector(9, 9, 70), FRotator(-20, 0, 0), Skin, 0.02f);
		Part(BodyPivot, S::Cylinder, FVector(-4, Side * 16.0f, 40), FVector(6, 6, 70), FRotator(25, 0, 0), Skin, 0.02f);
		Part(BodyPivot, S::Box, FVector(10, Side * 16.0f, 4), FVector(32, 6, 4), FRotator::ZeroRotator, Skin, 0.02f);
	}
	// Hunched, too-long torso with a spine ridge.
	Part(BodyPivot, S::Cylinder, FVector(10, 0, 160), FVector(42, 30, 90), FRotator(-28, 0, 0), Skin, 0.03f);
	for (int32 i = 0; i < 6; ++i)
	{
		Part(BodyPivot, S::Cone, FVector(-14.0f + i * 5.0f, 0, 130.0f + i * 14.0f), FVector(5, 5, 9), FRotator(-60, 0, 0), Skin, 0.05f);
	}

	// Head: stretched skull, slack jaw, two small hot eyes.
	HeadPivot = AMarsPropBuilder::AddPivot(BodyPivot, FVector(32, 0, 210), FRotator(0, 0, 0), NAME_None);
	Part(HeadPivot, S::Sphere, FVector(6, 0, 6), FVector(26, 22, 34), FRotator(-15, 0, 0), Skin, 0.03f);
	Part(HeadPivot, S::Box, FVector(14, 0, -12), FVector(14, 14, 16), FRotator(20, 0, 0), Skin, 0.02f);
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		if (UStaticMeshComponent* Eye = Part(HeadPivot, S::Sphere, FVector(18, Side * 6.0f, 9), FVector(3.5f, 3.5f, 2.5f), FRotator::ZeroRotator, EyeColor, 6.0f))
		{
			EyeParts.Add(Eye);
		}
	}

	// Arms that hang far too low, ending in long fingers.
	ArmL = AMarsPropBuilder::AddPivot(BodyPivot, FVector(26, -26, 195), FRotator::ZeroRotator, NAME_None);
	ArmR = AMarsPropBuilder::AddPivot(BodyPivot, FVector(26, 26, 195), FRotator::ZeroRotator, NAME_None);
	for (USceneComponent* Arm : { ArmL.Get(), ArmR.Get() })
	{
		Part(Arm, S::Cylinder, FVector(4, 0, -45), FVector(7, 7, 90), FRotator(8, 0, 0), Skin, 0.02f);
		Part(Arm, S::Cylinder, FVector(12, 0, -125), FVector(5, 5, 80), FRotator(-6, 0, 0), Skin, 0.02f);
		for (int32 f = -2; f <= 2; ++f)
		{
			Part(Arm, S::Cylinder, FVector(14, f * 2.5f, -180), FVector(1.5f, 1.5f, 30), FRotator(-10, f * 6.0f, f * 6.0f), Skin, 0.04f);
		}
	}
}

void AHallucinationPhantom::InitPhantom(APawn* InViewer, float InIntensity)
{
	Viewer = InViewer;
	Intensity = FMath::Clamp(InIntensity, 0.0f, 1.0f);
	Lifetime = FMath::FRandRange(8.0f, 13.0f);
	const FVector Rand = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal();
	StalkDir = Rand.IsNearlyZero() ? FVector::ForwardVector : Rand;
}

void AHallucinationPhantom::SetGlow(float Eyes, float Body)
{
	for (UStaticMeshComponent* E : EyeParts)
	{
		if (UMaterialInstanceDynamic* MID = E ? Cast<UMaterialInstanceDynamic>(E->GetMaterial(0)) : nullptr)
		{
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), EyeColor * Eyes);
		}
	}
}

bool AHallucinationPhantom::IsViewerLookingAtMe(float& OutDistance) const
{
	const APawn* V = Viewer.Get();
	if (!V)
	{
		OutDistance = 1e9f;
		return false;
	}
	FVector Eye;
	FRotator View;
	V->GetActorEyesViewPoint(Eye, View);
	const FVector ToMe = GetActorLocation() + FVector(0, 0, 150.0f) - Eye;
	OutDistance = ToMe.Size();
	return FVector::DotProduct(View.Vector(), ToMe.GetSafeNormal()) > 0.94f;
}

void AHallucinationPhantom::InitPeeker(APawn* InViewer, float InIntensity, const FVector& LeanDirection)
{
	Viewer = InViewer;
	Intensity = FMath::Clamp(InIntensity, 0.0f, 1.0f);
	Lifetime = FMath::FRandRange(5.0f, 8.0f);
	LeanDir = LeanDirection.GetSafeNormal2D();
	PeekBase = GetActorLocation();
	State = EState::Peeking;
	StateTime = 0.0f;
}

bool AHallucinationPhantom::IsViewerLookingDirectly(float CosAngle) const
{
	const APawn* V = Viewer.Get();
	if (!V)
	{
		return false;
	}
	FVector Eye;
	FRotator View;
	V->GetActorEyesViewPoint(Eye, View);
	const FVector ToMe = (GetActorLocation() + FVector(0, 0, 150.0f) - Eye).GetSafeNormal();
	return FVector::DotProduct(View.Vector(), ToMe) > CosAngle;
}

bool AHallucinationPhantom::TestSegment(const FVector& Start, const FVector& End) const
{
	if (State == EState::Collapsing || State == EState::Vanishing)
	{
		return false;
	}
	// Closest distance between the bullet segment and the creature's vertical axis.
	const FVector A = GetActorLocation() + FVector(0, 0, 20.0f);
	const FVector B = GetActorLocation() + FVector(0, 0, CapsuleHeight);
	FVector P1, P2;
	FMath::SegmentDistToSegmentSafe(Start, End, A, B, P1, P2);
	return FVector::Dist(P1, P2) < CapsuleRadius;
}

void AHallucinationPhantom::Awaken()
{
	if (State == EState::Collapsing)
	{
		return;
	}
	State = EState::Collapsing;
	StateTime = 0.0f;
	SpawnAsh();

	if (AMarsCharacter* C = Cast<AMarsCharacter>(Viewer.Get()))
	{
		if (UPlayerBiometricsComponent* Bio = C->GetBiometrics())
		{
			Bio->ApplyAwakeningClarity();
		}
		if (UMarsAudioManager* Audio = C->GetAudioManager())
		{
			Audio->PlayCherenkovZap(1.0f);
		}
		C->ShowNotification(FText::FromString(TEXT("...there was nothing there.")), 3.0f);
	}
}

void AHallucinationPhantom::SpawnAsh()
{
	AshParts.Reset();
	AshVelocity.Reset();
	for (int32 i = 0; i < 46; ++i)
	{
		const FVector Pos(FMath::FRandRange(-25.0f, 30.0f), FMath::FRandRange(-25.0f, 25.0f), FMath::FRandRange(20.0f, 220.0f));
		const bool bStatic = (i % 5) == 0; // a few bright "static" specks among the ash
		UStaticMeshComponent* Ash = AMarsPropBuilder::AddPart(GetRootComponent(), EMarsPropShape::Box, Pos, FVector(FMath::FRandRange(2.0f, 6.0f)),
			FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), 0.0f),
			bStatic ? FLinearColor(0.7f, 0.85f, 1.0f) : FLinearColor(0.01f, 0.01f, 0.012f), bStatic ? 6.0f : 0.0f, false);
		if (Ash)
		{
			Ash->SetCastShadow(false);
			AshParts.Add(Ash);
			AshVelocity.Add(FVector(FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(20.0f, 140.0f)));
		}
	}
}

void AHallucinationPhantom::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Age += DeltaTime;
	StateTime += DeltaTime;

	APawn* V = Viewer.Get();
	if (!V)
	{
		Destroy();
		return;
	}
	const FVector ToViewer = V->GetActorLocation() - GetActorLocation();
	const FVector ToViewer2D = FVector(ToViewer.X, ToViewer.Y, 0.0f);
	float Distance = 0.0f;
	const bool bWatched = IsViewerLookingAtMe(Distance);

	// Idle animation: arms sway, head twitches (only while "alive").
	if (State != EState::Collapsing)
	{
		const float Sway = FMath::Sin(Age * 1.7f) * 9.0f;
		if (ArmL) { ArmL->SetRelativeRotation(FRotator(Sway, 0, -6.0f)); }
		if (ArmR) { ArmR->SetRelativeRotation(FRotator(-Sway, 0, 6.0f)); }
		if (HeadPivot)
		{
			const float Twitch = FMath::FRand() < 0.03f ? FMath::FRandRange(-25.0f, 25.0f) : 0.0f;
			HeadPivot->SetRelativeRotation(FRotator(0, 0, FMath::Sin(Age * 0.8f) * 8.0f + Twitch));
		}
	}

	switch (State)
	{
	case EState::Stalking:
		SetActorRotation(FRotator(0.0f, ToViewer2D.Rotation().Yaw, 0.0f));
		// Creeps toward the crew member with a slow sideways sway (stays in the corridor they are looking down).
		AddActorWorldOffset((ToViewer2D.GetSafeNormal() + StalkDir * 0.25f * FMath::Sin(Age * 0.7f)).GetClampedToMaxSize(1.0f) * StalkSpeed * DeltaTime * (0.5f + Intensity));
		SetGlow(4.0f, 0.0f);
		if (bWatched && Distance < 1600.0f)
		{
			State = EState::Staring;
			StateTime = 0.0f;
		}
		if (Distance < 220.0f || Age > Lifetime)
		{
			State = EState::Vanishing;
			StateTime = 0.0f;
		}
		break;

	case EState::Staring:
		// Frozen, eyes flaring - daring you to pull the trigger.
		SetActorRotation(FRotator(0.0f, ToViewer2D.Rotation().Yaw, 0.0f));
		SetGlow(6.0f + 10.0f * FMath::Clamp(StateTime / StareBeforeLungeSeconds, 0.0f, 1.0f), 0.0f);
		if (StateTime > StareBeforeLungeSeconds)
		{
			State = EState::Lunging;
			StateTime = 0.0f;
			if (const AMarsCharacter* C = Cast<AMarsCharacter>(V))
			{
				if (UMarsAudioManager* Audio = C->GetAudioManager())
				{
					Audio->PlayPhantomShriek(GetActorLocation());
				}
			}
		}
		break;

	case EState::Lunging:
		AddActorWorldOffset(ToViewer2D.GetSafeNormal() * LungeSpeed * DeltaTime);
		SetGlow(18.0f, 0.0f);
		if (ToViewer2D.Size() < 140.0f || StateTime > 1.5f)
		{
			if (AMarsCharacter* C = Cast<AMarsCharacter>(V))
			{
				if (UPlayerBiometricsComponent* Bio = C->GetBiometrics())
				{
					// It was never there - but advanced, untreated disease makes the body believe it.
					const float Severity = Bio->GetUntreatedSeverity();
					if (Intensity >= 0.55f || Severity >= 0.6f)
					{
						Bio->ApplyPsychosomaticShock(0.16f + 0.24f * Severity);
						C->ShowNotification(FText::FromString(TEXT("PSYCHOSOMATIC SHOCK - TREAT YOUR CONDITION (MED-BAY)")), 3.0f);
					}
					else
					{
						Bio->NotifyStartle(0.9f);
					}
				}
			}
			Destroy(); // it was never there
			return;
		}
		break;

	case EState::Peeking:
	{
		// Leans out from behind the jamb, watching; gone the instant you look straight at it or light it.
		SetActorRotation(FRotator(0.0f, ToViewer2D.Rotation().Yaw, 0.0f));
		const float Lean = FMath::Clamp(StateTime / 1.5f, 0.0f, 1.0f) * (28.0f + 6.0f * FMath::Sin(Age * 1.3f));
		SetActorLocation(PeekBase + LeanDir * Lean);
		SetGlow(4.0f + 3.0f * FMath::Sin(Age * 5.0f), 0.0f);
		bool bLit = false;
		if (const AMarsCharacter* C = Cast<AMarsCharacter>(V))
		{
			bLit = C->IsLampOn() && IsViewerLookingDirectly(0.9f);
		}
		if (IsViewerLookingDirectly(0.975f) || bLit || Distance < 350.0f || Age > Lifetime)
		{
			State = EState::Vanishing;
			StateTime = 0.0f;
		}
		break;
	}

	case EState::Vanishing:
		SetActorScale3D(FVector(1.0f, FMath::Max(1.0f - StateTime * 4.0f, 0.02f), 1.0f));
		if (StateTime > 0.25f)
		{
			Destroy();
			return;
		}
		break;

	case EState::Collapsing:
	{
		// Topple onto the deck (0.45 s), then crumble into ash/static (by 1.5 s).
		const float Fall = FMath::Clamp(StateTime / 0.45f, 0.0f, 1.0f);
		if (BodyPivot)
		{
			BodyPivot->SetRelativeRotation(FRotator(-88.0f * FMath::Square(Fall), 0.0f, 12.0f * Fall));
			const float Shrink = FMath::Clamp(1.0f - (StateTime - 0.45f) / 0.9f, 0.0f, 1.0f);
			BodyPivot->SetRelativeScale3D(FVector(Shrink > 0.0f ? 1.0f : 0.01f, Shrink, FMath::Max(Shrink, 0.01f)));
		}
		SetGlow(FMath::Max(8.0f * (1.0f - StateTime), 0.0f), 0.0f);
		for (int32 i = 0; i < AshParts.Num(); ++i)
		{
			if (UStaticMeshComponent* Ash = AshParts[i])
			{
				AshVelocity[i].Z -= 160.0f * DeltaTime;
				Ash->AddRelativeLocation(AshVelocity[i] * DeltaTime);
				Ash->SetRelativeScale3D(Ash->GetRelativeScale3D() * FMath::Max(1.0f - DeltaTime * 1.4f, 0.0f));
			}
		}
		if (StateTime > 1.5f)
		{
			Destroy();
			return;
		}
		break;
	}
	}
}
