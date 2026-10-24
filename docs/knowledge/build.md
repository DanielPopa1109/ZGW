# Build configuration and binary artifacts

## Projects and build inputs

`ZGW/ZGW_APP/.project` and `.cproject` define an Eclipse/Infineon AURIX C project with generated build configurations. TASKING is represented by `TriCore Debug (TASKING)/` and `ZGW_APP TriCore Debug (TASKING).launch`; app and FBL each have TASKING and GNU linker command files (`Lcf_Tasking_Tricore_Tc.lsl`, `Lcf_Gnuc_Tricore_Tc.lsl`). The SCR has separate SDCC sources/configuration under `ZGW_APP/SCR` and generated `SCR Debug (SDCC)` output. `ZGW/ZGW_FBL/.project/.cproject` is an independent target.

Main application sources are under `APP`, project BSW under `BSW`, hardware/third-party support under `Libraries`. Generated makefiles, `*.o`, `*.d`, `*.src`, map/hex/elf outputs under debug directories are build products, not source. No standalone root-level build or automated firmware test harness was found in the inspected project structure; a working TASKING/Infineon environment is required for native build reproduction.

## Image post-processing

`ZGW_APP/makefile.targets` and `ZGW_FBL/makefile.targets` add `main-build` hooks that run `Scripts/Build/filter_ihex_ranges.ps1` over generated HEX and exclude UCB range `0xAF400000-0xAF405FFF`, yielding `*_edited.hex`. This is HEX record range filtering, not a flash operation. `Scripts/Build/validate_flash_artifacts.py` separately validates supplied ELF/HEX images, UCB/BMHD inclusion policy and overlapping combined HEX address conflicts. It accepts repeated `--elf` and `--hex` paths and exits nonzero on validation errors.

## Repository binary/data areas

`Binary/` contains prebuilt ELF, MAP and HEX images for ZGW and companion ECU names; these are inputs/artifacts, not source. Verify provenance and hashes before use. `DB/` contains DBC for CAN/CAN FD and LDF for LIN; firmware-generated COM configuration is the runtime authority. `SIM/DB` contains TSMaster-format bus databases. Build outputs do not establish that current source built them.

## Reproduction guidance

Inspect project `.cproject`, compiler include options and generated source lists for the chosen build configuration. Prefer IDE/toolchain-driven build in the matching AURIX environment. After build, check post-processing output and run the explicit artifact validator for the exact ELF/HEX set. Do not flash or change boot state merely to inspect/build these projects.
