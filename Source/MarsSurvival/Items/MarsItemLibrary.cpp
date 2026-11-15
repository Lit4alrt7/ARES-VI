#include "Items/MarsItemLibrary.h"

namespace
{
	FMarsItemInfo MakeInfo(const TCHAR* Name, EMarsItemCategory Category, float MassKg, const TCHAR* Description)
	{
		FMarsItemInfo Info;
		Info.DisplayName = FText::FromString(Name);
		Info.Category = Category;
		Info.MassKg = MassKg;
		Info.Description = FText::FromString(Description);
		return Info;
	}
}

FMarsItemInfo UMarsItemLibrary::GetItemInfo(EMarsItemType ItemType)
{
	switch (ItemType)
	{
	case EMarsItemType::Glock18:                return MakeInfo(TEXT("Glock-18"), EMarsItemCategory::Weapon, 0.62f, TEXT("Select-fire 9x19mm sidearm. Every round fired indoors can puncture the pressure hull."));
	case EMarsItemType::WeldSealantGun:         return MakeInfo(TEXT("Weld/Sealant Gun"), EMarsItemCategory::Tool, 2.1f, TEXT("Cures fast-set sealant over punctures; arc-weld mode restores structural integrity."));
	case EMarsItemType::TorqueWrench:           return MakeInfo(TEXT("Torque Wrench"), EMarsItemCategory::Tool, 1.2f, TEXT("Re-torques bulkhead flanges to re-compress cold-contracted elastomer seals."));
	case EMarsItemType::RadiationDosimeter:     return MakeInfo(TEXT("Radiation Dosimeter"), EMarsItemCategory::Tool, 0.3f, TEXT("Tissue-equivalent proportional counter. Integrates dose in mSv with audible clicks."));
	case EMarsItemType::Ammo9mm:                return MakeInfo(TEXT("9x19mm Rounds"), EMarsItemCategory::Ammunition, 0.008f, TEXT("Full metal jacket. Penetrates aluminium-lithium hull panels."));
	case EMarsItemType::SealantCartridge:       return MakeInfo(TEXT("Sealant Cartridge"), EMarsItemCategory::Consumable, 0.4f, TEXT("Enough fast-cure sealant for five puncture patches."));
	case EMarsItemType::O2Canister:             return MakeInfo(TEXT("O2 Canister"), EMarsItemCategory::Consumable, 1.5f, TEXT("300 L (STP) of breathing oxygen for the EVA suit."));
	case EMarsItemType::MethyleneBlueCartridge: return MakeInfo(TEXT("Methylene Blue Cartridge"), EMarsItemCategory::Medical, 0.05f, TEXT("1-2 mg/kg IV. Accelerates NADPH-dependent reduction of methaemoglobin."));
	case EMarsItemType::IodinePills:            return MakeInfo(TEXT("Potassium Iodide Pills"), EMarsItemCategory::Medical, 0.02f, TEXT("Out-competes perchlorate at the thyroid sodium-iodide symporter."));
	case EMarsItemType::RegolithSample:         return MakeInfo(TEXT("Regolith Sample"), EMarsItemCategory::Sample, 0.25f, TEXT("Fine oxidised dust. Contains ~0.5-1 wt% perchlorate salts."));
	case EMarsItemType::RockSample:             return MakeInfo(TEXT("Rock Sample"), EMarsItemCategory::Sample, 0.8f, TEXT("Basaltic fragment with endolithic micro-cavities."));
	case EMarsItemType::QuantumTransistor:      return MakeInfo(TEXT("Quantum Transistor"), EMarsItemCategory::Component, 0.15f, TEXT("Cryogenic single-photon switching element for the quantum comm-relay."));
	case EMarsItemType::SpareParts:             return MakeInfo(TEXT("Spare Parts"), EMarsItemCategory::Component, 1.0f, TEXT("Assorted fasteners, boards and fibre for electronics repair."));
	case EMarsItemType::SurfaceBeacon:          return MakeInfo(TEXT("Surface Beacon"), EMarsItemCategory::Deployable, 1.8f, TEXT("Battery-powered strobe and radio ping for navigating in dust storms."));
	case EMarsItemType::WaterPouch:             return MakeInfo(TEXT("Water Pouch"), EMarsItemCategory::Consumable, 0.5f, TEXT("500 mL recycled water from the WRS / photobioreactor condensate loop."));
	case EMarsItemType::FoodRation:             return MakeInfo(TEXT("Food Ration"), EMarsItemCategory::Consumable, 0.6f, TEXT("Thermostabilised NASA-style meal bar, ~900 kcal."));
	case EMarsItemType::ChlorellaPaste:         return MakeInfo(TEXT("Chlorella Paste"), EMarsItemCategory::Consumable, 0.3f, TEXT("Harvested algal biomass: ~50% protein, bitter, edible."));
	case EMarsItemType::NeuroAntitoxin:         return MakeInfo(TEXT("Neuro-Antitoxin"), EMarsItemCategory::Medical, 0.05f, TEXT("Targeted chelator / binding agent against the bio-agent's neuroactive metabolite."));
	case EMarsItemType::Amifostine:             return MakeInfo(TEXT("Amifostine"), EMarsItemCategory::Medical, 0.05f, TEXT("Thiol radioprotectant (WR-2721). Scavenges radiolytic free radicals for several hours."));
	case EMarsItemType::BioAgentCulture:        return MakeInfo(TEXT("Bio-Agent Culture"), EMarsItemCategory::Sample, 0.1f, TEXT("Sealed culture of the extremophile from the deep core sample. BSL-3 handling."));
	default:                                    return MakeInfo(TEXT("Unknown"), EMarsItemCategory::Component, 0.0f, TEXT(""));
	}
}

bool UMarsItemLibrary::IsEquippable(EMarsItemType ItemType)
{
	return ItemType == EMarsItemType::Glock18
		|| ItemType == EMarsItemType::WeldSealantGun
		|| ItemType == EMarsItemType::TorqueWrench
		|| ItemType == EMarsItemType::RadiationDosimeter;
}

FText UMarsItemLibrary::GetItemName(EMarsItemType ItemType)
{
	return GetItemInfo(ItemType).DisplayName;
}
