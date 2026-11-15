#include "Vehicles/MarsReconShuttle.h"

#include "Audio/MarsAudioManager.h"
#include "Core/MarsGameMode.h"
#include "Core/MarsHUD.h"
#include "Core/MarsNarrativeDirector.h"
#include "Equipment/MarsImpactFX.h"
#include "World/MarsSolarStormManager.h"
#include "Core/MarsMissionDirector.h"
#include "Player/EVASuitComponent.h"
#include "Player/MarsCharacter.h"
#include "Props/MarsPropBuilder.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr float HullHalfHeight = 90.0f;
	constexpr float GearClearance = 190.0f;   // actor origin above ground when standing on the gear
	constexpr float MarsG = 371.0f;            // cm/s^2
}

AMarsReconShuttle::AMarsReconShuttle()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	Hull = CreateDefaultSubobject<UBoxComponent>(TEXT("Hull"));
	Hull->SetBoxExtent(FVector(330.0f, 300.0f, HullHalfHeight));
	Hull->SetCollisionProfileName(TEXT("BlockAll"));
	Hull->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(Hull);

	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Hull);

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(Hull);
	Boom->TargetArmLength = 1500.0f;
	Boom->SocketOffset = FVector(0.0f, 0.0f, 280.0f);
	Boom->bUsePawnControlRotation = false;
	Boom->bInheritPitch = false;
	Boom->bInheritRoll = false;
	Boom->bInheritYaw = false;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 5.0f;
	Boom->bEnableCameraRotationLag = true;
	Boom->CameraRotationLagSpeed = 9.0f;
	Boom->CameraLagMaxDistance = 800.0f;
	Boom->bDoCollisionTest = true;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(Boom);
	ChaseCamera->SetFieldOfView(85.0f);

	Floodlight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Floodlight"));
	Floodlight->SetupAttachment(Hull);
	Floodlight->SetRelativeLocation(FVector(380.0f, 0.0f, -120.0f));
	Floodlight->SetIntensityUnits(ELightUnits::Lumens);
	Floodlight->SetIntensity(9000.0f);
	Floodlight->SetAttenuationRadius(3500.0f);
	Floodlight->SetLightColor(FLinearColor(0.85f, 0.92f, 1.0f));
	Floodlight->SetCastShadows(false);
}

void AMarsReconShuttle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AMarsPropBuilder::BuildProp(Visual, EMarsPropKind::ReconShuttle, 1.0f, false);
}

void AMarsReconShuttle::BeginPlay()
{
	Super::BeginPlay();
	Yaw = GetActorRotation().Yaw;
	SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
	if (DockLocation.IsNearlyZero())
	{
		DockLocation = GetActorLocation();
	}
	// Settle on the gear.
	const float Ground = GroundHeightBelow(GetActorLocation());
	if (Ground > -1e6f)
	{
		SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Ground + GearClearance));
	}
}

float AMarsReconShuttle::GroundHeightBelow(const FVector& At) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ReconGround), false, this);
	if (Pilot.IsValid())
	{
		Params.AddIgnoredActor(Pilot.Get());
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 200.0f), At - FVector(0, 0, 200000.0f), ECC_WorldStatic, Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return -1e7f;
}

void AMarsReconShuttle::SetupPlayerInputComponent(UInputComponent* In)
{
	Super::SetupPlayerInputComponent(In);
	In->BindAxis(TEXT("MoveForward"), this, &AMarsReconShuttle::MoveForwardInput);
	In->BindAxis(TEXT("MoveRight"), this, &AMarsReconShuttle::MoveRightInput);
	In->BindAxis(TEXT("Turn"), this, &AMarsReconShuttle::TurnInput);
	In->BindAxis(TEXT("LookUp"), this, &AMarsReconShuttle::LookUpInput);
	In->BindAction(TEXT("Jump"), IE_Pressed, this, &AMarsReconShuttle::ClimbPressed);
	In->BindAction(TEXT("Jump"), IE_Released, this, &AMarsReconShuttle::ClimbReleased);
	In->BindAction(TEXT("Crouch"), IE_Pressed, this, &AMarsReconShuttle::DescendPressed);
	In->BindAction(TEXT("Crouch"), IE_Released, this, &AMarsReconShuttle::DescendReleased);
	In->BindAction(TEXT("Sprint"), IE_Pressed, this, &AMarsReconShuttle::BoostPressed);
	In->BindAction(TEXT("Sprint"), IE_Released, this, &AMarsReconShuttle::BoostReleased);
	In->BindAction(TEXT("Interact"), IE_Pressed, this, &AMarsReconShuttle::ExitPressed);
	In->BindAction(TEXT("LandingGear"), IE_Pressed, this, &AMarsReconShuttle::ToggleGear);
	In->BindAction(TEXT("Reload"), IE_Pressed, this, &AMarsReconShuttle::RepairPressed);
	In->BindAction(TEXT("Reload"), IE_Released, this, &AMarsReconShuttle::RepairReleased);
	In->BindAction(TEXT("ToggleHUD"), IE_Pressed, this, &AMarsReconShuttle::ToggleHUDPressed);
	FInputActionBinding& Pause = In->BindAction(TEXT("Pause"), IE_Pressed, this, &AMarsReconShuttle::PausePressed);
	Pause.bExecuteWhenPaused = true;
}

void AMarsReconShuttle::PausePressed()
{
	if (AMarsGameMode* GM = GetWorld()->GetAuthGameMode<AMarsGameMode>())
	{
		GM->TogglePauseMenu();
	}
}

void AMarsReconShuttle::ToggleHUDPressed()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AMarsHUD* Hud = Cast<AMarsHUD>(PC->GetHUD()))
		{
			Hud->ToggleMinimalHUD();
		}
	}
}

void AMarsReconShuttle::ToggleGear()
{
	if (bLanded)
	{
		return; // can't retract while standing on it
	}
	bGearDown = !bGearDown;
	if (AMarsCharacter* P = Pilot.Get())
	{
		P->ShowNotification(FText::FromString(bGearDown ? TEXT("LANDING GEAR DOWN") : TEXT("LANDING GEAR UP")), 2.0f);
		if (UMarsAudioManager* Audio = P->GetAudioManager())
		{
			Audio->PlayRatchet();
		}
	}
}

// ----------------------------------------------------------------------------
// Boarding
// ----------------------------------------------------------------------------

void AMarsReconShuttle::Interact_Implementation(AMarsCharacter* Character)
{
	Board(Character);
}

FText AMarsReconShuttle::GetInteractionPrompt_Implementation(AMarsCharacter* Character) const
{
	return FText::FromString(FString::Printf(TEXT("[E] Board recon shuttle RS-2   (battery %.0f %%)"), GetBattery01() * 100.0f));
}

void AMarsReconShuttle::Board(AMarsCharacter* Character)
{
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PC || Pilot.IsValid() || Character->IsDead())
	{
		return;
	}
	if (!Character->GetSuit() || !Character->GetSuit()->IsSuitDonned())
	{
		Character->ShowNotification(FText::FromString(TEXT("SUIT UP FIRST - THE COCKPIT IS UNPRESSURISED UNTIL SEALED")), 3.0f);
		return;
	}
	Pilot = Character;
	Character->Holster();
	Character->GetCharacterMovement()->DisableMovement();
	Character->SetActorEnableCollision(false);
	Character->SetActorHiddenInGame(true);
	Character->AttachToComponent(Visual, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Character->SetActorRelativeLocation(FVector(220.0f, 0.0f, 40.0f));
	PC->Possess(this);
	PC->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	CameraPitch = -12.0f;
	if (UMarsAudioManager* Audio = Character->GetAudioManager())
	{
		Audio->PlayAirlockHiss(GetActorLocation(), 1.5f);
	}
	UMarsNarrativeDirector::Report(this, TEXT("ShuttleBoarded"));
}

bool AMarsReconShuttle::TryExit()
{
	AMarsCharacter* Character = Pilot.Get();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!Character || !PC)
	{
		return false;
	}
	if (bInOrbit)
	{
		Character->ShowNotification(FText::FromString(TEXT("YOU ARE IN ORBIT - HOLD C TO DE-ORBIT FIRST")), 2.5f);
		return false;
	}
	if (!bLanded && (AltitudeCm > 250.0f || Velocity.Size() > 300.0f))
	{
		Character->ShowNotification(FText::FromString(TEXT("LAND FIRST (gear down, SPACE / C to climb / descend)")), 2.5f);
		return false;
	}
	const FVector Right = FRotationMatrix(FRotator(0.0f, Yaw, 0.0f)).GetUnitAxis(EAxis::Y);
	FVector Out = GetActorLocation() + Right * 480.0f;
	const float Ground = GroundHeightBelow(Out);
	Out.Z = (Ground > -1e6f ? Ground : GetActorLocation().Z - GearClearance) + 110.0f;

	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorLocationAndRotation(Out, FRotator(0.0f, Yaw + 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Character->SetActorHiddenInGame(false);
	Character->SetActorEnableCollision(true);
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	PC->Possess(Character);
	PC->SetControlRotation(FRotator(0.0f, Yaw + 90.0f, 0.0f));
	Pilot.Reset();
	InForward = InRight = InClimb = InDescend = 0.0f;
	bBoost = false;
	UMarsNarrativeDirector::Report(this, TEXT("ShuttleExited"));
	return true;
}

// ----------------------------------------------------------------------------
// Flight
// ----------------------------------------------------------------------------

void AMarsReconShuttle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (DeltaTime <= 0.0f)
	{
		return;
	}
	UpdateFlight(DeltaTime);
	UpdateSystems(DeltaTime);
	UpdateVisuals(DeltaTime);
}

void AMarsReconShuttle::EnterOrbit(const FVector& Where)
{
	bInOrbit = true;
	bLanded = false;
	bDocked = false;
	Velocity = FRotator(0.0f, Yaw, 0.0f).Vector() * 2000.0f;
	SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
	BurnHold = 0.0f;
}

void AMarsReconShuttle::LeaveOrbit(const FVector& Where)
{
	bInOrbit = false;
	bLanded = false;
	Velocity = FVector::ZeroVector;
	SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
	BurnHold = 0.0f;
}

void AMarsReconShuttle::UpdateOrbitFlight(float DeltaTime)
{
	// Zero-g, inertial flight with gentle dampers so it stays controllable.
	const bool bPiloted = Pilot.IsValid() && GetController() != nullptr;
	const bool bPower = BatteryWh > 0.0f;
	const UMarsMissionDirector* Missions = UMarsMissionDirector::Get(this);
	const EMarsMalfunction Fault = Missions ? Missions->GetMalfunction() : EMarsMalfunction::None;
	if (bPiloted)
	{
		const bool bMouse = !FMath::IsNearlyZero(InYaw) || !FMath::IsNearlyZero(InLook);
		CameraYawOffset = FRotator::NormalizeAxis(CameraYawOffset + InYaw * 1.2f);
		CameraPitch = FMath::Clamp(CameraPitch - InLook * 1.0f, -85.0f, 85.0f);
		LookIdle = bMouse ? 0.0f : LookIdle + DeltaTime;
		YawRate = FMath::FInterpTo(YawRate, InRight * 50.0f + (Fault == EMarsMalfunction::ThrusterStuck ? 22.0f : 0.0f), DeltaTime, 2.0f);
		Yaw += YawRate * DeltaTime;
	}
	InYaw = 0.0f;
	InLook = 0.0f;
	const FVector Fwd = FRotator(0.0f, Yaw, 0.0f).Vector();
	const float Forward = bPiloted && bPower ? InForward : 0.0f;
	const float Vertical = bPiloted && bPower ? (InClimb - InDescend) : 0.0f;
	const FVector Accel = Fwd * Forward * (bBoost ? 3200.0f : 1800.0f) + FVector::UpVector * Vertical * 1200.0f;
	Velocity += Accel * DeltaTime;
	Velocity *= FMath::Exp(-0.25f * DeltaTime);
	Velocity = Velocity.GetClampedToMaxSize(bBoost ? 22000.0f : 15000.0f);
	ThrustLevel = FMath::FInterpTo(ThrustLevel, bPower ? FMath::Clamp(0.2f + Accel.Size() / 3200.0f, 0.0f, 1.0f) : 0.0f, DeltaTime, 3.0f);
	FHitResult Hit;
	SetActorLocation(GetActorLocation() + Velocity * DeltaTime, true, &Hit);
	if (Hit.bBlockingHit)
	{
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.ImpactNormal) * 0.3f;
	}
	VisualPitch = FMath::FInterpTo(VisualPitch, -Forward * 10.0f, DeltaTime, 2.0f);
	VisualRoll = FMath::FInterpTo(VisualRoll, YawRate / 50.0f * 20.0f, DeltaTime, 2.0f);
	SetActorRotation(FRotator(VisualPitch, Yaw, VisualRoll));
	AltitudeCm = 12000000.0f;

	// Hold C (descend) 3 s: de-orbit burn.
	if (bPiloted && InDescend > 0.0f && InClimb <= 0.0f)
	{
		BurnHold += DeltaTime;
		if (BurnHold >= 3.0f && Missions)
		{
			BurnHold = 0.0f;
			UMarsMissionDirector::Get(this)->RequestDeorbit(this);
		}
	}
	else
	{
		BurnHold = 0.0f;
	}
}

void AMarsReconShuttle::UpdateFlight(float DeltaTime)
{
	if (bInOrbit)
	{
		UpdateOrbitFlight(DeltaTime);
		return;
	}
	const bool bPiloted = Pilot.IsValid() && GetController() != nullptr;
	// Solar storm: avionics brown-outs make the thrusters cut out in bursts.
	const UMarsSolarStormManager* StormNow = UMarsSolarStormManager::Get(this);
	const bool bStormGlitch = StormNow && StormNow->IsActive() && !bLanded && FMath::Frac(GetWorld()->GetTimeSeconds() * 0.37f) < 0.18f;
	const bool bPower = BatteryWh > 0.0f && !bStormGlitch;

	// Dev capture: "-AutoOrbit" jumps straight into orbital op 1 two seconds after boarding.
	if (bPiloted && !bInOrbit && FParse::Param(FCommandLine::Get(), TEXT("AutoOrbit")))
	{
		AutoFlyTime += DeltaTime;
		if (AutoFlyTime > 2.0f && AutoFlyTime - DeltaTime <= 2.0f)
		{
			if (UMarsMissionDirector* Ops = UMarsMissionDirector::Get(this))
			{
				Ops->DevForceOrbit(this);
			}
		}
	}

	// Dev capture: "-AutoFly" climbs for 5 s, then cruises forward.
	if (bPiloted && FParse::Param(FCommandLine::Get(), TEXT("AutoFly")))
	{
		AutoFlyTime += DeltaTime;
		InClimb = AutoFlyTime < 5.0f ? 1.0f : 0.0f;
		InForward = AutoFlyTime > 4.0f ? 1.0f : 0.0f;
		InRight = AutoFlyTime > 9.0f && AutoFlyTime < 11.0f ? 1.0f : 0.0f;
	}

	// Mouse = free-look: orbit the camera 360 degrees around the craft (independent of the heading).
	// A/D = yaw (turn), smoothed into a turn rate. The view eases back behind the craft only when
	// you fly forward and leave the mouse alone for a few seconds.
	if (bPiloted)
	{
		const bool bMouse = !FMath::IsNearlyZero(InYaw) || !FMath::IsNearlyZero(InLook);
		CameraYawOffset = FRotator::NormalizeAxis(CameraYawOffset + InYaw * 1.2f);
		CameraPitch = FMath::Clamp(CameraPitch - InLook * 1.0f, -75.0f, 35.0f);
		LookIdle = bMouse ? 0.0f : LookIdle + DeltaTime;
		if (LookIdle > 2.5f && FMath::Abs(InForward) > 0.1f)
		{
			CameraYawOffset = FMath::FInterpTo(CameraYawOffset, 0.0f, DeltaTime, 1.2f);
		}
		YawRate = FMath::FInterpTo(YawRate, InRight * (bBoost ? 55.0f : 70.0f), DeltaTime, 3.0f);
		Yaw += YawRate * DeltaTime;
	}
	else
	{
		YawRate = FMath::FInterpTo(YawRate, 0.0f, DeltaTime, 3.0f);
	}
	InYaw = 0.0f;
	InLook = 0.0f;

	const FRotator Heading(0.0f, Yaw, 0.0f);
	const FVector Fwd = Heading.Vector();
	const FVector Right = FRotationMatrix(Heading).GetUnitAxis(EAxis::Y);
	const float Forward = bPiloted ? InForward : 0.0f;
	const float Strafe = 0.0f; // A/D now yaw; banking is visual
	const float Vertical = bPiloted ? (InClimb - InDescend) : 0.0f;

	if (bLanded)
	{
		Velocity = FVector::ZeroVector;
		ThrustLevel = FMath::FInterpTo(ThrustLevel, bPiloted && bPower ? 0.12f : 0.0f, DeltaTime, 2.0f);
		if (bPiloted)
		{
			// Allow pivoting on the pad (helps lining up a take-off).
			Yaw += YawRate * DeltaTime * 0.5f;
		}
		if (bPiloted && bPower && Vertical > 0.0f)
		{
			bLanded = false;
			bDocked = false;
			Velocity.Z = 150.0f;
		}
		else
		{
			SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
			return;
		}
	}

	// Horizontal: VTOL tilts its thrust vector; hover assist trims vertical speed.
	const float Accel = bPower ? (bBoost ? 2100.0f : 1300.0f) : 0.0f;
	FVector HAccel = (Fwd * Forward + Right * Strafe * 0.75f) * Accel;
	FVector HVel(Velocity.X, Velocity.Y, 0.0f);
	HVel += HAccel * DeltaTime;
	// Coordinated turn: velocity bleeds toward the heading so the craft carves instead of drifting sideways.
	const float Along = FVector::DotProduct(HVel, Fwd);
	HVel = FMath::Lerp(HVel, Fwd * Along, FMath::Clamp(DeltaTime * 1.5f, 0.0f, 1.0f));
	HVel *= FMath::Exp(-(FMath::IsNearlyZero(Forward) ? 0.8f : 0.2f) * DeltaTime);
	const float MaxH = MaxSpeedCmS * (bBoost ? 1.4f : 1.0f);
	if (HVel.Size() > MaxH)
	{
		HVel = HVel.GetSafeNormal() * MaxH;
	}

	float Vz = Velocity.Z;
	if (bPower)
	{
		const float HighUp = FMath::Clamp((AltitudeCm - 15000.0f) / 30000.0f, 0.0f, 1.0f);
		const float TargetVz = Vertical * (bBoost ? 1400.0f : 900.0f) * (1.0f + 3.0f * HighUp * (Vertical > 0.0f ? 1.0f : 0.6f));
		Vz = FMath::FInterpConstantTo(Vz, TargetVz, DeltaTime, 1100.0f);
	}
	else
	{
		Vz -= MarsG * DeltaTime; // dead stick: Mars pulls you down at 0.38 g
	}
	Velocity = FVector(HVel.X, HVel.Y, Vz);

	const float Load = FMath::Clamp(0.45f + HAccel.Size() / 2500.0f + FMath::Max(Vertical, 0.0f) * 0.35f, 0.0f, 1.0f);
	ThrustLevel = FMath::FInterpTo(ThrustLevel, bPower ? Load : 0.0f, DeltaTime, 3.0f);

	// Move with collision.
	const FVector Start = GetActorLocation();
	FVector Target = Start + Velocity * DeltaTime;
	const float Ground = GroundHeightBelow(Target);
	const float Clearance = bGearDown ? GearClearance : HullHalfHeight + 10.0f;
	AltitudeCm = Ground > -1e6f ? FMath::Max(Target.Z - Clearance - Ground, 0.0f) : 100000.0f;

	if (Ground > -1e6f && Target.Z - Clearance <= Ground)
	{
		// Touchdown.
		Target.Z = Ground + Clearance;
		const float ImpactSpeed = -Velocity.Z;
		if (bGearDown && ImpactSpeed < 650.0f && HVel.Size() < 900.0f)
		{
			bLanded = true;
			bDocked = FVector::Dist2D(Target, DockLocation) < 2500.0f;
			if (AMarsCharacter* P = Pilot.Get())
			{
				P->ShowNotification(FText::FromString(bDocked ? TEXT("DOCKED AT ARES-IV PAD - CHARGING") : TEXT("TOUCHDOWN - E TO DISEMBARK")), 2.5f);
			}
		}
		else if (ImpactSpeed > 50.0f)
		{
			// Belly / hard landing: scrape to a stop and complain.
			if (AMarsCharacter* P = Pilot.Get())
			{
				P->ShowNotification(FText::FromString(bGearDown ? TEXT("HARD LANDING - SLOW YOUR DESCENT") : TEXT("BELLY CONTACT - LOWER THE GEAR (L)")), 2.0f);
			}
			BatteryWh = FMath::Max(BatteryWh - 120.0f, 0.0f);
		}
		Velocity.Z = 0.0f;
		Velocity *= bLanded ? 0.0f : 0.6f;
	}

	// Above the burn altitude: hold SPACE 3 s to burn for orbit (mission director decides if there is tasking).
	if (bPiloted && bPower && AltitudeCm > OrbitBurnAltitudeM * 100.0f && InClimb > 0.0f)
	{
		BurnHold += DeltaTime;
		if (BurnHold >= 3.0f)
		{
			BurnHold = 0.0f;
			if (UMarsMissionDirector* Missions = UMarsMissionDirector::Get(this))
			{
				Missions->RequestOrbitInsertion(this);
			}
		}
	}
	else
	{
		BurnHold = 0.0f;
	}

	FHitResult Hit;
	SetActorLocation(Target, true, &Hit);
	if (Hit.bBlockingHit)
	{
		// Slide along whatever we touched (rock spires, station walls).
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.ImpactNormal) * 0.5f;
	}

	// Body attitude follows the thrust vector (visual + chase camera).
	// Smooth body attitude: nose dips with forward thrust, banks into turns in proportion to speed.
	const float SpeedFrac = FMath::Clamp(HVel.Size() / MaxSpeedCmS, 0.0f, 1.0f);
	VisualPitch = FMath::FInterpTo(VisualPitch, -Forward * 12.0f * (bPower ? 1.0f : 0.0f), DeltaTime, 2.2f);
	VisualRoll = FMath::FInterpTo(VisualRoll, YawRate / 70.0f * (8.0f + 22.0f * SpeedFrac), DeltaTime, 2.2f);
	SetActorRotation(FRotator(VisualPitch, Yaw, VisualRoll));
}

void AMarsReconShuttle::UpdateSystems(float DeltaTime)
{
	// Battery: avionics + thrust; recharges on the base pad.
	if (bDocked && bLanded)
	{
		BatteryWh = FMath::Min(BatteryWh + 60.0f * DeltaTime, BatteryCapacityWh);
		O2ReserveLiters = FMath::Min(O2ReserveLiters + 20.0f * DeltaTime, O2ReserveCapacity);
	}
	else
	{
		const UMarsSolarStormManager* SolarStorm = UMarsSolarStormManager::Get(this);
		const UMarsMissionDirector* Missions = UMarsMissionDirector::Get(this);
		const EMarsMalfunction Fault = Missions ? Missions->GetMalfunction() : EMarsMalfunction::None;
		if (Fault == EMarsMalfunction::CoolantLeak)
		{
			O2ReserveLiters = FMath::Max(O2ReserveLiters - 18.0f * DeltaTime, 0.0f);
		}
		const float StormDrain = (SolarStorm && SolarStorm->IsActive() ? 3.0f : 1.0f) * (Fault == EMarsMalfunction::PowerFault ? 3.0f : 1.0f);
		BatteryWh = FMath::Max(BatteryWh - (0.5f + 6.5f * ThrustLevel) * DeltaTime * StormDrain, 0.0f);
	}

	// Pressurised cabin: top up the pilot's suit O2 and suit battery.
	if (AMarsCharacter* P = Pilot.Get())
	{
		if (UEVASuitComponent* Suit = P->GetSuit())
		{
			if (O2ReserveLiters > 0.0f)
			{
				O2ReserveLiters -= Suit->RefillO2(FMath::Min(15.0f * DeltaTime, O2ReserveLiters));
			}
			if (BatteryWh > 200.0f && Suit->BatteryWh < Suit->BatteryCapacityWh)
			{
				const float Wh = FMath::Min(8.0f * DeltaTime, Suit->BatteryCapacityWh - Suit->BatteryWh);
				Suit->BatteryWh += Wh;
				BatteryWh -= Wh;
			}
		}
		WarnCooldown -= DeltaTime;
		if (WarnCooldown <= 0.0f && GetBattery01() < 0.15f && !bDocked)
		{
			WarnCooldown = 20.0f;
			P->ShowNotification(FText::FromString(GetBattery01() <= 0.0f ? TEXT("BATTERY DEAD - THRUSTERS OFFLINE") : TEXT("SHUTTLE BATTERY LOW - RETURN TO THE ARES-IV PAD")), 3.0f);
		}
	}

	// Chase camera.
	Boom->SetWorldRotation(FRotator(CameraPitch, Yaw + CameraYawOffset, 0.0f));
}

void AMarsReconShuttle::UpdateVisuals(float DeltaTime)
{
	GearBlend = FMath::FInterpTo(GearBlend, bGearDown ? 1.0f : 0.0f, DeltaTime, 3.0f);
	const float Flicker = 0.85f + 0.15f * FMath::FRand();
	TArray<USceneComponent*> Comps;
	Visual->GetChildrenComponents(true, Comps);
	const float Spin = DeltaTime * (200.0f + 2600.0f * ThrustLevel);
	for (USceneComponent* C : Comps)
	{
		if (C->ComponentHasTag(TEXT("Fan")))
		{
			C->AddRelativeRotation(FRotator(0.0f, Spin, 0.0f));
		}
		else if (C->ComponentHasTag(TEXT("Gear")))
		{
			// Legs fold up into the fan pods (pivot at the top of the strut).
			C->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f) + FRotator((1.0f - GearBlend) * 80.0f, 0.0f, 0.0f));
		}
		else if (C->ComponentHasTag(TEXT("Thrust")))
		{
			if (UStaticMeshComponent* M = Cast<UStaticMeshComponent>(C))
			{
				if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
				{
					MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.4f, 0.7f, 1.0f) * 30.0f * ThrustLevel * Flicker);
				}
			}
		}
	}
	Floodlight->SetVisibility(Pilot.IsValid());

	// Downwash kicks up dust near the ground.
	if (!bLanded && ThrustLevel > 0.3f && AltitudeCm < 1500.0f && FMath::FRand() < DeltaTime * 12.0f)
	{
		const float A = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Radial(FMath::Cos(A), FMath::Sin(A), 0.0f);
		const FVector Base = GetActorLocation() - FVector(0, 0, AltitudeCm + (bGearDown ? GearClearance : HullHalfHeight));
		AMarsImpactFX::Spawn(GetWorld(), Base + Radial * FMath::FRandRange(150.0f, 600.0f), (Radial + FVector(0, 0, 0.3f)).GetSafeNormal(), EMarsImpactType::ThrusterDust);
	}
}
