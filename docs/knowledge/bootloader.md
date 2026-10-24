# Bootloader and flash update

## Separate firmware target

`ZGW/ZGW_FBL` is a standalone Eclipse/Infineon project and image, separate from `ZGW_APP`. Core-0 implementation is concentrated in `Cpu0_Main.c`; `Cpu1_Main.c`/`Cpu2_Main.c` are minimal core entry points. Ethernet/lwIP adapter is `FblEth_AurixLwip.c`; transport sizing is `FblTransport_Cfg.h`; RAM-resident flash/runtime support is `Fbl_RamUpdater_AURIX.c`; update state record/layout is `FblBlu.h`. FBL has its own linker scripts, GETH/lwIP port, timer and iLLD dependencies.

## Runtime and interfaces

Core 0 startup restores/validates retained boot/update state, initializes platform/CAN acknowledgement and Ethernet, then runs the bootloader request loop. DoIP discovery/routing activation and diagnostic payload handling are implemented locally in `Cpu0_Main.c`; CAN and Ethernet transport identifiers/selectors are defined there and in FBL config. `FblEth_AurixLwip.c` supplies TCP/UDP sockets, stream assembly and callbacks. Maximum configured UDS TransferData data is 32,768 bytes plus 2-byte UDS overhead; DoIP/TCP buffers are sized for a complete maximum frame (`FblTransport_Cfg.h`).

`Fbl_UdsHandle` in `Cpu0_Main.c` dispatches supported UDS services and routes erase/program/verify/activation flows to RAM-resident helpers. `FblRamFlash_EraseRange`, `ProgramPage` and related functions execute direct PFLASH operations from `Fbl_RamUpdater_AURIX.c`. The bootloader also contains RAM closure/trap handling, retained SCR/Blu state and recovery-state logic. Read the switch and state transitions in source before relying on any service, security policy, image layout or rollback behavior.

## Update-state caveat

Source implements destructive flash programming paths. Presence of dual-bank terminology, state records, image verification, or generated files is not sufficient evidence of safe A/B update, atomic activation, rollback, power-loss recovery, or compatibility with another tool's package format. Verify bank selection, address map, authenticity/integrity checks, activation and reset/boot confirmation end to end for the exact image and target. FBL changes are safety-critical and must be validated on a controlled target before deployment.

## Debug visibility

FBL exports many volatile diagnostics for transfer sizing, TCP buffering, flash commands, errors, phase timing and retained-state rejection in `Cpu0_Main.c` and `FblBlu.h`. Use them to find instrumentation, but do not equate counters with proof of successful complete vehicle update.
