#include "Core/MarsNarrativeDirector.h"

#include "MarsSurvival.h"
#include "AI/AriaSubsystem.h"
#include "Base/CommRelayModule.h"
#include "Core/MarsTutorialManager.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "Stations/MarsStations.h"
#include "Stations/MarsStoryStations.h"
#include "Vehicles/MarsReconShuttle.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsStationSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	ACommRelayModule* FindComm(UWorld* World)
	{
		for (TActorIterator<ACommRelayModule> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

UMarsNarrativeDirector* UMarsNarrativeDirector::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMarsNarrativeDirector>() : nullptr;
}

void UMarsNarrativeDirector::Report(const UObject* WorldContext, FName Event)
{
	UMarsNarrativeDirector* D = Get(WorldContext);
	if (!D || !D->bStarted || !D->Steps.IsValidIndex(D->StepIndex))
	{
		return;
	}
	if (D->Steps[D->StepIndex].Event == Event)
	{
		D->Advance();
	}
}

void UMarsNarrativeDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildSteps();
}

void UMarsNarrativeDirector::BuildSteps()
{
	Steps.Reset();
	auto Step = [this](EMarsChapter Ch, const TCHAR* Text, FName Event, UClass* Target, FName Tag, const TCHAR* Aria, TFunction<bool(UWorld*)> Poll = nullptr)
	{
		FMarsStoryStep S;
		S.Chapter = Ch;
		S.Text = Text;
		S.Event = Event;
		S.TargetClass = Target;
		S.TargetTag = Tag;
		S.AriaOnComplete = Aria;
		S.Poll = MoveTemp(Poll);
		Steps.Add(MoveTemp(S));
	};
	using C = EMarsChapter;

	// ---------------- Chapter 1: Sol 1-2, stabilisation ----------------
	Step(C::Stabilization, TEXT("Complete SV-4 suit training (follow the training card)."), TEXT("TutorialComplete"), nullptr, NAME_None,
		TEXT("A.R.I.A.: Suit calibration logged. Habitat nominal. Take your time today - nothing out there is in a hurry."),
		[](UWorld* W) { const AMarsTutorialManager* T = AMarsTutorialManager::Get(W); return !T || !T->IsActive(); });
	Step(C::Stabilization, TEXT("Stock up: open the supply lockers in SEC-06 Storage & Comm."), TEXT("Locker_Supply"), AMarsStorageLocker::StaticClass(), TEXT("Supply"),
		TEXT("A.R.I.A.: Inventory reconciled. Rations for forty sols. One quantum transistor - the other two went down with the relay program."));
	Step(C::Stabilization, TEXT("Run a diagnostic at the Comm-Relay terminal (SEC-06)."), TEXT("CommTerminalUsed"), ACommRelayTerminal::StaticClass(), NAME_None,
		TEXT("A.R.I.A.: Diagnostic complete. Uplink dark - but I am logging a repeating black-box ping on 401.5 MHz. Source: orbital relay R-3. R-3 was reported de-orbited on Sol 97."));
	Step(C::Stabilization, TEXT("Sol 1 is ending. Rest in your bunk (SEC-07 Crew Quarters)."), TEXT("Slept"), AMarsBunkStation::StaticClass(), NAME_None,
		TEXT("A.R.I.A.: Good morning. Sol 2. The ping repeated eleven times overnight. It is not random."));
	Step(C::Stabilization, TEXT("Decode the black-box signal at the Comm-Relay terminal."), TEXT("CommTerminalUsed"), ACommRelayTerminal::StaticClass(), NAME_None,
		TEXT("A.R.I.A.: Decoded. R-3 did not burn up - it came down in this valley. It is transmitting crash coordinates and two survey-beacon IDs: ALPHA and BETA. The recon shuttle on pad 2 has the range."));
	Step(C::Stabilization, TEXT("Pre-flight the Recon Shuttle on the pad outside SEC-01 (board it with E)."), TEXT("ShuttleBoarded"), AMarsReconShuttle::StaticClass(), NAME_None,
		TEXT("A.R.I.A.: Recon shuttle RS-2 online. VTOL thrusters green. Cabin pressurised - it will recharge your suit while you fly."));

	// ---------------- Chapter 2: Sol 3-4, survey ----------------
	Step(C::Survey, TEXT("Fly to SURVEY BEACON ALPHA: crashed relay satellite R-3. Recover its black box."), TEXT("Log_Relay"), AMarsStorageLocker::StaticClass(), TEXT("POI_Relay"),
		TEXT("A.R.I.A.: Black box recovered, plus an intact quantum transistor. The final R-3 command was not a fault. It was a manual de-orbit, signed by the Directorate."));
	Step(C::Survey, TEXT("Fly to SURVEY BEACON BETA: the deep ice-core drill rig."), TEXT("Log_Drill"), AMarsStorageLocker::StaticClass(), TEXT("POI_Drill"),
		TEXT("A.R.I.A.: Second transistor secured. Warning: the core samples are biologically active. Your bio-load just rose. The Ares-III drill crew hit something at 140 metres."));
	Step(C::Survey, TEXT("Investigate the abandoned ARES-III outpost."), TEXT("Log_Outpost"), AMarsStorageLocker::StaticClass(), TEXT("POI_Outpost"),
		TEXT("A.R.I.A.: Ares-III logs decrypted. They knew. Ares-IV was placed downwind of the drill site on purpose. The quarantine was never meant to keep it away from Earth. It was meant to keep you here, observed."));
	Step(C::Survey, TEXT("Return to Ares-IV and rest. Storm season is coming."), TEXT("Slept"), AMarsBunkStation::StaticClass(), NAME_None,
		TEXT("A.R.I.A.: Sol briefing: dust-storm probability is climbing and the solar weather is turning. Your symptoms will not wait. Neither should the uplink."));

	// ---------------- Chapter 3: Sol 5+, uplink ----------------
	Step(C::Uplink, TEXT("Install the quantum transistors at the Comm-Relay terminal."), NAME_None, ACommRelayTerminal::StaticClass(), NAME_None,
		TEXT("A.R.I.A.: Transistor bank complete. The transceiver itself is still damaged."),
		[](UWorld* W) { const ACommRelayModule* Comm = FindComm(W); return Comm && Comm->AreTransistorsInstalled(); });
	Step(C::Uplink, TEXT("Repair the transceiver with spare parts (Comm-Relay terminal)."), NAME_None, ACommRelayTerminal::StaticClass(), NAME_None,
		TEXT("A.R.I.A.: Transceiver integrity acceptable. Route power to the uplink if the grid is weak - Engineering terminal, SEC-06."),
		[](UWorld* W) { const ACommRelayModule* Comm = FindComm(W); return Comm && Comm->TransceiverIntegrity >= 0.6f; });
	Step(C::Uplink, TEXT("Align the dish: hold E at the alignment terminal until calibration reaches 100 %."), NAME_None, ADishAlignmentTerminal::StaticClass(), NAME_None,
		TEXT("A.R.I.A.: Dish locked on Earth. Open civilian band available. Everything we found is queued. Say the word."),
		[](UWorld* W) { const ACommRelayModule* Comm = FindComm(W); return Comm && Comm->IsReadyToTransmit(); });
	Step(C::Uplink, TEXT("Transmit the truth about Ares-IV to Earth."), NAME_None, ACommRelayTerminal::StaticClass(), NAME_None,
		TEXT("A.R.I.A.: Transmission acknowledged by three ground stations. It is public now. Whatever they decide, they cannot pretend they did not know."),
		[](UWorld* W) { const UMarsStationSubsystem* S = W->GetSubsystem<UMarsStationSubsystem>(); return S && S->HasEarthContact(); });
}

void UMarsNarrativeDirector::StartStory()
{
	bStarted = true;
	ObjectiveFlash = 1.0f;
	RefreshTarget();
}

void UMarsNarrativeDirector::SetStepIndex(int32 Index)
{
	StepIndex = FMath::Clamp(Index, 0, Steps.Num());
	bStarted = true;
	RefreshTarget();
}

EMarsChapter UMarsNarrativeDirector::GetChapter() const
{
	return Steps.IsValidIndex(StepIndex) ? Steps[StepIndex].Chapter : EMarsChapter::Epilogue;
}

FString UMarsNarrativeDirector::GetChapterTitle() const
{
	switch (GetChapter())
	{
	case EMarsChapter::Stabilization: return TEXT("CHAPTER I  //  STABILISATION");
	case EMarsChapter::Survey:        return TEXT("CHAPTER II  //  THE SURVEY");
	case EMarsChapter::Uplink:        return TEXT("CHAPTER III  //  THE UPLINK");
	default:                          return TEXT("EPILOGUE");
	}
}

FString UMarsNarrativeDirector::GetObjectiveText() const
{
	return Steps.IsValidIndex(StepIndex) ? Steps[StepIndex].Text : TEXT("The truth is out. Survive until the rescue window.");
}

void UMarsNarrativeDirector::Advance()
{
	if (!Steps.IsValidIndex(StepIndex))
	{
		return;
	}
	const FMarsStoryStep& Done = Steps[StepIndex];
	if (!Done.AriaOnComplete.IsEmpty())
	{
		if (UAriaSubsystem* Aria = GetWorld()->GetSubsystem<UAriaSubsystem>())
		{
			Aria->SayScripted(Done.AriaOnComplete);
		}
	}
	const EMarsChapter OldChapter = Done.Chapter;
	++StepIndex;
	ObjectiveFlash = 1.0f;
	RefreshTarget();
	UE_LOG(LogMarsSurvival, Log, TEXT("Story step %d: %s"), StepIndex, *GetObjectiveText());

	if (AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->ShowNotification(FText::FromString(GetChapter() != OldChapter ? GetChapterTitle() : TEXT("OBJECTIVE UPDATED")), 4.0f);
	}
}

void UMarsNarrativeDirector::RefreshTarget()
{
	CachedTarget.Reset();
	if (!Steps.IsValidIndex(StepIndex) || !Steps[StepIndex].TargetClass)
	{
		return;
	}
	const FMarsStoryStep& S = Steps[StepIndex];
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	float Best = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> It(GetWorld(), S.TargetClass); It; ++It)
	{
		if (S.TargetTag != NAME_None && !It->ActorHasTag(S.TargetTag))
		{
			continue;
		}
		const AMarsStorageLocker* Locker = Cast<AMarsStorageLocker>(*It);
		if (Locker && Locker->bOpened)
		{
			continue;
		}
		const float D = Player ? FVector::DistSquared(Player->GetActorLocation(), It->GetActorLocation()) : 0.0f;
		if (D < Best)
		{
			Best = D;
			CachedTarget = *It;
		}
	}
}

void UMarsNarrativeDirector::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || World->IsPaused())
	{
		return;
	}
	ObjectiveFlash = FMath::Max(ObjectiveFlash - DeltaTime * 0.4f, 0.0f);

	if (Steps.IsValidIndex(StepIndex) && Steps[StepIndex].Poll && Steps[StepIndex].Poll(World))
	{
		Advance();
	}

	TargetRefresh -= DeltaTime;
	if (TargetRefresh <= 0.0f)
	{
		TargetRefresh = 1.0f;
		RefreshTarget();
	}

	// Escalation: the longer you are here and the deeper the story, the more the symptoms bite.
	const UMarsEnvironmentSubsystem* Env = World->GetSubsystem<UMarsEnvironmentSubsystem>();
	const int32 Sol = Env ? Env->GetSolNumber() : 1;
	static const float SolFactor[] = { 0.0f, 0.0f, 0.1f, 0.3f, 0.45f, 0.65f };
	const float FromSol = Sol < 6 ? SolFactor[FMath::Max(Sol, 0)] : FMath::Min(0.65f + 0.12f * (Sol - 5), 1.0f);
	const EMarsChapter Chapter = GetChapter();
	const float FromChapter = Chapter == EMarsChapter::Stabilization ? 0.0f : Chapter == EMarsChapter::Survey ? 0.35f : 0.6f;
	Escalation = FMath::Max(FromSol, FromChapter);

	if (AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (UPlayerBiometricsComponent* Bio = Player->GetBiometrics())
		{
			Bio->NarrativeEscalation = Escalation;
			// Chapter III: the bio-agent progresses on its own (about +1 % load per game hour).
			if (Chapter == EMarsChapter::Uplink && !Bio->IsDead())
			{
				Bio->ExtremophileBioLoad = FMath::Min(Bio->ExtremophileBioLoad + MarsTime::ToGameHours(this, DeltaTime) * 0.01f, 1.0f);
			}
		}
	}
	else if (const AMarsReconShuttle* Shuttle = Cast<AMarsReconShuttle>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (AMarsCharacter* Pilot = Shuttle->GetPilot())
		{
			if (UPlayerBiometricsComponent* Bio = Pilot->GetBiometrics())
			{
				Bio->NarrativeEscalation = Escalation;
			}
		}
	}

	// Weather follows the story: a calm first Sol, storm season in chapter III.
	if (UMarsEnvironmentSubsystem* MutableEnv = World->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		MutableEnv->StormChancePerHour = Chapter == EMarsChapter::Stabilization ? (Sol <= 1 ? 0.0f : 0.02f) : Chapter == EMarsChapter::Survey ? 0.04f : 0.12f;
		MutableEnv->SpeChancePerStormHour = Chapter == EMarsChapter::Uplink ? 0.2f : 0.05f;
	}
}
