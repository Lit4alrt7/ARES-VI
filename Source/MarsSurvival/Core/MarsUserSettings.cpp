#include "Core/MarsUserSettings.h"

#include "Audio/MarsAudioManager.h"
#include "Hallucination/HallucinationDirectorComponent.h"
#include "Player/MarsCharacter.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* Section = TEXT("MarsSurvival.Settings");
}

FMarsUserSettings& FMarsUserSettings::Get()
{
	static FMarsUserSettings Instance;
	if (!Instance.bLoaded)
	{
		Instance.Load();
	}
	return Instance;
}

void FMarsUserSettings::Load()
{
	bLoaded = true;
	if (!GConfig)
	{
		return;
	}
	const FString& Ini = GGameUserSettingsIni;
	GConfig->GetFloat(Section, TEXT("MouseSensitivity"), MouseSensitivity, Ini);
	GConfig->GetBool(Section, TEXT("InvertY"), bInvertY, Ini);
	GConfig->GetBool(Section, TEXT("MouseSmoothing"), bMouseSmoothing, Ini);
	GConfig->GetFloat(Section, TEXT("HeadBob"), HeadBob, Ini);
	GConfig->GetFloat(Section, TEXT("WeaponSway"), WeaponSway, Ini);
	GConfig->GetFloat(Section, TEXT("FieldOfView"), FieldOfView, Ini);
	GConfig->GetFloat(Section, TEXT("MasterVolume"), MasterVolume, Ini);
	GConfig->GetFloat(Section, TEXT("SfxVolume"), SfxVolume, Ini);
	GConfig->GetFloat(Section, TEXT("AmbientVolume"), AmbientVolume, Ini);
	GConfig->GetFloat(Section, TEXT("VoiceVolume"), VoiceVolume, Ini);
	GConfig->GetFloat(Section, TEXT("MusicVolume"), MusicVolume, Ini);
	GConfig->GetBool(Section, TEXT("Subtitles"), bSubtitles, Ini);
	GConfig->GetBool(Section, TEXT("InspectNewItems"), bInspectNewItems, Ini);
}

void FMarsUserSettings::Save() const
{
	if (!GConfig)
	{
		return;
	}
	const FString& Ini = GGameUserSettingsIni;
	GConfig->SetFloat(Section, TEXT("MouseSensitivity"), MouseSensitivity, Ini);
	GConfig->SetBool(Section, TEXT("InvertY"), bInvertY, Ini);
	GConfig->SetBool(Section, TEXT("MouseSmoothing"), bMouseSmoothing, Ini);
	GConfig->SetFloat(Section, TEXT("HeadBob"), HeadBob, Ini);
	GConfig->SetFloat(Section, TEXT("WeaponSway"), WeaponSway, Ini);
	GConfig->SetFloat(Section, TEXT("FieldOfView"), FieldOfView, Ini);
	GConfig->SetFloat(Section, TEXT("MasterVolume"), MasterVolume, Ini);
	GConfig->SetFloat(Section, TEXT("SfxVolume"), SfxVolume, Ini);
	GConfig->SetFloat(Section, TEXT("AmbientVolume"), AmbientVolume, Ini);
	GConfig->SetFloat(Section, TEXT("VoiceVolume"), VoiceVolume, Ini);
	GConfig->SetFloat(Section, TEXT("MusicVolume"), MusicVolume, Ini);
	GConfig->SetBool(Section, TEXT("Subtitles"), bSubtitles, Ini);
	GConfig->SetBool(Section, TEXT("InspectNewItems"), bInspectNewItems, Ini);
	GConfig->Flush(false, Ini);
}

void FMarsUserSettings::Apply(UWorld* World) const
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const AMarsCharacter* C = PC ? Cast<AMarsCharacter>(PC->GetPawn()) : nullptr;
	if (!C)
	{
		return;
	}
	if (C->GetFirstPersonCamera())
	{
		C->GetFirstPersonCamera()->SetFieldOfView(FieldOfView);
	}
	if (UMarsAudioManager* Audio = C->GetAudioManager())
	{
		Audio->ApplyVolumeSettings(MasterVolume, SfxVolume, AmbientVolume, VoiceVolume, MusicVolume);
	}
}

// ----------------------------------------------------------------------------
// Bindings
// ----------------------------------------------------------------------------

const TArray<FMarsBinding>& FMarsUserSettings::GetBindings()
{
	static TArray<FMarsBinding> Bindings;
	if (Bindings.Num() == 0)
	{
		auto Axis = [](const TCHAR* Label, const TCHAR* Name, float Scale) { FMarsBinding B; B.Label = Label; B.MappingName = Name; B.bAxis = true; B.AxisScale = Scale; return B; };
		auto Action = [](const TCHAR* Label, const TCHAR* Name) { FMarsBinding B; B.Label = Label; B.MappingName = Name; return B; };
		Bindings =
		{
			Axis(TEXT("Move forward"), TEXT("MoveForward"), 1.0f),
			Axis(TEXT("Move back"), TEXT("MoveForward"), -1.0f),
			Axis(TEXT("Strafe left"), TEXT("MoveRight"), -1.0f),
			Axis(TEXT("Strafe right"), TEXT("MoveRight"), 1.0f),
			Action(TEXT("Jump"), TEXT("Jump")),
			Action(TEXT("Sprint"), TEXT("Sprint")),
			Action(TEXT("Crouch"), TEXT("Crouch")),
			Action(TEXT("Interact / hold"), TEXT("Interact")),
			Action(TEXT("Use tool / fire"), TEXT("PrimaryAction")),
			Action(TEXT("Alt use / fire mode"), TEXT("SecondaryAction")),
			Action(TEXT("Reload"), TEXT("Reload")),
			Action(TEXT("Slot 1 - Glock-18"), TEXT("EquipSlot1")),
			Action(TEXT("Slot 2 - Sealant gun"), TEXT("EquipSlot2")),
			Action(TEXT("Slot 3 - Torque wrench"), TEXT("EquipSlot3")),
			Action(TEXT("Slot 4 - Dosimeter"), TEXT("EquipSlot4")),
			Action(TEXT("Slot 5 - Drink"), TEXT("EquipSlot5")),
			Action(TEXT("Slot 6 - Eat"), TEXT("EquipSlot6")),
			Action(TEXT("Holster"), TEXT("Holster")),
			Action(TEXT("Med-kit (tap/hold)"), TEXT("UseMedication")),
			Action(TEXT("O2 canister"), TEXT("UseO2Canister")),
			Action(TEXT("Deploy beacon"), TEXT("DeployBeacon")),
			Action(TEXT("Talk to A.R.I.A."), TEXT("TalkToAria")),
			Action(TEXT("Wrist computer"), TEXT("WristDisplay")),
			Action(TEXT("Nav waypoints on/off"), TEXT("Waypoints")),
			Action(TEXT("Helmet lamp"), TEXT("HelmetLamp")),
			Action(TEXT("Inventory"), TEXT("Inventory")),
			Action(TEXT("Shuttle landing gear"), TEXT("LandingGear")),
			Action(TEXT("Toggle HUD"), TEXT("ToggleHUD")),
			Action(TEXT("Pause menu"), TEXT("Pause")),
		};
	}
	return Bindings;
}

FKey FMarsUserSettings::GetBoundKey(const FMarsBinding& Binding)
{
	const UInputSettings* Settings = GetDefault<UInputSettings>();
	if (Binding.bAxis)
	{
		for (const FInputAxisKeyMapping& M : Settings->GetAxisMappings())
		{
			if (M.AxisName == Binding.MappingName && FMath::IsNearlyEqual(M.Scale, Binding.AxisScale) && !M.Key.IsMouseButton() && !M.Key.IsGamepadKey())
			{
				return M.Key;
			}
		}
	}
	else
	{
		for (const FInputActionKeyMapping& M : Settings->GetActionMappings())
		{
			if (M.ActionName == Binding.MappingName && !M.Key.IsGamepadKey())
			{
				return M.Key;
			}
		}
	}
	return EKeys::Invalid;
}

static void RebuildPlayerKeymaps(UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (PC->PlayerInput)
			{
				PC->PlayerInput->ForceRebuildingKeyMaps(true);
			}
		}
	}
}

void FMarsUserSettings::Rebind(UWorld* World, const FMarsBinding& Binding, const FKey& NewKey)
{
	UInputSettings* Settings = GetMutableDefault<UInputSettings>();
	const FKey Old = GetBoundKey(Binding);
	if (Binding.bAxis)
	{
		if (Old.IsValid())
		{
			Settings->RemoveAxisMapping(FInputAxisKeyMapping(Binding.MappingName, Old, Binding.AxisScale), false);
		}
		Settings->AddAxisMapping(FInputAxisKeyMapping(Binding.MappingName, NewKey, Binding.AxisScale), false);
	}
	else
	{
		if (Old.IsValid())
		{
			Settings->RemoveActionMapping(FInputActionKeyMapping(Binding.MappingName, Old), false);
		}
		Settings->AddActionMapping(FInputActionKeyMapping(Binding.MappingName, NewKey), false);
	}
	Settings->SaveKeyMappings();
	RebuildPlayerKeymaps(World);
}

void FMarsUserSettings::ResetBindingsToDefaults(UWorld* World)
{
	struct FDefault { const TCHAR* Name; FKey Key; };
	const FDefault Actions[] =
	{
		{ TEXT("Jump"), EKeys::SpaceBar }, { TEXT("Sprint"), EKeys::LeftShift }, { TEXT("Crouch"), EKeys::C },
		{ TEXT("Interact"), EKeys::E }, { TEXT("PrimaryAction"), EKeys::LeftMouseButton }, { TEXT("SecondaryAction"), EKeys::RightMouseButton },
		{ TEXT("Reload"), EKeys::R }, { TEXT("EquipSlot1"), EKeys::One }, { TEXT("EquipSlot2"), EKeys::Two }, { TEXT("EquipSlot3"), EKeys::Three },
		{ TEXT("EquipSlot4"), EKeys::Four }, { TEXT("EquipSlot5"), EKeys::Five }, { TEXT("EquipSlot6"), EKeys::Six }, { TEXT("Holster"), EKeys::X },
		{ TEXT("UseMedication"), EKeys::H }, { TEXT("UseO2Canister"), EKeys::Q }, { TEXT("DeployBeacon"), EKeys::B }, { TEXT("TalkToAria"), EKeys::T },
		{ TEXT("WristDisplay"), EKeys::G }, { TEXT("Waypoints"), EKeys::N }, { TEXT("HelmetLamp"), EKeys::F }, { TEXT("Inventory"), EKeys::I }, { TEXT("LandingGear"), EKeys::L }, { TEXT("ToggleHUD"), EKeys::U }, { TEXT("Pause"), EKeys::Escape },
	};
	for (const FMarsBinding& B : GetBindings())
	{
		FKey Key = EKeys::Invalid;
		if (B.bAxis)
		{
			Key = B.MappingName == TEXT("MoveForward") ? (B.AxisScale > 0 ? EKeys::W : EKeys::S) : (B.AxisScale > 0 ? EKeys::D : EKeys::A);
		}
		else
		{
			for (const FDefault& D : Actions)
			{
				if (B.MappingName == D.Name) { Key = D.Key; break; }
			}
		}
		if (Key.IsValid())
		{
			Rebind(World, B, Key);
		}
	}
}
