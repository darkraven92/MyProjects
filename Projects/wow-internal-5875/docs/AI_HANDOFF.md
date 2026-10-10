# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `b3101eb87d4e5532e3b1eabae3a484f794760684`
— `docs: trace p07 script callbacks and deferred close`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: declared listener and resource
callback effects. Three parallel read-only agents traced script bindings,
loading continuations and secondary window targets. Coordinator reconciled
results, checked key sites and narrowed object/graphics virtual bindings.
No production code, tests, observer contract, hooks, controller ownership or
navigation changed.

Identified a concrete stock farsight listener declaration in local assets,
resource continuations that outlive request-pointer clearing, and GUI callbacks
behind HWND properties. These narrow source obligations without proving the
live callback set, complete side effects or retained-reference safety.
**SOURCE GAP remains; profile location population stays BLOCKED.**

Detailed anchors, exact asset hashes, reproduction ranges and limits are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — declared listener and resource callback effects.” Earlier sections
retain exact executable provenance and saved runtime capture evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four qualification flags remain false. Additional manual paths
  and archive extraction are outside that manifest; coverage is not exhaustive.
- Local patch.MPQ FrameXML.toc/XML/Lua declare the stock PetActionBar listener
  for PLAYER_FARSIGHT_FOCUS_CHANGED. Its event branch calls PetActionBar_Update;
  the apparent ControlReleased call is commented out. Coordinator re-extracted
  three members and matched pinned hashes. This is asset evidence, not live
  installation or archive-precedence evidence.
- Native event registration/removal, register-all and target cleanup are mapped.
  Register-all prevents literal event searches from enumerating every listener.
  Pet API registrations are bound, but UI methods, script overrides and native
  helper effects remain open. UI event nodes and object-field listeners are
  distinct protocols; one cannot supply the other's lifetime guarantee.
- Selected unit/player getter slots resolve to movement helpers, which may
  resolve another object and invoke more virtuals. GameObject position/angle
  helpers can invoke object+0x210 virtual slots +0x44/+0x48. These mappings
  narrow targets without proving all types, all effects or retained lifetime.
- Loading callbacks parse textures/map/model resources, publish resource globals,
  and can submit additional texture work. Request pointers in 0x6C3840,
  0x6C3F50 and the earlier 0x71D5E0 path clear before downstream completion.
  The 0x71D640 success bit is set before dependent callbacks. Neither pointer
  null nor a local ready bit establishes completion of all descendants.
- Graphics allocation slots are conditionally resolved for the two known
  factory tables; backend/helper/error callback closure remains incomplete.
  Resource/global writes are not established map/zone/area or manager writes.
- Secondary window targets resolve through GetPropA/SetPropA OsGuiPointer and
  two control vtables into dynamic local/parent callbacks. Literal-address and
  earlier direct-reference absence do not prove the route cannot activate.
- Earlier inline descriptor storage, shared listener nodes, bulk cleanup,
  deferred close and shared-TLS worker limits remain. Profile location/generation
  stay absent, area unsupported, XYZ without a qualified map association.

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

Asset declarations and virtual mappings narrow possible paths but do not establish
actual live callbacks or transitive absence of cleanup/nested traversal. Request
and ready flags are insufficient full-continuation witnesses. No destructive
reentry, torn sample, unsafe runtime overlap or gameplay failure was observed.

### UNKNOWN / SOURCE GAP

- Actual FrameXML/archive selection, installed/addon-mutated event targets,
  synchronous UI scripts/native helpers and complete callback closure.
- Resource-owner retention, decoder/backend/error callbacks, all submitters,
  remaining object virtuals, GUI activation/callbacks and helper TLS effects.
- Objects/descriptors/listeners retained across callbacks; actual writer threads,
  complete writer/counter and initialization/invalidation coverage, coherent
  sampling, same-map/character/address ABA and player-bound zone/area freshness.
- Completed unload, post-stop responsiveness, visible-screen/loading annotations
  and heartbeat continuity during unchanged dwell remain unestablished. Missing
  completion telemetry does not establish unload failure.
- Live Glue/UI eligibility remains unqualified; reconnect is unimplemented.
  No merge-target comparison or whole-branch integration verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — asset/native bindings, bounded
  resource/GUI effects, reproduction/provenance and remaining proof obligations.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; three extracted asset member hashes
  independently matched. No automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records;
  111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests,
  10 Lua fixtures, SQL/TSV fixture, full build and diff check.
  Report: `/tmp/wow-validation-j9fy1hek/results.json`.
- BUILD: separate `cmake --build build` PASS for DLL, testhost, loader and GUI.
- REVIEW: three read-only reviews reconciled; no actionable corrections.
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

Trace the declared stock pet-action update's UI methods and possible synchronous
OnShow/OnHide scripts; establish FrameXML selection and the limits of addon/event
listener enumeration. Prioritize routes to bulk object cleanup or nested field
notification while outer references remain live. Resolve resource-owner
retention/descendant completion and outstanding object/GUI callback targets as
needed for that proof. Close writer, invalidation, coherence and ABA obligations
before any profile location sampler design; retain SOURCE GAP while incomplete.
Client mutation helpers are not an observe-only acquisition mechanism.

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
