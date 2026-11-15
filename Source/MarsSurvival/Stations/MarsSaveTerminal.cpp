#include "Stations/MarsSaveTerminal.h"

#include "Core/MarsSaveGame.h"
#include "Components/PointLightComponent.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"

AMarsSaveTerminal::AMarsSaveTerminal()
{
	StationName = TEXT("MISSION LOG");
	PropKind = EMarsPropKind::Terminal;
	ScreenGlow->SetLightColor(FLinearColor(1.0f, 0.7f, 0.25f));
}

void AMarsSaveTerminal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Cooldown = FMath::Max(Cooldown - DeltaTime, 0.0f);
	WrittenFlash = FMath::Max(WrittenFlash - DeltaTime, 0.0f);
}

void AMarsSaveTerminal::Interact_Implementation(AMarsCharacter* C)
{
	if (!C || Cooldown > 0.0f)
	{
		return;
	}
	if (C->GetBiometrics() && C->GetBiometrics()->GetStageIndex() >= 4)
	{
		Beep(C, 0.5f);
		C->ShowNotification(FText::FromString(TEXT("LOG WRITE REFUSED - VITALS CRITICAL")), 2.5f);
		return;
	}
	Cooldown = 2.0f;
	if (UMarsSaveGame::SaveWorld(GetWorld(), UMarsSaveGame::ManualSlot, FString::Printf(TEXT("MISSION LOG - %s"), *StationName)))
	{
		WrittenFlash = 3.0f;
		LastWrite = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
		Beep(C, 1.5f);
		C->ShowNotification(FText::FromString(TEXT("MISSION LOG WRITTEN  //  GAME SAVED")), 3.0f);
	}
	else
	{
		Beep(C, 0.5f);
		C->ShowNotification(FText::FromString(TEXT("LOG WRITE FAILED")), 2.5f);
	}
}

FText AMarsSaveTerminal::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	return FText::FromString(TEXT("[E] Write mission log (save game)"));
}

FString AMarsSaveTerminal::GetScreenText() const
{
	if (WrittenFlash > 0.0f)
	{
		return TEXT("MISSION LOG\n>> WRITTEN <<");
	}
	return LastWrite.IsEmpty() ? TEXT("MISSION LOG\nREADY") : FString::Printf(TEXT("MISSION LOG\nLAST %s"), *LastWrite);
}

int32 AMarsSaveTerminal::GetScreenStatus() const
{
	return 0;
}
