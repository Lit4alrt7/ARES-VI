#include "Base/AirlockModule.h"

#include "MarsSurvival.h"
#include "Audio/MarsAudioManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/MarsCharacter.h"
#include "Player/EVASuitComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAirlockModule::AAirlockModule()
{
	ModuleType = EMarsModuleType::AirlockDecon;
	RoomSize = FVector(500.0f, 400.0f, 300.0f);
	BuildShell();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	InnerHatch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InnerHatch"));
	InnerHatch->SetupAttachment(SceneRoot);
	OuterHatch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OuterHatch"));
	OuterHatch->SetupAttachment(SceneRoot);
	for (UStaticMeshComponent* Hatch : { InnerHatch.Get(), OuterHatch.Get() })
	{
		if (CubeMesh.Succeeded())
		{
			Hatch->SetStaticMesh(CubeMesh.Object);
		}
		Hatch->SetCollisionProfileName(TEXT("BlockAll"));
		Hatch->SetMobility(EComponentMobility::Movable);
		ShellPieces.Add(Hatch);
	}

	PlaceHatches();
	SetHatchOpen(InnerHatch, true);
	SetHatchOpen(OuterHatch, false);
}

void AAirlockModule::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PlaceHatches();
	SetHatchOpen(InnerHatch, AirlockState == EAirlockState::PressurizedInnerOpen);
	SetHatchOpen(OuterHatch, AirlockState == EAirlockState::DepressurizedOuterOpen);
}

void AAirlockModule::PlaceHatches()
{
	const FVector HatchSize(WallThickness * 0.8f, DoorwaySize.X, DoorwaySize.Y);
	const float X = (RoomSize.X + WallThickness) * 0.5f;
	if (OuterHatch)
	{
		OuterHatch->SetRelativeLocation(FVector(X, 0, DoorwaySize.Y * 0.5f));
		OuterHatch->SetRelativeScale3D(HatchSize / 100.0f);
	}
	if (InnerHatch)
	{
		InnerHatch->SetRelativeLocation(FVector(-X, 0, DoorwaySize.Y * 0.5f));
		InnerHatch->SetRelativeScale3D(HatchSize / 100.0f);
	}
}

void AAirlockModule::SetHatchOpen(UStaticMeshComponent* Hatch, bool bOpen)
{
	if (Hatch)
	{
		Hatch->SetVisibility(!bOpen);
		Hatch->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
}

void AAirlockModule::SetState(EAirlockState NewState)
{
	AirlockState = NewState;
	SetHatchOpen(InnerHatch, NewState == EAirlockState::PressurizedInnerOpen);
	SetHatchOpen(OuterHatch, NewState == EAirlockState::DepressurizedOuterOpen);
	OnAirlockStateChanged.Broadcast(NewState);

	// Pressure equalisation valves: audible hiss while pumping.
	if (NewState == EAirlockState::Depressurizing || NewState == EAirlockState::DeconAndRepressurizing)
	{
		if (const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (UMarsAudioManager* Audio = Player->GetAudioManager())
			{
				const float Seconds = (NominalPressureKpa - MarsAmbientPressureKpa) / FMath::Max(PumpRateKpaPerSecond, 1.0f)
					+ (NewState == EAirlockState::DeconAndRepressurizing ? DeconDurationSeconds : 0.0f);
				Audio->PlayAirlockHiss(GetActorLocation(), Seconds);
			}
		}
	}
}

bool AAirlockModule::IsCycling() const
{
	return AirlockState == EAirlockState::Depressurizing || AirlockState == EAirlockState::DeconAndRepressurizing;
}

float AAirlockModule::GetCycleProgress() const
{
	const float Span = FMath::Max(NominalPressureKpa - MarsAmbientPressureKpa, 1.0f);
	if (AirlockState == EAirlockState::Depressurizing)
	{
		return 1.0f - (CurrentPressureKpa - MarsAmbientPressureKpa) / Span;
	}
	if (AirlockState == EAirlockState::DeconAndRepressurizing)
	{
		return (CurrentPressureKpa - MarsAmbientPressureKpa) / Span;
	}
	return 1.0f;
}

bool AAirlockModule::RequestCycle()
{
	if (AirlockState == EAirlockState::PressurizedInnerOpen)
	{
		SetState(EAirlockState::Depressurizing);
		return true;
	}
	if (AirlockState == EAirlockState::DepressurizedOuterOpen)
	{
		DeconElapsed = 0.0f;
		SetState(EAirlockState::DeconAndRepressurizing);
		return true;
	}
	return false;
}

void AAirlockModule::TickModuleSystems(float RealDeltaTime, float GameDeltaSeconds)
{
	switch (AirlockState)
	{
	case EAirlockState::PressurizedInnerOpen:
		bLifeSupportOnline = true;
		break;

	case EAirlockState::Depressurizing:
		bLifeSupportOnline = false;
		CurrentPressureKpa = FMath::Max(CurrentPressureKpa - PumpRateKpaPerSecond * RealDeltaTime, MarsAmbientPressureKpa);
		if (CurrentPressureKpa <= MarsAmbientPressureKpa + 0.05f)
		{
			SetState(EAirlockState::DepressurizedOuterOpen);
		}
		break;

	case EAirlockState::DepressurizedOuterOpen:
		bLifeSupportOnline = false;
		CurrentPressureKpa = MarsAmbientPressureKpa;
		AddDustContamination(0.02f * RealDeltaTime);
		break;

	case EAirlockState::DeconAndRepressurizing:
		bLifeSupportOnline = false;
		DeconElapsed += RealDeltaTime;
		DecontaminateOccupants(RealDeltaTime);
		// Hold at low pressure for the electrostatic pass, then pump up.
		if (DeconElapsed >= DeconDurationSeconds)
		{
			CurrentPressureKpa = FMath::Min(CurrentPressureKpa + PumpRateKpaPerSecond * RealDeltaTime, NominalPressureKpa);
			if (CurrentPressureKpa >= NominalPressureKpa - 0.1f)
			{
				CurrentO2Fraction = 0.21f;
				SetState(EAirlockState::PressurizedInnerOpen);
			}
		}
		break;
	}
}

void AAirlockModule::DecontaminateOccupants(float RealDeltaTime)
{
	// Electrostatic precipitator curtain: strips suit dust and clears the chamber.
	DustContamination = FMath::Max(DustContamination - 0.4f * RealDeltaTime, 0.0f);

	TArray<AActor*> Occupants;
	InteriorVolume->GetOverlappingActors(Occupants, AMarsCharacter::StaticClass());
	for (AActor* Actor : Occupants)
	{
		if (const AMarsCharacter* Character = Cast<AMarsCharacter>(Actor))
		{
			if (UEVASuitComponent* Suit = Character->GetSuit())
			{
				Suit->RemoveDust(0.35f * RealDeltaTime);
			}
		}
	}
}

FString AAirlockModule::GetModuleSpecificTelemetry() const
{
	static const TCHAR* StateNames[] = { TEXT("PRESSURIZED_INNER_OPEN"), TEXT("DEPRESSURIZING"), TEXT("DEPRESSURIZED_OUTER_OPEN"), TEXT("DECON_REPRESS") };
	return FString::Printf(TEXT("AIRLOCK_STATE=%s"), StateNames[static_cast<int32>(AirlockState)]);
}
