#include "AI/AriaSubsystem.h"

#include "MarsSurvival.h"
#include "AI/ClaudeAPIClient.h"
#include "Audio/AriaVoiceComponent.h"
#include "Base/MarsBaseModule.h"
#include "Player/EVASuitComponent.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsStationSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

bool UAriaSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UAriaSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAriaSubsystem, STATGROUP_Tickables);
}

void UAriaSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	Client = UClaudeAPIClient::CreateAriaClient(this);
	Client->OnAriaResponseReceived.AddDynamic(this, &UAriaSubsystem::HandleResponse);
	Client->OnAriaRequestFailed.AddDynamic(this, &UAriaSubsystem::HandleFailure);

	if (UMarsStationSubsystem* Station = InWorld.GetSubsystem<UMarsStationSubsystem>())
	{
		Station->OnStationAlert.AddDynamic(this, &UAriaSubsystem::HandleStationAlert);
	}

}

void UAriaSubsystem::Deinitialize()
{
	if (Client)
	{
		Client->CancelRequest();
	}
	Super::Deinitialize();
}

void UAriaSubsystem::Tick(float DeltaTime)
{
	TimeSincePeriodic += DeltaTime;
	TimeSinceEvent += DeltaTime;

	// Late-bind to the player's biometrics (pawn may spawn after world begin play).
	if (!bBoundToPlayer)
	{
		if (const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (UPlayerBiometricsComponent* Bio = Player->GetBiometrics())
			{
				Bio->OnVitalsCritical.AddDynamic(this, &UAriaSubsystem::HandleVitalsCritical);
				bBoundToPlayer = true;
			}
		}
	}

	// Gunfire bursts.
	if (const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>())
	{
		const int32 Discharges = Station->GetTotalDischarges();
		if (Discharges >= LastSeenDischarges + 3 && TimeSinceEvent > EventCooldownSeconds)
		{
			LastSeenDischarges = Discharges;
			RequestReport(TEXT("KineticDischarge"));
		}
	}

	if (TimeSincePeriodic >= PeriodicReportSeconds)
	{
		RequestReport(TEXT("PeriodicStatus"));
	}
}

void UAriaSubsystem::HandleStationAlert(FName AlertType, AMarsBaseModule* Module)
{
	if (TimeSinceEvent > EventCooldownSeconds)
	{
		RequestReport(AlertType);
	}
}

void UAriaSubsystem::HandleVitalsCritical(FName Reason)
{
	if (TimeSinceEvent > EventCooldownSeconds)
	{
		RequestReport(Reason);
	}
}

bool UAriaSubsystem::IsAwaitingResponse() const
{
	return Client && Client->IsRequestInFlight();
}

void UAriaSubsystem::RequestReport(FName Reason, const FString& CrewStatement)
{
	TimeSinceEvent = 0.0f;
	TimeSincePeriodic = 0.0f;
	if (const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>())
	{
		LastSeenDischarges = Station->GetTotalDischarges();
	}

	PendingReason = Reason;
	PendingStatement = CrewStatement;

	if (Client && Client->HasApiKey())
	{
		if (!Client->IsRequestInFlight())
		{
			Client->SendTelemetryToClaude(BuildTelemetry(Reason), CrewStatement);
		}
	}
	else
	{
		PushMessage(BuildOfflineReport(Reason, CrewStatement), true);
	}
}

FString UAriaSubsystem::BuildTelemetry(FName Reason) const
{
	FString Telemetry = FString::Printf(TEXT("TRIGGER=%s\n"), *Reason.ToString());

	if (const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		Telemetry += TEXT("ENV: ") + Env->GetTelemetryReport() + TEXT("\n");
	}
	if (const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>())
	{
		Telemetry += Station->BuildStationTelemetry();
	}
	if (const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		const AMarsBaseModule* Module = Player->GetCurrentModule();
		Telemetry += FString::Printf(TEXT("CREW_LOCATION=%s\n"), Module ? *Module->GetModuleName() : TEXT("EXTERIOR_SURFACE"));
		if (const UPlayerBiometricsComponent* Bio = Player->GetBiometrics())
		{
			Telemetry += Bio->GetTelemetryReport() + TEXT("\n");
		}
		if (const UEVASuitComponent* Suit = Player->GetSuit())
		{
			Telemetry += Suit->GetTelemetryReport() + TEXT("\n");
		}
	}
	return Telemetry;
}

void UAriaSubsystem::HandleResponse(const FString& ResponseText)
{
	PushMessage(ResponseText, false);
}

void UAriaSubsystem::HandleFailure(int32 HttpStatusCode, const FString& ErrorMessage)
{
	// Keep the game informative even when the uplink fails.
	PushMessage(BuildOfflineReport(PendingReason, PendingStatement), true);
}

void UAriaSubsystem::PushMessage(const FString& Text, bool bOffline)
{
	FAriaMessage Message;
	Message.Text = Text;
	Message.WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Message.bOffline = bOffline;
	MessageLog.Add(Message);
	if (MessageLog.Num() > 20)
	{
		MessageLog.RemoveAt(0);
	}
	OnAriaMessage.Broadcast(Message);

	// Every transmission is also spoken (formant voice over lo-fi radio).
	if (const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (UAriaVoiceComponent* Voice = Player->GetAriaVoice())
		{
			Voice->Speak(Text);
		}
	}
}

void UAriaSubsystem::SayScripted(const FString& Text)
{
	TimeSinceEvent = 0.0f;
	TimeSincePeriodic = 0.0f;
	PushMessage(Text, false);
}

FString UAriaSubsystem::BuildOfflineReport(FName Reason, const FString& CrewStatement) const
{
	TArray<FString> Lines;
	const AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UPlayerBiometricsComponent* Bio = Player ? Player->GetBiometrics() : nullptr;
	const UEVASuitComponent* Suit = Player ? Player->GetSuit() : nullptr;
	const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>();

	// Worst module first.
	if (Station)
	{
		for (const AMarsBaseModule* Module : Station->GetModules())
		{
			if (Module->IsBreached())
			{
				Lines.Add(FString::Printf(TEXT("%s: HULL BREACH. P %.1f kPa. Evacuate or don suit."), *Module->GetModuleName().ToUpper(), Module->CurrentPressureKpa));
			}
			else if (Module->PunctureCount > 0)
			{
				Lines.Add(FString::Printf(TEXT("%s: %d penetrations consistent with crew sidearm. Leak %.2f cm2. Apply sealant."),
					*Module->GetModuleName().ToUpper(), Module->PunctureCount, Module->GetTotalLeakAreaCm2()));
			}
			else if (Module->SealIntegrity < 0.7f)
			{
				Lines.Add(FString::Printf(TEXT("%s: seal integrity %.0f%% at %.0f C. Re-torque flanges."),
					*Module->GetModuleName().ToUpper(), Module->SealIntegrity * 100.0f, Module->SealTemperatureC));
			}
			if (Lines.Num() >= 2)
			{
				break;
			}
		}
		const int32 Recent = Station->GetDischargesInLastSeconds(60.0f);
		if (Recent > 0)
		{
			Lines.Add(FString::Printf(TEXT("Unauthorised kinetic discharge: %d rounds in 60 s. Cease fire."), Recent));
		}
	}

	if (Bio)
	{
		static const TCHAR* StageNames[] = { TEXT(""), TEXT("prodromal hypoxia"), TEXT("acute methaemoglobinaemia"), TEXT("neuro-toxic encephalopathy"), TEXT("systemic collapse") };
		const int32 Stage = Bio->GetStageIndex();
		if (Stage >= 4)
		{
			Lines.Insert(FString::Printf(TEXT("DIAGNOSTIC STAGE 4: %s. Time to irreversible collapse %.0f s. Report to Med-Bay scanner for stabilisation."),
				StageNames[4], Bio->GetStage4SecondsRemaining()), 0);
		}
		else if (Stage >= 1)
		{
			Lines.Add(FString::Printf(TEXT("Diagnostic stage %d: %s."), Stage, StageNames[Stage]));
		}
		if (Bio->Hydration < 0.25f)
		{
			Lines.Add(FString::Printf(TEXT("Hydration %.0f%%. Ingest water."), Bio->Hydration * 100.0f));
		}
		if (Bio->Nutrition < 0.2f)
		{
			Lines.Add(FString::Printf(TEXT("Nutrition %.0f%%. Caloric intake required."), Bio->Nutrition * 100.0f));
		}
		if (Bio->MethemoglobinPercentage >= 15.0f)
		{
			Lines.Add(FString::Printf(TEXT("MetHb %.1f%%. Report to Medical Bay. Administer methylene blue."), Bio->MethemoglobinPercentage));
		}
		if (Bio->Co2LevelPpm >= 5000.0f)
		{
			Lines.Add(FString::Printf(TEXT("Inspired CO2 %.0f ppm. Hypercapnia. Restore scrubber."), Bio->Co2LevelPpm));
		}
		if (Bio->HeartRateBpm > 110.0f || Bio->CortisolNmolL > 650.0f)
		{
			Lines.Add(FString::Printf(TEXT("Heart rate %.0f bpm. Cortisol %.0f nmol/L. Sympathetic overactivation."), Bio->HeartRateBpm, Bio->CortisolNmolL));
		}
	}
	if (Suit && Suit->IsSuitDonned() && Suit->GetO2Fraction01() < 0.25f)
	{
		Lines.Add(FString::Printf(TEXT("Suit O2 %.0f L. Return to airlock."), Suit->O2RemainingLiters));
	}

	if (!CrewStatement.IsEmpty())
	{
		Lines.Insert(TEXT("Crew report logged. Motion, thermal and CO2-plume sensors register one occupant."), 0);
	}
	if (Lines.Num() == 0)
	{
		Lines.Add(TEXT("All monitored parameters within limits. Comm-relay link: DOWN."));
	}

	return TEXT("A.R.I.A.: ") + FString::Join(Lines, TEXT(" "));
}
