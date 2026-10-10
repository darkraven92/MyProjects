# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `fb9b574b5188e5774f8d5a5222351b0a6659853f`
— `docs: checkpoint p07 source research and automate Git workflow`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. Final task contents comprise the two documentation files
below, uncommitted at handoff preparation. The commit containing this handoff
is the authoritative checkpoint; publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: queued transfer execution,
packet dispatch, PLAYER_FARSIGHT notifications and cache publication ordering.
Three parallel read-only agents researched scheduling, field writers and packet
ownership. Coordinator reconciled their findings, checked key disassembly and
traced cache suppression. No production source, tests, reader contract, hooks,
controller ownership or navigation changed.

New paths establish local timer/event-6/event-5 ordering, nested packet-handler
dispatch, field subscriptions and individual descriptor stores. They do not
establish an eligible serialized sampling phase or complete lifetime contract.
**SOURCE GAP remains; profile location population stays BLOCKED.**

Detailed anchors, limits and reproduction ranges are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — queued transfer, packet dispatch and field notifications.” Earlier
sections retain the exact executable hash and saved runtime capture evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client audit PASS: SHA256/PE layout, 43 instruction anchors, 12 strings.
  All four location/lifetime flags remain false. New manual paths are outside
  that manifest; linear searches do not establish all indirect/alias writers.
- Deferred transfer callback is invoked by timed-queue consumer 0x428510 after
  releasing its context lock. One inspected main-loop branch runs due callbacks,
  optional window-message processing, event 6, queued events, then event 5.
  Both direct and created-thread entry paths exist; actual world-writer thread
  and context ownership remain unqualified.
- Event 6 registers a packet pump. Its inspected connection-list and queue locks
  remain held through packet-handler execution. These are not proven locks for
  every world/cache writer or safe locks for an observer to acquire.
- Direct different-map transfer can replace the manager inside temporary packet
  ownership before restore. Handler 0x603CE0 calls packet dispatcher 0x537AA0
  again for embedded buffers. This is nested handler dispatch, not evidence of
  a runtime transfer failure or nested switch/restore-wrapper entry.
- PLAYER_FARSIGHT subscribes an eight-byte relative field range. Its callback
  resolves the active player's alternate object. Removal can defer; callback
  completion ordering remains open. Generic descriptor mutation stores one
  DWORD per loop iteration, including potential indices 0x2C8/0x2C9.
- Additional preferred-GUID cleanup clears precede one conditional free but
  follow earlier cleanup calls. A packet-driven path changes object+0xC58 bit
  0x400; its gameplay meaning remains UNKNOWN.
- Cache suppression can skip numeric publication; deduplication markers may
  publish before helpers and the numeric writer. Markers are not a generation
  or completed-publication witness. Profile location/generation remain absent,
  area unsupported, and XYZ has no qualified map association.

### RUNTIME OBSERVED

**P0.7.4 RAW LOCATION LIFECYCLE OBSERVER — RUNTIME PASS**

Preserved prior qualification, limited to saved capture
`runtime-captures/p074-location-lifecycle-retry-2026-10-10/`, PID `296`, session
`296.134360886272945680.57555570.500`. No new capture inspection, WoW run,
attach, input, native invocation or memory write occurred in this task.

The earlier capture showed manager loss/stale zone-area, later clearing and
return with the same GUID/new player pointer; all five signature samples passed
with unqualified/sequential/no-owner/no-command labels. Stop and logical detach/
unload-request telemetry do not establish completed module unload. Detailed
hashes and rows remain in the audit; earlier PID 300 records are separate.

### INFERRED

Released callback locks, sequential field stores and late clears explain why
local event order is insufficient for qualification. They do not prove a torn
sample, erroneous restore or gameplay failure occurred in WoW.

### UNKNOWN / SOURCE GAP

- Actual world-writer thread/context, complete writer/callback coverage,
  notification completion and deferred removal, window/loading reentrancy,
  initialization/invalidation, coherent samples and same-map/character/address
  ABA. Player-bound zone/area ownership and freshness remain unqualified.
- Completed unload, post-stop responsiveness, visible-screen/loading annotations
  and heartbeat continuity during unchanged dwell remain unestablished. Missing
  completion telemetry does not establish unload failure.
- Live Glue/UI eligibility remains unqualified; reconnect is unimplemented.
  No merge-target comparison or whole-branch integration verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — bounded queue/packet/field/cache
  research, reproduction ranges, proof limits and next investigation.
- `docs/AI_HANDOFF.md` — final task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS. New bounded source paths reviewed
  independently; no automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records: 111 C++
  executables, 49 audit Python tests, 13 QuestDB Python tests, 10 Lua fixtures,
  SQL/TSV fixture, full MinGW build and diff check.
  Report: `/tmp/wow-validation-o3yxzmhz/results.json`.
- BUILD: separate `cmake --build build` PASS (DLL, testhost, loader, GUI).
- REVIEW: three read-only reviews found no actionable corrections. Full diff
  and worktree status inspected: exactly the two task documents, no unrelated
  changes; `git diff --check` PASS.
- RUNTIME: NOT RUN; preserved observer qualification is not extended.

## Runtime qualification and blockers

Raw observer: **RUNTIME PASS** within prior limits. Location candidates:
**UNQUALIFIED**. ProfileWorldEvidence location: **BLOCKED** by source gaps.
Manual unload acceptance remains open. Vendor/water qualifications remain
pending, reconnect remains unimplemented, and R0.1 is not closed.
This bounded source-research task is complete; no sampler design is qualified.

## Recommended next task

Connect masked descriptor-update completion (`0x466590`) to field-notification
execution and deferred removal (`0x467FB0/0x468023–057`), including destruction
and temporary-owner scopes. Trace context creation/selection (`0x421430`) and
window-context assignment to establish which context/thread runs world setup,
packet event 6, zone-update event 5 and transfer callbacks. Cover window/loading
callback reentrancy, initialization/invalidation, coherence and ABA before any
profile location sampler design. Retain SOURCE GAP if proof remains incomplete.

Separately obtain manual module-absence/post-stop responsiveness evidence for
unload acceptance. Future qualified location consumption also needs independent
loading, teardown, same-map reload, map-transfer and zone/area-only runtime cases.
Matching values or ordinary logout/login cannot qualify location by themselves.
Preserve Detour/NavMesh and controller ownership.

## Integration and Git checkpoint status

Manual runtime evidence required: No new run for this source-only task; yes for
unload acceptance, future qualified location and pending vendor/water evidence.
Safe to commit: Yes, exactly these two files; tests/build, documentation review
and diff checks passed. Staged checks are required before committing.
Safe to merge: Documentation checkpoint only; no automatic merge authorized.
Whole-branch integration remains UNKNOWN and profile location remains blocked.

AGENTS.md authorizes automatic checkpoint/push after all gates pass. Live origin
matched starting HEAD before checkpoint creation. The commit containing this
handoff is authoritative, avoiding a self-updating SHA loop. Commit/push results,
local HEAD versus origin tracking/live SHA, and final worktree status are checked
after commit and reported in the final response; do not infer verified push from
this pre-commit handoff. Recheck with `git rev-parse HEAD`, `git rev-parse
origin/codex/p07-world-zone-preparation`, `git ls-remote --heads origin
refs/heads/codex/p07-world-zone-preparation`, and `git status --short`.

## Persistent project statuses

VENDOR EPISODE — RUNTIME PENDING
WATER EMERGENCY EGRESS — RUNTIME PENDING
SOURCE GAP — RECONNECT NOT IMPLEMENTED
