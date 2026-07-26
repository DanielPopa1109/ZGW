# BLU RAM Runtime Audit

## Post-0x0155 Ethernet Static-Function Closure

Observed defect: after `31 01 01 55 -> 71 01 01 55`, the BLU state was `RAM_READY` for the FBL target, but the next FBL erase request reached PC `0x80000160`, resolved by the debugger as `lwip_geth_CacheInvalidateRange()`. That is a PF0 fetch during the post-transition communication path.

Exact emitted section before this correction: the generated TASKING source metadata contains `.sdecl '.text.lwip_geth_netif.lwip_geth_CacheInvalidateRange'` and `.sect '.text.lwip_geth_netif.lwip_geth_CacheInvalidateRange'` in `TriCore Debug (TASKING)/LWIP_GETH/port/src/lwip_geth_netif.src`. The current map also shows `lwip_geth_netif.src:lwip_geth_CacheInvalidateRange` reachable from `lwip_geth_low_level_input`.

Static helpers relocated by source attribute: `lwip_geth_AssertFail`, `lwip_geth_CopyBytes`, `lwip_geth_IsExpectedRxDescriptorAddress`, `lwip_geth_GetRxDescriptorIndex`, `lwip_geth_GetValidatedRxDescriptor`, `lwip_geth_PrepareRxHandle`, `lwip_geth_IsDmaNonCachedRange`, `lwip_geth_IsRxBufferValid`, `lwip_geth_RestoreRxDescriptorBuffer`, `lwip_geth_CacheInvalidateRange`, `lwip_geth_CacheWritebackInvalidateRange`, `lwip_geth_FreeReceiveDescriptor`, `lwip_geth_WaitTransmitBufferBounded`, `lwip_geth_SendSingleTransmitBuffer`, `lwip_geth_low_level_output`, `lwip_geth_GetRxFrameSize`, `lwip_geth_low_level_input`, `lwip_geth_netif_input_once`, `lwip_geth_netif_input`, and `lwip_geth_RamClosureIsValid` now use `FBL_RAM_ETH_CODE`.

Post-transition byte-copy dependency: `lwip_geth_low_level_output()` and `lwip_geth_low_level_input()` no longer call `memcpy()` for packet payload movement. They use `lwip_geth_CopyBytes()` in `.ram_eth_code`.

Project-local GETH runtime helpers now replace the post-`0x0155` iLLD GETH call chain: `FblRamGeth_FreeReceiveBuffer`, `FblRamGeth_ShuffleRxDescriptor`, `FblRamGeth_GetTransmitBuffer`, `FblRamGeth_WakeupReceiver`, `FblRamGeth_WakeupTransmitter`, and `FblRamGeth_SetLineSpeed` are implemented with `FBL_RAM_ETH_CODE`. The original `IfxGeth_Eth_freeReceiveBuffer`, `IfxGeth_Eth_shuffleRxDescriptor`, `IfxGeth_Eth_getTransmitBuffer`, `IfxGeth_Eth_wakeupReceiver`, `IfxGeth_Eth_wakeupTransmitter`, and `IfxGeth_mac_setLineSpeed` are no longer called from `lwip_geth_netif.c` or `lwip_geth_lwip.c` and are no longer selected into `code_eth_runtime_ram`.

Initialization-only GETH helpers removed from the RAM selector set: `IfxGeth_enableModule`, `IfxGeth_getSrcPointer`, `IfxGeth_mac_setMacAddress`, `IfxGeth_mac_setMaxPacketSize`, `IfxGeth_mtl_clearAllInterruptFlags`, `IfxGeth_mtl_clearInterruptFlag`, `IfxGeth_mtl_isInterruptFlagSet`, `IfxGeth_resetModule`, and the `IfxGeth_Eth_*` configure/init/setup/start/stop/shuffle initialization helpers. Header-inline GETH helpers used by the runtime path are emitted into their relocated callers.

The broad `.text.lwip_geth_netif.*` RAM selector was removed. The netif static closure is now selected through `FBL_RAM_ETH_CODE` and `(.ram_eth_code|.ram_eth_code.*)`, while initialization-only netif helpers remain outside the post-`0x0155` RAM closure.

Runtime breadcrumb result: `g_FblRuntimeClosureFailStep = 0x7D9` and `g_LwipGethRuntimeClosureFailStep = 9` decoded to lwIP/GETH closure sub-step 9, `lwip_geth_CacheInvalidateRange`. To harden TASKING placement, the active LSL now also selects the exact emitted `.text.lwip_geth_netif.lwip_geth_*` static runtime-helper sections individually. The broad `.text.lwip_geth_netif.*` selector remains absent.

Runtime closure guard: `lwip_geth_RamClosureIsValid()` validates the netif module's static RX/TX/cache helpers and the project-local `FblRamGeth_*` helpers actually called by the post-transition path. `FblEth_RuntimeClosureOk()` now calls this guard and validates the active FBL Ethernet callback targets. `Cpu0_Main.c` re-runs `Fbl_RuntimeClosureOk()` immediately before accepting FBL erase `RoutineControl 0x0001`; failure sets `FBL_BLU_FAILURE_RAM_CLOSURE` and rejects the erase before PFLASH erasure begins.

Constants/data: the post-transition RX/TX helpers use macro constants, existing RAM DMA descriptors/buffers, lwIP RAM pools, exact lwIP live-rodata selectors, and mutable FBL Ethernet state. No new constant object was identified for `LWIP_GETH_CACHE_LINE_SIZE`; it is a preprocessor constant. The active option file now carries both `-Wl--map-file=ZGW_FBL.map` and `-Wl--map-file-format=+statics`.

Map address status: requires a new TASKING link. The current map is stale relative to these edits and still records the pre-fix `.text.lwip_geth_netif.*` placement. The next map must show the relocated static helpers in `.ram_eth_code`/RAM and must not show any post-`0x0155` RX/TX/cache callback target in `0x80000000..0x802FFFFF` or `0xA0000000..0xA02FFFFF`.

Remaining ECU test requirements: confirm startup reaches `core0_main`, `0x0200` and `0x0155` succeed, FBL erase `0x0001` starts, all observed PCs during erase and communication polling remain in RAM, repeated `0x78` responses reach FCD, and no PC resolves to `lwip_geth_CacheInvalidateRange` at `0x800...`.

## GETH Descriptor Closure Correction

Observed defect after PF0 erase: `lwip_geth_FreeReceiveDescriptor()` called `IfxGeth_Eth_freeReceiveBuffer()` in RAM, but that original iLLD function called `IfxGeth_Eth_shuffleRxDescriptor()` at `0x80013A58`. The trap captured `g_FblRamRuntimeLastAddress = 0xA0010000`, then the previous RAM trap path requested a system reset. That was unsafe because PF0 had already been erased.

Replacement call chain:

`lwip_geth_FreeReceiveDescriptor() -> FblRamGeth_FreeReceiveBuffer() -> FblRamGeth_ShuffleRxDescriptor()`

`FblRamGeth_FreeReceiveBuffer()` mirrors the iLLD descriptor ownership update: it writes RDES3 with `BUF1V = 1`, `BUF2V = 0`, `IOC = 0`, and `OWN = 1`. `FblRamGeth_ShuffleRxDescriptor()` advances `rxDescrPtr` to the next descriptor and wraps from `descr[IFXGETH_MAX_RX_DESCRIPTORS - 1]` to `descr[0]`. RX release then writes back/invalidates the descriptor and calls `FblRamGeth_WakeupReceiver()`.

TX and line-speed post-runtime dependencies were also replaced: `FblRamGeth_GetTransmitBuffer()` checks the current TX descriptor ownership and returns TDES0 only when software owns the descriptor; `FblRamGeth_WakeupTransmitter()` performs the channel-0 DMA/MTL wakeup sequence directly through GETH registers; `FblRamGeth_SetLineSpeed()` updates MAC `PS/FES` directly for the supported speeds. These helpers use no memcpy/memset/compiler helper and do not call the original iLLD GETH functions.

Destructive-phase trap behavior now uses `g_FblRamRuntimeDestructivePhase`. FBL erase sets this flag before the first PF0 erase. If a trap occurs while the flag is set, the RAM trap vector records trap class in `g_FblRamRuntimeTrapClass`, TIN in `g_FblRamRuntimeTrapTin`, and `CPU_DEADD` in `g_FblRamRuntimeLastAddress`, then calls `Fbl_BluEnterRecoveryWaitFromTrap()`, disables interrupts, and stays in a RAM-only loop. It does not call `FblRam_RequestSystemReset()` in the destructive phase. The flag is cleared only after the FBL image reaches `FBL_BLU_STATE_COMPLETE`, before the final drained reset path.

Current source/static validation:

- `rg` found no remaining `IfxGeth_Eth_freeReceiveBuffer`, `IfxGeth_Eth_shuffleRxDescriptor`, `IfxGeth_Eth_getTransmitBuffer`, `IfxGeth_Eth_wakeupReceiver`, `IfxGeth_Eth_wakeupTransmitter`, or `IfxGeth_mac_setLineSpeed` references in `lwip_geth_netif.c`, `lwip_geth_lwip.c`, or the FBL RAM linker group.
- `Select-String` found no exact `select ".text.IfxGeth...` or `select ".text.IfxGeth_Eth...` entries in `code_eth_runtime_ram`.
- `git diff --check` reported no whitespace errors for the touched files; only CRLF normalization warnings.

Required next TASKING/disassembly proof is still pending: new map addresses for `FblRamGeth_FreeReceiveBuffer`, `FblRamGeth_ShuffleRxDescriptor`, and `lwip_geth_FreeReceiveDescriptor`; generated disassembly proving none of those functions call `IfxGeth_Eth_freeReceiveBuffer`, `IfxGeth_Eth_shuffleRxDescriptor`, `0x800...`, or `0xA000...`; and ECU runtime confirmation that `0x0200 -> 0x0155 -> 0x0001` completes without executing `IfxGeth_Eth_shuffleRxDescriptor` at `0x80013A58`.

## Project-Local RAM Helper Isolation

Stated architecture:

- Statically proven: original Infineon startup, iLLD and TASKING/compiler-library helper symbols are not selected into BLU RAM by broad linker patterns.
- Statically proven: startup continues to use original PFLASH implementations selected by `code_startup_pflash` or by the normal PFLASH catch-all.
- Statically proven: destructive BLU runtime has project-local helpers with unique `FblRam_*`/`FblRamFlash_*` names.
- Requires TASKING map: final symbol addresses for every helper after the next link.
- Requires ECU runtime test: the erase/program/reset sequence on TC375 hardware.

Original helpers retained in PFLASH:

- Statically proven by linker selectors: no RAM group selects broad startup/helper families `.text.IfxCpu.*`, `.text.IfxCpu_Irq.*`, `.text.IfxCpu_Trap.*`, `.text.IfxScuCcu.*`, `.text.IfxScuRcu.*`, `.text.IfxScuWdt.*`, `.text.IfxFlash.*`, `.text.CompilerTasking.*`, `.text._*`, `.text.memcpy.*`, `.text.memset.*`, `.text.memmove.*`, or `.text.memcmp.*`.
- Statically proven by linker selectors/source attributes: Ethernet retains exact GETH/iLLD runtime dependencies and project-owned static RX/TX/cache helpers in RAM under `code_eth_runtime_ram`; no broad `.text.IfxGeth.*` or `.text.IfxGeth_Eth.*` selector is used.
- Requires TASKING map: `Ifx_C_Init`, `_c_init`, `_c_init_entry`, `_ldmst_clear_byte`, `_ldmst_copy_byte`, `IfxScuCcu_*`, `IfxScuRcu_*`, `IfxScuWdt_*`, `memcpy`, `memset`, `memmove`, and `memcmp` all remain at `0x800...`.

Project-local RAM helpers added:

- Statically proven source placement: `FblRam_DisableInterrupts()`, `FblRam_RestoreInterrupts()`, `FblRam_InvalidateProgramCache()`, `FblRam_GetCpuWatchdogPassword()`, `FblRam_ClearCpuEndinit()`, `FblRam_SetCpuEndinit()`, `FblRam_GetSafetyWatchdogPassword()`, `FblRam_ClearSafetyEndinit()`, `FblRam_SetSafetyEndinit()`, `FblRam_RequestSystemReset()`, `FblRam_CopyBytes()`, `FblRam_SetBytes()`, `FblRam_MoveBytes()`, and `FblRam_CompareBytes()` are implemented in `Fbl_RamUpdater_AURIX.c` under `.ram_blu_helpers`.
- Statically proven source placement: low-level flash command helpers `FblRamFlash_EnterPageMode()`, `FblRamFlash_Load2X32()`, and `FblRamFlash_WritePage()` are under `.ram_flash_helpers`; the public flash primitives remain under `.ram_flash_code`.
- Behavioral references inspected: `IfxScuWdt.h` inline ENDINIT/password sequences, `IfxScuRcu.c::IfxScuRcu_performReset()`, `IfxCpu.h::IfxCpu_disableInterrupts()` and `IfxCpu_invalidateProgramCache()`, and `IfxFlash.h` command primitives.

Post-erase call sites replaced:

- Statically proven: `FblRamRuntime_EnterCritical()` now uses `FblRam_DisableInterrupts()` and restores the saved interrupt state on guard failure.
- Statically proven: `FblRamRuntime_RequestReset()` delegates to `FblRam_RequestSystemReset()` and no longer calls `IfxScuWdt_*`.
- Statically proven: `FblRamFlash_EraseRange()` and `FblRamFlash_ProgramPage()` use `FblRam_GetSafetyWatchdogPassword()`, `FblRam_ClearSafetyEndinit()`, and `FblRam_SetSafetyEndinit()` instead of `IfxScuWdt_*`.
- Statically proven: `Fbl_FlashEraseRange()`, CRC verification, transfer-exit verification, boot-critical commit, and app jump cache invalidation use `FblRam_InvalidateProgramCache()` instead of `IfxCpu_invalidateProgramCache()`.
- Statically proven: BLU SCR framing, page-buffer initialization, DoIP response padding, diagnostic payload copy, and flash page-buffer refill use `FblRam_CopyBytes()`/`FblRam_SetBytes()` instead of local wrappers or standard library calls.

Linker placement:

- Statically proven: `code_blu_helpers_ram` selects only `(.ram_blu_helpers|.ram_blu_helpers.*)` and `(.ram_flash_helpers|.ram_flash_helpers.*)` into PSPR0 with `copy`.
- Statically proven: broad helper-family RAM selectors were removed permanently, including `.text.IfxFlash.*`, `.text.IfxGeth.*`, and `.text.IfxGeth_Eth.*`. Required GETH dependencies are selected only by exact function-section names in `code_eth_runtime_ram`.
- Requires TASKING map: `FblRam_*`, `FblRamFlash_*`, and BLU trap helpers resolve to acceptable RAM ranges and do not overlap stack, CSA, heap, Ethernet DMA descriptors, lwIP pools, or transfer buffers.

Hidden compiler-helper audit:

- Statically proven: the RAM byte helpers are byte loops with no standard-library calls, no recursion, no dynamic allocation, no structure assignment, and no diagnostic strings.
- Statically proven: RAM flash helpers use direct volatile SFR/DMU command writes and no `IfxFlash_*` calls.
- Requires TASKING map/assembly: compiler did not transform RAM helper loops into `memcpy`, `memset`, `memmove`, `memcmp`, `_ldmst_*`, division, modulo, stack-check, profiling, assertion, or other compiler-library calls.

Runtime guards:

- Statically proven: before destructive transition, `Fbl_RuntimeClosureOk()` and `FblRamRuntime_EnterCritical()` check that critical BLU runtime, flash, reset, cache, interrupt, and byte helpers are executable from RAM.
- Statically proven: compile-time checks enforce 32-bit `uint32` and 32-byte PFLASH page assumptions using typedef-size assertions.
- Requires TASKING map: the guard symbols resolve to RAM addresses, not PF0 aliases.

Remaining requirements:

- Requires TASKING map: original Infineon/TASKING helpers remain in PFLASH while all project-local `FblRam_*`, `FblRamFlash_*`, and `FblTrap_*` symbols are in RAM.
- Requires TASKING map: copy-table entries initialize every RAM helper before RoutineControl `0x0155`.
- Requires ECU runtime test: full FBL erase/program/verify/reset without any PF0 fetch after erase begins.

## Pre-C-Initialization PFLASH Closure

Active linker inputs:

- Option file: `ZGW/ZGW_FBL/TriCore Debug (TASKING)/.ZGW_FBL.elf.opt`
- Active LSL: `ZGW/ZGW_FBL/Lcf_Tasking_Tricore_Tc.lsl`
- Proof: `.ZGW_FBL.elf.opt` contains `--lsl-file=../Lcf_Tasking_Tricore_Tc.lsl`
- Static map output options: `-Wl--map-file=ZGW_FBL.map`, `-Wl--map-file-format=+statics`

Runtime symptom addressed: after reset the CPU reached PC `0x7003B008`, which is inside the CPU0 CSA reservation (`_lc_ub_csa_tc0 = 0x7003B000`, `_lc_ue_csa_tc0 = 0x7003BC00`) before `core0_main`. With `_START = 0x80000000` and `Ifx_C_Init = 0x80000A90/0x80000A94`, any branch into `0x7003B000..0x7003BC00` during startup indicates startup/control-flow state reached reserved context RAM, not valid copied code.

Root-cause linker defect: `code_iild_runtime_ram` was a copied DSPR0 RAM group with broad selectors:

```lsl
select ".text.IfxCpu.*";
select ".text.IfxCpu_Irq.*";
select ".text.IfxCpu_Trap.*";
select ".text.IfxScuCcu.*";
select ".text.IfxScuRcu.*";
select ".text.IfxScuWdt.IfxScuWdt_clearCpuEndinit";
select ".text.IfxScuWdt.IfxScuWdt_clearSafetyEndinit";
select ".text.IfxScuWdt.IfxScuWdt_getCpuWatchdogPassword";
select ".text.IfxScuWdt.IfxScuWdt_getSafetyWatchdogPassword";
select ".text.IfxScuWdt.IfxScuWdt_setCpuEndinit";
select ".text.IfxScuWdt.IfxScuWdt_setSafetyEndinit";
select ".text.CompilerTasking.*";
select ".text._*";
```

Those selectors can capture startup clock/reset/watchdog helpers and compiler C-init helpers into a `copy` group with `run_addr=mem:dsram0`. That is invalid for pre-C-init execution because `Ifx_C_Init` is the code responsible for initializing copied sections.

Corrected PFLASH startup group:

```lsl
group code_startup_pflash
(
    ordered,
    contiguous,
    align = 4,
    attributes=rx,
    run_addr=mem:BootManager_PFLASH
)
{
    select ".text.Ifx_Ssw_Tc0.__Core0_start";
    select ".text.Ifx_Ssw_Tc0.__StartUpSoftware";
    select ".text.Ifx_Ssw_Tc0.__StartUpSoftware_Phase1";
    select ".text.Ifx_Ssw_Tc0.__StartUpSoftware_Phase2";
    select ".text.Ifx_Ssw_Tc0.__StartUpSoftware_Phase3";
    select ".text.Ifx_Ssw_Tc0.__StartUpSoftware_Phase3ApplicationResetPath";
    select ".text.Ifx_Ssw_Tc0.__StartUpSoftware_Phase3PowerOnResetPath";
    select ".text.Ifx_Ssw_Tc0.__StartUpSoftware_Phase4";
    select ".text.Ifx_Ssw_Tc0.__StartUpSoftware_Phase5";
    select ".text.Ifx_Ssw_Tc0.__StartUpSoftware_Phase6";
    select ".text.Ifx_Ssw_Tc0.hardware_init_hook";
    select ".text.CompilerTasking.Ifx_C_Init";
    select ".text.Ifx_Ssw_Infra.Ifx_Ssw_doCppInit";
    select ".text.IfxScuCcu.IfxScuCcu_init";
    select ".text.IfxScuCcu.IfxScuCcu_getSourceFrequency";
    select ".text.IfxScuCcu.IfxScuCcu_getPllFrequency";
    select ".text.IfxScuCcu.IfxScuCcu_getPerPllFrequency1";
    select ".text.IfxScuCcu.IfxScuCcu_getPerPllFrequency2";
    select ".text.IfxScuCcu.IfxScuCcu_calRGainParameters";
    select ".text.IfxScuCcu.IfxScuCcu_modulation_init";
    select ".text.IfxScuRcu.IfxScuRcu_evaluateReset";
    select ".text.IfxScuRcu.IfxScuRcu_performReset";
    select ".text.IfxScuWdt.IfxScuWdt_clearCpuEndinit";
    select ".text.IfxScuWdt.IfxScuWdt_clearSafetyEndinit";
    select ".text.IfxScuWdt.IfxScuWdt_disableCpuWatchdog";
    select ".text.IfxScuWdt.IfxScuWdt_disableSafetyWatchdog";
    select ".text.IfxScuWdt.IfxScuWdt_getCpuWatchdogPassword";
    select ".text.IfxScuWdt.IfxScuWdt_getSafetyWatchdogPassword";
    select ".text.IfxScuWdt.IfxScuWdt_setCpuEndinit";
    select ".text.IfxScuWdt.IfxScuWdt_setSafetyEndinit";
    select ".text._c_init.libcs_fpu";
    select ".text._c_init_entry.libcs_fpu";
    select ".text._ldmst_clear_byte.libcs_fpu";
    select ".text._ldmst_copy_byte.libcs_fpu";
    select ".text.memcpy.libcs_fpu";
    select ".text.memset.libcs_fpu";
}
```

Startup functions previously vulnerable to RAM placement by broad selectors:

- `IfxScuCcu_init`, `IfxScuCcu_getSourceFrequency`, PLL helper sections, gain and `IfxScuCcu_modulation_init` helpers: previously matched by `.text.IfxScuCcu.*`.
- `IfxScuRcu_evaluateReset`, `IfxScuRcu_performReset`: previously matched by `.text.IfxScuRcu.*`.
- Watchdog password/endinit/disable helpers: previously matched by exact `IfxScuWdt` selectors inside the copied RAM group.
- `Ifx_C_Init` and other `CompilerTasking.*` sections: previously vulnerable to `.text.CompilerTasking.*`.
- `_c_init`, `_c_init_entry`, `_ldmst_clear_byte`, `_ldmst_copy_byte` and other underscore runtime helpers: previously vulnerable to `.text._*`.

Corrected BLU RAM helper group:

```lsl
group code_iild_runtime_ram
(
    ordered,
    align = 32,
    attributes=rwx,
    run_addr=mem:dsram0,
    copy
)
{
    select "(.ram_blu_helpers|.ram_blu_helpers.*)";
    select "(.ram_flash_helpers|.ram_flash_helpers.*)";
}
```

Corrected Ethernet/iLLD RAM runtime group: FBL Ethernet, lwIP, and DoIP driver code remains in RAM. The required GETH dependency closure is kept through exact selectors only: `IfxGeth_mac_setLineSpeed`, `IfxGeth_Eth_freeReceiveBuffer`, `IfxGeth_Eth_getTransmitBuffer`, `IfxGeth_Eth_wakeupReceiver`, and `IfxGeth_Eth_wakeupTransmitter`. Header-inline GETH calls such as duplex mode, DMA tail-pointer update, descriptor-current/base access, and MAC disable helpers are emitted into their relocated callers rather than broad-selected as separate iLLD families.

Compiler helpers kept in PFLASH:

- `.text.CompilerTasking.Ifx_C_Init`
- `.text._c_init.libcs_fpu`
- `.text._c_init_entry.libcs_fpu`
- `.text._ldmst_clear_byte.libcs_fpu`
- `.text._ldmst_copy_byte.libcs_fpu`
- `.text.memcpy.libcs_fpu`
- `.text.memset.libcs_fpu`
- All other `.text._*` helpers not explicitly selected by a RAM section macro

Compiler helpers still required in RAM: none are selected by a broad compiler-helper wildcard. The BLU code uses project-local `Fbl_CopyBytes()` and `Fbl_SetBytes()` in `.ram_blu_code` for post-erase memory operations.

Startup constants/tables: the live-rodata RAM group selects only exact lwIP runtime constants. Startup clock/reset constants such as `.rodata.IfxScuCcu.*`, linker copy/clear table descriptors, vector/trap startup data, and unselected literal pools remain in PFLASH through the later `rodata_fbl_pflash` catch-all or the fixed startup/vector sections. The copy table itself remains in `section_setup :vtc:linear` with `copytable_space = vtc:linear` and is not placed in a copied RAM group.

Linker assertions: no supported assertion syntax was present in the active TASKING LSL files, so no speculative assertion syntax was added. The LSL now emits `__STARTUP_PFLASH_START` and `__STARTUP_PFLASH_END` symbols for map auditing.

Static validation:

- Active map currently shows `_START`, `__StartUpSoftware*`, `Ifx_C_Init`, `Ifx_Ssw_doCppInit`, and `IfxScuCcu_defaultClockConfig` at `0x800...` PFLASH addresses.
- Removed broad copied-RAM selectors `.text.IfxCpu.*`, `.text.IfxCpu_Irq.*`, `.text.IfxCpu_Trap.*`, `.text.IfxScuCcu.*`, `.text.IfxScuRcu.*`, `.text.CompilerTasking.*`, and `.text._*`.
- Kept the BLU RAM closure groups for `.ram_flash_code`, `.ram_blu_code`, `.ram_eth_code`, lwIP/DoIP/Ethernet sections, exact live rodata, exact project-local flash helpers, exact trap helpers, exact project-local cache/interrupt helpers, and exact GETH runtime dependencies.
- Stack/CSA reservations remain unchanged: ustack ends at `0x7003AA00`, istack ends at `0x7003AF00`, CSA is `0x7003B000..0x7003BC00`.
- No local build was run, per repository instruction.

Post-build acceptance criteria:

- `_START`, `__Core0_start`, `__StartUpSoftware*`, `hardware_init_hook`, `Ifx_C_Init`, `Ifx_Ssw_doCppInit`, startup `IfxScuCcu/IfxScuRcu/IfxScuWdt` helpers, and C-init helpers must all map to `0x800...`.
- No pre-C-init symbol may have a runtime address in `0x70000000..0x7003BFFF`, `0x70100000..0x7010FFFF`, or `0xB0000000..0xB000FFFF`.
- Copy-table destinations must not enter `0x7003A600..0x7003BC00`.
- Runtime must pass breakpoints in order: `_START`, `__StartUpSoftware`, `Ifx_Ssw_initCSA`/CSA setup path, `Ifx_C_Init`, `core0_main`.
- The CPU must not branch into `0x7003B000..0x7003BC00`.

## TASKING Live-Rodata Linker Correction

Active linker inputs:

- Option file: `ZGW/ZGW_FBL/TriCore Debug (TASKING)/.ZGW_FBL.elf.opt`
- Active LSL: `ZGW/ZGW_FBL/Lcf_Tasking_Tricore_Tc.lsl`
- Included active LSL fragments: `tc1v1_6_2.lsl`, `inttab0.lsl`
- Static map output options: `-Wl--map-file=ZGW_FBL.map`, `-Wl--map-file-format=+statics`

Root cause of `ltc E112`: the active runtime rodata group used `attributes = r` with `run_addr = mem:dsram0` and no `copy`. The generated lwIP rodata inputs are ROM-classified by TASKING, so the linker tried to place ROM sections directly in the DSPR0 range `0x70000000-0x7003c000`. This is why the diagnostic requested ROM area inside DSPR; it was a placement-classification/copy-arrangement error, not a DSPR capacity problem.

Defective group before the correction:

```lsl
group rodata_fbl_live_runtime
(
    ordered,
    attributes = r,
    run_addr = mem:dsram0
)
{
    select "(.ram_eth_rodata|.ram_eth_rodata.*)";
    select ".rodata.ethernet.ethbroadcast";
    select ".rodata.ethernet.ethzero";
    select ".rodata.ip4_addr.ip_addr_any";
    select ".rodata.memp.memp_pools";
    select ".rodata.memp_std.memp_*";
}
```

Corrected runtime group: the original sections are selected exactly once as RAM runtime objects, with TASKING `copy` enabled so startup copy-table initialization moves their PFLASH load image into DSPR0.

```lsl
group rodata_fbl_live_runtime
(
    ordered,
    align = 4,
    run_addr = mem:dsram0,
    copy
)
{
    select "(.ram_eth_rodata|.ram_eth_rodata.*)";
    select ".rodata.ethernet.ethbroadcast";
    select ".rodata.ethernet.ethzero";
    select ".rodata.ip4_addr.ip_addr_any";
    select ".rodata.memp.memp_pools";
    select ".rodata.memp_std.memp_PBUF";
    select ".rodata.memp_std.memp_PBUF_POOL";
    select ".rodata.memp_std.memp_RAW_PCB";
    select ".rodata.memp_std.memp_SYS_TIMEOUT";
    select ".rodata.memp_std.memp_TCP_PCB";
    select ".rodata.memp_std.memp_TCP_PCB_LISTEN";
    select ".rodata.memp_std.memp_TCP_SEG";
    select ".rodata.memp_std.memp_UDP_PCB";
}
```

Corrected load-copy group: the generated bracketed copy sections are selected exactly once into `BootManager_PFLASH`. No DSPR `run_addr` group selects these bracketed sections.

```lsl
group rodata_fbl_live_load
(
    ordered,
    align = 4,
    load_addr = mem:BootManager_PFLASH
)
{
    select "[.rodata.ethernet.ethbroadcast]";
    select "[.rodata.ethernet.ethzero]";
    select "[.rodata.ip4_addr.ip_addr_any]";
    select "[.rodata.memp.memp_pools]";
    select "[.rodata.memp_std.memp_PBUF]";
    select "[.rodata.memp_std.memp_PBUF_POOL]";
    select "[.rodata.memp_std.memp_RAW_PCB]";
    select "[.rodata.memp_std.memp_SYS_TIMEOUT]";
    select "[.rodata.memp_std.memp_TCP_PCB]";
    select "[.rodata.memp_std.memp_TCP_PCB_LISTEN]";
    select "[.rodata.memp_std.memp_TCP_SEG]";
    select "[.rodata.memp_std.memp_UDP_PCB]";
}
```

The 12 original live-rodata sections are:

- `.rodata.ethernet.ethbroadcast`
- `.rodata.ethernet.ethzero`
- `.rodata.ip4_addr.ip_addr_any`
- `.rodata.memp.memp_pools`
- `.rodata.memp_std.memp_PBUF`
- `.rodata.memp_std.memp_PBUF_POOL`
- `.rodata.memp_std.memp_RAW_PCB`
- `.rodata.memp_std.memp_SYS_TIMEOUT`
- `.rodata.memp_std.memp_TCP_PCB`
- `.rodata.memp_std.memp_TCP_PCB_LISTEN`
- `.rodata.memp_std.memp_TCP_SEG`
- `.rodata.memp_std.memp_UDP_PCB`

The 12 generated ROM-copy selectors are:

- `[.rodata.ethernet.ethbroadcast]`
- `[.rodata.ethernet.ethzero]`
- `[.rodata.ip4_addr.ip_addr_any]`
- `[.rodata.memp.memp_pools]`
- `[.rodata.memp_std.memp_PBUF]`
- `[.rodata.memp_std.memp_PBUF_POOL]`
- `[.rodata.memp_std.memp_RAW_PCB]`
- `[.rodata.memp_std.memp_SYS_TIMEOUT]`
- `[.rodata.memp_std.memp_TCP_PCB]`
- `[.rodata.memp_std.memp_TCP_PCB_LISTEN]`
- `[.rodata.memp_std.memp_TCP_SEG]`
- `[.rodata.memp_std.memp_UDP_PCB]`

Conflict controls:

- Removed the runtime wildcard selector `.rodata.memp_std.memp_*` and replaced it with the exact 8 required `memp_std` objects.
- Kept `rodata_fbl_pflash` after the dedicated live runtime/load groups so the remaining `.rodata` catch-all cannot consume the live originals first.
- Bracketed generated copy sections are selected only by `rodata_fbl_live_load`.
- No DSPR `run_addr` group selects bracketed copy sections.

Static validation:

- `copytable_space = vtc:linear` and the `copytable` declaration remain present in the active LSL.
- `rodata_fbl_live_runtime` no longer has `attributes = r`; it has `run_addr = mem:dsram0` and `copy`.
- `rodata_fbl_live_load` has `load_addr = mem:BootManager_PFLASH`.
- `__FBL_LIVE_RODATA_LIMIT` was removed and replaced with `__FBL_LIVE_RODATA_SIZE`.
- `-Wl--map-file=ZGW_FBL.map` and `-Wl--map-file-format=+statics` are present in the active option file.
- No local build was run, per repository instruction.

Remaining build/map acceptance checks for the normal TASKING environment:

- The E112 ROM-in-DSPR diagnostic is gone.
- The map shows the 12 original sections with DSPR0 run addresses under `rodata_fbl_live_runtime`.
- The map shows the 12 bracketed copy sections with BootManager_PFLASH load placement under `rodata_fbl_live_load`.
- The copy table contains entries that initialize those RAM runtime objects before the live FBL erase path can execute.

## Direct Answers

1. Erase is now cooperative for FBL. Statically proven: FBL RoutineControl `0x0001` erases one 16 KiB logical sector at a time through `Fbl_BluEraseFblRange()`, emits repeated NRC `0x78`, drains TCP, and polls Ethernet/lwIP between sectors. Requires ECU runtime test: actual maximum DMU busy time for one TC375 sector.

2. Maximum Ethernet/lwIP polling interval during FBL erase is one PFLASH logical sector erase. Statically proven: the FBL erase loop calls `Fbl_FlashEraseRange(..., PFLASH_SECTOR_SIZE)` then `Fbl_ServiceCommsDuringLongOp()`. Requires ECU runtime test: one-sector erase latency on the target.

3. NRC `0x78` strategy is repeated response-pending before each sector erase. Statically proven: FBL erase calls `Fbl_UdsKeepAlive(0x31, ETH)` for every sector. FCD no longer resends RoutineControl for `0x78`; the DoIP client waits for final `0x71` and extends the deadline on each `0x78`.

4. CRC `0x0002` RAM closure is explicitly selected through `.text.Cpu0_Main.Fbl_*` plus direct `FBL_RAM_CODE` attributes on `Fbl_Crc32()`, `Fbl_Crc32FblWithDelayedBootPage()`, and `Fbl_Crc32UpdateByte()`. Requires new TASKING map: static/local symbol placement and literal pools.

5. Flash verification now checks DMU command completion and DMU error status in `FblRamFlash_*`, then invalidates program cache and verifies programmed pages through the non-cached PF0 alias. Statically proven: CPU-side full erase readback was removed. Requires ECU runtime test: DMU status bit/error handling against real TC375 failures.

6. Boot-critical data is programmed last. Statically proven: bytes `0xA0000000..0xA000001F` are delayed in `g_fblBootCriticalPage`, not programmed during `TransferData`, included in a pre-commit CRC overlay, then committed by `Fbl_CommitBootCriticalPage()` only after all other bytes are received. This range is the first 32-byte PFLASH page containing the FBL reset/start page at the beginning of the FBL partition. Requires new TASKING map/HEX audit: confirm no wider boot-header or project validity metadata exists outside that page.

7. TASKING map output is enabled through `-Wl--map-file=ZGW_FBL.map` and `-Wl--map-file-format=+statics` in `ZGW_FBL/TriCore Debug (TASKING)/.ZGW_FBL.elf.opt`. Cross-reference/call-graph options were not added because no supported syntax was proven from the local project metadata.

8. Copy-table proof status: requires new TASKING map/ELF. The linker script defines a copy table for `:tc0:linear` and RAM run groups for BLU code, flash code, trap code, Ethernet/lwIP runtime, selected iLLD/runtime sections, RAM data, and selected rodata. This audit does not claim the edited copy table is proven until the rebuilt ELF/map is inspected. Suggested normal-build-environment command: `hldumptc -F sections -F symbols "TriCore Debug (TASKING)/ZGW_FBL.elf"`.

9. CPU1/CPU2/HSM/DMA PF0 access status: statically proven only for linker configuration that CPU1/CPU2 cores are commented out in this FBL LSL and HSM/SCR/MCS are excluded by project source entries. Requires ECU/runtime confirmation that no external HSM activity or debugger agent accesses PF0. Ethernet DMA uses RAM descriptors/buffers from the lwIP/GETH RAM runtime; final placement requires map inspection.

10. Failure/reconnection behavior after erase begins: statically proven: TCP disconnect in FBL erase/download/verify/commit clears volatile transfer/page buffers, keeps bootloader target selected, and enters `BLU_RECOVERY_WAIT` instead of returning to PF0 or resetting. Supported retry sequence is a full FBL restart: reconnect, programming prerequisites, `0x0200`, `0x0155`, `0x0001`, `RequestDownload`, full transfer. Arbitrary mid-download resume is not claimed.

11. Final-response drain/reset ordering: statically proven in ECU code for FBL `RequestTransferExit` and ECUReset path: positive response is enqueued, state enters `BLU_RESPONSE_DRAIN`, TCP is drained with a bounded poll budget, and reset is requested only after drain success. Requires ECU runtime test: MAC TX descriptor completion observability is limited to the current lwIP TCP send queue drain.

12. Removed or rewritten obsolete behavior: `0x0155` no longer auto-selects bootloader; FBL erase no longer uses one full-range blocking erase; CPU-read erase verification was removed as the primary erase gate; FBL first page is no longer programmed in arrival order; FCD no longer resends erase on NRC `0x78`; FCD disables automatic reconnect/retry for `RequestDownload`, `TransferData`, `RequestTransferExit`, erase, and CRC.

## Required FBL Sequence

`programming prerequisites -> RoutineControl 0x0200 select FBL -> RoutineControl 0x0155 enter RAM runtime -> RoutineControl 0x0001 erase complete FBL range -> RequestDownload -> TransferData loop -> RequestTransferExit with CRC -> optional RoutineControl 0x0002 CRC readback -> final positive response drain -> controlled reset`

Concrete requests:

- `31 01 02 00 02` -> `71 01 02 00 02`
- `31 01 01 55` -> `71 01 01 55`
- `31 01 00 01 <addr32> <len32>` -> repeated `7F 31 78`, final `71 01 00 01 00`
- `34 00 44 <addr32> <len32>` -> `74 ...`
- `36 <bsc> <data>` -> `76 <bsc>`
- `37 <crc32>` -> `77`
- optional `31 01 00 02 <addr32> <len32> <crc32>` -> `71 01 00 02 <crc32> <status>`

APP programming remains separate and still honors the existing erase checkbox.

## State Rules

Statically proven:

- Erase before `RAM_READY` is rejected.
- FBL `RequestDownload` before `ERASE_COMPLETE` is rejected.
- `TransferData` before active download is rejected.
- FBL CRC `0x0002` before `TRANSFER_COMPLETE` or `COMPLETE` is rejected.
- ECUReset is rejected in FBL destructive/intermediate states.
- Disconnect after erase begins enters `RECOVERY_WAIT`.

## Remaining Required Proof

Requires new TASKING map:

- every post-`0x0155` code section, local static function, literal pool, switch table, compiler helper, callback target, and RAM constant is outside PF0;
- all RAM run groups have load images and copy-table entries;
- no RAM group overlaps stack, CSA, heap, Ethernet DMA descriptors, or buffers;
- delayed boot-critical range is sufficient for this project’s actual startup/boot metadata.

Requires ECU runtime test:

- one-sector erase time and resulting P2* margin;
- TCP/lwIP connection survival through a full FBL erase;
- final response drain through observable MAC/TX completion;
- reconnect/restart sequence from `BLU_RECOVERY_WAIT`;
- TC375 DMU error/status behavior for injected erase/program/protection failures.

## Ethernet libc closure

The reported `ethernet_output()` failure was caused by lwIP `SMEMCPY()` expanding to the TASKING library `memcpy` in PF0 (`calla 0x80001EA8`). The FBL lwIP configuration now includes `FblRam_LwipHooks.h` from `lwipopts.h`, before lwIP `opt.h` can apply its default libc macros. `MEMCPY`, `SMEMCPY`, `MEMSET`, `MEMMOVE`, and `MEMCMP` resolve to project-local `FblRam_Mem*` helpers in RAM.

The selected post-`0x0155` lwIP/GETH runtime sources with direct `memcpy`, `memset`, or `memmove` calls were patched to use the uppercase RAM hooks instead of libc. The original TASKING libc functions remain in their normal PFLASH placement; they are not relocated or selected into FBL RAM runtime.

Runtime closure now also checks `ethernet_output`, `FblRam_Memcpy`, `FblRam_Memset`, `FblRam_Memmove`, and `FblRam_Memcmp`. This is static source-level closure only until a new TASKING build proves the generated disassembly no longer contains post-erase calls from `ethernet_output()` or selected lwIP runtime sections into `0x80000000..0x800FFFFF`.

## Post-0x0155 libc and compiler-runtime closure

Observed defect: after PF0 erase, a RAM-resident lwIP input path called TASKING `memcmp()` at `0x80014A2A`. The suspected caller is the RAW/input path. Static source audit found the likely hidden source: `eth_addr_eq()` in `lwip/src/include/lwip/prot/ethernet.h` used lowercase `memcmp()`, so callers such as `ethernet_input()` could bind to libc even though no lowercase `memcmp()` was visible in the `.c` caller.

Replacement:

- `FblRam_Memcmp()` in `.ram_blu_helpers` was corrected to a byte loop returning `-1`, `0`, or `1`; it calls no library/iLLD code and uses no division/modulo.
- `eth_addr_eq()` now uses `MEMCMP()`, which resolves to `FblRam_Memcmp()`.
- `raw.c` direct lowercase `memset()` in `raw_new_ip_type()` now uses `MEMSET()`.
- `pbuf.c` direct lowercase `strlen()` in `pbuf_strstr()` now uses `STRLEN()`, backed by project-local `FblRam_Strlen()` in `.ram_blu_helpers`.
- `FblEth_RuntimeClosureOk()` now validates `FblRam_Strlen`, `raw_input`, and `ethernet_input` in addition to the existing `FblRam_Memcpy`, `FblRam_Memset`, `FblRam_Memmove`, `FblRam_Memcmp`, and `ethernet_output` checks.
- `code_eth_runtime_ram` now includes `.text.raw.*` so the guarded `raw_input()` path is part of the FBL RAM runtime group.

Static grep results for the selected post-`0x0155` runtime files:

- No remaining lowercase `memcpy(`, `memset(`, `memmove(`, or `memcmp(` call sites were found in the audited selected runtime source set.
- The remaining `strlen/malloc/free/abort` hits in that selected search are comments, except one `LWIP_ASSERT` message in `mem.c`; `LWIP_PLATFORM_ASSERT(msg)` expands to `lwip_geth_AssertFail((uint32)__LINE__)`, so the assertion string is not passed to runtime code by the active platform macro.
- No `_ldmst_`, `_div`, `_mod`, `_mul`, `_lcmp`, stack-check, or profiling helper names were found in the focused touched/runtime source search. This source grep cannot prove compiler code generation.

Required next TASKING proof is still pending because no local build was run: map address of `FblRam_Memcmp`, map address of `FblRam_Strlen`, `raw_input -> FblRam_Memcmp` disassembly proof, and a search of all RAM-resident section disassembly proving no call targets `memcmp` at `0x80014A2A` or any `memcpy/memset/memmove/memcmp/_ldmst_*` helper in `0x80000000..0x802FFFFF` or `0xA0000000..0xA02FFFFF`.

## Post-Erase TransferData Timeout

Observed FCD log result: `0x0200` succeeded, `0x0155` succeeded, and FBL erase `0x0001` completed in 2.569 seconds with 13 NRC `0x78` responses. The next request was `TransferData #1` with 4091 payload bytes because FBL answered `RequestDownload` with max block length `0x0FF4`. FCD then timed out after 8 seconds waiting for DoIP data.

Static source diagnosis: FBL programmed the whole received TransferData payload synchronously before sending `0x76`. A 4091-byte first block can require roughly 128 PFLASH page programs plus verification before any response is sent, which can exceed the client timeout even when no trap occurs.

Correction: FBL now advertises `FBL_UDS_MAX_BLOCK_LENGTH = FBL_TRANSFER_DATA_BYTES + 2`, with `FBL_TRANSFER_DATA_BYTES = 256`. The `RequestDownload` positive response should become `74 20 01 02`, causing FCD to send 256-byte data chunks instead of 4091-byte chunks on the FBL Ethernet path. This keeps each synchronous page-program batch bounded while leaving the 4096-byte DoIP TCP reassembly buffer intact.

Required next runtime proof: repeat `0x0200 -> 0x0155 -> 0x0001 -> RequestDownload -> TransferData`. Acceptance requires `RequestDownload` to advertise `0x0102`, FCD to send 256-byte TransferData payloads, each block to receive `0x76 <bsc>` before the 8-second timeout, no RAM trap, and all observed PCs to remain in RAM.

Follow-up FCD log at 17:23:41 proved ZGW already returned `74 20 01 02`, but FCD still sent `TransferData #1` with 4091 payload bytes. Root cause was in `Scripts/FCD/FCD.pyw`: the ZGW Ethernet path forced `block_size = ZGW_ETHERNET_TRANSFER_DATA_MAX_CHUNK_SIZE` and ignored the positive `RequestDownload` max block length. FCD now parses the `0x74` response, computes `maxNumberOfBlockLength - 2`, and clamps the actual TransferData chunk size to the ECU-advertised payload limit. Expected next FCD log is `block_data=256` and `TransferData ... payload_len=256`.

Follow-up FCD log at 17:27:43 proved the packet-length correction: `RequestDownload` returned `74 20 01 02`, FCD sent 768 `TransferData` requests with `payload_len=256`, every block received `76 <bsc>`, and `RequestTransferExit` with CRC received `77`. This confirms the previous transfer failure was caused by the oversized 4091-byte TransferData payload.

New observed failure after that correction: the redundant post-exit `RoutineControl CRC` request (`31 01 00 02 A0 00 00 00 00 03 00 00 <crc>`) returned `7F 31 24`. Static diagnosis: the bootloader `RequestTransferExit` path set `g_blu.state = FBL_BLU_STATE_COMPLETE`, then sent the positive `0x77` through `Fbl_SendAndDrainFinalPositive()`. That helper overwrote the BLU state with `FBL_BLU_STATE_RESPONSE_DRAIN`. The CRC routine gate accepts FBL CRC only while the BLU state is `TRANSFER_COMPLETE` or `COMPLETE`, so the next CRC request was rejected as a sequence error.

Correction: FBL bootloader `RequestTransferExit` now sends and drains the `0x77` response through `Fbl_SendAndDrainPositive()`, which does not alter `g_blu.state`. `Fbl_SendAndDrainFinalPositive()` is retained for true final reset responses and still sets `RESPONSE_DRAIN`. Expected next runtime result: after 768 positive 256-byte `TransferData` responses and `RequestTransferExit -> 77`, the optional `RoutineControl CRC` is accepted instead of returning NRC `0x24`.

Follow-up FCD log at 17:38:10 proved FBL programming complete: `RoutineControl CRC FBL` returned `71 01 00 02 <crc> 00`, the tester reset the ECU, reconnected over DoIP, and the FBL remained active. The next APP phase failed at `RoutineControl Erase APPL` with `31 01 00 01 A0 03 00 00 00 5A 00 00 -> 7F 31 31`.

Static diagnosis: after a successful FBL update, `g_blu.imageKind` remained `BOOTLOADER`. The shared erase routine dispatched by `g_blu.imageKind`, so the valid APP erase range was incorrectly checked by `Fbl_IsFullFblLogicalRange()` and rejected with NRC `0x31`. The NRC is still correct for a real out-of-range request; the bug was the stale target classification.

Correction: erase routine `0x0001` now classifies a 12-byte erase request by the requested range. Full FBL range still enters the FBL erase path. Non-FBL ranges are validated as APP ranges, switch the BLU target to APP, and erase only the requested validated APP window. The APP transition is allowed only after the bootloader update is finished (`COMPLETE`) or after the final reset response has been drained (`RESPONSE_DRAIN`); active destructive FBL states still return sequence error. `Fbl_BluSelectTarget()` also permits this normal `BOOTLOADER -> APPLICATION` transition so an explicit APP `0x0200` selection after an FBL update does not fail on stale target lock.

Follow-up FCD log at 17:45:48 showed the APP erase changed from NRC `0x31` to NRC `0x24`. That proves the APP range is no longer treated as out-of-range, but the target transition was still rejected by the bootloader-update state gate after the reset/reconnect. The transition rule is now centralized in `Fbl_BluCanSwitchFromBootloaderToApplication()`. APP `0x0200`, APP erase, and APP `RequestDownload` use the same rule: switching from `BOOTLOADER` to `APPLICATION` is allowed only from completed-FBL states (`COMPLETE`, `RESPONSE_DRAIN`, or a retained idle completed-image record), while active FBL erase/download/program/verify/failure/recovery states still return sequence error.

## Validation

Static validation was used only. The repository instruction prohibits local builds and external build/test-environment search.
