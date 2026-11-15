# Developer tools

## Console commands

Open the console with the backtick key (`` ` ``) during play.

| Command | Effect |
|---|---|
| `MarsSetStage 0-4` | Jump the disease to the middle of a stage, to demo the hallucinations |
| `MarsEquip 1-6` | Use a belt slot |
| `MarsTurn <yaw>` | Set the view yaw in degrees |
| `MarsSpawnPhantom` | Project a phantom 7 m ahead, to demo the ballistic awakening |
| `MarsShowScan` | Open the visual Med-Bay diagnostic panel |

## Launch flags

Pass these on the command line, for example in `Play_MarsSurvival.bat` or **Additional Launch Parameters** in the editor.

| Flag | Effect |
|---|---|
| `-NoBriefing` | Skip the intro briefing |
| `-StartAt=<n>` | Start at a given point (read by `AMarsGameMode`) |
| `-AutoShot=<t1>,<t2>,…` | Take in-engine screenshots at the given times (saved to `Saved/Screenshots`) |
| `-AutoSpin` | Rotate the view while auto-shooting |
| `-AutoPauseAt=<t>` | Open the pause menu after `t` seconds |
| `-OpenSettingsAt=<t>` | Open the settings panel on the main menu after `t` seconds |
| `-AutoEquip=<slot>` | Equip a belt slot on spawn |
| `-AutoBoard` | Board the recon shuttle on spawn |
| `-AutoFly` | Make the piloted shuttle fly itself |
| `-AutoWrist` | Raise the wrist computer on spawn |

These flags exist for screenshots and demos. Read the code that handles each one (search for `FParse::` in `Source/`) before relying on exact behaviour.

## Python bootstrap

`Content/Python/setup_mars_project.py` runs inside the editor and creates:

- `M_PS1_PostProcess`: pixelation, Bayer dither, palette quantisation, CRT scanlines and aperture mask, chromatic aberration, hallucination warp, Cherenkov flash, hypoxia grey-out
- `M_PS1_Surface`: vertex snapping ("jitter") and affine texture warping
- `MI_HullPanel`, `MI_MarsRegolith`, `MI_MarsRock`
- `M_Phantom`: translucent hallucination silhouette
- the `AresStation` map

You can run it from the editor:

**Tools → Execute Python Script…** → `Content/Python/setup_mars_project.py`

Or headless:

```powershell
UnrealEditor-Cmd.exe MarsSurvival.uproject -run=pythonscript -script="<path>\Content\Python\setup_mars_project.py" -unattended -nop4 -nosplash
```

Running it again rebuilds existing assets in place and recreates the map.

## Logging

Everything game-specific logs under the `LogMarsSurvival` category. Logs are written to `Saved/Logs/`.

## Toolchain note

Both `MarsSurvival.Target.cs` and `MarsSurvivalEditor.Target.cs` pin:

```csharp
WindowsPlatform.CompilerVersion = "14.44.35207";
```

UE 5.3 does not support MSVC 14.5x (VS 2026). Install the 14.44 toolset through the Visual Studio Installer, or change this line to a 14.3x/14.4x toolset you have.
