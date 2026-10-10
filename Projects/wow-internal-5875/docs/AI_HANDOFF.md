# AI Handoff — wow-internal-5875

## Repository state

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `090344528c3c6f46e0f51ea7a02b44d991f1b4b9`
— `docs: trace p07 visibility callbacks and FrameXML loading`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Clean at task start. The two task documents below are uncommitted at handoff
preparation. The commit containing this handoff is the authoritative checkpoint;
publication verification follows commit.

## Task and result

P0.7.4 source research continuation, 2026-10-10: tooltip listener removal, model
delivery and archive priority. Three parallel read-only agents traced those
paths; coordinator reconciled findings, checked key binary chains and four asset
hashes, and narrowed the button OnHide override. No production code, tests,
observer contract, hooks, controller ownership or navigation changed.

Conditional tooltip Hide reaches field-listener removal and clearing scripts
before the base visibility gate. Model callback event codes separate a suppressed
setter callback from update-driven completion. Archive insertion/lookup now has
a conditional priority rule. None establishes loaded scripts or outer retained
lifetime. **SOURCE GAP remains; profile location population stays BLOCKED.**

Detailed anchors, asset hashes, reproduction ranges and limits are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — tooltip listener removal, model delivery and archive priority.” Earlier
sections retain executable/archive provenance and saved runtime capture evidence.

## Evidence

### SOURCE VERIFIED

- Exact-client offline audit PASS: SHA256/PE layout, 43 instruction anchors,
  12 strings. All four qualification flags remain false. Additional manual paths
  and archive findings are outside that manifest; coverage is not exhaustive.
- Tooltip Hide clears before base visibility checks. Conditional GUID/context
  state reaches category-3/offset-0x40 listener removal, callback 0x529560;
  this differs from the outer farsight callback 0x5DE0D0. The existing active/
  pending removal protocol applies; bulk cleanup or nested notification is not
  established by this route.
- OnTooltipCleared can run before base Hide. Pinned assets hide a money frame;
  its OnHide conditionally hides CoinPickupFrame. Tooltip OnHide also hides
  shopping tooltips. Four extracted member lengths/hashes independently matched;
  actual loaded bytes, overrides and complete child/layout effects remain open.
- Button OnHide virtual+0x9C narrows to region/state changes and helpers; the
  separate OnLeave callback through +0x31C still has an unresolved target.
- Sequence setters can queue an operation; one inspected direct callback carries
  event 1, rejected by the installed OnAnimFinished adapter. An update/drain can
  deliver event 0 to that script. The inspected native AdvanceTime helper returns
  without advancing. OnUpdateModel has a separate registered callback path and
  reads widget/model fields after script return. Nested farsight execution is
  not established.
- Model references surround specific callbacks/queue entries, and archive open
  increments an archive count. These operations do not prove UI-widget user
  pointer or outer world-object/descriptor/listener retention.
- Archive insertion orders descending signed priority, newest before equals;
  lookup searches eligible archives in that order with per-archive selector
  matching/fallback and special stop conditions. Actual opens, selectors, views,
  earlier index/mode paths and loaded FrameXML remain UNKNOWN.
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

Tooltip hiding expands the concrete listener-mutation path, while event-code
checks narrow a specific model callback route. Resource-specific counting does
not discharge outer lifetime obligations. No runtime fault, unsafe overlap,
bulk cleanup or nested field notification was observed or proved here.

### UNKNOWN / SOURCE GAP

- Loaded scripts/overrides, actual widget/hover/visibility state; remaining money/
  coin/button/child/layout callbacks and nested event pumping.
- Model queued-operation replay, callback-driven queue mutation and widget user
  pointer retention; outer object/descriptor/listener lifetime; resource-owner/
  descendant completion, remaining object/GUI virtuals and TLS effects.
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

- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — tooltip/button effects, model
  callback delivery/retention, archive priority/selection, provenance and limits.
- `docs/AI_HANDOFF.md` — task state, evidence, validation and Git checkpoint.

## Validation

- SOURCE: exact-client offline audit PASS; four asset member hashes independently
  matched. No automated-manifest expansion or qualification upgrade.
- TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records;
  111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests,
  10 Lua fixtures, SQL/TSV fixture, full build and diff check.
  Report: `/tmp/wow-validation-ylzhgbvd/results.json`.
- BUILD: separate `cmake --build build` PASS for DLL, testhost, loader and GUI.
- REVIEW: three read-only reviews reconciled; archive-count wording narrowed
  to the verified increment, without claiming closed destruction semantics.
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

Prioritize the newly grounded tooltip listener-removal route: trace money/coin
Hide scripts and remaining tooltip child/layout virtuals for bulk cleanup or
nested field notification while outer references remain live. Resolve the
button+0x31C callback if reachable. Bound model queued-operation replay and
widget callback-context ownership before treating model counts as protection.
Use the conditional archive rule only when actual selection inputs are known;
retain loaded-script UNKNOWN otherwise. Close writer, invalidation, lifetime,
coherence and ABA obligations before sampler design; retain SOURCE GAP while
incomplete. Client mutation helpers are not observe-only acquisition.

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
