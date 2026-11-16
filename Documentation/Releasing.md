# Releasing a Windows build

## 1. Package

From the project root:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\Package-Windows.ps1
```

Or double-click `Scripts\Package-Windows.bat`.

| Option | Default | Notes |
|---|---|---|
| `-EngineDir` | auto-detects `UE_5.3` under `D:\` or `C:\Program Files\Epic Games` | Folder that contains `Engine\` |
| `-GameName` | `ARES-IV` | Name of the folder, the zip and the launcher `.exe` |
| `-Configuration` | `Shipping` | Use `Development` for a test build with the console and logs |
| `-TempDir` | *(system temp)* | Redirects UE temp files, for example to `D:\Dev\Temp` when C: is full |
| `-NoZip` | off | Skip creating the zip |

The script runs Unreal Automation Tool (`BuildCookRun`) with `-build -cook -stage -pak -prereqs -archive` for Win64. It cooks only the `AresStation` map. Output:

```
Release/
├── ARES-IV/            the game (ARES-IV.exe, MarsSurvival/, Engine/)
└── ARES-IV-Win64.zip   upload this
```

`Release/` is in `.gitignore`. Builds are never committed.

## 2. Test checklist

Test the **zip**, not your working folder. Extract it somewhere new, ideally on a second PC without Unreal Engine:

- [ ] `ARES-IV.exe` starts, and the splash and main menu appear
- [ ] BEGIN NEW MISSION → briefing → suit training works
- [ ] Settings change and persist after restarting
- [ ] Save at a MISSION LOG terminal, quit, then CONTINUE
- [ ] Leave the station, board and fly the RS-2 shuttle
- [ ] Hallucinations and phantoms appear (the `MarsSetStage` console command doesn't exist in Shipping builds, so test it in a Development build)
- [ ] No missing-DLL errors (if there are, ship the bundled `UEPrereqSetup_x64.exe`, which `-prereqs` already includes)

## 3. Publish on GitHub

1. Tag the commit the build came from:
   ```
   git tag -a v0.1.0-demo -m "ARES-VI playable demo"
   git push origin v0.1.0-demo
   ```
2. On GitHub, go to **Releases → Draft a new release** and pick the tag.
3. Title: `ARES-VI — Playable Demo v0.1.0`. Paste the notes below and attach `ARES-IV-Win64.zip`.
4. Tick **Set as a pre-release** while it's a demo, then publish.

GitHub release assets can be up to 2 GB each. They don't use Git LFS storage.

### Release notes template

```markdown
## ARES-VI — Playable Demo

Hard sci-fi psychological survival horror on Mars, 2087. By Team Paradox.

### Install
1. Download **ARES-IV-Win64.zip** below.
2. Extract it, then run **ARES-IV.exe**.
Full guide: [INSTALL.md](https://github.com/Lit4alrt7/ARES-VI/blob/main/INSTALL.md)

### Requirements
Windows 10/11 64-bit, DirectX 11/12 GPU, ~2 GB disk.

### Known issues
- The build is not code-signed. Windows SmartScreen will ask you to confirm.
```
