# Downstream ECU migration checklist

1. Read [INDEX](INDEX.md), then only relevant pages. Treat their content as a navigation aid and verify each proposed requirement against current source/configuration and observable behavior.
2. Independently inspect relevant ZGW, FCD and target ECU implementations. Inventory target app, bootloader, CAN/CAN FD/LIN configs, message databases, diagnostic descriptors, memory map, build/test tooling and current FCD package metadata.
3. Identify physical bus, baud/mode, IDs, TP addressing, payload, route byte and response path from all three implementations. Reconcile ECU config with ZGW CanIf/CanTp/LinIf and DBC/LDF; do not change identifiers alone.
4. Build an explicit service matrix per session: SIDs, DIDs, RIDs/options, security, timing, positive/negative responses, reset and recovery. Compare against `Dcm_Cfg.c`, forwarding envelope and FCD request builder.
5. For flash, trace target bootloader end-to-end: entry, session, comm/DTC control, security, erase scope, RequestDownload format, max block size, BSC/retries, exit, integrity check, reset/readiness and interrupted-transfer recovery. Keep target procedure distinct from ZGW FBL.
6. For coding, specify target-specific format, ranges, NVM atomicity, validation, read-back and FCD mapping. Do not assume ZGW CodingApp schema applies.
7. Write compatibility-gap analysis with each item marked implemented/configured/experimental/incomplete/unsupported/unresolved, an owner (ZGW/FCD/target/system), and evidence paths/symbols. If ZGW or FCD owns the incompatibility, report it explicitly; do not add a target workaround that conceals it.
8. Preserve target ECU hardware-specific behavior and architecture. Implement only target changes justified by verified integration requirements; do not blindly port ZGW code. Run available target builds, tests, protocol checks and compatibility cases.
9. Bench-verify real bus timing, FC/pacing, NM/wake, target response/reset, CRC, recovery and repeated operation. Report hardware-dependent behavior still unverified.
10. Update the relevant compatibility page, machine-readable contract if suitable, and knowledge index when source contracts change.
