# AI Handoff — wow-internal-5875

## Repository state

Branch: `codex/p07-world-zone-preparation` (observed in this worktree).
Starting checkpoint and current HEAD: `f10daba3d955808de2c55f789153ee2730cb5031`
— `chore: add AI handoff protocol`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start; now contains uncommitted documentation changes listed below.
Nothing staged or committed. Historical branch/path descriptions in AGENTS.md
and CODEX_PROJECT_STATE.md do not describe this checkout.

## Task and result

Roadmap phase: P0.7.4 lifecycle capture reconciliation and location-source
research continuation. Completed 2026-10-10, documentation only.

Reconciled the prior user-supplied observer PASS against the full saved retry
capture, corrected the manual unload runbook to match existing telemetry, and
continued exact-client research into the zone updater's preferred GUID.
Three parallel read-only agents independently researched runtime evidence,
location sources and validation/status boundaries before coordinator edits.
Their reconciled conclusion: retain the observer PASS, distinguish logical
detach from completed unload, and keep profile location population blocked.
No production source, tests, gameplay ownership or navigation changed.

Detailed evidence, capture hashes, exact instruction addresses, reproduction
commands and next research scope are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
P0.7.4 capture reconciliation and source research continuation.

## Evidence

### SOURCE VERIFIED

- Current branch, clean starting status, history and source were inspected.
- Exact executable SHA256/PE layout, 43 instruction anchors and 12 strings
  pass the existing offline audit. This qualifies bounded source provenance
  only; all four current-location/lifetime qualification flags remain false.
- Additional manual disassembly confirms separate preferred-GUID stores at
  `0x600791/0x60079A`, equality suppression, and type-8 lookup after publication.
  Both active-GUID and object-derived callers exist. These new bounded paths
  are not part of the automated 43-anchor manifest or an exhaustive call graph.
- Raw observer sampling is sequential, with no lifetime/generation contract.
  Profile map/zone and position-map association remain absent; area unsupported.
- DllMain has no DLL_PROCESS_DETACH completion logger. UnloadSelf logs its
  request before FreeLibraryAndExitThread; logical IPC detach precedes unload.

### RUNTIME OBSERVED

**P0.7.4 RAW LOCATION LIFECYCLE OBSERVER — RUNTIME PASS**

Prior user-supplied status preserved and now corroborated by direct inspection
of `runtime-captures/p074-location-lifecycle-retry-2026-10-10/`:
`wow-internal.log` lines 11–21 and `wow-internal.lifecycle.log` lines 19–34.
PID `296`, session `296.134360886272945680.57555570.500`.
These are ignored local artifacts; the tracked detailed audit preserves hashes
and the sanitized five-row evidence table. Earlier PID 300 records are separate.
No new WoW run, attach or input was performed in this task.

- In-world: manager `0x14fe508`, GUID `0x1aa56`, player `0xde68008`, candidates
  map/zone/area `1/14/363`.
- Manager loss: identity unavailable, map unknown, zone/area retained `14/363`;
  subsequent sample clears zone/area to `0/0` with identity explicitly unknown.
- New manager `0x8d35d08`: GUID zero, player unknown, candidates `1/0/0`.
- Return: same GUID, new player `0x19da8008`, candidates `1/14/363`.
- File fingerprint and all five live-signature samples PASS; all rows retain
  unqualified/sequential/no-owner/no-command labels. Server predicate remains
  yes; historical charselect does not establish the visible screen.
- GUI Stop Bot, observer STOP, logical RUNTIME DETACHED and unload_requested
  are recorded. These confirm stop-path telemetry, not completed module unload.

### INFERRED

Observed stale zone/area and early map publication agree with the traced late
reset/early publication paths. They do not identify exact execution points at
sampling time, prove current-player location, coherence or a world generation.

### UNKNOWN / SOURCE GAP

- SOURCE GAP: qualified world lifetime/initialization, alternate GUID ownership,
  player-bound zone/area freshness, coherence, generation/ABA and serialized
  sampling boundary. Profile location population remains BLOCKED.
- SOURCE GAP: completed-unload telemetry is absent. Actual module absence,
  post-stop client responsiveness, visible-screen/loading annotations and
  heartbeat continuity during unchanged dwell are not established by capture.
  Missing completion telemetry does not establish an unload failure.
- Live Glue/UI eligibility and reconnect remain unqualified; reconnect is not
  implemented. No merge-target comparison or whole-branch verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — reconcile saved runtime evidence,
  clarify historical status and unload runbook, record bounded source research.
- `docs/AI_HANDOFF.md` — current checkpoint, evidence, validation and next task.

## Validation

- SOURCE: exact-client offline source audit PASS; bounded additional disassembly
  independently reviewed. Five documented capture rows and fixed qualification
  labels match the saved retry log in a separate comparison; hashes verified.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records: 111 C++
  executables, 49 audit Python tests, 13 QuestDB Python tests, 10 Lua fixtures,
  SQL/TSV fixture, full MinGW build and diff check.
  Report: `/tmp/wow-validation-gkxuejoo/results.json`.
- BUILD: separate `cmake --build build` PASS (DLL, testhost, loader, GUI).
- REVIEW: independent read-only evidence/source reviews found no corrections;
  complete diff inspected; `git diff --check -- .` PASS. Only the two listed
  documents changed, nothing staged; production source/tests unchanged.
- RUNTIME: saved capture inspected only; no new run. Test/build success does
  not extend the raw-observer runtime qualification.

## Runtime qualification and blockers

- Raw lifecycle observer: **RUNTIME PASS**, limited as described above.
- mapCandidate / zoneCandidate / areaCandidate: **UNQUALIFIED**.
- ProfileWorldEvidence location population: **BLOCKED** by source gaps.
- Full manual-runbook unload acceptance: completed module unload and subsequent
  client responsiveness still need independent observation.
- Vendor episode and water emergency egress remain runtime pending; automatic
  reconnect remains unimplemented and R0.1 is not closed.

No blocker to this documentation/research task. No candidate was promoted and
no runtime gameplay fix is claimed.

## Recommended next task

Continue source research into a defensible serialized sampling/lifetime boundary:
trace update scheduling against constructor publication, object destruction and
temporary owner switching. Finish the preferred zone-updater GUID ownership
analysis across its callers. Establish initialization/invalidation ordering and
coherence/ABA handling before designing or implementing any profile location
sampler. Retain SOURCE GAP if the evidence does not establish that contract.

Separately close the existing unload runbook with manual module-absence and
post-stop client-responsiveness evidence. Matching values or another ordinary
logout/login alone cannot qualify profile location. Future qualified consumption
also needs independent loading, teardown, same-map reload, map-transfer and
zone/area-only runtime cases. Preserve Detour/NavMesh and controller ownership.

## Integration / merge status

Manual runtime evidence required: No new run for this reconciliation/source
research; yes for outstanding unload acceptance, future qualified location
consumption and pending vendor/water qualifications.
Safe to commit: Yes, for the two listed documentation files; validation passed.
No commit requested or made.
Safe to merge: These documentation edits are suitable for review/merge.
Whole-branch integration remains UNKNOWN; no target comparison/merge
verification performed, and qualified profile location remains blocked.

## Persistent project statuses

VENDOR EPISODE — RUNTIME PENDING
WATER EMERGENCY EGRESS — RUNTIME PENDING
SOURCE GAP — RECONNECT NOT IMPLEMENTED
