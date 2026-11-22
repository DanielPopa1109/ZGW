# winIDEA debug workspace for ZGW (standalone)

ACS 1.0.24 has no winIDEA/iSYSTEM integration, so debugging uses winIDEA directly.
`ADS.xjrf` / `ADS.xgrf` are copies of `ZGW_APP/.ads/winIDEAWorkspaces/TriCore-Debug-(TASKING)/` with only the four file paths changed:

| Use | ADS workspace path | This workspace path (relative to this folder) |
|---|---|---|
| APP symbols | `../../../TriCore Debug (TASKING)/ZGW_APP.elf` | `../../../Binary/ZGW_APP.elf` |
| FBL symbols | `../../../../ZGW_FBL/TriCore Debug (TASKING)/ZGW_FBL.elf` | `../../../Binary/ZGW_FBL.elf` |
| APP download | `.../TriCore Debug (TASKING)/ZGW_APP.hex` | `../../ZGW_APP/TriCore Debug (TASKING)/ZGW_APP.hex` |
| FBL download | `.../TriCore Debug (TASKING)/ZGW_FBL.hex` | `../../ZGW_FBL/TriCore Debug (TASKING)/ZGW_FBL.hex` |

Open with `powershell -File open_winidea.ps1`. The ADS workspace is untouched.

## Not carried over (set manually in winIDEA if wanted)
The ADS launch file `ZGW_APP/ZGW_APP TriCore Debug (TASKING).launch` held settings that are not stored in the workspace: device TC375TP, hardware IFX_DAS over USB, after-download goto `0x80000000`, stop at `core0_main`, build-before-launch. In this workspace `AfterDownloadGoto` is empty. Check the hardware/derivative and download options on first open.

## Caveats
- Symbols come from `Binary/*.elf`, which are not rebuilt by `ZGW_APP_ACS` (its TASKING makefile is blocked by the toolset license). Refresh the ELF/HEX files from an ADS build before debugging, and make sure ELF and the hex that is downloaded come from the same build.
- The downloaded HEX is the full `ZGW_APP.hex`/`ZGW_FBL.hex` as in the ADS workspace (not the `_edited.hex` with the UCB range removed). That behaviour is unchanged from ADS; confirm it is intended before downloading.
- Opening the workspace does not touch hardware. Not tested with a board, and winIDEA was not launched in this check.
