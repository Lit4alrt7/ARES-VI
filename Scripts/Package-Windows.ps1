<#
.SYNOPSIS
    Builds, cooks and packages ARES-VI as a standalone Windows 64-bit game and zips it for a GitHub Release.

.DESCRIPTION
    Wraps Unreal Automation Tool (RunUAT BuildCookRun). Output:
        Release\<GameName>\<GameName>.exe   (+ engine/game content, prerequisites installer)
        Release\<GameName>-Win64.zip
    Nothing outside Release\ (and the usual Binaries\, Intermediate\, Saved\) is touched.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\Package-Windows.ps1
    powershell -ExecutionPolicy Bypass -File Scripts\Package-Windows.ps1 -EngineDir "C:\Program Files\Epic Games\UE_5.3" -Configuration Development
#>
[CmdletBinding()]
param(
    # Unreal Engine 5.3 install folder (the one that contains Engine\).
    [string]$EngineDir = "",

    # Name of the release folder, the zip and the launcher .exe players double-click.
    [string]$GameName = "ARES-IV",

    # Shipping for releases; Development keeps the console and logs for testing.
    [ValidateSet("Shipping", "Development")]
    [string]$Configuration = "Shipping",

    # Optional folder for UE temp files and the shader cache (useful when drive C: is low on space).
    [string]$TempDir = "",

    # Skip creating the zip.
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project     = Join-Path $ProjectRoot "MarsSurvival.uproject"
$ReleaseDir  = Join-Path $ProjectRoot "Release"
$StagingDir  = Join-Path $ReleaseDir "_staging"
$GameDir     = Join-Path $ReleaseDir $GameName
$ZipPath     = Join-Path $ReleaseDir "$GameName-Win64.zip"
$Map         = "/Game/MarsSurvival/Maps/AresStation"

function Fail($Message) { Write-Host "ERROR: $Message" -ForegroundColor Red; exit 1 }

# ---- Locate the engine -------------------------------------------------------
if (-not $EngineDir) {
    $candidates = @(
        "D:\Program Files\Epic Games\UE_5.3",
        "C:\Program Files\Epic Games\UE_5.3",
        "$env:ProgramFiles\Epic Games\UE_5.3"
    )
    $EngineDir = $candidates | Where-Object { Test-Path (Join-Path $_ "Engine\Build\BatchFiles\RunUAT.bat") } | Select-Object -First 1
}
if (-not $EngineDir) { Fail "Unreal Engine 5.3 not found. Pass -EngineDir `"<path to UE_5.3>`"." }
$RunUAT = Join-Path $EngineDir "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path $RunUAT)) { Fail "RunUAT.bat not found under $EngineDir." }
if (-not (Test-Path $Project)) { Fail "MarsSurvival.uproject not found at $Project." }

if ($TempDir) {
    New-Item -ItemType Directory -Force $TempDir | Out-Null
    $env:TEMP = $TempDir
    $env:TMP  = $TempDir
}

Write-Host "Engine:        $EngineDir"
Write-Host "Project:       $Project"
Write-Host "Configuration: $Configuration"
Write-Host "Output:        $GameDir"
Write-Host ""

# ---- Clean previous output (only inside Release\) ----------------------------
foreach ($path in @($StagingDir, $GameDir, $ZipPath)) {
    if (Test-Path $path) { Remove-Item -Recurse -Force $path }
}
New-Item -ItemType Directory -Force $ReleaseDir | Out-Null

# ---- Build, cook, stage, pak, archive ----------------------------------------
$uatArgs = @(
    "BuildCookRun",
    "-project=`"$Project`"",
    "-noP4",
    "-utf8output",
    "-platform=Win64",
    "-clientconfig=$Configuration",
    "-build",
    "-cook",
    "-map=$Map",
    "-stage",
    "-pak",
    "-prereqs",
    "-archive",
    "-archivedirectory=`"$StagingDir`"",
    "-unattended"
)
if ($Configuration -eq "Shipping") { $uatArgs += "-nodebuginfo" }

& $RunUAT @uatArgs
if ($LASTEXITCODE -ne 0) { Fail "BuildCookRun failed with exit code $LASTEXITCODE. See the log above." }

# ---- Rename the archive to the player-facing name ----------------------------
$archived = Join-Path $StagingDir "Windows"
if (-not (Test-Path $archived)) { Fail "Expected archive folder not found: $archived" }
Move-Item $archived $GameDir
Remove-Item -Recurse -Force $StagingDir

# The root .exe is a small bootstrap that launches MarsSurvival\Binaries\Win64\...; it can be renamed freely.
$bootstrap = Join-Path $GameDir "MarsSurvival.exe"
if (Test-Path $bootstrap) {
    Rename-Item $bootstrap "$GameName.exe"
} else {
    Write-Warning "MarsSurvival.exe not found in the archive; the launcher was not renamed."
}

# ---- Zip ---------------------------------------------------------------------
if (-not $NoZip) {
    Write-Host "Creating $ZipPath ..."
    Compress-Archive -Path $GameDir -DestinationPath $ZipPath -CompressionLevel Optimal
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "  Game folder: $GameDir"
if (-not $NoZip) {
    $sizeMb = [math]::Round((Get-Item $ZipPath).Length / 1MB, 1)
    Write-Host "  Zip:         $ZipPath ($sizeMb MB)"
}
Write-Host "Play-test $GameName.exe before uploading it to a GitHub Release."
