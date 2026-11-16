# How to install and play ARES-VI

You don't need Unreal Engine or any developer tools to play the demo.

## System requirements

| | Minimum |
|---|---|
| OS | Windows 10 or 11, 64-bit |
| Graphics | DirectX 11 or 12 capable GPU |
| Disk space | About 500 MB free (download ~180 MB, ~280 MB unpacked) |
| Input | Keyboard and mouse |

## Install

1. Go to the **[Releases](https://github.com/Lit4alrt7/ARES-VI/releases)** page.
2. Under the latest release, open **Assets** and download **`ARES-IV-Win64.zip`**.
   (Don't download "Source code". That's the project for developers, not the game.)
3. Right-click the zip and choose **Extract All…**. Pick a normal folder, such as `Documents\Games`.
   Don't run the game from inside the zip. It won't find its files.
4. Open the extracted `ARES-IV` folder and double-click **`ARES-IV.exe`**.

### If Windows warns you

- **"Windows protected your PC" (SmartScreen):** the demo isn't code-signed. Click **More info → Run anyway**.
- **Missing DLL error (for example `VCRUNTIME140.dll`):** run the installer in `ARES-IV\Engine\Extras\Redist\en-us\` (`UEPrereqSetup_x64.exe`), then start the game again.
- **DirectX error on start:** update your graphics driver.

## First launch

- The game opens on the main menu. Choose **BEGIN NEW MISSION**.
- An archival briefing plays first. A short suit-training card then teaches the basics.
- Change keys, mouse sensitivity, FOV, audio and resolution under **SETTINGS & CONTROLS**.

## Basic controls

| Key | Action |
|---|---|
| W A S D / Mouse | Move / look |
| Space / Shift / C | Jump / sprint / crouch |
| E | Interact (hold for longer actions) |
| Left / Right mouse | Use tool or fire / secondary action |
| 1 – 4 | Pistol, weld gun, torque wrench, dosimeter |
| 5 / 6 | Drink / eat |
| H | Med-kit (tap to select, hold to inject) |
| Q | O2 canister |
| T | Talk to A.R.I.A. |
| G / N / U | Wrist computer / waypoints / minimal HUD |
| Esc | Pause menu |

The full list, including shuttle controls, is in the [README](README.md#controls).

## Saves and settings

- The game saves automatically every few minutes while you're inside the station. You can also save at any **MISSION LOG** terminal.
- Saves and settings are stored in `%LOCALAPPDATA%\MarsSurvival\Saved\`. Delete that folder to reset everything.

## Uninstall

Delete the `ARES-IV` folder. To also remove saves and settings, delete `%LOCALAPPDATA%\MarsSurvival\`.

## Reporting a problem

Open an issue on the [Issues](https://github.com/Lit4alrt7/ARES-VI/issues) page. Describe what you were doing, what happened, your Windows version and your graphics card. If the game crashed, attach any crash folder from `%LOCALAPPDATA%\MarsSurvival\Saved\Crashes\`.
