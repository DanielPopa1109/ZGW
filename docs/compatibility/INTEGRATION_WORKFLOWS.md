# Integration workflows

## FCD discovery and connection to ZGW

FCD discovery sends DoIP vehicle-identification requests over UDP or uses configured host candidates; ZGW DoIP returns identification data. FCD then opens TCP `13400`, sends routing activation from tester logical address `0x0710` with activation type `0`, and uses target `0x1001` for ZGW. DoIP ACK/NACK are transport-level; UDS positive/negative response is separate. Sources: `FCD.pyw::DoipClient`, `DoIP.c`, `EthStack_Cfg.c`.

## FCD local ZGW diagnostics

Origin FCD -> DoIP client -> DoIP endpoint -> PduR/DCM -> local service handler (CodingApp/GatewaySwc/FBL as active image) -> DCM response -> DoIP diagnostic response -> FCD. UDS bytes are not prefixed with downstream route address. DCM service/session matrix is in [diagnostic contracts](DIAGNOSTIC_CONTRACTS.md). Negative response follows UDS `7F SID NRC`; pending `7F SID 78` is tool-polled within configured timeout.

## FCD routed downstream diagnostics

Origin FCD -> DoIP TCP diag request payload `[0710][1001][route][UDS...]` -> DoIP -> PduR/DCM -> `Dcm_Service_ExtendedAddressForward` -> ParallelFlashSwc route lookup -> classic CanTp, FD CanTp or LinTp -> target ECU -> reverse response -> relay/correlation -> DoIP -> FCD. The downstream receives UDS bytes after route byte is removed. Route and bus mapping documented in network page. Failures include unknown route, queue/transport error, target timeout, malformed response and target NRC. CAN TX confirmation can trigger synthetic ACK for selected service classes; distinguish that from downstream UDS response.

## FCD downstream programming

FCD reads target package/image, selects the ECU descriptor and sends per-target UDS through same routed workflow. It sequences only what that target descriptor/branch specifies: session/control, optional erase, RequestDownload, TransferData blocks with BSC, TransferExit, CRC/verification, and reset/readiness where supported. ZGW routes frames; it does not perform downstream flash erase itself. Exact per-target routine/security/reset profile is unresolved unless target code/metadata states it.

## ZGW application/FBL update

FCD talks directly to ZGW at DoIP target `1001`. App enters update path and transitions/reset to ZGW FBL; FBL serves its own UDS/DoIP update contract. FCD selects ZGW software block and FBL RAM updater as needed, performs erase/download/transfer/exit/verification and reset/reconnect. This workflow programs ZGW memory using ZGW FBL and is not the downstream route flow. `Fbl_GetTransport` selects Ethernet; do not infer CAN FBL programming from CAN ACK support.

## Bundle scheduler

FCD `fcd_parallel.py::create_bundle` creates ZIP `.pfpkg` containing `manifest.json`, payload files and size/CRC32 integrity entries; schema `FCD_PARALLEL_PACKAGE_V1`. Schedule order is non-ZGW then ZGW; flash then coding; at most 2 active targets per bus, up to 6 UI workers. `FCD.pyw` performs actual UDS worker operations. ZGW `ParallelFlashSwc` contains route forwarding but its internal bundle stepper is incomplete (transport/coding step stubs, no public submit API). Package schema is not a wire protocol and not evidence of firmware acceptance. See [flashing contracts](FLASHING_CONTRACTS.md).
