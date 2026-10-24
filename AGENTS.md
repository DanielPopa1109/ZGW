# Repository guidance for Codex

## Knowledge base first

- Before investigating a repository area, consult `docs/knowledge/INDEX.md` and load only the topic page relevant to the task.
- For downstream ECU integration/migration, treat `docs/compatibility/` as a reference and navigation aid, not an unquestionable specification. Start at `docs/compatibility/INDEX.md`, then load only relevant pages; independently inspect current ZGW, FCD, and target ECU source/configuration and observable behavior before deciding compatibility.
- Determine each gap's owner from evidence: ZGW, FCD, target ECU, or unresolved/system integration. If the incompatibility is in ZGW or FCD, report it explicitly; do not mask it with an inappropriate target-side workaround.
- For target ECU changes, preserve its existing hardware-specific functionality and architecture. Do not blindly copy ZGW modules or conventions. Produce an explicit source-backed gap analysis, implement only changes justified by verified integration requirements, validate available builds/tests/protocol behavior, and report hardware-dependent behavior that remains unverified.
- Use the source references there to jump to implementation/configuration. Avoid rescanning areas already documented unless the task depends on detail absent from the page.
- Treat the knowledge base as an index and architectural map, not a substitute for source: verify current implementation and configuration before changing behavior.
- Verify compatibility documentation against actual code before changing functionality. When implementation, interfaces, configuration, build flow, or tool behavior changes, update affected `docs/knowledge/` or `docs/compatibility/` pages and their index when links or scope change.

## Scope and workflow

- `ZGW/ZGW_APP` and `ZGW/ZGW_FBL` are firmware projects; see `docs/knowledge/architecture.md` for boundaries. Separate application firmware from simulator, diagnostics, build checks, debug tools and generated artifacts.
- Do not assume AUTOSAR compliance based on module names. Establish behavior from source and configuration.
- Preserve user data and avoid unrelated changes. Do not modify generated output in place unless the task explicitly targets artifact generation.
- For protocol changes, trace complete RX/TX path and serialization/transport contract. For flash/update changes, inspect target address, sequencing, verification, reset and recovery paths; do not infer rollback safety from labels or state names.
- Report what was changed and verified, along with material gaps. Distinguish source-level evidence from build, bench, and vehicle validation.
