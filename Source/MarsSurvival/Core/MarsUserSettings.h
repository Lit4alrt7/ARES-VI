#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UWorld;

/** One rebindable control (an action mapping, or one direction of an axis mapping). */
struct FMarsBinding
{
	FString Label;
	FName MappingName;
	bool bAxis = false;
	float AxisScale = 1.0f;
};

/**
 * Player preferences, persisted in GameUserSettings.ini ([MarsSurvival.Settings]); key bindings are
 * the engine's input mappings, saved to the user Input.ini.
 */
struct MARSSURVIVAL_API FMarsUserSettings
{
	// Camera / controls
	float MouseSensitivity = 1.0f;   // 0.2 .. 3
	bool bInvertY = false;
	bool bMouseSmoothing = false;
	float HeadBob = 0.5f;            // 0 .. 1 (0 = off, for motion sickness)
	float WeaponSway = 0.7f;         // 0 .. 1
	float FieldOfView = 80.0f;       // 65 .. 110

	// Audio (0 .. 1)
	float MasterVolume = 0.9f;
	float SfxVolume = 1.0f;
	float AmbientVolume = 0.8f;
	float VoiceVolume = 1.0f;
	float MusicVolume = 0.8f;
	bool bSubtitles = true;

	// Gameplay
	bool bInspectNewItems = true;

	static FMarsUserSettings& Get();
	void Load();
	void Save() const;

	/** Push the current values to the live game (camera, audio, captions). */
	void Apply(UWorld* World) const;

	// ---- Key bindings ----
	static const TArray<FMarsBinding>& GetBindings();
	static FKey GetBoundKey(const FMarsBinding& Binding);
	static void Rebind(UWorld* World, const FMarsBinding& Binding, const FKey& NewKey);
	static void ResetBindingsToDefaults(UWorld* World);

private:
	bool bLoaded = false;
};
