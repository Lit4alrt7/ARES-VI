#include "Stations/MarsStations.h"
#include "Core/MarsNarrativeDirector.h"
#include "World/MarsBaseManager.h"

#include "Audio/MarsAudioManager.h"
#include "Algo/Reverse.h"
#include "Core/MarsTutorialManager.h"
#include "Base/AirlockModule.h"
#include "Base/BioDomeModule.h"
#include "Base/CommRelayModule.h"
#include "Base/MedicalBayModule.h"
#include "Items/MarsInventoryComponent.h"
#include "Items/MarsItemLibrary.h"
#include "Player/EVASuitComponent.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsStationSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FText Prompt(const FString& S) { return FText::FromString(S); }

	UMarsStationSubsystem* StationOf(const UObject* Ctx)
	{
		const UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UMarsStationSubsystem>() : nullptr;
	}

	FString ItemName(EMarsItemType T) { return UMarsItemLibrary::GetItemName(T).ToString().ToUpper(); }
}

// ============================================================================
// Base
// ============================================================================

AMarsStationBase::AMarsStationBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Console = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Console"));
	Console->SetupAttachment(Root);
	if (CubeMesh.Succeeded())
	{
		Console->SetStaticMesh(CubeMesh.Object);
	}
	Console->SetCollisionProfileName(TEXT("BlockAll"));
	Console->SetHiddenInGame(true);
	Console->SetVisibility(false);
	Console->SetCastShadow(false);

	Screen = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Screen"));
	Screen->SetupAttachment(Root);
	Screen->SetHorizontalAlignment(EHTA_Center);
	Screen->SetVerticalAlignment(EVRTA_TextCenter);
	Screen->SetTextRenderColor(FColor(110, 255, 150));
	Screen->SetCastShadow(false);

	ScreenGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("ScreenGlow"));
	ScreenGlow->SetupAttachment(Root);
	ScreenGlow->SetIntensityUnits(ELightUnits::Lumens);
	ScreenGlow->SetIntensity(90.0f);
	ScreenGlow->SetAttenuationRadius(280.0f);
	ScreenGlow->SetLightColor(FLinearColor(0.3f, 1.0f, 0.5f));
	ScreenGlow->SetCastShadows(false);
}

void AMarsStationBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Console->SetRelativeLocation(FVector(0, 0, ConsoleSize.Z * 0.5f));
	Console->SetRelativeScale3D(ConsoleSize / 100.0f);
	Screen->SetRelativeLocationAndRotation(ScreenOffset, FRotator::ZeroRotator);
	Screen->SetWorldSize(ScreenTextSize);
	Screen->SetText(FText::FromString(StationName));
	ScreenGlow->SetRelativeLocation(ScreenOffset + FVector(25, 0, 0));
	AMarsPropBuilder::BuildProp(Root, PropKind, 1.0f, false);
}

void AMarsStationBase::BeginPlay()
{
	Super::BeginPlay();
	ResolveModule();
}

void AMarsStationBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Only refresh screens near the player (cheap and keeps distant text static).
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Player && FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) < FMath::Square(1500.0f))
	{
		Screen->SetText(FText::FromString(GlitchText(GetScreenText(), GetPlayerGlitchAmount(this))));
		// Readable at a glance: green nominal, amber caution, flashing red critical.
		const int32 Status = GetScreenStatus();
		const bool bFlashOn = FMath::Frac(GetWorld()->GetTimeSeconds() * 2.0f) < 0.6f;
		const FColor Col = Status >= 2 ? (bFlashOn ? FColor(255, 50, 40) : FColor(90, 10, 8)) : Status == 1 ? FColor(255, 176, 40) : FColor(110, 255, 150);
		Screen->SetTextRenderColor(Col);
		ScreenGlow->SetLightColor(FLinearColor(Col));
	}
}

AMarsBaseModule* AMarsStationBase::ResolveModule() const
{
	if (!TargetModule)
	{
		if (const UMarsStationSubsystem* Station = StationOf(this))
		{
			const_cast<AMarsStationBase*>(this)->TargetModule = Station->FindModuleContainingPoint(GetActorLocation() + FVector(0, 0, 50));
		}
	}
	return TargetModule.Get();
}

void AMarsStationBase::Beep(AMarsCharacter* C, float Pitch) const
{
	if (C && C->GetAudioManager())
	{
		C->GetAudioManager()->PlayUIBeep(Pitch);
	}
}

float AMarsStationBase::GetPlayerGlitchAmount(const UObject* WorldContext)
{
	const AMarsCharacter* C = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(WorldContext, 0));
	const UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	if (!Bio || Bio->GetStageIndex() < 3)
	{
		return 0.0f;
	}
	return FMath::Clamp(0.08f * (Bio->GetStageIndex() - 2) + 0.15f * Bio->HallucinationIntensity, 0.0f, 0.5f);
}

FString AMarsStationBase::GlitchText(const FString& In, float Amount)
{
	if (Amount <= 0.0f)
	{
		return In;
	}
	static const TCHAR Glyphs[] = TEXT("#%&@?/\\|<>*=~");
	FString Out = In;
	for (int32 i = 0; i < Out.Len(); ++i)
	{
		if (Out[i] != TEXT('\n') && Out[i] != TEXT(' ') && FMath::FRand() < Amount)
		{
			Out[i] = Glyphs[FMath::RandRange(0, UE_ARRAY_COUNT(Glyphs) - 2)];
		}
	}
	// Occasionally the whole display inverts its line order (neuro-toxic misreading).
	if (FMath::FRand() < Amount * 0.3f)
	{
		TArray<FString> Lines;
		Out.ParseIntoArrayLines(Lines, false);
		Algo::Reverse(Lines);
		Out = FString::Join(Lines, TEXT("\n"));
	}
	return Out;
}

// ============================================================================
// SEC-01 Airlock panel
// ============================================================================

AAirlockPanelStation::AAirlockPanelStation()
{
	StationName = TEXT("AIRLOCK CYCLE");
	PropKind = EMarsPropKind::AirlockValve;
	ConsoleSize = FVector(40, 60, 190);
	ScreenOffset = FVector(14, 0, 195);
	ScreenGlow->SetLightColor(FLinearColor(1.0f, 0.6f, 0.1f));
}

void AAirlockPanelStation::Interact_Implementation(AMarsCharacter* C)
{
	AAirlockModule* Airlock = GetModuleAs<AAirlockModule>();
	if (!Airlock || !C)
	{
		return;
	}
	if (Airlock->GetAirlockState() == EAirlockState::PressurizedInnerOpen && !C->GetSuit()->IsSuitDonned())
	{
		Beep(C, 0.5f);
		C->ShowNotification(Prompt(TEXT("INTERLOCK: CREW MEMBER NOT SUITED")), 3.0f);
		return;
	}
	if (Airlock->RequestCycle())
	{
		Beep(C, 1.2f);
	}
}

FText AAirlockPanelStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const AAirlockModule* Airlock = GetModuleAs<AAirlockModule>();
	if (!Airlock)
	{
		return Prompt(TEXT("AIRLOCK PANEL OFFLINE"));
	}
	if (Airlock->IsCycling())
	{
		return Prompt(FString::Printf(TEXT("CYCLING %.0f%%"), Airlock->GetCycleProgress() * 100.0f));
	}
	return Prompt(Airlock->GetAirlockState() == EAirlockState::PressurizedInnerOpen ? TEXT("[E] Cycle to SURFACE") : TEXT("[E] Cycle to STATION (decon)"));
}

FString AAirlockPanelStation::GetScreenText() const
{
	const AAirlockModule* Airlock = GetModuleAs<AAirlockModule>();
	return Airlock ? FString::Printf(TEXT("SEC-01 AIRLOCK\nP %.1f kPa\n%s"), Airlock->CurrentPressureKpa,
		Airlock->IsCycling() ? TEXT("CYCLING") : Airlock->GetAirlockState() == EAirlockState::PressurizedInnerOpen ? TEXT("INNER OPEN") : TEXT("OUTER OPEN")) : StationName;
}

// ============================================================================
// SEC-01 Dust scrubber
// ============================================================================

ADustScrubberStation::ADustScrubberStation()
{
	StationName = TEXT("E-STATIC SCRUBBER");
	PropKind = EMarsPropKind::ScrubberUnit;
	ConsoleSize = FVector(70, 100, 170);
	ScreenOffset = FVector(37, 0, 118);
}

void ADustScrubberStation::InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds)
{
	if (C && C->GetSuit())
	{
		C->GetSuit()->RemoveDust(0.3f * DeltaSeconds);
	}
	if (AMarsBaseModule* M = ResolveModule())
	{
		M->AddDustContamination(-0.2f * DeltaSeconds);
	}
}

FText ADustScrubberStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const float Dust = C && C->GetSuit() ? C->GetSuit()->DustLoad : 0.0f;
	return Prompt(FString::Printf(TEXT("[hold E] Electrostatic dust strip (suit dust %.0f%%)"), Dust * 100.0f));
}

FString ADustScrubberStation::GetScreenText() const
{
	const AMarsBaseModule* M = ResolveModule();
	return FString::Printf(TEXT("ClO4- DUST\n%.0f %%"), M ? M->DustContamination * 100.0f : 0.0f);
}

// ============================================================================
// SEC-02 Suit rack
// ============================================================================

ASuitRackStation::ASuitRackStation()
{
	StationName = TEXT("MCP SUIT RACK");
	PropKind = EMarsPropKind::SuitMannequin;
	ConsoleSize = FVector(60, 90, 225);
	ScreenOffset = FVector(10, 0, 232);
}

void ASuitRackStation::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// The suit hangs on the stand only while it is not being worn (the stand base always stays).
	const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const bool bWorn = Player && Player->GetSuit() && Player->GetSuit()->IsSuitDonned();
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* M : Meshes)
	{
		if (M == Console)
		{
			continue;
		}
		const bool bStand = M->GetRelativeLocation().Z < 8.0f || FMath::Abs(M->GetRelativeLocation().X + 22.0f) < 1.0f;
		M->SetVisibility(!bWorn || bStand);
	}
}

void ASuitRackStation::Interact_Implementation(AMarsCharacter* C)
{
	HoldTime = 0.0f;
	UEVASuitComponent* Suit = C ? C->GetSuit() : nullptr;
	if (!Suit)
	{
		return;
	}
	if (Suit->IsSuitDonned())
	{
		const AMarsBaseModule* Module = ResolveModule();
		if (Module && Module->CurrentPressureKpa < 70.0f)
		{
			Beep(C, 0.5f);
			C->ShowNotification(Prompt(TEXT("CABIN PRESSURE TOO LOW TO REMOVE HELMET")), 3.0f);
			return;
		}
		Suit->DoffSuit();
		C->ShowNotification(Prompt(TEXT("MCP SUIT STOWED")), 2.0f);
	}
	else
	{
		Suit->DonSuit();
		C->ShowNotification(Prompt(TEXT("MCP SUIT ON - HELMET SEALED (100% O2 @ 30 kPa)")), 3.0f);
	}
	Beep(C);
}

void ASuitRackStation::InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds)
{
	HoldTime += DeltaSeconds;
	UEVASuitComponent* Suit = C ? C->GetSuit() : nullptr;
	if (!Suit || HoldTime < 0.6f || StationO2ReserveLiters <= 0.0f)
	{
		return;
	}
	StationO2ReserveLiters -= Suit->RefillO2(FMath::Min(RefillLitersPerSecond * DeltaSeconds, StationO2ReserveLiters));
	Suit->ServiceConsumables();
}

FText ASuitRackStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const UEVASuitComponent* Suit = C ? C->GetSuit() : nullptr;
	return Suit ? Prompt(FString::Printf(TEXT("[E] %s MCP suit   [hold E] Service suit"), Suit->IsSuitDonned() ? TEXT("Stow") : TEXT("Don"))) : FText::GetEmpty();
}

FString ASuitRackStation::GetScreenText() const
{
	return FString::Printf(TEXT("SEC-02 MCP RACK\nO2 RESERVE %.0f L"), StationO2ReserveLiters);
}

// ============================================================================
// SEC-02 O2 refill
// ============================================================================

AO2RefillStation::AO2RefillStation()
{
	StationName = TEXT("O2 MANIFOLD");
	PropKind = EMarsPropKind::O2RefillRack;
	ConsoleSize = FVector(40, 130, 180);
	ScreenOffset = FVector(22, 0, 150);
}

void AO2RefillStation::Interact_Implementation(AMarsCharacter* C)
{
	if (!C || !C->GetInventory())
	{
		return;
	}
	if (CanistersAvailable <= 0)
	{
		Beep(C, 0.5f);
		C->ShowNotification(Prompt(TEXT("MANIFOLD EMPTY")), 2.0f);
		return;
	}
	if (C->GetInventory()->AddItem(EMarsItemType::O2Canister, 1) > 0)
	{
		--CanistersAvailable;
		Beep(C);
		C->ShowNotification(Prompt(TEXT("+1 O2 CANISTER")), 2.0f);
	}
}

FText AO2RefillStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	return Prompt(FString::Printf(TEXT("[E] Fill O2 canister (%d left)"), CanistersAvailable));
}

FString AO2RefillStation::GetScreenText() const
{
	return FString::Printf(TEXT("O2 MANIFOLD\n%d x 300 L"), CanistersAvailable);
}

// ============================================================================
// Status panels
// ============================================================================

AModuleStatusPanel::AModuleStatusPanel()
{
	StationName = TEXT("MODULE STATUS");
	PropKind = EMarsPropKind::Terminal;
	ConsoleSize = FVector(50, 70, 150);
	ScreenOffset = FVector(18, 0, 130);
	ScreenTextSize = 3.4f;
}

void AModuleStatusPanel::Interact_Implementation(AMarsCharacter* C)
{
	UMarsStationSubsystem* Station = StationOf(this);
	if (!Station || !C)
	{
		return;
	}
	switch (Mode)
	{
	case EStatusPanelMode::GridBreaker:
		if (ABioDomeModule* Dome = Cast<ABioDomeModule>(Station->FindModuleByType(EMarsModuleType::AeroponicBioDome)))
		{
			Dome->bGrowLightsOnline = !Dome->bGrowLightsOnline;
			C->ShowNotification(Prompt(Dome->bGrowLightsOnline ? TEXT("BREAKER: GROW LIGHTS ON") : TEXT("BREAKER: GROW LIGHTS SHED")), 2.5f);
		}
		break;
	case EStatusPanelMode::ThermalControl:
	{
		bool bNewState = true;
		const TArray<AMarsBaseModule*> Modules = Station->GetModules();
		if (Modules.Num() > 0)
		{
			bNewState = !Modules[0]->bSealHeatersOnline;
		}
		for (AMarsBaseModule* M : Modules)
		{
			M->bSealHeatersOnline = bNewState;
		}
		C->ShowNotification(Prompt(bNewState ? TEXT("SEAL HEATERS: ON (+120 W / module)") : TEXT("SEAL HEATERS: OFF")), 2.5f);
		break;
	}
	case EStatusPanelMode::LifeSupport:
	{
		const TArray<AMarsBaseModule*> Modules = Station->GetModules();
		const bool bNewState = Modules.Num() > 0 ? !Modules[0]->bCo2ScrubberOnline : true;
		for (AMarsBaseModule* M : Modules)
		{
			M->bCo2ScrubberOnline = bNewState;
		}
		C->ShowNotification(Prompt(bNewState ? TEXT("AMINE SCRUBBERS: ON") : TEXT("AMINE SCRUBBERS: OFF")), 2.5f);
		break;
	}
	default:
		break;
	}
	Beep(C);
}

FText AModuleStatusPanel::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	switch (Mode)
	{
	case EStatusPanelMode::GridBreaker:    return Prompt(TEXT("[E] Toggle Bio-Dome grow-light breaker (load shedding)"));
	case EStatusPanelMode::ThermalControl: return Prompt(TEXT("[E] Toggle station seal heaters"));
	case EStatusPanelMode::LifeSupport:    return Prompt(TEXT("[E] Toggle amine CO2 scrubbers"));
	default:                               return FText::GetEmpty();
	}
}

FString AModuleStatusPanel::GetScreenText() const
{
	const UMarsStationSubsystem* Station = StationOf(this);
	switch (Mode)
	{
	case EStatusPanelMode::GridBreaker:
		return Station ? FString::Printf(TEXT("GRID BREAKER\n%.0f / %.0f W\n%s"), Station->GetPowerDemandW(), Station->GetPowerSupplyW(),
			Station->GetPowerAvailability() < 1.0f ? TEXT("!! BROWNOUT") : TEXT("NOMINAL")) : StationName;
	case EStatusPanelMode::ThermalControl:
	{
		const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
		const AMarsBaseModule* M = ResolveModule();
		return FString::Printf(TEXT("THERMAL CTRL\nEXT %.0f C\nSEAL %.0f C"), Env ? Env->GetAmbientTemperatureC() : 0.0f, M ? M->SealTemperatureC : 0.0f);
	}
	case EStatusPanelMode::LifeSupport:
	{
		float WorstCo2 = 0.0f;
		float WorstP = 200.0f;
		if (Station)
		{
			for (const AMarsBaseModule* M : Station->GetModules())
			{
				WorstCo2 = FMath::Max(WorstCo2, M->CurrentCo2Ppm);
				WorstP = FMath::Min(WorstP, M->CurrentPressureKpa);
			}
		}
		return FString::Printf(TEXT("LIFE SUPPORT\nCO2 MAX %.0f ppm\nP MIN %.1f kPa"), WorstCo2, WorstP);
	}
	default:
	{
		const AMarsBaseModule* M = ResolveModule();
		return M ? FString::Printf(TEXT("%s\nP %.1f kPa  T %.0f C\nO2 %.1f%%  CO2 %.0f\nHULL %.0f%%  SEAL %.0f%%"),
			*M->GetModuleName(), M->CurrentPressureKpa, M->CurrentTemperatureC, M->CurrentO2Fraction * 100.0f, M->CurrentCo2Ppm,
			M->HullIntegrity * 100.0f, M->SealIntegrity * 100.0f) : StationName;
	}
	}
}

// ============================================================================
// SEC-03 CO2 scrubber
// ============================================================================

ACo2ScrubberStation::ACo2ScrubberStation()
{
	StationName = TEXT("CO2 SCRUBBER");
	PropKind = EMarsPropKind::ScrubberUnit;
	ConsoleSize = FVector(70, 100, 170);
	ScreenOffset = FVector(37, 0, 118);
}

void ACo2ScrubberStation::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	PurgeCooldown = FMath::Max(PurgeCooldown - DeltaTime, 0.0f);
	HoldTime = FMath::Max(HoldTime - DeltaTime * 0.5f, 0.0f);
}

void ACo2ScrubberStation::Interact_Implementation(AMarsCharacter* C)
{
	if (ABioDomeModule* Dome = GetModuleAs<ABioDomeModule>())
	{
		Dome->bGrowLightsOnline = !Dome->bGrowLightsOnline;
		Beep(C);
	}
}

void ACo2ScrubberStation::InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds)
{
	HoldTime += DeltaSeconds * 1.5f;
	if (HoldTime >= 2.0f && PurgeCooldown <= 0.0f)
	{
		HoldTime = 0.0f;
		PurgeCooldown = 60.0f;
		if (UMarsStationSubsystem* Station = StationOf(this))
		{
			for (AMarsBaseModule* M : Station->GetModules())
			{
				M->RemoveCo2Ppm(3000.0f);
			}
		}
		if (C)
		{
			C->ShowNotification(Prompt(TEXT("EMERGENCY LiOH PURGE: -3000 ppm CO2 STATION-WIDE")), 3.0f);
			Beep(C, 1.5f);
		}
	}
}

FText ACo2ScrubberStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	return Prompt(PurgeCooldown > 0.0f ? FString::Printf(TEXT("[E] Toggle grow lights   LiOH purge recharging %.0f s"), PurgeCooldown)
		: FString(TEXT("[E] Toggle grow lights   [hold E] Emergency LiOH purge")));
}

FString ACo2ScrubberStation::GetScreenText() const
{
	const ABioDomeModule* Dome = GetModuleAs<ABioDomeModule>();
	return Dome ? FString::Printf(TEXT("SEC-03 SCRUBBER\nCO2 %.0f ppm\nCHLORELLA %.0f%%\nLIGHTS %s"), Dome->CurrentCo2Ppm,
		Dome->ChlorellaCultureHealth * 100.0f, Dome->bGrowLightsOnline ? TEXT("ON") : TEXT("OFF")) : StationName;
}

// ============================================================================
// SEC-03 Water / food from the dome
// ============================================================================

AWaterDispenserStation::AWaterDispenserStation()
{
	StationName = TEXT("WRS WATER");
	PropKind = EMarsPropKind::WaterDispenser;
	ConsoleSize = FVector(50, 50, 185);
	ScreenOffset = FVector(27, 0, 118);
	ScreenGlow->SetLightColor(FLinearColor(0.3f, 0.5f, 1.0f));
}

void AWaterDispenserStation::Interact_Implementation(AMarsCharacter* C)
{
	UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	if (!Bio || ReserveLiters < 0.5f)
	{
		return;
	}
	if (Bio->Hydration < 0.9f)
	{
		Bio->Drink(0.3f);
		ReserveLiters -= 0.5f;
		C->ShowNotification(Prompt(TEXT("DRANK 500 mL")), 1.5f);
	}
	else if (C->GetInventory()->AddItem(EMarsItemType::WaterPouch, 1) > 0)
	{
		ReserveLiters -= 0.5f;
		C->ShowNotification(Prompt(TEXT("+1 WATER POUCH")), 1.5f);
	}
	Beep(C);
}

FText AWaterDispenserStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	return Prompt(Bio && Bio->Hydration < 0.9f ? TEXT("[E] Drink") : TEXT("[E] Fill water pouch"));
}

FString AWaterDispenserStation::GetScreenText() const
{
	return FString::Printf(TEXT("WRS\n%.1f L"), ReserveLiters);
}

AChlorellaHarvesterStation::AChlorellaHarvesterStation()
{
	StationName = TEXT("CHLORELLA HARVEST");
	PropKind = EMarsPropKind::Terminal;
	ScreenGlow->SetLightColor(FLinearColor(0.3f, 1.0f, 0.3f));
}

void AChlorellaHarvesterStation::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Cooldown = FMath::Max(Cooldown - DeltaTime, 0.0f);
}

void AChlorellaHarvesterStation::Interact_Implementation(AMarsCharacter* C)
{
	ABioDomeModule* Dome = GetModuleAs<ABioDomeModule>();
	if (!C || !Dome)
	{
		return;
	}
	if (Cooldown > 0.0f || Dome->ChlorellaCultureHealth < 0.3f)
	{
		Beep(C, 0.5f);
		C->ShowNotification(Prompt(TEXT("CULTURE DENSITY TOO LOW")), 2.0f);
		return;
	}
	if (C->GetInventory()->AddItem(EMarsItemType::ChlorellaPaste, 1) > 0)
	{
		Dome->ChlorellaCultureHealth = FMath::Max(Dome->ChlorellaCultureHealth - 0.05f, 0.0f);
		Cooldown = 90.0f;
		Beep(C);
		C->ShowNotification(Prompt(TEXT("+1 CHLORELLA PASTE")), 2.0f);
	}
}

FText AChlorellaHarvesterStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	return Prompt(Cooldown > 0.0f ? FString::Printf(TEXT("Regrowing culture... %.0f s"), Cooldown) : FString(TEXT("[E] Harvest Chlorella biomass")));
}

FString AChlorellaHarvesterStation::GetScreenText() const
{
	const ABioDomeModule* Dome = GetModuleAs<ABioDomeModule>();
	return FString::Printf(TEXT("HARVESTER\nDENSITY %.0f%%"), Dome ? Dome->ChlorellaCultureHealth * 100.0f : 0.0f);
}

// ============================================================================
// SEC-04 Xenobiology
// ============================================================================

AXenoAnalyzerStation::AXenoAnalyzerStation()
{
	StationName = TEXT("SPECTROMETER");
	PropKind = EMarsPropKind::Spectrometer;
	ConsoleSize = FVector(80, 120, 130);
	ScreenOffset = FVector(42, 40, 82);
	ScreenTextSize = 3.0f;
	ScreenGlow->SetLightColor(FLinearColor(0.6f, 0.3f, 1.0f));
}

void AXenoAnalyzerStation::Interact_Implementation(AMarsCharacter* C)
{
	UMarsInventoryComponent* Inv = C ? C->GetInventory() : nullptr;
	UMarsStationSubsystem* Station = StationOf(this);
	if (!Inv || !Station)
	{
		return;
	}

	if (Inv->RemoveItem(EMarsItemType::BioAgentCulture, 1))
	{
		Station->SetBioAgentCharacterized();
		Station->RecordSampleAnalysed();
		C->ShowPanel(TEXT(
			"XENOLAB ASSAY // BIO-AGENT CULTURE\n"
			"--------------------------------------------\n"
			"Organism  : obligate psychrophilic extremophile\n"
			"Metabolism: perchlorate-respiring (ClO4- -> Cl- + O2)\n"
			"Secretes  : low-MW neuroactive alkaloid, crosses BBB\n"
			"            when radiation raises permeability\n"
			"Effect    : 5-HT2A agonism -> visual/auditory\n"
			"            hallucinations, autonomic instability\n"
			"--------------------------------------------\n"
			"TARGETS UNLOCKED IN BIOPRINTER:\n"
			"  > NEURO-ANTITOXIN (metabolite chelator)\n"
			"  > AMIFOSTINE (radioprotectant, restores BBB)"), 18.0f);
		Beep(C, 1.6f);
		return;
	}

	if (!Inv->RemoveItem(EMarsItemType::RockSample, 1) && !Inv->RemoveItem(EMarsItemType::RegolithSample, 1))
	{
		Beep(C, 0.5f);
		C->ShowNotification(Prompt(TEXT("NO SAMPLE LOADED")), 2.0f);
		return;
	}

	static const TCHAR* Findings[] =
	{
		TEXT("RAMAN: Mg(ClO4)2 / Ca(ClO4)2 at 0.6 wt%.\nInhaled dust is the crew's dominant ClO4- route.\nPerchlorate oxidises haemoglobin -> MetHb."),
		TEXT("MICROSCOPY: endolithic micro-colonies in basalt vesicles.\nSame organism as the deep-core contaminant.\nDormant at surface UV levels."),
		TEXT("ISOTOPES: delta-37Cl shift confirms biological\nperchlorate reduction. Organism is the source\nof the neuroactive metabolite in crew blood."),
		TEXT("CORRELATION: crew perceptual events match\nmetabolite load + hypercapnia + GCR exposure.\nNO EXTERNAL INTRUDER IS PHYSICALLY POSSIBLE.")
	};
	C->ShowPanel(FString::Printf(TEXT("SAMPLE ANALYSIS %d\n%s"), FindingsIndex + 1, Findings[FindingsIndex % UE_ARRAY_COUNT(Findings)]), 14.0f);
	++FindingsIndex;
	Station->RecordSampleAnalysed();
	Beep(C, 1.3f);
}

FText AXenoAnalyzerStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const UMarsInventoryComponent* Inv = C ? C->GetInventory() : nullptr;
	if (Inv && Inv->HasItem(EMarsItemType::BioAgentCulture))
	{
		return Prompt(TEXT("[E] Assay BIO-AGENT culture"));
	}
	const int32 Samples = Inv ? Inv->GetItemCount(EMarsItemType::RockSample) + Inv->GetItemCount(EMarsItemType::RegolithSample) : 0;
	return Prompt(FString::Printf(TEXT("[E] Analyse geological sample (%d carried)"), Samples));
}

FString AXenoAnalyzerStation::GetScreenText() const
{
	const UMarsStationSubsystem* Station = StationOf(this);
	return FString::Printf(TEXT("SPECTRO\n%s"), Station && Station->IsBioAgentCharacterized() ? TEXT("AGENT: KNOWN") : TEXT("AGENT: ???"));
}

AMicroscopeStation::AMicroscopeStation()
{
	StationName = TEXT("ELECTRON MICROSCOPE");
	PropKind = EMarsPropKind::Microscope;
	ConsoleSize = FVector(80, 70, 170);
	ScreenOffset = FVector(41, 0, 60);
	ScreenTextSize = 3.0f;
}

void AMicroscopeStation::Interact_Implementation(AMarsCharacter* C)
{
	const UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	if (!Bio)
	{
		return;
	}
	const int32 PerField = FMath::RoundToInt(Bio->ExtremophileBioLoad * 140.0f);
	C->ShowPanel(FString::Printf(TEXT(
		"SEM // PERIPHERAL BLOOD SMEAR (SELF)\n"
		"--------------------------------------------\n"
		"Erythrocytes : %s\n"
		"Inclusions   : rod-shaped bodies 0.6 um\n"
		"Count        : ~%d per high-power field\n"
		"Heinz bodies : %s\n"
		"--------------------------------------------\n"
		"%s"),
		Bio->MethemoglobinPercentage > 10.0f ? TEXT("dark, oxidised (MetHb)") : TEXT("normal morphology"),
		PerField,
		Bio->MethemoglobinPercentage > 15.0f ? TEXT("PRESENT (oxidant stress)") : TEXT("absent"),
		PerField > 20 ? TEXT("FOREIGN ORGANISM IN BLOOD. ASSAY CULTURE IN SPECTROMETER.") : TEXT("Low organism burden.")), 14.0f);
	Beep(C, 1.1f);
}

FText AMicroscopeStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	return Prompt(TEXT("[E] Examine own blood smear"));
}

AContainmentBoxStation::AContainmentBoxStation()
{
	StationName = TEXT("BSL-3 CONTAINMENT");
	PropKind = EMarsPropKind::ContainmentBox;
	ConsoleSize = FVector(110, 80, 150);
	ScreenOffset = FVector(49, 0, 152);
	ScreenTextSize = 3.0f;
	ScreenGlow->SetLightColor(FLinearColor(1.0f, 0.2f, 0.1f));
}

void AContainmentBoxStation::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Cooldown = FMath::Max(Cooldown - DeltaTime, 0.0f);
}

void AContainmentBoxStation::Interact_Implementation(AMarsCharacter* C)
{
	if (!C || Cooldown > 0.0f)
	{
		return;
	}
	if (C->GetInventory()->AddItem(EMarsItemType::BioAgentCulture, 1) > 0)
	{
		Cooldown = 240.0f;
		// Glove-box breach risk: the suit protects, bare hands do not.
		if (!C->GetSuit()->IsSuitDonned())
		{
			C->GetBiometrics()->ExposeToBioAgent(0.03f);
		}
		C->ShowNotification(Prompt(TEXT("+1 BIO-AGENT CULTURE (sealed)")), 2.5f);
		Beep(C);
	}
}

FText AContainmentBoxStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	if (Cooldown > 0.0f)
	{
		return Prompt(FString::Printf(TEXT("Culture incubating... %.0f s"), Cooldown));
	}
	return Prompt(C && C->GetSuit() && C->GetSuit()->IsSuitDonned() ? TEXT("[E] Extract bio-agent culture") : TEXT("[E] Extract bio-agent culture (!! exposure risk without suit)"));
}

FString AContainmentBoxStation::GetScreenText() const
{
	return TEXT("BIOHAZARD\nBSL-3");
}

// ============================================================================
// SEC-05 Med-Bay
// ============================================================================

ADiagnosticScannerBed::ADiagnosticScannerBed()
{
	StationName = TEXT("DIAGNOSTIC SCANNER");
	PropKind = EMarsPropKind::ScannerBed;
	ConsoleSize = FVector(200, 110, 190);
	ScreenOffset = FVector(-84, 0, 112);
	ScreenTextSize = 3.0f;
	ScreenGlow->SetLightColor(FLinearColor(0.2f, 0.8f, 1.0f));
}

void ADiagnosticScannerBed::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	IdleTime += DeltaTime;
	if (IdleTime > 0.5f)
	{
		HoldTime = 0.0f;
	}
}

void ADiagnosticScannerBed::Interact_Implementation(AMarsCharacter* C)
{
	const AMedicalBayModule* MedBay = GetModuleAs<AMedicalBayModule>();
	if (C && MedBay)
	{
		C->ShowDiagnostic(18.0f);
		AMarsTutorialManager::Report(this, TEXT("ScannerUsed"));
		Beep(C, 1.2f);
	}
}

void ADiagnosticScannerBed::InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds)
{
	IdleTime = 0.0f;
	HoldTime += DeltaSeconds;
	UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	if (Bio && HoldTime >= 4.0f)
	{
		HoldTime = -1000.0f; // one stabilisation per hold
		if (Bio->EmergencyStabilize())
		{
			C->ShowNotification(Prompt(TEXT("EMERGENCY STABILISATION: O2, MB infusion, chelation cycle complete")), 4.0f);
			Beep(C, 1.8f);
		}
		else
		{
			C->ShowNotification(Prompt(TEXT("STABILISATION CYCLE RECHARGING")), 2.5f);
			Beep(C, 0.5f);
		}
	}
}

FText ADiagnosticScannerBed::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	if (HoldTime > 0.2f && HoldTime < 4.0f)
	{
		return Prompt(FString::Printf(TEXT("STABILISING... %.0f%%"), HoldTime / 4.0f * 100.0f));
	}
	return Prompt(TEXT("[E] Full diagnostic scan   [hold E] Emergency stabilisation"));
}

FString ADiagnosticScannerBed::GetScreenText() const
{
	const AMarsCharacter* C = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	return Bio ? FString::Printf(TEXT("SCANNER\nSTAGE %d\nHR %.0f"), Bio->GetStageIndex(), Bio->HeartRateBpm) : StationName;
}

ABioprinterStation::ABioprinterStation()
{
	StationName = TEXT("BIOPRINTER");
	PropKind = EMarsPropKind::Terminal;
	ScreenGlow->SetLightColor(FLinearColor(0.3f, 0.5f, 1.0f));
	ScreenTextSize = 3.2f;
}

void ABioprinterStation::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	IdleTime += DeltaTime;
	if (IdleTime > 0.5f)
	{
		HoldTime = 0.0f;
	}
}

void ABioprinterStation::Interact_Implementation(AMarsCharacter* C)
{
	const AMedicalBayModule* MedBay = GetModuleAs<AMedicalBayModule>();
	if (!MedBay)
	{
		return;
	}
	const int32 Count = MedBay->GetAvailableRecipes().Num();
	SelectedRecipe = Count > 0 ? (SelectedRecipe + 1) % Count : 0;
	Beep(C, 1.0f);
}

void ABioprinterStation::InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds)
{
	IdleTime = 0.0f;
	HoldTime += DeltaSeconds;
	AMedicalBayModule* MedBay = GetModuleAs<AMedicalBayModule>();
	if (!C || !MedBay || HoldTime < 1.0f)
	{
		return;
	}
	HoldTime = -1000.0f;

	if (const UMarsBaseManager* BaseMgr = UMarsBaseManager::Get(this); BaseMgr && BaseMgr->IsCraftingLocked() && MedBay->GetPendingOutputCount() == 0)
	{
		C->ShowNotification(FText::FromString(FString::Printf(TEXT("BIOPRINTER LOCKED - %s. REPAIR / RESTORE POWER"), *BaseMgr->GetStatus().CraftingLockReason)), 3.5f);
		Beep(C, 0.5f);
		return;
	}

	if (MedBay->GetPendingOutputCount() > 0)
	{
		C->ShowNotification(Prompt(FString::Printf(TEXT("COLLECTED %d DOSE(S)"), MedBay->CollectOutput(C->GetInventory()))), 2.5f);
		AMarsTutorialManager::Report(this, TEXT("MBPrinted"));
		Beep(C, 1.5f);
		return;
	}
	const TArray<EMarsItemType> Recipes = MedBay->GetAvailableRecipes();
	if (Recipes.IsValidIndex(SelectedRecipe) && MedBay->StartSynthesis(Recipes[SelectedRecipe]))
	{
		C->ShowNotification(Prompt(TEXT("SYNTHESIS STARTED: ") + ItemName(Recipes[SelectedRecipe])), 2.5f);
		Beep(C, 1.3f);
	}
	else
	{
		C->ShowNotification(Prompt(MedBay->IsSynthesizing() ? TEXT("BIOPRINTER BUSY") : TEXT("NO FEEDSTOCK")), 2.0f);
		Beep(C, 0.5f);
	}
}

FText ABioprinterStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const AMedicalBayModule* MedBay = GetModuleAs<AMedicalBayModule>();
	if (!MedBay)
	{
		return Prompt(TEXT("BIOPRINTER OFFLINE"));
	}
	if (MedBay->GetPendingOutputCount() > 0)
	{
		return Prompt(FString::Printf(TEXT("[hold E] Collect %d dose(s)"), MedBay->GetPendingOutputCount()));
	}
	if (MedBay->IsSynthesizing())
	{
		return Prompt(FString::Printf(TEXT("SYNTHESIS %.0f%%"), MedBay->GetSynthesisProgress() * 100.0f));
	}
	const TArray<EMarsItemType> Recipes = MedBay->GetAvailableRecipes();
	const FString Selected = Recipes.IsValidIndex(SelectedRecipe) ? ItemName(Recipes[SelectedRecipe]) : TEXT("-");
	return Prompt(FString::Printf(TEXT("[E] Recipe: %s   [hold E] Print (feedstock %d)"), *Selected, MedBay->BioprinterFeedstock));
}

FString ABioprinterStation::GetScreenText() const
{
	const AMedicalBayModule* MedBay = GetModuleAs<AMedicalBayModule>();
	if (!MedBay)
	{
		return StationName;
	}
	const TArray<EMarsItemType> Recipes = MedBay->GetAvailableRecipes();
	return FString::Printf(TEXT("BIOPRINTER\n%s\n%s"),
		Recipes.IsValidIndex(SelectedRecipe) ? *ItemName(Recipes[SelectedRecipe]) : TEXT("-"),
		MedBay->IsSynthesizing() ? *FString::Printf(TEXT("PRINTING %.0f%%"), MedBay->GetSynthesisProgress() * 100.0f)
			: MedBay->GetPendingOutputCount() > 0 ? TEXT("READY") : TEXT("IDLE"));
}

// ============================================================================
// SEC-06 Storage & Comm-Relay
// ============================================================================

ARtgCoreStation::ARtgCoreStation()
{
	StationName = TEXT("RTG CORE");
	PropKind = EMarsPropKind::RtgCore;
	ConsoleSize = FVector(130, 130, 140);
	ScreenOffset = FVector(0, 0, 165);
	ScreenTextSize = 5.0f;
	ScreenGlow->SetLightColor(FLinearColor(1.0f, 0.45f, 0.1f));
}

void ARtgCoreStation::Interact_Implementation(AMarsCharacter* C)
{
	const UMarsStationSubsystem* Station = StationOf(this);
	if (!C || !Station)
	{
		return;
	}
	FString Panel = FString::Printf(TEXT("RTG ARRAY / POWER GRID\n--------------------------------------------\nSUPPLY  %.0f W\nDEMAND  %.0f W\nAVAIL   %.0f %%\n--------------------------------------------\n"),
		Station->GetPowerSupplyW(), Station->GetPowerDemandW(), Station->GetPowerAvailability() * 100.0f);
	for (const AMarsBaseModule* M : Station->GetModules())
	{
		if (M->ModuleType != EMarsModuleType::RingCorridor)
		{
			Panel += FString::Printf(TEXT("%-28s %5.0f W\n"), *M->GetModuleName(), M->GetPowerDemandW());
		}
	}
	C->ShowPanel(Panel, 12.0f);
	Beep(C);
}

FText ARtgCoreStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	return Prompt(TEXT("[E] Grid load report"));
}

FString ARtgCoreStation::GetScreenText() const
{
	const UMarsStationSubsystem* Station = StationOf(this);
	return Station ? FString::Printf(TEXT("RTG %.0f W"), Station->GetPowerSupplyW()) : StationName;
}

ACommRelayTerminal::ACommRelayTerminal()
{
	StationName = TEXT("QUANTUM SOCKETS");
	PropKind = EMarsPropKind::Terminal;
	ScreenGlow->SetLightColor(FLinearColor(0.2f, 0.7f, 1.0f));
	ScreenTextSize = 3.2f;
}

void ACommRelayTerminal::Interact_Implementation(AMarsCharacter* C)
{
	ACommRelayModule* Relay = GetModuleAs<ACommRelayModule>();
	UMarsInventoryComponent* Inv = C ? C->GetInventory() : nullptr;
	if (!Relay || !Inv)
	{
		return;
	}
	UMarsNarrativeDirector::Report(this, TEXT("CommTerminalUsed"));

	if (!Relay->AreTransistorsInstalled())
	{
		const int32 Installed = Relay->InstallTransistors(Inv->GetItemCount(EMarsItemType::QuantumTransistor));
		if (Installed > 0)
		{
			Inv->RemoveItem(EMarsItemType::QuantumTransistor, Installed);
			C->ShowNotification(Prompt(FString::Printf(TEXT("SOCKETED %d QUANTUM TRANSISTOR(S) (%d/%d)"), Installed, Relay->InstalledTransistors, Relay->RequiredTransistors)), 3.0f);
			Beep(C, 1.4f);
		}
		else
		{
			C->ShowNotification(Prompt(TEXT("QUANTUM TRANSISTORS REQUIRED - STORAGE, RELAY R-3, DRILL RIG")), 4.0f);
			Beep(C, 0.5f);
		}
		return;
	}
	if (Relay->TransceiverIntegrity < 0.6f)
	{
		if (Inv->RemoveItem(EMarsItemType::SpareParts, 1))
		{
			Relay->RepairTransceiver(0.25f);
			C->ShowNotification(Prompt(FString::Printf(TEXT("TRANSCEIVER REPAIRED TO %.0f%%"), Relay->TransceiverIntegrity * 100.0f)), 3.0f);
			Beep(C, 1.2f);
		}
		else
		{
			C->ShowNotification(Prompt(TEXT("SPARE PARTS REQUIRED")), 3.0f);
			Beep(C, 0.5f);
		}
		return;
	}
	if (Relay->IsReadyToTransmit() && Relay->TransmitToEarth())
	{
		C->ShowNotification(Prompt(TEXT("QUANTUM LINK LOCKED - TRANSMITTING TO EARTH")), 6.0f);
		Beep(C, 2.0f);
	}
	else
	{
		C->ShowNotification(Prompt(TEXT("ALIGN THE ORBITAL DISH AT THE ALIGNMENT TERMINAL")), 3.0f);
	}
}

FText ACommRelayTerminal::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const ACommRelayModule* Relay = GetModuleAs<ACommRelayModule>();
	if (!Relay)
	{
		return Prompt(TEXT("TRANSCEIVER OFFLINE"));
	}
	if (!Relay->AreTransistorsInstalled())
	{
		return Prompt(FString::Printf(TEXT("[E] Socket quantum transistors (%d/%d)"), Relay->InstalledTransistors, Relay->RequiredTransistors));
	}
	if (Relay->TransceiverIntegrity < 0.6f)
	{
		return Prompt(FString::Printf(TEXT("[E] Repair transceiver with spare parts (%.0f%%)"), Relay->TransceiverIntegrity * 100.0f));
	}
	return Prompt(Relay->IsReadyToTransmit() ? TEXT("[E] TRANSMIT TO EARTH") : TEXT("Link not aligned - use dish alignment terminal"));
}

FString ACommRelayTerminal::GetScreenText() const
{
	const ACommRelayModule* Relay = GetModuleAs<ACommRelayModule>();
	return Relay ? FString::Printf(TEXT("Q-SOCKETS %d/%d\nTRX %.0f%%\nLINK %s"), Relay->InstalledTransistors, Relay->RequiredTransistors,
		Relay->TransceiverIntegrity * 100.0f, Relay->IsReadyToTransmit() ? TEXT("READY") : TEXT("DOWN")) : StationName;
}

ADishAlignmentTerminal::ADishAlignmentTerminal()
{
	StationName = TEXT("DISH ALIGNMENT");
	PropKind = EMarsPropKind::Terminal;
	ScreenGlow->SetLightColor(FLinearColor(0.2f, 0.7f, 1.0f));
	ScreenTextSize = 3.2f;
}

void ADishAlignmentTerminal::InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds)
{
	if (ACommRelayModule* Relay = GetModuleAs<ACommRelayModule>())
	{
		Relay->Calibrate(DeltaSeconds);
	}
}

FText ADishAlignmentTerminal::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const ACommRelayModule* Relay = GetModuleAs<ACommRelayModule>();
	if (!Relay)
	{
		return Prompt(TEXT("ALIGNMENT OFFLINE"));
	}
	const FString Blocking = Relay->GetBlockingReason();
	if (!Blocking.IsEmpty())
	{
		return Prompt(Blocking);
	}
	return Prompt(FString::Printf(TEXT("[hold E] Align orbital dish %.0f%%"), Relay->CalibrationProgress * 100.0f));
}

FString ADishAlignmentTerminal::GetScreenText() const
{
	const ACommRelayModule* Relay = GetModuleAs<ACommRelayModule>();
	const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
	return Relay ? FString::Printf(TEXT("DISH ALIGN\n%.0f %%\nDUST TAU %.1f"), Relay->CalibrationProgress * 100.0f, Env ? Env->GetStormIntensity() * 5.0f : 0.0f) : StationName;
}

AFoodLockerStation::AFoodLockerStation()
{
	StationName = TEXT("FOOD LOCKER");
	PropKind = EMarsPropKind::FoodLocker;
	ConsoleSize = FVector(60, 120, 180);
	ScreenOffset = FVector(32, 30, 165);
	ScreenTextSize = 3.0f;
	ScreenGlow->SetLightColor(FLinearColor(1.0f, 0.7f, 0.2f));
}

void AFoodLockerStation::Interact_Implementation(AMarsCharacter* C)
{
	UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	if (!Bio || RationsAvailable <= 0)
	{
		Beep(C, 0.5f);
		return;
	}
	if (Bio->Nutrition < 0.85f)
	{
		Bio->Eat(0.4f);
		C->ShowNotification(Prompt(TEXT("ATE A RATION (~900 kcal)")), 2.0f);
		--RationsAvailable;
	}
	else if (C->GetInventory()->AddItem(EMarsItemType::FoodRation, 1) > 0)
	{
		C->ShowNotification(Prompt(TEXT("+1 FOOD RATION")), 2.0f);
		--RationsAvailable;
	}
	Beep(C);
}

FText AFoodLockerStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	return Prompt(FString::Printf(TEXT("[E] %s (%d left)"), Bio && Bio->Nutrition < 0.85f ? TEXT("Eat a ration") : TEXT("Take a ration"), RationsAvailable));
}

FString AFoodLockerStation::GetScreenText() const
{
	return FString::Printf(TEXT("RATIONS %d"), RationsAvailable);
}

// ============================================================================
// Screen status (colour coding)
// ============================================================================

int32 AAirlockPanelStation::GetScreenStatus() const
{
	const AAirlockModule* Airlock = GetModuleAs<AAirlockModule>();
	return Airlock && Airlock->IsCycling() ? 1 : 0;
}

int32 AModuleStatusPanel::GetScreenStatus() const
{
	const UMarsStationSubsystem* Station = StationOf(this);
	if (Mode == EStatusPanelMode::GridBreaker)
	{
		return !Station ? 0 : Station->GetPowerAvailability() < 0.6f ? 2 : Station->GetPowerAvailability() < 1.0f ? 1 : 0;
	}
	const AMarsBaseModule* M = ResolveModule();
	if (!M)
	{
		return 0;
	}
	if (M->CurrentPressureKpa < 60.0f || M->SealIntegrity < 0.5f || M->IsBreached() || M->CurrentCo2Ppm > 8000.0f)
	{
		return 2;
	}
	return (M->PunctureCount > 0 || M->SealIntegrity < 0.8f || M->SealTemperatureC < M->SealCriticalTemperatureC || M->CurrentCo2Ppm > 3000.0f) ? 1 : 0;
}

int32 ACo2ScrubberStation::GetScreenStatus() const
{
	const ABioDomeModule* Dome = GetModuleAs<ABioDomeModule>();
	return !Dome ? 0 : Dome->CurrentCo2Ppm > 5000.0f ? 2 : (Dome->CurrentCo2Ppm > 2000.0f || Dome->ChlorellaCultureHealth < 0.4f) ? 1 : 0;
}

int32 ADiagnosticScannerBed::GetScreenStatus() const
{
	const AMarsCharacter* C = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	return !Bio ? 0 : Bio->GetStageIndex() >= 3 ? 2 : Bio->GetStageIndex() >= 1 ? 1 : 0;
}

int32 ARtgCoreStation::GetScreenStatus() const
{
	const UMarsStationSubsystem* Station = StationOf(this);
	return !Station ? 0 : Station->GetPowerAvailability() < 0.6f ? 2 : Station->GetPowerAvailability() < 1.0f ? 1 : 0;
}

int32 ACommRelayTerminal::GetScreenStatus() const
{
	const ACommRelayModule* Relay = GetModuleAs<ACommRelayModule>();
	return !Relay ? 0 : Relay->IsReadyToTransmit() ? 0 : Relay->TransceiverIntegrity < 0.6f ? 2 : 1;
}

int32 ADishAlignmentTerminal::GetScreenStatus() const
{
	const ACommRelayModule* Relay = GetModuleAs<ACommRelayModule>();
	return !Relay ? 0 : Relay->CalibrationProgress >= 1.0f ? 0 : Relay->IsReadyToCalibrate() ? 1 : 2;
}
