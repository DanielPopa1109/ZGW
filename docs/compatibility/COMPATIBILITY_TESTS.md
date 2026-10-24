# Compatibility tests

Tests below are protocol acceptance cases derived from source contracts; hardware bench is required for real bus/flash claims. Keep target-specific expected SIDs/RIDs and memory ranges in each ECU test profile.

| ID | Setup / stimulus | Expected result |
|---|---|---|
| NET-01 | ZGW classic diagnostic physical request on `0x710` | Local DCM receives request and configured classic TP TX uses `0x710`; `0x711` macro is not the active DCM/CanIf mapping in this config. |
| NET-02 | Send classic frame with wrong ID or DLC above 8 | No matching diagnostic acceptance / rejected by interface; no false positive. |
| NET-03 | FD diagnostic TP on ID `0x710`, payload >8 and <=64 link bytes | Correct FD channel and TP reassembly; classic channel must not be mistaken for FD. |
| NET-04 | Routed request `[0x51][UDS...]` | ZGW strips route byte and transmits via LIN NAD `4`; response relays under same route. |
| NET-05 | Route `0x42` and a classic-CAN route such as `0x54` | Correct FD vs classic channel chosen; no cross-bus transmit. |
| NET-06 | Unknown route and malformed short routed envelope | Bounded failure/negative response; no arbitrary bus broadcast. |
| DIAG-01 | Local supported SID in allowed session | DCM handler response; confirm service/session table. |
| DIAG-02 | Local unsupported SID `0x27` to app | NRC `0x11` (app service absent), distinguish from FBL. |
| DIAG-03 | Target route + unsupported downstream SID | Target NRC relayed; do not substitute local DCM result. |
| DIAG-04 | Route + UDS `3E` | Confirm local-vs-forward interception and broadcast behavior against `Dcm_Service_TesterPresent`; response contract. |
| DIAG-05 | Target silent after request | FCD times out/reports failure; TX-confirmation synthetic ACK must not be accepted as proof of target execution in strict case. |
| FLASH-01 | FCD ZGW DoIP connect/activation | TCP 13400, tester `0710`, target `1001`, activation `00`; transport ACK and UDS response distinguished. |
| FLASH-02 | Direct ZGW FBL valid block sequence | Target-specific `0200`, `0155`, erase `0001`, `34/36/37`, CRC/verification and reset accepted in correct order. |
| FLASH-03 | Same FBL sequence out of order / wrong block counter | Rejected with configured NRC; no unintended flash write. |
| FLASH-04 | Downstream per-target sequence | ECU-specific profile passes; explicitly verify routed request max, response correlation, target CRC, reset/reconnect. |
| FLASH-05 | Interrupt TransferData and reconnect | Target's documented resume/restart/recovery behavior; never infer from successful normal transfer. |
| CODING-01 | ZGW coding read DID `F1C0/F1C3`, run `0203/0201` in coding session `41` | Exact response shape/echo and expected status/version. |
| CODING-02 | Invalid coding image magic/version/length/message counts | Reject with source-configured NRC; active image remains valid. CodingApp validation checks these fields; do not assume image CRC validation. |
| CODING-03 | Downstream coding descriptor round-trip | Target read/write/read-back matches target schema; test does not assume ZGW coding DIDs. |
| NEG-01 | Physical target response from different route/address | Not correlated to request; no cross-ECU response leakage. |
| NEG-02 | Segmented real CAN transfer without expected flow control | Characterize configured assume-FC behavior; reject or document target-specific successful pacing. |

Existing `Scripts/FCD/test_fcd_parallel.py` and `Scripts/FCD/test_diagnostic_acquisition.py` validate tool logic/simulation only; they do not prove firmware build or physical ECU compatibility.
