#include "Player/MarsCharacter.h"

#include "MarsSurvival.h"
#include "AI/AriaSubsystem.h"
#include "Audio/MarsAudioManager.h"
#include "Audio/MarsProceduralSynth.h"
#include "Audio/AriaVoiceComponent.h"
#include "Core/MarsTutorialManager.h"
#include "Base/MarsBaseModule.h"
#include "Core/MarsGameMode.h"
#include "Core/MarsInteractable.h"
#include "Core/MarsHUD.h"
#include "Core/MarsUserSettings.h"
#include "Props/MarsPropBuilder.h"
#include "Player/MarsSuitViewmodel.h"
#include "Vehicles/MarsReconShuttle.h"
#include "EngineUtils.h"
#include "Player/PlayerFatigueComponent.h"
#include "Components/SpotLightComponent.h"
#include "Items/MarsPickup.h"
#include "World/MarsSolarStormManager.h"
#include "Core/MarsNarrativeDirector.h"
#include "Equipment/FuturisticGlock18.h"
#include "Equipment/MarsTools.h"
#include "Hallucination/HallucinationDirectorComponent.h"
#include "Hallucination/HallucinationPhantom.h"
#include "Items/MarsInventoryComponent.h"
#include "Items/MarsItemLibrary.h"
#include "Player/EVASuitComponent.h"
#include "Rendering/RetroPostProcessComponent.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsStationSubsystem.h"
#include "World/SurfaceBeacon.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// Mars surface atmosphere: ~95% CO2, 0.13% O2, ~0.6 kPa.
	constexpr float MarsAmbientPressureKpa = 0.6f;
	constexpr float MarsAmbientCo2Ppm = 950000.0f;
	constexpr float MarsAmbientO2Fraction = 0.0013f;

	FMarsBeltSlot Belt(EMarsItemType Type, const TCHAR* Name, bool bCount)
	{
		FMarsBeltSlot S;
		S.ItemType = Type;
		S.ShortName = Name;
		S.bShowCount = bCount;
		return S;
	}
}

AMarsCharacter::AMarsCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(38.0f, 90.0f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 68.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(80.0f);

	SuitViewmodel = CreateDefaultSubobject<UMarsSuitViewmodel>(TEXT("SuitViewmodel"));
	SuitViewmodel->SetupAttachment(FirstPersonCamera);

	Fatigue = CreateDefaultSubobject<UPlayerFatigueComponent>(TEXT("Fatigue"));

	HelmetLamp = CreateDefaultSubobject<USpotLightComponent>(TEXT("HelmetLamp"));
	HelmetLamp->SetupAttachment(FirstPersonCamera);
	HelmetLamp->SetRelativeLocation(FVector(10.0f, 12.0f, 8.0f));
	HelmetLamp->SetIntensityUnits(ELightUnits::Lumens);
	HelmetLamp->SetIntensity(1800.0f);
	HelmetLamp->SetAttenuationRadius(2600.0f);
	HelmetLamp->SetInnerConeAngle(14.0f);
	HelmetLamp->SetOuterConeAngle(30.0f);
	HelmetLamp->SetLightColor(FLinearColor(0.85f, 0.92f, 1.0f));
	HelmetLamp->SetCastShadows(false);
	HelmetLamp->SetVisibility(false);

	Biometrics = CreateDefaultSubobject<UPlayerBiometricsComponent>(TEXT("Biometrics"));
	Suit = CreateDefaultSubobject<UEVASuitComponent>(TEXT("EVASuit"));
	Inventory = CreateDefaultSubobject<UMarsInventoryComponent>(TEXT("Inventory"));
	HallucinationDirector = CreateDefaultSubobject<UHallucinationDirectorComponent>(TEXT("HallucinationDirector"));
	RetroPostProcess = CreateDefaultSubobject<URetroPostProcessComponent>(TEXT("RetroPostProcess"));
	AudioManager = CreateDefaultSubobject<UMarsAudioManager>(TEXT("AudioManager"));
	Synth = CreateDefaultSubobject<UMarsProceduralSynth>(TEXT("ProceduralSynth"));
	Synth->SetupAttachment(GetCapsuleComponent());
	AriaVoice = CreateDefaultSubobject<UAriaVoiceComponent>(TEXT("AriaVoice"));

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GravityScale = 0.38f; // 3.72 m/s^2
	Move->JumpZVelocity = 280.0f;
	Move->AirControl = 0.25f;
	Move->MaxWalkSpeed = BaseWalkSpeed;
	Move->BrakingDecelerationWalking = 1200.0f;
	Move->NavAgentProps.bCanCrouch = true;
	Move->SetCrouchedHalfHeight(50.0f);
	Move->MaxWalkSpeedCrouched = 140.0f;

	EquipmentClasses.Add(EMarsItemType::Glock18, AFuturisticGlock18::StaticClass());
	EquipmentClasses.Add(EMarsItemType::WeldSealantGun, AWeldSealantTool::StaticClass());
	EquipmentClasses.Add(EMarsItemType::TorqueWrench, ATorqueWrenchTool::StaticClass());
	EquipmentClasses.Add(EMarsItemType::RadiationDosimeter, ARadiationDosimeter::StaticClass());

	BeltSlots =
	{
		Belt(EMarsItemType::Glock18, TEXT("G-18"), false),
		Belt(EMarsItemType::WeldSealantGun, TEXT("WELD"), false),
		Belt(EMarsItemType::TorqueWrench, TEXT("TORQ"), false),
		Belt(EMarsItemType::RadiationDosimeter, TEXT("DOSE"), false),
		Belt(EMarsItemType::WaterPouch, TEXT("H2O"), true),
		Belt(EMarsItemType::FoodRation, TEXT("FOOD"), true)
	};

	BeaconClass = ASurfaceBeacon::StaticClass();
}

void AMarsCharacter::BeginPlay()
{
	Super::BeginPlay();
	GrantStartingLoadout();
	Biometrics->OnDeath.AddDynamic(this, &AMarsCharacter::HandleDeath);
	Biometrics->OnDiseaseStageChanged.AddDynamic(this, &AMarsCharacter::HandleStageChanged);
	if (bStartInSuit)
	{
		Suit->DonSuit();
	}
	FMarsUserSettings::Get().Apply(GetWorld());

	// Dev capture flags: "-AutoEquip=1" equips a belt slot, "-AutoWrist" raises the wrist computer.
	int32 AutoSlot = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoEquip="), AutoSlot) && AutoSlot > 0)
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, AutoSlot]() { UseBeltSlot(AutoSlot - 1); }), 3.0f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("AutoBoard")))
	{
		FTimerHandle BoardHandle;
		GetWorldTimerManager().SetTimer(BoardHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			for (TActorIterator<AMarsReconShuttle> It(GetWorld()); It; ++It)
			{
				It->Board(this);
				break;
			}
		}), 2.0f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("AutoWrist")) && SuitViewmodel)
	{
		SuitViewmodel->ToggleRaised();
	}
}

void AMarsCharacter::GrantStartingLoadout()
{
	if (Inventory->GetSlots().Num() > 0)
	{
		return;
	}
	Inventory->AddItem(EMarsItemType::Glock18, 1);
	Inventory->AddItem(EMarsItemType::Ammo9mm, 33);
	Inventory->AddItem(EMarsItemType::WeldSealantGun, 1);
	Inventory->AddItem(EMarsItemType::SealantCartridge, 1);
	Inventory->AddItem(EMarsItemType::TorqueWrench, 1);
	Inventory->AddItem(EMarsItemType::RadiationDosimeter, 1);
	Inventory->AddItem(EMarsItemType::MethyleneBlueCartridge, 1);
	Inventory->AddItem(EMarsItemType::WaterPouch, 1);
	Inventory->AddItem(EMarsItemType::FoodRation, 1);
	Inventory->AddItem(EMarsItemType::SurfaceBeacon, 3);
}

// ----------------------------------------------------------------------------
// Tick
// ----------------------------------------------------------------------------

void AMarsCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateNotifications(DeltaTime);
	if (bIsDead)
	{
		return;
	}

	UpdateCurrentModule();
	UpdateBreathingEnvironment(DeltaTime);
	UpdateInteractionFocus();
	UpdateMovement(DeltaTime);
	UpdateTremor(DeltaTime);
	UpdateViewFeel(DeltaTime);
	UpdateSleep(DeltaTime);

	if (bInteractHeld && FocusedActor.IsValid())
	{
		IMarsInteractable::Execute_InteractHold(FocusedActor.Get(), this, DeltaTime);
	}
}

void AMarsCharacter::UpdateCurrentModule()
{
	const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>();
	const FVector Eye = FirstPersonCamera->GetComponentLocation();
	AMarsBaseModule* Found = Station ? Station->FindModuleContainingPoint(Eye) : nullptr;
	// Doorways sit inside the wall thickness between two interior volumes. Don't treat that gap as the
	// Martian surface (it used to hit the lungs with 0.6 kPa for a frame -> hypoxia, panic, HR spike).
	if (!Found && CurrentModule.IsValid() && CurrentModule->IsPointInside(Eye, 160.0f))
	{
		Found = CurrentModule.Get();
	}
	if (!Found && Station)
	{
		for (AMarsBaseModule* M : Station->GetModules())
		{
			if (M && M->IsPointInside(Eye, 60.0f))
			{
				Found = M;
				break;
			}
		}
	}
	CurrentModule = Found;
}

void AMarsCharacter::UpdateBreathingEnvironment(float DeltaTime)
{
	const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
	const float ExternalTemp = Env ? Env->GetAmbientTemperatureC() : -60.0f;
	const float Storm = Env ? Env->GetStormIntensity() : 0.0f;

	AMarsBaseModule* Module = CurrentModule.Get();
	const bool bOutside = Module == nullptr;
	const float LocalTemp = bOutside ? ExternalTemp : Module->CurrentTemperatureC;

	Suit->UpdateSuit(DeltaTime, Biometrics->HeartRateBpm, LocalTemp, Storm, bOutside);

	if (Suit->IsHelmetSealed())
	{
		float Pressure, Co2, O2;
		Suit->GetBreathingEnvironment(Pressure, Co2, O2);
		const bool bHeated = Suit->BatteryWh > 0.0f;
		Biometrics->SetEnvironmentalConditions(Pressure, Co2, bHeated ? 22.0f : LocalTemp, O2, 0.0f);

		// Heater dead outside: hypothermia in ~2.5 real minutes (faster at night).
		if (!bHeated && bOutside)
		{
			ColdExposureSeconds += DeltaTime * (ExternalTemp < -60.0f ? 1.6f : 1.0f);
			Biometrics->Stamina = FMath::Max(Biometrics->Stamina - DeltaTime * 0.02f, 0.0f);
			if (FMath::Fmod(ColdExposureSeconds, 20.0f) < DeltaTime)
			{
				ShowNotification(FText::FromString(TEXT("SUIT HEATER OFFLINE - HYPOTHERMIA. RETURN TO BASE OR SHUTTLE")), 4.0f);
			}
			if (ColdExposureSeconds > 150.0f)
			{
				Biometrics->KillFromExposure(TEXT("Hypothermia (suit battery depleted)"));
			}
		}
		else
		{
			ColdExposureSeconds = FMath::Max(ColdExposureSeconds - DeltaTime * 2.0f, 0.0f);
		}
	}
	else if (Module)
	{
		Biometrics->SetEnvironmentalConditions(Module->CurrentPressureKpa, Module->CurrentCo2Ppm, Module->CurrentTemperatureC,
			Module->CurrentO2Fraction, Module->DustContamination);
	}
	else
	{
		Biometrics->SetEnvironmentalConditions(MarsAmbientPressureKpa, MarsAmbientCo2Ppm, ExternalTemp, MarsAmbientO2Fraction, 0.6f + 0.4f * Storm);
	}

	Biometrics->SetOnEVA(bOutside);
	if (bOutside && !bWasOutside && Env && Env->IsNight())
	{
		ShowNotification(FText::FromString(FString::Printf(TEXT("MARTIAN NIGHT %.0f C - SUIT HEATERS AT MAXIMUM, BATTERY DRAIN x2"), ExternalTemp)), 4.0f);
	}
	bWasOutside = bOutside;
	Biometrics->SetDustStormActive(bOutside && Storm > 0.5f);

	const UMarsSolarStormManager* SolarStorm = UMarsSolarStormManager::Get(this);
	const float Shelter = SolarStorm ? SolarStorm->GetIndoorDoseFactor(Module) : (bOutside ? 1.0f : 0.25f);
	const float Spe = Env ? Env->GetSolarParticleDoseRateMsvPerHour() * Shelter : 0.0f;
	Biometrics->SetAdditionalDoseRate(Spe + HazardDoseRateMsvPerHour);

	if (Module && Suit->DustLoad > 0.01f)
	{
		const float Transfer = Suit->DustLoad * 0.15f * DeltaTime;
		Module->AddDustContamination(Transfer);
		Suit->RemoveDust(Transfer);
	}
}

void AMarsCharacter::UpdateInteractionFocus()
{
	FocusedActor.Reset();
	FocusPrompt = FText::GetEmpty();

	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + FirstPersonCamera->GetForwardVector() * InteractionRangeCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MarsInteractTrace), false, this);
	if (EquippedItem)
	{
		Params.AddIgnoredActor(EquippedItem);
	}

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->GetClass()->ImplementsInterface(UMarsInteractable::StaticClass()))
		{
			FocusedActor = HitActor;
			FocusPrompt = IMarsInteractable::Execute_GetInteractionPrompt(HitActor, this);
		}
	}
}

void AMarsCharacter::UpdateMovement(float DeltaTime)
{
	const float Base = Suit->IsSuitDonned() ? SuitedWalkSpeed : BaseWalkSpeed;
	const float ThyroidFactor = FMath::Lerp(0.7f, 1.0f, Biometrics->ThyroidFunction);
	const float OxygenFactor = FMath::Clamp(Biometrics->EffectiveO2SaturationPct / 90.0f, 0.45f, 1.0f);
	const float StageFactor = Biometrics->GetDiseaseStage() == EDiseaseStage::SystemicCollapse ? 0.55f : 1.0f;

	const float Speed2D = GetVelocity().Size2D();
	const bool bMoving = Speed2D > 30.0f && !GetCharacterMovement()->IsFalling();
	const bool bSprinting = Biometrics->SetSprinting(bWantsSprint && bMoving);
	GetCharacterMovement()->MaxWalkSpeed = Base * ThyroidFactor * OxygenFactor * StageFactor * (bSprinting ? SprintMultiplier : 1.0f);
	// Walking around the station is light work (~+3 bpm); only sprinting really loads the heart.
	Biometrics->SetExertion(FMath::Clamp(Speed2D / FMath::Max(Base, 1.0f), 0.0f, 1.0f) * (bSprinting ? 1.0f : 0.1f));

	// Footsteps (boots on deck plate; muffled regolith outside).
	if (bMoving)
	{
		StepDistance += Speed2D * DeltaTime;
		const float Stride = bSprinting ? 170.0f : 130.0f;
		if (StepDistance >= Stride)
		{
			StepDistance = 0.0f;
			if (AudioManager)
			{
				AudioManager->PlayFootstep(CurrentModule.IsValid() ? 1.0f : 0.5f);
			}
		}
	}
}

void AMarsCharacter::UpdateTremor(float DeltaTime)
{
	// Stage 2+: micro-tremor / camera sway that genuinely moves the aim (weapon and wrench precision).
	const int32 Stage = Biometrics->GetStageIndex();
	const float Amp = FMath::Max(Stage - 1, 0) * 0.18f + Biometrics->PanicLevel * 0.12f + (Stage >= 4 ? 0.35f : 0.0f);
	if (Amp <= 0.0f || !Controller)
	{
		return;
	}
	TremorTime += DeltaTime * (2.0f + Stage);
	const FVector2D Now(FMath::PerlinNoise1D(TremorTime) * Amp, FMath::PerlinNoise1D(TremorTime + 37.3f) * Amp);
	const FVector2D Delta = Now - LastTremor;
	LastTremor = Now;
	AddControllerPitchInput(Delta.X);
	AddControllerYawInput(Delta.Y);
}

void AMarsCharacter::UpdateNotifications(float DeltaTime)
{
	for (int32 i = Notifications.Num() - 1; i >= 0; --i)
	{
		Notifications[i].RemainingSeconds -= DeltaTime;
		if (Notifications[i].RemainingSeconds <= 0.0f)
		{
			Notifications.RemoveAt(i);
		}
	}
	PanelRemaining = FMath::Max(PanelRemaining - DeltaTime, 0.0f);
	DiagnosticRemaining = FMath::Max(DiagnosticRemaining - DeltaTime, 0.0f);
}

// ----------------------------------------------------------------------------
// Input
// ----------------------------------------------------------------------------

void AMarsCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AMarsCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AMarsCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AMarsCharacter::OnTurn);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AMarsCharacter::OnLookUp);
	PlayerInputComponent->BindAction(TEXT("ToggleHUD"), IE_Pressed, this, &AMarsCharacter::OnToggleHUD);
	PlayerInputComponent->BindAction(TEXT("WristDisplay"), IE_Pressed, this, &AMarsCharacter::OnWristDisplay);
	PlayerInputComponent->BindAction(TEXT("Waypoints"), IE_Pressed, this, &AMarsCharacter::OnToggleWaypoints);
	PlayerInputComponent->BindAction(TEXT("HelmetLamp"), IE_Pressed, this, &AMarsCharacter::OnToggleLamp);
	PlayerInputComponent->BindAction(TEXT("Inventory"), IE_Pressed, this, &AMarsCharacter::OnInventory);
	FInputActionBinding& PauseBinding = PlayerInputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &AMarsCharacter::OnPause);
	PauseBinding.bExecuteWhenPaused = true;

	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &AMarsCharacter::OnSprintPressed);
	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &AMarsCharacter::OnSprintReleased);
	PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AMarsCharacter::OnInteractPressed);
	PlayerInputComponent->BindAction(TEXT("Interact"), IE_Released, this, &AMarsCharacter::OnInteractReleased);
	PlayerInputComponent->BindAction(TEXT("PrimaryAction"), IE_Pressed, this, &AMarsCharacter::OnPrimaryPressed);
	PlayerInputComponent->BindAction(TEXT("PrimaryAction"), IE_Released, this, &AMarsCharacter::OnPrimaryReleased);
	PlayerInputComponent->BindAction(TEXT("SecondaryAction"), IE_Pressed, this, &AMarsCharacter::OnSecondaryPressed);
	PlayerInputComponent->BindAction(TEXT("Reload"), IE_Pressed, this, &AMarsCharacter::OnReloadPressed);
	PlayerInputComponent->BindAction(TEXT("EquipSlot1"), IE_Pressed, this, &AMarsCharacter::OnSlot1);
	PlayerInputComponent->BindAction(TEXT("EquipSlot2"), IE_Pressed, this, &AMarsCharacter::OnSlot2);
	PlayerInputComponent->BindAction(TEXT("EquipSlot3"), IE_Pressed, this, &AMarsCharacter::OnSlot3);
	PlayerInputComponent->BindAction(TEXT("EquipSlot4"), IE_Pressed, this, &AMarsCharacter::OnSlot4);
	PlayerInputComponent->BindAction(TEXT("EquipSlot5"), IE_Pressed, this, &AMarsCharacter::OnSlot5);
	PlayerInputComponent->BindAction(TEXT("EquipSlot6"), IE_Pressed, this, &AMarsCharacter::OnSlot6);
	PlayerInputComponent->BindAction(TEXT("Holster"), IE_Pressed, this, &AMarsCharacter::Holster);
	PlayerInputComponent->BindAction(TEXT("UseO2Canister"), IE_Pressed, this, &AMarsCharacter::OnUseO2);
	PlayerInputComponent->BindAction(TEXT("UseMedication"), IE_Pressed, this, &AMarsCharacter::OnMedPressed);
	PlayerInputComponent->BindAction(TEXT("UseMedication"), IE_Released, this, &AMarsCharacter::OnMedReleased);
	PlayerInputComponent->BindAction(TEXT("DeployBeacon"), IE_Pressed, this, &AMarsCharacter::OnDeployBeacon);
	PlayerInputComponent->BindAction(TEXT("TalkToAria"), IE_Pressed, this, &AMarsCharacter::OnTalkToAria);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &AMarsCharacter::OnCrouchToggle);
}

void AMarsCharacter::MoveForward(float Value)
{
	if (Value != 0.0f && !bIsDead)
	{
		AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), Value);
	}
}

void AMarsCharacter::MoveRight(float Value)
{
	if (Value != 0.0f && !bIsDead)
	{
		AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Value);
	}
}

void AMarsCharacter::OnInteractPressed()
{
	if (IsInspecting())
	{
		EndInspect();
		return;
	}
	bInteractHeld = true;
	if (AActor* Target = FocusedActor.Get())
	{
		IMarsInteractable::Execute_Interact(Target, this);
	}
}

void AMarsCharacter::OnInteractReleased() { bInteractHeld = false; }
void AMarsCharacter::OnPrimaryPressed()
{
	if (IsInspecting()) { EndInspect(); return; }
	if (bIsDead) { return; }
	if (EquippedItem) { EquippedItem->StartPrimary(); return; }
	// Empty hands: left click works like E on whatever you are looking at (terminals, pickups, switches).
	OnInteractPressed();
}

void AMarsCharacter::OnPrimaryReleased()
{
	if (EquippedItem) { EquippedItem->StopPrimary(); return; }
	OnInteractReleased();
}
void AMarsCharacter::OnSecondaryPressed() { if (EquippedItem && !bIsDead) { EquippedItem->StartSecondary(); } }
void AMarsCharacter::OnReloadPressed() { if (EquippedItem && !bIsDead) { EquippedItem->Reload(); } }

void AMarsCharacter::UseBeltSlot(int32 Index)
{
	if (!BeltSlots.IsValidIndex(Index))
	{
		return;
	}
	const EMarsItemType Type = BeltSlots[Index].ItemType;
	if (UMarsItemLibrary::IsEquippable(Type))
	{
		EquipItem(Type);
		return;
	}
	// Consumable slots; the food slot falls back to Chlorella paste.
	if (Type == EMarsItemType::FoodRation && !Inventory->HasItem(Type) && Inventory->HasItem(EMarsItemType::ChlorellaPaste))
	{
		UseConsumable(EMarsItemType::ChlorellaPaste);
		return;
	}
	if (Type == EMarsItemType::None)
	{
		return;
	}
	UseInventoryItem(Type);
}

TArray<EMarsItemType> AMarsCharacter::GetCarriedMedications() const
{
	TArray<EMarsItemType> Meds;
	for (const EMarsItemType T : { EMarsItemType::MethyleneBlueCartridge, EMarsItemType::IodinePills, EMarsItemType::NeuroAntitoxin, EMarsItemType::Amifostine })
	{
		if (Inventory->HasItem(T))
		{
			Meds.Add(T);
		}
	}
	return Meds;
}

FString AMarsCharacter::GetSelectedMedicationLabel() const
{
	if (!Inventory->HasItem(SelectedMedication))
	{
		const TArray<EMarsItemType> Meds = GetCarriedMedications();
		if (Meds.Num() == 0)
		{
			return TEXT("EMPTY");
		}
		return FString::Printf(TEXT("%s x%d"), *UMarsItemLibrary::GetItemName(Meds[0]).ToString().ToUpper(), Inventory->GetItemCount(Meds[0]));
	}
	return FString::Printf(TEXT("%s x%d"), *UMarsItemLibrary::GetItemName(SelectedMedication).ToString().ToUpper(), Inventory->GetItemCount(SelectedMedication));
}

void AMarsCharacter::OnMedPressed()
{
	MedPressTime = GetWorld()->GetRealTimeSeconds();
}

void AMarsCharacter::OnMedReleased()
{
	const TArray<EMarsItemType> Meds = GetCarriedMedications();
	if (Meds.Num() == 0)
	{
		ShowNotification(FText::FromString(TEXT("MED-KIT EMPTY - USE THE BIOPRINTER")), 2.0f);
		return;
	}
	if (!Meds.Contains(SelectedMedication))
	{
		SelectedMedication = Meds[0];
	}

	const bool bHold = GetWorld()->GetRealTimeSeconds() - MedPressTime >= 0.45;
	if (bHold)
	{
		UseConsumable(SelectedMedication);
	}
	else
	{
		const int32 Index = Meds.IndexOfByKey(SelectedMedication);
		SelectedMedication = Meds[(Index + 1) % Meds.Num()];
		if (AudioManager)
		{
			AudioManager->PlayUIBeep(1.1f);
		}
	}
}

void AMarsCharacter::OnUseO2()
{
	if (!Inventory->HasItem(EMarsItemType::O2Canister))
	{
		ShowNotification(FText::FromString(TEXT("NO O2 CANISTER")), 2.0f);
		return;
	}
	UseConsumable(EMarsItemType::O2Canister);
}

// ----------------------------------------------------------------------------
// Equipment / consumables
// ----------------------------------------------------------------------------

void AMarsCharacter::EquipItem(EMarsItemType ItemType)
{
	if (bIsDead)
	{
		return;
	}
	if (EquippedItem && EquippedItem->ItemType == ItemType)
	{
		Holster();
		return;
	}
	if (!Inventory->HasItem(ItemType))
	{
		ShowNotification(FText::FromString(FString::Printf(TEXT("NO %s ON BELT"), *UMarsItemLibrary::GetItemName(ItemType).ToString().ToUpper())), 2.0f);
		return;
	}
	const TSubclassOf<AMarsEquipment>* ClassPtr = EquipmentClasses.Find(ItemType);
	if (!ClassPtr || !*ClassPtr)
	{
		return;
	}

	Holster();
	TObjectPtr<AMarsEquipment>* Existing = SpawnedEquipment.Find(ItemType);
	AMarsEquipment* Item = Existing ? Existing->Get() : nullptr;
	if (!Item)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Item = GetWorld()->SpawnActor<AMarsEquipment>(*ClassPtr, GetActorTransform(), Params);
		SpawnedEquipment.Add(ItemType, Item);
	}
	if (Item)
	{
		Item->OnEquipped(this);
		EquippedItem = Item;
		if (AudioManager)
		{
			AudioManager->PlayRatchet();
		}
	}
}

void AMarsCharacter::Holster()
{
	if (EquippedItem)
	{
		EquippedItem->OnUnequipped();
		EquippedItem = nullptr;
	}
}

bool AMarsCharacter::UseConsumable(EMarsItemType ItemType)
{
	if (bIsDead || !Inventory->HasItem(ItemType))
	{
		ShowNotification(FText::FromString(FString::Printf(TEXT("NO %s"), *UMarsItemLibrary::GetItemName(ItemType).ToString().ToUpper())), 2.0f);
		return false;
	}

	FString Message;
	switch (ItemType)
	{
	case EMarsItemType::O2Canister:
		if (Suit->IsSuitDonned())
		{
			if (Suit->RefillO2(O2CanisterLiters) <= 0.0f)
			{
				ShowNotification(FText::FromString(TEXT("SUIT O2 ALREADY FULL")), 2.0f);
				return false;
			}
			Message = TEXT("SUIT O2 TANK REFILLED");
		}
		else
		{
			Biometrics->AdministerSupplementalO2(0.75f);
			Message = TEXT("SUPPLEMENTAL O2 MASK (FiO2 60%)");
		}
		break;
	case EMarsItemType::MethyleneBlueCartridge: Biometrics->AdministerMethyleneBlue(); Message = TEXT("METHYLENE BLUE 1% IV INJECTED"); AMarsTutorialManager::Report(this, TEXT("MBInjected")); break;
	case EMarsItemType::IodinePills:            Biometrics->AdministerIodine(); Message = TEXT("POTASSIUM IODIDE TAKEN"); break;
	case EMarsItemType::NeuroAntitoxin:         Biometrics->AdministerNeuroAntitoxin(); Message = TEXT("NEURO-ANTITOXIN INJECTED"); break;
	case EMarsItemType::Amifostine:             Biometrics->AdministerAmifostine(); Message = TEXT("AMIFOSTINE AUTO-INJECTOR USED"); break;
	case EMarsItemType::WaterPouch:             Biometrics->Drink(0.35f); Message = TEXT("DRANK WATER POUCH"); break;
	case EMarsItemType::FoodRation:             Biometrics->Eat(0.4f); Message = TEXT("ATE A RATION"); break;
	case EMarsItemType::ChlorellaPaste:         Biometrics->Eat(0.2f); Biometrics->Drink(0.05f); Message = TEXT("ATE CHLORELLA PASTE (bitter)"); break;
	default:
		return false;
	}

	Inventory->RemoveItem(ItemType, 1);
	ShowNotification(FText::FromString(Message), 2.5f);
	if (AudioManager)
	{
		AudioManager->PlaySealantHiss();
	}
	return true;
}

void AMarsCharacter::OnDeployBeacon()
{
	if (bIsDead || !BeaconClass)
	{
		return;
	}
	if (CurrentModule.IsValid())
	{
		ShowNotification(FText::FromString(TEXT("BEACONS ARE FOR SURFACE USE")), 2.0f);
		return;
	}
	if (!Inventory->RemoveItem(EMarsItemType::SurfaceBeacon, 1))
	{
		ShowNotification(FText::FromString(TEXT("NO BEACONS LEFT")), 2.0f);
		return;
	}

	const FVector Location = GetActorLocation() + GetActorForwardVector() * 80.0f - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (ASurfaceBeacon* Beacon = GetWorld()->SpawnActor<ASurfaceBeacon>(BeaconClass, Location, FRotator::ZeroRotator, Params))
	{
		Beacon->BeaconLabel = FString::Printf(TEXT("BCN-%02d"), KnownBeacons.Num() + 1);
		RegisterBeacon(Beacon);
		ShowNotification(FText::FromString(FString::Printf(TEXT("BEACON %s DEPLOYED"), *Beacon->BeaconLabel)), 2.0f);
	}
}

void AMarsCharacter::RegisterBeacon(ASurfaceBeacon* Beacon)
{
	if (Beacon)
	{
		KnownBeacons.AddUnique(Beacon);
	}
}

TArray<ASurfaceBeacon*> AMarsCharacter::GetKnownBeacons() const
{
	TArray<ASurfaceBeacon*> Result;
	for (const TWeakObjectPtr<ASurfaceBeacon>& B : KnownBeacons)
	{
		if (B.IsValid())
		{
			Result.Add(B.Get());
		}
	}
	return Result;
}

void AMarsCharacter::OnTalkToAria()
{
	UAriaSubsystem* Aria = GetWorld()->GetSubsystem<UAriaSubsystem>();
	if (!Aria || bIsDead)
	{
		return;
	}

	// What the crew member *believes* is happening; A.R.I.A. only has sensor data to check it against.
	FString Statement = TEXT("A.R.I.A., status report.");
	if (HallucinationDirector && HallucinationDirector->HasRecentPhantom(25.0f))
	{
		Statement = FString::Printf(TEXT("A.R.I.A.! %s There is someone in here with me! Lock down the station!"), *HallucinationDirector->GetLastPhantomDescription());
	}
	else if (Biometrics->PanicLevel > 0.5f)
	{
		Statement = TEXT("Something is wrong with this place. I keep hearing things in the walls. Are we alone?");
	}

	ShowNotification(FText::FromString(FString::Printf(TEXT("> %s"), *Statement)), 5.0f);
	Aria->RequestReport(TEXT("CrewQuery"), Statement);
}

void AMarsCharacter::ShowNotification(const FText& Text, float DurationSeconds)
{
	FMarsNotification Notification;
	Notification.Text = Text;
	Notification.RemainingSeconds = DurationSeconds;
	Notifications.Add(Notification);
	if (Notifications.Num() > 5)
	{
		Notifications.RemoveAt(0);
	}
}

void AMarsCharacter::ShowPanel(const FString& Text, float DurationSeconds)
{
	PanelText = Text;
	PanelRemaining = DurationSeconds;
}

void AMarsCharacter::MarsEquip(int32 Slot)
{
	UseBeltSlot(Slot - 1);
}

void AMarsCharacter::MarsSetStage(int32 Stage)
{
	static const float MetHb[] = { 2.0f, 6.0f, 15.0f, 30.0f, 50.0f };
	static const float Injury[] = { 20.0f, 80.0f, 200.0f, 500.0f, 800.0f };
	static const float Bio[] = { 0.05f, 0.2f, 0.5f, 0.75f, 0.9f };
	const int32 S = FMath::Clamp(Stage, 0, 4);
	Biometrics->MethemoglobinPercentage = MetHb[S];
	Biometrics->RadiationInjuryMsv = Injury[S];
	Biometrics->ExtremophileBioLoad = Bio[S];
	Biometrics->PerchlorateBodyBurdenMg = S >= 2 ? 400.0f : 150.0f;
	ShowNotification(FText::FromString(FString::Printf(TEXT("[DEV] DISEASE STAGE -> %d"), S)), 2.0f);
}

void AMarsCharacter::MarsTurn(float Yaw)
{
	if (Controller)
	{
		Controller->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	}
}

void AMarsCharacter::MarsSpawnPhantom()
{
	const FVector Forward = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
	FVector Location = GetActorLocation() + Forward * 700.0f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DevPhantom), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Location, ECC_Visibility, Params))
	{
		Location = Hit.Location - Forward * 80.0f;
	}
	Location.Z = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AHallucinationPhantom* Phantom = GetWorld()->SpawnActor<AHallucinationPhantom>(AHallucinationPhantom::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams))
	{
		Phantom->InitPhantom(this, 0.8f);
	}
}

void AMarsCharacter::HandleStageChanged(EDiseaseStage OldStage, EDiseaseStage NewStage)
{
	if (NewStage > OldStage && AudioManager)
	{
		AudioManager->SpikeTinnitus(0.05f);
	}
}

void AMarsCharacter::HandleDeath(FName Cause)
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;
	Holster();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	if (AMarsGameMode* GameMode = GetWorld()->GetAuthGameMode<AMarsGameMode>())
	{
		GameMode->NotifyPlayerDied(Cause);
	}
}

void AMarsCharacter::OnCrouchToggle()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

// ----------------------------------------------------------------------------
// View feel: sensitivity, head-bob, weapon sway / breathing, recoil inertia
// ----------------------------------------------------------------------------

void AMarsCharacter::OnTurn(float Value)
{
	const FMarsUserSettings& S = FMarsUserSettings::Get();
	float V = Value * S.MouseSensitivity;
	if (S.bMouseSmoothing)
	{
		SmoothedLook.X = FMath::Lerp(SmoothedLook.X, V, 0.45f);
		V = SmoothedLook.X;
	}
	LookThisFrame.X = V;
	if (IsInspecting())
	{
		InspectActor->AddActorLocalRotation(FRotator(0.0f, -V * 4.0f, 0.0f));
		return;
	}
	AddControllerYawInput(V);
}

void AMarsCharacter::OnLookUp(float Value)
{
	const FMarsUserSettings& S = FMarsUserSettings::Get();
	float V = Value * S.MouseSensitivity * (S.bInvertY ? -1.0f : 1.0f);
	if (S.bMouseSmoothing)
	{
		SmoothedLook.Y = FMath::Lerp(SmoothedLook.Y, V, 0.45f);
		V = SmoothedLook.Y;
	}
	LookThisFrame.Y = V;
	if (IsInspecting())
	{
		InspectActor->AddActorLocalRotation(FRotator(0.0f, 0.0f, V * 4.0f));
		return;
	}
	AddControllerPitchInput(V);
}

void AMarsCharacter::AddRecoil(float PitchDegrees, float YawDegrees)
{
	AddControllerPitchInput(-PitchDegrees);
	AddControllerYawInput(YawDegrees);
	// Return ~55 % of the kick over the next few frames (muscle recovery), keep the rest (inertia).
	RecoilDebt += FVector2D(PitchDegrees, -YawDegrees) * 0.55f;
	ViewKick = FMath::Min(ViewKick + 1.0f, 1.6f);
}

void AMarsCharacter::UpdateViewFeel(float DeltaTime)
{
	const FMarsUserSettings& S = FMarsUserSettings::Get();

	// Recoil recovery.
	if (!RecoilDebt.IsNearlyZero(0.001f))
	{
		const FVector2D Step = RecoilDebt * FMath::Min(DeltaTime * 9.0f, 1.0f);
		RecoilDebt -= Step;
		AddControllerPitchInput(Step.X);
		AddControllerYawInput(Step.Y);
	}
	ViewKick = FMath::FInterpTo(ViewKick, 0.0f, DeltaTime, 12.0f);

	// Head-bob: low gravity = long, floaty strides.
	const float Speed2D = GetVelocity().Size2D();
	const bool bGrounded = !GetCharacterMovement()->IsFalling();
	const float MoveFrac = bGrounded ? FMath::Clamp(Speed2D / 400.0f, 0.0f, 1.3f) : 0.0f;
	BobBlend = FMath::FInterpTo(BobBlend, MoveFrac, DeltaTime, 6.0f);
	BobTime += DeltaTime * (5.0f + 3.0f * MoveFrac);
	BreathTime += DeltaTime * (0.9f + Biometrics->HeartRateBpm / 120.0f);
	const float BobAmp = 2.2f * S.HeadBob * BobBlend;
	const FVector Bob(0.0f, FMath::Sin(BobTime) * BobAmp * 0.6f, FMath::Abs(FMath::Sin(BobTime)) * BobAmp - BobAmp * 0.5f);

	// Eye height follows crouching (maintenance ducts are 1.35 m high).
	EyeHeight = FMath::FInterpTo(EyeHeight, bIsCrouched ? 30.0f : 68.0f, DeltaTime, 10.0f);
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, Bob.Y, EyeHeight + Bob.Z));

	// Weapon sway: lags behind the look direction, breathes, bobs with the stride, kicks back on fire.
	if (EquippedItem)
	{
		const float Sway = S.WeaponSway;
		const FRotator TargetRot(
			FMath::Clamp(LookThisFrame.Y * 1.6f, -5.0f, 5.0f) * Sway + FMath::Sin(BreathTime * 2.0f) * 0.6f * Sway + ViewKick * 4.0f,
			FMath::Clamp(-LookThisFrame.X * 1.6f, -5.0f, 5.0f) * Sway,
			FMath::Clamp(-LookThisFrame.X * 1.2f, -4.0f, 4.0f) * Sway);
		SwayRotation = FMath::RInterpTo(SwayRotation, TargetRot, DeltaTime, 7.0f);
		const FVector TargetLoc(
			-ViewKick * 3.5f,
			FMath::Sin(BobTime) * 0.6f * BobBlend * Sway,
			-FMath::Abs(FMath::Sin(BobTime)) * 0.5f * BobBlend * Sway + FMath::Sin(BreathTime * 2.0f) * 0.25f * Sway);
		SwayLocation = FMath::VInterpTo(SwayLocation, TargetLoc, DeltaTime, 14.0f);
		// Sway is an offset ON TOP of the hold pose (overwriting it parked the gun inside the lens).
		EquippedItem->SetActorRelativeLocation(EquippedItem->HeldOffset + SwayLocation);
		EquippedItem->SetActorRelativeRotation((FQuat(EquippedItem->HeldRotation) * FQuat(SwayRotation)).Rotator());
	}
	LookThisFrame = FVector2D::ZeroVector;

	if (InspectActor)
	{
		InspectActor->SetActorRelativeLocation(FVector(42.0f, 0.0f, -2.0f + FMath::Sin(BreathTime * 2.0f) * 0.3f));
	}
}

void AMarsCharacter::OnToggleHUD()
{
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (AMarsHUD* Hud = Cast<AMarsHUD>(PC->GetHUD()))
		{
			Hud->ToggleMinimalHUD();
		}
	}
}

void AMarsCharacter::OnPause()
{
	if (IsInspecting())
	{
		EndInspect();
		return;
	}
	if (AMarsGameMode* GM = GetWorld()->GetAuthGameMode<AMarsGameMode>())
	{
		GM->TogglePauseMenu();
	}
}

// ----------------------------------------------------------------------------
// Item inspection
// ----------------------------------------------------------------------------

void AMarsCharacter::BeginInspect(EMarsItemType ItemType, bool bForce)
{
	if (bIsDead || ItemType == EMarsItemType::None)
	{
		return;
	}
	InspectedTypes.Add(ItemType);
	if (!bForce && !FMarsUserSettings::Get().bInspectNewItems)
	{
		return;
	}
	EndInspect();

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* A = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
	if (!A)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("InspectRoot"));
	A->SetRootComponent(Root);
	Root->RegisterComponent();
	USceneComponent* Model = NewObject<USceneComponent>(A, TEXT("InspectModel"));
	Model->SetupAttachment(Root);
	Model->RegisterComponent();
	AMarsPropBuilder::BuildProp(Model, AMarsPropBuilder::GetPropForItem(ItemType), 1.0f, false, 0.35f);

	// Fit into ~16 cm and centre it on the pivot.
	A->AttachToComponent(FirstPersonCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	const FBox Bounds = A->GetComponentsBoundingBox(true);
	if (Bounds.IsValid)
	{
		const float Extent = FMath::Max(Bounds.GetExtent().GetMax(), 1.0f);
		const float Scale = FMath::Clamp(8.0f / Extent, 0.15f, 3.0f);
		const FVector LocalCenter = Root->GetComponentTransform().InverseTransformPosition(Bounds.GetCenter());
		Model->SetRelativeScale3D(FVector(Scale));
		Model->SetRelativeLocation(-LocalCenter * Scale);
	}
	A->SetActorRelativeRotation(FRotator(0.0f, 150.0f, 0.0f));
	for (UActorComponent* C : A->GetComponents())
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(C))
		{
			Prim->SetCastShadow(false);
		}
	}
	InspectActor = A;
	InspectItem = ItemType;
	if (EquippedItem)
	{
		EquippedItem->SetActorHiddenInGame(true);
	}
}

void AMarsCharacter::EndInspect()
{
	if (InspectActor)
	{
		InspectActor->Destroy();
		InspectActor = nullptr;
		if (AudioManager)
		{
			AudioManager->PlayUIBeep(1.1f);
		}
		if (EquippedItem)
		{
			EquippedItem->SetActorHiddenInGame(false);
		}
	}
	InspectItem = EMarsItemType::None;
}

void AMarsCharacter::OnWristDisplay()
{
	if (SuitViewmodel && Suit && Suit->IsSuitDonned())
	{
		SuitViewmodel->ToggleRaised();
		if (AudioManager)
		{
			AudioManager->PlayUIBeep(SuitViewmodel->IsRaised() ? 1.4f : 1.0f);
		}
	}
}

// ----------------------------------------------------------------------------
// Sleep: fade out (1.5 s) -> skip to morning -> fade in (2.5 s)
// ----------------------------------------------------------------------------

void AMarsCharacter::BeginSleep()
{
	BeginRest(7.0f, true);
}

void AMarsCharacter::BeginRest(float TargetHour, bool bSleep)
{
	if (IsSleeping() || bIsDead)
	{
		return;
	}
	RestTargetHour = TargetHour;
	bRestIsSleep = bSleep;
	SleepTime = 0.0f;
	bSleepApplied = false;
	Holster();
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}
}

float AMarsCharacter::GetSleepFade() const
{
	if (!IsSleeping())
	{
		return 0.0f;
	}
	if (SleepTime < 1.5f)
	{
		return SleepTime / 1.5f;
	}
	if (SleepTime < 3.5f)
	{
		return 1.0f;
	}
	return FMath::Clamp(1.0f - (SleepTime - 3.5f) / 2.5f, 0.0f, 1.0f);
}

void AMarsCharacter::UpdateSleep(float DeltaTime)
{
	if (!IsSleeping())
	{
		return;
	}
	SleepTime += DeltaTime;
	if (!bSleepApplied && SleepTime >= 1.6f)
	{
		bSleepApplied = true;
		if (Fatigue)
		{
			const float Hours = bRestIsSleep && FMath::IsNearlyEqual(RestTargetHour, 7.0f) ? Fatigue->GetHoursUntilMorning() : Fatigue->GetHoursUntil(RestTargetHour);
			Fatigue->PerformRest(Hours, bRestIsSleep);
		}
		if (AMarsGameMode* GM = GetWorld()->GetAuthGameMode<AMarsGameMode>())
		{
			GM->RequestCheckpoint(bRestIsSleep ? TEXT("NEW SOL") : TEXT("REST"));
		}
	}
	if (SleepTime >= 6.0f)
	{
		SleepTime = -1.0f;
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
		}
	}
}

void AMarsCharacter::OnToggleWaypoints()
{
	if (UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this))
	{
		Story->ToggleWaypoints();
		ShowNotification(FText::FromString(Story->AreWaypointsEnabled() ? TEXT("NAV WAYPOINTS ON") : TEXT("NAV WAYPOINTS OFF")), 1.5f);
	}
}

bool AMarsCharacter::IsLampOn() const
{
	return HelmetLamp && HelmetLamp->IsVisible();
}

void AMarsCharacter::OnToggleLamp()
{
	if (HelmetLamp)
	{
		HelmetLamp->SetVisibility(!HelmetLamp->IsVisible());
		if (AudioManager)
		{
			AudioManager->PlayUIBeep(HelmetLamp->IsVisible() ? 1.5f : 1.0f);
		}
	}
}

// ----------------------------------------------------------------------------
// Inventory screen actions
// ----------------------------------------------------------------------------

namespace
{
	bool IsConsumableItem(EMarsItemType T)
	{
		switch (T)
		{
		case EMarsItemType::O2Canister:
		case EMarsItemType::MethyleneBlueCartridge:
		case EMarsItemType::IodinePills:
		case EMarsItemType::NeuroAntitoxin:
		case EMarsItemType::Amifostine:
		case EMarsItemType::WaterPouch:
		case EMarsItemType::FoodRation:
		case EMarsItemType::ChlorellaPaste:
			return true;
		default:
			return false;
		}
	}
}

FString AMarsCharacter::GetInventoryUseLabel(EMarsItemType ItemType) const
{
	switch (ItemType)
	{
	case EMarsItemType::O2Canister:             return Suit && Suit->IsSuitDonned() ? TEXT("REFILL SUIT O2") : TEXT("BREATHE FROM O2 MASK");
	case EMarsItemType::WaterPouch:             return TEXT("DRINK");
	case EMarsItemType::FoodRation:
	case EMarsItemType::ChlorellaPaste:         return TEXT("EAT");
	case EMarsItemType::MethyleneBlueCartridge:
	case EMarsItemType::NeuroAntitoxin:         return TEXT("INJECT");
	case EMarsItemType::Amifostine:             return TEXT("USE AUTO-INJECTOR");
	case EMarsItemType::IodinePills:            return TEXT("TAKE PILLS");
	default:                                    return TEXT("TAKE IN HAND / EXAMINE");
	}
}

EMarsInventoryUse AMarsCharacter::UseInventoryItem(EMarsItemType ItemType)
{
	if (bIsDead || !Inventory || !Inventory->HasItem(ItemType))
	{
		return EMarsInventoryUse::Failed;
	}
	if (UMarsItemLibrary::IsEquippable(ItemType))
	{
		EndInspect();
		EquipItem(ItemType);
		return EMarsInventoryUse::Equipped;
	}
	if (IsConsumableItem(ItemType))
	{
		if (ItemType == EMarsItemType::MethyleneBlueCartridge || ItemType == EMarsItemType::IodinePills
			|| ItemType == EMarsItemType::NeuroAntitoxin || ItemType == EMarsItemType::Amifostine)
		{
			SelectedMedication = ItemType;
		}
		return UseConsumable(ItemType) ? EMarsInventoryUse::Consumed : EMarsInventoryUse::Failed;
	}
	// Parts, samples, transistors, cultures, beacons: hold it up and turn it over in your hands.
	Holster();
	BeginInspect(ItemType, true);
	return IsInspecting() ? EMarsInventoryUse::Held : EMarsInventoryUse::Failed;
}

bool AMarsCharacter::DropInventoryItem(EMarsItemType ItemType)
{
	if (bIsDead || !Inventory || !Inventory->HasItem(ItemType))
	{
		return false;
	}
	FVector Where = GetActorLocation() + GetActorForwardVector() * 90.0f;
	FHitResult Floor;
	if (GetWorld()->LineTraceSingleByChannel(Floor, Where, Where - FVector(0, 0, 400.0f), ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(DropItem), false, this)))
	{
		Where = Floor.ImpactPoint + FVector(0, 0, 15.0f);
	}
	const FTransform Xf(FRotator(0.0f, GetActorRotation().Yaw, 0.0f), Where);
	AMarsPickup* Pickup = GetWorld()->SpawnActorDeferred<AMarsPickup>(AMarsPickup::StaticClass(), Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return false;
	}
	Pickup->ItemType = ItemType;
	Pickup->Count = 1;
	Pickup->FinishSpawning(Xf);
	Inventory->RemoveItem(ItemType, 1);
	if (EquippedItem && !Inventory->HasItem(ItemType) && UMarsItemLibrary::IsEquippable(ItemType))
	{
		Holster();
	}
	return true;
}

void AMarsCharacter::AssignBeltSlot(int32 Index, EMarsItemType ItemType)
{
	if (!BeltSlots.IsValidIndex(Index))
	{
		return;
	}
	FString Short = UMarsItemLibrary::GetItemName(ItemType).ToString().ToUpper();
	Short.ReplaceInline(TEXT(" "), TEXT(""));
	BeltSlots[Index].ItemType = ItemType;
	BeltSlots[Index].ShortName = Short.Left(5);
	BeltSlots[Index].bShowCount = !UMarsItemLibrary::IsEquippable(ItemType);
}

void AMarsCharacter::OnInventory()
{
	if (AMarsGameMode* GM = GetWorld()->GetAuthGameMode<AMarsGameMode>())
	{
		GM->ToggleInventory();
	}
}
