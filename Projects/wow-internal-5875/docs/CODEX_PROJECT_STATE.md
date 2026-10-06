# Project

Updated: 2026-10-06. Read this file AND `AGENTS.md` before continuing.

- Repository scope: `/home/ludvig/Programming/Projects/wow-internal-5875`.
- Git root is `/home/ludvig/Programming`; sibling projects are OUT OF SCOPE.
- Branch: `codex/wow-internal-continuation`.
- Client: WoW Vanilla 1.12.1 build 5875, 32-bit DLL under Wine/X11.
- Environment: Linux/CachyOS; C++20, MinGW/CMake; user commands in fish.
- Active request: Master Roadmap V6. This explicitly authorizes reviewed,
  validated checkpoint commits/pushes (superseding earlier no-commit requests).
- This is a bounded P0.1 source/test checkpoint, NOT completion of the roadmap
  or an unattended qualification.

# Current architecture

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

# Source/test verified

2026-10-06 dirty-worktree validation using `python3 tools/validate.py`:

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

- Surface-recovery episode semantics have live pre-fix evidence above; the
  new local attribution/staging change is not yet runtime validated.
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

1. The newer live capture supplied actual player XYZ and recovery/route events;
   this source fix still needs two post-fix natural traversals. Do not confuse
   recovery target with player position or new-route pair with failed-ray pair.
2. Detour's local `raycast` is explicitly 2D (local VMaNGOS
   `DetourNavMeshQuery.cpp`, raycast contract). Runtime
   `IsSurfaceSegmentReachable` interpolates requested Z and does not verify end
   surface height. This is a source-backed contract risk, NOT proof that it
   caused the user's cave failure.
3. Offline sample at the reported recovery target (-169.688,-4333.750,67.899)
   to the reported objective (-58.1846,-4220.64,62.3418) gives 83 non-steep
   polygons, 304.325-yard straight-path length, matching start projection
   across route/local scopes and no 14O.1 rejection. First three ray endpoint
   heights match; later direct rays hit walls. This does NOT establish wrong
   layer selection. One portal midpoint clearance is below existing 0.70;
   that alone does not prove the full portal untraversable.
4. Offline tool does not replay runtime persistent hazards, loaded-tile history,
   wall-inset steering or collision. Its report cannot grant runtime PASS.
5. Current Grinding run has repeated acquisition/watchdog resets; exact control
   flow/root cause awaits P0 combat/ownership audit.
6. AFK candidate signal remains unverified in current log; no verified stationary
   clearing action or measured two-window prevention result. Do not add a blind
   W/jump loop.
7. Much prior working project code/data/tests is still dirty/untracked. This
   checkpoint publishes only reviewed audit/tooling files, not the complete
   current runtime. A clean remote checkout is NOT this validated worktree.

# Current phase

V6 P0.1 local steering and directed-failure attribution source/test/build
complete; gameplay reliability is RUNTIME PENDING. No later V6 subsystem
behavior patched.

Proceed in requested order, with source/test/build/diff and checkpoint after
each bounded subphase. Do not stack speculative fixes on the unproven cave
failure. Keep user/runtime claims, local source evidence and new runtime proof
separate.

# Runtime validation gates

1. Preserve one continuous post-fix session log before another GUI log clear.
2. Repeat the SAME natural corridor twice: intent -> exact steering attribution
   (known only if ray/portal proves it) -> safe stage/alternate route -> verified
   crossing and combat handoff or bounded failure/release. Zero manual movement.
3. No repeated known directed edge, anonymous pair where ray/portal evidence
   is sufficient, unearned attempt reset, same-target loop or safety weakening.
4. Compare actual player XYZ, route generation, projection and clipped portal
   geometry if post-fix behavior differs; offline reports are not live proof.
5. P0 water source audit next; then hazards, combat, AFK, invariants, consolidation
   and cross-workload validation. Do not claim implemented water safety early.
6. AFK requires two natural windows; drowning tests must not deliberately risk
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
tail -F build/wow-internal.log | rg --line-buffered 'LOGGER SESSION|BOT SESSION|MOVEMENT INTENT|STEERING FAILURE ATTRIBUTION|NAV ENTRANCE STAGE|NAV RECOVERY|NAV 14N.1 STEERING|NAV 14N.2 HYSTERESIS|LOCAL RECOVERY|NAV 14O.1|COMBAT STALL|AFK'

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

- Inspected base HEAD for this P0.1 phase: `f5d04e5` (offline cave audit).
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

1. Capture/reconstruct the failing live cave transition with current recovery
   telemetry; use the expanded offline report. Apply only a proved shared-layer
   correction, then regression/build/diff and runtime qualification.
2. Audit source-backed water flags/mirror timers/input before implementing
   shared water survival. Mesh water flags are insufficient by themselves.
3. Audit shared hazard confidence/direction/persistence; then combat watchdog,
   AFK input/actual state, general invariants and service consolidation.
4. Review/publish remaining prior runtime work in scoped dependency-complete
   checkpoints. Do not blindly stage all dirty files to make status clean.
5. P1 queue: ClassProfile -> live Spellbook -> Warrior -> Hunter -> racials ->
   two-level trainer cadence/refresh -> TalentBuild/URL validation/allocation ->
   private stable-ID library -> capability refresh -> LevelingProfile/Orc1-10 ->
   read-only GUI status. No personal talent build hardcoded into runtime.
6. P2 qualification: Questing 5/15/30/60 minutes, Grinding, natural water,
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
