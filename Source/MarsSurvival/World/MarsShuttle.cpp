#include "World/MarsShuttle.h"

#include "Audio/MarsAudioManager.h"
#include "Equipment/MarsImpactFX.h"
#include "Player/MarsCharacter.h"
#include "Props/MarsPropBuilder.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

AMarsShuttle::AMarsShuttle()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	EngineGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("EngineGlow"));
	EngineGlow->SetupAttachment(GetRootComponent());
	EngineGlow->SetRelativeLocation(FVector(-1050.0f, 0.0f, 0.0f));
	EngineGlow->SetIntensityUnits(ELightUnits::Lumens);
	EngineGlow->SetIntensity(0.0f);
	EngineGlow->SetAttenuationRadius(6000.0f);
	EngineGlow->SetLightColor(FLinearColor(1.0f, 0.55f, 0.2f));
	EngineGlow->SetCastShadows(false);

	BeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLight"));
	BeaconLight->SetupAttachment(GetRootComponent());
	BeaconLight->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f));
	BeaconLight->SetIntensityUnits(ELightUnits::Lumens);
	BeaconLight->SetIntensity(2000.0f);
	BeaconLight->SetAttenuationRadius(1500.0f);
	BeaconLight->SetLightColor(FLinearColor(1.0f, 0.1f, 0.05f));
	BeaconLight->SetCastShadows(false);
}

void AMarsShuttle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AMarsPropBuilder::BuildProp(GetRootComponent(), EMarsPropKind::Shuttle, 1.0f, false);
}

void AMarsShuttle::StartDeparture()
{
	if (bDeparting)
	{
		return;
	}
	bDeparting = true;
	Time = 0.0f;
	StartLocation = GetActorLocation();
	StartRotation = GetActorRotation();
	if (const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (UMarsAudioManager* Audio = Player->GetAudioManager())
		{
			Audio->PlayShuttleRumble(34.0f);
		}
	}
}

namespace
{
	float Smooth01(float X)
	{
		X = FMath::Clamp(X, 0.0f, 1.0f);
		return X * X * (3.0f - 2.0f * X);
	}

	FVector Bezier(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float U)
	{
		const float V = 1.0f - U;
		return P0 * (V * V * V) + P1 * (3.0f * V * V * U) + P2 * (3.0f * V * U * U) + P3 * (U * U * U);
	}

	FVector BezierTangent(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float U)
	{
		const float V = 1.0f - U;
		return (P1 - P0) * (3.0f * V * V) + (P2 - P1) * (6.0f * V * U) + (P3 - P2) * (3.0f * U * U);
	}
}

void AMarsShuttle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	BeaconLight->SetVisibility(FMath::Frac(GetWorld()->GetTimeSeconds() * 0.8f) < 0.15f);
	if (!bDeparting)
	{
		return;
	}
	Time += DeltaTime;

	// Timeline (seconds):
	//   0 - 2   cold, beacon only
	//   2 - 6   ignition + spool: exhaust grows, regolith blasts outward from the pad
	//   6 - 14  VTOL hover lift to ~9 m with small attitude corrections
	//   14 - 45 transition to forward flight along a cubic Bezier: nose dips to accelerate,
	//           then a climbing, banking turn out over the horizon
	constexpr float IgniteAt = 2.0f, LiftAt = 6.0f, CruiseAt = 14.0f, EndAt = 45.0f;
	const float Spool = Smooth01((Time - IgniteAt) / (LiftAt - IgniteAt));
	const float Flicker = 0.88f + 0.12f * FMath::FRand();
	const float Throttle = Spool * (Time < CruiseAt ? 0.8f : 1.0f);
	EngineGlow->SetIntensity(250000.0f * Throttle * Flicker);
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* M : Meshes)
	{
		if (M->ComponentHasTag(TEXT("Thrust")))
		{
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
			{
				MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(1.0f, 0.55f, 0.15f) * 40.0f * Throttle * Flicker);
			}
			M->SetRelativeScale3D(FVector(0.8f + 1.8f * Throttle * Flicker, 0.8f, 0.8f));
		}
	}

	const FVector Fwd = StartRotation.Vector();
	const FVector Right = FRotationMatrix(StartRotation).GetUnitAxis(EAxis::Y);
	const FVector Up = FVector::UpVector;
	const float HoverHeight = 900.0f;
	const FVector HoverPoint = StartLocation + Up * HoverHeight;

	// Ground effect: dust blown radially out from under the engines while low.
	const float Height = GetActorLocation().Z - StartLocation.Z;
	if (Spool > 0.2f && Height < 2500.0f)
	{
		DustAccumulator += DeltaTime * 10.0f * Spool * (1.0f - Height / 2500.0f);
		while (DustAccumulator >= 1.0f)
		{
			DustAccumulator -= 1.0f;
			const float A = FMath::FRandRange(0.0f, 2.0f * PI);
			const FVector Radial(FMath::Cos(A), FMath::Sin(A), 0.0f);
			AMarsImpactFX::Spawn(GetWorld(), FVector(StartLocation.X, StartLocation.Y, StartLocation.Z - 240.0f) + Radial * FMath::FRandRange(200.0f, 900.0f),
				(Radial + FVector(0, 0, 0.25f)).GetSafeNormal(), EMarsImpactType::ThrusterDust);
		}
	}

	FVector Location = StartLocation;
	FRotator Rotation = StartRotation;
	if (Time >= LiftAt && Time < CruiseAt)
	{
		// Hover lift: S-curve height, tiny RCS wobble.
		const float T = Smooth01((Time - LiftAt) / (CruiseAt - LiftAt));
		const float Wobble = FMath::Sin(Time * 1.7f) * (1.0f - T) * 0.5f + FMath::Sin(Time * 0.9f) * 0.4f;
		Location = StartLocation + Up * HoverHeight * T + Right * FMath::Sin(Time * 0.6f) * 20.0f * T;
		Rotation = StartRotation + FRotator(Wobble * 0.6f, 0.0f, Wobble);
	}
	else if (Time >= CruiseAt)
	{
		const FVector P0 = HoverPoint;
		const FVector P1 = HoverPoint + Fwd * 900.0f + Up * 150.0f;
		const FVector P2 = HoverPoint + Fwd * 9000.0f + Right * 2500.0f + Up * 2200.0f;
		const FVector P3 = HoverPoint + Fwd * 42000.0f + Right * 26000.0f + Up * 9000.0f;
		// Accelerating parameter: gentle at first, then a full burn.
		const float T = FMath::Clamp((Time - CruiseAt) / (EndAt - CruiseAt), 0.0f, 1.0f);
		const float U = FMath::Pow(T, 1.7f);
		Location = Bezier(P0, P1, P2, P3, U);
		const FVector Tangent = BezierTangent(P0, P1, P2, P3, FMath::Max(U, 0.002f)).GetSafeNormal();

		// Heading follows the path; pitch: nose down to accelerate, then up into the climb.
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X));
		const float Climb = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Tangent.Z, -1.0f, 1.0f)));
		const float NoseDown = -9.0f * Smooth01(T / 0.08f) * (1.0f - Smooth01((T - 0.08f) / 0.15f));
		const float TargetPitch = NoseDown + Climb * 0.8f * Smooth01((T - 0.06f) / 0.2f);
		const float YawRate = FRotator::NormalizeAxis(Yaw - LastYaw) / FMath::Max(DeltaTime, 0.001f);
		LastYaw = Yaw;
		Bank = FMath::FInterpTo(Bank, FMath::Clamp(YawRate * 2.2f, -38.0f, 38.0f), DeltaTime, 2.0f);
		Rotation = FRotator(TargetPitch, FMath::Lerp(StartRotation.Yaw, Yaw, Smooth01(T / 0.05f)), Bank);

		if (!bAnnounced && Time > CruiseAt + 4.0f)
		{
			bAnnounced = true;
			OnDeparted.Broadcast();
		}
	}
	if (Time < CruiseAt)
	{
		LastYaw = StartRotation.Yaw;
	}
	SetActorLocationAndRotation(Location, Rotation);

	if (Time > EndAt + 2.0f)
	{
		Destroy();
	}
}
