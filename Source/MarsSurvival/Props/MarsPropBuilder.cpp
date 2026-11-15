#include "Props/MarsPropBuilder.h"
#include "Math/RandomStream.h"

#include "Base/BioDomeModule.h"
#include "Player/PlayerBiometricsComponent.h"
#include "World/MarsStationSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

FLinearColor AMarsPropBuilder::BuildTint = FLinearColor(1.0f, 0.62f, 0.1f);
FVector AMarsPropBuilder::BuildExtent = FVector(400.0f, 100.0f, 250.0f);

namespace PropPalette
{
	const FLinearColor Hull(0.55f, 0.56f, 0.58f);
	const FLinearColor White(0.82f, 0.83f, 0.85f);
	const FLinearColor Dark(0.07f, 0.075f, 0.08f);
	const FLinearColor Polymer(0.11f, 0.12f, 0.11f);
	const FLinearColor Titanium(0.58f, 0.6f, 0.63f);
	const FLinearColor Steel(0.42f, 0.43f, 0.45f);
	const FLinearColor Orange(0.9f, 0.38f, 0.08f);
	const FLinearColor Yellow(0.92f, 0.74f, 0.1f);
	const FLinearColor Red(0.95f, 0.08f, 0.05f);
	const FLinearColor Green(0.2f, 1.0f, 0.35f);
	const FLinearColor Cyan(0.1f, 0.8f, 1.0f);
	const FLinearColor Amber(1.0f, 0.62f, 0.1f);
	const FLinearColor Blue(0.12f, 0.3f, 0.9f);
	const FLinearColor Purple(0.6f, 0.2f, 0.9f);
	const FLinearColor Gold(0.85f, 0.6f, 0.15f);
	const FLinearColor Olive(0.32f, 0.36f, 0.18f);
	const FLinearColor Regolith(0.62f, 0.3f, 0.15f);
	const FLinearColor ScreenGreen(0.05f, 0.35f, 0.15f);
}

namespace
{
	UStaticMesh* ShapeMesh(EMarsPropShape Shape)
	{
		static const TCHAR* Paths[] =
		{
			TEXT("/Engine/BasicShapes/Cube.Cube"),
			TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
			TEXT("/Engine/BasicShapes/Sphere.Sphere"),
			TEXT("/Engine/BasicShapes/Cone.Cone")
		};
		return LoadObject<UStaticMesh>(nullptr, Paths[static_cast<int32>(Shape)]);
	}

	UMaterialInterface* PropMaterial()
	{
		static TSoftObjectPtr<UMaterialInterface> Material(FSoftObjectPath(TEXT("/Game/MarsSurvival/Materials/M_PS1_Surface.M_PS1_Surface")));
		return Material.LoadSynchronous();
	}

	// Cylinder axis is Z: rotate pitch 90 so it runs along X.
	const FRotator AlongX(90.0f, 0.0f, 0.0f);
	const FRotator AlongY(0.0f, 0.0f, 90.0f);
}

AMarsPropBuilder::AMarsPropBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.04f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AMarsPropBuilder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildTint = Tint;
	BuildExtent = Extent;
	BuildProp(GetRootComponent(), Kind, Scale, bCollision);
	BuildTint = FLinearColor(1.0f, 0.62f, 0.1f);
	BuildExtent = FVector(400.0f, 100.0f, 250.0f);

	SpinParts.Reset();
	GlowParts.Reset();
	GlowBase.Reset();
	GlowMode.Reset();
	BreatheParts.Reset();
	BreatheBase.Reset();

	TArray<USceneComponent*> All;
	GetComponents<USceneComponent>(All);
	for (USceneComponent* C : All)
	{
		if (C->ComponentHasTag(TEXT("Spin")))
		{
			SpinParts.Add(C);
		}
		UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(C);
		if (!Mesh)
		{
			continue;
		}
		const bool bPulse = Mesh->ComponentHasTag(TEXT("Pulse"));
		const bool bBlink = Mesh->ComponentHasTag(TEXT("Blink"));
		if (bPulse || bBlink)
		{
			FLinearColor Base = FLinearColor::White;
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
			{
				MID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("EmissiveColor")), Base);
			}
			GlowParts.Add(Mesh);
			GlowBase.Add(Base);
			GlowMode.Add(bBlink ? 1 : 0);
		}
		if (Mesh->ComponentHasTag(TEXT("Breathe")))
		{
			BreatheParts.Add(Mesh);
			BreatheBase.Add(Mesh->GetRelativeScale3D());
		}
	}
	SetActorTickEnabled(SpinParts.Num() + GlowParts.Num() + BreatheParts.Num() > 0);
}

USceneComponent* AMarsPropBuilder::AddPivot(USceneComponent* Parent, const FVector& Location, const FRotator& Rotation, FName Tag)
{
	AActor* Owner = Parent ? Parent->GetOwner() : nullptr;
	if (!Owner)
	{
		return nullptr;
	}
	USceneComponent* Pivot = NewObject<USceneComponent>(Owner, NAME_None, RF_Transactional);
	Pivot->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Pivot->SetupAttachment(Parent);
	Pivot->SetRelativeLocationAndRotation(Location, Rotation);
	if (Tag != NAME_None)
	{
		Pivot->ComponentTags.Add(Tag);
	}
	Pivot->RegisterComponent();
	return Pivot;
}

void AMarsPropBuilder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AnimTime += DeltaTime;

	for (USceneComponent* Spin : SpinParts)
	{
		if (Spin)
		{
			Spin->AddLocalRotation(FRotator(0.0f, 35.0f * DeltaTime, 0.0f));
		}
	}

	// Xenoflora react to radiation (dose rate at the player) and to darkness (Bio-Dome grow lights off).
	float RadFactor = 0.0f;
	float DarkFactor = 1.0f;
	if (const APawn* Player = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr)
	{
		if (const UPlayerBiometricsComponent* Bio = Player->FindComponentByClass<UPlayerBiometricsComponent>())
		{
			RadFactor = FMath::Clamp(Bio->CurrentDoseRateMsvPerHour / 0.4f, 0.0f, 1.0f);
		}
	}
	if (const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>())
	{
		if (const ABioDomeModule* Dome = Cast<ABioDomeModule>(Station->FindModuleByType(EMarsModuleType::AeroponicBioDome)))
		{
			DarkFactor = Dome->bGrowLightsOnline ? 1.0f : 1.8f;
		}
	}

	for (int32 i = 0; i < GlowParts.Num(); ++i)
	{
		UStaticMeshComponent* Part = GlowParts[i];
		UMaterialInstanceDynamic* MID = Part ? Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)) : nullptr;
		if (!MID)
		{
			continue;
		}
		float Factor;
		if (GlowMode[i] == 1)
		{
			// Server LEDs: pseudo-random data blinking.
			Factor = FMath::Frac(FMath::Sin((AnimTime * 7.0f + i * 1.37f)) * 43758.5f) > 0.45f ? 1.0f : 0.1f;
		}
		else
		{
			const float Wave = 0.55f + 0.45f * FMath::Sin(AnimTime * (1.3f + 2.5f * RadFactor) + i * 0.9f);
			Factor = Wave * (1.0f + 2.5f * RadFactor) * DarkFactor;
		}
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), GlowBase[i] * Factor);
	}

	for (int32 i = 0; i < BreatheParts.Num(); ++i)
	{
		if (BreatheParts[i])
		{
			BreatheParts[i]->SetRelativeScale3D(BreatheBase[i] * (1.0f + 0.07f * FMath::Sin(AnimTime * 0.9f + i)));
		}
	}
}

UStaticMeshComponent* AMarsPropBuilder::AddPart(USceneComponent* Parent, EMarsPropShape Shape, const FVector& Location, const FVector& Size,
	const FRotator& Rotation, const FLinearColor& Color, float Emissive, bool bCollision, FName Tag)
{
	AActor* Owner = Parent ? Parent->GetOwner() : nullptr;
	if (!Owner)
	{
		return nullptr;
	}

	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transactional);
	Part->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Part->SetStaticMesh(ShapeMesh(Shape));
	Part->SetupAttachment(Parent);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Size / 100.0f);
	Part->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Part->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
	Part->SetCastShadow(bCollision);
	Part->SetCanEverAffectNavigation(bCollision);
	if (Tag != NAME_None)
	{
		Part->ComponentTags.Add(Tag);
	}

	// Alpha < 1 = translucent glass (frosted partitions, terrariums, spore pods).
	static TSoftObjectPtr<UMaterialInterface> GlassMaterial(FSoftObjectPath(TEXT("/Game/MarsSurvival/Materials/M_Glass.M_Glass")));
	UMaterialInterface* Glass = Color.A < 0.99f ? GlassMaterial.LoadSynchronous() : nullptr;
	if (Glass)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Glass, Part);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color.R, Color.G, Color.B, 1.0f));
		MID->SetScalarParameterValue(TEXT("Opacity"), Color.A);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(Color.R, Color.G, Color.B, 1.0f) * Emissive);
		Part->SetMaterial(0, MID);
		Part->SetCastShadow(false);
	}
	else if (UMaterialInterface* Base = PropMaterial())
	{
		// Outer = the part, so the instance serialises with it.
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Part);
		MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color * Emissive);
		MID->SetScalarParameterValue(TEXT("PatternType"), 0.0f);
		MID->SetScalarParameterValue(TEXT("JitterAmount"), 0.0025f);
		Part->SetMaterial(0, MID);
	}

	Part->RegisterComponent();
	return Part;
}

EMarsPropKind AMarsPropBuilder::GetPropForItem(EMarsItemType ItemType)
{
	switch (ItemType)
	{
	case EMarsItemType::Glock18:                return EMarsPropKind::GlockModel;
	case EMarsItemType::WeldSealantGun:         return EMarsPropKind::WeldGun;
	case EMarsItemType::TorqueWrench:           return EMarsPropKind::TorqueWrench;
	case EMarsItemType::RadiationDosimeter:     return EMarsPropKind::Dosimeter;
	case EMarsItemType::Ammo9mm:                return EMarsPropKind::AmmoBox;
	case EMarsItemType::SealantCartridge:       return EMarsPropKind::SealantCartridge;
	case EMarsItemType::O2Canister:             return EMarsPropKind::O2Canister;
	case EMarsItemType::MethyleneBlueCartridge: return EMarsPropKind::SyringeMethyleneBlue;
	case EMarsItemType::IodinePills:            return EMarsPropKind::KIBlister;
	case EMarsItemType::RegolithSample:         return EMarsPropKind::SampleContainer;
	case EMarsItemType::RockSample:             return EMarsPropKind::RockSample;
	case EMarsItemType::QuantumTransistor:      return EMarsPropKind::QuantumTransistor;
	case EMarsItemType::SpareParts:             return EMarsPropKind::SpareParts;
	case EMarsItemType::SurfaceBeacon:          return EMarsPropKind::BeaconKit;
	case EMarsItemType::WaterPouch:             return EMarsPropKind::WaterPouch;
	case EMarsItemType::FoodRation:             return EMarsPropKind::FoodRation;
	case EMarsItemType::ChlorellaPaste:         return EMarsPropKind::ChlorellaTube;
	case EMarsItemType::NeuroAntitoxin:         return EMarsPropKind::SyringeAntitoxin;
	case EMarsItemType::Amifostine:             return EMarsPropKind::AutoInjectorAmifostine;
	case EMarsItemType::BioAgentCulture:        return EMarsPropKind::BioCulture;
	default:                                    return EMarsPropKind::Crate;
	}
}

TArray<UStaticMeshComponent*> AMarsPropBuilder::BuildProp(USceneComponent* Parent, EMarsPropKind Kind, float S, bool bCol, float SelfIllumination)
{
	using namespace PropPalette;
	TArray<UStaticMeshComponent*> Parts;
	if (!Parent || Kind == EMarsPropKind::None)
	{
		return Parts;
	}

	auto P = [&](EMarsPropShape Shape, FVector Loc, FVector Size, FRotator Rot, FLinearColor Color, float Emissive = 0.0f, FName Tag = NAME_None)
	{
		// Viewmodels get a little self-illumination so they read in dark modules (PS1 games did the same).
		if (UStaticMeshComponent* Part = AddPart(Parent, Shape, Loc * S, Size * S, Rot, Color, FMath::Max(Emissive, SelfIllumination), bCol, Tag))
		{
			Parts.Add(Part);
		}
	};
	constexpr EMarsPropShape Box = EMarsPropShape::Box;
	constexpr EMarsPropShape Cyl = EMarsPropShape::Cylinder;
	constexpr EMarsPropShape Sph = EMarsPropShape::Sphere;
	constexpr EMarsPropShape Cone = EMarsPropShape::Cone;
	const FRotator R0 = FRotator::ZeroRotator;

	switch (Kind)
	{
	// ------------------------------------------------------------------ hand-held
	case EMarsPropKind::GlockModel:
	{
		// 2087 NASA-modified Glock-18C: chamfered titanium slide with lightening cuts and comp ports, polymer
		// frame with stippled grip panels, extended 33-round magazine, enclosed holo sight, laser module.
		const FLinearColor Brass(0.78f, 0.6f, 0.28f);
		const FLinearColor Gunmetal(0.22f, 0.23f, 0.25f);
		const FLinearColor Tritium(0.3f, 1.0f, 0.45f);
		// --- Frame
		P(Box, FVector(2.0f, 0, 0.3f), FVector(17, 3.0f, 2.4f), R0, Polymer);
		P(Box, FVector(8.6f, 0, -1.3f), FVector(5.2f, 2.6f, 1.3f), R0, Polymer);                    // dust cover
		for (int32 i = 0; i < 3; ++i)
		{
			P(Box, FVector(7.4f + i * 1.3f, 0, -2.0f), FVector(0.5f, 2.7f, 0.3f), R0, Dark);           // accessory rail slots
		}
		P(Box, FVector(0.8f, 0, -3.9f), FVector(5.8f, 0.9f, 0.6f), R0, Polymer);                    // trigger guard
		P(Box, FVector(3.6f, 0, -2.9f), FVector(0.6f, 0.9f, 2.2f), FRotator(-8, 0, 0), Polymer);
		P(Box, FVector(0.5f, 0, -2.5f), FVector(0.55f, 0.55f, 2.0f), FRotator(-12, 0, 0), Gunmetal); // trigger
		P(Box, FVector(0.65f, 0, -2.4f), FVector(0.2f, 0.3f, 1.2f), FRotator(-12, 0, 0), Orange);    // trigger safety
		P(Box, FVector(-4.6f, 0, -6.0f), FVector(3.6f, 3.0f, 10.5f), FRotator(-18, 0, 0), Polymer);  // grip
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			P(Box, FVector(-4.7f, Side * 1.55f, -6.2f), FVector(2.6f, 0.12f, 7.6f), FRotator(-18, 0, 0), Dark);     // stippling panel
			P(Box, FVector(2.6f, Side * 1.56f, 0.5f), FVector(9.5f, 0.12f, 0.25f), R0, Cyan, 3.0f);                  // emissive frame trim
			P(Box, FVector(-1.2f, Side * 1.52f, -0.6f), FVector(0.7f, 0.15f, 0.7f), R0, Gunmetal);                   // mag release / pins
			P(Box, FVector(5.0f, Side * 1.52f, -0.2f), FVector(0.5f, 0.15f, 0.5f), R0, Gunmetal);
		}
		P(Box, FVector(-6.9f, 0, 0.6f), FVector(2.6f, 2.6f, 1.0f), FRotator(12, 0, 0), Polymer);    // beavertail
		P(Box, FVector(-1.6f, 1.6f, 1.1f), FVector(1.1f, 0.4f, 0.6f), R0, Orange, 1.5f);            // fire selector (orange = auto)
		// --- Extended magazine
		P(Box, FVector(-7.4f, 0, -13.4f), FVector(3.0f, 2.5f, 6.2f), FRotator(-18, 0, 0), Gunmetal);
		P(Box, FVector(-8.4f, 0, -16.6f), FVector(3.7f, 2.9f, 1.2f), FRotator(-18, 0, 0), Orange);   // baseplate
		P(Box, FVector(-7.3f, 1.27f, -13.2f), FVector(0.4f, 0.1f, 4.4f), FRotator(-18, 0, 0), Cyan, 2.0f); // witness window
		// --- Slide (tagged: recoils)
		P(Box, FVector(2.5f, 0, 2.7f), FVector(19, 2.7f, 2.3f), R0, Titanium, 0.0f, TEXT("Slide"));
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			P(Box, FVector(2.5f, Side * 1.05f, 3.85f), FVector(19, 0.9f, 0.9f), FRotator(0, 0, 45), Titanium, 0.0f, TEXT("Slide")); // top chamfer
			for (int32 i = 0; i < 5; ++i)
			{
				P(Box, FVector(-5.8f + i * 0.75f, Side * 1.37f, 2.6f), FVector(0.3f, 0.1f, 1.9f), R0, Dark, 0.0f, TEXT("Slide")); // rear serrations
			}
			for (int32 i = 0; i < 3; ++i)
			{
				P(Box, FVector(9.6f + i * 0.75f, Side * 1.37f, 2.6f), FVector(0.3f, 0.1f, 1.9f), R0, Dark, 0.0f, TEXT("Slide")); // front serrations
			}
			P(Box, FVector(-3.0f, Side * 1.37f, 2.0f), FVector(4.0f, 0.1f, 0.18f), R0, Orange, 2.0f, TEXT("Slide"));
		}
		for (int32 i = 0; i < 3; ++i)
		{
			P(Box, FVector(5.4f + i * 1.6f, 0, 3.9f), FVector(1.0f, 1.2f, 0.25f), R0, Dark, 0.0f, TEXT("Slide"));  // lightening cuts
		}
		P(Box, FVector(1.2f, 1.36f, 3.0f), FVector(3.6f, 0.12f, 1.2f), R0, Dark, 0.0f, TEXT("Slide"));    // ejection port
		P(Box, FVector(1.2f, 0, 3.55f), FVector(3.4f, 1.3f, 0.5f), R0, Brass, 0.0f, TEXT("Slide"));       // barrel hood
		P(Box, FVector(12.8f, 0, 2.7f), FVector(2.4f, 2.7f, 2.3f), R0, Gunmetal, 0.0f, TEXT("Slide"));    // compensator
		P(Box, FVector(12.4f, 0, 3.9f), FVector(0.5f, 1.0f, 0.2f), R0, Orange, 4.0f, TEXT("Slide"));      // comp ports (heat)
		P(Box, FVector(13.3f, 0, 3.9f), FVector(0.5f, 1.0f, 0.2f), R0, Orange, 4.0f, TEXT("Slide"));
		P(Cyl, FVector(14.05f, 0, 2.4f), FVector(1.1f, 1.1f, 0.4f), AlongX, Dark, 0.0f, TEXT("Slide"));   // crown
		P(Cyl, FVector(11.0f, 0, 1.15f), FVector(0.6f, 0.6f, 1.6f), AlongX, Steel);                       // guide rod
		// --- Sights
		P(Box, FVector(-6.3f, 0, 4.2f), FVector(0.8f, 2.2f, 0.9f), R0, Dark, 0.0f, TEXT("Slide"));
		P(Sph, FVector(-5.85f, 0.6f, 4.35f), FVector(0.25f, 0.25f, 0.25f), R0, Tritium, 10.0f, TEXT("Slide"));
		P(Sph, FVector(-5.85f, -0.6f, 4.35f), FVector(0.25f, 0.25f, 0.25f), R0, Tritium, 10.0f, TEXT("Slide"));
		P(Box, FVector(11.0f, 0, 4.15f), FVector(0.6f, 0.5f, 0.9f), R0, Dark, 0.0f, TEXT("Slide"));
		P(Sph, FVector(10.75f, 0, 4.35f), FVector(0.25f, 0.25f, 0.25f), R0, Tritium, 10.0f, TEXT("Slide"));
		// Enclosed holo reflex sight on the frame-mounted bridge.
		P(Box, FVector(-1.0f, 0, 4.25f), FVector(4.6f, 2.3f, 0.6f), R0, Gunmetal, 0.0f, TEXT("Slide"));
		P(Box, FVector(0.6f, 1.0f, 5.6f), FVector(0.7f, 0.3f, 2.4f), R0, Dark, 0.0f, TEXT("Slide"));
		P(Box, FVector(0.6f, -1.0f, 5.6f), FVector(0.7f, 0.3f, 2.4f), R0, Dark, 0.0f, TEXT("Slide"));
		P(Box, FVector(0.6f, 0, 6.9f), FVector(0.7f, 2.3f, 0.3f), R0, Dark, 0.0f, TEXT("Slide"));
		P(Box, FVector(0.65f, 0, 5.6f), FVector(0.08f, 1.7f, 2.2f), R0, FLinearColor(0.1f, 0.8f, 1.0f, 0.35f), 0.6f, TEXT("Slide")); // glass
		P(Sph, FVector(0.7f, 0, 5.7f), FVector(0.22f, 0.22f, 0.22f), R0, Amber, 14.0f, TEXT("Slide"));    // reticle
		P(Box, FVector(-2.6f, 1.2f, 4.35f), FVector(1.2f, 0.1f, 0.4f), R0, Green, 3.0f, TEXT("Slide"));    // sight battery LED
		// --- Laser / light module with its armoured cable
		P(Box, FVector(8.2f, 0, -1.7f), FVector(4.4f, 2.4f, 1.8f), R0, Dark);
		P(Box, FVector(8.2f, 0, -0.75f), FVector(4.0f, 1.6f, 0.3f), R0, Gunmetal);
		P(Cyl, FVector(10.45f, 0.45f, -1.7f), FVector(0.7f, 0.7f, 0.3f), AlongX, Red, 8.0f);
		P(Cyl, FVector(10.45f, -0.5f, -1.7f), FVector(0.9f, 0.9f, 0.3f), AlongX, FLinearColor::White, 2.0f);
		P(Box, FVector(3.0f, 1.45f, -2.3f), FVector(8.0f, 0.35f, 0.35f), FRotator(4, 0, 0), Orange);    // cable
		// --- Rear ammo display (the TextRender counter sits in front of it)
		P(Box, FVector(-7.25f, 0, 2.6f), FVector(0.3f, 2.0f, 1.3f), R0, ScreenGreen, 0.6f);
		break;
	}

	case EMarsPropKind::TorqueWrench:
		P(Cyl, FVector(0, 0, 0), FVector(2.5f, 2.5f, 32), AlongX, Steel);
		P(Cyl, FVector(-10, 0, 0), FVector(3.3f, 3.3f, 12), AlongX, Orange);
		P(Box, FVector(17, 0, 0), FVector(6, 5, 3), R0, Titanium);
		P(Cyl, FVector(18, 0, 0), FVector(4, 4, 3.6f), R0, Dark);
		P(Box, FVector(2, 0, 1.4f), FVector(4, 1.6f, 0.4f), R0, Green, 2.0f);
		break;

	case EMarsPropKind::WeldGun:
		P(Box, FVector(0, 0, 0), FVector(20, 6, 9), R0, Hull);
		P(Box, FVector(-4, 0, -8), FVector(5, 5, 10), FRotator(15, 0, 0), Dark);
		P(Cyl, FVector(14, 0, 1), FVector(2.5f, 2.5f, 10), AlongX, Steel);
		P(Cyl, FVector(2, 0, 7), FVector(5, 5, 10), R0, Yellow);
		P(Sph, FVector(19.5f, 0, 1), FVector(1.6f, 1.6f, 1.6f), R0, Cyan, 10.0f);
		P(Box, FVector(-3, 3.1f, 1), FVector(6, 0.2f, 3), R0, Amber, 2.0f);
		break;

	case EMarsPropKind::Dosimeter:
		P(Box, FVector(0, 0, 0), FVector(3, 7, 12), R0, Yellow);
		P(Box, FVector(1.6f, 0, 2), FVector(0.3f, 5, 4), R0, ScreenGreen, 1.2f);
		P(Box, FVector(1.8f, 0.6f, 2), FVector(0.2f, 0.3f, 3.2f), FRotator(0, 0, 25), FLinearColor::White, 4.0f, TEXT("Needle"));
		P(Sph, FVector(1.6f, 2.6f, 5), FVector(1, 1, 1), R0, Red, 8.0f, TEXT("Led"));
		P(Cyl, FVector(0, 2.6f, 9), FVector(0.6f, 0.6f, 6), R0, Dark);
		P(Box, FVector(1.6f, -1.5f, -3.5f), FVector(0.4f, 2, 2), R0, Dark);
		break;

	case EMarsPropKind::QuantumTransistor:
		P(Box, FVector(0, 0, 0), FVector(8, 8, 1.5f), R0, Gold, 0.3f);
		P(Box, FVector(0, 0, 1.3f), FVector(6, 6, 1.2f), R0, White);
		for (int32 i = -1; i <= 1; ++i)
		{
			P(Cyl, FVector(0, i * 2.0f, 3.8f), FVector(1.5f, 1.5f, 4), R0, Cyan, 6.0f);
		}
		break;

	case EMarsPropKind::SyringeMethyleneBlue:
	case EMarsPropKind::SyringeAntitoxin:
	{
		const FLinearColor Liquid = Kind == EMarsPropKind::SyringeMethyleneBlue ? Cyan : Amber;
		P(Cyl, FVector(0, 0, 0), FVector(1.6f, 1.6f, 9), AlongX, Liquid, 1.5f);
		P(Cyl, FVector(-6.5f, 0, 0), FVector(1, 1, 4), AlongX, White);
		P(Box, FVector(-4.6f, 0, 0), FVector(0.4f, 3.5f, 1.2f), R0, White);
		P(Cyl, FVector(6, 0, 0), FVector(0.25f, 0.25f, 3), AlongX, Steel);
		P(Cyl, FVector(5, 0, 0), FVector(0.9f, 0.9f, 1.2f), AlongX, Orange);
		break;
	}

	case EMarsPropKind::AutoInjectorAmifostine:
		P(Cyl, FVector(0, 0, 0), FVector(2.2f, 2.2f, 14), AlongX, Purple, 0.6f);
		P(Cyl, FVector(7.5f, 0, 0), FVector(2, 2, 1.5f), AlongX, Orange, 1.5f);
		P(Box, FVector(0, 1.15f, 0), FVector(6, 0.1f, 1.2f), R0, White);
		break;

	case EMarsPropKind::KIBlister:
		P(Box, FVector(0, 0, 0), FVector(8, 5, 0.4f), R0, Titanium);
		for (int32 x = -1; x <= 1; ++x)
		{
			for (int32 y = 0; y <= 1; ++y)
			{
				P(Sph, FVector(x * 2.4f, -1.2f + y * 2.4f, 0.4f), FVector(1.2f, 1.2f, 0.8f), R0, White, 0.4f);
			}
		}
		break;

	case EMarsPropKind::O2Canister:
		P(Cyl, FVector(0, 0, 0), FVector(10, 10, 36), R0, White);
		P(Cyl, FVector(0, 0, 8), FVector(10.4f, 10.4f, 4), R0, Green, 0.8f);
		P(Cyl, FVector(0, 0, 20), FVector(4, 4, 5), R0, Steel);
		P(Box, FVector(0, 0, 24), FVector(8, 1.2f, 1.2f), R0, Dark);
		break;

	case EMarsPropKind::SampleContainer:
		P(Cyl, FVector(0, 0, 0), FVector(6, 6, 10), R0, FLinearColor(0.7f, 0.75f, 0.8f), 0.1f);
		P(Cyl, FVector(0, 0, -1), FVector(5, 5, 6), R0, Regolith);
		P(Cyl, FVector(0, 0, 5.8f), FVector(6.4f, 6.4f, 1.6f), R0, Red);
		break;

	case EMarsPropKind::RockSample:
		P(Box, FVector(0, 0, 0), FVector(10, 8, 7), FRotator(12, 30, 8), FLinearColor(0.35f, 0.17f, 0.11f));
		P(Box, FVector(3, -2, 2), FVector(6, 5, 5), FRotator(-20, 70, 15), FLinearColor(0.28f, 0.14f, 0.1f));
		break;

	case EMarsPropKind::BioCulture:
		P(Cyl, FVector(0, 0, 0), FVector(9, 9, 1.5f), R0, FLinearColor(0.7f, 0.8f, 0.8f), 0.1f);
		P(Cyl, FVector(0, 0, 0.3f), FVector(8, 8, 0.8f), R0, Green, 2.5f);
		P(Box, FVector(0, 4.6f, 0), FVector(3, 0.2f, 1.2f), R0, Orange, 1.0f);
		break;

	case EMarsPropKind::WaterPouch:
		P(Box, FVector(0, 0, 0), FVector(4, 10, 14), R0, FLinearColor(0.4f, 0.65f, 0.95f), 0.3f);
		P(Cyl, FVector(0, 3, 9), FVector(0.8f, 0.8f, 5), R0, White);
		break;

	case EMarsPropKind::FoodRation:
		P(Box, FVector(0, 0, 0), FVector(14, 7, 4), R0, Olive);
		P(Box, FVector(0, 0, 2.05f), FVector(10, 5, 0.1f), R0, White);
		break;

	case EMarsPropKind::ChlorellaTube:
		P(Cyl, FVector(0, 0, 0), FVector(3, 3, 12), AlongX, FLinearColor(0.1f, 0.5f, 0.15f), 0.5f);
		P(Cyl, FVector(6.5f, 0, 0), FVector(1.5f, 1.5f, 1.5f), AlongX, White);
		break;

	case EMarsPropKind::SpareParts:
		P(Box, FVector(0, 0, 0), FVector(20, 15, 10), R0, Hull);
		P(Box, FVector(2, 0, 5.5f), FVector(10, 8, 1), R0, FLinearColor(0.1f, 0.5f, 0.2f));
		P(Cyl, FVector(-5, 3, 6), FVector(3, 3, 2), R0, Gold);
		break;

	case EMarsPropKind::BeaconKit:
		P(Cyl, FVector(0, 0, 0), FVector(6, 6, 30), R0, Orange);
		P(Sph, FVector(0, 0, 17), FVector(5, 5, 5), R0, Amber, 6.0f);
		break;

	case EMarsPropKind::AmmoBox:
		P(Box, FVector(0, 0, 0), FVector(6, 3, 8), R0, FLinearColor(0.75f, 0.55f, 0.2f));
		P(Box, FVector(0, 0, 4.2f), FVector(5, 2.5f, 0.5f), R0, Dark);
		break;

	case EMarsPropKind::SealantCartridge:
		P(Cyl, FVector(0, 0, 0), FVector(5, 5, 18), R0, Yellow);
		P(Cone, FVector(0, 0, 11), FVector(3, 3, 4), R0, White);
		break;

	// ------------------------------------------------------------------ furniture
	case EMarsPropKind::Terminal:
		P(Box, FVector(0, 0, 50), FVector(40, 60, 100), R0, Hull);
		P(Box, FVector(20, 0, 98), FVector(26, 56, 4), R0, Dark);
		P(Box, FVector(10, 0, 128), FVector(6, 66, 44), FRotator(18, 0, 0), Dark);
		P(Box, FVector(13.5f, 0, 129), FVector(1, 58, 36), FRotator(18, 0, 0), ScreenGreen, 0.8f);
		P(Box, FVector(21, 0, 60), FVector(1, 50, 4), R0, Amber, 2.0f);
		break;

	case EMarsPropKind::PhotobioreactorRack:
		P(Box, FVector(0, 0, 5), FVector(60, 280, 10), R0, Steel);
		P(Box, FVector(0, 0, 235), FVector(60, 280, 10), R0, Steel);
		for (int32 i = 0; i < 6; ++i)
		{
			const float Y = -110.0f + i * 44.0f;
			P(Cyl, FVector(0, Y, 120), FVector(24, 24, 220), R0, FLinearColor(0.15f, 0.85f, 0.25f), 2.2f);
			P(Cyl, FVector(0, Y, 120), FVector(26, 26, 6), R0, Steel);
		}
		P(Cyl, FVector(-20, 0, 228), FVector(6, 6, 280), AlongY, White);
		break;

	case EMarsPropKind::RtgCore:
		P(Box, FVector(0, 0, 4), FVector(130, 130, 8), R0, Dark);
		P(Cyl, FVector(0, 0, 70), FVector(72, 72, 120), R0, FLinearColor(0.35f, 0.12f, 0.05f), 0.6f);
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = i * 45.0f;
			const FVector Offset(FMath::Cos(FMath::DegreesToRadians(A)) * 46.0f, FMath::Sin(FMath::DegreesToRadians(A)) * 46.0f, 70.0f);
			P(Box, Offset, FVector(30, 4, 110), FRotator(0, A, 0), Steel);
		}
		P(Cyl, FVector(0, 0, 134), FVector(60, 60, 8), R0, Yellow);
		P(Cyl, FVector(0, 0, 10), FVector(90, 90, 6), R0, Yellow);
		break;

	case EMarsPropKind::Dish:
		P(Cyl, FVector(0, 0, 125), FVector(16, 16, 250), R0, Hull);
		P(Cyl, FVector(30, 0, 270), FVector(240, 240, 18), FRotator(-45, 0, 0), White);
		P(Cone, FVector(80, 0, 320), FVector(20, 20, 40), FRotator(-45, 0, 0), Dark);
		P(Sph, FVector(95, 0, 335), FVector(8, 8, 8), R0, Red, 10.0f);
		break;

	case EMarsPropKind::ScannerBed:
		P(Box, FVector(0, 0, 30), FVector(200, 80, 60), R0, White);
		P(Box, FVector(0, 0, 65), FVector(190, 70, 10), R0, FLinearColor(0.1f, 0.18f, 0.3f));
		P(Box, FVector(0, -50, 120), FVector(14, 12, 120), R0, Hull);
		P(Box, FVector(0, 50, 120), FVector(14, 12, 120), R0, Hull);
		P(Box, FVector(0, 0, 184), FVector(22, 112, 12), R0, Hull);
		P(Box, FVector(0, 0, 176), FVector(4, 100, 3), R0, Cyan, 8.0f, TEXT("ScanBar"));
		P(Box, FVector(-90, 0, 90), FVector(12, 60, 40), R0, Dark);
		break;

	case EMarsPropKind::Spectrometer:
		P(Box, FVector(0, 0, 40), FVector(80, 120, 80), R0, White);
		P(Box, FVector(0, 0, 105), FVector(70, 90, 50), R0, Hull);
		P(Box, FVector(36, 0, 110), FVector(2, 50, 28), R0, Purple, 2.5f);
		P(Cyl, FVector(48, -35, 90), FVector(18, 18, 24), AlongX, Dark);
		P(Box, FVector(41, 40, 60), FVector(2, 24, 16), R0, ScreenGreen, 1.0f);
		break;

	case EMarsPropKind::Microscope:
		P(Box, FVector(0, 0, 38), FVector(80, 70, 76), R0, Hull);
		P(Box, FVector(0, 0, 81), FVector(50, 40, 10), R0, Dark);
		P(Cyl, FVector(-10, 0, 115), FVector(12, 12, 60), R0, White);
		P(Box, FVector(0, 0, 148), FVector(34, 24, 18), R0, White);
		P(Cyl, FVector(-14, 0, 162), FVector(5, 5, 14), FRotator(-40, 0, 0), Dark);
		P(Cyl, FVector(8, 0, 128), FVector(6, 6, 16), R0, Dark);
		P(Sph, FVector(8, 0, 90), FVector(5, 5, 2), R0, FLinearColor::White, 6.0f);
		break;

	case EMarsPropKind::ContainmentBox:
		P(Box, FVector(0, 0, 40), FVector(110, 80, 80), R0, Hull);
		P(Box, FVector(0, 0, 112), FVector(90, 64, 64), R0, FLinearColor(0.05f, 0.3f, 0.1f), 0.8f);
		for (int32 x = -1; x <= 1; x += 2)
		{
			for (int32 y = -1; y <= 1; y += 2)
			{
				P(Box, FVector(x * 46.0f, y * 33.0f, 112), FVector(5, 5, 68), R0, Yellow);
			}
		}
		P(Cyl, FVector(46, -16, 108), FVector(20, 20, 4), AlongX, Dark);
		P(Cyl, FVector(46, 16, 108), FVector(20, 20, 4), AlongX, Dark);
		P(Box, FVector(48, 0, 140), FVector(1, 24, 10), R0, Red, 3.0f);
		break;

	case EMarsPropKind::SuitRack:
		P(Box, FVector(-30, -60, 110), FVector(10, 10, 220), R0, Steel);
		P(Box, FVector(-30, 60, 110), FVector(10, 10, 220), R0, Steel);
		P(Box, FVector(-30, 0, 215), FVector(10, 130, 10), R0, Steel);
		// MCP suit: skin-tight torso, limbs with blue tension bands, gas helmet with gold visor, PLSS pack.
		P(Cyl, FVector(0, 0, 140), FVector(38, 30, 60), R0, White);
		P(Sph, FVector(0, 0, 190), FVector(32, 32, 32), R0, White);
		P(Box, FVector(13, 0, 190), FVector(8, 22, 14), R0, Gold, 0.5f);
		P(Box, FVector(-24, 0, 148), FVector(14, 36, 50), R0, Hull);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			P(Cyl, FVector(0, Side * 11.0f, 75), FVector(14, 14, 72), R0, White);
			P(Cyl, FVector(0, Side * 11.0f, 85), FVector(14.5f, 14.5f, 4), R0, Blue, 0.6f);
			P(Cyl, FVector(0, Side * 26.0f, 140), FVector(10, 10, 58), R0, White);
			P(Cyl, FVector(0, Side * 26.0f, 140), FVector(10.5f, 10.5f, 4), R0, Blue, 0.6f);
		}
		break;

	case EMarsPropKind::WaterDispenser:
		P(Box, FVector(0, 0, 70), FVector(50, 50, 140), R0, White);
		P(Box, FVector(26, 0, 95), FVector(2, 30, 20), R0, Blue, 2.0f);
		P(Box, FVector(30, 0, 70), FVector(10, 8, 6), R0, Steel);
		P(Cyl, FVector(0, 0, 162), FVector(40, 40, 44), R0, FLinearColor(0.3f, 0.55f, 0.95f), 0.8f);
		break;

	case EMarsPropKind::FoodLocker:
		P(Box, FVector(0, 0, 90), FVector(60, 120, 180), R0, Olive);
		P(Box, FVector(31, 0, 90), FVector(1, 2, 170), R0, Dark);
		P(Box, FVector(31, 0, 120), FVector(1, 110, 2), R0, Dark);
		P(Box, FVector(31, 0, 60), FVector(1, 110, 2), R0, Dark);
		P(Box, FVector(31, 30, 150), FVector(1, 30, 12), R0, Amber, 1.5f);
		break;

	case EMarsPropKind::O2RefillRack:
		for (int32 i = -1; i <= 1; ++i)
		{
			P(Cyl, FVector(0, i * 40.0f, 80), FVector(30, 30, 160), R0, White);
			P(Cyl, FVector(0, i * 40.0f, 120), FVector(31, 31, 8), R0, Green, 0.8f);
		}
		P(Cyl, FVector(0, 0, 168), FVector(8, 8, 130), AlongY, Steel);
		P(Cyl, FVector(20, 0, 120), FVector(24, 24, 4), AlongX, White);
		P(Box, FVector(22.5f, 0, 124), FVector(0.5f, 1.5f, 10), FRotator(0, 0, 35), Red, 3.0f);
		break;

	case EMarsPropKind::Crate:
		P(Box, FVector(0, 0, 30), FVector(80, 80, 60), R0, FLinearColor(0.35f, 0.36f, 0.33f));
		P(Box, FVector(0, 0, 61), FVector(82, 20, 2), R0, Yellow);
		break;

	case EMarsPropKind::AirlockValve:
		P(Cyl, FVector(0, 0, 110), FVector(12, 12, 160), R0, Steel);
		P(Cyl, FVector(12, 0, 120), FVector(42, 42, 5), AlongX, Red);
		P(Box, FVector(14, 0, 120), FVector(2, 40, 4), R0, Red);
		P(Box, FVector(14, 0, 120), FVector(2, 4, 40), R0, Red);
		P(Cyl, FVector(10, 0, 165), FVector(20, 20, 3), AlongX, White);
		P(Box, FVector(12, 0, 168), FVector(0.5f, 1.5f, 8), FRotator(0, 0, -40), Red, 3.0f);
		break;

	case EMarsPropKind::ScrubberUnit:
		P(Box, FVector(0, 0, 75), FVector(70, 100, 150), R0, Hull);
		for (int32 i = 0; i < 5; ++i)
		{
			P(Box, FVector(36, 0, 40.0f + i * 18.0f), FVector(1, 80, 4), R0, Dark);
		}
		P(Box, FVector(36, 0, 135), FVector(1, 40, 12), R0, Cyan, 2.0f);
		P(Cyl, FVector(0, 0, 160), FVector(30, 30, 20), R0, Steel);
		break;

	// ------------------------------------------------------------------ environmental detail
	case EMarsPropKind::PipeRun:
		// Three coolant lines with insulation, colour bands and brackets.
		P(Cyl, FVector(200, 0, 0), FVector(11, 11, 400), AlongX, White);
		P(Cyl, FVector(200, 15, 2), FVector(8, 8, 400), AlongX, Steel);
		P(Cyl, FVector(200, 27, -2), FVector(6, 6, 400), AlongX, FLinearColor(0.2f, 0.35f, 0.75f));
		for (int32 i = 0; i < 4; ++i)
		{
			P(Box, FVector(50 + i * 100, 13, 9), FVector(5, 40, 4), R0, Dark);
			P(Cyl, FVector(30 + i * 100, 0, 0), FVector(11.6f, 11.6f, 3), AlongX, i % 2 ? Green : Yellow, 0.4f);
		}
		P(Box, FVector(260, 15, -8), FVector(6, 6, 10), R0, Red, 0.6f);
		break;

	case EMarsPropKind::CableTray:
		P(Box, FVector(200, 0, 0), FVector(400, 32, 3), R0, Dark);
		P(Box, FVector(200, -16, 4), FVector(400, 2, 8), R0, Steel);
		P(Box, FVector(200, 16, 4), FVector(400, 2, 8), R0, Steel);
		{
			const FLinearColor CableColors[4] = { Orange, Blue, Dark, Yellow };
			for (int32 i = 0; i < 4; ++i)
			{
				P(Cyl, FVector(200, -10 + i * 6.5f, 4), FVector(3.5f, 3.5f, 400), AlongX, CableColors[i]);
			}
		}
		for (int32 i = 0; i < 3; ++i)
		{
			P(Box, FVector(70 + i * 130, 0, 20), FVector(3, 3, 40), R0, Steel);
		}
		break;

	case EMarsPropKind::ValveWheel:
	{
		P(Cyl, FVector(0, 0, 130), FVector(12, 12, 260), R0, Steel);
		P(Cyl, FVector(14, 0, 130), FVector(16, 16, 28), AlongX, Hull);
		// Hand-wheel on a spinning pivot (axis = pivot Z, pointing out of the wall).
		USceneComponent* Wheel = AddPivot(Parent, FVector(30, 0, 130) * S, FRotator(90.0f, 0.0f, 0.0f), TEXT("Spin"));
		if (Wheel)
		{
			for (int32 i = 0; i < 12; ++i)
			{
				const float A = i * 30.0f;
				const FVector RimPos(FMath::Cos(FMath::DegreesToRadians(A)) * 22.0f, FMath::Sin(FMath::DegreesToRadians(A)) * 22.0f, 0.0f);
				if (UStaticMeshComponent* Rim = AddPart(Wheel, Box, RimPos * S, FVector(12, 4, 4) * S, FRotator(0, A + 90.0f, 0), Red, 0.0f, bCol)) { Parts.Add(Rim); }
			}
			for (int32 i = 0; i < 3; ++i)
			{
				if (UStaticMeshComponent* Spoke = AddPart(Wheel, Box, FVector::ZeroVector, FVector(44, 3, 3) * S, FRotator(0, i * 60.0f, 0), Red, 0.0f, bCol)) { Parts.Add(Spoke); }
			}
			if (UStaticMeshComponent* Hub = AddPart(Wheel, Cyl, FVector::ZeroVector, FVector(8, 8, 6) * S, R0, Steel, 0.0f, bCol)) { Parts.Add(Hub); }
		}
		P(Cyl, FVector(10, 22, 180), FVector(18, 18, 3), AlongX, White);
		P(Box, FVector(12, 22, 183), FVector(0.5f, 1.2f, 7), FRotator(0, 0, -30), Red, 3.0f);
		break;
	}

	case EMarsPropKind::ServerRack:
		P(Box, FVector(0, 0, 100), FVector(60, 80, 200), R0, Dark);
		P(Box, FVector(31, 0, 100), FVector(1, 76, 196), R0, FLinearColor(0.12f, 0.13f, 0.15f));
		for (int32 Row = 0; Row < 9; ++Row)
		{
			const float Z = 25.0f + Row * 19.0f;
			P(Box, FVector(32, 0, Z), FVector(1, 70, 1.5f), R0, Steel);
			for (int32 Col = 0; Col < 4; ++Col)
			{
				const FLinearColor Led = (Row + Col) % 3 == 0 ? Amber : Green;
				P(Box, FVector(32.5f, -26 + Col * 5.0f, Z + 6), FVector(0.8f, 2.0f, 1.5f), R0, Led, 8.0f, TEXT("Blink"));
			}
			P(Box, FVector(32.5f, 15, Z + 6), FVector(0.5f, 30, 6), R0, FLinearColor(0.05f, 0.05f, 0.06f));
		}
		P(Box, FVector(32, 0, 196), FVector(1, 60, 5), R0, Cyan, 2.0f);
		break;

	case EMarsPropKind::StructuralRib:
		// Flush wall pilaster (no floor footing - those read as stray blocks on the floor).
		P(Box, FVector(0, 0, 150), FVector(8, 18, 300), R0, FLinearColor(0.22f, 0.23f, 0.25f));
		P(Box, FVector(4.2f, 0, 150), FVector(1, 8, 300), R0, Dark);
		for (int32 i = 0; i < 5; ++i)
		{
			P(Cyl, FVector(4.6f, 0, 30 + i * 60), FVector(2.5f, 2.5f, 1.0f), AlongX, Steel);
		}
		break;

	case EMarsPropKind::GlassPartition:
		P(Box, FVector(0, -100, 115), FVector(8, 8, 230), R0, Steel);
		P(Box, FVector(0, 100, 115), FVector(8, 8, 230), R0, Steel);
		P(Box, FVector(0, 0, 4), FVector(10, 208, 8), R0, Dark);
		P(Box, FVector(0, 0, 226), FVector(10, 208, 8), R0, Steel);
		P(Box, FVector(0, 0, 115), FVector(3, 192, 214), R0, FLinearColor(0.75f, 0.85f, 0.95f, 0.35f), 0.05f);
		P(Box, FVector(0, 0, 120), FVector(5, 200, 4), R0, Steel);
		P(Box, FVector(3, 60, 160), FVector(0.5f, 50, 10), R0, Amber, 1.5f);
		break;

	case EMarsPropKind::BioFlora:
	{
		// Alien stalk cluster: segmented curving stems topped by bioluminescent bulbs (cyan/magenta).
		P(Sph, FVector(0, 0, 6), FVector(70, 60, 24), R0, FLinearColor(0.08f, 0.1f, 0.12f));
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = i * 60.0f + 17.0f;
			const FVector Dir(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0.0f);
			const float H = 60.0f + (i * 23 % 50);
			FVector Pos = Dir * 10.0f + FVector(0, 0, 10);
			for (int32 Seg = 0; Seg < 3; ++Seg)
			{
				const float Lean = 10.0f + Seg * 12.0f;
				const FRotator Rot = FRotationMatrix::MakeFromZ(FVector(Dir.X * Lean / 40.0f, Dir.Y * Lean / 40.0f, 1.0f)).Rotator();
				P(Cyl, Pos + Rot.RotateVector(FVector(0, 0, H / 6.0f)), FVector(4.0f - Seg, 4.0f - Seg, H / 3.0f), Rot, FLinearColor(0.05f, 0.25f, 0.28f), 0.2f, TEXT("Pulse"));
				Pos += Rot.RotateVector(FVector(0, 0, H / 3.0f));
			}
			const FLinearColor Glow = i % 2 ? FLinearColor(0.1f, 0.9f, 1.0f) : FLinearColor(0.9f, 0.2f, 1.0f);
			P(Sph, Pos, FVector(13, 13, 16), R0, Glow, 3.5f, TEXT("Pulse"));
			P(Sph, Pos + FVector(0, 0, 10), FVector(5, 5, 5), R0, FLinearColor::White, 6.0f, TEXT("Pulse"));
		}
		break;
	}

	case EMarsPropKind::SporePods:
		for (int32 i = 0; i < 7; ++i)
		{
			const float A = i * 51.4f;
			const float R = 18.0f + (i % 3) * 14.0f;
			const FVector Base(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, 0.0f);
			const float H = 20.0f + (i * 13 % 35);
			P(Cyl, Base + FVector(0, 0, H * 0.5f), FVector(2.5f, 2.5f, H), R0, FLinearColor(0.15f, 0.12f, 0.08f));
			P(Sph, Base + FVector(0, 0, H + 10), FVector(22, 22, 26), R0, FLinearColor(0.6f, 0.85f, 0.4f, 0.45f), 0.3f, TEXT("Breathe"));
			P(Sph, Base + FVector(0, 0, H + 10), FVector(9, 9, 11), R0, FLinearColor(1.0f, 0.7f, 0.2f), 4.0f, TEXT("Pulse"));
		}
		break;

	case EMarsPropKind::Mycelium:
		// Branching fungal network creeping across the deck.
		for (int32 i = 0; i < 18; ++i)
		{
			const float A = i * 137.5f;
			const float Len = 40.0f + (i * 29 % 80);
			const float R = 20.0f + (i * 17 % 90);
			const FVector Start(FMath::Cos(FMath::DegreesToRadians(A)) * R * 0.4f, FMath::Sin(FMath::DegreesToRadians(A)) * R * 0.4f, 1.5f);
			const FRotator Rot(0.0f, A + 20.0f, 0.0f);
			P(Box, Start + Rot.RotateVector(FVector(Len * 0.5f, 0, 0)), FVector(Len, 2.0f, 1.5f), Rot, FLinearColor(0.2f, 0.8f, 0.9f), 1.8f, TEXT("Pulse"));
			P(Sph, Start + Rot.RotateVector(FVector(Len, 0, 0)), FVector(6, 6, 4), R0, FLinearColor(0.4f, 1.0f, 0.9f), 3.0f, TEXT("Pulse"));
		}
		P(Sph, FVector(0, 0, 3), FVector(30, 30, 8), R0, FLinearColor(0.15f, 0.5f, 0.55f), 1.0f, TEXT("Pulse"));
		break;

	case EMarsPropKind::Terrarium:
		P(Box, FVector(0, 0, 40), FVector(120, 70, 80), R0, Hull);
		P(Box, FVector(0, 0, 81), FVector(116, 66, 2), R0, FLinearColor(0.2f, 0.1f, 0.06f));
		P(Box, FVector(0, 0, 122), FVector(114, 64, 80), R0, FLinearColor(0.7f, 0.9f, 0.85f, 0.25f), 0.05f);
		for (int32 x = -1; x <= 1; x += 2)
		{
			for (int32 y = -1; y <= 1; y += 2)
			{
				P(Box, FVector(x * 57.0f, y * 32.0f, 122), FVector(3, 3, 82), R0, Steel);
			}
		}
		P(Box, FVector(0, 0, 164), FVector(118, 68, 4), R0, Steel);
		P(Box, FVector(0, 0, 161), FVector(100, 4, 2), R0, FLinearColor(0.8f, 0.3f, 1.0f), 4.0f);
		// Miniature flora inside.
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector Pos(-35.0f + i * 23.0f, (i % 2 ? 12.0f : -10.0f), 82.0f);
			P(Cyl, Pos + FVector(0, 0, 15), FVector(2, 2, 30), R0, FLinearColor(0.05f, 0.25f, 0.28f));
			P(Sph, Pos + FVector(0, 0, 33), FVector(8, 8, 10), R0, i % 2 ? FLinearColor(0.1f, 0.9f, 1.0f) : FLinearColor(0.9f, 0.2f, 1.0f), 4.0f, TEXT("Pulse"));
		}
		break;

	case EMarsPropKind::Shuttle:
		// Supply / extraction shuttle (~18 m), engines on tag "Thrust".
		P(Cyl, FVector(0, 0, 0), FVector(320, 280, 1500), AlongX, White);
		P(Cone, FVector(900, 0, 0), FVector(320, 280, 320), FRotator(-90.0f, 0.0f, 0.0f), White);
		P(Box, FVector(820, 0, 70), FVector(120, 160, 40), FRotator(10, 0, 0), Cyan, 1.5f);
		P(Box, FVector(-150, 0, -40), FVector(700, 1400, 30), R0, Hull);
		P(Box, FVector(-600, 0, 220), FVector(300, 20, 380), FRotator(0, 0, 0), Hull);
		P(Box, FVector(100, 0, 150), FVector(600, 120, 20), R0, Orange);
		for (int32 i = -1; i <= 1; ++i)
		{
			P(Cone, FVector(-830, i * 100.0f, 0), FVector(120, 120, 140), FRotator(90.0f, 0.0f, 0.0f), Dark);
			P(Sph, FVector(-900, i * 100.0f, 0), FVector(80, 80, 80), R0, FLinearColor(1.0f, 0.55f, 0.15f), 0.0f, TEXT("Thrust"));
		}
		for (int32 i = -1; i <= 1; i += 2)
		{
			P(Box, FVector(-200, i * 600.0f, -60), FVector(120, 20, 20), R0, Red, 6.0f, TEXT("Blink"));
		}
		// Hard-surface detail: armour panel bands, cockpit glazing, radiators, RCS quads.
		for (int32 i = 0; i < 6; ++i)
		{
			P(Cyl, FVector(-650.0f + i * 230.0f, 0, 0), FVector(330, 290, 18), AlongX, i % 2 ? Hull : Steel);
		}
		P(Box, FVector(760, 0, 95), FVector(160, 200, 30), FRotator(14, 0, 0), FLinearColor(0.15f, 0.45f, 0.6f, 0.5f), 1.0f);
		for (int32 i = -1; i <= 1; i += 2)
		{
			P(Box, FVector(-250, i * 170.0f, 120), FVector(500, 6, 90), FRotator(0, 0, i * 25.0f), FLinearColor(0.85f, 0.85f, 0.8f));  // radiators
			P(Box, FVector(600, i * 150.0f, 40), FVector(30, 30, 30), R0, Dark);                                                       // RCS quads
			P(Box, FVector(-500, i * 150.0f, 40), FVector(30, 30, 30), R0, Dark);
			P(Box, FVector(100, i * 162.0f, -20), FVector(900, 4, 10), R0, Cyan, 3.0f);                                                // trim
		}
		// Belly VTOL lift engines + landing legs (the departure lifts on these before transitioning).
		for (int32 x = -1; x <= 1; x += 2)
		{
			for (int32 y = -1; y <= 1; y += 2)
			{
				const FVector Pod(x * 420.0f, y * 230.0f, -130.0f);
				P(Cyl, Pod, FVector(150, 150, 80), R0, Dark);
				P(Cyl, Pod - FVector(0, 0, 45.0f), FVector(120, 120, 12), R0, FLinearColor(1.0f, 0.55f, 0.15f), 0.0f, TEXT("Thrust"));
				P(Cyl, Pod + FVector(x * 60.0f, y * 60.0f, -60.0f), FVector(20, 20, 140), FRotator(x * 15.0f, 0, -y * 15.0f), Steel);    // strut
				P(Cyl, Pod + FVector(x * 80.0f, y * 80.0f, -128.0f), FVector(70, 70, 10), R0, Dark);                                    // foot
			}
		}
		break;

	// ------------------------------------------------------------------ storage furniture
	case EMarsPropKind::WallLocker:
	case EMarsPropKind::MedCabinet:
	{
		const bool bMed = Kind == EMarsPropKind::MedCabinet;
		const float D = bMed ? 40.0f : 50.0f, Wd = 90.0f, Ht = bMed ? 170.0f : 200.0f;
		const FLinearColor Body = bMed ? FLinearColor(0.86f, 0.88f, 0.9f) : FLinearColor(0.28f, 0.32f, 0.36f);
		const FLinearColor Accent = bMed ? FLinearColor(0.1f, 0.85f, 1.0f) : BuildTint;
		P(Box, FVector(-D * 0.5f + 2.0f, 0, Ht * 0.5f), FVector(4, Wd, Ht), R0, Dark);                       // back
		P(Box, FVector(0, -Wd * 0.5f + 2.0f, Ht * 0.5f), FVector(D, 4, Ht), R0, Body);                        // sides
		P(Box, FVector(0, Wd * 0.5f - 2.0f, Ht * 0.5f), FVector(D, 4, Ht), R0, Body);
		P(Box, FVector(0, 0, Ht - 2.0f), FVector(D, Wd, 4), R0, Body);                                        // top / bottom
		P(Box, FVector(0, 0, 6.0f), FVector(D, Wd, 12), R0, Dark);
		P(Box, FVector(D * 0.5f - 1.0f, 0, Ht + 3.0f), FVector(3, Wd, 4), R0, Accent, 4.0f);                  // light bar
		// Shelves with neatly stowed kit.
		const int32 Shelves = bMed ? 3 : 4;
		for (int32 s = 0; s < Shelves; ++s)
		{
			const float Z = 22.0f + s * (Ht - 40.0f) / Shelves;
			P(Box, FVector(0, 0, Z), FVector(D - 6, Wd - 8, 2), R0, Steel);
			for (int32 k = 0; k < 3; ++k)
			{
				const FLinearColor Item = bMed ? (k == 1 ? Red : FLinearColor::White) : (k == 0 ? Orange : k == 1 ? Olive : Blue);
				P(Box, FVector(-4.0f, -26.0f + k * 26.0f, Z + 9.0f), FVector(D - 18, 20, 16), R0, Item * 0.85f);
			}
		}
		// Doors on pivots (DoorL / DoorR), opened by AMarsStorageLocker.
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			USceneComponent* Hinge = AddPivot(Parent, FVector(D * 0.5f, Side * (Wd * 0.5f), 0.0f) * S, R0, Side < 0 ? TEXT("DoorL") : TEXT("DoorR"));
			if (!Hinge) { continue; }
			auto DP = [&](EMarsPropShape Sh, FVector L, FVector Sz, FLinearColor C, float E = 0.0f)
			{
				if (UStaticMeshComponent* Part = AddPart(Hinge, Sh, L * S, Sz * S, R0, C, FMath::Max(E, SelfIllumination), false)) { Parts.Add(Part); }
			};
			const float Y = -Side * Wd * 0.25f;
			DP(Box, FVector(1.0f, Y, Ht * 0.5f), FVector(2.5f, Wd * 0.5f - 1.0f, Ht - 6.0f), Body);
			DP(Box, FVector(2.6f, Y, Ht * 0.5f), FVector(0.6f, Wd * 0.5f - 10.0f, Ht - 20.0f), Body * 0.85f);  // inset panel
			for (int32 v = 0; v < 5; ++v)
			{
				DP(Box, FVector(3.0f, Y, Ht * 0.72f + v * 5.0f), FVector(0.6f, Wd * 0.3f, 1.4f), Dark);           // vent slats
			}
			DP(Box, FVector(3.5f, -Side * 4.0f, Ht * 0.5f), FVector(2.0f, 2.0f, 22.0f), Steel);                  // handle
			DP(Box, FVector(3.0f, Y, Ht * 0.3f), FVector(0.5f, Wd * 0.4f, 3.0f), Accent, 3.0f);                 // ID stripe
			if (bMed)
			{
				DP(Box, FVector(3.2f, Y, Ht * 0.55f), FVector(0.5f, 14.0f, 4.0f), Red, 2.0f);
				DP(Box, FVector(3.2f, Y, Ht * 0.55f), FVector(0.5f, 4.0f, 14.0f), Red, 2.0f);
			}
		}
		break;
	}

	case EMarsPropKind::CargoRack:
	{
		// Magnetised cargo rack: open frame, clamped crates, blue magnet strips.
		const float D = 70.0f, Wd = 140.0f, Ht = 190.0f;
		for (int32 x = -1; x <= 1; x += 2)
		{
			for (int32 y = -1; y <= 1; y += 2)
			{
				P(Box, FVector(x * (D * 0.5f - 3.0f), y * (Wd * 0.5f - 3.0f), Ht * 0.5f), FVector(6, 6, Ht), R0, Yellow * 0.8f);
			}
		}
		for (int32 s = 0; s < 3; ++s)
		{
			const float Z = 10.0f + s * 62.0f;
			P(Box, FVector(0, 0, Z), FVector(D, Wd, 4), R0, Steel);
			P(Box, FVector(D * 0.5f - 1.0f, 0, Z + 3.0f), FVector(2, Wd - 10, 2), R0, BuildTint, 5.0f);         // mag-lock strip
			for (int32 k = 0; k < 2; ++k)
			{
				const float Y = -32.0f + k * 64.0f;
				P(Box, FVector(-2.0f, Y, Z + 24.0f), FVector(52, 56, 42), R0, k == 0 ? Olive : Hull * 0.8f);
				P(Box, FVector(-2.0f, Y, Z + 24.0f), FVector(54, 6, 44), R0, Orange);                               // strap
				P(Box, FVector(25.0f, Y, Z + 32.0f), FVector(1, 20, 8), R0, FLinearColor::White, 0.5f);            // label
			}
		}
		P(Box, FVector(0, 0, Ht), FVector(D, Wd, 4), R0, Yellow * 0.8f);
		break;
	}

	case EMarsPropKind::Bunk:
	{
		// Double crew bunk along +X with privacy curtain and reading lights.
		const float L = 210.0f, Wd = 100.0f;
		for (int32 x = -1; x <= 1; x += 2)
		{
			for (int32 y = -1; y <= 1; y += 2)
			{
				P(Box, FVector(x * (L * 0.5f - 3.0f), y * (Wd * 0.5f - 3.0f), 90.0f), FVector(6, 6, 180), R0, Steel);
			}
		}
		for (int32 b = 0; b < 2; ++b)
		{
			const float Z = 35.0f + b * 95.0f;
			P(Box, FVector(0, 0, Z), FVector(L, Wd, 6), R0, Hull * 0.7f);
			P(Box, FVector(0, 0, Z + 9.0f), FVector(L - 10, Wd - 10, 12), R0, FLinearColor(0.2f, 0.27f, 0.38f));     // mattress
			P(Box, FVector(-L * 0.5f + 25.0f, 0, Z + 19.0f), FVector(30, Wd - 30, 8), R0, FLinearColor(0.8f, 0.8f, 0.78f)); // pillow
			P(Box, FVector(L * 0.15f, 0, Z + 16.0f), FVector(L * 0.5f, Wd - 12, 3), R0, FLinearColor(0.45f, 0.18f, 0.12f)); // blanket
			P(Box, FVector(-L * 0.5f + 8.0f, -Wd * 0.5f + 8.0f, Z + 45.0f), FVector(4, 10, 3), R0, Amber, 6.0f);  // reading light
		}
		P(Box, FVector(0, Wd * 0.5f + 2.0f, 120.0f), FVector(L * 0.6f, 1.5f, 110.0f), R0, FLinearColor(0.25f, 0.4f, 0.6f, 0.55f), 0.2f); // curtain
		P(Box, FVector(0, -Wd * 0.5f + 1.0f, 160.0f), FVector(L - 20.0f, 2.0f, 30.0f), R0, Dark);               // personal shelf
		break;
	}

	// ------------------------------------------------------------------ sector dressing (use BuildTint / BuildExtent)
	case EMarsPropKind::SectorTrim:
		P(Box, FVector(0, 0, 0), FVector(BuildExtent.X, 4, 10), R0, Dark);
		P(Box, FVector(0, 2.5f, 0), FVector(BuildExtent.X - 4.0f, 1.5f, 3.0f), R0, BuildTint, 8.0f);
		for (float X = -BuildExtent.X * 0.5f + 40.0f; X < BuildExtent.X * 0.5f; X += 120.0f)
		{
			P(Box, FVector(X, 2.0f, 0), FVector(4, 3, 12), R0, Steel);
		}
		break;

	case EMarsPropKind::HazardStripe:
	{
		const float Len = BuildExtent.X, Ht = BuildExtent.Z;
		P(Box, FVector(0, 0, 0), FVector(Len, 1.0f, Ht), R0, FLinearColor(0.05f, 0.05f, 0.05f));
		for (float X = -Len * 0.5f + 10.0f; X < Len * 0.5f - 5.0f; X += 28.0f)
		{
			P(Box, FVector(X, 0.8f, 0), FVector(12.0f, 0.6f, Ht * 0.92f), FRotator(35.0f, 0, 0), Yellow, 0.4f);
		}
		P(Box, FVector(0, 1.0f, Ht * 0.5f), FVector(Len, 1.2f, 3.0f), R0, Dark);
		P(Box, FVector(0, 1.0f, -Ht * 0.5f), FVector(Len, 1.2f, 3.0f), R0, Dark);
		break;
	}

	case EMarsPropKind::HoloScreen:
	{
		const float Wd = BuildExtent.Y, Ht = BuildExtent.Z;
		const FLinearColor Glass(BuildTint.R, BuildTint.G, BuildTint.B, 0.3f);
		P(Box, FVector(-4.0f, 0, -Ht * 0.5f - 6.0f), FVector(8, 30, 6), R0, Dark);                                // projector
		P(Box, FVector(-2.0f, 0, -Ht * 0.5f - 2.0f), FVector(2, 20, 2), R0, BuildTint, 10.0f);
		P(Box, FVector(0, 0, 0), FVector(0.6f, Wd, Ht), R0, Glass, 1.2f);                                          // holo pane
		P(Box, FVector(0.4f, 0, Ht * 0.42f), FVector(0.4f, Wd * 0.9f, 3.0f), R0, BuildTint, 6.0f);                // header bar
		for (int32 i = 0; i < 4; ++i)
		{
			P(Box, FVector(0.4f, -Wd * 0.3f + (i % 2) * Wd * 0.15f, Ht * 0.25f - i * Ht * 0.15f), FVector(0.4f, Wd * (0.3f + 0.12f * i), 2.0f), R0, BuildTint, 5.0f, TEXT("Blink"));
		}
		P(Box, FVector(0.4f, Wd * 0.3f, -Ht * 0.15f), FVector(0.4f, Wd * 0.2f, Ht * 0.35f), R0, FLinearColor(BuildTint.R, BuildTint.G, BuildTint.B, 0.5f), 3.0f, TEXT("Pulse"));
		// Corner brackets.
		for (int32 y = -1; y <= 1; y += 2)
		{
			for (int32 z = -1; z <= 1; z += 2)
			{
				P(Box, FVector(0.3f, y * Wd * 0.5f, z * Ht * 0.5f), FVector(0.5f, 8.0f, 2.0f), R0, BuildTint, 8.0f);
				P(Box, FVector(0.3f, y * Wd * 0.5f, z * Ht * 0.5f), FVector(0.5f, 2.0f, 8.0f), R0, BuildTint, 8.0f);
			}
		}
		break;
	}

	case EMarsPropKind::SteamVent:
		P(Box, FVector(0, 0, 0), FVector(6, 50, 36), R0, Dark);
		for (int32 i = 0; i < 6; ++i)
		{
			P(Box, FVector(3.5f, 0, -14.0f + i * 5.6f), FVector(1.5f, 46, 2.0f), FRotator(0, 0, 0), Steel);
		}
		P(Cyl, FVector(-6.0f, 0, 26.0f), FVector(10, 10, 30), R0, Steel);
		for (int32 i = 0; i < 4; ++i)
		{
			P(Sph, FVector(14.0f + i * 10.0f, FMath::Sin(i * 1.7f) * 6.0f, 6.0f + i * 14.0f), FVector(18 + i * 8.0f, 18 + i * 8.0f, 14 + i * 6.0f), R0,
				FLinearColor(0.9f, 0.92f, 0.95f, 0.16f - i * 0.03f), 0.3f, TEXT("Breathe"));
		}
		P(Box, FVector(3.5f, 0, 22.0f), FVector(1.0f, 20, 3), R0, Orange, 2.0f);
		break;

	case EMarsPropKind::CableBundle:
	{
		// Four sagging data / power cables along +X (catenary), with clamps.
		const float Len = BuildExtent.X;
		const FLinearColor Cols[4] = { Dark, Orange, FLinearColor(0.1f, 0.2f, 0.6f), BuildTint };
		for (int32 c = 0; c < 4; ++c)
		{
			const float Y = -9.0f + c * 6.0f;
			const float Sag = 22.0f + c * 6.0f;
			const int32 Segs = 10;
			for (int32 s = 0; s < Segs; ++s)
			{
				const float T0 = static_cast<float>(s) / Segs, T1 = static_cast<float>(s + 1) / Segs;
				const FVector A(-Len * 0.5f + Len * T0, Y, -Sag * 4.0f * T0 * (1.0f - T0));
				const FVector B(-Len * 0.5f + Len * T1, Y, -Sag * 4.0f * T1 * (1.0f - T1));
				const FVector Mid = (A + B) * 0.5f;
				const FRotator Rot = FRotationMatrix::MakeFromZ((B - A).GetSafeNormal()).Rotator();
				P(Cyl, Mid, FVector(3.0f, 3.0f, (B - A).Size() + 1.0f), Rot, Cols[c], c == 3 ? 2.0f : 0.0f);
			}
		}
		P(Box, FVector(-Len * 0.5f, 0, 2), FVector(8, 30, 8), R0, Steel);
		P(Box, FVector(Len * 0.5f, 0, 2), FVector(8, 30, 8), R0, Steel);
		break;
	}

	case EMarsPropKind::WallPanels:
	{
		// Modular wall cladding across BuildExtent.X x BuildExtent.Z, a lit trim line at hip height.
		const float Len = BuildExtent.X, Ht = BuildExtent.Z;
		int32 Idx = 0;
		for (float X = -Len * 0.5f + 60.0f; X <= Len * 0.5f - 60.0f; X += 120.0f, ++Idx)
		{
			const FLinearColor Gun(0.24f, 0.25f, 0.27f);
			const FLinearColor Shade = Idx % 3 == 0 ? Gun : Idx % 3 == 1 ? Gun * 1.12f : Gun * 0.88f;
			P(Box, FVector(X, 0, Ht * 0.55f), FVector(112, 3, Ht * 0.8f), R0, Shade);
			P(Box, FVector(X, 1.8f, Ht * 0.55f), FVector(96, 1, Ht * 0.6f), R0, Shade * 0.9f);
			if (Idx % 4 == 1)
			{
				for (int32 v = 0; v < 6; ++v)
				{
					P(Box, FVector(X, 2.5f, Ht * 0.75f + v * 5.0f), FVector(70, 1, 2), R0, Dark);
				}
			}
			if (Idx % 5 == 2)
			{
				P(Box, FVector(X + 30.0f, 2.5f, Ht * 0.6f), FVector(18, 1.2f, 12), R0, ScreenGreen, 1.5f);   // gauge
				P(Sph, FVector(X + 30.0f, 3.0f, Ht * 0.6f - 10.0f), FVector(2.5f, 2.5f, 2.5f), R0, Red, 8.0f, TEXT("Blink"));
			}
		}
		P(Box, FVector(0, 2.0f, Ht * 0.32f), FVector(Len - 20.0f, 1.5f, 2.5f), R0, BuildTint, 6.0f);
		P(Box, FVector(0, 1.0f, 6.0f), FVector(Len, 4.0f, 12.0f), R0, Dark);                                       // kick plate
		break;
	}

	case EMarsPropKind::Grime:
	{
		// Dirt, oil and regolith tracked in from EVA: translucent smears near the floor.
		const FLinearColor Dirt(0.12f, 0.07f, 0.04f, 0.35f);
		const FLinearColor Dust(0.45f, 0.22f, 0.11f, 0.3f);
		P(Box, FVector(0, 0, 0.6f), FVector(BuildExtent.X, 60, 0.6f), R0, Dust);
		P(Box, FVector(-BuildExtent.X * 0.2f, 10, 0.8f), FVector(BuildExtent.X * 0.3f, 30, 0.6f), FRotator(0, 12, 0), Dirt);
		P(Box, FVector(BuildExtent.X * 0.3f, -15, 0.9f), FVector(40, 25, 0.6f), FRotator(0, -30, 0), Dirt);
		P(Box, FVector(BuildExtent.X * 0.1f, 32.0f, 25.0f), FVector(30, 0.6f, 50), FRotator(0, 0, 0), Dirt);          // wall streak
		break;
	}

	// ------------------------------------------------------------------ vehicles
	case EMarsPropKind::ReconShuttle:
	{
		// RS-2 VTOL: armoured pod fuselage, bubble canopy, four ducted lift fans on stub pylons, retractable legs.
		const FLinearColor Shell(0.78f, 0.79f, 0.8f);
		const FLinearColor Plate(0.55f, 0.57f, 0.6f);
		const FLinearColor Glass(0.2f, 0.5f, 0.65f, 0.45f);
		P(Box, FVector(0, 0, 0), FVector(460, 200, 150), R0, Shell);                                                  // main body
		P(Box, FVector(-30, 0, 85), FVector(360, 170, 30), R0, Plate);                                               // dorsal armour
		P(Box, FVector(260, 0, -10), FVector(120, 170, 110), FRotator(-12, 0, 0), Shell);                           // nose
		P(Box, FVector(330, 0, -30), FVector(40, 120, 60), FRotator(-25, 0, 0), Dark);                               // sensor chin
		P(Sph, FVector(190, 0, 70), FVector(200, 150, 110), R0, Glass, 0.4f);                                        // canopy
		P(Box, FVector(190, 0, 128), FVector(150, 10, 8), R0, Dark);                                                 // canopy frame
		P(Box, FVector(125, 0, 95), FVector(10, 150, 8), FRotator(0, 0, 0), Dark);
		P(Box, FVector(-250, 0, 10), FVector(80, 160, 120), R0, Plate);                                              // engine bay
		P(Box, FVector(-300, 0, 110), FVector(80, 10, 120), FRotator(-15, 0, 0), Shell);                             // tail fin
		P(Box, FVector(-300, 0, 160), FVector(40, 12, 10), R0, Red, 6.0f, TEXT("Blink"));
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			P(Box, FVector(0, Side * 101.0f, 0), FVector(380, 3, 100), R0, Plate);                                   // side armour
			P(Box, FVector(40, Side * 103.0f, 30), FVector(300, 2, 6), R0, Cyan, 4.0f);                              // trim light
			P(Box, FVector(-60, Side * 103.0f, -30), FVector(120, 2, 16), R0, Orange);                               // ID stripe
			P(Box, FVector(-120, Side * 103.0f, 10), FVector(30, 2, 60), R0, Dark);                                  // hatch seam
		}
		P(Box, FVector(0, 0, -85), FVector(300, 140, 20), R0, Dark);                                                 // belly cargo pod
		P(Cyl, FVector(150, 70, 110), FVector(4, 4, 80), FRotator(0, 0, 10), Steel);                                 // antenna
		// Four ducted fans on pylons.
		for (int32 fx = -1; fx <= 1; fx += 2)
		{
			for (int32 fy = -1; fy <= 1; fy += 2)
			{
				const FVector C(fx * 210.0f, fy * 230.0f, 10.0f);
				P(Box, FVector(fx * 150.0f, fy * 165.0f, 20.0f), FVector(70, 130, 22), FRotator(0, fy * fx * 25.0f, 0), Plate); // pylon
				P(Box, FVector(fx * 150.0f, fy * 165.0f, 33.0f), FVector(40, 110, 4), FRotator(0, fy * fx * 25.0f, 0), Yellow); // hazard edge
				P(Cyl, C, FVector(170, 170, 50), R0, Dark);                                                           // duct ring
				P(Cyl, C + FVector(0, 0, 26.0f), FVector(176, 176, 6), R0, Plate);                                   // lip
				P(Cyl, C, FVector(40, 40, 30), R0, Steel);                                                           // hub
				P(Cyl, C - FVector(0, 0, 28.0f), FVector(150, 150, 4), R0, FLinearColor(0.4f, 0.7f, 1.0f), 0.0f, TEXT("Thrust"));
				if (USceneComponent* Fan = AddPivot(Parent, C * S, R0, TEXT("Fan")))
				{
					for (int32 b = 0; b < 4; ++b)
					{
						if (UStaticMeshComponent* Blade = AddPart(Fan, Box, FVector(0, 0, 4) * S, FVector(150, 14, 3) * S, FRotator(0, b * 45.0f, 8.0f), Steel, SelfIllumination, false))
						{
							Parts.Add(Blade);
						}
					}
				}
				P(Sph, C + FVector(fx * 90.0f, fy * 10.0f, 10.0f), FVector(10, 10, 10), R0, fy > 0 ? Green : Red, 8.0f, TEXT("Blink")); // nav light
				// Landing leg: pivot at the pod, hydraulic strut + foot.
				if (USceneComponent* Leg = AddPivot(Parent, (C - FVector(0, 0, 20.0f)) * S, R0, TEXT("Gear")))
				{
					auto LP = [&](EMarsPropShape Sh, FVector L, FVector Sz, FLinearColor Col)
					{
						if (UStaticMeshComponent* Part = AddPart(Leg, Sh, L * S, Sz * S, R0, Col, SelfIllumination, false)) { Parts.Add(Part); }
					};
					LP(Cyl, FVector(0, 0, -45.0f), FVector(18, 18, 90), Plate);
					LP(Cyl, FVector(0, 0, -110.0f), FVector(10, 10, 60), Steel);                                     // piston
					LP(Cyl, FVector(0, 0, -145.0f), FVector(60, 60, 10), Dark);                                      // foot pad
				}
			}
		}
		break;
	}

	// ------------------------------------------------------------------ surface POIs
	case EMarsPropKind::LandingPad:
	{
		P(Cyl, FVector(0, 0, 5), FVector(1600, 1600, 14), R0, FLinearColor(0.16f, 0.16f, 0.17f));
		P(Cyl, FVector(0, 0, 13), FVector(1450, 1450, 2), R0, Yellow * 0.7f);
		P(Cyl, FVector(0, 0, 14), FVector(1380, 1380, 2), R0, FLinearColor(0.16f, 0.16f, 0.17f));
		P(Box, FVector(0, -150, 15), FVector(500, 60, 2), R0, FLinearColor::White * 0.8f);                           // "H"
		P(Box, FVector(0, 150, 15), FVector(500, 60, 2), R0, FLinearColor::White * 0.8f);
		P(Box, FVector(0, 0, 15), FVector(60, 300, 2), R0, FLinearColor::White * 0.8f);
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = i * PI / 6.0f;
			P(Cyl, FVector(FMath::Cos(A) * 760.0f, FMath::Sin(A) * 760.0f, 20), FVector(22, 22, 20), R0, i % 2 ? Amber : Cyan, 10.0f, TEXT("Blink"));
		}
		break;
	}

	case EMarsPropKind::RelaySatellite:
	{
		// Crashed relay R-3: gold-foil bus nose-down in its own impact scar, one array snapped off.
		P(Cyl, FVector(0, 0, -20), FVector(2600, 2600, 30), R0, FLinearColor(0.12f, 0.07f, 0.05f));                  // scorch scar
		P(Box, FVector(0, 0, 160), FVector(360, 260, 260), FRotator(18, 25, 12), Gold);
		P(Box, FVector(0, 0, 160), FVector(370, 140, 140), FRotator(18, 25, 12), Dark);                              // radiator belt
		P(Box, FVector(-60, 380, 120), FVector(140, 600, 8), FRotator(5, 25, -28), FLinearColor(0.08f, 0.12f, 0.35f)); // solar wing
		for (int32 i = 0; i < 6; ++i)
		{
			P(Box, FVector(-60, 160 + i * 80.0f, 128), FVector(142, 3, 9), FRotator(5, 25, -28), Steel);
		}
		P(Box, FVector(520, -350, 15), FVector(130, 520, 8), FRotator(2, 70, 6), FLinearColor(0.08f, 0.12f, 0.35f));  // snapped wing
		P(Sph, FVector(180, -120, 330), FVector(260, 260, 70), FRotator(30, 25, 0), FLinearColor(0.85f, 0.85f, 0.82f)); // dish
		P(Cyl, FVector(150, -100, 280), FVector(10, 10, 140), FRotator(30, 25, 0), Steel);
		P(Cyl, FVector(-200, 100, 300), FVector(8, 8, 260), FRotator(-20, 25, 15), Steel);                          // boom
		P(Sph, FVector(-150, 140, 420), FVector(18, 18, 18), R0, Red, 12.0f, TEXT("Blink"));                       // black-box beacon
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = i * 0.7f;
			P(Box, FVector(FMath::Cos(A) * (400 + i * 60.0f), FMath::Sin(A) * (400 + i * 60.0f), 10), FVector(40 + i * 6.0f, 30, 20), FRotator(i * 13.0f, i * 40.0f, i * 7.0f), i % 2 ? Gold : Hull);
		}
		break;
	}

	case EMarsPropKind::IceDrillRig:
	{
		// Deep ice-core drill: lattice derrick, drill string, tanks, generator, core racks, flood lights.
		const float H = 1500.0f;
		for (int32 x = -1; x <= 1; x += 2)
		{
			for (int32 y = -1; y <= 1; y += 2)
			{
				P(Box, FVector(x * 160.0f, y * 160.0f, H * 0.5f), FVector(18, 18, H), FRotator(-x * 3.0f, 0, y * 3.0f), Orange * 0.85f);
			}
		}
		for (float Z = 150.0f; Z < H; Z += 220.0f)
		{
			const float In = 160.0f - Z * 0.02f;
			P(Box, FVector(0, In, Z), FVector(In * 2.0f, 10, 10), R0, Orange * 0.7f);
			P(Box, FVector(0, -In, Z), FVector(In * 2.0f, 10, 10), R0, Orange * 0.7f);
			P(Box, FVector(In, 0, Z), FVector(10, In * 2.0f, 10), R0, Orange * 0.7f);
			P(Box, FVector(-In, 0, Z), FVector(10, In * 2.0f, 10), R0, Orange * 0.7f);
			P(Box, FVector(0, In, Z + 110.0f), FVector(In * 2.3f, 6, 6), FRotator(45, 0, 0), Steel);
		}
		P(Box, FVector(0, 0, H), FVector(240, 240, 60), R0, Hull);                                                  // crown block
		P(Sph, FVector(0, 0, H + 50.0f), FVector(26, 26, 26), R0, Red, 12.0f, TEXT("Blink"));
		P(Cyl, FVector(0, 0, H * 0.45f), FVector(30, 30, H * 0.9f), R0, Steel);                                     // drill string
		P(Box, FVector(0, 0, 60), FVector(420, 420, 40), R0, Dark);                                                 // drill floor
		P(Cyl, FVector(0, 0, 10), FVector(120, 120, 30), R0, FLinearColor(0.3f, 0.75f, 0.95f), 3.0f);              // borehole glow (brine)
		for (int32 t = 0; t < 3; ++t)
		{
			P(Cyl, FVector(-520.0f, -200.0f + t * 200.0f, 180.0f), FVector(170, 170, 340), R0, White);              // tanks
			P(Cyl, FVector(-520.0f, -200.0f + t * 200.0f, 200.0f), FVector(174, 174, 14), R0, Orange);
		}
		P(Box, FVector(500, -250, 110), FVector(260, 180, 220), R0, Olive);                                          // generator
		P(Box, FVector(500, -250, 230), FVector(200, 140, 20), R0, Dark);
		for (int32 r = 0; r < 5; ++r)
		{
			P(Cyl, FVector(450.0f + r * 30.0f, 260.0f, 120.0f), FVector(14, 14, 220), FRotator(0, 0, 8), FLinearColor(0.6f, 0.9f, 1.0f, 0.55f), 1.5f); // ice cores
		}
		P(Box, FVector(510, 260, 15), FVector(220, 80, 30), R0, Steel);
		for (int32 L = 0; L < 2; ++L)
		{
			P(Box, FVector(L ? 200.0f : -200.0f, 200.0f, H * 0.7f), FVector(30, 50, 30), R0, FLinearColor::White, 14.0f); // floodlights
		}
		break;
	}

	case EMarsPropKind::AbandonedOutpost:
	{
		// ARES-III: half-buried hab can, collapsed antenna, dust-drowned solar field.
		P(Cyl, FVector(0, 0, 120), FVector(420, 420, 900), FRotator(90, 0, 0), FLinearColor(0.7f, 0.68f, 0.64f));
		for (int32 i = 0; i < 5; ++i)
		{
			P(Cyl, FVector(-360.0f + i * 180.0f, 0, 120), FVector(428, 428, 12), FRotator(90, 0, 0), Hull * 0.5f);
		}
		P(Box, FVector(470, 0, 120), FVector(40, 180, 220), R0, Dark);                                              // airlock door
		P(Box, FVector(492, 0, 150), FVector(4, 120, 8), R0, Orange, 1.0f);
		P(Box, FVector(0, 0, -40), FVector(1200, 700, 160), FRotator(0, 0, 3), Regolith);                           // dune burying it
		P(Box, FVector(-200, 260, 60), FVector(500, 300, 120), FRotator(8, 20, -10), Regolith * 1.1f);
		P(Cyl, FVector(-300, -380, 300), FVector(14, 14, 700), FRotator(55, 0, 20), Steel);                         // fallen mast
		P(Sph, FVector(-520, -560, 60), FVector(200, 200, 50), FRotator(70, 0, 30), FLinearColor(0.75f, 0.75f, 0.72f));
		for (int32 i = 0; i < 6; ++i)
		{
			P(Box, FVector(700.0f + (i % 3) * 260.0f, -400.0f + (i / 3) * 300.0f, 60.0f), FVector(220, 160, 6), FRotator(25, 0, (i % 2) ? 8.0f : -6.0f), FLinearColor(0.08f, 0.12f, 0.3f));
			P(Box, FVector(700.0f + (i % 3) * 260.0f, -400.0f + (i / 3) * 300.0f, 30.0f), FVector(8, 8, 60), R0, Steel);
		}
		P(Box, FVector(300, 230, 260), FVector(120, 4, 40), R0, Red * 0.6f);                                        // faded ID band
		break;
	}

	case EMarsPropKind::BasaltColumns:
	{
		FRandomStream Rng(static_cast<int32>(BuildExtent.X * 7.0f + BuildExtent.Y));
		const int32 N = 9 + Rng.RandRange(0, 6);
		for (int32 i = 0; i < N; ++i)
		{
			const float A = Rng.FRandRange(0.0f, 2.0f * PI);
			const float R = Rng.FRandRange(0.0f, 420.0f);
			const float Ht = Rng.FRandRange(250.0f, 1300.0f) * (1.0f - R / 900.0f);
			const float Dm = Rng.FRandRange(90.0f, 170.0f);
			const FLinearColor Rock = FLinearColor(0.14f, 0.12f, 0.12f) * Rng.FRandRange(0.8f, 1.2f);
			const FVector Pos(FMath::Cos(A) * R, FMath::Sin(A) * R, Ht * 0.5f - 40.0f);
			P(Cyl, Pos, FVector(Dm, Dm, Ht), FRotator(Rng.FRandRange(-6.0f, 6.0f), 0, Rng.FRandRange(-6.0f, 6.0f)), Rock);
			P(Cyl, Pos + FVector(0, 0, Ht * 0.5f), FVector(Dm * 0.9f, Dm * 0.9f, 10.0f), R0, Regolith * 0.8f);      // dust cap
		}
		break;
	}

	case EMarsPropKind::SurveyBeaconMast:
		P(Cyl, FVector(0, 0, 300), FVector(14, 14, 600), R0, Steel);
		P(Box, FVector(0, 0, 20), FVector(120, 120, 40), R0, Dark);
		P(Box, FVector(40, 0, 200), FVector(10, 140, 90), FRotator(-30, 0, 0), FLinearColor(0.08f, 0.12f, 0.35f));    // solar panel
		P(Box, FVector(0, 9, 420), FVector(40, 2, 60), R0, Orange, 1.5f);                                            // ID plate
		P(Sph, FVector(0, 0, 610), FVector(24, 24, 24), R0, BuildTint, 14.0f, TEXT("Blink"));
		break;

	case EMarsPropKind::NavArrow:
	{
		// Wall-mounted, faces +Y. Backing plate + glowing chevron arrow + colour bar.
		const float Dir = BuildExtent.X < 0.0f ? -1.0f : 1.0f;
		const float Len = 70.0f;
		P(Box, FVector(0, 0, 0), FVector(Len + 16.0f, 1.5f, 22.0f), R0, FLinearColor(0.03f, 0.03f, 0.035f));
		P(Box, FVector(-Dir * 6.0f, 1.2f, 0), FVector(Len - 22.0f, 1.0f, 5.0f), R0, BuildTint, 9.0f);
		P(Box, FVector(Dir * (Len * 0.5f - 12.0f), 1.3f, 4.6f), FVector(18.0f, 1.0f, 5.0f), FRotator(0, 0, 0) + FRotator(-Dir * 40.0f, 0, 0), BuildTint, 9.0f);
		P(Box, FVector(Dir * (Len * 0.5f - 12.0f), 1.3f, -4.6f), FVector(18.0f, 1.0f, 5.0f), FRotator(Dir * 40.0f, 0, 0), BuildTint, 9.0f);
		P(Box, FVector(0, 1.0f, -10.0f), FVector(Len + 16.0f, 1.0f, 1.5f), R0, BuildTint * 0.6f, 3.0f);
		break;
	}

	case EMarsPropKind::Asteroid:
	{
		// Lumpy carbonaceous rock (deterministic per position): overlapping squashed spheres.
		FRandomStream Rng(static_cast<int32>(BuildExtent.X * 13.0f + BuildExtent.Y * 7.0f) + 11);
		const int32 N = 6 + Rng.RandRange(0, 4);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector Off = FMath::VRand() * Rng.FRandRange(0.0f, 900.0f);
			const float R = Rng.FRandRange(900.0f, 1700.0f);
			const FLinearColor Rock = FLinearColor(0.22f, 0.19f, 0.17f) * Rng.FRandRange(0.7f, 1.2f);
			P(Sph, Off, FVector(R, R * Rng.FRandRange(0.6f, 1.0f), R * Rng.FRandRange(0.6f, 1.0f)), FRotator(Rng.FRandRange(0.0f, 360.0f), Rng.FRandRange(0.0f, 360.0f), 0), Rock);
		}
		for (int32 i = 0; i < 5; ++i)
		{
			P(Cyl, FMath::VRand() * 1100.0f, FVector(300, 300, 80), FRotator(Rng.FRandRange(0.0f, 180.0f), 0, Rng.FRandRange(0.0f, 180.0f)), FLinearColor(0.1f, 0.09f, 0.08f)); // craters
		}
		break;
	}

	case EMarsPropKind::ScienceBuoy:
		P(Cyl, FVector(0, 0, 0), FVector(180, 180, 260), R0, FLinearColor(0.8f, 0.8f, 0.78f));
		P(Cyl, FVector(0, 0, 140), FVector(200, 200, 20), R0, Gold);
		P(Box, FVector(0, 380, 0), FVector(120, 520, 6), R0, FLinearColor(0.08f, 0.12f, 0.35f));
		P(Box, FVector(0, -380, 0), FVector(120, 520, 6), R0, FLinearColor(0.08f, 0.12f, 0.35f));
		P(Cyl, FVector(0, 0, 300), FVector(8, 8, 300), R0, Steel);
		P(Sph, FVector(0, 0, 460), FVector(40, 40, 40), R0, Cyan, 14.0f, TEXT("Blink"));
		P(Box, FVector(92, 0, 20), FVector(4, 80, 60), R0, Orange, 1.5f);
		break;

	case EMarsPropKind::DerelictStation:
	{
		// Supply tug "Kestrel": twin pressure modules on a truss, torn solar wings, docking ring.
		const FLinearColor Hullc(0.62f, 0.62f, 0.6f);
		P(Cyl, FVector(0, 0, 0), FVector(600, 600, 2400), FRotator(90, 0, 0), Hullc);
		P(Cyl, FVector(-1700, 0, 0), FVector(500, 500, 1200), FRotator(90, 0, 0), Hullc * 0.8f);
		for (int32 i = 0; i < 6; ++i)
		{
			P(Cyl, FVector(-1100.0f + i * 400.0f, 0, 0), FVector(620, 620, 40), FRotator(90, 0, 0), Steel);
		}
		P(Box, FVector(-500, 0, 0), FVector(3200, 120, 120), R0, Steel);                                           // truss
		P(Box, FVector(-400, 1400, 0), FVector(500, 2000, 10), FRotator(0, 0, 8), FLinearColor(0.08f, 0.12f, 0.35f));
		P(Box, FVector(-400, -1250, 200), FVector(500, 1500, 10), FRotator(0, 15, -25), FLinearColor(0.08f, 0.12f, 0.35f)); // torn wing
		P(Cyl, FVector(1250, 0, 0), FVector(420, 420, 80), FRotator(90, 0, 0), Dark);                              // docking ring
		P(Cyl, FVector(1300, 0, 0), FVector(360, 360, 20), FRotator(90, 0, 0), Yellow, 2.0f);
		P(Sph, FVector(0, 0, 330), FVector(40, 40, 40), R0, Red, 12.0f, TEXT("Blink"));
		P(Box, FVector(300, 305, 0), FVector(800, 6, 80), R0, Orange, 1.0f);                                     // ID band
		P(Box, FVector(-2300, 0, 0), FVector(400, 300, 300), R0, Dark);                                          // reactor
		break;
	}

	case EMarsPropKind::DebrisContainer:
		P(Box, FVector(0, 0, 0), FVector(220, 160, 160), R0, FLinearColor(0.85f, 0.85f, 0.85f));
		P(Box, FVector(0, 0, 0), FVector(230, 30, 170), R0, Red, 2.0f);
		P(Box, FVector(0, 0, 0), FVector(230, 170, 30), R0, Red, 2.0f);
		P(Sph, FVector(0, 0, 100), FVector(24, 24, 24), R0, Green, 12.0f, TEXT("Blink"));
		break;

	case EMarsPropKind::StarField:
	{
		// Shell of point stars around the orbital zone (radius = Extent.X).
		FRandomStream Rng(424242);
		const float R = BuildExtent.X;
		for (int32 i = 0; i < 260; ++i)
		{
			const FVector Dir = FVector(Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-0.6f, 1.0f)).GetSafeNormal();
			const float Size = Rng.FRandRange(500.0f, 1600.0f) * (Rng.FRand() < 0.08f ? 2.5f : 1.0f);
			const FLinearColor Tint = Rng.FRand() < 0.2f ? FLinearColor(1.0f, 0.8f, 0.6f) : FLinearColor(0.8f, 0.88f, 1.0f);
			P(Sph, Dir * R, FVector(Size), R0, Tint, Rng.FRandRange(20.0f, 60.0f));
		}
		break;
	}

	case EMarsPropKind::CeilingChamfer:
	{
		// Origin on the wall/ceiling corner line; the panel slopes down and into the room (+Y).
		const float Len = BuildExtent.X;
		const float Sz = BuildExtent.Y;           // leg length of the 45-degree chamfer
		const FLinearColor Gun(0.24f, 0.25f, 0.27f);
		const FVector Mid(0.0f, Sz * 0.5f, -Sz * 0.5f);
		const FRotator Slope(0.0f, 0.0f, 45.0f);
		P(Box, Mid, FVector(Len, Sz * 1.414f, 4.0f), Slope, Gun);
		// Recessed sub-panels with dark seams and bolt heads (reference: chunky gunmetal plating).
		for (float X = -Len * 0.5f + 70.0f; X < Len * 0.5f - 40.0f; X += 140.0f)
		{
			P(Box, Mid + FVector(X, -1.5f, -1.5f), FVector(120.0f, Sz * 1.1f, 1.0f), Slope, Gun * 0.75f);
			P(Box, Mid + FVector(X + 64.0f, -2.0f, -2.0f), FVector(6.0f, Sz * 1.3f, 1.5f), Slope, Dark);
		}
		// Sector light strip along the lower edge of the chamfer.
		P(Box, FVector(0.0f, Sz - 2.0f, -Sz + 1.0f), FVector(Len - 10.0f, 3.0f, 2.5f), R0, BuildTint, 7.0f);
		break;
	}

	case EMarsPropKind::CeilingLight:
	{
		// Octagonal recessed fixture (origin on the ceiling, hangs down).
		P(Cyl, FVector(0, 0, -2.0f), FVector(70.0f, 70.0f, 4.0f), R0, FLinearColor(0.18f, 0.18f, 0.2f));
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = i * PI / 4.0f;
			P(Box, FVector(FMath::Cos(A) * 30.0f, FMath::Sin(A) * 30.0f, -4.5f), FVector(26.0f, 5.0f, 3.0f), FRotator(0, FMath::RadiansToDegrees(A) + 90.0f, 0), Dark);
		}
		P(Cyl, FVector(0, 0, -5.0f), FVector(50.0f, 50.0f, 2.0f), R0, FLinearColor(1.0f, 0.97f, 0.9f), 9.0f);
		break;
	}

	case EMarsPropKind::FloorRunner:
	{
		const float Len = BuildExtent.X, Wd = BuildExtent.Y;
		const FLinearColor Plate(0.2f, 0.21f, 0.22f);
		P(Box, FVector(0, 0, 0.8f), FVector(Len, Wd, 1.6f), R0, Plate);
		// Diamond-plate texture: rows of small raised lozenges.
		for (float X = -Len * 0.5f + 20.0f; X < Len * 0.5f; X += 40.0f)
		{
			for (float Y = -Wd * 0.5f + 24.0f; Y < Wd * 0.5f - 20.0f; Y += 32.0f)
			{
				P(Box, FVector(X, Y, 1.7f), FVector(15.0f, 4.0f, 0.4f), FRotator(0, ((FMath::RoundToInt(X / 40.0f) + FMath::RoundToInt(Y / 32.0f)) % 2) ? 45.0f : -45.0f, 0), Plate * 1.12f);
			}
		}
		// Grated edges.
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			P(Box, FVector(0, Side * (Wd * 0.5f + 9.0f), 0.6f), FVector(Len, 18.0f, 1.2f), R0, Dark);
			for (float X = -Len * 0.5f + 8.0f; X < Len * 0.5f; X += 16.0f)
			{
				P(Box, FVector(X, Side * (Wd * 0.5f + 9.0f), 1.3f), FVector(3.0f, 16.0f, 0.6f), R0, Plate * 0.9f);
			}
			P(Box, FVector(0, Side * (Wd * 0.5f + 19.0f), 1.0f), FVector(Len, 1.5f, 1.0f), R0, Yellow * 0.6f, 0.5f);
		}
		break;
	}

	case EMarsPropKind::CircuitBox:
		// Faces +Y (mounted on a wall). Reference: grey box with a 3x4 LED matrix and a label plate.
		P(Box, FVector(0, 4.0f, 0), FVector(30.0f, 8.0f, 42.0f), R0, FLinearColor(0.3f, 0.31f, 0.33f));
		P(Box, FVector(0, 8.2f, 0), FVector(26.0f, 0.6f, 38.0f), R0, Dark);
		for (int32 r = 0; r < 4; ++r)
		{
			for (int32 c = 0; c < 3; ++c)
			{
				const bool bAlt = (r + c) % 3 == 0;
				P(Box, FVector(-7.0f + c * 7.0f, 8.8f, 10.0f - r * 7.0f), FVector(4.0f, 0.6f, 4.0f), R0, bAlt ? FLinearColor(0.2f, 0.4f, 1.0f) : BuildTint, 6.0f, (r + c) % 4 == 0 ? FName(TEXT("Blink")) : FName());
			}
		}
		P(Box, FVector(0, 8.8f, -16.0f), FVector(20.0f, 0.6f, 4.0f), R0, Yellow, 0.4f);
		P(Cyl, FVector(0, 4.0f, -26.0f), FVector(4.0f, 4.0f, 12.0f), R0, Dark);                // conduit down
		break;

	case EMarsPropKind::PaddedPanels:
	{
		// Faces +Y. Rounded white quilted panels (med-bay reference) on a steel frame.
		const float Len = BuildExtent.X, Ht = BuildExtent.Z;
		const FLinearColor Pad(0.86f, 0.87f, 0.88f);
		P(Box, FVector(0, 1.0f, Ht * 0.5f + 20.0f), FVector(Len, 2.0f, Ht), R0, FLinearColor(0.5f, 0.52f, 0.55f));
		for (float X = -Len * 0.5f + 55.0f; X <= Len * 0.5f - 55.0f; X += 110.0f)
		{
			for (int32 Row = 0; Row < 2; ++Row)
			{
				const float Z = 20.0f + Ht * (0.27f + 0.5f * Row);
				P(Box, FVector(X, 5.0f, Z), FVector(100.0f, 7.0f, Ht * 0.44f), R0, Pad);
				P(Sph, FVector(X, 7.5f, Z), FVector(96.0f, 4.0f, Ht * 0.40f), R0, Pad * 1.02f);   // soft bulge
			}
		}
		P(Box, FVector(0, 4.0f, 10.0f), FVector(Len, 6.0f, 20.0f), R0, FLinearColor(0.35f, 0.36f, 0.38f));    // kick plate
		P(Box, FVector(0, 6.0f, Ht * 0.52f + 20.0f), FVector(Len, 1.0f, 3.0f), R0, BuildTint, 4.0f);           // light line
		break;
	}

	case EMarsPropKind::HydroponicRack:
	{
		// Faces +X. Three trays of leafy crops under violet UV bars, water lines, frame.
		const float L = 180.0f, Wd = 70.0f;
		for (int32 x = -1; x <= 1; x += 2)
		{
			for (int32 y = -1; y <= 1; y += 2)
			{
				P(Box, FVector(x * (L * 0.5f - 3.0f), y * (Wd * 0.5f - 3.0f), 100.0f), FVector(5.0f, 5.0f, 200.0f), R0, Steel);
			}
		}
		for (int32 t = 0; t < 3; ++t)
		{
			const float Z = 30.0f + t * 60.0f;
			P(Box, FVector(0, 0, Z), FVector(L, Wd, 8.0f), R0, FLinearColor(0.85f, 0.86f, 0.88f));       // tray
			P(Box, FVector(0, 0, Z + 4.5f), FVector(L - 10.0f, Wd - 10.0f, 1.0f), R0, FLinearColor(0.05f, 0.08f, 0.12f)); // nutrient film
			for (int32 i = 0; i < 6; ++i)
			{
				for (int32 j = 0; j < 2; ++j)
				{
					const float PX = -L * 0.5f + 18.0f + i * 29.0f, PY = -14.0f + j * 28.0f;
					const float H = 12.0f + ((i + j + t) % 3) * 4.0f;
					P(Sph, FVector(PX, PY, Z + 6.0f + H * 0.5f), FVector(18.0f, 18.0f, H), R0, FLinearColor(0.12f, 0.45f + 0.1f * ((i + t) % 2), 0.15f));
				}
			}
			P(Box, FVector(0, 0, Z + 46.0f), FVector(L - 12.0f, 6.0f, 2.0f), R0, FLinearColor(0.6f, 0.25f, 1.0f), 9.0f);  // UV bar
			P(Cyl, FVector(0, Wd * 0.5f - 4.0f, Z + 10.0f), FVector(3.0f, 3.0f, L), FRotator(90.0f, 0, 0), FLinearColor(0.2f, 0.45f, 0.9f)); // water line
		}
		P(Box, FVector(0, 0, 205.0f), FVector(L, Wd, 6.0f), R0, Steel);
		P(Box, FVector(L * 0.5f + 1.0f, 0, 190.0f), FVector(1.0f, 30.0f, 10.0f), R0, ScreenGreen, 2.0f);         // tray monitor
		break;
	}

	case EMarsPropKind::NavSignPanel:
		// Faces +Y. Dark board with a tinted frame.
		P(Box, FVector(0, 1.0f, 0), FVector(BuildExtent.X, 2.0f, BuildExtent.Z), R0, FLinearColor(0.02f, 0.02f, 0.025f));
		P(Box, FVector(0, 2.2f, BuildExtent.Z * 0.5f - 1.0f), FVector(BuildExtent.X, 0.6f, 2.0f), R0, BuildTint, 5.0f);
		P(Box, FVector(0, 2.2f, -BuildExtent.Z * 0.5f + 1.0f), FVector(BuildExtent.X, 0.6f, 2.0f), R0, BuildTint, 5.0f);
		break;

	case EMarsPropKind::NavChevronUp:
		// Faces +Y. "^" made of two glowing bars.
		P(Box, FVector(-3.2f, 2.6f, 0), FVector(2.2f, 0.6f, 9.0f), FRotator(-35.0f, 0, 0), BuildTint, 9.0f);
		P(Box, FVector(3.2f, 2.6f, 0), FVector(2.2f, 0.6f, 9.0f), FRotator(35.0f, 0, 0), BuildTint, 9.0f);
		break;

	case EMarsPropKind::SuitMannequin:
	{
		// MCP EVA suit on a stand, facing +X. Reference: off-white suit, glowing cyan seam lines,
		// blue wrist bands, chest telemetry box, dark visor with helmet lights, blue boots.
		const FLinearColor Suit(0.72f, 0.72f, 0.7f);
		const FLinearColor Seam(0.55f, 0.9f, 1.0f);
		const FLinearColor Band(0.1f, 0.45f, 0.9f);
		const FLinearColor Rub(0.06f, 0.06f, 0.065f);
		const FLinearColor Boot(0.1f, 0.18f, 0.35f);
		P(Cyl, FVector(0, 0, 2.0f), FVector(80.0f, 80.0f, 4.0f), R0, Dark);                                   // stand base
		P(Cyl, FVector(-22.0f, 0, 70.0f), FVector(5.0f, 5.0f, 140.0f), R0, Steel);
		// Legs + knee bellows + boots.
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float Y = Side * 13.0f;
			P(Box, FVector(2.0f, Y, 14.0f), FVector(34.0f, 15.0f, 16.0f), R0, Boot);
			for (int32 k = 0; k < 4; ++k) { P(Box, FVector(9.0f + k * 3.0f, Y, 22.0f), FVector(2.0f, 15.5f, 3.0f), R0, Boot * 0.6f); }
			P(Cyl, FVector(0, Y, 44.0f), FVector(17.0f, 17.0f, 42.0f), R0, Suit);
			for (int32 r = 0; r < 4; ++r) { P(Cyl, FVector(0, Y, 68.0f + r * 3.0f), FVector(r % 2 ? 16.0f : 18.0f, r % 2 ? 16.0f : 18.0f, 2.5f), R0, Rub); }
			P(Cyl, FVector(0, Y, 98.0f), FVector(19.0f, 19.0f, 44.0f), R0, Suit);
			P(Box, FVector(9.5f, Y, 92.0f), FVector(1.0f, 12.0f, 1.5f), R0, Seam, 6.0f);
			for (int32 b = 0; b < 3; ++b) { P(Box, FVector(9.6f, Y, 100.0f + b * 5.0f), FVector(1.0f, 10.0f, 2.0f), R0, Band); }
		}
		// Torso, belt, chest telemetry, seam lines.
		P(Box, FVector(0, 0, 124.0f), FVector(26.0f, 40.0f, 10.0f), R0, Suit * 0.85f);
		P(Box, FVector(13.5f, 0, 124.0f), FVector(1.0f, 30.0f, 3.0f), R0, Band, 2.0f);
		P(Box, FVector(0, 0, 152.0f), FVector(30.0f, 44.0f, 48.0f), R0, Suit);
		P(Box, FVector(15.5f, 0, 150.0f), FVector(2.0f, 15.0f, 18.0f), R0, FLinearColor(0.5f, 0.5f, 0.5f));    // chest unit
		P(Box, FVector(16.6f, -3.5f, 152.0f), FVector(0.5f, 4.0f, 8.0f), R0, ScreenGreen, 2.0f);
		P(Box, FVector(16.6f, 3.5f, 152.0f), FVector(0.5f, 4.0f, 8.0f), R0, ScreenGreen, 2.0f);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			P(Cyl, FVector(16.0f, Side * 14.0f, 160.0f), FVector(5.0f, 5.0f, 2.0f), FRotator(90, 0, 0), Band);   // hose ports
			P(Cyl, FVector(16.0f, Side * 14.0f, 142.0f), FVector(5.0f, 5.0f, 2.0f), FRotator(90, 0, 0), FLinearColor(0.6f, 0.1f, 0.1f));
		}
		P(Box, FVector(15.5f, 0, 174.0f), FVector(1.0f, 44.0f, 1.5f), R0, Seam, 6.0f);                        // collar seam
		P(Box, FVector(-24.0f, 0, 155.0f), FVector(18.0f, 38.0f, 46.0f), R0, Suit * 0.8f);                     // PLSS backpack
		// Arms: shoulder plates, upper arm, elbow bellows, forearm, wrist band, glove.
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float Y = Side * 28.0f;
			P(Box, FVector(0, Y, 170.0f), FVector(22.0f, 14.0f, 10.0f), FRotator(0, 0, Side * 15.0f), Suit * 0.9f); // shoulder plate
			P(Box, FVector(0, Y + Side * 1.0f, 171.0f), FVector(8.0f, 10.0f, 2.0f), R0, Band);
			P(Cyl, FVector(0, Y, 152.0f), FVector(13.0f, 13.0f, 30.0f), R0, Suit);
			for (int32 r = 0; r < 3; ++r) { P(Cyl, FVector(0, Y, 135.0f - r * 2.6f), FVector(r % 2 ? 11.5f : 13.0f, r % 2 ? 11.5f : 13.0f, 2.2f), R0, Rub); }
			P(Cyl, FVector(2.0f, Y, 116.0f), FVector(12.0f, 12.0f, 26.0f), R0, Suit);
			P(Cyl, FVector(2.0f, Y, 103.0f), FVector(12.6f, 12.6f, 3.0f), R0, Seam, 5.0f);                    // glowing cuff seam
			P(Cyl, FVector(2.0f, Y, 99.5f), FVector(12.4f, 12.4f, 3.0f), R0, Band, 3.0f);                    // blue wrist band
			P(Cyl, FVector(2.0f, Y, 96.5f), FVector(11.0f, 11.0f, 2.0f), R0, FLinearColor(0.55f, 0.57f, 0.6f)); // metal wrist ring
			P(Box, FVector(3.0f, Y, 88.0f), FVector(10.0f, 9.0f, 14.0f), R0, Rub * 2.0f);                    // glove
		}
		// Helmet: titanium collar, shell, dark visor, cyan helmet lights.
		P(Cyl, FVector(0, 0, 181.0f), FVector(30.0f, 30.0f, 5.0f), R0, FLinearColor(0.6f, 0.62f, 0.65f));
		P(Cyl, FVector(0, 0, 185.0f), FVector(27.0f, 27.0f, 3.0f), R0, Rub);
		P(Sph, FVector(0, 0, 200.0f), FVector(34.0f, 32.0f, 34.0f), R0, FLinearColor(0.4f, 0.41f, 0.43f));
		P(Sph, FVector(7.0f, 0, 198.0f), FVector(24.0f, 26.0f, 22.0f), R0, FLinearColor(0.02f, 0.02f, 0.03f));
		P(Box, FVector(0, 0, 215.0f), FVector(18.0f, 8.0f, 4.0f), R0, FLinearColor(0.3f, 0.3f, 0.32f));
		for (int32 i = 0; i < 3; ++i) { P(Sph, FVector(4.0f + i * 3.0f, 0, 217.5f), FVector(2.0f, 2.0f, 1.5f), R0, FLinearColor(0.2f, 0.8f, 1.0f), 12.0f); }
		P(Box, FVector(-2.0f, 17.5f, 205.0f), FVector(8.0f, 3.0f, 8.0f), R0, FLinearColor(0.2f, 0.8f, 1.0f), 8.0f); // side lamp
		break;
	}

	default:
		break;
	}
	return Parts;
}

void AMarsPropBuilder::SetSurface(UStaticMeshComponent* Part, float Roughness, float Metallic)
{
	if (UMaterialInstanceDynamic* MID = Part ? Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)) : nullptr)
	{
		MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		MID->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	}
}
