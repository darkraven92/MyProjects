# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `0c4621dc6639389fa34bbe80bda521ee13533821`
— `docs: trace p07 listener assets and resource callback effects`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: visibility callbacks and FrameXML
selection boundaries. Three parallel read-only agents traced stock UI assets,
native visibility dispatch and file loading. Coordinator reconciled results,
checked key chains, re-extracted six members and traced ReloadUI request handling.
No production code, tests, observer contract, hooks, controller ownership or
navigation changed.

Distinguished immediate pet-widget updates from deferred parent-bar animation;
connected conditional native visibility/OnLeave script dispatch and narrowed
FrameXML signature, file flags and archive-open ordering. These facts do not
establish actual loaded scripts, complete callback effects or retained lifetime.
**SOURCE GAP remains; profile location population stays BLOCKED.**

Detailed anchors, asset hashes, reproduction ranges and limits are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — visibility callbacks and FrameXML selection boundaries.” Earlier
sections retain executable/archive provenance and saved runtime capture evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four qualification flags remain false. Additional manual paths
  and archive findings are outside that manifest; coverage is not exhaustive.
- Inspected pet-button/base/model XML has no OnShow/OnHide declarations; the
  parent bar has both, invoking global layout helpers. HidePetActionBar only
  changes animation fields; the separately bound OnUpdate performs bar Hide.
  This does not establish immediate parent OnHide in the farsight handler.
- Global layout can Show/Hide chat frames and reposition bags through mutable
  globals/tables. Cooldown helpers invoke model methods and Show/Hide. Six asset
  members were re-extracted with matching pinned lengths/hashes; availability
  does not establish actual client selection or absence of overrides.
- Native frame Show/Hide has conditional synchronous script dispatch. Visibility
  state changes precede child traversal and parent scripts; traversal accesses
  node links after child virtuals return. Script bookkeeping is not a proved
  outer world-object or listener-retention mechanism.
- A conditional Hide→OnLeave path is resolved through owner/state checks and a
  button override. The declared pet OnLeave hides GameTooltip. Actual live class,
  hover state and callbacks remain UNKNOWN; derived callbacks remain open.
  Texture/region Show/Hide paths differ from frame script dispatch.
- ReloadUI is a separate candidate, not found in the stock farsight handler.
  Its wrapper gates and sets a byte; an event-5 consumer later performs cleanup/
  reload calls. This narrows one request route without excluding nested pumping.
- FrameXML initialization checks signature/content, marks context cleanup on
  failure, then still falls through into loading; it is not an immediate return.
  This task did not execute the client verifier or establish signature acceptance.
- Startup flag initialization disables one later loose-file probe. Earlier
  index/mode paths and other callers remain open. Archive discovery/sorting/open
  sequence and numeric arguments are traced; final priority/member selection
  and actual successful opens remain UNKNOWN.
- Earlier inline descriptors, shared listener nodes, bulk cleanup, resource
  continuation ordering and shared-TLS worker limits remain. Profile location/
  generation stay absent, area unsupported, XYZ without a qualified map association.

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

Conditional native callbacks widen the effect-coverage obligation; deferred bar
animation and ReloadUI request handling narrow specific routes. Neither local
assets nor loader order establish all live callbacks or absence of destructive
reentry. No torn sample, unsafe runtime overlap or gameplay failure was observed.

### UNKNOWN / SOURCE GAP

- Selected archive/loose member bytes, actual scripts/addon overrides and complete
  reachable visibility/hover/model/layout callbacks; nested event pumping.
- UI traversal mutation safety and outer object/descriptor/listener retention;
  resource-owner/descendant completion, remaining object/GUI virtuals and TLS effects.
- Actual writer threads, complete writer/counter and initialization/invalidation
  coverage, coherent sampling, same-map/character/address ABA, player-bound
  zone/area freshness.
- Completed unload, post-stop responsiveness, visible-screen/loading annotations
  and heartbeat continuity during unchanged dwell remain unestablished. Missing
  completion telemetry does not establish unload failure.
- Live Glue/UI eligibility remains unqualified; reconnect is unimplemented.
  No merge-target comparison or whole-branch integration verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — stock/native visibility paths,
  FrameXML selection boundaries, ReloadUI route, provenance and proof limits.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; six asset member hashes independently
  matched. No automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records;
  111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests,
  10 Lua fixtures, SQL/TSV fixture, full build and diff check.
  Report: `/tmp/wow-validation-69w5d0v3/results.json`.
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

Follow the conditional pet-button OnLeave→GameTooltip:Hide route through the
actual tooltip class/OnHide and remaining button callbacks. Trace cooldown
SetSequence/model callback delivery where reachable from the stock update.
Determine whether these paths reach bulk cleanup or nested field notification;
distinguish UI-node mutation from retained outer world references. Complete
lower-level archive insertion/member selection as needed for loaded-script
provenance; do not assume open order is priority. Close writer, invalidation,
lifetime/coherence/ABA obligations before sampler design; retain SOURCE GAP
while incomplete. Client mutation helpers are not observe-only acquisition.

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
