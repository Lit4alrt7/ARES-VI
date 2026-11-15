#include "Hallucination/HallucinationDirectorComponent.h"
#include "Player/PlayerBiometricsComponent.h"

#include "Audio/MarsAudioManager.h"
#include "Base/MarsBaseModule.h"
#include "Hallucination/MarsPhantomActor.h"
#include "Hallucination/HallucinationPhantom.h"
#include "EngineUtils.h"
#include "Player/MarsCharacter.h"
#include "Rendering/RetroPostProcessComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

UHallucinationDirectorComponent::UHallucinationDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PhantomClass = AMarsPhantomActor::StaticClass();
}

void UHallucinationDirectorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner()))
	{
		if (UPlayerBiometricsComponent* Bio = Character->GetBiometrics())
		{
			Bio->OnPhantomTriggered.AddDynamic(this, &UHallucinationDirectorComponent::HandlePhantom);
			Bio->OnCherenkovFlash.AddDynamic(this, &UHallucinationDirectorComponent::HandleCherenkovFlash);
		}
	}
}

void UHallucinationDirectorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateStructuralGroans(DeltaTime);
	UpdatePeekers(DeltaTime);
}

void UHallucinationDirectorComponent::HandlePhantom(EPhantomType Type, float Intensity)
{
	LastPhantomType = Type;
	LastPhantomTime = GetWorld()->GetTimeSeconds();

	if (AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner()))
	{
		if (UPlayerBiometricsComponent* Bio = Character->GetBiometrics())
		{
			Bio->NotifyStartle(0.15f + 0.25f * Intensity);
		}
	}

	if (Type == EPhantomType::ShadowFigure || Type == EPhantomType::PeripheralMovement)
	{
		SpawnVisualPhantom(Type, Intensity);
	}
	else
	{
		PlayAudioPhantom(Type, Intensity);
	}
}

void UHallucinationDirectorComponent::HandleCherenkovFlash(float Strength)
{
	if (const AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner()))
	{
		if (Character->RetroPostProcess)
		{
			Character->RetroPostProcess->TriggerCherenkovFlash(Strength);
		}
		if (UMarsAudioManager* Audio = Character->GetAudioManager())
		{
			Audio->PlayCherenkovZap(Strength);
		}
	}
}

bool UHallucinationDirectorComponent::FindPhantomLocation(EPhantomType Type, FVector& OutLocation) const
{
	const AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner());
	if (!Character || !Character->GetFirstPersonCamera())
	{
		return false;
	}
	const UCameraComponent* Camera = Character->GetFirstPersonCamera();
	const FVector Eye = Camera->GetComponentLocation();
	const float ViewYaw = Camera->GetComponentRotation().Yaw;

	// Peripheral vision (> ~50 deg) for darting shapes; figures stand at the far end of a corridor or behind.
	const float YawOffset = Type == EPhantomType::PeripheralMovement
		? FMath::FRandRange(50.0f, 85.0f) * (FMath::RandBool() ? 1.0f : -1.0f)
		: (FMath::RandBool() ? FMath::FRandRange(-35.0f, 35.0f) : FMath::FRandRange(140.0f, 220.0f));

	const FVector Direction = FRotator(0.0f, ViewYaw + YawOffset, 0.0f).Vector();
	float Distance = FMath::FRandRange(350.0f, 1100.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PhantomPlacement), false, Character);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Direction * Distance, ECC_Visibility, Params))
	{
		Distance = Hit.Distance - 60.0f;
	}
	if (Distance < 280.0f)
	{
		return false;
	}

	const FVector Candidate = Eye + Direction * Distance;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Candidate, Candidate - FVector(0, 0, 400.0f), ECC_Visibility, Params))
	{
		OutLocation = Hit.ImpactPoint + FVector(0, 0, 92.0f);
		return true;
	}
	return false;
}

void UHallucinationDirectorComponent::SpawnVisualPhantom(EPhantomType Type, float Intensity)
{
	FVector Location;
	if (!PhantomClass || !FindPhantomLocation(Type, Location))
	{
		PlayAudioPhantom(EPhantomType::Footsteps, Intensity);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (Type == EPhantomType::ShadowFigure)
	{
		// Full 3D cognitive phantom (at most two at once).
		int32 Alive = 0;
		for (TActorIterator<AHallucinationPhantom> It(GetWorld()); It; ++It)
		{
			++Alive;
		}
		if (Alive < 2)
		{
			if (AHallucinationPhantom* Creature = GetWorld()->SpawnActor<AHallucinationPhantom>(AHallucinationPhantom::StaticClass(), Location - FVector(0, 0, 92.0f), FRotator::ZeroRotator, Params))
			{
				Creature->InitPhantom(Cast<APawn>(GetOwner()), Intensity);
			}
		}
	}
	else if (AMarsPhantomActor* Phantom = GetWorld()->SpawnActor<AMarsPhantomActor>(PhantomClass, Location, FRotator::ZeroRotator, Params))
	{
		Phantom->InitPhantom(Cast<APawn>(GetOwner()), Type, Intensity, FMath::FRandRange(1.0f, 2.0f));
	}
	if (const AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner()))
	{
		if (UMarsAudioManager* Audio = Character->GetAudioManager())
		{
			Audio->SpikeTinnitus(0.03f);
		}
	}
}

void UHallucinationDirectorComponent::PlayAudioPhantom(EPhantomType Type, float Intensity)
{
	const AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	// Behind the player, usually beyond a bulkhead.
	const FVector Offset = FRotator(0.0f, Character->GetControlRotation().Yaw + FMath::FRandRange(100.0f, 260.0f), 0.0f).Vector() * FMath::FRandRange(250.0f, 700.0f);
	if (UMarsAudioManager* Audio = Character->GetAudioManager())
	{
		Audio->PlayPhantom(Type, Character->GetActorLocation() + Offset, Intensity);
	}

	switch (Type)
	{
	case EPhantomType::Footsteps:     Caption(TEXT("[footsteps behind you]")); break;
	case EPhantomType::HullKnocking:  Caption(TEXT("[slow knocks on the outer hull]")); break;
	case EPhantomType::MetalScraping: Caption(TEXT("[metal scraping behind the bulkhead]")); break;
	case EPhantomType::Whisper:       Caption(TEXT("[someone whispers your name]")); break;
	case EPhantomType::RadioVoice:    Caption(TEXT("[radio: \"...let us in...\"]")); break;
	default: break;
	}
}

void UHallucinationDirectorComponent::UpdateStructuralGroans(float DeltaTime)
{
	const AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner());
	const AMarsBaseModule* Module = Character ? Character->GetCurrentModule() : nullptr;
	if (!Module || Module->SealTemperatureC >= Module->SealCriticalTemperatureC)
	{
		return;
	}

	// Real thermal contraction of the frame at -80 C nights: colder seals groan more often.
	const float ColdFactor = FMath::Clamp((Module->SealCriticalTemperatureC - Module->SealTemperatureC) / 20.0f, 0.2f, 2.0f);
	if (FMath::FRand() < 1.0f - FMath::Exp(-GroanChancePerSecond * ColdFactor * DeltaTime))
	{
		if (UMarsAudioManager* Audio = Character->GetAudioManager())
		{
			const FVector WallPoint = Module->GetActorLocation() + FMath::VRand() * 300.0f;
			Audio->PlayHullGroan(WallPoint, 0.6f + 0.3f * ColdFactor);
		}
		Caption(TEXT("[the hull groans as it contracts]"));
	}
}

void UHallucinationDirectorComponent::Caption(const TCHAR* Text)
{
	if (bShowCaptions)
	{
		if (AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner()))
		{
			Character->ShowNotification(FText::FromString(Text), 3.0f);
		}
	}
}

bool UHallucinationDirectorComponent::HasRecentPhantom(float WithinSeconds) const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() - LastPhantomTime <= WithinSeconds;
}

FString UHallucinationDirectorComponent::GetLastPhantomDescription() const
{
	switch (LastPhantomType)
	{
	case EPhantomType::ShadowFigure:       return TEXT("I saw a tall dark figure standing at the end of the corridor.");
	case EPhantomType::PeripheralMovement: return TEXT("Something just moved past me, fast.");
	case EPhantomType::Footsteps:          return TEXT("I heard footsteps right behind me.");
	case EPhantomType::HullKnocking:       return TEXT("Something is knocking on the outside of the hull.");
	case EPhantomType::Whisper:            return TEXT("Someone whispered my name.");
	case EPhantomType::RadioVoice:         return TEXT("There's a voice on the radio asking to be let in.");
	case EPhantomType::MetalScraping:      return TEXT("Something is dragging metal behind the bulkhead.");
	default:                               return TEXT("Something is wrong.");
	}
}

// ----------------------------------------------------------------------------
// Doorway / corner peekers (untreated disease, sleep deprivation, toxicity)
// ----------------------------------------------------------------------------

bool UHallucinationDirectorComponent::FindPeekSpot(FVector& OutLocation, FVector& OutLean) const
{
	const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwner());
	const AMarsBaseModule* M = C ? C->GetCurrentModule() : nullptr;
	if (!M || !C->GetFirstPersonCamera())
	{
		return false;
	}
	const FVector Eye = C->GetFirstPersonCamera()->GetComponentLocation();
	const FVector Fwd = C->GetFirstPersonCamera()->GetForwardVector().GetSafeNormal2D();
	const FTransform Xf = M->GetActorTransform();
	const float HL = M->RoomSize.X * 0.5f, HW = M->RoomSize.Y * 0.5f, T = M->WallThickness;

	struct FDoor { FVector Center; FVector Normal; FVector Along; float Width; };
	TArray<FDoor> Doors;
	if (M->bFrontDoorway) { Doors.Add({ FVector(HL + T, 0, 0), FVector(1, 0, 0), FVector(0, 1, 0), static_cast<float>(M->DoorwaySize.X) }); }
	if (M->bBackDoorway) { Doors.Add({ FVector(-HL - T, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), static_cast<float>(M->DoorwaySize.X) }); }
	if (M->bRightDoorway) { Doors.Add({ FVector(M->RightDoorwayOffset, HW + T, 0), FVector(0, 1, 0), FVector(1, 0, 0), static_cast<float>(M->SideDoorwaySize.X) }); }
	if (M->bLeftDoorway) { Doors.Add({ FVector(M->LeftDoorwayOffset, -HW - T, 0), FVector(0, -1, 0), FVector(1, 0, 0), static_cast<float>(M->SideDoorwaySize.X) }); }

	// A doorway at the edge of vision (20-60 degrees off centre), 4-20 m away.
	TArray<const FDoor*> Candidates;
	for (const FDoor& D : Doors)
	{
		const FVector World = Xf.TransformPosition(D.Center);
		const FVector To = (World - Eye).GetSafeNormal2D();
		const float Dist = FVector::Dist2D(World, Eye);
		const float Cos = FVector::DotProduct(To, Fwd);
		if (Dist > 400.0f && Dist < 2000.0f && Cos < 0.94f && Cos > 0.5f)
		{
			Candidates.Add(&D);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	const FDoor& D = *Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	const float Side = FMath::RandBool() ? 1.0f : -1.0f;
	// Just behind the jamb, in the next compartment, leaning into the opening.
	const FVector Local = D.Center + D.Normal * 45.0f + D.Along * Side * (D.Width * 0.5f + 32.0f);
	OutLocation = Xf.TransformPosition(Local);
	OutLocation.Z = M->GetActorLocation().Z;
	OutLean = Xf.TransformVectorNoScale(-D.Along * Side);
	return true;
}

void UHallucinationDirectorComponent::UpdatePeekers(float DeltaTime)
{
	const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwner());
	const UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	if (!Bio || Bio->IsDead() || C->IsSleeping())
	{
		return;
	}
	const float Severity = Bio->GetUntreatedSeverity();
	if (Severity < 0.35f)
	{
		PeekTimer = FMath::Max(PeekTimer, 20.0f);
		return;
	}
	PeekTimer -= DeltaTime;
	if (PeekTimer > 0.0f)
	{
		return;
	}
	PeekTimer = FMath::Lerp(55.0f, 14.0f, FMath::Clamp((Severity - 0.35f) / 0.65f, 0.0f, 1.0f)) * FMath::FRandRange(0.7f, 1.3f);
	for (TActorIterator<AHallucinationPhantom> It(GetWorld()); It; ++It)
	{
		return; // one manifestation at a time
	}
	FVector Where, Lean;
	if (!FindPeekSpot(Where, Lean))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AHallucinationPhantom* P = GetWorld()->SpawnActor<AHallucinationPhantom>(AHallucinationPhantom::StaticClass(), Where, FRotator::ZeroRotator, Params))
	{
		P->InitPeeker(Cast<APawn>(GetOwner()), Severity, Lean);
	}
}
