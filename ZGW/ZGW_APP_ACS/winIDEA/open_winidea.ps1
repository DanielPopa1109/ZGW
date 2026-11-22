<#
Opens the ZGW debug workspace in standalone winIDEA (no ADS/Eclipse needed).
winIDEA is not part of ACS; it is taken from the ADS install (override with -WinIdea or $env:WINIDEA_EXE).
This only opens the workspace. It does not connect, download, flash or reset anything by itself:
connect, download and run from inside winIDEA after checking the loaded ELF/HEX paths.
#>
param(
    [string]$WinIdea = $(if ($env:WINIDEA_EXE) { $env:WINIDEA_EXE } else { 'C:\Infineon\AURIX-Studio-1.10.40\winIDEA\winIDEA.exe' })
)
$ErrorActionPreference = 'Stop'
$ws = Join-Path $PSScriptRoot 'ADS.xjrf'
if (-not (Test-Path $WinIdea)) { throw "winIDEA not found: $WinIdea (set -WinIdea or `$env:WINIDEA_EXE)" }
$missing = @(
    '..\..\..\Binary\ZGW_APP.elf', '..\..\..\Binary\ZGW_FBL.elf',
    '..\..\ZGW_APP\TriCore Debug (TASKING)\ZGW_APP.hex', '..\..\ZGW_FBL\TriCore Debug (TASKING)\ZGW_FBL.hex'
) | Where-Object { -not (Test-Path (Join-Path $PSScriptRoot $_)) }
if ($missing) { Write-Warning ("Workspace references missing files:`n  " + ($missing -join "`n  ")) }
Start-Process -FilePath $WinIdea -ArgumentList ('"{0}"' -f $ws)
