# Project

Updated: 2026-10-06. Read this file AND `AGENTS.md` before continuing.

- Repository scope: `/home/ludvig/Programming/Projects/wow-internal-5875`.
- Git root is `/home/ludvig/Programming`; sibling projects are OUT OF SCOPE.
- Branch: `codex/wow-internal-continuation`.
- Client: WoW Vanilla 1.12.1 build 5875, 32-bit DLL under Wine/X11.
- Environment: Linux/CachyOS; C++20, MinGW/CMake; user commands in fish.
- Active request: Master Roadmap V6. This explicitly authorizes reviewed,
  validated checkpoint commits/pushes (superseding earlier no-commit requests).
- This is an initial continuity/navigation-audit checkpoint, NOT completion of
  the roadmap or an unattended qualification.

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

Current inspected log: `build/wow-internal.log`, 2026-10-06 17:02-17:06 UTC,
session `1404.134357533165383490.42335680.1980`. Level 21 Grinding run, NOT a
fresh Orc/cave qualification. Intent 1 arrived at tick 75, intent 2 arrived at
269; later roam intents were released during acquisition/watchdog activity.
Intent 8 failed `no_path` at tick 820. Acquisition watchdog reset at tick 856;
world became unavailable and GUI reported process absent at 17:06:43 UTC.
Cause of process exit is unknown. No surface-recovery episode appears in this
log. No WoW process was running at audit time.

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

New checkpoint changes are diagnostic/tooling only:

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

- Surface-recovery episode patch: current source requires ordinary portal
  progress plus destination gain before resetting; route refresh/combat retain
  evidence. No qualifying live run of this patch in the current log.
- Cave/interior navigation: P0.1 investigation incomplete. No narrow-entrance
  movement patch or runtime pass at this checkpoint.
- Combat attack continuity, acquisition liveness and AFK prevention need live
  diagnosis. Do not infer combat success from arrival events.
- Vendor/repair, equipment and trainer end-to-end qualification remains distinct
  from existing frame/spellbook/equip verification mechanisms and unit tests.
- Water-state/breath/surfacing/shore systems are not implemented by this work.
- Class profiles, automatic capability discovery, Hunter/racials, talent builds,
  talent library, leveling profiles and operational GUI remain queued.
- All unattended 5/15/30/60 minute and fresh Orc->10 gates remain RUNTIME PENDING.

# Known blockers

1. Current log replaced the cave run. Need a natural failure capture including
   actual player XYZ, current route generation and recovery events. A previous
   recovery TARGET is not proof of actual player position.
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

V6 continuity bootstrap and P0.1 evidence tooling complete; P0.1 gameplay
reliability is still RUNTIME PENDING. No later subsystem behavior patched.

Proceed in requested order, with source/test/build/diff and checkpoint after
each bounded subphase. Do not stack speculative fixes on the unproven cave
failure. Keep user/runtime claims, local source evidence and new runtime proof
separate.

# Runtime validation gates

1. Preserve one continuous session log before another GUI log clear/restart.
2. Natural cave route: intent acquisition -> source-backed corridor -> steering
   choice -> bounded recovery -> meaningful portal/destination progress OR
   bounded failure/release. Capture rejected target and actual player position.
3. Re-run offline audit at THAT player position, destination and portal; compare
   projection layers and clipped portal geometry before selecting a correction.
4. Repeat representative Questing and Grinding movement. No unearned attempt
   reset and no repeated same-target recovery loop.
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
tail -F build/wow-internal.log | rg --line-buffered 'LOGGER SESSION|BOT SESSION|MOVEMENT INTENT|NAV RECOVERY|NAV 14N.1 STEERING|LOCAL RECOVERY|NAV 14O.1|COMBAT STALL|AFK'

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

- Inspected base HEAD: `3c097cf` (`Diagnose long-distance route rejection`).
- Branch: `codex/wow-internal-continuation`.
- Remote verified from configuration: `origin`,
  `git@github.com:darkraven92/MyProjects.git`.
- Last fully validated clean runtime commit: **not established**. Validation
  above applies to the existing dirty worktree, not just base HEAD.
- This checkpoint includes this document, validation runner, offline inspector,
  inspector build script/helper/test and its unchanged terrain-policy dependency.
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
