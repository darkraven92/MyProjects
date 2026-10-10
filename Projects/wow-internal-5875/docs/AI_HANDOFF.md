# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `5cbd7834402e38ec4115b27a6507ccb1d8802132`
— `docs: trace p07 notification completion and startup context`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: descriptor lifetime, bulk
teardown and window/TLS association. Three parallel read-only agents traced
listener/descriptor cleanup, counter callers and window creation. Coordinator
reconciled the results, checked key paths and resolved loading progress targets.
No production code, tests, observer contract, hooks, controller ownership or
navigation changed.

Established inline saved-descriptor ownership and distinct bulk listener
cleanup; extended teardown through late packet-handler removal. Bound startup
window creation/forwarding and identified a separate worker sharing the context
TLS block. These narrow source gaps without establishing callback exclusion or
a complete lifetime/coherence contract. **SOURCE GAP remains; profile location
population stays BLOCKED.**

Detailed anchors, reproduction ranges and limits are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — descriptor lifetime, bulk teardown and window association.” Earlier
sections retain the exact executable hash and saved runtime capture evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four location/lifetime flags remain false. New manual paths
  are outside that manifest; indirect/alias coverage is not exhaustive.
- Inspected active-player saved descriptor base is object+0x3178, within the
  same allocation as the live descriptors. Selected teardown frees that
  allocation, so the saved bytes are not an independently retained snapshot.
- Bulk listener cleanup unlinks/frees nodes without the single-listener
  active/pending checks. Notification traversal uses shared nodes and accesses
  their links after callback return. Reachability of destructive or nested
  callbacks remains unproved; no lifetime failure is claimed.
- Player/unit destructor forwarding and preferred-GUID clear ordering are
  connected to the inspected teardown. A second object pass precedes late
  A9/1F6/AA handler removal, which is not a pre-teardown invalidation witness.
- Direct-call inventory links object+0xE8 counter helpers to GameObject-related
  state logic. This does not establish a notification guard or exhaustive
  counter use; these helpers mutate client state.
- Two inspected graphics constructors resolve loading-progress virtual targets
  and window creation/getter paths. Startup assigns the HWND before scheduler
  creation; window procedures can forward to installed callback 0x42CFE0.
  Complete downstream/reentrant effects and live backend selection remain open.
- Setup requests a separate loading worker and passes the current context's TLS
  block. Successful creation lets another thread use that same block. One
  scheduler slot or matching context identity does not establish writer thread
  affinity. The inspected worker path does not prove world-location mutation.
- Profile location/generation remain absent, area unsupported, and XYZ has no
  qualified map association. Context IDs and callback return are not qualified
  world generations or freshness witnesses. Event 5 can initialize the world;
  no new sampling boundary follows from its label or ordering.

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

Shared nodes and inline saved storage explain why a local pending flag cannot
alone establish lifetime through bulk teardown. Shared TLS explains why context
equality cannot substitute for thread ownership. No concurrent teardown,
destructive callback reentry, torn read or gameplay fault was observed.

### UNKNOWN / SOURCE GAP

- Field-callback reachability to bulk cleanup/nested traversal, retention of
  objects/descriptors during callbacks, complete counter/teardown coverage,
  worker/completion callback effects, window dispatch/TLS interaction and
  loading/backend indirect targets.
- Actual runtime writer/thread ownership, complete writer coverage,
  initialization/invalidation, sample coherence, same-map/character/address
  ABA, and player-bound zone/area freshness.
- Completed unload, post-stop responsiveness, visible-screen/loading annotations
  and heartbeat continuity during unchanged dwell remain unestablished. Missing
  completion telemetry does not establish unload failure.
- Live Glue/UI eligibility remains unqualified; reconnect is unimplemented.
  No merge-target comparison or whole-branch integration verification occurred.

## Exact files changed

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — bounded descriptor/teardown/
  window research, reproduction ranges, proof limits and next investigation.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; bounded disassembly independently
  investigated without automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records: 111 C++
  executables, 49 audit Python tests, 13 QuestDB Python tests, 10 Lua fixtures,
  SQL/TSV fixture, full MinGW build and diff check.
  Report: `/tmp/wow-validation-ynkotj4a/results.json`.
- BUILD: separate `cmake --build build` PASS (DLL, testhost, loader, GUI).
- REVIEW: three read-only reviews found no actionable corrections. Full diff
  and worktree status inspected: exactly two task documents, no unrelated
  changes; `git diff --check` PASS.
- RUNTIME: NOT RUN; prior observer qualification is not extended.

## Runtime qualification and blockers

Raw observer: **RUNTIME PASS** within prior limits. Location candidates:
**UNQUALIFIED**. ProfileWorldEvidence location: **BLOCKED** by source gaps.
Manual unload acceptance remains open. Vendor/water qualifications remain
pending, reconnect remains unimplemented, and R0.1 is not closed.
The bounded research is complete; no sampler design is qualified.

## Recommended next task

Trace registered field callbacks into bulk cleanup or nested notification,
starting from farsight `0x5DE0D0`, and enumerate loading worker/completion paths
from `0x443300/0x443360/0x443E70`. Connect installed window callback `0x42CFE0`
to downstream dispatch, TLS and world-transition effects. Establish reachability
or exclusion while listener/object references are live; close writer coverage,
coherence and ABA before any profile location sampler design. Retain SOURCE GAP
if proof remains incomplete. Client mutation helpers are not an observe-only
acquisition mechanism.

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
