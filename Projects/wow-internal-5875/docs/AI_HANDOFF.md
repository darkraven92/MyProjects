# AI Handoff — wow-internal-5875

## Repository state

Branch: `codex/p07-world-zone-preparation` (observed in this worktree).
Starting checkpoint: `42a5a7b88acec9d4c55feb57dcf102c7b691983c` — `chore: add AI handoff protocol`.
Current HEAD: `42a5a7b88acec9d4c55feb57dcf102c7b691983c`; this documentation update is uncommitted.
Working tree: Clean at task start; now contains only unstaged modifications to `AGENTS.md` and `docs/AI_HANDOFF.md`. Nothing staged or committed in this task.

Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
The older branch/path descriptions in AGENTS.md and CODEX_PROJECT_STATE.md
are historical; use the observed branch and checkpoint above for continuation.

## Current task

Roadmap phase: P0.7.4 raw location lifecycle runtime-result handoff.
Objective: Maintain the permanent protocol and record the supplied runtime evidence and actual checkpoint without changing production source or tests.
Status: Complete, documentation only; no commit requested or made. Updated 2026-10-10.

## Result

Summary: Updated the existing protocol to the requested rule formatting and
refreshed this handoff with the user-supplied P0.7.4 runtime result.
The committed P0.7 sequence is foundation `386ddaf`, isolated evidence adapter
`93dad5c`, location source-gap research `71e2ed6`, and raw observer `100034e`.
P0.7.4 adds unqualified location diagnostics through the existing connection
observer; it does not qualify location or enable runtime profile selection.
The detailed continuation reference is
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
especially P0.7.4 and its controlled manual lifecycle runbook. Its historical
NOT RUN / PENDING statements predate the runtime result recorded below; the
detailed audit was not edited because this task permits only two files.

## Evidence

### SOURCE VERIFIED

- Git branch, HEAD, clean starting status and checkpoint history were inspected.
- The prior handoff source inspection found `ConnectionObservationPolicy.h` emits
  `locationQualification=unqualified locationReason=source_lifetime_gap`
  and `inputOwner=none commands=none`.
- The committed P0.7.4 audit records fingerprint/signature-gated raw reads,
  independent candidate failure groups, profile exclusion and preserved gameplay
  ownership. This task did not repeat the full source audit or executable audit.
- The requested protocol remains present once; only the two listed documents changed.

### RUNTIME OBSERVED

**P0.7.4 RAW LOCATION LIFECYCLE OBSERVER — RUNTIME PASS**

Provenance: runtime result and lifecycle values supplied by the user for this
task. No new WoW run or independent raw-log inspection was performed here.

| Observed stage | manager | playerGuid | localPlayer | mapCandidate | zoneCandidate | areaCandidate |
| --- | --- | --- | --- | --- | --- | --- |
| In-world | `0x14fe508` | `0x1aa56` | `0xde68008` | `1` | `14` | `363` |
| Logout / teardown | `0x0` | unknown | unknown | unknown | `14` | `363` |
| Later teardown | `0x0` | not supplied | not supplied | unknown | `0` | `0` |
| New world initialization | `0x8d35d08` | `0x0` | unknown | `1` | `0` | `0` |
| Fresh world return | `0x8d35d08` | `0x1aa56` | `0x19da8008` | `1` | `14` | `363` |

Zone/area are runtime-observed stale after world identity disappears.
mapCandidate can appear before active-player identity exists. The same player
GUID returned with fresh manager/local-player pointers. "Not supplied" marks
omitted evidence rather than an observed unknown value.

### INFERRED

The observed stale zone/area values and early map publication support retaining
the source lifetime gate. Matching values after return do not establish valid
current-player location, cross-field coherence or a world generation.

### UNKNOWN

The supplied lifecycle excerpt does not include fingerprint-helper or
Stop Bot/detach telemetry; those details were not independently inspected here.
SOURCE GAP: current-world lifetime, initialization,
zone/area player binding, cross-field coherence and generation/ABA handling.
Profile map/zone and position-map association remain unknown; area is unsupported.
SOURCE GAP: live Glue/UI action eligibility and reconnect remain unqualified;
reconnect is not implemented. No merge-target comparison was performed.

## Files changed

- `AGENTS.md` — formatted the last two protocol rules as requested bullets.
- `docs/AI_HANDOFF.md` — updated checkpoint, runtime evidence, qualification and continuation status.

These are the exact changes for this task. No production source or tests changed.

## Validation

C++: Not run for this documentation task. Committed P0.7.4 audit reports PASS, 111 C++ executables.
Python: Not run here. P0.7.4 audit reports PASS, 49 audit Python tests.
QuestDB: Not run here. P0.7.4 audit reports PASS, 13 QuestDB Python tests.
Lua/SQL: Not run here. P0.7.4 audit reports PASS, 10 Lua fixtures and SQL/TSV fixture.
Build: Not run for documentation-only changes. P0.7.4 audit reports full MinGW validation build and separate `cmake --build build` PASS (DLL, testhost, loader, GUI).
git diff --check: PASS for this task.

Historical results above come from the committed P0.7.4 audit (2026-10-10,
247 validation records PASS, report recorded as
`/tmp/wow-validation-vvyf3kod/results.json`); the report itself was not revalidated.
Final `git status --short` confirms only the two intended documents changed.

## Runtime qualification

Status: **P0.7.4 RAW LOCATION LIFECYCLE OBSERVER — RUNTIME PASS**, as supplied by the user; limited to raw lifecycle observation.

- mapCandidate remains **UNQUALIFIED**.
- zoneCandidate remains **UNQUALIFIED**.
- areaCandidate remains **UNQUALIFIED**.
- ProfileWorldEvidence location population remains **BLOCKED**.

Evidence still required: Before profile consumption, establish source-qualified
lifetime/initialization, player binding, coherence and generation/ABA handling,
then independently qualify loading, teardown, same-map reload, map transfer and
zone/area-only changes. The observed lifecycle does not close these source gaps
or qualify any candidate. Vendor/water acceptance needs separate runtime evidence.

## Open blockers

- None for this documentation task.
- Qualified profile location consumption is blocked by the source gaps above.
- Vendor episode and water emergency egress runtime acceptance remain pending;
  consult the connection reliability audit and
  [WATER_SWIMMING_DROWNING_AUDIT.md](WATER_SWIMMING_DROWNING_AUDIT.md).
- Automatic reconnect remains unimplemented; R0.1 is not closed.

## Recommended next task

Reconcile the supplied runtime result with the full lifecycle capture in the
detailed P0.7.4 audit in a separately authorized task. Continue source research
for location lifetime/initialization and player binding before considering
ProfileWorldEvidence population. Do not promote raw candidates from matching
values alone. Keep Detour/NavMesh and existing controller ownership intact.

## Integration / merge status

Safe to commit: Yes, for these two documentation files only; no commit performed.
Safe to merge: These documentation edits are suitable for review/merge. Whole-branch integration is UNKNOWN: no target comparison or merge verification was performed; location population remains blocked despite the observer runtime PASS.
Manual runtime test required: No for this documentation task or recording the supplied observer PASS; yes for future qualified location consumption and the pending vendor/water qualifications.

## Persistent project statuses

VENDOR EPISODE — RUNTIME PENDING
WATER EMERGENCY EGRESS — RUNTIME PENDING
SOURCE GAP — RECONNECT NOT IMPLEMENTED
