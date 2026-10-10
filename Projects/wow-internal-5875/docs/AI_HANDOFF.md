# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `d67c9ba4a5e2a584cd0442056d3ac8df333dd9cc`
— `docs: trace p07 descriptor teardown and window ownership`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: script dispatch, loading
completion and deferred close. Three parallel read-only agents traced farsight,
loading and window routes. Coordinator reconciled the results, checked key
paths and connected context cleanup to manager destruction and worker shutdown.
No production code, tests, observer contract, hooks, controller ownership or
navigation changed.

Connected synchronous script execution inside farsight notification, loading
completion outside event 6, callbacks after worker termination, and TLS-selected
deferred close. These broaden callback coverage and narrow one teardown ingress
without proving exclusion on all paths. **SOURCE GAP remains; profile location
population stays BLOCKED.**

Detailed anchors, reproduction ranges and limits are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — script dispatch, loading completion and deferred close.” Earlier
sections retain the exact executable hash and saved runtime capture evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four location/lifetime flags remain false. Additional manual
  paths are outside that manifest; indirect/alias coverage is not exhaustive.
- Farsight callback dispatches PLAYER_FARSIGHT_FOCUS_CHANGED synchronously
  after its state-gated wrappers, before the outer notification executor resumes
  raw listener access. Registered scripts can reach dynamic native closures or
  the interpreter. Installed handlers' cleanup/nested-traversal effects remain
  UNKNOWN; flag tests and bounded direct-call absence do not prove exclusion.
- Loading worker marks completion separately from the normal callback consumer.
  The consumer sets entry+0x1C before invoking the callback, so that byte does
  not prove callback return. Synchronous waiter 0x443BD0 pumps that consumer;
  completion is not restricted to event 6.
- Shutdown waits for the worker, then can invoke alternate callbacks before
  deregistering event 6. One concrete alternate performs another read and then
  normal continuation; another registered completion callback can submit work.
  These paths do not establish world mutation or safe lifetime boundaries.
- Context cleanup can destroy the manager before loading drain/shutdown. This
  local sequence requires callback-effect coverage; no runtime fault is claimed.
- Window input routes through a bounded ring. The installed close handler reads
  element 0 of the current context TLS block, conditionally marks that context
  for cleanup and returns zero, suppressing the generic fallback. The main loop
  later dispatches cleanup events, reaches manager destruction through event 4
  and clears TLS afterward.
  This is a deferred ordinary-loop path, not proven recursive teardown inside
  the window callback. A secondary immediate window callback has unresolved
  activation and indirect targets.
- Profile location/generation remain absent, area unsupported, and XYZ has no
  qualified map association. Context IDs, event labels and completion flags are
  not qualified lifetime, world-generation or freshness witnesses. Earlier
  shared-TLS worker, inline descriptor storage and bulk-cleanup limits remain.

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

Event labels, request flags and worker termination cannot alone provide a safe
sampling boundary because additional synchronous callback scopes exist. The
deferred close route narrows one ingress without proving all other callback
paths exclude cleanup/nested traversal. No destructive reentry, torn sample or
gameplay failure was observed.

### UNKNOWN / SOURCE GAP

- Installed script/native listener effects, unresolved virtual targets, loading
  normal/alternate callback effects and all submitters, secondary window route
  activation, nested pumping and helper TLS preservation.
- Retention of objects/descriptors/listeners during callbacks, actual writer
  thread ownership, complete counter/writer coverage, initialization/invalidation,
  coherence, same-map/character/address ABA and player-bound zone/area freshness.
- Completed unload, post-stop responsiveness, visible-screen/loading annotations
  and heartbeat continuity during unchanged dwell remain unestablished. Missing
  completion telemetry does not establish unload failure.
- Live Glue/UI eligibility remains unqualified; reconnect is unimplemented.
  No merge-target comparison or whole-branch integration verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — bounded script/loading/close
  research, reproduction ranges, proof limits and next investigation.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; bounded disassembly independently
  investigated without automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records;
  111 C++ test executables, 49 audit Python tests, 13 QuestDB Python tests,
  10 Lua fixtures, SQL/TSV fixture, full build and diff check.
  Report: `/tmp/wow-validation-p3ma2u5s/results.json`.
- BUILD: separate `cmake --build build` PASS for DLL, testhost, loader and GUI.
- REVIEW: three read-only reviews reconciled; corrected context TLS element
  terminology. No remaining findings. Complete diff/status review and
  `git diff --check` PASS; exactly the two intended documents changed.
- RUNTIME: NOT RUN; prior observer qualification is not extended.

## Runtime qualification and blockers

Raw observer: **RUNTIME PASS** within prior limits. Location candidates:
**UNQUALIFIED**. ProfileWorldEvidence location: **BLOCKED** by source gaps.
Manual unload acceptance remains open. Vendor/water qualifications remain
pending, reconnect remains unimplemented, and R0.1 is not closed.
The bounded research is complete; no sampler design is qualified.

## Recommended next task

Identify installed `PLAYER_FARSIGHT_FOCUS_CHANGED` listeners and script/native
bindings; resolve outstanding object virtual calls against bulk cleanup/nested
notification. Trace loading normal/alternate callback bodies (including
`0x71D640`, `0x4497F0`, `0x44A500`, `0x6C21F0`, `0x6C3840`, `0x6C3F50`) and
remaining window dispatch routes to world writes or teardown. Establish
reachability/exclusion while outer references remain live and close writer,
lifetime/coherence/ABA coverage before any profile location sampler design.
Retain SOURCE GAP if proof remains incomplete. Client mutation helpers are not
an observe-only acquisition mechanism.

Separately obtain manual module-absence/post-stop responsiveness evidence for
unload acceptance. Future qualified location consumption also needs independent
loading, teardown, same-map reload, map-transfer and zone/area-only runtime cases.
Matching values or ordinary logout/login cannot qualify location by themselves.
Preserve Detour/NavMesh and controller ownership.

## Integration and Git checkpoint status

Manual runtime evidence required: No new run for this source-only task; yes for
unload acceptance, future qualified location and pending vendor/water evidence.
Safe to commit: Yes, validation/build, review and worktree diff checks passed
for exactly the two documentation files. Staged checks are required before commit.
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
