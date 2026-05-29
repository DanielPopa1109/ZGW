# Lab Time Synchronization Design

## Scope

The TC375 is the lab time master. Runtime operation does not require PC, NTP, GPS, cloud, or other external time. The primary clock is monotonic VehicleTime; UTC is a mapped calendar layer for diagnostics, logs, and PC display.

Default boot UTC is `2026-05-19 00:00:00 UTC`. It is marked valid so logs have a calendar value, but `time_source = TIMEBASE_SOURCE_DEFAULT_COMPILETIME` means the PC shall display it as default/not synchronized. If a valid TimeBase NvM block is present after `NvM_ReadAll()`, the default mapping is replaced with the stored UTC mapping and `sync_status` includes `TIMEBASE_SYNC_STATUS_NVM_RESTORED`.

## Modules

- `BSW/Time/TimeSync_Cfg.h`: compile-time switches and routine IDs.
- `BSW/Time/TimeBase.h/.c`: monotonic VehicleTime and UTC mapping.
- `BSW/Mem/Nvm/NvM_Cfg.h/.c` and `BSW/Mem/Fee/Fee_Cfg.h/.c`: TimeBase NvM block descriptor.
- `BSW/Time/Dcm_TimeRoutine.h/.c`: UDS RoutineControl helpers.
- `BSW/Time/EthTimeSync.h/.c`: UDP lab time broadcast.
- `BSW/Time/Gptp_Lab.h/.c`: isolated first-stage gPTP/BMCA build, parse, and state logic.

## TimeBase

VehicleTime is derived from STM0 ticks and accumulated as nanoseconds. STM wrap is handled by unsigned delta arithmetic. UTC is stored as a mapping:

```text
utc_now_ns = utc_time_at_set_ns + (vehicle_time_now_ns - vehicle_time_at_utc_set_ns)
```

At boot:

- `utc_valid = TRUE`
- `time_source = TIMEBASE_SOURCE_DEFAULT_COMPILETIME`
- `sync_status` contains `TIMEBASE_SYNC_STATUS_DEFAULT_UTC`

After UDS routine `0xF190`, `time_source = TIMEBASE_SOURCE_UDS_ROUTINE`.

## NvM Time Storage

The TimeBase has its own Fee-backed NvM block:

```text
NVM_BLOCK_ID_TIMEBASE = 3
NVM_BLOCK_TIMEBASE_LENGTH = 32
Fee management = redundant
NvM CRC = enabled
```

Block payload, big-endian:

```text
0..3    uint32 magic = 0x54494D45 ("TIME")
4       uint8  version = 1
5       uint8  valid = 1
6       uint8  original time_source (UDS routine or gPTP master)
7       uint8  reserved
8..15   uint64 utc_time_ns
16..23  uint64 vehicle_time_ns at RAM image update
24..27  uint32 sync_status at RAM image update
28..31  uint32 store_counter
```

`TimeBase_LoadUtcFromNvM()` is called after `NvM_ReadAll()` and before `Dem_Init()`. If the block is valid and plausible, TimeBase uses the stored UTC as the boot UTC mapping and sets `TIMEBASE_SYNC_STATUS_NVM_RESTORED`. If an armed SCR RTC handoff is present from the last standby entry, TimeBase adds the elapsed SCR RTC time before installing the mapping and also sets `TIMEBASE_SYNC_STATUS_SCR_RTC_ELAPSED`.

When UTC is corrected by UDS or future gPTP, TimeBase updates the NvM RAM image and marks the block changed. `TimeBase_MainFunction()` refreshes the RAM image once per `TIMESYNC_NVM_RAM_UPDATE_PERIOD_MS` while UTC is sourced from UDS/gPTP, so the normal `NvM_WriteAll()` path stores the latest mapped UTC. It does not force flash writes in the cyclic task.

## SCR RTC Standby Elapsed Time

The SCR firmware runs the SCR RTC and updates a retained handoff record in SCR XRAM. The handoff record starts at `0x176A`, directly after the existing SCR debug bytes at `0x1760..0x1769`.

Standby sequence:

```text
SysMgr_GoSleep()
  wait NvM idle
  TimeBase_PrepareStandbyRtc()
    refresh TimeBase NvM RAM image with current mapped UTC
    prepare and initially write the SCR XRAM record with UTC, source, and sync_status
  NvM_WriteAll()
  clear/reload SCR XRAM
  TimeBase_RearmStandbyRtc()
    restore the prepared SCR XRAM record after the destructive SCR clear/copy path
  start/restart SCR and enter standby
```

When the SCR starts for standby and sees the armed record, it resets the SCR RTC counter to zero and records `rtc_start_ticks = 0`. While the TC375 is in standby, the SCR main loop keeps writing `rtc_last_ticks` and `elapsed_ticks`. On the next core0 startup, `TimeBase_CaptureStandbyRtcBeforeScrReset()` copies the SCR XRAM record before the existing SCR RAM cleanup (`IfxMtu_MbistSel_scrXram = 77`) can erase it. `TimeBase_LoadUtcFromNvM()` then adds the captured elapsed time to the NvM UTC.

If McuSm TRAP4 reacts to a bus error whose failing address is inside SCR XRAM, the handler copies the SCR RTC record into an application-side TimeBase capture and an NCR-backed McuSm backup before zero-filling XRAM. If the failing address is inside the NCR linker group, the handler zero-fills NCR and then re-captures the SCR RTC record into the restored NCR backup. `Core0_HandleScrStartup()` imports that backup before SCR reset/copy/disable, so the RTC handoff is not lost by the zero-fill reaction.

Configured default tick conversion:

```text
SCR clock source in current SCR firmware: 100 MHz backup clock / DIV5 = 20 MHz
SCR RTC clock select: RTCCLKSEL = 1 for the 20 MHz PCLK path
SCR RTC prescaler: enabled, divide by 512
Configured TimeBase tick rate: 20 MHz / 512 = 39062.5 Hz
TIMESYNC_SCR_RTC_TICK_HZ_NUM = 390625
TIMESYNC_SCR_RTC_TICK_HZ_DEN = 10
```

The SCR RTC is 32-bit. At the default 39062.5 Hz tick rate, one wrap is roughly 30.5 hours. Unsigned delta arithmetic handles one wrap, but standby durations longer than one wrap cannot be distinguished without extending the SCR-side counter.

Calibration:

- `TIMESYNC_SCR_RTC_CALIBRATION_PPM` can apply a fixed lab correction to elapsed SCR RTC time.
- `TIMESYNC_SCR_RTC_USE_CCU6_CALIBRATION` is present as a configuration marker, but the CCU6 measurement path is not wired in this change.
- The Infineon SCR clock guidance distinguishes the 100 MHz backup clock divided by `DIV1/2/5/10/...` from the 70 kHz standby clock: https://community.infineon.com/t5/Knowledge-Base-Articles/How-to-configure-clocks-on-AURIX-TC3xx-for-Standby-Controller-SCR/ta-p/906186. Infineon's RTC clock-selection forum guidance points to `RTCCLKSEL = 1` for the 20 MHz PCLK RTC path and clearing it for the 70 kHz standby path: https://community.infineon.com/t5/AURIX/SCR-RTC-Clock-Selection-in-Normal-mode-and-standby-mode/td-p/384695. The large standby oscillator tolerance concern applies to the 70 kHz path. This project currently keeps `Scr_set_fsys(DIV5)`, so the default calculation uses the 20 MHz path and does not switch SCR into the lower-power 70 kHz mode.
- If `Scr_set_fsys_70kHz()` is enabled later for lower standby current, clear `SCR_TIME_RTC_USE_PCLK_20MHZ`, update `TIMESYNC_SCR_RTC_TICK_HZ_NUM/DEN` for the new RTC tick source, and add the CCU6 calibration adaptation before relying on calendar-grade standby elapsed time.

DTC/log code should use `TimeBase_GetDtcTimestamp()` or `TimeBase_GetTimestampSnapshot()`:

- store `vehicle_time_ns` always
- store/display `utc_time_ns` only when `utc_valid == TRUE`
- store `time_source` so the PC can label default, diagnostic, or gPTP time

## UDS RoutineControl

Configured routine IDs:

- `0xF190`: SetVehicleUtcTime
- `0xF191`: GetVehicleUtcTimeStatus

`0xF190` supports `routineControlType = 0x01`.

Accepted payload 1, big-endian:

```text
uint64 utc_time_ns_since_unix_epoch
```

Accepted payload 2, big-endian:

```text
uint16 year
uint8  month
uint8  day
uint8  hour
uint8  minute
uint8  second
uint16 millisecond
sint16 timezone_offset_min
uint8  flags
```

The timezone offset is minutes east of UTC. The routine converts local date/time to UTC by subtracting the offset. Timestamps outside configured plausible years `2020..2100` are rejected with request out of range.

`0xF191` supports `routineControlType = 0x01` and `0x03` with empty request payload.

Response payload for both routines, big-endian, 40 bytes:

```text
0..7    uint64 vehicle_time_ns
8..15   uint64 utc_time_ns
16      uint8  utc_valid
17      uint8  time_source
18..21  uint32 sync_status
22      uint8  gptp_state
23      uint8  gptp_enabled
24      uint8  gptp_force_master
25      uint8  reserved
26..27  uint16 gptp_tx_sequence_id
28..31  uint32 gptp_sync_tx_counter
32..35  uint32 gptp_announce_tx_counter
36..39  uint32 gptp_rx_announce_counter
```

Integration is through `CodingApp_RoutineControl()`, which already owns the strong `DcmAppl_RoutineControl()` hook used by `Dcm_Service_0x31`.

## UDP Lab Time Broadcast

Enabled by `TIMESYNC_UDP_ENABLE`, default port `35000`, default period `100 ms`. The sender uses the existing nonblocking `TcpIpH` UDP wrapper and broadcasts to `255.255.255.255`.

Packet format, big-endian, 40 bytes:

```text
0..3    uint32 magic = 0x5A545331 ("ZTS1")
4       uint8  version = 1
5       uint8  message_type = 1
6..7    uint16 packet_length = 40
8..11   uint32 sequence_counter
12..19  uint64 vehicle_time_ns
20..27  uint64 utc_time_ns
28      uint8  utc_valid
29      uint8  time_source
30..31  uint16 flags
32..35  uint32 sync_status
36..39  uint32 crc32 over bytes 0..35
```

RX processing is intentionally not implemented in the baseline because the TC375 is the lab time master.

## CAN Classic and CAN FD SDAT Broadcast

The existing `SDAT` frame is also populated from the same `TimeBase_GetTimestampSnapshot()` result:

- CAN Classic database: `DB/ZGW_CAN_3.dbc`
- CAN FD database: `DB/ZGW_CANFD_2.dbc`
- message: `SDAT`, CAN ID `0x202`, length `7`
- cycle time: `800 ms`

The COM configuration keeps `periodTicks = 159u`. With the current `COM_MAIN_PERIOD_MS = 5 ms` implementation, the counter transmits after ticks `159..0`, which is 160 ticks or 800 ms. The SDAT signals are excluded from change-triggered immediate transmit in `Com_IsTxSignalChangeTriggerExcluded()` so the 800 ms cycle is preserved while the application refreshes the buffer more often. The DBCs now also carry `GenMsgCycleTime = 800` on `SDAT`.

`SDAT` calendar fields are UTC and come from the TimeBase UTC mapping:

```text
Year        = UTC year minus 2000, saturated to raw 63 above 2063
Month       = UTC month
Day         = UTC day
Hour        = UTC hour
Minute      = UTC minute
Second      = UTC second
Millisecond = UTC millisecond, encoded with 4 ms resolution
```

Unused bits in the 7-byte payload carry the time quality:

```text
TimeSource  = 0 default compile-time, 1 UDS routine, 2 gPTP master, 3 invalid
UtcValid    = 1 when UTC/calendar fields are valid
DefaultTime = 1 when TimeSource is default compile-time and the PC shall mark it not synchronized
NvmRestored = 1 when UTC was restored from the TimeBase NvM block
```

Classic `SDAT` extension bits:

```text
CSB_SDAT_TimeSource  bit 6,  length 2
CSB_SDAT_UtcValid    bit 14, length 1
CSB_SDAT_DefaultTime bit 15, length 1
CSB_SDAT_NvmRestored bit 22, length 1
```

CAN FD `SDAT` extension bits:

```text
CANFD_SDAT_TimeSource  bit 5,  length 2
CANFD_SDAT_UtcValid    bit 7,  length 1
CANFD_SDAT_DefaultTime bit 13, length 1
CANFD_SDAT_NvmRestored bit 14, length 1
```

The application sender is `GatewaySwc_GenerateSdatOutputs()`. It sends both Classic and FD SDAT signals from one snapshot, so both buses carry the same time calculation.

## gPTP/BMCA Lab Status

`TIMESYNC_GPTP_ENABLE` defaults to `STD_OFF`. This keeps the existing lwIP/iLLD Ethernet path stable because the current project exposes UDP/IP sockets, not a raw Ethernet 0x88F7 TX/RX API.

Implemented:

- message builders for Sync, Follow_Up, Announce, Delay_Req, Delay_Resp
- Announce parser with length and ethertype checks
- BMCA dataset comparison using priority1, clockClass, clockAccuracy, offsetScaledLogVariance, priority2, and clockIdentity
- state handling with `TIMESYNC_GPTP_FORCE_MASTER = STD_ON` by default

Adaptation points:

- `Gptp_Lab_PlatformTransmitFrame()`: connect to raw Ethernet frame TX
- `Gptp_Lab_PlatformGetTxTimestamp()`: connect to GETH hardware timestamp or platform timestamp
- `Gptp_Lab_PlatformGetRxTimestamp()`: connect to GETH hardware timestamp or platform timestamp
- call `Gptp_Lab_RxIndication()` from raw Ethernet RX for ethertype `0x88F7`

When raw Ethernet and hardware timestamps are available, enable `TIMESYNC_GPTP_ENABLE` and optionally `TIMESYNC_USE_GETH_HW_TIMESTAMP`.

## Integration Points

- `TimeBase_Init()` during core0 BSW/system init.
- `TimeBase_CaptureStandbyRtcBeforeScrReset()` at the start of `Core0_HandleScrStartup()`, before SCR reset/copy/clear.
- `TimeBase_RestoreStandbyRtcCapture()` at the start of `Core0_HandleScrStartup()` if McuSm TRAP4 preserved an SCR RTC record after an XRAM/NCR zero-fill reaction.
- `TimeBase_LoadUtcFromNvM()` after `NvM_ReadAll()` and before `Dem_Init()`.
- `TimeBase_MainFunction()` in the core0 5 ms diagnostic cyclic task.
- `TimeBase_PrepareStandbyRtc()` in `SysMgr_GoSleep()` after NvM is idle and immediately before `NvM_WriteAll()`.
- `TimeBase_RearmStandbyRtc()` in `SysMgr_GoSleep()` after SCR XRAM clear/program copy and before `IfxScr_init(1)`.
- `EthTimeSync_Init()` after Ethernet/lwIP stack initialization on core2.
- `Gptp_Lab_Init()` after Ethernet driver initialization on core2.
- `EthTimeSync_MainFunction(5u)` and `Gptp_Lab_MainFunction(5u)` in the core2 Ethernet cyclic task.
- `GatewaySwc_GenerateSdatOutputs()` from the existing gateway owned-output generation path; it refreshes CAN Classic and CAN FD SDAT COM buffers from TimeBase.

## PC Interpretation

- `DEFAULT_COMPILETIME`: display calendar time, but mark default/not synchronized.
- `UDS_ROUTINE`: display as manually/diagnostically synchronized.
- `GPTP_MASTER`: display as network master time only if gPTP is enabled and integrated.
- `TIMEBASE_SYNC_STATUS_NVM_RESTORED`: display as restored lab time.
- `TIMEBASE_SYNC_STATUS_SCR_RTC_ELAPSED`: restored time includes SCR RTC elapsed standby time.
- `utc_valid = FALSE`: display VehicleTime only.

## Assumptions and Risks

- STM0 is available on TC375 and has a stable frequency from iLLD.
- Existing DCM service 0x31 is active only in extended/programming sessions with the current security masks.
- UDP broadcast uses the current `TcpIpH` socket wrapper; lwIP internal allocation is outside this module.
- gPTP is not production IEEE1588 compliance. It is a first-stage lab scaffold and remains inactive until raw Ethernet TX/RX is explicitly connected.
- The TriCore project consumes the generated `SCR_AURIX_TC3x.c` SCR image. This change updates SCR source files only; the generated SCR image must be refreshed by the normal SCR SDCC build flow when builds are allowed.
- Default SCR RTC standby elapsed time wraps after roughly 30.5 hours at the current 20 MHz / 512 tick rate.
