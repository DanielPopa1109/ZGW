# ZGW integration and compatibility

Reference and navigation guide for integrating downstream ECUs with this checkout of ZGW and FCD; it is not an unquestionable specification or vehicle validation. For every migration, independently inspect current ZGW, FCD and target ECU source/configuration and observable behavior. Resolve conflicts from evidence and assign each gap to ZGW, FCD, the target ECU or unresolved system integration.

| Task | Read |
|---|---|
| First integration | [ECU requirements](ECU_REQUIREMENTS.md), [migration checklist](MIGRATION_CHECKLIST.md) |
| Buses, frames and timing | [network contracts](NETWORK_CONTRACTS.md), [contracts.json](contracts.json) |
| UDS | [diagnostic contracts](DIAGNOSTIC_CONTRACTS.md) |
| Programming | [flashing contracts](FLASHING_CONTRACTS.md), [workflows](INTEGRATION_WORKFLOWS.md) |
| Coding | [coding contracts](CODING_CONTRACTS.md) |
| Verification | [compatibility tests](COMPATIBILITY_TESTS.md) |

**Status terms:** implemented = active source path; configured = static configuration, not proof of hardware; experimental = lab/simulation path; incomplete = stub/scaffold; unsupported = no active support; unresolved = source evidence is insufficient. No universal downstream UDS, flash, security or coding profile exists. Do not infer these from ZGW's own FBL.

Reference implementation: `ZGW/ZGW_APP/`, `ZGW/ZGW_FBL/`; reference engineering tool: `Scripts/FCD/FCD.pyw`, `Scripts/FCD/fcd_parallel.py`. Key bus data: `DB/ZGW_CAN_3.dbc`, `DB/ZGW_CANFD_2.dbc`, `DB/ZGW_LIN_4.ldf`. For the broader inventory of supporting tools, see [knowledge: supporting tools](../knowledge/tools.md). Simulation under `SIM/` and FCD defaults are not proof of live ECU support. See each page for source pointers.
