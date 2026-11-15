@echo off
rem Opens MarsSurvival in UE 5.3, with temp files and shader cache kept on D: (drive C: is full)
set TEMP=D:\Dev\Temp
set TMP=D:\Dev\Temp
set UE-LocalDataCachePath=D:\Dev\DDC
start "" "D:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor.exe" "D:\Dev\MarsSurvival\MarsSurvival.uproject"