@echo off
rem Starts MarsSurvival directly as a game (no editor). Temp/cache files go to D: because C: was full.
set TEMP=D:\Dev\Temp
set TMP=D:\Dev\Temp
set UE-LocalDataCachePath=D:\Dev\DDC
start "" "D:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor.exe" "D:\Dev\MarsSurvival\MarsSurvival.uproject" /Game/MarsSurvival/Maps/AresStation -game -windowed -resx=1600 -resy=900