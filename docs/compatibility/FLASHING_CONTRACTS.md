# Flashing contracts

## Two separate programming targets

**ZGW's own image:** FCD directly addresses ZGW over DoIP and talks to the ZGW FBL after app-to-FBL reset/transition. `ZGW/ZGW_FBL/` owns its flash layout, UDS handlers, security and flash driver. **Downstream ECU:** FCD still connects to ZGW DoIP; route byte selects an ECU and ZGW forwards UDS over CAN/FD/LIN. Target bootloader owns its memory/security/erase/reset behavior. ZGW FBL protocol is not a downstream ECU requirement.

## FCD image transfer sequence

`FCD.pyw::_execute_payload` reads the selected payload (HEX/binary handling is in its parser), checks file size/CRC32, then uses `31` erase routine when requested, `34` RequestDownload, repeated `36` TransferData, `37` RequestTransferExit, and optional CRC verification (`31` routine or transfer-exit CRC according to target/image path). RequestDownload uses data-format `00`, address/length format `44`, followed by 4-byte big-endian address and size. BSC begins at 1 and wraps modulo 256; response block counter is checked. Maximum tool chunk is config dependent: default 32768 for direct ZGW DoIP, otherwise local target limit is clamped to ECU max block length and configured smaller UDS limit. Routed request overhead is one route byte plus two UDS bytes, leaving up to 253 data bytes under configured 256-byte TP/request cap. Confirm exact branch and payload type in `FCD.pyw::_execute_payload` before applying to a target.

For ZGW FBL block, FCD selects software block using RID `0200`, starts RAM updater RID `0155`, erases RID `0001`, downloads/transfers/exits and validates per FBL. ZGW application update includes app programming preamble/session and transitions to FBL; exact sequencing code is in `FCD.pyw::_execute_payload`, `_select_fbl_software_block`, `ZGW/ZGW_FBL/Cpu0_Main.c` UDS dispatch and `Fbl_RamUpdater_AURIX.c`. Block values are target-specific; do not infer a downstream ECU supports `0200/0155/0001`.

## Per-target requirements

For each image/target, establish: image target/address/range; block type; supported session; comm/DTC control; security; erase RID/options; RequestDownload address/length widths; ECU max block length; TransferData BSC and retry semantics; transfer exit; integrity check algorithm/byte order; reset type and readiness; interrupted transfer recovery. These are not portable across all configured ECUs. The FCD package manifest is metadata, not proof of target capability/signature.

## FCD parallel package format

`Scripts/FCD/fcd_parallel.py::create_bundle` writes ZIP `.pfpkg` bundles containing `manifest.json` plus selected payload files under `payloads/`. Manifest schema is `FCD_PARALLEL_PACKAGE_V1`; payload integrity entries record byte size and CRC32. This is corruption detection metadata, not a signed/authenticated package contract. `load_bundle` checks schema and returns manifest; FCD later loads payload entries and executes schedule events. Schedule generation places non-ZGW targets before ZGW, separates `flash` then `coding`, sorts bus lanes, caps active targets per bus at 2 (`MAX_ACTIVE_PER_BUS`), and the UI worker pool is capped at 6 (`PARALLEL_BUNDLE_MAX_WORKERS`). The package is an FCD input/output artifact; ZGW firmware does not consume this ZIP schema.

## Limitations

- FBL transport config permits up to 32768 image bytes per `36` transfer plus UDS bytes for direct Ethernet; downstream routed CAN/LIN cannot carry this in one TP request.
- ZGW forwarding config assumes CAN FC in the selected path; test segmented real ECU transfers.
- `ParallelFlashSwc` bundle-flash stepper currently has stub transport steps and no public submit API; it is not the FCD flashing engine.
- A successful transfer/CRC does not establish bootability, application health, rollback or power-loss safety. Reset recovery needs hardware proof.

Sources: `Scripts/FCD/FCD.pyw::_execute_payload`, `Scripts/FCD/fcd_parallel.py::build_schedule`, `ZGW/ZGW_FBL/FblTransport_Cfg.h`, `ZGW/ZGW_FBL/Cpu0_Main.c`, `ZGW/ZGW_FBL/Fbl_RamUpdater_AURIX.c`, `ZGW/ZGW_APP/APP/ParallelFlashSwc/ParallelFlashSwc.c`.
