#include "Core/MarsTutorialManager.h"

#include "Audio/MarsAudioManager.h"
#include "Base/MarsBaseModule.h"
#include "Equipment/MarsEquipment.h"
#include "Player/EVASuitComponent.h"
#include "Player/MarsCharacter.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AMarsTutorialManager::AMarsTutorialManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
}

AMarsTutorialManager* AMarsTutorialManager::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AMarsTutorialManager> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AMarsTutorialManager::Report(const UObject* WorldContext, FName Event)
{
	if (AMarsTutorialManager* Tutorial = Get(WorldContext))
	{
		Tutorial->HandleEvent(Event);
	}
}

void AMarsTutorialManager::BuildSteps()
{
	auto Task = [](const TCHAR* Text, const TCHAR* Event, bool bOptional = false)
	{
		FMarsTutorialTask T;
		T.Text = Text;
		T.Event = Event;
		T.bOptional = bOptional;
		return T;
	};

	Steps.Reset();
	{
		FMarsTutorialStep S;
		S.Title = TEXT("PRESSURE & SUIT");
		S.Icon = 0;
		S.Science = TEXT("The habitat holds 101 kPa of air. Outside: 0.6 kPa - below the Armstrong limit, body fluids boil.");
		S.Tasks = { Task(TEXT("Walk through the inner hatch into the ring (W)"), TEXT("EnteredHabitat")),
		            Task(TEXT("Stow your MCP suit at the EVA Prep rack (E)"), TEXT("SuitStowed")) };
		Steps.Add(S);
	}
	{
		FMarsTutorialStep S;
		S.Title = TEXT("RADIATION");
		S.Icon = 1;
		S.Science = TEXT("Clicks speed up exponentially with dose rate. Hotspots: solar storms and the lander's leaking RTG.");
		S.Tasks = { Task(TEXT("Hold the Geiger dosimeter (press 4)"), TEXT("DosimeterChecked")) };
		Steps.Add(S);
	}
	{
		FMarsTutorialStep S;
		S.Title = TEXT("MED-BAY");
		S.Icon = 2;
		S.Science = TEXT("Perchlorate turns haemoglobin into methaemoglobin - blood goes brown and cannot carry O2.");
		S.Tasks = { Task(TEXT("Scan yourself on the Med-Bay scanner bed (E)"), TEXT("ScannerUsed")),
		            Task(TEXT("Print Methylene Blue at the bioprinter (hold E)"), TEXT("MBPrinted"), true),
		            Task(TEXT("Inject it from your med-kit (hold H)"), TEXT("MBInjected")) };
		Steps.Add(S);
	}
	{
		FMarsTutorialStep S;
		S.Title = TEXT("COLD SEALS");
		S.Icon = 3;
		S.Science = TEXT("At -80 C night, rubber seals shrink and leak. Re-torque the flanges before it gets that cold.");
		S.Tasks = { Task(TEXT("Torque a bulkhead flange (press 3, hold LMB)"), TEXT("SealsTorqued")),
		            Task(TEXT("Patch a hull puncture if you find one (press 2)"), TEXT("HullPatched"), true) };
		Steps.Add(S);
	}
}

void AMarsTutorialManager::StartTutorial()
{
	BuildSteps();
	StepIndex = 0;
	bActive = true;
}

void AMarsTutorialManager::SkipTutorial()
{
	bActive = false;
}

void AMarsTutorialManager::HandleEvent(FName Event)
{
	if (!bActive || !Steps.IsValidIndex(StepIndex))
	{
		return;
	}
	bool bChanged = false;
	for (FMarsTutorialTask& T : Steps[StepIndex].Tasks)
	{
		if (!T.bDone && T.Event == Event)
		{
			T.bDone = true;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		if (const AMarsCharacter* C = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (UMarsAudioManager* Audio = C->GetAudioManager())
			{
				Audio->PlayUIBeep(1.8f);
			}
		}
		AdvanceIfDone();
	}
}

void AMarsTutorialManager::AdvanceIfDone()
{
	for (const FMarsTutorialTask& T : Steps[StepIndex].Tasks)
	{
		if (!T.bDone && !T.bOptional)
		{
			return;
		}
	}
	CompleteFlash = 1.0f;
	NextStepDelay = 2.0f;
}

void AMarsTutorialManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CompleteFlash = FMath::Max(CompleteFlash - DeltaTime * 0.6f, 0.0f);
	if (!bActive)
	{
		return;
	}

	if (NextStepDelay > 0.0f)
	{
		NextStepDelay -= DeltaTime;
		if (NextStepDelay <= 0.0f)
		{
			++StepIndex;
			if (!Steps.IsValidIndex(StepIndex))
			{
				bActive = false;
				if (AMarsCharacter* C = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
				{
					C->ShowNotification(FText::FromString(TEXT("SUIT TRAINING COMPLETE")), 4.0f);
				}
			}
		}
		return;
	}

	// Polled conditions.
	const AMarsCharacter* C = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!C)
	{
		return;
	}
	const AMarsBaseModule* Module = C->GetCurrentModule();
	if (Module && Module->ModuleType != EMarsModuleType::AirlockDecon)
	{
		HandleEvent(TEXT("EnteredHabitat"));
	}
	if (C->GetSuit() && !C->GetSuit()->IsSuitDonned() && Module && Module->ModuleType != EMarsModuleType::AirlockDecon)
	{
		HandleEvent(TEXT("SuitStowed"));
	}
	const AMarsEquipment* Held = C->GetEquippedItem();
	DosimeterHeldSeconds = Held && Held->ItemType == EMarsItemType::RadiationDosimeter ? DosimeterHeldSeconds + DeltaTime : 0.0f;
	if (DosimeterHeldSeconds > 2.0f)
	{
		HandleEvent(TEXT("DosimeterChecked"));
	}
}
