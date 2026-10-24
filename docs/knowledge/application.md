# Application features

## GatewaySwc: gateway runtime

Implementation: `ZGW/ZGW_APP/APP/GatewaySwc/GatewaySwc.c`; public contract and signal/PDU ranges: `GatewaySwc.h`. `Cpu0_Main.c` initializes it; `Os.c` schedules its core-0 main and core-2 Ethernet main. Core-0 periodic work includes COM signal forwarding and generated command handling; it also manages diagnostic RX message/signal status, timeout/invalid reporting, DTC transitions, coding masks and Ethernet publication queues. Core-2 handles Ethernet sockets, RX/TX queues and application telemetry.

Wire frame IDs/magic and interface details are defined in the implementation and `Scripts/ETH_LOGG/GW_ETH_LOG_FRAMES.md`. The logger document describes frame layouts and current documented receiver behavior. Use `GatewaySwc.h` constants and serialization functions as source of truth before protocol changes. Debug instrumentation is compile-time gated (`GATEWAYSWC_DEBUG_INSTRUMENTATION`, default 0 in header).

## CodingApp

`CodingApp.c/.h` implements local coding image validation, state, NvM persistence, DIDs `F1C0..F1C3`, and routines `0x0201..0x0204` (validate, write all, read NvM, defaults). Coding includes expected-RX-message and enabled-TX-PDU masks; GatewaySwc checks these masks. This local feature is distinct from flashing code to downstream ECUs. Verify exact validation and persistence flow in `CodingApp_RoutineControl` and `CodingApp_MainFunction` before changes.

## ParallelFlashSwc

`ParallelFlashSwc.c/.h` routes/forwards diagnostic coding requests to configured downstream targets, with bounds for target count, blocks and concurrently active targets. It is a tester request forwarding/scheduling component in the application; it is not the local flash driver and does not itself prove successful ECU programming. Downstream programming behavior is ultimately owned by the target ECU's bootloader. Check configured target identities, response/session tracking and timeout/retry behavior in source before editing.

## AI load predictor (experimental)

Embedded inference is in `APP/AiModel/AiModel.c`, `bcm_infer.c`, and generated weights `bcm_model.h`; the separate training/export workflow is under `Load Predictor ML/`. The checked-in ML README describes schema v4: 75 channels, 5 ms histories of 200 samples (1 s), 34 derived features, shared `34 -> 64 -> 32 -> 16 -> 6` MLP per channel, +500 ms current prediction and fault/class outputs. `Os.c` schedules inference on core 1. Inference cadence and input validity are source/configuration dependent; inspect actual compile-time switches and task body.

The model is a prototype trained with synthetic precursor data according to the training README. It is not a deterministic protection mechanism; documented UV/OV/OC safeguards remain separate. Do not imply real-vehicle failure accuracy or safety certification.

## Time

`APP/TimeSync/TimeBase.c/.h` provides vehicle monotonic time and UTC mapping, configurable UDS setting, NvM persistence/restore and SCR standby handoff. It is initialized before network and NvM recovery completes; later `Os_NvMStartup_MainFunction` calls restore integration. `TimeSync_Cfg.h` currently enables UDP publication (35000), DCM routine and NvM/SCR restore; gPTP feature enable is `STD_OFF` while force-master configuration is on, so the latter alone does not mean gPTP is active.

`Gptp_Lab.c/.h` builds/sends lab Sync/Follow_Up/Announce Ethernet frames. Treat as lab code; verify config and call guards before assuming transmission. `BSW/Com/Ethernet/EthTimeSync.c` is a separate configured ZTS1 UDP time publisher. Do not conflate that telemetry with IEEE 802.1AS synchronization.
