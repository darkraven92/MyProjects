# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `e0808215461462588a407f1e6573a1e6f6abdefa`
— `docs: trace p07 surviving callbacks and deferred UI delivery`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: render cleanup ordering and
callback ownership limits. Three parallel read-only agents traced reset callers,
representation/parent membership and UI scheduling. Coordinator checked key
chains, current raw-reader/adapter boundaries and a conditional scene worker
handshake. No production code, tests, observer contract, hooks, controller
ownership or navigation changed. Work stays in this P0.7 worktree and branch.

Ordinary rebuilds reset records before registration/delivery; destruction also
reaches reset. Lua representation counts, parent membership and worker waits do
not establish complete native retention or exclusion during callbacks.
**SOURCE GAP remains; profile location population stays BLOCKED.**
No sampler design, live overlap or runtime defect is established.

Detailed anchors, reproduction ranges and limits are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — render cleanup ordering and callback ownership limits.” Earlier sections
retain the acquisition matrix, provenance and saved runtime evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four qualification flags remain false. Additional manual paths
  are outside that manifest; coverage is not exhaustive.
- Both frame-local and UI-context rebuilds reset dirty layers before registering
  and delivering records. Frame and UI-context destruction reach the same reset,
  which frees callback nodes. The direct delivery loop reads its node after
  callback return without a local retain/active-removal protocol; caller protection
  remains unproved. Ordinary ordering does not establish callback/reset overlap.
- UI scheduling registers a concrete update callback; it processes deferred
  deletion before update/render work. Context teardown separately deletes frames
  and layers before releasing its scheduler handle. Queue insertion exists, but
  bounded direct-reference/literal searches found no uses. Full enqueue reachability,
  registration removal semantics and reentry exclusion remain open.
- Additional model-destructor wrappers add no callback detachment. Script
  dispatch only creates a Lua representation when widget+4 is zero, without a
  paired per-call native retention bracket. Representation cleanup, parent-link
  membership and native deletion operate separately; complete ownership is unproved.
- A scene-helper prefix conditionally distributes alternating model-list work
  through a worker callback and event handshake. Thread creation and signal/wait
  paths are connected; actual activation is unknown and the caller does not check
  the completion wait result. This does not establish script/world-writer execution
  on that worker or a scene/widget lifetime guarantee.
- Writer/invalidation/lifetime/coherence/ABA/player-bound freshness obligations
  remain separate and open. Raw reads remain sequential; map/positionMap/zone/world
  generation unqualified, area unsupported. Archive/script mutability,
  resource-descendant and TLS limits remain unchanged.

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

Ordinary ordering, Lua representation, parent membership and worker completion
concern different scopes. They do not alone establish complete native context,
scene or raw-world lifetime. Concrete cleanup paths require scheduling proof;
they do not prove destructive reentry or a qualified sampling phase.

### UNKNOWN / SOURCE GAP

- Actual surviving old models, installed callbacks/classes and list membership;
  complete caller-side clearing, queued cancellation and native retention.
- Mutation-safe traversal, registration removal/reentry rules, concrete queue
  insertion/destruction callers and scene/widget/context lifetime across delivery.
- Actual worker activation, full model-helper effects and completion/exclusion
  guarantees; outer object/descriptor/listener lifetime and resource/TLS effects.
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

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — reset/destruction ordering,
  UI scheduling, representation/parent boundaries and conditional scene worker.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; key manual chains independently checked.
  No automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records;
  111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests,
  10 Lua fixtures, SQL/TSV fixture, full build and diff check.
  Report: `/tmp/wow-validation-ahchu5j2/results.json`.
- BUILD: separate `cmake --build build` PASS for DLL, testhost, loader and GUI.
- REVIEW: three read-only reviews reconciled; cleanup-before-free wording clarified
  and reproduction ranges completed. Full diff/status review and `git diff --check`
  PASS; exactly the two intended documents changed, with no unrelated changes.
  Prior runtime-observation block is unchanged; persistent statuses preserved.
- RUNTIME: NOT RUN; prior observer qualification is not extended.

## Runtime qualification and blockers

Raw observer: **RUNTIME PASS** within prior limits. Location candidates:
**UNQUALIFIED**. ProfileWorldEvidence location: **BLOCKED** by source gaps.
Manual unload acceptance remains open. Vendor/water qualifications remain
pending, reconnect remains unimplemented, and R0.1 is not closed.
The bounded research is complete; no sampler design is qualified.

## Recommended next task

Trace UI scheduler registration removal and dispatch reentry rules, and concrete
callers of context destruction/queue insertion. Determine whether those rules
protect post-callback widget/scene/record reads. Follow the scene worker handshake
only where completion/model effects bear on lifetime or writer exclusion.
Do not import parent/Lua/model counts as a complete ownership contract.
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
