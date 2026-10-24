# Communication stacks and diagnostics

## CAN and CAN FD

Project code is in `ZGW/ZGW_APP/BSW/Com/Can/Can.c`, `CanIf/CanIf.c`, `CanTp/CanTp.c`, `CanSM/CanSM.c`, `CanNm/CanNm.c`, `Com/Com.c`, and `PduR/PduR.c`; local ID, PDU, signal and controller configuration is in module `*_Cfg.*` files and COM configuration. Core-0 startup initializes CAN, CANIF, PduR, COM, CANTP and CANSM, then requests full communication for classic and FD controllers (`Cpu0_Main.c::Core0_ComInit`). The low-level `Can.c` and configuration determine actual controller/channel details.

`Com` holds signal-level data. PduR routes interface and TP traffic among CANIF/CANTP, LINIF/LINTP, SoAd and DCM. Gateway signal route tables and bus ranges are in `APP/GatewaySwc/GatewaySwc.h/.c`; do not treat the entire COM signal catalog as routed without checking its tables and coding state.

The G26 lab route uses Classic CAN request `0x710` with target EA `0x55`, response `0x709` with tester EA `0xF0`, and gateway response flow control EA `0x41`. The M_CAN Classic RX filter admits `0x709`; CanIf PDU 3/8, DCM/PduR PDU 12 and a dedicated CanTp channel carry the reverse path. `ParallelFlashSwc` uses real G26 FC and target replies; it does not treat CAN TX confirmation as a G26 programming response. This is source configuration, not a built or measured gateway session.

## LIN

`BSW/Com/Lin/` contains low-level LIN driver, protocol, interface, state manager, transport protocol and diagnostics. LIN configuration resides in `Lin_Cfg.*`; `Cpu0_Main.c` initializes LIN state manager/transport and requests full communication. `Os.c` runs LIN periodic functions in the QM LIN task. For frame/schedule behavior, inspect local LIN config and `DB/ZGW_LIN_4.ldf`; the LDF is reference input, not proof that each frame is active in firmware.

## Ethernet, DoIP and SOME/IP

`BSW/Com/Ethernet/` contains project stack layers: `EthStack`/`EthSM`, GETH+lwIP port in `GETH_LWIP_ILLD/`, `TcpIpH`, `SoAd`, `DoIP`, `SomeIp`, `SomeIpSd`, Ethernet diagnostics and time-sync publisher. `EthStack_Cfg.c` defines socket IDs and addresses: DoIP UDP/TCP 13400, SD UDP 30490 with multicast 224.244.224.245, SOME/IP UDP 30500, and a UDP interface connection 30600; local configured gateway address is 192.168.1.10. These are defaults in checked-in source and can differ at runtime/build configuration.

DoIP (`DoIP.c`) processes vehicle identification, routing activation, alive check and diagnostic messages; diagnostic payload is passed through callbacks/bridge into PduR/DCM. `Dcm_DoIP_Adapter.c` and `Dcm_EthTpBridge.c` adapt Ethernet transport to DCM/TP interfaces. PduR includes queues and session reset handling; check its generation/reset paths when changing asynchronous DoIP traffic.

The configured SOME/IP sample is a demonstration service/method (`EthStack_Cfg.c`: service 0x1234, method 1, response payload 0x12 0x34); SOME/IP-SD offers it on UDP 30500. This is not evidence of a vehicle-wide SOME/IP gateway implementation.

Ethernet application telemetry includes ZGW frames and TimeSync/MCU-status UDP packets. Frame names/layouts and receive ports are documented in `Scripts/ETH_LOGG/GW_ETH_LOG_FRAMES.md`; implementation is in `APP/GatewaySwc/GatewaySwc.c`, `EthTimeSync.c`, `SoAd.c` and related config. Treat the logger's documented defaults as PC-tool config, not firmware API beyond verified frame contracts.

## Diagnostics

`BSW/Diag/Dcm/` implements UDS request/session/service handling and configured DID/routine dispatch; `Dcm_Cfg.c` is the service configuration. `BSW/Diag/Dem/` stores event/DTC state, status and NvM integration. `BSW/Com/CanDiag.c` and `LinDiag.c` provide bus diagnostic handling. `GatewaySwc` contributes DIDs, event snapshots, communication control and DTC transition reporting; `CodingApp` provides coding DIDs/routines; `TimeBase` provides UTC time routines; `ParallelFlashSwc` handles target-extended-address routing.

For a service change, trace the full chain: transport RX -> PduR/TP -> DCM -> configured handler -> response -> PduR/TP -> transport TX. Check callback ownership, buffer sizes and asynchronous `OpStatus` behavior. Module naming alone does not guarantee standard conformance.
