# AI Model Integration Notes

## Startup and Ownership

- Core0 (`Cpu0_Main.c`) initializes watchdogs, SafetyKit, pins, COM, diagnostics/NvM, starts the other cores, then creates Core0 FreeRTOS tasks through `Os_Init_C0`.
- Core1 (`Cpu1_Main.c`) waits for `OsInit_C0`, calls `Os_Init_C1`, and starts the Core1 scheduler.
- Core2 (`Cpu2_Main.c`) waits for `OsInit_C1`, initializes RMII pins, calls `Os_Init_C2`, and starts the Core2 scheduler.
- Ethernet ownership is Core2: `QM_BSW_Task_C2` initializes `LWIP_GETH_Init`, then runs `TcpIp_MainFunction`, `SoAd_MainFunction`, `SomeIpSd_MainFunction`, `SomeIp_MainFunction`, `DoIP_MainFunction`, `GatewaySwc_EthernetMainFunction`, and `EthernetDiag_MainFunction`.

## PDM1 Inputs

The AI adapter reads these COM signals:

- `COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_001`, PDU cycle 200 ms.
- `COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_001`, PDU cycle 50 ms.
- `COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_001`, PDU cycle 1000 ms.

DBC and generated COM metadata declare the signals as 32-bit unsigned values with scale 1 and no unit string. The adapter assumes these COM values are already physical engineering values in V, A, and deg C, then rejects values outside the model contract.

## Core1 Scheduling

- Function: `AiModel_MainFunction`.
- Core: Core1, called from existing `ASIL_BSW_Task_C1`.
- Priority/stack: inherits existing `ASIL_BSW_Task_C1` task configuration.
- Period: existing 5 ms `ASIL_BSW_Task_C1` timer activation.
- Inference WCET budget constant: 2000 us, measured with `TimeBase_PlatformGetCounterNs`.

## Ethernet Publication

Core1 does not call Ethernet APIs. It writes a double-buffered latest result guarded by an `IfxCpu_spinLock` and `__dsync`. Core2 reads that result from `GatewaySwc_PublishEthernetSummary` and emits Gateway frame type `0x05` through the existing Gateway Ethernet transmit queue.

AI frame payload uses the existing Gateway magic/header style:

- bytes 0..2: `0x5A 0x47 0x57`
- byte 3: frame type `0x05`
- bytes 4..7: Gateway main cycle
- byte 8: bus `GATEWAYSWC_BUS_CANFD`
- bytes 9..11: `inputValid`, `inferenceValid`, `windowReady`
- then big-endian IEEE-754 float32 values for measured inputs, predictions, anomaly, health, and 9 fault probabilities
- dominant fault, timestamp, inference sequence, execution time us, and error flags follow

## Compiler and Linker Facts

- TASKING configuration targets `tc37x`, `--core=tc1.6.2`.
- Existing compiler metadata shows `--fp-model=+float` and `.cproject` selects `fastSingle`.
- Linker script defines Core1 DSPR as `dsram1` at `0x60000000`, size 240 KB.
- Model weights are only included by `bcm_infer.c`; they remain `static const` read-only data and are not exposed through `bcm_infer.h`.

## Verification Gaps

Local instructions prohibit building or searching for test environments, so no TASKING compiler/linker run, final map comparison, target execution-time measurement, stack high-water measurement, or golden-vector execution was performed here.

No host reference vectors were supplied with the embedded files. The ONNX/PyTorch artifacts remain outside the embedded build path and were not embedded or executed on target.
