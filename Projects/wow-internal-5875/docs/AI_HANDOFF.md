# AI Handoff — wow-internal-5875

## Repository state

Branch: `codex/p07-world-zone-preparation` (observed in this worktree).
Starting checkpoint: `100034e127db4e224500c80db0677fa4e7945050` — `p07: add raw location lifecycle observer`.
Current HEAD: `100034e127db4e224500c80db0677fa4e7945050`; this documentation task is uncommitted.
Working tree: Clean at task start; now contains only unstaged `AGENTS.md` changes and untracked `docs/AI_HANDOFF.md`. Nothing staged or committed.

Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
The older branch/path descriptions in AGENTS.md and CODEX_PROJECT_STATE.md
are historical; use the observed branch and checkpoint above for continuation.

## Current task

Roadmap phase: Handoff protocol setup following P0.7.4, the read-only world location lifecycle observer.
Objective: Establish permanent end-of-task handoffs and record the actual checkpoint without changing production source or tests.
Status: Complete, documentation only; no commit requested or made. Updated 2026-10-10.

## Result

Summary: Appended the requested protocol to AGENTS.md and created this handoff.
The committed P0.7 sequence is foundation `386ddaf`, isolated evidence adapter
`93dad5c`, location source-gap research `71e2ed6`, and raw observer `100034e`.
P0.7.4 adds unqualified location diagnostics through the existing connection
observer; it does not qualify location or enable runtime profile selection.
The detailed continuation reference is
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
especially P0.7.4 and its controlled manual lifecycle runbook.

## Evidence

### SOURCE VERIFIED

- Git branch, HEAD, clean starting status and checkpoint history were inspected.
- Current `ConnectionObservationPolicy.h` explicitly emits
  `locationQualification=unqualified locationReason=source_lifetime_gap`
  and `inputOwner=none commands=none`.
- The committed P0.7.4 audit records fingerprint/signature-gated raw reads,
  independent candidate failure groups, profile exclusion and preserved gameplay
  ownership. This task did not repeat the full source audit or executable audit.
- The requested protocol is appended; only the two listed documents changed.

### RUNTIME OBSERVED

No new runtime evidence collected in this task. The P0.7.4 audit explicitly
records no WoW run for the new reader. The inherited R0.1b.1 normal read-only
connection lifecycle PASS is recorded in
[RUNTIME_CONNECTION_RELIABILITY_AUDIT.md](RUNTIME_CONNECTION_RELIABILITY_AUDIT.md);
it does not qualify P0.7.4, vendor episodes, water egress or reconnect.

### INFERRED

The next useful P0.7 task is the documented manual lifecycle capture: static
validation cannot establish Wine fingerprint-helper behavior or live detach.
This is a recommended continuation, not new runtime evidence.

### UNKNOWN

P0.7.4 live raw values, Wine CryptoAPI behavior and successful Stop Bot/detach
remain unqualified. SOURCE GAP: current-world lifetime, initialization,
zone/area player binding, cross-field coherence and generation/ABA handling.
Profile map/zone and position-map association remain unknown; area is unsupported.
SOURCE GAP: live Glue/UI action eligibility and reconnect remain unqualified;
reconnect is not implemented. No merge-target comparison was performed.

## Files changed

- `AGENTS.md` — appended the permanent AI handoff protocol.
- `docs/AI_HANDOFF.md` — created and populated this checkpoint handoff.

These are the exact changes for this task. No production source or tests changed.

## Validation

C++: Not run for this documentation task. Committed P0.7.4 audit reports PASS, 111 C++ executables.
Python: Not run here. P0.7.4 audit reports PASS, 49 audit Python tests.
QuestDB: Not run here. P0.7.4 audit reports PASS, 13 QuestDB Python tests.
Lua/SQL: Not run here. P0.7.4 audit reports PASS, 10 Lua fixtures and SQL/TSV fixture.
Build: Not run for documentation-only changes. P0.7.4 audit reports full MinGW validation build and separate `cmake --build build` PASS (DLL, testhost, loader, GUI).
git diff --check: PASS for this task; new handoff also checked separately because it is untracked.

Historical results above come from the committed P0.7.4 audit (2026-10-10,
247 validation records PASS, report recorded as
`/tmp/wow-validation-vvyf3kod/results.json`); the report itself was not revalidated.
Final `git status --short` confirms only the two intended documents changed.

## Runtime qualification

Status: Documentation task requires no runtime qualification. P0.7.4 remains NOT RUN / RUNTIME PENDING.
Evidence still required: Follow the P0.7.4 manual runbook using
`WOW_INTERNAL_CONNECTION_MODE=observe`: stable world, normal logout,
character-select dwell, same-character Enter World/loading, world return,
Stop Bot and detach/unload with WoW still responsive. Preserve both runtime
and lifecycle logs plus visible-screen timestamps. Require unqualified labels
and no input commands throughout. Even a successful run does not qualify
current location for profile consumption or close the source lifetime gap.

## Open blockers

- None for this documentation task.
- P0.7.4 runtime acceptance awaits the manual evidence above.
- Qualified profile location consumption is blocked by the source gaps above.
- Vendor episode and water emergency egress runtime acceptance remain pending;
  consult the connection reliability audit and
  [WATER_SWIMMING_DROWNING_AUDIT.md](WATER_SWIMMING_DROWNING_AUDIT.md).
- Automatic reconnect remains unimplemented; R0.1 is not closed.

## Recommended next task

Perform and audit the controlled P0.7.4 manual lifecycle run, then update its
detailed audit and this handoff with observed evidence and remaining unknowns.
Do not promote raw candidates into profile inputs from matching values alone.
Keep Detour/NavMesh and existing controller ownership intact.

## Integration / merge status

Safe to commit: Yes, for these two documentation files only; no commit performed.
Safe to merge: These documentation edits are suitable for review/merge. Whole-branch integration is UNKNOWN: no target comparison or merge verification was performed, and P0.7.4 runtime qualification remains pending.
Manual runtime test required: No for this documentation task; yes for P0.7.4 runtime acceptance and the pending vendor/water qualifications.

## Persistent project statuses

VENDOR EPISODE — RUNTIME PENDING
WATER EMERGENCY EGRESS — RUNTIME PENDING
SOURCE GAP — RECONNECT NOT IMPLEMENTED
