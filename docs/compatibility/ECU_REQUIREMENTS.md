# ECU requirements

## Roles

| Role | Implemented boundary |
|---|---|
| ZGW app | Local UDS server, configured bus routing and DoIP endpoint. `ZGW/ZGW_APP/BSW/Diag/Dcm/Dcm.c`, `BSW/Com/DoIP/DoIP.c`, `APP/ParallelFlashSwc/ParallelFlashSwc.c`. |
| ZGW FBL | Separate ZGW update target over DoIP TCP/UDP 13400. `ZGW/ZGW_FBL/Cpu0_Main.c::Fbl_GetTransport`, `ZGW/ZGW_FBL/FblTransport_Cfg.h`. |
| Downstream ECU | Bus-specific physical target. Must match configured CAN/FD/LIN transport, route identity and the exact per-ECU diagnostic descriptor/sequence. No single downstream flash/coding profile is defined. |
| FCD | Windows Python/Tkinter engineering tool. DoIP to ZGW; downstream UDS is prefixed by one route byte. `Scripts/FCD/FCD.pyw`. |

## Minimum routed-diagnostic conditions

1. Target is physically attached to a configured ZGW bus. Route defaults from `ParallelFlashSwc_FindForwardBus`: `0x42` CAN FD; `0x51` LIN NAD `4`; `0x53..0x59` classic CAN; `0x60..0x7F` CAN FD; `0x90..0xAF` classic CAN; `0x46..0x4F` LIN. Check target override/list first.
2. FCD sends DoIP diagnostic payload `[target route byte][UDS request...]`; ZGW strips byte 0 and sends only UDS to the target. The target response is relayed through ZGW.
3. ECU TP addressing, CAN ID/mode or LIN NAD must match the current ZGW channel. LIN route `0x51` maps to wire NAD `4`; FCD route address is not the LIN NAD itself.
4. Flashing requires target-specific session/security, erase RID, memory range, download format, maximum block size, verification, reset and recovery behavior. A generic UDS service list does not make a target flash-compatible.

## FCD defaults are not support evidence

`Scripts/FCD/fcd_settings.json` defaults ZGW to ext address `0x41`, downstream PDM1 `0x42`, HVDCDC `0x51`, CBM `0x54`, DMU `0x56`, ELC `0x58`, FRBE `0x59`. Checked-in settings preserve ZGW as real/selected and downstream ECUs as simulated/unselected; CM5 is a separate, unselected node. The GUI's database discovery and bundle functions now live in `FCD.pyw`. It also exposes a built-in G26 lab profile, which is not evidence that a G26 ECU is attached or compatible. Parsing a node does not establish its diagnostic behavior.

G26 has a dedicated source route: FCD `[0x55][UDS...]` over DoIP to ZGW; ZGW sends extended-address Classic CAN requests on `0x710`, receives G26 responses on `0x709`, and sends response FC at EA `0x41`. FCD requires the returned `0x55` route prefix and a real UDS response. The corresponding board guard remains off pending wiring verification.

## Gaps and optional features

- CAN FD is configured for PDM1 route `0x42` and range `0x60..0x7F`; not mandatory for all targets.
- LIN diagnostic routing is configured for HVDCDC NAD 4, not a generic LIN programming guarantee.
- ZGW DCM has no configured `0x27 SecurityAccess`; ZGW FBL has a separate implementation. Downstream security is target-specific/unresolved.
- `ParallelFlashSwc` bundle manager is incomplete: no public bundle-submit API and flash transport step stubs advance state without target UDS programming. FCD is the evidenced programming orchestrator.
- Bus wake/sleep, bus-off recovery, target reset confirmation, power-loss recovery require bench validation.

Sources: `ZGW/ZGW_APP/APP/ParallelFlashSwc/ParallelFlashSwc.c`, `ParallelFlashSwc.h`, `ZGW/ZGW_APP/BSW/Diag/Dcm/Dcm.c`, `ZGW/ZGW_APP/BSW/Com/CanTp/CanTp.c`, `ZGW/ZGW_APP/BSW/Com/Lin/LinTp.c`, `Scripts/FCD/fcd_settings.json`.
