#include "Base/MarsBaseModule.h"

#include "MarsSurvival.h"
#include "Core/MarsTime.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsStationSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float Cm2ToM2 = 1.0e-4f;
}

AMarsBaseModule::AMarsBaseModule()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteriorVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteriorVolume"));
	InteriorVolume->SetupAttachment(SceneRoot);
	InteriorVolume->SetCollisionProfileName(TEXT("Trigger"));
	InteriorVolume->SetGenerateOverlapEvents(true);
	InteriorVolume->SetCanEverAffectNavigation(false);

	CeilingLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CeilingLight"));
	CeilingLight->SetupAttachment(SceneRoot);
	CeilingLight->SetIntensityUnits(ELightUnits::Lumens);
	CeilingLight->SetIntensity(1200.0f);
	CeilingLight->SetAttenuationRadius(1100.0f);
	CeilingLight->SetLightColor(FLinearColor(1.0f, 0.93f, 0.82f));
	CeilingLight->SetCastShadows(false);

	Floor       = CreateShellPiece(TEXT("Floor"));
	Ceiling     = CreateShellPiece(TEXT("Ceiling"));
	WallLeft    = CreateShellPiece(TEXT("WallLeft"));
	WallRight   = CreateShellPiece(TEXT("WallRight"));
	FrontWallA  = CreateShellPiece(TEXT("FrontWallA"));
	FrontWallB  = CreateShellPiece(TEXT("FrontWallB"));
	FrontLintel = CreateShellPiece(TEXT("FrontLintel"));
	BackWallA   = CreateShellPiece(TEXT("BackWallA"));
	BackWallB   = CreateShellPiece(TEXT("BackWallB"));
	BackLintel  = CreateShellPiece(TEXT("BackLintel"));
	WallLeftB      = CreateShellPiece(TEXT("WallLeftB"));
	WallLeftLintel = CreateShellPiece(TEXT("WallLeftLintel"));
	WallRightB      = CreateShellPiece(TEXT("WallRightB"));
	WallRightLintel = CreateShellPiece(TEXT("WallRightLintel"));
	WallLeftSill    = CreateShellPiece(TEXT("WallLeftSill"));

	WindowGlass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindowGlass"));
	WindowGlass->SetupAttachment(SceneRoot);
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> GlassCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (GlassCube.Succeeded())
		{
			WindowGlass->SetStaticMesh(GlassCube.Object);
		}
	}
	WindowGlass->SetCollisionProfileName(TEXT("BlockAll"));
	WindowGlass->SetCastShadow(false);
	WindowGlass->SetVisibility(false);
	WindowGlass->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SignFront = CreateSign(TEXT("SignFront"));
	SignBack  = CreateSign(TEXT("SignBack"));
	SignLeft  = CreateSign(TEXT("SignLeft"));
	SignRight = CreateSign(TEXT("SignRight"));

	ShellMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MarsSurvival/Materials/MI_HullPanel.MI_HullPanel")));

	SetCanBeDamaged(true);
	BuildShell();
}

UStaticMeshComponent* AMarsBaseModule::CreateShellPiece(const FName& Name)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Piece->SetupAttachment(SceneRoot);
	if (CubeMesh.Succeeded())
	{
		Piece->SetStaticMesh(CubeMesh.Object);
	}
	Piece->SetCollisionProfileName(TEXT("BlockAll"));
	Piece->SetMobility(EComponentMobility::Movable);
	ShellPieces.Add(Piece);
	return Piece;
}

UTextRenderComponent* AMarsBaseModule::CreateSign(const FName& Name)
{
	UTextRenderComponent* Sign = CreateDefaultSubobject<UTextRenderComponent>(Name);
	Sign->SetupAttachment(SceneRoot);
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetVerticalAlignment(EVRTA_TextCenter);
	Sign->SetWorldSize(18.0f);
	Sign->SetTextRenderColor(FColor(255, 176, 48));
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sign->SetCastShadow(false);
	return Sign;
}

void AMarsBaseModule::UpdateSignage()
{
	const float L = RoomSize.X;
	const float W = RoomSize.Y;
	const float H = RoomSize.Z;
	const float DoorTop = FMath::Min(DoorwaySize.Y, H - 10.0f);
	const float SignZ = FMath::Min(DoorTop + 28.0f, H - 15.0f);
	const FString Title = SectorLabel.IsEmpty() ? GetModuleName().ToUpper() : SectorLabel;

	// Text faces the component's +X; rotate every sign to face into the room.
	auto Setup = [](UTextRenderComponent* Sign, const FString& Text, const FVector& Location, float Yaw)
	{
		if (!Sign)
		{
			return;
		}
		Sign->SetText(FText::FromString(Text));
		Sign->SetRelativeLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f));
		Sign->SetVisibility(!Text.IsEmpty());
	};

	Setup(SignFront, bFrontDoorway ? SignFrontText : FString(), FVector(L * 0.5f - 2.0f, 0.0f, SignZ), 180.0f);
	Setup(SignBack, bBackDoorway ? SignBackText : FString(), FVector(-L * 0.5f + 2.0f, 0.0f, SignZ), 0.0f);

	// Side walls without a doorway carry the room title, mounted high.
	const FString LeftText = bLeftDoorway ? SignLeftText : (SignLeftText.IsEmpty() ? Title : SignLeftText);
	const FString RightText = bRightDoorway ? SignRightText : (SignRightText.IsEmpty() ? Title : SignRightText);
	const float LeftZ = bLeftDoorway ? SignZ : (bLeftWindow ? FMath::Min(WindowSize.Z + WindowSize.Y + 22.0f, H - 12.0f) : H * 0.72f);
	Setup(SignLeft, LeftText, FVector(bLeftDoorway || bLeftWindow ? LeftDoorwayOffset : 0.0f, -W * 0.5f + 2.0f, LeftZ), 90.0f);
	Setup(SignRight, RightText, FVector(bRightDoorway ? RightDoorwayOffset : 0.0f, W * 0.5f - 2.0f, bRightDoorway ? FMath::Min(SideDoorwaySize.Y + 25.0f, H - 12.0f) : H * 0.72f), -90.0f);
	if (SignLeft && !bLeftDoorway)
	{
		SignLeft->SetWorldSize(26.0f);
	}
	if (SignRight && !bRightDoorway)
	{
		SignRight->SetWorldSize(26.0f);
	}
}

void AMarsBaseModule::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildShell();
	UpdateSignage();

	if (UMaterialInterface* Mat = ShellMaterial.LoadSynchronous())
	{
		for (UStaticMeshComponent* Piece : ShellPieces)
		{
			if (Piece)
			{
				Piece->SetMaterial(0, Mat);
			}
		}
	}
	if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MarsSurvival/Materials/M_Glass.M_Glass")))
	{
		WindowGlass->SetMaterial(0, Glass);
	}
}

void AMarsBaseModule::BuildShell()
{
	// The engine cube is 100 uu per side and centred on its origin.
	const float L = RoomSize.X;
	const float W = RoomSize.Y;
	const float H = RoomSize.Z;
	const float T = WallThickness;
	const float DoorW = FMath::Min(DoorwaySize.X, W - 20.0f);
	const float DoorH = FMath::Min(DoorwaySize.Y, H - 10.0f);

	auto Place = [](UStaticMeshComponent* Piece, const FVector& Center, const FVector& Size, bool bVisible = true)
	{
		if (!Piece)
		{
			return;
		}
		Piece->SetRelativeLocation(Center);
		Piece->SetRelativeScale3D(Size / 100.0f);
		Piece->SetVisibility(bVisible);
		Piece->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	};

	Place(Floor,   FVector(0, 0, -T * 0.5f),    FVector(L + 2 * T, W + 2 * T, T));
	Place(Ceiling, FVector(0, 0, H + T * 0.5f), FVector(L + 2 * T, W + 2 * T, T));
	// Side wall with an optional opening (doorway from the floor, or a window above a sill), offset along X.
	auto PlaceSideWall = [&](float Sign, bool bDoorway, bool bWindow, float Offset, UStaticMeshComponent* A, UStaticMeshComponent* B,
		UStaticMeshComponent* Lintel, UStaticMeshComponent* Sill)
	{
		const float Y = Sign * (W + T) * 0.5f;
		if (!bDoorway && !bWindow)
		{
			Place(A, FVector(0, Y, H * 0.5f), FVector(L, T, H));
			Place(B, FVector::ZeroVector, FVector(1, 1, 1), false);
			Place(Lintel, FVector::ZeroVector, FVector(1, 1, 1), false);
			Place(Sill, FVector::ZeroVector, FVector(1, 1, 1), false);
			return;
		}
		const float OpenW = FMath::Min(bDoorway ? SideDoorwaySize.X : WindowSize.X, L - 40.0f);
		const float OpenBottom = bDoorway ? 0.0f : WindowSize.Z;
		const float OpenTop = bDoorway ? FMath::Min(SideDoorwaySize.Y, H - 10.0f) : FMath::Min(WindowSize.Z + WindowSize.Y, H - 10.0f);
		const float Cx = FMath::Clamp(Offset, -L * 0.5f + OpenW * 0.5f + 10.0f, L * 0.5f - OpenW * 0.5f - 10.0f);
		const float LeftEnd = Cx - OpenW * 0.5f;
		const float RightEnd = Cx + OpenW * 0.5f;
		const float SegA = LeftEnd + L * 0.5f;
		const float SegB = L * 0.5f - RightEnd;
		Place(A, FVector(-L * 0.5f + SegA * 0.5f, Y, H * 0.5f), FVector(FMath::Max(SegA, 1.0f), T, H), SegA > 1.0f);
		Place(B, FVector(RightEnd + SegB * 0.5f, Y, H * 0.5f), FVector(FMath::Max(SegB, 1.0f), T, H), SegB > 1.0f);
		Place(Lintel, FVector(Cx, Y, OpenTop + (H - OpenTop) * 0.5f), FVector(OpenW, T, H - OpenTop));
		Place(Sill, FVector(Cx, Y, OpenBottom * 0.5f), FVector(OpenW, T, FMath::Max(OpenBottom, 1.0f)), OpenBottom > 1.0f);
	};
	const bool bWindowLeft = bLeftWindow && !bLeftDoorway;
	PlaceSideWall(-1.0f, bLeftDoorway, bWindowLeft, LeftDoorwayOffset, WallLeft, WallLeftB, WallLeftLintel, WallLeftSill);
	PlaceSideWall(+1.0f, bRightDoorway, false, RightDoorwayOffset, WallRight, WallRightB, WallRightLintel, nullptr);

	if (WindowGlass)
	{
		WindowGlass->SetVisibility(bWindowLeft);
		WindowGlass->SetCollisionEnabled(bWindowLeft ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bWindowLeft)
		{
			const float GlassW = FMath::Min(WindowSize.X, L - 40.0f);
			WindowGlass->SetRelativeLocation(FVector(LeftDoorwayOffset, -(W + T) * 0.5f, WindowSize.Z + WindowSize.Y * 0.5f));
			WindowGlass->SetRelativeScale3D(FVector(GlassW, 4.0f, WindowSize.Y) / 100.0f);
		}
	}

	auto PlaceEndWall = [&](float Sign, bool bDoorway, UStaticMeshComponent* A, UStaticMeshComponent* B, UStaticMeshComponent* Lintel)
	{
		const float X = Sign * (L + T) * 0.5f;
		if (!bDoorway)
		{
			Place(A, FVector(X, 0, H * 0.5f), FVector(T, W, H));
			Place(B, FVector::ZeroVector, FVector(1, 1, 1), false);
			Place(Lintel, FVector::ZeroVector, FVector(1, 1, 1), false);
			return;
		}
		const float SideW = (W - DoorW) * 0.5f;
		const float SideY = DoorW * 0.5f + SideW * 0.5f;
		Place(A, FVector(X, -SideY, H * 0.5f), FVector(T, SideW, H));
		Place(B, FVector(X,  SideY, H * 0.5f), FVector(T, SideW, H));
		Place(Lintel, FVector(X, 0, DoorH + (H - DoorH) * 0.5f), FVector(T, DoorW, H - DoorH));
	};
	PlaceEndWall(+1.0f, bFrontDoorway, FrontWallA, FrontWallB, FrontLintel);
	PlaceEndWall(-1.0f, bBackDoorway, BackWallA, BackWallB, BackLintel);

	if (InteriorVolume)
	{
		InteriorVolume->SetRelativeLocation(FVector(0, 0, H * 0.5f));
		InteriorVolume->SetBoxExtent(FVector(L * 0.5f, W * 0.5f, H * 0.5f));
	}
	if (CeilingLight)
	{
		CeilingLight->SetRelativeLocation(FVector(0, 0, H - 30.0f));
		CeilingLight->SetAttenuationRadius(FMath::Max(L, W) * 1.1f);
	}

	// Approximate interior air volume from the room dimensions (cm^3 -> m^3).
	InternalVolumeM3 = FMath::Max(L * W * H * 1.0e-6f, 1.0f);
}

void AMarsBaseModule::BeginPlay()
{
	Super::BeginPlay();
	HullIntegrity = FMath::Clamp(HullIntegrity, 0.0f, 1.0f);
	SealIntegrity = FMath::Clamp(SealIntegrity, 0.0f, 1.0f);
	SealTemperatureC = FMath::Lerp(CurrentTemperatureC, ExternalAmbientTemperatureC, SealExposureFactor);

	if (UMarsStationSubsystem* Station = GetStation())
	{
		Station->RegisterModule(this);
	}
}

void AMarsBaseModule::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMarsStationSubsystem* Station = GetStation())
	{
		Station->UnregisterModule(this);
	}
	Super::EndPlay(EndPlayReason);
}

UMarsStationSubsystem* AMarsBaseModule::GetStation() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UMarsStationSubsystem>() : nullptr;
}

float AMarsBaseModule::GetGridPowerAvailability() const
{
	const UMarsStationSubsystem* Station = GetStation();
	return Station ? Station->GetModulePowerAvailability(this) : 1.0f;
}

void AMarsBaseModule::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	if (const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		ExternalAmbientTemperatureC = Env->GetAmbientTemperatureC();
	}

	const float GameSeconds = DeltaTime * MarsTime::GetTimeScale(this);
	UpdateThermalContraction(DeltaTime, ExternalAmbientTemperatureC);
	UpdatePressure(DeltaTime);
	UpdateAtmosphere(GameSeconds / MarsTime::SecondsPerHour);
	TickModuleSystems(DeltaTime, GameSeconds);
	UpdateLighting(DeltaTime);
}

// ----------------------------------------------------------------------------
// Damage
// ----------------------------------------------------------------------------

float AMarsBaseModule::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage <= 0.0f)
	{
		return 0.0f;
	}

	FVector ImpactPoint = GetActorLocation();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		ImpactPoint = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.ImpactPoint;
	}
	TakeBallisticDamageAtLocation(ActualDamage, ImpactPoint);
	return ActualDamage;
}

void AMarsBaseModule::TakeBallisticDamage(float DamageAmount)
{
	TakeBallisticDamageAtLocation(DamageAmount, GetActorLocation());
}

void AMarsBaseModule::TakeBallisticDamageAtLocation(float DamageAmount, FVector ImpactPoint)
{
	if (DamageAmount <= 0.0f)
	{
		return;
	}

	HullIntegrity = FMath::Clamp(HullIntegrity - DamageAmount * IntegrityLossPerDamagePoint, 0.0f, 1.0f);
	++PunctureCount;
	++TotalBallisticImpacts;
	PunctureLeakAreaCm2 += BulletHoleAreaCm2;

	UE_LOG(LogMarsSurvival, Warning, TEXT("[%s] Ballistic impact: %.1f dmg, integrity %.2f, punctures %d"),
		*GetModuleName(), DamageAmount, HullIntegrity, PunctureCount);

	OnHullPunctured.Broadcast(this, ImpactPoint, HullIntegrity);
	if (UMarsStationSubsystem* Station = GetStation())
	{
		Station->ReportStructuralImpact(this, ImpactPoint);
	}

	if (!bCatastrophicBreach && HullIntegrity < BreachIntegrityThreshold)
	{
		bCatastrophicBreach = true;
		UE_LOG(LogMarsSurvival, Error, TEXT("[%s] CATASTROPHIC HULL BREACH"), *GetModuleName());
		OnHullBreached.Broadcast(this);
		if (UMarsStationSubsystem* Station = GetStation())
		{
			Station->RaiseAlert(TEXT("HullBreach"), this);
		}
	}
}

// ----------------------------------------------------------------------------
// Thermal / seals
// ----------------------------------------------------------------------------

void AMarsBaseModule::UpdateThermalContraction(float DeltaTime, float ExternalAmbientTemp)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}
	const float SimDt = DeltaTime * MarsTime::GetTimeScale(this);

	// Proportional thermostat, limited by what the grid can deliver.
	float HeaterPowerW = 0.0f;
	if (bHeatingOnline)
	{
		const float Error = HeatingSetpointC - CurrentTemperatureC;
		HeaterPowerW = FMath::Clamp(Error * ThermostatGainWPerK, 0.0f, MaxHeaterPowerW);
	}
	LastHeaterPowerW = HeaterPowerW;
	HeaterPowerW *= GetGridPowerAvailability();

	// Lumped capacitance, exact exponential step (stable at large SimDt).
	const float UA = FMath::Max(HullThermalConductanceWPerK, KINDA_SMALL_NUMBER);
	const float Mass = FMath::Max(ThermalMassJPerK, KINDA_SMALL_NUMBER);
	const float EquilibriumC = ExternalAmbientTemp + HeaterPowerW / UA;
	CurrentTemperatureC = EquilibriumC + (CurrentTemperatureC - EquilibriumC) * FMath::Exp(-UA / Mass * SimDt);

	// Seals sit at the hull boundary: colder than the cabin even when heated.
	SealTemperatureC = FMath::Lerp(CurrentTemperatureC, ExternalAmbientTemp, SealExposureFactor);
	if (SealTemperatureC < SealCriticalTemperatureC)
	{
		const float KelvinBelow = SealCriticalTemperatureC - SealTemperatureC;
		const float Protection = bSealHeatersOnline ? SealHeaterProtection : 0.0f;
		SealIntegrity = FMath::Clamp(
			SealIntegrity - SealContractionRatePerKelvinHour * KelvinBelow * (SimDt / MarsTime::SecondsPerHour) * (1.0f - Protection),
			0.0f, 1.0f);
	}
}

float AMarsBaseModule::GetSealLeakAreaCm2() const
{
	return MaxSealLeakAreaCm2 * FMath::Square(1.0f - SealIntegrity);
}

float AMarsBaseModule::GetTotalLeakAreaCm2() const
{
	return PunctureLeakAreaCm2 + (bCatastrophicBreach ? CatastrophicBreachAreaCm2 : 0.0f) + GetSealLeakAreaCm2();
}

// ----------------------------------------------------------------------------
// Pressure
// ----------------------------------------------------------------------------

void AMarsBaseModule::UpdatePressure(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}
	const float SimDt = DeltaTime * MarsTime::GetTimeScale(this);
	const float PressureBefore = CurrentPressureKpa;

	// Choked-flow blowdown: d(P - Pamb)/dt = -(P - Pamb) * A * v / V.
	const float DecayRate = GetTotalLeakAreaCm2() * Cm2ToM2 * EffectiveDischargeVelocity / FMath::Max(InternalVolumeM3, 1.0f);
	const float Excess = FMath::Max(CurrentPressureKpa - MarsAmbientPressureKpa, 0.0f);
	CurrentPressureKpa = MarsAmbientPressureKpa + Excess * FMath::Exp(-DecayRate * SimDt);

	if (bLifeSupportOnline && CurrentPressureKpa < NominalPressureKpa)
	{
		const float Makeup = RepressurizationRateKpaPerHour * GetGridPowerAvailability() * (SimDt / MarsTime::SecondsPerHour);
		CurrentPressureKpa = FMath::Min(CurrentPressureKpa + Makeup, NominalPressureKpa);
	}
	CurrentPressureKpa = FMath::Max(CurrentPressureKpa, MarsAmbientPressureKpa);

	const float RateKpaPerHour = (CurrentPressureKpa - PressureBefore) / SimDt * MarsTime::SecondsPerHour;
	const bool bNowDecompressing = RateKpaPerHour < -0.5f;
	if (bNowDecompressing != bIsDecompressing)
	{
		bIsDecompressing = bNowDecompressing;
		if (bIsDecompressing)
		{
			OnDecompressionStarted.Broadcast(this);
			if (UMarsStationSubsystem* Station = GetStation())
			{
				Station->RaiseAlert(TEXT("Decompression"), this);
			}
		}
		else
		{
			OnDecompressionStopped.Broadcast(this);
		}
	}

	if (!bPressureCriticalLatched && CurrentPressureKpa < CriticalPressureKpa)
	{
		bPressureCriticalLatched = true;
		OnPressureCritical.Broadcast(this);
		if (UMarsStationSubsystem* Station = GetStation())
		{
			Station->RaiseAlert(TEXT("PressureCritical"), this);
		}
	}
	else if (bPressureCriticalLatched && CurrentPressureKpa > CriticalPressureKpa + 10.0f)
	{
		bPressureCriticalLatched = false;
	}
}

// ----------------------------------------------------------------------------
// Atmosphere
// ----------------------------------------------------------------------------

void AMarsBaseModule::UpdateAtmosphere(float GameHours)
{
	TArray<AActor*> Occupants;
	InteriorVolume->GetOverlappingActors(Occupants, APawn::StaticClass());
	OccupantCount = Occupants.Num();

	const float Power = GetGridPowerAvailability();

	// CO2: crew exhalation vs. amine/LiOH scrubber.
	CurrentCo2Ppm += OccupantCount * Co2PerOccupantPpmPerHour * GameHours;
	if (bCo2ScrubberOnline && CurrentCo2Ppm > ScrubberFloorPpm)
	{
		CurrentCo2Ppm = FMath::Max(CurrentCo2Ppm - ScrubberRemovalPpmPerHour * Power * GameHours, ScrubberFloorPpm);
	}

	// O2: crew consumption vs. life-support make-up toward 21%.
	CurrentO2Fraction -= OccupantCount * O2ConsumptionPerOccupantPerHour * GameHours;
	if (bLifeSupportOnline && CurrentO2Fraction < 0.21f)
	{
		CurrentO2Fraction = FMath::Min(CurrentO2Fraction + 0.01f * Power * GameHours, 0.21f);
	}

	// Dust: wind-driven ingress through holes once the cabin has bled down; HEPA filtration otherwise.
	float StormFactor = 0.3f;
	if (const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		StormFactor += Env->GetStormIntensity();
	}
	const float PressureDeficit = 1.0f - FMath::Clamp(CurrentPressureKpa / NominalPressureKpa, 0.0f, 1.0f);
	DustContamination += GetTotalLeakAreaCm2() * 0.02f * StormFactor * PressureDeficit * GameHours;
	if (bLifeSupportOnline)
	{
		DustContamination -= DustFiltrationPerHour * Power * GameHours;
	}

	CurrentCo2Ppm = FMath::Max(CurrentCo2Ppm, 0.0f);
	CurrentO2Fraction = FMath::Clamp(CurrentO2Fraction, 0.0f, 1.0f);
	DustContamination = FMath::Clamp(DustContamination, 0.0f, 1.0f);
}

void AMarsBaseModule::RemoveCo2Ppm(float Ppm)
{
	CurrentCo2Ppm = FMath::Max(CurrentCo2Ppm - FMath::Max(Ppm, 0.0f), 400.0f);
}

void AMarsBaseModule::AddO2Fraction(float Fraction)
{
	CurrentO2Fraction = FMath::Clamp(CurrentO2Fraction + Fraction, 0.0f, 0.30f);
}

void AMarsBaseModule::AddDustContamination(float Amount)
{
	DustContamination = FMath::Clamp(DustContamination + Amount, 0.0f, 1.0f);
}

// ----------------------------------------------------------------------------
// Lighting feedback: flicker on brownout, red emergency light when breached / critical
// ----------------------------------------------------------------------------

void AMarsBaseModule::UpdateLighting(float DeltaTime)
{
	if (!CeilingLight)
	{
		return;
	}

	const float Power = GetGridPowerAvailability();
	const UMarsStationSubsystem* Grid = GetStation();
	// Steady light whenever the grid copes; only a real brownout dims it.
	float Intensity = 1200.0f * (Power >= 0.7f ? 1.0f : FMath::Lerp(0.3f, 1.0f, Power / 0.7f))
		* (Grid ? FMath::Lerp(1.0f, Grid->GetLightingLevel(), ModuleType == EMarsModuleType::RingCorridor ? 1.0f : 0.5f) : 1.0f);

	// Flicker ONLY during a severe radiation storm / SPE with a stressed grid, or a genuine RTG overload.
	const UMarsEnvironmentSubsystem* LightEnv = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
	const bool bSevereStorm = LightEnv && (LightEnv->IsSolarParticleEventActive() || LightEnv->GetStormIntensity() > 0.75f);
	const bool bShouldFlicker = Power < 0.4f || (bSevereStorm && Power < 0.85f);
	if (bShouldFlicker)
	{
		LightFlickerTimer -= DeltaTime;
		if (LightFlickerTimer <= 0.0f)
		{
			LightFlickerTimer = FMath::FRandRange(0.05f, 0.6f);
		}
		if (LightFlickerTimer < 0.08f)
		{
			Intensity *= 0.1f;
		}
	}

	const bool bEmergency = bCatastrophicBreach || bPressureCriticalLatched;
	CeilingLight->SetLightColor(bEmergency ? FLinearColor(1.0f, 0.08f, 0.05f) : FMath::Lerp(FLinearColor(1.0f, 0.93f, 0.82f), SectorColor, 0.45f));
	CeilingLight->SetIntensity(bEmergency ? Intensity * 0.6f : Intensity);
}

// ----------------------------------------------------------------------------
// Repair
// ----------------------------------------------------------------------------

int32 AMarsBaseModule::PatchPunctures(int32 NumPatches)
{
	const int32 Patched = FMath::Clamp(NumPatches, 0, PunctureCount);
	PunctureCount -= Patched;
	PunctureLeakAreaCm2 = PunctureCount == 0 ? 0.0f : FMath::Max(PunctureLeakAreaCm2 - Patched * BulletHoleAreaCm2, 0.0f);
	return Patched;
}

void AMarsBaseModule::RepairHull(float IntegrityRestored)
{
	HullIntegrity = FMath::Clamp(HullIntegrity + FMath::Max(IntegrityRestored, 0.0f), 0.0f, 1.0f);
	if (bCatastrophicBreach && HullIntegrity >= BreachIntegrityThreshold + 0.15f)
	{
		bCatastrophicBreach = false;
		UE_LOG(LogMarsSurvival, Log, TEXT("[%s] Hull breach sealed"), *GetModuleName());
	}
}

void AMarsBaseModule::ApplySealTorque(float Amount)
{
	SealIntegrity = FMath::Clamp(SealIntegrity + FMath::Max(Amount, 0.0f), 0.0f, 1.0f);
}

void AMarsBaseModule::ServiceSeals()
{
	SealIntegrity = 1.0f;
}

// ----------------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------------

bool AMarsBaseModule::IsPointInside(const FVector& WorldLocation, float MarginCm) const
{
	if (!InteriorVolume)
	{
		return false;
	}
	const FVector Local = InteriorVolume->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = InteriorVolume->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X + MarginCm && FMath::Abs(Local.Y) <= Extent.Y + MarginCm && FMath::Abs(Local.Z) <= Extent.Z + MarginCm;
}

FString AMarsBaseModule::GetModuleName() const
{
	if (!SectorLabel.IsEmpty())
	{
		return SectorLabel;
	}
	const UEnum* ModuleEnum = StaticEnum<EMarsModuleType>();
	return ModuleEnum ? ModuleEnum->GetDisplayNameTextByValue(static_cast<int64>(ModuleType)).ToString() : GetName();
}

float AMarsBaseModule::GetPowerDemandW() const
{
	const UMarsStationSubsystem* Grid = GetStation();
	float Demand = BaseLoadPowerW * (Grid ? FMath::Lerp(0.6f, 1.0f, Grid->GetLightingLevel()) : 1.0f) + LastHeaterPowerW;
	if (bLifeSupportOnline)
	{
		Demand += LifeSupportPowerW;
	}
	if (bSealHeatersOnline)
	{
		Demand += 120.0f;
	}
	return Demand;
}

FString AMarsBaseModule::GetTelemetryReport() const
{
	FString Report = FString::Printf(
		TEXT("MODULE=%s; P_KPA=%.1f; T_C=%.1f; HULL=%.2f; SEALS=%.2f; SEAL_T_C=%.1f; PUNCTURES=%d; BALLISTIC_IMPACTS=%d; BREACH=%s; ")
		TEXT("LEAK_CM2=%.2f; DECOMPRESSING=%s; CO2_PPM=%.0f; O2_PCT=%.1f; DUST=%.2f; OCCUPANTS=%d; HEAT=%s; LIFE_SUPPORT=%s"),
		*GetModuleName(), CurrentPressureKpa, CurrentTemperatureC, HullIntegrity, SealIntegrity, SealTemperatureC,
		PunctureCount, TotalBallisticImpacts, bCatastrophicBreach ? TEXT("YES") : TEXT("NO"),
		GetTotalLeakAreaCm2(), bIsDecompressing ? TEXT("YES") : TEXT("NO"), CurrentCo2Ppm, CurrentO2Fraction * 100.0f,
		DustContamination, OccupantCount, bHeatingOnline ? TEXT("ON") : TEXT("OFF"), bLifeSupportOnline ? TEXT("ON") : TEXT("OFF"));

	const FString Extra = GetModuleSpecificTelemetry();
	if (!Extra.IsEmpty())
	{
		Report += TEXT("; ");
		Report += Extra;
	}
	return Report;
}
