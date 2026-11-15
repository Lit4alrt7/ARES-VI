# Architecture

ARES-VI is a single C++ runtime module, `MarsSurvival`, on Unreal Engine 5.3. Almost everything is generated in code: the station layout, props, terrain, audio and UI. The only content assets are a handful of materials (created by the Python bootstrap script), the `AresStation` map, and two splash images.

## High-level flow

```
AMarsGameMode
 ├─ USplashIntroWidget        PARADOX logo → team poster → main menu (procedural Mars globe)
 ├─ UIntroBriefingWidget      archival briefing
 ├─ AMarsTutorialManager      4-step suit training card
 ├─ UMarsNarrativeDirector    3-chapter story, objectives, escalation
 └─ UMarsSaveGame             checkpoints / manual saves

World subsystems (tick every frame)
 ├─ UMarsEnvironmentSubsystem  Sol clock, temperatures, dust storms, solar particle events
 ├─ UMarsStationSubsystem      module registry, RTG power grid, alerts, Earth-contact win state
 ├─ UMarsNarrativeDirector     current objective + wayfinding target
 └─ UAriaSubsystem             telemetry → Claude API (or offline report) → A.R.I.A. message

AMarsCharacter (player)
 ├─ UPlayerBiometricsComponent   physiology, disease stages, hallucination intensity
 ├─ UPlayerFatigueComponent      awake-time fatigue, sleep
 ├─ UEVASuitComponent            suit O2, LiOH CO2 scrubbing, battery/heater
 ├─ UMarsInventoryComponent      items and counts
 ├─ UHallucinationDirectorComponent  turns hallucination intensity into phantoms, sounds, flashes
 ├─ URetroPostProcessComponent   drives the PS1 post-process material from physiology
 ├─ UMarsAudioManager + UMarsProceduralSynth   real-time procedural audio
 ├─ UAriaVoiceComponent          A.R.I.A. synthetic voice
 └─ UMarsSuitViewmodel           first-person arms / wrist computer
```

## Source folders

| Folder | Key classes | Responsibility |
|---|---|---|
| `AI/` | `UAriaSubsystem`, `UClaudeAPIClient` | Builds telemetry, requests reports periodically and on events, handles offline fallback. See [ARIA-Integration.md](ARIA-Integration.md). |
| `Audio/` | `UMarsProceduralSynth`, `UMarsAudioManager`, `UAriaVoiceComponent` | Stereo synthesis on the audio render thread. No sound assets. Optional `Content/Audio/IntroTheme.wav`. |
| `Base/` | `AMarsBaseModule`, `AAirlockModule`, `ABioDomeModule`, `AMedicalBayModule`, `ACommRelayModule` | Pressurised compartments: pressure, thermal, seals, CO2, hull damage and repair. |
| `Core/` | `AMarsGameMode`, `UMarsNarrativeDirector`, `UMarsSaveGame`, `AMarsTutorialManager`, `AMarsHUD`, `MarsTime`, `MarsUserSettings` | Mission rules, story, saving, settings, shared simulation clock. |
| `Equipment/` | `AMarsEquipment`, `AFuturisticGlock18`, `AMarsBullet`, `AWeldSealantTool`, `ATorqueWrenchTool`, `ARadiationDosimeter`, `AMarsImpactFX` | Hand-held tools and the weapon. Bullets are physical actors that damage modules. |
| `Hallucination/` | `UHallucinationDirectorComponent`, `AHallucinationPhantom`, `AMarsPhantomActor` | Phantoms and the "ballistic awakening". |
| `Items/` | `EMarsItemType`, `UMarsInventoryComponent`, `UMarsItemLibrary`, `AMarsPickup` | Item definitions and pickups. |
| `Player/` | `AMarsCharacter`, `UPlayerBiometricsComponent`, `UEVASuitComponent`, `UPlayerFatigueComponent`, `UMarsSuitViewmodel` | The player and their body. |
| `Props/` | `AMarsPropBuilder` | Builds every machine and item model procedurally in a PS1 style. |
| `Rendering/` | `URetroPostProcessComponent` | Feeds `M_PS1_PostProcess` parameters: pixel size, colour levels, dither, scanlines, hallucination, panic, Cherenkov flash, dust storm, hypoxia. |
| `Stations/` | `AMarsStationBase` + ~20 subclasses, `AMarsSaveTerminal` | Interactive machines with live in-world screens. |
| `UI/` | `UMarsPlayerHUD`, `USplashIntroWidget`, `UIntroBriefingWidget`, `UMarsPauseMenuWidget`, `FMarsSettingsPanel`, `MarsUIStyle` | All UI, drawn in C++ (Slate/UMG). |
| `Vehicles/` | `AMarsReconShuttle` | Flyable RS-2 VTOL pawn with its own battery and O2. |
| `World/` | `AMarsStationGenerator`, `AMarsTerrainGenerator`, `AMarsSurfaceGenerator`, `UMarsEnvironmentSubsystem`, `UMarsStationSubsystem`, `ASurfaceBeacon`, `ARadiationHazardZone`, `AMarsWaypointTrail`, `AMarsShuttle` | Station and surface generation, environment, navigation aids. |

## Station layout

`AMarsStationGenerator` builds the base as a closed ring with a maintenance shortcut:

```
 J-4 ---- [SEC-05 MED-BAY] ---- [SEC-06 STORAGE & COMM] ---- J-3
  |                                     ||                      |
RING CORRIDOR R-1               MAINTENANCE DUCT M-1      [SEC-04 XENOLAB]
  |                              (crouch to enter)               |
 J-1 ---- [SEC-02 EVA PREP] ---- [SEC-03 BIO-DOME] ----------- J-2
  |
[SEC-01 AIRLOCK] (viewport window -> shuttle pad) -> surface
```

SEC-07 Crew Quarters holds the bunk. The surface generator places three story points of interest at roughly 0.8, 1.1 and 1.4 km from the base: relay R-3, the ice-core drill rig and the ARES-III outpost.

## Key patterns

- **Game-time simulation.** Physical systems run in accelerated game time through `MarsTime` (default 60×: 1 real second = 1 game minute, 1 Sol = 24 real minutes). Rates in code are expressed per game hour.
- **Telemetry strings.** Modules and biometrics expose `GetTelemetryReport()`. These are the only facts A.R.I.A. sees. Hallucination intensity is deliberately left out.
- **Event delegates.** Modules broadcast `OnHullPunctured`, `OnHullBreached`, `OnDecompressionStarted`, `OnPressureCritical`, and so on. Biometrics broadcast stage changes, critical vitals, phantoms and flashes. UI, audio and A.R.I.A. subscribe to these.
- **Story reporting.** Gameplay code calls `UMarsNarrativeDirector::Report(this, "EventName")` to complete objectives. Some steps poll a world condition instead.
- **Interaction.** Stations implement `IMarsInteractable` with `Interact` (tap E) and `InteractHold` (hold E).
- **Saving.** `UMarsSaveGame` exports each saveable object's `UPROPERTY` values as text, plus inventory, story progress, logs and remaining pickups.

## Content and configuration

| Path | Purpose |
|---|---|
| `Config/DefaultEngine.ini` | Default map `AresStation`, game mode `MarsGameMode`, renderer settings for the PS1 look (no AA, motion blur, bloom, Lumen or VSM) |
| `Config/DefaultGame.ini` | Display title "ARES-VI", A.R.I.A. client config (model, effort, fallbacks) |
| `Config/DefaultInput.ini` | Default action/axis key mappings |
| `Content/Python/setup_mars_project.py` | Rebuilds the materials and the map. Run it again any time |
| `Content/MarsSurvival/Materials/` | `M_PS1_PostProcess`, `M_PS1_Surface`, `M_Glass`, `M_Phantom`, `MI_HullPanel`, `MI_MarsRegolith`, `MI_MarsRock` |
| `Content/UI/` | `ParadoxLogo`, `ParadoxTeamPoster` (source JPGs in `Content/UI/Source/`) |
