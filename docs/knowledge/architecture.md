# Firmware architecture and runtime

## Product boundary

`ZGW/ZGW_APP` is the main AURIX TriCore application: startup, multicore FreeRTOS, project BSW, automotive network communication, diagnostics, and gateway application. `ZGW/ZGW_FBL` is a separate bootloader/flash updater. `Libraries/iLLD`, Infineon SafetyKit material, FreeRTOS sources, and lwIP are dependencies/platform code, not gateway application logic. The `TriCore Debug (TASKING)` and `SCR Debug (SDCC)` subtrees contain generated object/dependency/build metadata and output files; do not use them as source-of-truth when corresponding source/configuration exists elsewhere.

The repository includes modules named in an AUTOSAR-like style (CanIf, PduR, Dcm, etc.); their presence does not establish AUTOSAR compliance. Read the local implementations and configuration.

## Startup sequence

`ZGW/ZGW_APP/Cpu0_Main.c::Core0_InitSequence` initializes Ethernet startup timing, CPU/safety watchdogs, SCR state and image, software safety checks, MPU/safety kit, then starts other cores. It restores retained time, initializes pins, GTM TOM and `TimeBase`, initializes CAN/CANIF/PduR/COM/CANTP/CANSM/diagnostics/LIN/NM/ComM, then DEM/NvM and application components (`CodingApp`, `ParallelFlashSwc`, `GatewaySwc`), initializes OS core 0 and starts its scheduler. Core 1 waits for `OsInit_C0`, initializes its OS and scheduler (`Cpu1_Main.c`). Core 2 waits for core 1, initializes RMII pins and OS, then starts its scheduler (`Cpu2_Main.c`). The core-2 Ethernet BSW task captures startup timing and calls `EthStack_Init` before its periodic Ethernet services (`Os.c`). These waits are synchronization gates, not proof that network link or every subsystem is ready.

NvM recovery is intentionally deferred from core-0 startup: `Os.c::Os_NvMStartup_MainFunction` coordinates ReadAll, DEM restoration, operation-cycle startup and deferred Fee initialization in the ASIL_NVM task.

## Scheduling and core ownership

`ZGW/ZGW_APP/BSW/Sys/Os/Os.c` creates the FreeRTOS tasks and is the authoritative schedule map. The application has separate FreeRTOS instances/ports per core under `BSW/Sys/FreeRTOS_core{0,1,2}`.

- Core 0: QM application invokes `GatewaySwc_MainFunction`, `CodingApp_MainFunction`, `ParallelFlashSwc_MainFunction`, requested COM/LIN transmit work and `TimeBase_MainFunction`; separate tasks service CAN, LIN, diagnostics, COM/NM, safety and NvM.
- Core 1: `AiModel_MainFunction` is in QM application task.
- Core 2: Ethernet/DoIP/SOME-IP/SD and `GatewaySwc_EthernetMainFunction` run in Ethernet BSW task. gPTP lab main is called from this task, but feature enable/configuration must be checked separately.

Main task period and subsystem timing are set in `Os.c`, module headers, and configuration. Avoid inferring a deadline only from a function name or timer callback; verify task loop/tick and feature enable switches.

## Data flow overview

Bus drivers/low-level controllers feed CAN/LIN interfaces and COM signal buffers. `GatewaySwc` periodically reads and forwards selected signal ranges/routes, generates/interprets gateway commands, tracks RX timeouts/invalid values and reports diagnostic transitions. Ethernet service receives are dispatched through SoAd and protocol handlers, then PduR/DCM/DoIP bridge for diagnostics. COM and diagnostic transmit paths return through PduR to the selected lower interface. `ParallelFlashSwc` forwards tester UDS requests to configured downstream ECU targets; the local FBL is a separate image and programming endpoint.

TimeBase is a monotonic vehicle-time counter plus optional UTC mapping. UDS routine, NvM, SCR standby record and UDP publication are separate interfaces. AI prediction consumes COM data through `AiModel` on core 1; deterministic protection logic is distinct from the model. See topic pages for detail.

## Platform/build dependencies

TriCore project sources use Infineon iLLD, linker command files, safety support and core-specific FreeRTOS. Ethernet uses project GETH integration plus lwIP. The source workspaces are Eclipse/Infineon AURIX projects (`.project`, `.cproject`) configured around TASKING; generated makefiles/launch data are under `TriCore Debug (TASKING)`. Linker files are `ZGW/ZGW_APP/Lcf_Tasking_Tricore_Tc.lsl` and corresponding GNU linker script. The SCR has a separate SDCC build subtree. See [build](build.md).

## Evidence limits

This tree documents software paths and configured behavior, not bench/vehicle operation, timing performance, safety qualification, flash endurance, or production validity. Existing HEX/ELF/map files are build outputs and do not independently identify which source revision produced them.

