# FW Check Deviations

Scope: TC375DP SafetyKit FW check implementation under `BSW/Sys/SmM/00_Ssw`.

## Deviations

1. SMU AG6 and AG10 LBIST alarm aliases are accepted instead of requiring a strict Appendix-A value match.
   The implementation masks and accepts `ALM6[20]` and `ALM10[20]` as reset-path dependent LBIST side effects.

2. SMU AG8 `ALM8[5]` is filtered as an expected LBIST side effect.
   This prevents the FW check from failing on the observed TC375DP LBIST reset sequence where AG8 bit 5 is present.

3. SMU AG7 masks `ALM7[0]` and `ALM7[14]`.
   The current table treats these as device erratum or BROM-related side effects, not FW-check failures.

   3a. SMU AG7 additionally masks `ALM7[17]` for the LBIST reset path.
   This covers the observed coding endurance reset where FW_CHECK classified the reset as `safetyKitResetTypeLbist` and AG7 compared as actual `0x00020002` versus expected `0x00000002`; `ALM7[1]` remains the expected table value.

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

   9a. LBIST SSH checking additionally accepts the initialized signature `{ECCD∈{0x0,0x5}, FAULTSTS=0x9, ERRINFO=0}` unconditionally, independent of the `DMU_HF_PROCONRAM` evaluation.
   The DMU RAM/LMU auto-init at the LBIST PORST-class exit re-initializes some memories regardless of the SW-visible `RAMINSEL`/`LMUINSEL` config, so the deviation-9 gating rejects the same benign state. The post-init `ECCD` is observed as `0x5` (tracking bits set) or `0x0` (no ECC flags); `FAULTSTS=0x9` (init-done) and `ERRINFO=0` in both. Observed as a single power-on-only FW-check failure (`resetType=LBIST`), e.g. SSH instances with resultMask `0x28`/`0xA8`, `FAULTSTS=0x9`.

10. FW-check failure handling clears selected SMU/SSH evidence before reset reaction.
   Snapshot variables preserve debug information, but raw hardware status can be altered before a debugger can inspect it after the reset hook.

11. Expected SSH values are derived at runtime from `DMU_HF_PROCONRAM`, reset type, and standby state.
   This is intentional for the current board behavior, but it is not a single fixed table row comparison.

## Current Non-Deviations

1. STMEM and LCLCON values are compared directly against the selected reset expectation.

2. SMU groups with no documented or observed reset-path side effect are still compared strictly.

3. Bus MPU setup is separate from FW Check. It does not change FW-check pass criteria.
