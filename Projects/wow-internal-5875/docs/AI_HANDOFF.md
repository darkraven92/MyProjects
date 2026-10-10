# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `0ad53433a7e3fb49456c268fcf86372b34cca108`
— `docs: trace p07 render cleanup and callback ownership limits`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 acquisition closure research, 2026-10-10. Single coordinator; no subagents.
One coherent pass covered render reset/removal, callback-list mutation, deferred
UI teardown, scene/widget/model lifetime, queued context invalidation, concrete
world cleanup/notification routes, writer scope, publication, freshness and ABA.

**Decision B: SOURCE GAP remains; ProfileWorldEvidence location stays BLOCKED.**
The remaining source requirements consolidate into three explicit blockers:
protected acquisition, lifecycle/identity binding, and player-bound cache freshness.
No serialized read boundary is qualified. No sampler, production source, tests,
observer contract, controller ownership or navigation changed. This closes this
research block, not P0.7.4 location qualification. Stop peripheral UI tracing until
new evidence directly addresses the blockers; advance the independent offline
catalogue/provenance roadmap task below.

Detailed anchors, reproduction ranges, candidate-boundary decisions and the final
acquisition matrix are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — acquisition closure review and bounded research decision.” Earlier
sections preserve provenance and historical runtime evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings; all four qualification flags and runtimeObserved remain false.
  Additional manual paths are outside that manifest; coverage is not exhaustive.
- Render registration final release unlinks/frees the record without a local
  active-callback check. Its dispatch loop saves the next pointer without a local
  retention bracket. Layer records have their separate previously traced reset.
- Generic event traversal instead uses a linked stack cursor skipped by the
  removal helper. This narrows local registration-removal behavior; it does not
  retain render/model records, callback contexts or world objects.
- Ordinary event-5/render phase ordering and direct render bypass are connected.
  Deferred ReloadUI consumption reaches the concrete UI-context deleting virtual,
  frame/layer reset and late registration release; it also removes its event-5
  registration. UI destruction does not establish bulk world destruction.
- Model queues copy callback/context values and retain the model. Replacing model
  callback fields does not itself rewrite copied records. Delivery rereads a record
  after callback return; growth can move the buffer. Installed animation callback
  has no direct current-model/epoch comparison. Actual invalid delivery is unproved.
- Ordinary listener pending removal differs from bulk cleanup. Known bulk cleanup,
  packet nesting and teardown callers are concrete; no new installed-script route
  to nested field notification/bulk cleanup or complete exclusion is established.
- Manager publication precedes setup; root invalidation follows bulk object cleanup.
  Preferred player/cache update skips and separate publication remain incompatible with
  assuming current-player freshness from matching values. Window/TLS/event/packet
  scopes do not establish all-writer ownership. Map/positionMap/zone/worldGeneration
  remain empty in the adapter; area unsupported, XYZ has no map association.

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

A→B→A roots, same-address reuse, a writer paused between stores, and delivery of a
copied old callback context are proof obligations, not observed races or bugs.
Separate local counts, cursors and completion mechanisms cannot be combined into
an unproved end-to-end lifetime/coherence contract. More stock UI leaf closure
cannot by itself discharge independent acquisition and cache-publication gaps.

### UNKNOWN / SOURCE GAP

1. B1 — protected acquisition: complete relevant writer/thread ownership and
   exclusion of destructive/reentrant callbacks, or an equivalent retained snapshot.
   Includes outer object/descriptor/listener and dependent UI/model/buffer lifetime.
2. B2 — lifecycle/identity: readiness and pre-destruction invalidation bound to the
   exact player/manager/map/position sample, covering same-map/character/address ABA.
   A proven serialized interval could replace a generation protocol; none is qualified.
3. B3 — cache freshness: completed zone/area publication bound to the active player
   and sample. Keeping these Unknown does not resolve B1/B2 for a map-only sampler.

These are limits of currently qualified evidence, not a claim no boundary exists.
Actual installed scripts/overrides, callback overlap, surviving old contexts,
worker effects, archive selection/overrides and resource/TLS closure remain
unproved within B1/B2. Reopen research only for evidence targeting these blockers directly; matching values cannot do so.
Manual module absence, post-stop responsiveness, visible-screen/loading annotations
and unchanged-dwell heartbeat continuity remain unestablished; missing completion
telemetry does not prove unload failure. Live Glue/UI eligibility remains
unqualified. No merge-target comparison or whole-branch verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — coherent acquisition closure review,
  source anchors, final obligation matrix, three blockers and next roadmap step.
- `docs/AI_HANDOFF.md` — final task/evidence/validation/Git handoff.

## Validation

- SOURCE: exact-client offline audit PASS; manual instruction/table paths checked.
  No automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records, including
  111 C++ executables, audit/QuestDB Python tests, Lua/SQL/TSV fixtures and build.
  Report: `/tmp/wow-validation-2w33p715/results.json`.
- BUILD: separate `cmake --build build` PASS for DLL, testhost, loader and GUI.
- REVIEW: single-coordinator full diff/source-anchor review and `git diff --check`
  PASS; exactly the two intended documents changed, no staged/untracked unrelated
  files. No source, build artifact or capture changes included. Prior runtime
  block is byte-for-byte unchanged; persistent statuses preserved.
- RUNTIME: NOT RUN; prior observer qualification is not extended.

## Runtime qualification and blockers

Raw observer: **RUNTIME PASS** within prior limits. Location candidates:
**UNQUALIFIED**. ProfileWorldEvidence location: **BLOCKED** by B1–B3.
Manual unload acceptance remains open. Vendor/water qualifications remain
pending, reconnect remains unimplemented, and R0.1 is not closed.
This bounded research block is complete; no sampler design is qualified.

## Recommended next task

Implement an isolated offline leveling-profile catalogue/quest-reference provenance
validator. `LevelingProfileValidation` currently checks structure, IDs and links,
not reference resolution/provenance against the quest catalogue. Inspect
`VanillaQuestDatabase`, `tools/quest_catalogue_audit.cpp` and
`tools/questdb/areas/orc_starting_route.json`; define a bounded authored input and
version/source identity contract, reject missing/ambiguous references/provenance,
and add deterministic malformed/reference/version-change fixtures. Document reload
invalidation before runtime consumption. Keep quest existence separate from runtime
executability/completion and authored map metadata separate from live location.
No owner integration, location sampler, automatic travel or quest execution.
Preserve Unknown fields and all controller/Detour/NavMesh ownership boundaries.

This is an independent remaining P0.7 roadmap step, not completion of location
qualification. Do not resume open-ended peripheral UI research. Reopen B1–B3 only
with targeted new acquisition/lifecycle/publication evidence. Separately obtain
manual module-absence/post-stop responsiveness evidence for unload acceptance.
Future location qualification still requires independent loading, teardown,
same-map reload, transfer and zone/area-only cases after source qualification.

## Integration and Git checkpoint status

Manual runtime evidence required: No new run for this documentation research;
yes for unload acceptance, future location and pending vendor/water evidence.
Safe to commit: Yes, required validation/build and reviewed worktree checks passed
for exactly the two documents. Staged checks are required before commit.
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
