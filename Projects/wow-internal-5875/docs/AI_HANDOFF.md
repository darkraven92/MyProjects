# AI Handoff — wow-internal-5875

## Repository state

Branch: `codex/p07-world-zone-preparation` (observed in this worktree).
Starting checkpoint: `5cd22ed596a9adceb5483d1b746d31730b36804b`
— `docs: reconcile p07 location runtime evidence`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Source research started clean. This workflow/checkpoint follow-up started with
exactly the two intended research documentation changes, explicitly authorized
for inclusion by the user. The complete checkpoint contains the three files
listed below. The Git commit containing this handoff is the authoritative
checkpoint; see Git checkpoint status below. AGENTS.md now resolves the actual
worktree/branch; historical CODEX_PROJECT_STATE.md descriptions are not authority
for this checkout.

## Task and result

Roadmap phase: P0.7.4 source research into serialized sampling/lifetime and
preferred zone-updater GUID ownership, followed by permanent Git workflow and
checkpoint publication. Completed 2026-10-10, documentation only.

Updated AGENTS.md to automatically commit/push validated, internally consistent
substantial tasks on the coordinator's current `codex/` branch, with exact-file
staging, unrelated-change stops, explicit safety rules and remote SHA verification.
Reviewed both existing research documents for the user-authorized checkpoint.
No production source was changed. Git publication does not change any evidence
classification or imply merge readiness.

Traced event-slot-5 scheduling, manager publication/destruction and temporary
packet-handler ownership. Identified the alternate GUID as the exact client's
PLAYER_FARSIGHT field and reviewed five direct preferred-GUID setter calls.
Three parallel read-only agents reviewed lifetime, alternate ownership and the
repository boundary; coordinator reconciled and independently checked findings.
No defensible sampling/lifetime/coherence contract was established. **SOURCE GAP
remains; profile location population stays BLOCKED.** No production source,
tests, reader, hook, gameplay ownership or navigation changed.

Detailed bounded addresses, limits and reproduction commands are in
[P07_WORLD_LEVELING_PROFILE_AUDIT.md](P07_WORLD_LEVELING_PROFILE_AUDIT.md),
“P0.7.4 — scheduling, lifetime and alternate GUID continuation.”

## Evidence

### SOURCE VERIFIED

- Workflow rules now cover this project's worktrees, actual branch detection,
  validation gates, exact task staging, push failure handling and the
  containing-commit convention. Superseded no-automatic-commit rules removed.
- Exact executable SHA256/PE layout, 43 instruction anchors and 12 strings
  pass the existing offline audit. All four location/lifetime qualification
  flags remain false. Additional manual disassembly is outside that manifest;
  neither direct-call search nor these paths are an exhaustive writer audit.
- Root and connection+0x1AD8 publish before map setup. Object cleanup precedes
  root/connection/saved-owner clearing. Packet dispatch is bracketed by temporary
  manager switch/restore; a single saved pointer supplies no generation.
- Zone updater is registered in event slot 5 and removed before teardown.
  Registration uses a TLS context. Event execution releases its inspected
  context lock before callbacks. Complete writer thread/reentrancy coverage
  remains absent; neither this lock nor callback registration qualifies reads.
- The alternate pair is PLAYER_FARSIGHT at descriptor+0xB20/+0xB24, reached
  through object+0xE68 (descriptor+0x2F0) and relative offset +0x830/+0x834.
  One setter path uses the resolved alternate object's GUID. Separate owner
  and preferred GUID publication and conditional clearing do not qualify
  current-player cache ownership/freshness.
- Observer runs on the bootstrap worker with sequential before/world/after
  reads. Raw connection/location reads use ReadProcessMemory; WorldStateReader
  still uses readability-check-then-memcpy. GameThreadDispatcher targets the
  window-owner thread and has no established world phase/lifetime contract.
- Profile map/zone, position-map association and world generation remain absent;
  area unsupported. No profile sampler was introduced.

### RUNTIME OBSERVED

**P0.7.4 RAW LOCATION LIFECYCLE OBSERVER — RUNTIME PASS**

Prior qualification preserved, limited to the saved retry capture reconciled
at starting HEAD. No capture reinspection or new WoW run/attach/input occurred
in this task. Reference: ignored local directory
`runtime-captures/p074-location-lifecycle-retry-2026-10-10/`, PID `296`, session
`296.134360886272945680.57555570.500`; detailed audit retains hashes/five rows.

That capture showed in-world candidates `1/14/363`, manager loss with retained
zone/area, later `0/0`, a new manager with zero GUID, and return with the same
GUID/new player pointer. All five signature samples passed while labels stayed
unqualified/sequential/no-owner/no-command. GUI Stop Bot, observer STOP, logical
RUNTIME DETACHED and unload_requested were recorded. These are stop-path
telemetry, not completed module unload. Earlier PID 300 records are separate.

### INFERRED

The named alternate field and owner comparisons are consistent with viewpoint
selection; gameplay activation semantics were not established. Prior stale-cache
and early-map observations agree with source ordering but do not identify exact
execution points, coherence or a world generation.

### UNKNOWN / SOURCE GAP

- SOURCE GAP: complete writer/indirect-callback closure, writer thread affinity,
  reentrancy, eligible sampling phase, initialization/invalidation across loading,
  teardown and transfers, same-map/character/address ABA, player-bound zone/area
  freshness and coherent publication. Profile location population remains BLOCKED.
- Object+0xC58 bit 0x400 gameplay meaning and full PLAYER_FARSIGHT/flag writer
  coverage remain UNKNOWN; neither is a world-ready predicate.
- SOURCE GAP: completed-unload telemetry is absent. Actual module absence,
  post-stop client responsiveness, visible-screen/loading annotations and
  heartbeat continuity during unchanged dwell remain unestablished. Missing
  completion telemetry does not establish an unload failure.
- Live Glue/UI eligibility and reconnect remain unqualified; reconnect is not
  implemented. No merge-target comparison or whole-branch verification occurred.

## Exact files changed

- `AGENTS.md` — permanent automatic checkpoint/push workflow, worktree scope,
  safety rules and handoff checkpoint reporting.
- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md` — bounded scheduling/lifetime and
  alternate-GUID source continuation, remaining proof obligations and commands.
- `docs/AI_HANDOFF.md` — final research/workflow state, evidence, validation,
  checkpoint convention and next task.

## Validation

- SOURCE: exact-client offline audit PASS; new bounded instruction/data paths
  independently reviewed. Automated manifest unchanged; no qualification upgrade.
- RESEARCH TESTS: `python3 tools/validate.py --jobs 4` PASS, all 247 records: 111 C++
  executables, 49 audit Python tests, 13 QuestDB Python tests, 10 Lua fixtures,
  SQL/TSV fixture, full MinGW build and diff check.
  Report: `/tmp/wow-validation-wey3vd5_/results.json`.
- RESEARCH BUILD: separate `cmake --build build` PASS (DLL, testhost, loader, GUI).
- RESEARCH REVIEW: three independent read-only reviews found no factual corrections;
  expanded one reproduction range to include field-table base setup. Complete
  diff inspected; `git diff --check -- .` PASS. Only the two research documents
  changed at that point; production source/tests unchanged.
- WORKFLOW CHECKPOINT: repeated `python3 tools/validate.py --jobs 4` PASS,
  all 247 records, 111 C++ executables, no failures. Report:
  `/tmp/wow-validation-se_mb4uw/results.json`. Separate `cmake --build build`
  PASS (DLL, testhost, loader, GUI); exact-client offline audit PASS with all
  location/lifetime qualification flags still false. Full diff and every
  changed/untracked file reviewed: exactly the three authorized files, no
  production changes, artifacts or secrets. `git diff --check` PASS.
  Independent read-only workflow/handoff review found no corrections.
- RUNTIME: NOT RUN in this task. Source/test/build results do not extend the
  preserved raw-observer runtime qualification.

## Runtime qualification and blockers

- Raw lifecycle observer: **RUNTIME PASS**, limited as above.
- mapCandidate / zoneCandidate / areaCandidate: **UNQUALIFIED**.
- ProfileWorldEvidence location population: **BLOCKED** by source gaps.
- Full manual-runbook unload acceptance still needs completed module absence
  and subsequent client responsiveness evidence.
- Vendor episode and water emergency egress remain runtime pending; automatic
  reconnect remains unimplemented and R0.1 is not closed.

This bounded research task completed without a qualified sampler design.
No candidate was promoted and no runtime gameplay fix is claimed.

## Recommended next task

Continue source research from event execution `0x420F70/0x4245B0`: close thread
and reentrancy coverage against queued transfer `0x4200A0`, direct packet
handlers and temporary-owner restoration. Trace PLAYER_FARSIGHT and object
+0xC58 bit 0x400 writers/reset ordering, preferred GUID clearing and zone/area
publication. Establish a defensible initialization/invalidation, coherence and
ABA contract before designing or implementing any profile location sampler.
Do not treat window-thread dispatch, event registration or a released context
lock as that contract. Retain SOURCE GAP if proof remains unavailable.

Separately close the existing unload runbook with manual module-absence and
post-stop responsiveness evidence. Matching values or another ordinary
logout/login cannot qualify profile location. Future qualified consumption
also needs loading, teardown, same-map reload, map-transfer and zone/area-only
runtime cases. Preserve Detour/NavMesh and controller ownership.

## Integration / merge status

Manual runtime evidence required: No new run for this source-research task;
yes for outstanding unload acceptance, future qualified location consumption
and pending vendor/water qualifications.
Safe to commit: Yes, for exactly the three listed files; current validation,
build, diff checks and review passed. Staged checks are required before commit.
Safe to merge: Documentation checkpoint only; no automatic merge authorized.
Whole-branch integration remains UNKNOWN; no target comparison/merge
verification performed, and qualified profile location remains blocked.

## Git checkpoint status

The user authorized committing exactly AGENTS.md, docs/AI_HANDOFF.md and
docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md, then pushing the current branch
`codex/p07-world-zone-preparation` to origin. No unrelated changed/untracked
files were found. Before checkpoint creation, live origin and its local tracking
ref both matched starting checkpoint `5cd22ed596a9adceb5483d1b746d31730b36804b`.

The Git commit containing this handoff is the authoritative checkpoint; no
follow-up commit is needed merely to embed its own SHA. This handoff records
the final task contents before commit. Commit/push outcomes, equality of local
HEAD, origin tracking ref and live remote SHA, and final worktree status are
verified afterward and reported in the final task response. Do not infer a
verified push solely from this pre-commit document. Future coordinators can
recheck with `git rev-parse HEAD`, `git rev-parse
origin/codex/p07-world-zone-preparation`, `git ls-remote --heads origin
refs/heads/codex/p07-world-zone-preparation`, and `git status --short`.

## Persistent project statuses

VENDOR EPISODE — RUNTIME PENDING
WATER EMERGENCY EGRESS — RUNTIME PENDING
SOURCE GAP — RECONNECT NOT IMPLEMENTED
