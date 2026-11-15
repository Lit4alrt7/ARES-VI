# Contributing to ARES-VI

Thanks for your interest in the project. This guide explains how to set up, make changes and submit them.

## Before you start

1. Follow the setup steps in the [README](README.md#installation-and-setup). You need UE 5.3, Visual Studio 2022 with MSVC 14.44.35207, and Git LFS.
2. Read [Documentation/Architecture.md](Documentation/Architecture.md) to see where things live.
3. For larger changes, open an issue first so the approach can be discussed.

## Workflow

1. Fork the repository and create a branch from `main`:
   `feature/<short-name>`, `fix/<short-name>` or `docs/<short-name>`.
2. Make your change. Keep each pull request focused on one thing.
3. Build **Development Editor | Win64** with no errors and no new warnings.
4. Play through the part of the game you touched: at minimum, start a new mission and reach the affected system.
5. Open a pull request that describes:
   - what changed and why,
   - how you tested it,
   - screenshots or a short clip for visual or gameplay changes.

## Code conventions

The project is C++ only. There is no Blueprint gameplay logic, and the world is generated in code. Please keep it that way.

- Follow the [Unreal Engine coding standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine): `A`/`U`/`F`/`E`/`I` prefixes, PascalCase, `b` prefix for booleans.
- Put new code in the matching folder under `Source/MarsSurvival/` (`Base/`, `Player/`, `Stations/`, …). Includes are module-relative, for example `#include "Base/MarsBaseModule.h"`.
- Log with the `LogMarsSurvival` category.
- **Units are part of the name**: `PressureKpa`, `TemperatureC`, `DoseMsv`, `LeakAreaCm2`, `RatePerHour`. Keep them consistent.
- **Simulation runs in game time.** Convert real deltas with `MarsTime::ToGameHours()` instead of hard-coding 60×.
- Expose tuning values as `UPROPERTY(EditAnywhere)` with a one-line comment that gives the unit and meaning.
- Physiology and habitat numbers should be plausible. If you change a model, explain the reasoning in a comment.
- New saveable state: make sure `UMarsSaveGame` picks it up, and test save → load.
- New story beats: report them with `UMarsNarrativeDirector::Report(this, TEXT("EventName"))`.

## Assets

- `.uasset` and `.umap` files are stored with **Git LFS** (see `.gitattributes`). Run `git lfs install` before you commit.
- Binary assets cannot be merged. Coordinate before editing `AresStation.umap`, because two people changing it means one person's work is lost.
- Only add assets you have the right to distribute (your own work, or CC0 / compatible licences). Name the source in your PR.
- Do not commit `Binaries/`, `Intermediate/`, `Saved/` or `DerivedDataCache/`.

## Secrets

**Never commit an API key.** A.R.I.A. reads `ANTHROPIC_API_KEY` from the environment at runtime. Don't put keys in `.ini` files, source or assets.

## Reporting bugs

Open an issue that includes:

- what you did, what you expected and what happened,
- the Sol, chapter and disease stage if relevant,
- the relevant lines from `Saved/Logs/MarsSurvival.log` (search for `LogMarsSurvival`),
- your engine version and OS.
