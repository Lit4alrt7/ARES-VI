#pragma once

#include "CoreMinimal.h"
#include "MarsModuleTypes.generated.h"

/** The six pressurised compartments of Ares Station. */
UENUM(BlueprintType)
enum class EMarsModuleType : uint8
{
	AirlockDecon        UMETA(DisplayName = "Airlock & Decon"),
	EvaPrep             UMETA(DisplayName = "EVA Prep"),
	AeroponicBioDome    UMETA(DisplayName = "Aeroponic Bio-Dome"),
	XenobiologyLab      UMETA(DisplayName = "Xenobiology Lab"),
	MedicalBay          UMETA(DisplayName = "Medical Bay"),
	StorageCommRelay    UMETA(DisplayName = "Storage & Comm-Relay"),
	RingCorridor        UMETA(DisplayName = "Ring Corridor"),
	CrewQuarters        UMETA(DisplayName = "Crew Quarters")
};
