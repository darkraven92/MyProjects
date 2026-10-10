# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `84e5c4996f9d8bb2f0744feaa8439a35a19777ed`
— `docs: trace p07 callback boundaries and model replay lifetime`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: statusbar effects, script mutation
and scene ownership. Three parallel read-only agents traced color methods,
XML/SetScript binding and model/scene ownership. Coordinator checked key binary
paths and current reader/adapter/tests, and reconciled an acquisition-obligation
matrix. No production code, tests, observer contract, hooks, controller ownership
or navigation changed. Work remains confined to this P0.7 worktree and branch.

The known statusbar color tail ends in UI dirty marking; SetScript and XML can
replace/remove the exact callback slot. Scene construction, model association
and widget teardown are connected, but complete callback-context retention and
teardown exclusion remain unproved. **SOURCE GAP remains; profile location
population stays BLOCKED.** No sampler design or runtime defect is established.

Detailed anchors, reproduction ranges and the proof-obligation matrix are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — statusbar effects, script mutation and scene ownership.” Earlier sections
retain executable/archive provenance and saved runtime capture evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four qualification flags remain false. Additional manual paths
  are outside that manifest; coverage is not exhaustive.
- HealthBar's successful native min/max path reads statusbar fields. Its color
  method parses/packs color and dispatches through a known texture to color/alpha
  storage and UI dirty marks. Allocation/error paths, other classes/bindings and
  deferred consumption remain open. Prior callback-before-world-descriptor-read
  ordering is unchanged; no destructive reentry is demonstrated.
- SetScript resolves the StatusBar OnValueChanged slot, releases its old reference
  and replaces it or clears it for nil. XML installation uses the same resolver;
  inherited template handling precedes local processing. These concrete mutation
  paths prevent archived declarations from proving a live installed closure.
- Widget scene creation initializes a count of one. Model creation stores a scene
  backpointer and links into its model list; replacement acquires the new model,
  releases the previous model and installs widget-context callbacks. The inspected
  direct replacement body has no separate old-context detachment before release.
- Base UI cleanup follows subclass model/scene release. Those later operations do
  not prove pre-release callback detachment. Sole scene ownership, retention of
  surviving old models' contexts, and teardown exclusion remain unqualified.
- Reconciled matrix keeps writer coverage, initialization/invalidation, lifetime,
  coherence, generation/ABA and player-bound cache freshness separate. Current
  raw reader repeats sequential reads; the adapter deliberately omits qualified
  map/positionMap/zone/worldGeneration. Area remains unsupported. Tests assert
  this Unknown boundary, not intra-read ABA detection or client-world safety.
- Prior exact listener identity/removal, model replay, shared-node/descriptor,
  resource-descendant and shared-TLS findings retain their limits. Prior archive
  priority rules still do not establish actual loaded scripts or overrides.

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

Closing a known color tail narrows one conditional route. It cannot substitute
for live script identity, complete callback coverage or retention. Scene creation
and release do not alone exclude teardown during callbacks. No runtime failure,
bulk world cleanup, nested field notification or safe sampling phase follows.

### UNKNOWN / SOURCE GAP

- Actual installed scripts/classes, archive/template selection and overrides;
  allocation/error effects, deferred UI dirty-state consumers and nested pumping.
- Complete scene ownership, callback removal when an old model survives release,
  widget/context retention during synchronous load/replay/event callbacks and
  exclusion of concurrent/nested teardown.
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

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — statusbar color effects, script
  mutation/inheritance, scene ownership/teardown and acquisition-proof matrix.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; key manual chains independently checked.
  No automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records;
  111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests,
  10 Lua fixtures, SQL/TSV fixture, full build and diff check.
  Report: `/tmp/wow-validation-wizuu9nt/results.json`.
- BUILD: separate `cmake --build build` PASS for DLL, testhost, loader and GUI.
- REVIEW: three read-only reviews reconciled; region/owner field wording corrected.
  Full diff/status review and `git diff --check` PASS; exactly the two intended
  documents changed, with no unrelated changes.
- RUNTIME: NOT RUN; prior observer qualification is not extended.

## Runtime qualification and blockers

Raw observer: **RUNTIME PASS** within prior limits. Location candidates:
**UNQUALIFIED**. ProfileWorldEvidence location: **BLOCKED** by source gaps.
Manual unload acceptance remains open. Vendor/water qualifications remain
pending, reconnect remains unimplemented, and R0.1 is not closed.
The bounded research is complete; no sampler design is qualified.

## Recommended next task

Prioritize model callback-context removal when an old model survives replacement,
and scene retention across synchronous load/replay/event callbacks. Trace deferred
consumption of the now-bounded UI dirty marks only where it bears on bulk world
cleanup or nested field notification. Keep actual script selection and mutable
bindings UNKNOWN without evidence; stock callback closure cannot establish all
installed effects. Use the audit's obligation matrix: require writer,
invalidation, lifetime, coherence and ABA evidence before sampler design.
Retain SOURCE GAP while incomplete; client mutation helpers are not observe-only
acquisition.

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
