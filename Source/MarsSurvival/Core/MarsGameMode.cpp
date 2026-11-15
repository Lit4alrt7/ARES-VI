#include "Core/MarsGameMode.h"

#include "MarsSurvival.h"
#include "AI/AriaSubsystem.h"
#include "Core/MarsHUD.h"
#include "Core/MarsSaveGame.h"
#include "UI/MarsInventoryWidget.h"
#include "Base/MarsBaseModule.h"
#include "Core/MarsNarrativeDirector.h"
#include "World/MarsWaypointTrail.h"
#include "Player/PlayerBiometricsComponent.h"
#include "Core/MarsUserSettings.h"
#include "Player/MarsCharacter.h"
#include "UI/IntroBriefingWidget.h"
#include "UI/SplashIntroWidget.h"
#include "Core/MarsTutorialManager.h"
#include "Audio/MarsAudioManager.h"
#include "World/MarsShuttle.h"
#include "EngineUtils.h"
#include "World/MarsStationSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AMarsGameMode::AMarsGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AMarsCharacter::StaticClass();
	HUDClass = AMarsHUD::StaticClass();
}

void AMarsGameMode::BeginPlay()
{
	Super::BeginPlay();
	RemainingSeconds = CommWindowMinutes * 60.0f;

	if (UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>())
	{
		Station->OnEarthContactEstablished.AddDynamic(this, &AMarsGameMode::HandleEarthContact);
	}

	// Dev capture: "-AutoShot=8,30" grabs in-engine screenshots (UI included) at those real-time seconds.
	FString ShotList;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoShot="), ShotList, false))
	{
		TArray<FString> Parts;
		ShotList.ParseIntoArray(Parts, TEXT(","));
		TSharedRef<TArray<float>> Times = MakeShared<TArray<float>>();
		for (const FString& P : Parts)
		{
			Times->Add(FCString::Atof(*P));
		}
		TSharedRef<float> Elapsed = MakeShared<float>(0.0f);
		TSharedRef<int32> NextIndex = MakeShared<int32>(0);
		const bool bSpin = FParse::Param(FCommandLine::Get(), TEXT("AutoSpin"));
		TWeakObjectPtr<UWorld> WeakWorld = GetWorld();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Times, Elapsed, NextIndex, bSpin, WeakWorld](float Dt)
		{
			*Elapsed += Dt;
			if (Times->IsValidIndex(*NextIndex) && *Elapsed >= (*Times)[*NextIndex])
			{
				// Saved/Screenshots/<Platform>/ScreenShotNNNNN.png (UI included).
				if (WeakWorld.IsValid())
				{
					if (APlayerController* ShotPC = WeakWorld->GetFirstPlayerController())
					{
						ShotPC->ConsoleCommand(TEXT("shot showui"), false);
					}
					UE_LOG(LogMarsSurvival, Log, TEXT("AutoShot %d at %.1f s"), *NextIndex, *Elapsed);
				}
				++(*NextIndex);
				// "-AutoSpin": turn 90 degrees after each capture to survey the room.
				if (bSpin && WeakWorld.IsValid())
				{
					if (APlayerController* PC = WeakWorld->GetFirstPlayerController())
					{
						PC->SetControlRotation(FRotator(0.0f, PC->GetControlRotation().Yaw + 90.0f, 0.0f));
					}
				}
			}
			return Times->IsValidIndex(*NextIndex);
		}));
	}

	// Dev capture: "-AutoInventoryAt=8" opens the inventory screen after N seconds.
	float AutoInvAt = -1.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoInventoryAt="), AutoInvAt) && AutoInvAt > 0.0f)
	{
		FTimerHandle InvHandle;
		GetWorldTimerManager().SetTimer(InvHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { ToggleInventory(); }), AutoInvAt, false);
	}

	// Dev capture: "-AutoPauseAt=12" opens the pause menu after N real seconds.
	float AutoPauseAt = -1.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoPauseAt="), AutoPauseAt) && AutoPauseAt > 0.0f)
	{
		FTimerHandle PauseHandle;
		GetWorldTimerManager().SetTimer(PauseHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { ShowPauseMenu(bMissionOver); }), AutoPauseAt, false);
	}

	// Dev capture: "-StartAt=MED" teleports the player into the first module whose name contains the text.
	FString StartAt;
	if (FParse::Value(FCommandLine::Get(), TEXT("StartAt="), StartAt))
	{
		FTimerHandle StartHandle;
		GetWorldTimerManager().SetTimer(StartHandle, FTimerDelegate::CreateWeakLambda(this, [this, StartAt]()
		{
			APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
			for (TActorIterator<AMarsBaseModule> It(GetWorld()); It && Pawn; ++It)
			{
				if (It->GetModuleName().Contains(StartAt))
				{
					Pawn->SetActorLocation(It->GetActorTransform().TransformPosition(FVector(-It->RoomSize.X * 0.35f, 0.0f, 100.0f)), false, nullptr, ETeleportType::TeleportPhysics);
					if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
					{
						PC->SetControlRotation(FRotator(-5.0f, It->GetActorRotation().Yaw, 0.0f));
					}
					break;
				}
			}
		}), 1.5f, false);
	}

	// Level reloads from the pause menu: "?MarsLoad=<slot>" restores a save, "?MarsRestart" skips the intro.
	const FString LoadSlot = UGameplayStatics::ParseOption(OptionsString, TEXT("MarsLoad"));
	if (!LoadSlot.IsEmpty())
	{
		FTimerDelegate Load = FTimerDelegate::CreateWeakLambda(this, [this, LoadSlot]() { ApplySaveSlot(LoadSlot); });
		GetWorldTimerManager().SetTimerForNextTick(Load);
		return;
	}
	if (UGameplayStatics::HasOption(OptionsString, TEXT("MarsRestart")))
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &AMarsGameMode::StartOpeningSequence);
		return;
	}

	// "-NoBriefing" on the command line skips the intro (handy while iterating).
	if (bShowIntroBriefing && !FParse::Param(FCommandLine::Get(), TEXT("NoBriefing")))
	{
		// Next tick: the player controller, pawn and HUD exist by then.
		GetWorldTimerManager().SetTimerForNextTick(this, &AMarsGameMode::ShowSplash);
	}
	else
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &AMarsGameMode::StartOpeningSequence);
	}
}

void AMarsGameMode::ShowBriefing()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	Briefing = CreateWidget<UIntroBriefingWidget>(PC, UIntroBriefingWidget::StaticClass());
	if (!Briefing)
	{
		return;
	}
	Briefing->OnFinished.AddUObject(this, &AMarsGameMode::FinishBriefing);
	Briefing->AddToViewport(100);

	if (AMarsHUD* Hud = Cast<AMarsHUD>(PC->GetHUD()))
	{
		Hud->SetHUDVisible(false);
	}

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Briefing->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = false;
	UGameplayStatics::SetGamePaused(this, true);
}

void AMarsGameMode::FinishBriefing()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (Briefing)
	{
		Briefing->RemoveFromParent();
		Briefing = nullptr;
	}
	UGameplayStatics::SetGamePaused(this, false);
	if (PC)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		if (AMarsHUD* Hud = Cast<AMarsHUD>(PC->GetHUD()))
		{
			Hud->SetHUDVisible(true);
		}
	}
	if (AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->ShowNotification(FText::FromString(TEXT("SUIT TELEMETRY INITIALISED")), 3.0f);
		if (UMarsAudioManager* Audio = Player->GetAudioManager())
		{
			Audio->StopMusic(6.0f);
		}
	}
	StartOpeningSequence();
}

void AMarsGameMode::ShowSplash()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	Splash = CreateWidget<USplashIntroWidget>(PC, USplashIntroWidget::StaticClass());
	if (!Splash)
	{
		ShowBriefing();
		return;
	}
	Splash->OnStartMission.AddLambda([this]()
	{
		if (Splash)
		{
			Splash->RemoveFromParent();
			Splash = nullptr;
		}
		ShowBriefing();
	});
	Splash->OnContinueMission.AddLambda([this]()
	{
		if (Splash)
		{
			Splash->RemoveFromParent();
			Splash = nullptr;
		}
		ApplySaveSlot(UMarsSaveGame::MostRecentSlot());
	});
	Splash->AddToViewport(110);
	if (AMarsHUD* Hud = Cast<AMarsHUD>(PC->GetHUD()))
	{
		Hud->SetHUDVisible(false);
	}
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Splash->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
	UGameplayStatics::SetGamePaused(this, true);
}

void AMarsGameMode::StartOpeningSequence()
{
	// Stranded: the shuttle outside the airlock viewport lifts off into the storm.
	for (TActorIterator<AMarsShuttle> It(GetWorld()); It; ++It)
	{
		It->OnDeparted.AddUObject(this, &AMarsGameMode::HandleShuttleDeparted);
		It->StartDeparture();
		break;
	}

	FActorSpawnParameters Params;
	Tutorial = GetWorld()->SpawnActor<AMarsTutorialManager>(AMarsTutorialManager::StaticClass(), FTransform::Identity, Params);
	if (Tutorial)
	{
		Tutorial->StartTutorial();
	}
	if (UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this))
	{
		Story->StartStory();
	}
	GetWorld()->SpawnActor<AMarsWaypointTrail>(AMarsWaypointTrail::StaticClass(), FTransform::Identity);
}

void AMarsGameMode::HandleShuttleDeparted()
{
	if (UAriaSubsystem* Aria = GetWorld()->GetSubsystem<UAriaSubsystem>())
	{
		Aria->SayScripted(TEXT("A.R.I.A.: Supply shuttle away. It is just us now. Sol one, morning, weather calm. Airlock pressure nominal - stow your suit, settle in, and learn the station. There is no hurry today."));
	}
	// First checkpoint a few seconds after the shuttle is gone.
	CheckpointTimer = FMath::Max(CheckpointIntervalSeconds - 8.0f, 0.0f);
}

void AMarsGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bMissionOver)
	{
		// Let the end card breathe, then offer load / restart / quit.
		if (EndMenuDelay > 0.0f)
		{
			EndMenuDelay -= DeltaSeconds;
			if (EndMenuDelay <= 0.0f && !PauseMenu)
			{
				ShowPauseMenu(true);
			}
		}
		return;
	}
	MissionElapsed += DeltaSeconds;

	// Automatic checkpoints: only inside a pressurised module, never during Stage 4 collapse.
	CheckpointTimer += DeltaSeconds;
	if (CheckpointTimer >= CheckpointIntervalSeconds)
	{
		const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		if (Player && !Player->IsDead() && Player->GetCurrentModule() && Player->GetBiometrics() && Player->GetBiometrics()->GetStageIndex() < 4)
		{
			SaveCheckpoint(TEXT("AUTO CHECKPOINT"));
			return;
		}
		CheckpointTimer = CheckpointIntervalSeconds - 15.0f; // could not save here: retry soon if we could not save now
	}

	if (CommWindowMinutes <= 0.0f)
	{
		return;
	}
	RemainingSeconds = FMath::Max(RemainingSeconds - DeltaSeconds, 0.0f);
	if (RemainingSeconds <= 0.0f)
	{
		EndMission(false, TEXT("COMM WINDOW CLOSED - SOLAR CONJUNCTION. NO CONTACT WITH EARTH FOR 14 SOLS."));
	}
}

void AMarsGameMode::HandleEarthContact()
{
	EndMission(true, TEXT("QUANTUM LINK ESTABLISHED. EARTH ACKNOWLEDGES. RESCUE ASSET DISPATCHED."));
}

void AMarsGameMode::NotifyPlayerDied(FName Cause)
{
	EndMission(false, FString::Printf(TEXT("CREW MEMBER DECEASED. CAUSE: %s"), *Cause.ToString().ToUpper()));
}

void AMarsGameMode::EndMission(bool bInVictory, const FString& Reason)
{
	if (bMissionOver)
	{
		return;
	}
	bMissionOver = true;
	bVictory = bInVictory;
	EndReason = Reason;
	UE_LOG(LogMarsSurvival, Log, TEXT("MISSION %s: %s"), bVictory ? TEXT("SUCCESS") : TEXT("FAILURE"), *Reason);
	OnMissionEnded.Broadcast(bVictory, Reason);
	EndMenuDelay = 6.0f;
}

// ----------------------------------------------------------------------------
// Pause menu, save / load
// ----------------------------------------------------------------------------

void AMarsGameMode::ToggleInventory()
{
	if (Splash || Briefing || PauseMenu || bMissionOver)
	{
		return;
	}
	if (InventoryScreen)
	{
		CloseInventory();
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !Cast<AMarsCharacter>(PC->GetPawn()))
	{
		return;
	}
	InventoryScreen = CreateWidget<UMarsInventoryWidget>(PC, UMarsInventoryWidget::StaticClass());
	if (!InventoryScreen)
	{
		return;
	}
	InventoryScreen->OnClosed.AddUObject(this, &AMarsGameMode::CloseInventory);
	InventoryScreen->AddToViewport(115);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(InventoryScreen->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
	UGameplayStatics::SetGamePaused(this, true);
}

void AMarsGameMode::CloseInventory()
{
	if (InventoryScreen)
	{
		InventoryScreen->RemoveFromParent();
		InventoryScreen = nullptr;
	}
	UGameplayStatics::SetGamePaused(this, false);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void AMarsGameMode::TogglePauseMenu()
{
	if (Splash || Briefing)
	{
		return;
	}
	if (InventoryScreen)
	{
		CloseInventory();
		return;
	}
	if (PauseMenu)
	{
		if (!bMissionOver)
		{
			HidePauseMenu();
		}
		return;
	}
	ShowPauseMenu(bMissionOver);
}

void AMarsGameMode::ShowPauseMenu(bool bMissionOverMode)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || PauseMenu)
	{
		return;
	}
	PauseMenu = CreateWidget<UMarsPauseMenuWidget>(PC, UMarsPauseMenuWidget::StaticClass());
	if (!PauseMenu)
	{
		return;
	}
	PauseMenu->Setup(bMissionOverMode);
	PauseMenu->OnAction.AddUObject(this, &AMarsGameMode::HandlePauseAction);
	PauseMenu->AddToViewport(120);
	if (AMarsHUD* Hud = Cast<AMarsHUD>(PC->GetHUD()))
	{
		Hud->SetHUDVisible(false);
	}

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(PauseMenu->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
	UGameplayStatics::SetGamePaused(this, true);
}

void AMarsGameMode::HidePauseMenu()
{
	if (PauseMenu)
	{
		PauseMenu->RemoveFromParent();
		PauseMenu = nullptr;
	}
	UGameplayStatics::SetGamePaused(this, false);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		if (AMarsHUD* Hud = Cast<AMarsHUD>(PC->GetHUD()))
		{
			Hud->SetHUDVisible(true);
		}
	}
	FMarsUserSettings::Get().Apply(GetWorld());
}

void AMarsGameMode::ReloadLevel(const FString& Options)
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, Options);
}

void AMarsGameMode::HandlePauseAction(EMarsPauseAction Action)
{
	switch (Action)
	{
	case EMarsPauseAction::Resume:
		HidePauseMenu();
		break;
	case EMarsPauseAction::LoadLastSave:
	{
		const FString Slot = UMarsSaveGame::MostRecentSlot();
		if (!Slot.IsEmpty())
		{
			ReloadLevel(FString::Printf(TEXT("MarsLoad=%s"), *Slot));
		}
		break;
	}
	case EMarsPauseAction::Restart:
		ReloadLevel(TEXT("MarsRestart"));
		break;
	case EMarsPauseAction::QuitToMenu:
		ReloadLevel(FString());
		break;
	case EMarsPauseAction::QuitToDesktop:
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
		break;
	}
}

void AMarsGameMode::SaveCheckpoint(const FString& Label)
{
	CheckpointTimer = 0.0f;
	if (UMarsSaveGame::SaveWorld(GetWorld(), UMarsSaveGame::CheckpointSlot, Label))
	{
		if (AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Player->ShowNotification(FText::FromString(TEXT("CHECKPOINT SAVED")), 2.0f);
		}
	}
}

void AMarsGameMode::ApplySaveSlot(const FString& Slot)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const UMarsSaveGame* Save = UMarsSaveGame::LoadSlot(Slot);
	if (!Save || !PC)
	{
		UE_LOG(LogMarsSurvival, Warning, TEXT("No save in slot '%s' - starting a new mission."), *Slot);
		UGameplayStatics::SetGamePaused(this, false);
		if (PC)
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
		StartOpeningSequence();
		return;
	}

	// The shuttle left long ago.
	TArray<AMarsShuttle*> Shuttles;
	for (TActorIterator<AMarsShuttle> It(GetWorld()); It; ++It)
	{
		Shuttles.Add(*It);
	}
	for (AMarsShuttle* S : Shuttles)
	{
		S->Destroy();
	}

	Save->ApplyToWorld(GetWorld());
	GetWorld()->SpawnActor<AMarsWaypointTrail>(AMarsWaypointTrail::StaticClass(), FTransform::Identity);
	RemainingSeconds = Save->RemainingSeconds > 0.0f ? Save->RemainingSeconds : RemainingSeconds;
	CheckpointTimer = 0.0f;

	UGameplayStatics::SetGamePaused(this, false);
	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
	if (AMarsHUD* Hud = Cast<AMarsHUD>(PC->GetHUD()))
	{
		Hud->SetHUDVisible(true);
	}
	if (AMarsCharacter* Player = Cast<AMarsCharacter>(PC->GetPawn()))
	{
		if (UMarsAudioManager* Audio = Player->GetAudioManager())
		{
			Audio->StopMusic(2.0f);
		}
		Player->ShowNotification(FText::FromString(FString::Printf(TEXT("MISSION LOG RESTORED  //  %s"), *Save->Label)), 3.5f);
	}
}
