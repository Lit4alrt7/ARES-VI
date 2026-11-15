#include "Player/PlayerFatigueComponent.h"

#include "Core/MarsNarrativeDirector.h"
#include "Core/MarsTime.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "Engine/World.h"

UPlayerFatigueComponent::UPlayerFatigueComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPlayerFatigueComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AMarsCharacter* C = Cast<AMarsCharacter>(GetOwner());
	UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	if (!Bio || Bio->IsDead() || C->IsSleeping())
	{
		return;
	}
	const float GameHours = MarsTime::ToGameHours(this, DeltaTime);
	const float Exertion = 1.0f - Bio->Stamina;
	// Circadian rhythm: staying up through the Martian night (19:00-06:00) tires you 60 % faster.
	const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
	const float Circadian = Env && Env->IsNight() ? 1.6f : 1.0f;
	Fatigue = FMath::Clamp(Fatigue + GameHours / HoursToExhaustion * (1.0f + 0.6f * Exertion) * Circadian, 0.0f, 1.0f);
	Bio->SleepDeprivation = Fatigue;

	WarnCooldown -= DeltaTime;
	if (WarnCooldown <= 0.0f && Fatigue > 0.7f)
	{
		WarnCooldown = 90.0f;
		C->ShowNotification(FText::FromString(Fatigue > 0.9f ? TEXT("EXHAUSTED - YOUR MIND IS SLIPPING. SLEEP (SEC-07 BUNK)") : TEXT("TIRED - REST AT THE CREW BUNK (SEC-07)")), 4.0f);
	}
}

bool UPlayerFatigueComponent::CanSleep() const
{
	const UMarsEnvironmentSubsystem* Env = GetWorld() ? GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>() : nullptr;
	const float H = Env ? Env->GetLocalSolarTimeHours() : 12.0f;
	const bool bEvening = H >= 18.0f || H < 5.0f;
	return Fatigue >= 0.35f || bEvening;
}

float UPlayerFatigueComponent::GetHoursUntilMorning() const
{
	const UMarsEnvironmentSubsystem* Env = GetWorld() ? GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>() : nullptr;
	const float H = Env ? Env->GetLocalSolarTimeHours() : 22.0f;
	float Until = MarsTime::SolLengthHours - H + 7.0f;
	if (Until >= MarsTime::SolLengthHours)
	{
		Until -= MarsTime::SolLengthHours;
	}
	return FMath::Max(Until, 4.0f);
}

float UPlayerFatigueComponent::GetHoursUntil(float TargetHour) const
{
	const UMarsEnvironmentSubsystem* Env = GetWorld() ? GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>() : nullptr;
	const float H = Env ? Env->GetLocalSolarTimeHours() : 0.0f;
	float Until = TargetHour - H;
	if (Until <= 0.05f)
	{
		Until += MarsTime::SolLengthHours;
	}
	return Until;
}

void UPlayerFatigueComponent::PerformRest(float Hours, bool bSleep)
{
	AMarsCharacter* C = Cast<AMarsCharacter>(GetOwner());
	UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
	const bool bNightSleep = Env && Env->IsNight();
	if (Env)
	{
		Env->SkipHours(Hours);
	}
	UPlayerBiometricsComponent* Bio = C ? C->GetBiometrics() : nullptr;
	if (bSleep)
	{
		// Sleeping with the Martian night keeps the rhythm; daytime sleep is only partly restorative.
		Fatigue = bNightSleep ? 0.0f : Fatigue * 0.4f;
		if (!bNightSleep && C)
		{
			C->ShowNotification(FText::FromString(TEXT("DAYTIME SLEEP - ONLY PARTLY RESTED. SLEEP AT NIGHT TO KEEP YOUR RHYTHM")), 4.0f);
		}
	}
	else
	{
		Fatigue = FMath::Clamp(Fatigue - 0.03f * Hours, 0.0f, 1.0f); // resting helps a little
	}
	if (Bio)
	{
		Bio->SleepDeprivation = Fatigue;
		Bio->Stamina = 1.0f;
		Bio->Hydration = FMath::Max(Bio->Hydration - 0.012f * Hours, 0.05f);
		Bio->Nutrition = FMath::Max(Bio->Nutrition - 0.008f * Hours, 0.05f);
		if (bSleep)
		{
			Bio->HallucinationIntensity *= 0.3f;
			Bio->PanicLevel = 0.0f;
		}
	}
	if (bSleep)
	{
		UMarsNarrativeDirector::Report(this, TEXT("Slept"));
	}
}

void UPlayerFatigueComponent::PerformSleep()
{
	PerformRest(GetHoursUntilMorning(), true);
}
