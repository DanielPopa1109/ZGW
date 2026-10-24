# ZGW repository knowledge base

Start here. Paths below are relative to repository root (`D:\GitHub_Repos\ZGW`).

| Topic | Read when changing | Key entry points |
|---|---|---|
| [Architecture and runtime](architecture.md) | Core behavior, initialization, task scheduling, ownership | `ZGW/ZGW_APP/Cpu0_Main.c`, `ZGW/ZGW_APP/BSW/Sys/Os/Os.c` |
| [Communication and diagnostics](communications.md) | CAN/CAN FD/LIN, Ethernet, DoIP, SOME/IP, DCM/DEM, routing | `ZGW/ZGW_APP/BSW/Com/`, `ZGW/ZGW_APP/BSW/Diag/` |
| [Application features](application.md) | Signal routing, coding, ECU flash forwarding, AI, time | `ZGW/ZGW_APP/APP/` |
| [Bootloader and update](bootloader.md) | FBL UDS, flash programming, transport, update state | `ZGW/ZGW_FBL/Cpu0_Main.c`, `Fbl_RamUpdater_AURIX.c` |
| [Build and artifacts](build.md) | TASKING builds, linker, image filtering, release binaries | `ZGW/ZGW_APP/.cproject`, `Scripts/Build/` |
| [Supporting tools and data](tools.md) | FCD, logger, simulator, model pipeline, databases | `Scripts/`, `SIM/`, `DB/`, `Load Predictor ML/`, `Binary/` |

## Downstream ECU integration

For adapting an independently developed ECU to this reference gateway and FCD, begin with [compatibility INDEX](../compatibility/INDEX.md). It links source-verified bus, diagnostic, flashing, coding, workflow, migration and test contracts. Load only the pages relevant to the target ECU/task; verify against its own application, bootloader and configuration before making changes. ZGW is the reference gateway; FCD is the reference engineering tool.

Scope: this knowledge base describes source evidence in this checkout, not vehicle-level validation. `ZGW_APP` and `ZGW_FBL` are firmware projects. `Libraries` contain vendor/platform dependencies; generated IDE trees, prebuilt images, simulation projects, and PC utilities are not gateway runtime code. Do not infer AUTOSAR conformance from familiar module names: interfaces and behavior here are project-specific.
