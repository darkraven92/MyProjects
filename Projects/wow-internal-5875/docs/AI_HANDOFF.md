# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `7ede98b579414841c6112092e2d0ad05904dae7d`
— `docs: trace p07 transfer and field notification lifetimes`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: notification completion,
startup context and loading callback coverage. Three parallel read-only agents
traced notifications, scheduler setup and packet ownership. Coordinator
reconciled their findings, checked key disassembly and traced loading callbacks.
No production code, tests, observer contract, hooks, controller ownership or
navigation changed.

Connected descriptor mutation to the later packet notification pass and local
listener-removal completion. Connected startup context to packet pumping and
world initialization inside event 5. These are bounded source paths, not a
complete lifetime/coherence contract. **SOURCE GAP remains; profile location
population stays BLOCKED.**

Detailed anchors, reproduction ranges and limits are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — notification completion and startup context.” Earlier sections retain
the exact executable hash and saved runtime capture evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four location/lifetime flags remain false. Additional manual
  paths are outside that manifest; indirect/alias coverage is not exhaustive.
- Update packet 0xA9 mutates descriptors, rewinds its buffer, then executes a
  notification pass. Subscribed ranges are saved before mutation; callbacks
  compare current/saved bytes. Active listener removal marks pending; the
  notification executor unlinks/frees after callback return or equal-byte
  suppression, or clears active when removal is not pending.
  This establishes a local listener-node protocol, not object/manager lifetime.
- Normal packet switch/restore encloses mutation, notification and further
  virtual update hooks. Packet 0x1F6 directly forwards to the A9 handler without
  another wrapper. Callback completion and owner restoration are distinct.
- GUID object removal can defer using object+0xE8 and +0xE4 mask 0x10;
  0x4683E0/0x468410 increment/decrement and retry pending removal. These helpers
  mutate client state. Manager teardown's separate cleanup/destruction calls do
  not establish full coverage of that deferral mechanism.
- Concrete startup requests one scheduler slot, bypassing additional scheduler
  worker creation on that path. It creates a message-processing context, installs
  setup/cleanup callbacks and TLS storage, then selects it through the scheduler.
  This does not establish that all client writers run on one thread.
- Setup registers the packet pump in event 6 and a state callback in event 5.
  The latter can initialize the world, construct a manager and register the
  zone updater in event 5. Event 5 is not inherently a post-initialization phase.
- Transfer/loading execute registered work and progress callbacks, including
  calls after a queue lock is released. Indirect targets and window effects
  remain incomplete. No world reentry or runtime failure is inferred as fact.
- Profile location/generation remain absent, area unsupported, and XYZ has no
  qualified map association. Context IDs, callback return and deduplication
  markers are not qualified world generations or freshness witnesses.

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

The startup chain narrows context ownership, and the later notification pass
explains why returning from a field write is insufficient. Local ordering and
client-internal deferral do not establish a safe external read phase. No torn
sample, erroneous restore or gameplay fault was observed.

### UNKNOWN / SOURCE GAP

- Callback-triggered object/manager destruction, nested notification traversal,
  saved-descriptor lifetime, complete counter/teardown coverage, actual writer
  thread/window ownership, helper TLS effects and indirect loading callbacks.
- Complete writer coverage, initialization/invalidation, coherent samples,
  same-map/character/address ABA, and player-bound zone/area freshness.
- Completed unload, post-stop responsiveness, visible-screen/loading annotations
  and heartbeat continuity during unchanged dwell remain unestablished. Missing
  completion telemetry does not establish unload failure.
- Live Glue/UI eligibility remains unqualified; reconnect is unimplemented.
  No merge-target comparison or whole-branch integration verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — bounded notification/context/
  loading research, reproduction ranges, proof limits and next investigation.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; bounded disassembly independently
  investigated without automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records: 111 C++
  executables, 49 audit Python tests, 13 QuestDB Python tests, 10 Lua fixtures,
  SQL/TSV fixture, full MinGW build and diff check.
  Report: `/tmp/wow-validation-3aepci12/results.json`.
- BUILD: separate `cmake --build build` PASS (DLL, testhost, loader, GUI).
- REVIEW: three read-only reviews completed; equal-byte-suppression wording
  corrected, no remaining actionable findings. Full diff/status inspected:
  exactly two task documents, no unrelated changes; `git diff --check` PASS.
- RUNTIME: NOT RUN; prior observer qualification is not extended.

## Runtime qualification and blockers

Raw observer: **RUNTIME PASS** within prior limits. Location candidates:
**UNQUALIFIED**. ProfileWorldEvidence location: **BLOCKED** by source gaps.
Manual unload acceptance remains open. Vendor/water qualifications remain
pending, reconnect remains unimplemented, and R0.1 is not closed.
The bounded research is complete; no sampler design is qualified.

## Recommended next task

Trace callback-triggered object/manager destruction against listener cleanup
and saved-descriptor lifetime, including callers/scope of `0x4683E0/0x468410`
and teardown `0x467800`. Resolve loading callback targets and window-owner
creation/assignment against the startup context, including world initialization
inside event 5. Close reentrant traversal, helper TLS effects, complete writer
coverage, coherence and ABA before any profile location sampler design.
Retain SOURCE GAP if proof remains incomplete; client mutation helpers are not
an observe-only acquisition mechanism.

Separately obtain manual module-absence/post-stop responsiveness evidence for
unload acceptance. Future qualified location consumption also needs independent
loading, teardown, same-map reload, map-transfer and zone/area-only runtime cases.
Matching values or ordinary logout/login cannot qualify location by themselves.
Preserve Detour/NavMesh and controller ownership.

## Integration and Git checkpoint status

Manual runtime evidence required: No new run for this source-only task; yes for
unload acceptance, future qualified location and pending vendor/water evidence.
Safe to commit: Yes, exactly the two documentation files; validation/build,
review and diff checks passed. Staged checks are required before commit.
Safe to merge: Documentation checkpoint only; no automatic merge authorized.
Whole-branch integration remains UNKNOWN and profile location remains blocked.

AGENTS.md authorizes automatic checkpoint/push after all gates pass. The commit
containing this handoff is authoritative, avoiding a self-updating SHA loop.
Commit/push results, local HEAD versus origin tracking/live SHA, and final
worktree status are checked after commit and reported in the final response;
do not infer verified push from this pre-commit handoff. Recheck with
`git rev-parse HEAD`, `git rev-parse origin/codex/p07-world-zone-preparation`,
`git ls-remote --heads origin refs/heads/codex/p07-world-zone-preparation`,
and `git status --short`.

## Persistent project statuses

VENDOR EPISODE — RUNTIME PENDING
WATER EMERGENCY EGRESS — RUNTIME PENDING
SOURCE GAP — RECONNECT NOT IMPLEMENTED
