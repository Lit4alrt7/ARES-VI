#pragma once

#include "CoreMinimal.h"

/**
 * Shared simulation clock. All physical simulation (thermal, pressure, physiology, suit
 * consumables) runs in accelerated "game time" so a Martian night fits into a play session.
 */
namespace MarsTime
{
	constexpr float SecondsPerHour = 3600.0f;
	constexpr float SolLengthHours = 24.0f;      // game clock: 1 Sol = 24 real minutes at TimeScale 60
	constexpr float DefaultTimeScale = 60.0f;   // 1 real second = 1 game minute

	/** Game seconds per real second (from UMarsEnvironmentSubsystem when present). */
	MARSSURVIVAL_API float GetTimeScale(const UObject* WorldContextObject);

	/** Converts a real-time delta into game hours. */
	MARSSURVIVAL_API float ToGameHours(const UObject* WorldContextObject, float RealDeltaSeconds);
}
