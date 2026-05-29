# FW Check Deviations

Scope: TC375DP SafetyKit FW check implementation under `BSW/Sys/SmM/00_Ssw`.

## Deviations

1. SMU AG6 and AG10 LBIST alarm aliases are accepted instead of requiring a strict Appendix-A value match.
   The implementation masks and accepts `ALM6[20]` and `ALM10[20]` as reset-path dependent LBIST side effects.

2. SMU AG8 `ALM8[5]` is filtered as an expected LBIST side effect.
   This prevents the FW check from failing on the observed TC375DP LBIST reset sequence where AG8 bit 5 is present.

3. SMU AG7 masks `ALM7[0]` and `ALM7[14]`.
   The current table treats these as device erratum or BROM-related side effects, not FW-check failures.

4. SMU AG9 masks `ALM9[0]` and `ALM9[1]`.
   These are treated as DTS-related side effects and are not compared as strict failures.

5. SMU AG9 additionally masks `ALM9[5]` and `ALM9[17]` for the LBIST reset path.
   These are voltage-monitor alarms observed after the reset/run sequence following a download, where the FW check classifies the reset as `safetyKitResetTypeLbist`.

6. Unknown reset types use the warm-PORST expectation path for SMU/STMEM/LCLCON comparison.
   This avoids a reset loop on unclassified reset reporting, but it is not a strict fail-closed interpretation of the safety-manual tables.

7. Unknown reset types skip SSH checking by selecting no MTU instance.
   This prevents false reset reactions when reset classification is unavailable, but it means the SSH comparison is not complete for that reset case.

8. LBIST SSH checking accepts more than one valid state.
   The implementation accepts the table-derived state, the cleared zero state, and the not-initialized state observed after SSW/BROM handling.

9. LBIST SSH checking accepts initialized RAM state for CPU/LMU entries when `DMU_HF_PROCONRAM` says that memory is initialized.
   This covers the observed sleep wake-up from standby case where DAM0 reports `{ECCD=0x5, FAULTSTS=0x9, ERRINFO=0}` while the standby table entry is the not-initialized state.

10. FW-check failure handling clears selected SMU/SSH evidence before reset reaction.
   Snapshot variables preserve debug information, but raw hardware status can be altered before a debugger can inspect it after the reset hook.

11. Expected SSH values are derived at runtime from `DMU_HF_PROCONRAM`, reset type, and standby state.
   This is intentional for the current board behavior, but it is not a single fixed table row comparison.

## Current Non-Deviations

1. STMEM and LCLCON values are compared directly against the selected reset expectation.

2. SMU groups with no documented or observed reset-path side effect are still compared strictly.

3. Bus MPU setup is separate from FW Check. It does not change FW-check pass criteria.
