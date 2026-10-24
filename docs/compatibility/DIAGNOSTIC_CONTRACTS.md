# Diagnostic contracts

## DoIP addressing and downstream envelope

FCD opens DoIP to configured gateway `192.168.1.10:13400`, tester logical address `0x0710`, ZGW target `0x1001`. Header version/inverse `02/FD`; routing activation type `00`; TCP diagnostic payload type `8001`; ACK/NACK `8002/8003`. `DoIP.c`, `EthStack_Cfg.c`, `FCD.pyw::DoipClient` are source anchors.

For downstream request, FCD payload is `[route byte][UDS bytes]`. `Dcm.c` identifies first byte `0x41..0xEF` as the routed pseudo-SID for requests that are not handled as local supported UDS services; it strips the byte and calls `ParallelFlashSwc`. Thus local services/SIDs are interpreted locally. The forwarding response is correlated to the route and relayed via `PduR_DoIPRelayForwardedResponse`; route byte is not passed to the downstream ECU. Mapping and fallback ranges: `ParallelFlashSwc_FindForwardBus`; CAN/FD/LIN send functions in `ParallelFlashSwc.c`.

Route examples: `0x42` CAN FD, `0x51` LIN NAD4, `0x54/56/58/59` classic CAN. FCD fallback range allocation is tool-side convenience; confirm actual ZGW route mapping/target config.

## ZGW app DCM service/session contract

`Dcm_DefaultServices` in `ZGW/ZGW_APP/BSW/Diag/Dcm/Dcm_Cfg.c` enables `10,11,14,19,22,28,31,34,35,36,37,3E,85`. No `27 SecurityAccess` service is configured. Session bytes used by FCD: `01` default, `02` programming, `03` extended, `41` coding. Access configured by DCM: `10` all; `11/14/19` all; `22` default/extended/coding; `28` extended only; `31` extended/programming/coding; `34/35/36/37` programming only; `3E` all; `85` extended. Confirm actual `Dcm_Cfg.c` session table before changes.

Absent service yields NRC `11`; service handlers use configured NRCs for subfunction/length/range/sequence/session/conditions/security/programming failures. `Dcm.h` has numeric definitions. Do not state all NRCs per target: downstream ECU response is target-specific. ZGW forwarding may also synthesize some positive acknowledgements after CAN TX confirmation; inspect `BuildForwardTxAckResponse` in `ParallelFlashSwc.c`. TX confirmation is not target UDS execution; strict FCD path should require target response where applicable.

ZGW app NRC constants (`Dcm.h`; an available constant is not proof every handler emits it): `10` general reject, `11` service unsupported, `12` subfunction unsupported, `13` incorrect length, `14` response too long, `21` busy/repeat, `22` conditions, `24` sequence, `31` range, `34` authentication required, `70` download not accepted, `71` transfer suspended, `72` programming failure, `73` wrong block counter, `78` response pending, `7E` subfunction unsupported in session, `7F` service unsupported in session. Response-pending limit constant is 2 (`DCM_RESPONSE_PENDING_MAX`). Handler paths determine which are emitted.

Local ZGW DIDs / RIDs are distinct from downstream services. ZGW FBL has its own service matrix, DIDs and security; see flashing contract and `ZGW/ZGW_FBL/`.

## Transport

Local CAN/FD diagnostics use physical standard ID `0x710` configured by `CanIf_RxPduConfig`/`CanIf_TxPduConfig` and `Dcm_DefaultConnections`. Classic max link bytes 8; FD 64. LIN DCM connection is configured with diagnostic PDU IDs `0x712` request and `0x71A` response in `Dcm_Cfg.c`, while LIN frame transport uses MRF/SRF IDs `0x3C/0x3D` in `LinIf.c`; reconcile layer semantics. DoIP physical tester/target are `0710/1001`. FBL transport currently selected by `Fbl_GetTransport` in `ZGW/ZGW_FBL/Cpu0_Main.c` is Ethernet; CAN ACK hardware code does not imply a CAN UDS programming transport.

ZGW DCM timers are `DCM_P2_SERVER_TICKS=50` and `DCM_P2STAR_SERVER_TICKS=5000` (`Dcm.h`; tick unit comes from DCM main-function scheduling/config and must be verified before translating to milliseconds). TP config: 5ms main tick, configured 1000ms timers, BS8, STmin0, padding on, max payload256. Downstream per-target P2/P2*/S3 timer values and unsupported service handling must be obtained from target source/config. FCD timeouts/retries are tool policy, not universal ECU protocol promises.

## FBL versus downstream security

ZGW FBL handles service `27` in its `Cpu0_Main.c` UDS dispatch and has a local seed/key path; ZGW app DCM does not. Do not copy FBL's security scheme into a downstream ECU. Key material is intentionally not duplicated here. Source: `ZGW/ZGW_FBL/Cpu0_Main.c` (`UDS_SID_SECURITY_ACCESS`, security-access handler); app service enablement: `ZGW/ZGW_APP/BSW/Diag/Dcm/Dcm.c::Dcm_DefaultServices`.
