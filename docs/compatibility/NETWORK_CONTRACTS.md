# Network contracts

Configured values are source evidence, not physical validation. DBC/LDF and COM configs define signals, bit positions, byte order, scaling and periods; inspect both database and CanIf/COM because copies may diverge.

## CAN / CAN FD

`ZGW/ZGW_APP/BSW/Com/Can/Can.h`: controller 0 classic, 1 FD; CAN main period 5ms; payload ceilings 8/64. Runtime mappings: `BSW/Com/CanIf/CanIf.c::CanIf_RxPduConfig/CanIf_TxPduConfig`, TP: `BSW/Com/CanTp/CanTp.c`. Local physical diagnostic RX and configured DCM connections use standard ID `0x710` on classic and FD. Do not assume macro `CANIF_DIAGRESPONSE_ZGW_CAN_ID=0x711` is active: DCM config uses PDU `0x710`. Classic COM also contains raw `0x710` RX; layered config overlaps.

Classic COM RX (`ID:DLC label` from `CanIf.c`): `253:2 CentralLockData`; `252:3 LightData1`; `240:2 StatusActuator`; `251:4 OutsideTemperatureStatus`; `250:8 CentralCommand1`; `201:1 DMUStatus`; `6F6:8 BattFullStat`; `10D:3 PdcStat`; `299:8 Mileage`; `200:1 DMU_Alive`; `7C7/7C5/7C3/7C1:8 XCP responses`; `6EF:8 VoltageCurrent`; `6EE:8 TempMeas`; `051:1 LoadStatus`; `6F0:4 L1_I2T_Counter`; `753/752/751/750:4 Error_706/704/702/700`; `710:8 diag raw`; `707/705/703/701:8 diag responses`; `6F3:8 BattSocSoh`; `6F2:8 BattSoc`; `6F7:1 BattDiagnosis`; `6F5:8 BattCurrent`; `6F1:8 BattCapDischarge`; `6F4:8 BattCapRes` (IDs hexadecimal).

Classic COM TX: `3A0:1 VehicleState`; `221:4 DisplayOutTemp`; `040:4 StatusBodyData1`; `220:6 CommandDisplayStatus`; `7C8/7C6/7C4/7C2/7C0:8 XCP requests`; `202:7 SDAT`; `3FF:1 NM3`; `050:2 LoadRequest`; diagnostic PDU entries `706/704/702/700` associate with `CANIF_DIAGREQUEST_CLASSIC_CAN_ID=0x710`. Verify actual CanIf TX path before treating those PDU IDs as bus IDs.

FD COM RX: `090:12 PDM1_LoadStatus`; `305..308:64 PDM1_CurrentFeedback_1..4`; `309:48 PDM1_CurrentFeedback_5`; `30A:2 InputT30`; `721:64 PDM1 diagnostic response`. FD COM TX: `106:12 InfotainmentData1`; `105:6 EnergyManagementData2`; `104:64 EnergyManagementData1`; `3A0:1 VehicleState`; `3FF:1 NM3`; `202:7 SDAT`; `103:3 LightData1`; `100:20 BodyData1`; `710:64 PDM1 diagnostic request`; `080:12 PDM1 CommandLoad`; `107:5 EnergyManagementData3`. FD diagnostic TP uses `0x710`.

DBC `GenMsgCycleTime` assignments (ms; these are database attributes, not measured runtime periods): classic `0x202=800`, `0x051=1000`, `0x10D=100`, `0x200=1000`, `0x201=1000`, `0x240=100`, `0x250=200`, `0x251=1000`, `0x252=100`, `0x253=200`, `0x299=1000`, `0x6EE=5`, `0x6EF=5`, `0x6F0=5`, `0x6F1=1000`, `0x6F2=1000`, `0x6F3=1000`, `0x6F4=1000`, `0x6F5=5`, `0x6F6=1000`, `0x6F7=1000`; FD `0x202=800`, `0x090=1000`, `0x305..0x309=50`, `0x30A=5`. Other DBC frames have no explicit `GenMsgCycleTime` assignment. Signal start bit, size, byte order, signedness, factor, offset, range and unit are encoded in the DBC `SG_` records (and LIN signal/frame definitions in LDF); use those exact records rather than infer from labels.

TP configuration: normal physical channels on `0x710`, classic/FD link lengths 8/64; forwarded channels use extended addressing on `0x710`, response target address `0xF0`, tester default `0x41` overridden per target. Tick 5ms, configured timers 1000ms, block size 8, STmin 0, padding enabled, max payload 256. These do not specify downstream ECU acceptance timing. `PARALLELFLASHSWC_FORWARD_ASSUME_CAN_FC=TRUE` assumes flow control in forwarding (documented for dry-run/sim targets); real target segmented transfer needs bench verification.

## LIN

`DB/ZGW_LIN_4.ldf`: LIN 2.1, 19.2 kbit/s, ZGW master slot 10ms with 5ms jitter, HVDCDC slave. `LinIf.c`: diagnostic MRF `0x3C`, SRF `0x3D`, DLC 8, inter-frame 60ms. Application frames: `ZGW_REQUEST_HVDCDC` ID `0x03` DLC2; `HVDCDC_STATUS` `0x0B` DLC5; `ZGW_SDAT` `0x04` DLC7. LDF is authoritative for signal layouts. `LinTp`: broadcast NAD `0x7F`, functional `0x7E`, HVDCDC NAD `4`, max payload 256; route `0x51` maps to NAD4.

## Ethernet / DoIP / SOME/IP

`EthStack_Cfg.c`: IP `192.168.1.10`; DoIP TCP/UDP `13400`; SOME/IP-SD UDP `30490`, multicast `224.244.224.245`; SOME/IP UDP `30500`, service `0x1234`, instance 1, version 1, TTL 3, method `0x0001`. FCD defaults host `192.168.1.10`, tester `0x0710`, ZGW target `0x1001`. DoIP header `0x02/0xFD`; routing activation type `0x00`; diagnostic type `0x8001`, ACK `0x8002`, NACK `0x8003`. Vehicle identification uses UDP; activation/diagnostics TCP. gPTP is disabled (`TIMESYNC_GPTP_ENABLE=STD_OFF`); lab function calls do not enable it. Time telemetry is separate.

## Lifecycle / timing

CAN/TP task tick 5ms; LIN diagnostic inter-frame 60ms and IF timeout 100 ticks. `0x3FF` NM3 appears as a one-byte COM PDU; this alone does not specify a complete NM state machine. Startup/task owners: `Cpu0_Main.c`, `Cpu1_Main.c`, `Cpu2_Main.c`, `BSW/Sys/Os/Os.c`. Do not infer unspecified sleep/wake behavior.

Signal-level sources: `DB/ZGW_CAN_3.dbc` / `DB/ZGW_CANFD_2.dbc` (`BO_`/`SG_` records and `BA_ "GenMsgCycleTime"`), `DB/ZGW_LIN_4.ldf` (`Signals`/`Frames` sections), `ZGW/ZGW_APP/BSW/Com/CanIf/CanIf.c::CanIf_RxPduConfig/CanIf_TxPduConfig`, `ZGW/ZGW_APP/BSW/Com/Com.c`, `ZGW/ZGW_APP/BSW/Com/Lin/LinIf.c` (`LinIf_ScheduleTable`/diagnostic frame config), and `ZGW/ZGW_APP/BSW/Com/Ethernet/EthStack_Cfg.c` (`SoAdConnections`, `SomeIpMethods`, `OfferedServices`).
