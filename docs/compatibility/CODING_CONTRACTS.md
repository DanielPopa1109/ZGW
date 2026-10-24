# Coding contracts

## ZGW-local CodingApp

Implemented in `ZGW/ZGW_APP/APP/CodingApp/CodingApp.h` and `CodingApp.c`, dispatched by DCM. DIDs: `F1C0` coding status, `F1C1` coding image, `F1C2` expected RX-message state, `F1C3` coding version. Image magic `0x434F4447`, image version `0x0004`; exact packed fields, lengths, validation and byte order are in header and serializer/deserializer functions, not inferred here. RIDs: `0201` validate, `0202` write all, `0203` read NVM, `0204` defaults. FCD uses UDS DID/RID operations and parses response payload in `_decode_coding_status_payload`, `_parse_read_coding_result`, `_run_coding_routine`.

Session: FCD enters coding session `0x41` before routines (fallback/ensure logic in `_ensure_coding_session_for_routine`). DCM service `31` is enabled in coding session. Routine result may be pending and FCD polls (bounded by tool settings). Verify positive SID, subfunction, RID and payload echoes; see FCD response validators.

## Downstream coding

There is no common downstream coding DID/image schema in ZGW source. FCD supports target descriptors, per-node coding routines and paced routed operations, but those definitions are ECU specific. `fcd_settings.json`, app configuration and tooling descriptors describe candidates; they do not prove every target implements ZGW's `F1C0..F1C3` or `0201..0204`.

When migrating a target, preserve its own coding format/NVM transaction, add explicit FCD mapping and verify read-back; do not reuse ZGW coding image layout without target-side code evidence.

Sources: `CodingApp.h/.c`, `Dcm_Cfg.c`, `Scripts/FCD/FCD.pyw` coding constants/handlers, `Scripts/FCD/fcd_settings.json`.
