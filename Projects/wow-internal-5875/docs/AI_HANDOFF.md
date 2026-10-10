# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `bc00de2121da5b257597a101b6a20b3691a13938`
— `docs: trace p07 tooltip and model callback effects`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: tooltip callback boundaries and
model replay lifetime. Three parallel read-only agents traced coin/tooltip
callbacks, model replay/destruction and listener/button binding. Coordinator
reconciled findings and independently checked key binary paths and three asset
hashes. No production code, tests, observer contract, hooks, controller ownership
or navigation changed. Scope remains this P0.7 worktree and its current branch.

Tooltip removal cannot directly target the outer farsight listener. A separate
tooltip statusbar callback can run before a saved world-object descriptor is
reread. Queued model operations replay through resource completion; scene/context
retention remains unproved. **SOURCE GAP remains; profile location population
stays BLOCKED.** No runtime defect or safe sampling phase is established.

Detailed anchors, hashes, reproduction ranges and limits are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — tooltip callback boundaries and model replay lifetime.” Earlier sections
retain executable/archive provenance and saved runtime capture evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four qualification flags remain false. Additional manual paths
  and archive findings are outside that manifest; coverage is not exhaustive.
- Per-listener removal matches callback and context. Tooltip and outer farsight
  subscriptions differ in both, and category/offset; this direct removal cannot
  select the outer node. The active/pending protocol does not prove outer lifetime.
- Coin OnHide clears an owner flag and invokes sound; pickup/drop callbacks belong
  to different function bodies. Three member lengths/hashes independently matched.
  Actual script selection/overrides and native sound effects remain open.
- Tooltip statusbar helper can dispatch OnValueChanged before callback 0x529560
  rereads the saved world object's descriptor. The declared HealthBar handler
  normalizes a value and sets color; native color effects/overrides remain open.
  No destruction, stale read or nested field notification is demonstrated.
- FontString/anchor/region paths narrow to layout invalidation and UI-node
  mutations, distinct from object-manager cleanup. Downstream effects remain open.
- Button constructor zeros +0x31C; later binding remains UNKNOWN. Bounded setter
  reference searches cannot rule out indirect binding or alternative writers.
- Pending model operation code 4 replays the sequence setter during initialization;
  resource completion can enter that initializer. Operation/model/scene accesses
  follow the setter, adding lifetime obligations.
- Association installs animation and additional widget-context callbacks. The
  load callback may execute synchronously and is cleared after return. Inspected
  widget destruction releases model, scene and script references; direct bodies
  do not establish complete callback-context detachment.
- Scene destruction repeatedly releases models until zero, then frees event
  storage. A queued model increment alone cannot establish safety across scene
  teardown; scene ownership and teardown exclusion remain separate obligations.
  No overlap with pending delivery or an active callback is established.
- Earlier descriptor/shared-node, resource-descendant and shared-TLS limits remain.
  Map/zone/world generation stay unqualified, area unsupported, XYZ without a
  qualified map association. Prior conditional archive rules do not prove selection.

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

Listener identity narrows a specific removal route; callback-before-read ordering
and model replay/destruction identify remaining retention obligations. They do
not establish runtime failure, bulk cleanup, nested field notification, coherent
sampling or an execution phase safe for acquisition.

### UNKNOWN / SOURCE GAP

- Actual scripts/classes/bindings, statusbar color effects, downstream layout/
  sound/child callbacks and nested event pumping.
- Widget-context and scene retention, callback removal across replacement/
  destruction, replay/drain mutation and nested/concurrent teardown exclusion.
- Outer object/descriptor/listener lifetime; resource-owner/descendant completion,
  remaining object/GUI virtuals and TLS effects.
- Actual archive opens/selectors/views/mutations and open callback registration/
  effects, earlier file paths and signature acceptance.
- Actual writer threads, complete writer/counter and initialization/invalidation
  coverage, coherent sampling, same-map/character/address ABA, player-bound
  zone/area freshness.
- Completed unload, post-stop responsiveness, visible-screen/loading annotations
  and heartbeat continuity during unchanged dwell remain unestablished. Missing
  completion telemetry does not establish unload failure.
- Live Glue/UI eligibility remains unqualified; reconnect is unimplemented.
  No merge-target comparison or whole-branch integration verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — coin/statusbar/layout callbacks,
  listener identity, model replay/destruction, provenance and proof limits.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; three asset member hashes independently
  matched. No automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records;
  111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests,
  10 Lua fixtures, SQL/TSV fixture, full build and diff check.
  Report: `/tmp/wow-validation-cw5_kmge/results.json`.
- BUILD: separate `cmake --build build` PASS for DLL, testhost, loader and GUI.
- REVIEW: three read-only reviews reconciled; resource flag wording corrected
  to mask 0x1. Full diff/status review and `git diff --check` PASS; exactly the
  two intended documents changed, with no unrelated changes.
- RUNTIME: NOT RUN; prior observer qualification is not extended.

## Runtime qualification and blockers

Raw observer: **RUNTIME PASS** within prior limits. Location candidates:
**UNQUALIFIED**. ProfileWorldEvidence location: **BLOCKED** by source gaps.
Manual unload acceptance remains open. Vendor/water qualifications remain
pending, reconnect remains unimplemented, and R0.1 is not closed.
The bounded research is complete; no sampler design is qualified.

## Recommended next task

Prioritize the statusbar callback-before-world-read route: close native color
helper effects and actual script binding/override limits. Establish widget/model
scene ownership and callback removal across replacement/destruction; trace
remaining tooltip/layout paths for bulk cleanup or nested field notification.
Prove scheduling/retention rather than treating reference increments or callback
names as safety. Close writer, invalidation, lifetime, coherence and ABA
obligations before sampler design; retain SOURCE GAP while incomplete.
Client mutation helpers are not observe-only acquisition.

Separately obtain manual module-absence/post-stop responsiveness evidence for
unload acceptance. Future qualified location consumption also needs independent
loading, teardown, same-map reload, map-transfer and zone/area-only runtime cases.
Matching values or ordinary logout/login cannot qualify location by themselves.
Preserve Detour/NavMesh and controller ownership.

## Integration and Git checkpoint status

Manual runtime evidence required: No new run for this source-only task; yes for
unload acceptance, future qualified location and pending vendor/water evidence.
Safe to commit: Yes, validation/build, reviews and worktree diff checks passed
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
