# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `f63434828cbf6e191bb515cd8095c720fdb96741`
— `docs: trace p07 script mutation and scene ownership`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: surviving model callbacks and
deferred UI delivery. Three parallel read-only agents traced model release,
scene retention and UI dirty consumers. Coordinator checked key binary chains
and current raw-reader/adapter boundaries, and narrowed the load callback body.
No production code, tests, observer contract, hooks, controller ownership or
navigation changed. Work remains in this P0.7 worktree and branch.

Nonzero model release does not detach callbacks. Other owners explicitly clear
them, without establishing that cleanup for the widget. Deferred UI delivery can
run scripts before further native reads; scene/record/context retention remains
unproved. **SOURCE GAP remains; profile location population stays BLOCKED.**
No sampler design, live overlap or runtime defect is established.

Detailed anchors, reproduction ranges and limits are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — surviving model callbacks and deferred UI delivery.” Earlier sections
retain the acquisition-proof matrix, provenance and saved runtime evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four qualification flags remain false. Additional manual paths
  are outside that manifest; coverage is not exhaustive.
- The model release's nonzero branch only updates its count and returns; callback
  fields remain unchanged by that operation. Direct animation/render setters
  only store fields. Load delivery clears its callback after return, leaving the
  context field unchanged in that body; readiness/descendant work remains relevant.
- Explicit null callback registrations precede release in separately inspected
  owners. Those owners are not established as widget+0x318 replacement. Actual
  old-model survival, complete caller-side removal and queued cancellation remain open.
- Scene-acquire helper exists, but bounded direct-reference/literal searches found
  no uses. Inline/aliased retention and caller-owned references are not excluded.
  Inspected update/drain, load and resource wrappers have no local scene-retain
  bracket; they access scene/model/record storage after callbacks return.
- Render-registration container counting is distinct from scene retention. The
  known load callback links a model into a scene list, calls further scene/resource
  helpers, copies fields to the widget and invalidates layout; it is not empty.
- Dirty consumers invoke virtuals and deferred callback records. A model-widget
  route reaches OnUpdateModel before further widget/model accesses; delivery reads
  node+4 after callback return. Texture records also retain raw storage pointers.
  Layer reset frees callback nodes; exclusion during delivery is not established.
  This does not prove tooltip color changes select a model callback or live reentry.
- Writer/invalidation/lifetime/coherence/ABA/player-bound freshness obligations
  remain separate and open. Raw reads are sequential; map/positionMap/zone/world
  generation remain unqualified, area unsupported. Prior archive/script mutability,
  resource-descendant and shared-TLS limits remain unchanged.

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

Counts and callback stores operate at different scopes, and cannot alone establish
complete context or scene lifetime. Deferred scripts followed by native reads
increase the retention obligation without proving destructive runtime reentry,
nested field notification or a qualified sampling phase.

### UNKNOWN / SOURCE GAP

- Actual surviving old models, installed callbacks/classes and list membership;
  complete caller-side clearing and queued-record cancellation.
- Mutation-safe callback/list traversal, layer-reset exclusion and scene/widget/
  context retention across synchronous load, replay, render and event delivery.
- Outer object/descriptor/listener lifetime; resource-owner/descendant completion,
  remaining native/GUI effects and TLS behavior; concurrent/nested teardown exclusion.
- Actual archive opens/selectors/views/mutations, installed scripts/overrides,
  earlier file paths, signature acceptance and open-callback effects.
- Actual writer threads, complete writer/counter and initialization/invalidation
  coverage, coherent sampling, same-map/character/address ABA, player-bound
  zone/area freshness.
- Completed unload, post-stop responsiveness, visible-screen/loading annotations
  and heartbeat continuity during unchanged dwell remain unestablished. Missing
  completion telemetry does not establish unload failure.
- Live Glue/UI eligibility remains unqualified; reconnect is unimplemented.
  No merge-target comparison or whole-branch integration verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — surviving release/clearing scopes,
  scene-retention limits, load callback effects and deferred UI delivery/reset.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; key manual chains independently checked.
  No automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records;
  111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests,
  10 Lua fixtures, SQL/TSV fixture, full build and diff check.
  Report: `/tmp/wow-validation-b5kqnkpi/results.json`.
- BUILD: separate `cmake --build build` PASS for DLL, testhost, loader and GUI.
- REVIEW: three read-only reviews reconciled; callback-store scope and widget
  model-pointer wording corrected. OnUpdateModel binding independently rechecked.
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

Trace render-record removal/reset callers and list mutation during callbacks;
determine which caller-owned scene/widget references cover post-callback reads.
For surviving old models, trace context invalidation beyond the bounded release/
setter bodies without importing other owners' cleanup guarantees. Prioritize
concrete routes to bulk cleanup/nested notification and scheduling exclusion.
Use the audit's acquisition matrix: require writer, invalidation, lifetime,
coherence and ABA evidence before sampler design. Retain SOURCE GAP while
incomplete; client mutation helpers are not observe-only acquisition.

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
