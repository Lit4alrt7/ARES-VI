#include "Core/MarsTime.h"

#include "Engine/World.h"
#include "World/MarsEnvironmentSubsystem.h"

float MarsTime::GetTimeScale(const UObject* WorldContextObject)
{
	if (const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		if (const UMarsEnvironmentSubsystem* Env = World->GetSubsystem<UMarsEnvironmentSubsystem>())
		{
			return Env->GetTimeScale();
		}
	}
	return DefaultTimeScale;
}

float MarsTime::ToGameHours(const UObject* WorldContextObject, float RealDeltaSeconds)
{
	return RealDeltaSeconds * GetTimeScale(WorldContextObject) / SecondsPerHour;
}
