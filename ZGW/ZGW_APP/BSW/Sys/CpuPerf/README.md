# CPU Performance Counter Instrumentation

This module instruments selected TC375DP/TC37x execution regions with the
TriCore private CPU performance counters. It uses only `CCNT` and `ICNT`.
`M1CNT`, `M2CNT`, and `M3CNT` are intentionally not used because this repo does
not contain verified TC37x event selector definitions for those counters.

## Counter Model

- `CpuPerf_InitCore()` is called once from each CPU main function.
- Initialization resets stale counter state once per core and starts normal
  counting with `IfxCpu_resetAndStartCounters()`; it then validates that both
  CCNT and ICNT advance before exposing them.
- Per-region measurement reads the current core's `CCNT` and `ICNT` plus the
  FreeRTOS-configured per-core STM runtime counter at start and stop. Scoped
  measurements never reset the global core counters.
- Deltas are computed modulo the documented 31-bit counter value mask
  (`0x7FFFFFFF`). Sticky overflow state from `CCNT` or `ICNT` is accumulated in
  the measurement's overflow count.
- Existing FreeRTOS STM runtime statistics remain the CPU-load source of truth.
  CpuPerf is for scoped cycle/instruction diagnostics, not load accounting.

## Build-Time Control

`CpuPerf.h` defines `CPU_PERF_ENABLED` and feature flags that can be overridden
from project configuration:

- `CPU_PERF_ENABLED`
- `CPU_PERF_TASK_ENABLED`
- `CPU_PERF_ISR_ENABLED`
- `CPU_PERF_BSW_MAIN_ENABLED`
- `CPU_PERF_CRC_ENABLED`

When `CPU_PERF_ENABLED` is `0`, the public API compiles to inline no-ops.

## Diagnostic Routine

RoutineControl routine ID `0xF194` exposes the data through DCM.

- `0x31 0x01 F1 94 [startId count]`: read current statistics
- `0x31 0x03 F1 94 [startId count]`: read current statistics
- `0x31 0x04 F1 94`: reset accumulated statistics

The read response payload after the standard positive RoutineControl header is:

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 4 | Magic `CPF1` (`0x43504631`) |
| 4 | 2 | Version |
| 6 | 2 | Total measurement ID count |
| 8 | 1 | Start ID |
| 9 | 1 | Returned entry count |
| 10 | 2 | Entry length (`104` for protocol v3) |
| 12 | 4 | Counter value mask (`0x7FFFFFFF`) |
| 16 | n | Entries |

Each entry contains ID, valid flag, core ID, last/min/max/average cycles,
last/average instructions, CPI x1000, sample count, overflow count, last/average
nanoseconds measured from the FreeRTOS STM runtime counter, and accumulated byte count for byte oriented
measurements such as CRC32. The STM timing keeps the diagnostic useful when
private cycle/instruction counters are unavailable or return zero deltas.

Protocol version 3 appends scheduler attribution. Measurements that start in
the registered Core 2 QM BSW task report switched-out duration and switch-out
counts. Entry flag `0x02` marks this data valid. The derived value
`on-core+ISR = wall - scheduler-descheduled` still includes interrupt execution;
working CCNT/ICNT data is required to separate that final component.

## Measurement IDs

The ID order is defined by `CpuPerf_MeasurementIdType` in `CpuPerf.h` and must
stay synchronized with `Scripts/FCD/FCD.pyw`.

## Adding a Measurement

1. Add a `CPUPERF_ID_*` entry before `CPUPERF_ID_COUNT`.
2. Wrap the smallest useful region with `CpuPerf_Start(id, &ctx)` and
   `CpuPerf_Stop(id, &ctx)`.
3. Stop on every early return after the start point.
4. Keep ISR regions short. Avoid adding extra work inside the interrupt path.
5. Update the FCD `CPU_PERF_MEASUREMENT_TEXT` table if the ID is diagnostic.
