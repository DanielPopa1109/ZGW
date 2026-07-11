# ZGW_APP Static Resource Analysis

Date: 2026-06-27

Scope: static analysis only. No build, target run, simulator run, or test-environment discovery was performed.

Inputs:
- `ZGW/ZGW_APP/TriCore Debug (TASKING)/ZGW_APP.map`
- `ZGW/ZGW_APP/BSW/Sys/Os/Os.c`
- Selected scheduled module sources referenced by `Os.c`

## Summary

The existing TASKING map artifact shows the tightest RAM region is `mpe:cpu2_dlmu`: `0x00fe82` bytes used out of `0x010000`, leaving only `0x17e` bytes, or 382 B. The direct consumers in that region are Ethernet DMA cached buffers/descriptors and `ParallelFlashSwc` cached data.

PFLASH is not currently the pressure point. `mpe:pfls0` uses `0x05ac34` code bytes plus `0x025219` data bytes out of `0x2d0000`.

The CPU ranking below is not a measured runtime profile. It is a static risk ranking based on the 5 ms scheduler, direct call fan-out, explicit per-activation loop budgets, and loops over configured objects.

## Memory Region Summary

| Region | Used code | Used data | Reserved | Free | Total | Static note |
|---|---:|---:|---:|---:|---:|---|
| `mpe:cpu2_dlmu` | 0 B | 65,154 B | 0 B | 382 B | 65,536 B | Critical RAM pressure |
| `mpe:dsram0` | 0 B | 220,911 B | 8,320 B | 16,529 B | 245,760 B | High RAM pressure |
| `mpe:cpu1_dlmu` | 0 B | 56,420 B | 0 B | 9,116 B | 65,536 B | Moderate/high |
| `mpe:cpu0_dlmu` | 0 B | 49,860 B | 0 B | 15,676 B | 65,536 B | Moderate/high |
| `mpe:dsram2` | 0 B | 50,778 B | 8,192 B | 39,334 B | 98,304 B | Moderate |
| `mpe:dsram1` | 0 B | 111,658 B | 8,192 B | 126,934 B | 245,760 B | Comfortable |
| `mpe:pfls0` | 371,764 B | 152,089 B | 0 B | 2,425,267 B | 2,949,120 B | Comfortable |
| `mpe:pfls1` | 1,688 B | 0 B | 0 B | 3,143,016 B | 3,145,728 B | Comfortable |

## Top 20 RAM Consumers

Ranked by object-file contribution in RAM-like sections (`.bss`, `.data`, `.zbss`, `.zdata`, `.sbss`, `.sdata`) from the map link result.

| Rank | Object | RAM | Largest allocation in object |
|---:|---|---:|---|
| 1 | `memp.o` | 72.5 KiB | `.bss.memp_std.memp_memory_PBUF_POOL_base`, 54.0 KiB |
| 2 | `GatewaySwc.o` | 51.6 KiB | `.bss.GatewaySwc.GatewaySwc_EthTxQueue`, 8.4 KiB |
| 3 | `lwip_geth_lwip.o` | 48.8 KiB | `.bss.eth_dma_cached`, 24.2 KiB |
| 4 | `CanTp.o` | 48.7 KiB | `.bss.CanTp.CanTp_TxPayloadBuffer`, 48.0 KiB |
| 5 | `Dem.o` | 40.1 KiB | `.bss.Dem.Dem_NvImage`, 17.0 KiB |
| 6 | `PduR.o` | 33.3 KiB | `.bss.PduR.PduR_DoIPRxLargeQueue`, 15.9 KiB |
| 7 | `heap_4_core2.o` | 32.2 KiB | `.bss.heap_4_core2.ucHeap_core2`, 32.1 KiB |
| 8 | `Dcm.o` | 29.2 KiB | `.bss.Dcm.Dcm_ActiveReqBuffer`, 8.0 KiB |
| 9 | `Can.o` | 22.4 KiB | `.bss.Can.Can_TxQueue`, 10.5 KiB |
| 10 | `heap_4_core0.o` | 21.2 KiB | `.bss.heap_4_core0.ucHeap_core0`, 21.2 KiB |
| 11 | `LinTp.o` | 20.1 KiB | `.bss.LinTp.LinTp_PendingTx`, 16.0 KiB |
| 12 | `Fee.o` | 17.4 KiB | `.bss.Fee.Fee_JobData`, 17.1 KiB |
| 13 | `ParallelFlashSwc.o` | 16.5 KiB | `.bss.lmu_cached`, 13.1 KiB |
| 14 | `DoIP.o` | 16.1 KiB | `.bss.DoIP.DoIP_Rt`, 16.1 KiB |
| 15 | `mem.o` | 16.0 KiB | `.bss.mem.ram_heap`, 16.0 KiB |
| 16 | `Com.o` | 12.4 KiB | `.bss.Com.Com_RxRt`, 8.6 KiB |
| 17 | `Dcm_EthTpBridge.o` | 8.0 KiB | `.bss.Dcm_EthTpBridge.DcmEth`, 8.0 KiB |
| 18 | `heap_4_core1.o` | 5.3 KiB | `.bss.heap_4_core1.ucHeap_core1`, 5.3 KiB |
| 19 | `SomeIp.o` | 4.0 KiB | `.bss.SomeIp.SomeIp_Rt`, 4.0 KiB |
| 20 | `IfxPort_PinMap_TC37x_LQFP176.o` | 3.4 KiB | `.data.IfxPort_PinMap_TC37x_LQFP176.IfxPort_Pin_pinTable`, 2.4 KiB |

## Top 20 ROM Consumers

Ranked by object-file contribution in ROM-like sections (`.text`, `.rodata`, `.data`, vector/startup sections) from the map link result.

| Rank | Object | ROM | Largest single contribution |
|---:|---|---:|---|
| 1 | `Com.o` | 47.6 KiB | `.rodata.Com.Com_SignalCfg`, 30.3 KiB |
| 2 | `IfxGtm_PinMap_TC37x_LQFP176.o` | 32.8 KiB | Many GTM pin-map table entries |
| 3 | `GatewaySwc.o` | 12.3 KiB | `.text.GatewaySwc.GatewaySwc_Init`, 0.9 KiB |
| 4 | `Can.o` | 11.2 KiB | `.text.Can.Can_InitFdNode`, 0.8 KiB |
| 5 | `sockets.o` | 10.8 KiB | `.text.sockets.lwip_select`, 0.9 KiB |
| 6 | `tcp_in.o` | 10.7 KiB | `.text.tcp_in.tcp_receive`, 3.6 KiB |
| 7 | `tasks_core2.o` | 10.2 KiB | `.text.tasks_core2.xTaskGenericNotifyFromISR_core2`, 0.6 KiB |
| 8 | `tasks_core0.o` | 9.9 KiB | `.text.tasks_core0.xTaskIncrementTick_core0`, 0.6 KiB |
| 9 | `tasks_core1.o` | 9.9 KiB | `.text.tasks_core1.xTaskIncrementTick_core1`, 0.6 KiB |
| 10 | `PduR.o` | 9.2 KiB | `.rodata.PduR.PduR_CanIfRxRoutes`, 1.4 KiB |
| 11 | `IfxScuCcu.o` | 9.1 KiB | `.text.IfxScuCcu.IfxScuCcu_init`, 2.3 KiB |
| 12 | `Fee.o` | 8.7 KiB | `.text.Fee.Fee_MainFunctionStep`, 2.1 KiB |
| 13 | `Dcm.o` | 8.2 KiB | `.text.Dcm.Dcm_ProcessConnection`, 0.7 KiB |
| 14 | `tcp.o` | 7.9 KiB | `.text.tcp.tcp_slowtmr`, 1.3 KiB |
| 15 | `Dem.o` | 7.7 KiB | `.text.Dem.Dem_DebugUpdateEvent`, 0.4 KiB |
| 16 | `CanTp.o` | 7.5 KiB | `.text.CanTp.CanTp_TxConfirmation`, 0.6 KiB |
| 17 | `tcp_out.o` | 7.5 KiB | `.text.tcp_out.tcp_write`, 1.1 KiB |
| 18 | `api_msg.o` | 7.2 KiB | `.text.api_msg.lwip_netconn_do_writemore`, 0.7 KiB |
| 19 | `McuSm.o` | 7.1 KiB | `.text.McuSm.McuSm_ApplyRetainedState`, 1.1 KiB |
| 20 | `SysMgr.o` | 6.4 KiB | `.text.SysMgr.SysMgr_GoSleep`, 2.9 KiB |

## Top 20 Static CPU-Load Consumers

This is a static risk ranking, not a measured CPU percentage. All steady-state task entries below are 5 ms activations unless noted otherwise.

| Rank | Consumer | Evidence and reason |
|---:|---|---|
| 1 | `ASIL_NVM_Task_C0` memory-stack drain | Runs every 5 ms and allows `OS_NVM_MAIN_CYCLES_PER_ACTIVATION` = 20,000 loops; each loop calls `Fls_MainFunction`, `Fee_MainFunction`, and `NvM_MainFunction`, then yields. This can intentionally consume idle CPU while NvM/Fee/Fls jobs are pending. |
| 2 | Core0 startup NvM/Dem service loops | `Core0_WaitForNvMIdle` and `Core0_ServiceDemNvM` can run up to 1,000,000 service loops during startup/read-all/init paths. Not steady-state, but it is the highest static startup CPU risk. |
| 3 | `QM_BSW_Task_C2` Ethernet/network bundle | Runs every 5 ms and calls UdpNm, EthTimeSync, gPTP, SOME/IP SD, lwIP flag polling, TcpIp, SoAd, GatewaySwc Ethernet, DoIP, PduR, SOME/IP, and EthSM in one activation. |
| 4 | `SoAd_MainFunction` | Called from core2 every 5 ms; loops over `SOAD_MAX_CONNECTIONS` = 8 and can drain up to `SOAD_RX_DRAIN_BUDGET_PER_SOCON` = 128 receive units per connection/path. |
| 5 | lwIP receive/timer polling plus `TcpIp_MainFunction` | Core2 polls lwIP receive/timer flags every 5 ms and also initializes the lwIP `tcpip_thread`; packet bursts can shift CPU into lwIP input/output paths. |
| 6 | `GatewaySwc_EthernetMainFunction` | Called from core2 every 5 ms; drains Ethernet TX jobs with `GATEWAYSWC_ETH_TX_DRAIN_BUDGET` = 4 and posts TX results. |
| 7 | `DoIP_MainFunction` | Called from core2 every 5 ms; processes TCP stream data while enough bytes exist for a DoIP header and handles diagnostic payload state. |
| 8 | `PduR_DoIPCore2MainFunction` | Called from core2 every 5 ms; tied to DoIP queue draining and cross-core routing. |
| 9 | `SomeIpSd_MainFunction` | Called from core2 every 5 ms; iterates services and `SOMEIPSD_MAX_SUBSCRIPTIONS` = 8. |
| 10 | `SomeIp_MainFunction` | Called from core2 every 5 ms; parses stream data while at least a SOME/IP header is buffered. |
| 11 | `QM_DIAG_Task_C0` diagnostic bundle | Runs every 5 ms and calls TimeBase, PduR DoIP core0, Dcm, Dem, CodingApp, and ParallelFlashSwc. |
| 12 | `Dcm_MainFunction` | Called every 5 ms from `QM_DIAG_Task_C0`; loops configured DCM connections and service tables when requests are active. |
| 13 | `Dem_MainFunction` | Called every 5 ms from `QM_DIAG_Task_C0`; many paths loop over configured events and primary memory entries. |
| 14 | `ParallelFlashSwc_MainFunction` | Called every 5 ms from `QM_DIAG_Task_C0`; forwarding uses `PARALLELFLASHSWC_FORWARD_DISPATCH_BUDGET` = 12 with queue depth 40. |
| 15 | `PduR_DoIPCore0MainFunction` | Called every 5 ms from `QM_DIAG_Task_C0`; drains DoIP-to-DCM routing queues. |
| 16 | `QM_CAN_Task_C0` CAN bundle | Runs every 5 ms and calls Can, CanSM, and CanTp main functions. |
| 17 | `CanTp_MainFunction` | Called every 5 ms; loops configured CanTp channels and can transmit up to `CANTP_TX_CF_BURST_BUDGET` = 16 consecutive frames per channel state. |
| 18 | `QM_BSW_Task_C0` COM/NM bundle | Runs every 5 ms and calls Com RX, Com TX through `GatewaySwc_RequestComMainFunctionTx`, ComM, Nm, and CanNm. |
| 19 | `GatewaySwc_MainFunction` | Runs every 5 ms in ECU run state; processes cross-core Ethernet queues, publishes periodic summaries, routes signals, generates outputs, and updates diagnostics. |
| 20 | `QM_LIN_Task_C0` LIN bundle | Runs every 5 ms and calls LinSM, LinIf through the GatewaySwc wrapper, and LinTp. |

## Practical Pressure Points

- First RAM pressure to address is `mpe:cpu2_dlmu`, not PFLASH. The map places Ethernet DMA descriptors/buffers and `ParallelFlashSwc` cached data in this 64 KiB region, leaving only 382 B.
- The largest individual RAM allocations are lwIP pools/buffers, CanTp payload buffering, FreeRTOS core2 heap, DEM/FEE images, PduR DoIP queues, and DCM buffers.
- The largest ROM object is `Com.o`, mainly configuration tables rather than executable code.
- The main CPU-load risk is conditional: when NvM/Fee/Fls jobs are pending, the NVM task is designed to drain them aggressively using idle CPU. Under network traffic, core2 networking becomes the dominant steady-state candidate.
