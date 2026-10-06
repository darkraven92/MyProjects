# Project

Updated: 2026-10-06. Read this file AND `AGENTS.md` before continuing.

- Repository scope: `/home/ludvig/Programming/Projects/wow-internal-5875`.
- Git root is `/home/ludvig/Programming`; sibling projects are OUT OF SCOPE.
- Branch: `codex/wow-internal-continuation`.
- Client: WoW Vanilla 1.12.1 build 5875, 32-bit DLL under Wine/X11.
- Environment: Linux/CachyOS; C++20, MinGW/CMake; user commands in fish.
- Active request: Master Roadmap V6. This explicitly authorizes reviewed,
  validated checkpoint commits/pushes (superseding earlier no-commit requests).
- Current priority override: V6 **P0.0 AFK FIRST**. The shared AFK source/test
  checkpoint below is NOT runtime qualification or completion of the roadmap.

# Current architecture

- One `SharedAfkController` in WorldMonitor observes client/server AFK and the
  native input clock for both workloads. Safe-idle-only candidate input is
  paired, bounded and verified; the older Grinding AFK-named liveness mechanism
  remains distinct. See `docs/AFK_5875_AUDIT.md` for source proof and live gates.

- `WorldMonitor` dispatches existing Questing and Grinding controllers using
  live `WorldState`; shared `CombatController`, `GenericNavMeshPathFollower`,
  `DetourNavigationProvider`, `DeathRecoveryController`, vendor and loot
  mechanisms already exist. Workload orchestration is not fully consolidated.
- Quest data: local VMaNGOS SQL -> normalized/enriched SQLite -> regional TSV ->
  `VanillaQuestDatabase` -> authored/generated merge -> `QuestGraph` ->
  `QuestAcquisitionPolicy`/planner -> generic executors.
- Authored execution mechanics remain authoritative; source restrictions enrich
  acquisition metadata. Unknown prerequisites stay unknown.
- Generic discovery requires exact live offer identity, acceptance followed by
  live-log appearance, then same-giver reread. Turn-in requires reward action
  AND live-log removal before updating confirmed completion history.
- `QuestPlannerRuntimeController` currently owns equipment/trainer controllers;
  the same integration is not present in `GrindModeController`. Shared services
  beneath workload scheduling remain a V6 consolidation task.
- Navigation uses validated Detour routes, movement intents, surface-recovery
  episodes, local directed-transition evidence and `NavigationHazardMemory`.
  Hazard persistence already exists; audit it before adding another store.
- The provider has ground/water flag route fallback. This is NOT a swimming,
  breath-monitoring or drowning-recovery implementation.
- Warrior rotation exists. No explicit ClassProfile/TalentBuild/LevelingProfile
  implementation was found in the initial filename/source trace. Do not
  describe requested Hunter, talent library or GUI dashboard as implemented.

# Protected invariants

- One movement owner; terminal owners release; command issued is not success.
- Unknown evidence is neither true nor false. Live spellbook/talents must be
  authority for learning/spending, not static level assumptions.
- Preserve 14O.1, 16I, 16J, 16J.1 and focus 0/multiquest vs positive/exclusive.
- MaximumPathLength=2000; MaximumSurfaceRecoveryAttempts=4;
  MaximumLastSafeBacktracks=2. No budget/timeout inflation or random movement.
- Physical displacement alone must not reset a surface-recovery episode.
- Preserve exact offer, acceptance, reward/removal, MerchantFrame,
  TrainerFrame/spellbook and post-equip verification.
- No quest/NPC/item/zone coordinates in generic behavioral branches.
- Questing and Grinding are workloads on shared mechanisms, not separate bots.
- Water safety must be a shared survival owner; never equate water mesh flags
  with proof of a safe swimming route or sufficient breath.

# Runtime verified

Provenance matters: the following milestones were explicitly reported verified
by the user in the V6 request. The current log no longer contains those runs;
do not present them as reverified by this checkpoint.

- Vile Familiars: class-compatible duplicate-title identity, turn-in, reward and
  live-log removal.
- Sarkoth 790/804: graph eligibility filters title candidates; 804 remains
  PrerequisiteUnknown; 790 live offer and acceptance confirmed.
- Existing 16J safe pulling and 16J.1 failure 1->2->3/defer/skip and safe revisit.
- Generic starter acquisition/turn-in/chain progression previously verified.

Earlier inspected log: `build/wow-internal.log`, 2026-10-06 17:02-17:06 UTC,
session `1404.134357533165383490.42335680.1980`. Level 21 Grinding run, NOT a
fresh Orc/cave qualification. Intent 1 arrived at tick 75, intent 2 arrived at
269; later roam intents were released during acquisition/watchdog activity.
Intent 8 failed `no_path` at tick 820. Acquisition watchdog reset at tick 856;
world became unavailable and GUI reported process absent at 17:06:43 UTC.
Cause of process exit is unknown. No surface-recovery episode appears in this
log. No WoW process was running at audit time.

Newest inspected live capture (2026-10-06 19:39 local, 1,449,498 bytes)
supersedes that earlier cave/navigation sample for P0.1. Intent 11 retained one
owner while crossing the region near (-587,-4208,39) and later reached combat
handoff near (-457,-4216,50). `NAV RECOVERY RESET earnedProgress=no`
followed target arrival without verified forward corridor progress; ordinary
portal crossing plus destination gain logged `earnedProgress=yes`. Bounded
recovery/backtrack and terminal release worked. The new code below has NOT
yet run in WoW; these observations are pre-fix evidence only.

# P0.1 local steering and directed failure attribution (2026-10-06)

SOURCE VERIFIED root cause: after every steering candidate failed,
`IssueCurrentCorner` cleared the prior ordinary CTM evidence, then called
`IssueSurfaceRecovery`, which intentionally passed `ordinarySteeringStalled=false`
to `RegisterFailedCorridor`. The failed record therefore had refs 0/0. A later
`PlanFrom` computed a nonzero **candidate** pair from a newly planned corridor;
that pair was never evidence for the earlier failed ray and must not be copied
backward. Straight-path `fromIndex`/`candidateIndex` are not Detour corridor
indices; `pointPolyRefs_` identifies the polygon entered at a straight-path
point. Unknown ray/portal relationships remain unknown.

Concrete live sequence: intent 11 near (-587.299,-4208.641,39.114) accepted
the nearer wall-inset point with a full ray, then rejected a farther point
with ray fraction 0.304. Recovery episode 4 began with problem refs 0/0.
The following replan logged candidate 0x10000880002BCB ->
0x10000880002BCA but failed refs 0/0 and allowed `transition_unknown`.
Do not infer identical transitions merely from proximity. The runtime also
showed non-steep complete versus validated steep fallback from changed local
positions; offline geometry does not prove unstable projection or a 14O.1 bug.

Offline inspector at the exact failed-ray position found the ray stopped in
the live-start polygon near a clipped 8.707-yard portal, about 0.675 yard
from its edge. A direct ray to the NavMesh-derived portal midpoint was full,
with 3.0-yard measured clearance, and a ray onward from that midpoint reached
the previously failed target. Other exact positions near (-577,-4210),
(-570,-4215) and (-560,-4285) projected to different local polygons; nearby
routes are traversable. Offline Detour refs have different salt bits from live
tile loads, so match geometry/suffixes, not full 64-bit values across sessions.
The inspector does not replay live collision/hazards and grants no runtime pass.

Minimal generic P0.1 change: provider exposes the exact directed clipped
portal and optional ray hit/visited refs; follower attributes a failed pair
only when the complete blocked ray's last polygon is uniquely on the forward
corridor and its hit lies within the existing wall-clearance threshold of that
directed portal. It logs one `STEERING FAILURE ATTRIBUTION` per total steering
failure. Otherwise refs remain 0/0. A known local pair can drive one
geometry-derived portal-midpoint stage after projection, connected-poly,
vertical, ray, clearance, distance and duplicate-target checks. Staging uses
the existing four-attempt surface-recovery episode; no budget or 14O.1 change.
These steering observations are not automatically promoted to persistent
hazard exclusions. Later V6 hazard memory must require its own evidence.

Test-first evidence: the new strict local-portal regression failed to compile
before its policy existed, then passed. The initial full run found an existing
source-wiring assertion tied to the old call signature; it was updated without
changing recovery policy. Final `python3 tools/validate.py --jobs 4` result:
TEST PASS 70/70 C++ tests (C++20, Wall/extra/Werror), QuestDB 13/13 plus SQL
fixture, Lua 5 fixtures/89 checks; BUILD PASS; DIFF CHECK PASS. Artifact:
`/tmp/wow-validation-3hgjt6he/results.json` (temporary).

P0.1 RUNTIME PENDING: two natural traversals of the same problematic corridor
with zero human movement intervention are required. Inspect new
`STEERING FAILURE ATTRIBUTION`, `NAV ENTRANCE STAGE`, 14N.2, recovery and
movement-intent logs. A stage command alone is not proof of portal crossing.
Current validated worktree is still dirty; a source/test/build pass does not
establish a clean-checkout or WoW runtime pass.

# P0.1.1 hard-stall provenance and path diagnostics (2026-10-06)

New live capture: `build/wow-internal.log` at 20:26 local, 288,738 bytes.
Vendor intent 2 toward merchant 3882 issued ordinary CTM target
(-564.639,-4235.094,42.504), then logged HARD STALL with failed refs 0/0.
After lookahead/replan it issued (-566.400,-4233.333,41.914) twice, stalled,
and allowed a new route on `transition_unknown` although the *new* candidate
pair was 0x...2E6C -> 0x...2E6B. It failed boundedly at tick 200; the vendor
owner released. This is P0.1 RUNTIME FAIL, not a traversal pass.

SOURCE VERIFIED attribution cause: `ResolveIssuedLocalTransition` reprojected
the issued steering target with generic nearest-poly selection. Exact offline
inspection from the stalled live region projects the player to corridor poly
suffix 2E6C, but the portal target to overlapping off-corridor poly 2E78.
Detour's straight-path point and connected clipped portal identify the
command's intended local edge 2E6C -> 2E6B; the target reprojection loses
that evidence. Full poly salt bits differ by tile load, so do not compare
offline full refs to live refs. This is command intent for hysteresis, NOT
proof the physical portal is globally unsafe and NOT persistent hazard proof.

`IssuedSteeringCommandPolicy` now retains a compact source, sequence, global
CTM dispatch serial, intent, route generation, fingerprint, target, issue
position and exact route edge when the edge is independently proven at
command creation. An adjusted target that projects away from the directed
edge remains unknown. All follower CTM sources replace this provenance;
stop, route refresh, combat pause and terminal states invalidate it. HARD
STALL attributes only if the *same* global dispatched command still matches.
Unknown/replaced/stale commands stay unknown. The previous no-clearance ray
attribution and episode semantics are unchanged.

Separate live path failure: objective intent 3 failed before any CTM with
`path_validation_failed`, 71 polygons, `validationMs=0`. The real rejection
occurred inside the 14O.1 terrain query, before `ValidatePath`: unsafe
transitions 0x...207C -> 0x...2081 (vertical 6.125, rise/run 1.209), then
0x...2080 -> 0x...2085 (vertical 9.500, rise/run 2.096). The existing bounded
alternate search found no safe alternative in route, expanded or full-map
scope. Typed `PATH VALIDATION FAILED`/plan-profile subreason telemetry now
exposes `unsafe_terrain_no_alternative`, directed refs, flags, geometry and
attempt. This changes diagnostics only; it does not grant a rejected path.

The new code is **RUNTIME PENDING**. Qualify with two natural successful
traversals of the same problematic corridor, no manual movement and no
anonymous retry when the issued command's pair is genuinely known. If it
remains unknown, use new command-provenance reason and inspect that command;
never copy the next route's candidate pair backward. Do not start P0.2.

Final `python3 tools/validate.py --jobs 4`: TEST PASS 72/72 C++ tests under
`-std=c++20 -Wall -Wextra -Werror`; QuestDB Python 13/13 plus SQL fixture;
five Lua fixtures/89 checks; BUILD PASS; DIFF CHECK PASS. Strict focused
tests failed to compile before the new policies existed and passed afterward.
Detailed temporary artifact: `/tmp/wow-validation-tsfhxfdy/results.json`.

# P0.1 objective-anchor failover (2026-10-06)

The later live objective intent began at (-560.332,-4217.55,41.5904) and
selected the first source-backed objective seed at
(-168.068,-4403.07,77.5404). Route, expanded and full-map initialization
all rejected the same 9.5-yard vertical directed transition with
`unsafe_terrain_no_alternative`; zero CTM commands were issued. This is a
correct 14O.1 safety rejection, not evidence that the entire objective or
region is unreachable. The intent was retained between initialization tiers
and released once after terminal failure, so the prior ownership fix has
live confirmation for this path.

SOURCE VERIFIED generic gap: the regional catalogue supplies many distinct
source-backed objective spawn anchors, but `CollectItemFromMobExecutor` used
only the primary destination and failed the whole objective when its route
failed. The executor now considers a distinct, same-map search anchor only
after a terminal route-specific failure. It retains an authored destination
override, excludes duplicate/invalid anchors, and caps the total number of
distinct anchors at eight. Each alternate gets a fresh follower and the
unchanged Detour/14O.1 validation; no individual route or recovery budget was
raised. Unknown/initialization failures do not authorize a spawn switch.
Terminal follower ownership is released before the alternate starts.

Focused policy regression first failed compilation before the new selection
API, then passed. Full validation: TEST PASS 72/72 C++ tests under strict
C++20/Wall/extra/Werror, QuestDB 13/13, five Lua fixtures/89 checks;
BUILD PASS; DIFF CHECK PASS. Temporary artifact:
`/tmp/wow-validation-fp3_be0s/results.json`. RUNTIME PENDING: WoW must show
a rejected first seed, a different source-backed anchor selected, a validated
route, and an objective/combat handoff or bounded exhaustion. This is not a
claim that the cave/vendor corridor itself is fixed.

# Source/test verified

Latest P0.0 AFK checkpoint, 2026-10-06:

- SOURCE VERIFIED: local 5875 idle-check instruction uses 300000 ms, input
  timestamp at 0xCF0BC8 and local AFK flag at 0xB6E5CC. Server PLAYER_FLAGS
  bit 2 is independently observed. This is NOT a runtime-measured threshold.
- SOURCE VERIFIED gap: prior Grinding liveness counted displacement without
  input-clock proof; Questing only emitted blocked diagnostics. New shared
  policy never equates CTM/combat/displacement with qualifying activity.
- New files: AfkProtectionPolicy, AfkPreventionWindow, AfkInputPulse,
  AfkSafeInputScript, AfkClient5875, SharedAfkController; strict policy test,
  Lua guard fixture, read-only client audit tool, AFK audit/runbook.
  Scoped WorldMonitor/QuestPlanner safe-idle integration; validation fixture map.
- TEST PASS: 73 C++ executables, C++20 `-Wall -Wextra -Werror`;
  13 QuestDB Python tests + SQL/catalogue fixture; 6 Lua fixtures / 113 checks.
- BUILD PASS: full validation and explicit `cmake --build build`.
- DIFF CHECK PASS: `git diff --check -- .`.
- Validation artifacts: `/tmp/wow-validation-bf4fjpez/results.json`
  (final repeated full validation; earlier `/tmp/wow-validation-0or8jre6`).
- RUNTIME PENDING: targeted unbound-F12 message candidate, observed threshold,
  local/server clear and two continuous prevention intervals. No running WoW
  process available; newest log ends with process absent at 19:05:58 UTC.

Earlier P0.1 dirty-worktree validation using `python3 tools/validate.py`:

- TEST PASS: 69/69 C++ test executables, each compiled with
  `-std=c++20 -Wall -Wextra -Werror`.
- TEST PASS: 13/13 QuestDB Python tests and SQL/catalogue fixture.
- TEST PASS: all 5 Lua fixtures, 89 checks (trainer 23, equipment 40,
  durability 8, quest dialog 9, maintenance sale 9).
- BUILD PASS: `cmake --build build`.
- DIFF CHECK PASS: `git diff --check -- .`.
- Offline inspector strict build PASS: `bash tools/build_navmesh_debug.sh`.
- Detailed validation artifact from this run:
  `/tmp/wow-validation-9mu_8ta_/results.json` (temporary, not durable evidence).

Previous `f5d04e5` checkpoint changes were diagnostic/tooling only:

- `tools/validate.py`: reproducible full test/build/diff runner; bounded test
  subprocesses, no gameplay, no git mutations. A new Lua fixture without a
  registered producer fails validation rather than silently being skipped.
- `tools/navmesh_debug.cpp`: corrected clipped external-link portal endpoints;
  enumerates projection candidates using route/local extents, records flags,
  area, Z, corridor membership, portal width/clearance, local ray final surface
  height. Output explicitly labels missing live/hazard/inset replay.
- `tools/NavmeshAuditGeometry.h` + test: finite/clipped portal geometry.
- The pre-existing, unchanged `TerrainTransitionPolicy.h` is included in this
  checkpoint solely because the offline inspector depends on it. No runtime
  controller file is changed by this checkpoint.

# Runtime pending

- **AFK first:** launch a logged-in safe client with
  `WOW_INTERNAL_AFK_MODE=qualify` in the WoW process environment. Detailed fish
  commands and evidence sequence are in `docs/AFK_5875_AUDIT.md`. Need natural
  baseline, individually verified input, then two clear prevention intervals
  and both workload integrations. No automatic RUNTIME PASS from log counters.

- 2026-10-06 post-P0.1.1 natural log: vendor intent 1 proved issued hard-stall
  edge 0x...2E6C -> 0x...2E6B and 14N.2 suppressed its immediate retry.
  A distinct 0x...2E6C -> 0x...2E76 route then stalled and the bounded
  vendor intent failed. This is not a traversal pass. Objective intent 2
  rejected an unsafe 9.5-yard vertical transition; route/expanded tiers both
  failed validation, and the captured full-map tier had not finished.
- SOURCE VERIFIED ownership defect in that same log: initial route-scope
  `PlanFrom` entered terminal `Failed` and logged `MOVEMENT INTENT RELEASE`
  before `AdvanceInitialization` continued to expanded/full-map planning.
  The scoped fix retains the intent across internal initialization fallbacks
  and releases only on genuine terminal failure/arrival. Hard-stall budget
  exhaustion now reports `hard_stall_exhausted` rather than `other_unknown`.
  These changes are TEST PASS / BUILD PASS; WoW runtime is PENDING.
- P0.1 no-clearance attribution and recovery semantics have new live proof,
  but P0.1 overall failed on hard-stall retries. P0.1.1 code is runtime pending.
- Cave/interior navigation: generic portal staging is SOURCE VERIFIED and TEST
  PASS but RUNTIME PENDING. Do not advance to P0.2 swimming until two natural
  P0.1 traversals pass or an external runtime blocker is documented.
- Combat attack continuity, acquisition liveness and AFK prevention need live
  diagnosis. Do not infer combat success from arrival events.
- Vendor/repair, equipment and trainer end-to-end qualification remains distinct
  from existing frame/spellbook/equip verification mechanisms and unit tests.
- Water-state/breath/surfacing/shore systems are not implemented by this work.
- Class profiles, automatic capability discovery, Hunter/racials, talent builds,
  talent library, leveling profiles and operational GUI remain queued.
- All unattended 5/15/30/60 minute and fresh Orc->10 gates remain RUNTIME PENDING.

# Known blockers

1. Post-P0.1.1 vendor capture proves exact hard-stall attribution and same-edge
   suppression, but a different local route also stalls and the owner fails
   boundedly. Offline inspection at the live hard-stall position projected
   player Z=38.134 to mesh Z=39.362; the next straight portal was Z=41.914.
   The local Detour ray was horizontally complete but does not verify height.
   This is an investigation lead, not proof that lowering any safety threshold
   or issuing a different CTM would traverse the portal. P0.1 needs two
   unassisted successful traversals.
2. The objective route correctly rejects unsafe terrain. Generic bounded
   alternate-anchor selection is now SOURCE VERIFIED / TEST PASS but still
   RUNTIME PENDING; it may find another valid spawn, or correctly exhaust.
3. Detour's local `raycast` is explicitly 2D (local VMaNGOS
   `DetourNavMeshQuery.cpp`, raycast contract). Runtime
   `IsSurfaceSegmentReachable` interpolates requested Z and does not verify end
   surface height. This is a source-backed contract risk, NOT proof that it
   caused the user's cave failure.
4. Offline sample at the reported recovery target (-169.688,-4333.750,67.899)
   to the reported objective (-58.1846,-4220.64,62.3418) gives 83 non-steep
   polygons, 304.325-yard straight-path length, matching start projection
   across route/local scopes and no 14O.1 rejection. First three ray endpoint
   heights match; later direct rays hit walls. This does NOT establish wrong
   layer selection. One portal midpoint clearance is below existing 0.70;
   that alone does not prove the full portal untraversable.
5. Offline tool does not replay runtime persistent hazards, loaded-tile history,
   wall-inset steering or collision. Its report cannot grant runtime PASS.
6. Current Grinding run has repeated acquisition/watchdog resets; exact control
   flow/root cause awaits P0 combat/ownership audit.
7. AFK: WoW is closed. The new non-movement input candidate must advance the
   native input clock in Wine. If ignored, it faults once instead of pretending
   protection. Actual threshold and two windows require user login/start.
   GUI status transport and a broader water-state model remain unimplemented.
8. Much prior working project code/data/tests is still dirty/untracked. This
   checkpoint publishes only reviewed audit/tooling files, not the complete
   current runtime. A clean remote checkout is NOT this validated worktree.

# Current phase

**P0.0 AFK FIRST (user priority override):** shared source/test implementation
is SOURCE VERIFIED / TEST PASS / BUILD PASS / DIFF CHECK PASS.
RUNTIME PENDING because no live WoW process is available. Do not advance to
swimming before this external gate is exercised or explicitly retained as a
blocker. The runbook supplies an opt-in bounded controlled-idle test; ordinary
default protection cannot interfere with active gameplay owners. Candidate
input is not yet proven sufficient under Wine. No GUI/profile/talent expansion.

Preserved V6 P0.1 follow-up: internal initialization fallback retains movement
ownership, hard-stall terminal cause is typed, and objective execution can
try bounded alternate source-backed anchors after a terminal route rejection.
SOURCE VERIFIED / TEST PASS / BUILD PASS / DIFF CHECK PASS; RUNTIME PENDING
for the new anchor path. The physical vendor corridor still failed in the
newest natural log, so P0.1 remains RUNTIME FAIL until two later unassisted
traversals pass. No later V6 subsystem behavior patched.

Proceed in requested order, with source/test/build/diff and checkpoint after
each bounded subphase. Do not stack speculative fixes on the unproven cave
failure. Keep user/runtime claims, local source evidence and new runtime proof
separate.

# Runtime validation gates

1. Preserve one continuous post-fix session log before another GUI log clear.
2. Repeat the SAME natural corridor twice: intent -> exact steering attribution
   (known only if ray/portal proves it) -> safe stage/alternate route -> verified
   crossing and combat handoff or bounded failure/release. Zero manual movement.
3. Inspect `NAV COMMAND PROVENANCE` and `HARD STALL ATTRIBUTION` for the CTM
   that actually stalled. No repeated known edge, anonymous pair where issue
   evidence is sufficient, unearned reset, same-target loop or safety weakening.
4. Compare actual player XYZ, route generation, projection and clipped portal
   geometry if post-fix behavior differs; offline reports are not live proof.
5. For the objective route, require `OBJECTIVE ANCHOR FAILOVER` to select a
   distinct source-backed spawn only after terminal route-specific failure;
   each alternate must pass the ordinary path validator before movement.
   Exhaustion must release ownership and reach existing defer logic.
6. Priority override: AFK baseline/input/two windows first; then P0.1 traversal
   qualification, water, hazards, combat, invariants, consolidation and
   cross-workload validation. Do not claim implemented water safety early.
7. AFK requires two natural windows; drowning tests must not deliberately risk
   death; long runs only after shorter safety gates.

# Important commands

Run from project directory in fish:

```fish
git status --short -- .
python3 tools/validate.py
bash tools/build_navmesh_debug.sh
cmake --build build
git diff --check -- .

set -gx WOW_INTERNAL_QUEST_FOCUS_ID 0
# Start through the existing GUI workflow. No unsafe synthetic input.
tail -F build/wow-internal.log | rg --line-buffered 'LOGGER SESSION|BOT SESSION|MOVEMENT INTENT|NAV COMMAND PROVENANCE|HARD STALL ATTRIBUTION|PATH VALIDATION FAILED|STEERING FAILURE ATTRIBUTION|NAV ENTRANCE STAGE|NAV RECOVERY|NAV 14N.1 STEERING|NAV 14N.2 HYSTERESIS|LOCAL RECOVERY|NAV 14O.1|COMBAT STALL|AFK'

# At the end of the natural run, preserve its full log locally (not in Git).
set capture (mktemp -d /tmp/wow-v6-runtime.XXXXXX)
cp build/wow-internal.log "$capture/wow-internal.log"

# Replace every placeholder with values from the SAME live episode.
# build/navmesh_debug --map MAP --x X --y Y --z Z \
#   --to-x DX --to-y DY --to-z DZ --portal-x PX --portal-y PY --portal-z PZ \
#   --mmaps /home/ludvig/Games/WoW-NavData/mmaps --out build/cave-live-audit
```

The inspector reads local VMaNGOS Detour sources; `VMANGOS_DETOUR_ROOT` can
override their location. It writes reports only, never controls WoW.

# Git state

- Validated P0.0 AFK code checkpoint:
  `afde8e9a4157f2114ec8fa916b3fe21dc36a82c3`
  (`afk: add shared input verification and bounded qualification`). Pushed to
  origin; independent `git ls-remote` returned the same full hash before this
  documentation follow-up. Runtime remains pending; WoW login/start is needed.
- P0.0 AFK checkpoint is scoped to the new AFK files, validation registration,
  documentation and isolated WorldMonitor/QuestPlanner integration hunks.
  Existing dirty gameplay work remains unstaged; full-worktree validation does
  not mean the complete runtime is published. Resolve this checkpoint with
  `git log -1 --format='%H %s' -- src/Bot/SharedAfkController.h` and verify
  remote HEAD using the commands below. Base HEAD was `20d450b`.

- Current local HEAD during the objective-anchor fix:
  `20d450b5ee8e8da1a698532fc02efd0cee3e34f0`. The anchor fix is only in
  the dirty worktree; no commit/push was made for it because the touched
  executor, policy and test contain pre-existing uncommitted work that cannot
  be safely published as an isolated checkpoint without staging unrelated
  changes. Reconcile that work before any scoped publication.
- P0.1 initialization-ownership checkpoint: `a06374731903bfb025a855814762d5b4c26a0703`
  (`navigation: retain intent across initialization fallback`). Full
  source/test/build/diff validation passed in the dirty worktree; the commit
  was pushed, and independent `git ls-remote` returned the same remote HEAD
  before this documentation follow-up. Runtime remains pending.
- Inspected base HEAD for this P0.1 phase: `f5d04e5` (offline cave audit).
- Inspected base HEAD for P0.1.1: `db87c05` (continuity follow-up).
- P0.1.1 validated code checkpoint: `23421df6a67fd9bab1ce5929dc7e8d7fbc05e1cf`
  (`navigation: retain issued edge provenance for hard stalls`). It was pushed
  to `origin/codex/wow-internal-continuation`, and independent `git ls-remote`
  returned the same full hash before this documentation-only follow-up.
- P0.1 validated navigation checkpoint: `555cccdbaa99f9e9c5fd737cc0c7bc9b55f21a35`
  (`navigation: attribute local steering failures to verified portals`).
  Pushed to `origin/codex/wow-internal-continuation`; `git ls-remote`
  independently reported the same full hash before this continuity-only
  follow-up. Resolve the *latest* local/remote HEAD with the commands below;
  do not infer them from this historical checkpoint field.
- Branch: `codex/wow-internal-continuation`.
- Remote verified from configuration: `origin`,
  `git@github.com:darkraven92/MyProjects.git`.
- Last fully validated clean runtime commit: **not established**. Validation
  above applies to the existing dirty worktree, not just base HEAD.
- This P0.1 checkpoint reviews and includes the navigation follower/provider,
  their required navigation policy headers and focused navigation regressions,
  the inspector extension, and this document. Older unrelated dirty Questing,
  Grinding, vendor and GUI work remains unstaged and is not represented by
  this checkpoint.
- Do not stage build, state, cache, SQL database/WAL/SHM, captures, debug exports,
  private talent configs, or unrelated existing edits.
- Resolve continuity checkpoint with:
  `git log -1 --format='%H %s' -- docs/CODEX_PROJECT_STATE.md`.
- Verify synchronization explicitly with `git rev-parse HEAD` and
  `git ls-remote origin refs/heads/codex/wow-internal-continuation`.
  Equality does not mean remaining dirty runtime changes are published.

# Next steps

1. **AFK runtime gate first:** user launches/logs in on safe land with qualify
   mode; capture baseline, input-clock confirmation, flag clear and two windows.
   If targeted messages fail, inspect exact evidence before considering a
   bounded targeted Wine/X11 alternative. Never substitute a blind W loop.
2. Capture/reconstruct the failing live cave transition with current recovery
   telemetry; use the expanded offline report. Apply only a proved shared-layer
   correction, then regression/build/diff and runtime qualification.
3. Audit source-backed water flags/mirror timers/input before implementing
   shared water survival. Mesh water flags are insufficient by themselves.
4. Audit shared hazard confidence/direction/persistence; then combat watchdog,
   AFK input/actual state, general invariants and service consolidation.
5. Review/publish remaining prior runtime work in scoped dependency-complete
   checkpoints. Do not blindly stage all dirty files to make status clean.
6. P1 queue: ClassProfile -> live Spellbook -> Warrior -> Hunter -> racials ->
   two-level trainer cadence/refresh -> TalentBuild/URL validation/allocation ->
   private stable-ID library -> capability refresh -> LevelingProfile/Orc1-10 ->
   read-only GUI status. No personal talent build hardcoded into runtime.
7. P2 qualification: Questing 5/15/30/60 minutes, Grinding, natural water,
   fresh Orc to10; count manual interventions and all survival failures.

# Historical fixes that must not regress

- 792 identity uses live class to exclude incompatible same-title profiles;
  completion alone never invents an identity.
- 790/804 offer identity uses graph/acquisition eligibility before ambiguity;
  unknown history does not become satisfied from the offer title.
- Nonempty/ambiguous failed giver audit must not persist as verified-empty;
  acceptance requires live-log appearance and re-audits same giver.
- Generic discovery retains ownership across pending pickup; planner combat
  handoff cancels stale legacy pickup state.
- 16J.1 long ACTIVE attempts retain previous failure evidence; safe-boundary
  revisit cannot replace objective/retry/discovery/turn-in/death ownership.
- Navigation: displacement alone is not progress; episode evidence survives
  route refresh and combat suspension; no guessed directed polygon attribution.
- Vendor route probe shares execution validation, merchant capability requires
  live frame evidence, failed candidate uses bounded backoff.
- Equipment approval is not equip confirmation. Unknown compatibility fails
  closed; empty-slot support still requires independent/client evidence.
- Trainer command is not learning. Verify spellbook; completion ledger is
  confirmed history, not fabricated full historical quest completion.
- Preserve LOGGER/DLL/BOT/QUEST/GUI session markers and AFK diagnostics.
