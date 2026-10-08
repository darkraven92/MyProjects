# Project

Updated: 2026-10-08. Read this file AND `AGENTS.md` before continuing.

- Repository scope: `/home/ludvig/Programming/Projects/wow-internal-5875`.
- Git root is `/home/ludvig/Programming`; sibling projects are OUT OF SCOPE.
- Branch: `codex/wow-internal-continuation`.
- Client: WoW Vanilla 1.12.1 build 5875, 32-bit DLL under Wine/X11.
- Environment: Linux/CachyOS; C++20, MinGW/CMake; user commands in fish.
- Active request: Master Roadmap V6. This explicitly authorizes reviewed,
  validated checkpoint commits/pushes (superseding earlier no-commit requests).
- Current phase: V6 **P0.5.1 active-aggressor containment**. P0.4 autonomous
  swimming remains PAUSED; P0.4.1 observe-only is preserved, P0.4-TEMP living
  water avoidance remains enabled, and P0.4.2 has NOT STARTED. Preserve verified
  navigation/cache, AFK, DeathRecovery and mixed-state fail-closed behavior.

## P0.5.1 active-aggressor containment checkpoint (2026-10-08)

Starting HEAD `cdaa0486a74d29a5c952acbb072a85e474f51288` verified.
The newest ~98-minute natural Grind made 132 kills, 131 successful loots,
one DeathRecovery, and no movement recovery/idle deadlock/escalation before
active aggressor `0xF130000CD400514A` stalled at 80 HP. Selected GUID,
server victim, target victim, melee range, facing and Attack were correct;
player HP kept falling. Bounded refresh and same-target reengage restored the
latch, then a full fresh offensive observation window still saw no damage.
The prior terminal policy treated this live hostile as a system failure and
stopped the entire bot. Its `repairAttempts=2` was two repair dispatches,
not exhaustion of the three-dispatch ceiling; one hard refresh had been used.
P0.5 normal long-run status is RUNTIME FAIL at this active-aggressor terminal
event. P0.2.1 hard-refresh/post-reengage observation timing is RUNTIME PASS:
the complete fresh window elapsed; the still-unproductive hostile is the
separate P0.5.1 terminal-containment problem.

P0.5.1 adds one bounded, target-bound defensive containment episode before
that fatal branch: verify own Attack stop, plan a deterministic short dry
Detour escape from a complete aggressor-position snapshot, preserve hostile
identity and route/navigation ownership, and require sustained native combat-
and-aggressor-clear evidence before optional blacklist or mandatory owner
failure. Verified damage alone may resume the same fight with a fresh offense
clock; route dispatch and distance are not success. DeathRecovery still
preempts; water blocking cancels movement. Failed escape or contradictory
evidence remains explicitly fatal. No 3-repair/1-refresh, 120-second blacklist,
AFK, navigation 2000/4/2, or P0.4-TEMP water budget/policy changed. See
`docs/COMBAT_RELIABILITY_AUDIT.md`. Static status: SOURCE VERIFIED, TEST PASS,
BUILD PASS, DIFF CHECK PASS. Earlier full dirty worktree: 96 strict C++ tests, 26 audit
Python, 13 QuestDB Python, SQL fixture, eight Lua fixtures and DLL build PASS;
`/tmp/wow-validation-4jwcmhuh/results.json`. Isolated intended staged tree:
47 strict C++ tests, 26 audit Python, SQL fixture, three published Lua
fixtures, DLL/GUI/loader build PASS;
`/tmp/wow-validation-0j4fb46l/results.json`, including the final native-health
freshness guard. A normal GUI launch was attempted: GUI opened with WoW
stopped, but Start WoW did not start the client or create a gameplay session.
The launch replaced `build/wow-internal.log` (formerly ~20 MB) with a 253 KB
startup log; no old full-log copy was found in `runtime-captures/` or `debug/`.
The GUI opened for this attempt was closed. No new normal 30-minute Grind
or natural active-aggressor terminal containment occurred; P0.5.1 RUNTIME
PENDING. P0.4 remains PAUSED and P0.4-TEMP remains preserved.

Final post-ownership-hunk validation: full dirty worktree TEST PASS (96 strict
C++ tests, 26 navigation/water audit Python tests, 13 QuestDB Python tests,
SQL fixture, eight Lua fixtures), results
`/tmp/wow-validation-vj7xyv1l/results.json`. Fresh HEAD-plus-exact-staged
isolated tree `/tmp/wow-p051-final.4Yrndf` TEST PASS (47 strict C++ tests,
26 navigation/water audit Python tests, QuestDB SQL fixture, three published
Lua fixtures) and MinGW DLL/GUI/loader BUILD PASS; results
`/tmp/wow-validation-h2xbh6j7/results.json`. The final WorldMonitor activity
reports Movement only while the containment follower owns planning or
movement; arrival leaves the bounded containment policy observing hostile
evidence rather than manufacturing movement liveness. DIFF CHECK PASS.
Do not call the rare containment branch RUNTIME PASS without a natural episode.

## P0.5 combat closure checkpoint (2026-10-07)

Starting HEAD `bf08ec65c53edcb751d6c37da5533c5b00e2cb9b` verified.
Historical optional chase GUID `0xF130000CCD005092` exhausted its 3/3
recoveries, was blacklisted 120 seconds, and Grind continued. Source audit
found the prior global chase-abandon branch checked only that the failed
target's victim was not the player; this was not sufficient to establish safe
disengagement or mandatory/aggressor ownership. P0.5 routes exhausted chase
through full read-only combat evidence and the verified guarded terminal
release protocol. Direct chase now requires target-relative physical progress,
not command dispatch or lateral movement. Typed terminal reasons and target
loss/invalid no-kill attribution are added. Safe mandatory planner combat
failure is owner-visible rather than optional-blacklisted. Existing 3-repair/
1-refresh offense, 120-second blacklist, AFK, water pause, DeathRecovery, and
navigation 2000/4/2 budgets remain unchanged. See
`docs/COMBAT_RELIABILITY_AUDIT.md` for the source analysis.
Initial same-GUID UI selection retries are now bounded by the existing
five-failure limit and require native selected-GUID confirmation; optional
timeout abandonment requires complete no-hostile/no-Attack evidence, while
mandatory timeout is returned to its objective owner.

Static status: SOURCE VERIFIED, TEST PASS, BUILD PASS, DIFF CHECK PASS.
Full dirty worktree validation:
95 strict C++ tests, 26 navigation/water audit Python tests, 13 local QuestDB
Python tests, SQL fixture and eight Lua fixtures; results
`/tmp/wow-validation-f3ca94vp/results.json`. Isolated staged-tree validation:
46 C++ tests, 26 audit Python tests, SQL fixture and three published Lua
fixtures, plus complete DLL/GUI/loader build; results
`/tmp/wow-validation-22lbd2st/results.json`, tree
`ddb3b714bac16af6d6ef7fc0e078517cfd292565`. The first isolated run
exposed an accidental dependency on an unrelated dirty quest-interface
method; the staged implementation was corrected and the final isolated run
passed. P0.5's new normal-runtime gate remains PENDING. The P0.2.1 rare
deep-offensive-stall terminal path remains RUNTIME PENDING until a natural
episode exercises its fresh post-reengage window and terminal branch. Do not
infer a new runtime PASS from prior logs or a build. A normal GUI launch was
attempted after static validation: the GUI opened with `WoW: Stopped` and
`Game not running`, but its Start WoW control did not start the client in this
environment. No new `build/wow-internal.log` session or 20-minute P0.5 runtime
exists; the GUI opened for this attempt was closed. Runtime remains PENDING.

## P0.4-TEMP living-water avoidance checkpoint (2026-10-07)

Starting HEAD `b076ed53c9bf7a81f7a891fbaa5c7578d9a30a20`. Autonomous
swimming remains PAUSED; P0.4.2 is NOT STARTED. Production-default living
navigation now uses a query-time Ground include `0x01` plus Water exclude
`0x08` on route, projection, local ray/wall/surface and all fallback tiers.
The exclude bit is necessary because shallow mixed Ground|Water polygons are
`0x09`. A complete diagnostic water route is never returned to the follower;
where it proves a water-tagged corridor, terminal reason is
`water_traversal_disabled`. Optional Grind roam/approach uses existing bounded
abandon/cooldown; mandatory owners retain failure rather than objective success.
No water polygon is learned as a hazard. Session topology, cache generation,
terrain validation and 2000/4/2 budgets are unchanged. DeathRecovery alone
explicitly retains the old Ground|Water behavior after Ghost confirmation.

The source-verified movement-word SWIMMING bit now enters a production
`LivingWaterBlocked` pause for a living/unknown player: one CTM-to-current-
position neutralization, release of ordinary route owners, no workload or
seated recovery updates, and read-only observation until three consecutive
known non-swimming samples. Exit is `non_swimming_confirmed`, NOT DryGround.
DeathRecovery preempts on dead/Ghost evidence. No Space, Jump, ascent, liquid-Z,
surface or submerged assumption was added. AFK remains observed but candidate
input is conservatively deferred while water-blocked. A failed CTM stop ends
the bot session explicitly. Active combat target identity is retained; no
water combat optimization exists. This is manual-recovery containment, not
drowning prevention. General and water-specific runtime gates are PENDING.

P0.4.1 observe-only mode and audit remain intact. Swimming reader is SOURCE
VERIFIED but a natural live water encounter has not been observed in this
checkpoint. P0.4.2 may replace this guard only after independent source and
runtime qualification of surface swimming, submersion and ascent/release.
Validation: full dirty tree TEST PASS, 93 strict C++ tests plus 26 audit
Python, 13 local QuestDB Python, SQL and eight Lua fixtures;
`/tmp/wow-validation-s6hlrrkm/results.json`. Isolated intended staged tree
TEST PASS, 44 strict C++ tests plus 26 audit Python, SQL and three published
Lua fixtures, complete MinGW DLL/GUI/loader build;
`/tmp/wow-validation-v6h1_lbq/results.json` from
`/tmp/wow-water-isolated.qA8H48`. Explicit BUILD PASS and DIFF CHECK PASS.
No WoW process was running, so the >=20-minute normal Grind regression and
natural water-avoidance gates remain RUNTIME PENDING, never inferred from tests.
Offline real-Detour cache probe PASS: query-mode changes reused all 704 loaded
tiles with zero new reads/addTile calls. Direct short-range combat/interaction
CTM bypasses Detour route filtering; the swim-bit guard stops continued
automation after observed entry, not the first physical contact. Preserve this
residual risk and do not claim absolute water-entry prevention or P0.4 PASS.

## P0.4.1 observe-only evidence checkpoint (2026-10-07)

Starting HEAD `9107b2c93d661218c5b8902c42a9e16d63e9ab36`, correct branch.
P0.4's SOURCE GAP remains for submersion, breathable surface, waterline,
positive dry-ground contact, fresh live breath and safe ascent/release.
The P0.4.1 implementation adds a signature-guarded read-only movement-word
adapter and `WOW_INTERNAL_WATER_MODE=observe`. This diagnostic branches before
Combat/Quest/Grind controller startup. It only reads the world/player movement
word and emits sparse `WATER EVIDENCE`; it never moves, targets, sends input,
hooks mirror packets, invokes Jump or writes gameplay state. Normal mode is
unchanged. Source loss is Unknown; `!swimming` is `DryOrNonSwimming`, NOT dry
ground. Live breath/fatigue/submerged/surface/contact fields remain unknown.
Two consecutive known samples confirm a diagnostic swim/non-swim transition.

Binary audit now proves paired Jump *binding callbacks* (`0x513BD0` start,
`0x513D50` stop), but not a safe swim-ascent/held-release contract. Mirror
START/PAUSE/STOP are decoded in offline tests; their handler forwards Lua
events and does not expose a validated native timer cache. The `0x5E7B10`
helper is a spell-icon lookup, not a breath getter. The audit tool distinguishes
source-verified, runtime-observed and inferred counts; no breath event reader
is claimed. Exact details and safe manual runbook are in
`docs/WATER_SWIMMING_DROWNING_AUDIT.md`.

Latest existing full log still has ZERO water encounter events. No WoW process
was running during implementation; new swim reader, breath, surface,
submersion and ground exit are RUNTIME PENDING. P0.4.2 autonomous swimming,
surfacing, shoreline exit, water combat and loot are NOT IMPLEMENTED. A later
user-manual shallow-shoreline observation can qualify only the signals actually
observed. Do not label P0.4 RUNTIME PASS from the current test/build results.
All navigation/cache 2000/4/2 safety budgets and AFK/DeathRecovery/combat
behavior remain unchanged.

Validation: TEST PASS full dirty worktree 92 strict C++ tests, 26 audit Python
tests, 13 local QuestDB Python tests, SQL and eight Lua fixtures; results
`/tmp/wow-validation-0vostnxd/results.json`. BUILD PASS with explicit
`cmake --build build`; DIFF CHECK PASS. Isolated staged-only export
`/tmp/wow-p041-isolated.eZwvle`: TEST PASS 43 published C++ tests, 26 audit
Python tests, SQL, three published Lua fixtures and full DLL/GUI/loader build;
results `/tmp/wow-validation-vhvuu2a7/results.json`. The 13 local QuestDB
Python tests are unpublished unrelated work and absent from the isolated tree.
No live WoW observation occurred; all new runtime water gates PENDING.
Follow-up audit-only correction requires `breathActive=yes` before counting a
drain/refill scale. Final rerun PASS: full
`/tmp/wow-validation-oz2svn1s/results.json` (92 C++), isolated
`/tmp/wow-validation-2ixqiaz9/results.json` (43 C++), plus explicit build
and diff checks. The isolated export is
`/tmp/wow-p041-final-isolated.ZmhzAw`; no new gameplay path changed.

## P0.4 source gate (2026-10-07; authoritative current status)

Starting HEAD verified exactly4ff143940b0f3e409742e00ab0e0840e4117b572 on the
requested branch. Latest FULL capture preserved at
`/tmp/wow-p04-baseline.yBnhXp/wow-internal.log`,137775lines, session
2000.134358700080773630.132596093.1776,38.07115minutes, normal user Stop.
Final67kills/67lootOk/0deaths (longer than the user's55-kill intermediate report),
movementRecoveries0/runtimeRecoveries0/runtimeStrategic0/runtimeIdleDeadlocks0/
runtimeEscalations0; seven confirmed AFK actions.

P0.3 Navigation RUNTIME PASS; P0.3.1 Acquisition liveness RUNTIME PASS;
P0.3.2 NavMesh cache RUNTIME PASS: cold30+60+614=704 disk/additions,5085hits;
four warm full-map requests704hits/0reads/0adds each,264.276–293.499ms.
Prior combat/AFK/Ghost/death qualifications retained. No natural death in this
latest capture. Historical sections below retain their original pending gates;
this section supersedes those baseline statuses, not their evidence details.

Read `docs/WATER_SWIMMING_DROWNING_AUDIT.md` before continuing. SOURCE VERIFIED
movement swimming bit00200000 at player+118 -> movement+40; mirror packet/event
type1 BREATH/current/max/scale/paused; mesh ground01/water08/steep10. Critical
negative findings: active breath can be REFILLING at surface; missing breath
is not dry/surface proof; water polygons may be underwater terrain, not waterline.
No reliable live submersion/surface-height snapshot or bounded ascent RELEASE
adapter is yet SOURCE VERIFIED. User sections31/48 require STOP before guessed
behavior. No production file, ownership, filter, budget or input path changed.

Per-capability P0.4: water-state detection SOURCE GAP / RUNTIME PENDING;
surface swimming, shore exit, submersion detection, drowning prevention and
combat-in-water interaction NOT IMPLEMENTED / RUNTIME PENDING. DeathRecovery
baseline RUNTIME PASS, unchanged; new water interaction not qualified. Do not
claim water-safe eating/loot or enable capabilities from this audit alone.

Added read-only `tools/water_client_audit.py` (exact PE/hash/signatures),
`tools/water_log_audit.py` and13 deterministic Python tests. The log audit
reports no verified water encounter and waterRuntimeQualified=false for the
baseline; zero water events is not PASS. Behavior regressions for unimplemented
surfacing/shoreline are not claimed. No WoW process available for live probing.
Next: validate coherent exact-client mirror/submersion evidence and ascent/release
semantics, then implement/test supported water behavior. No unsafe runtime test
or later roadmap work. All2000/4/2, cache and liveness invariants remain intact.

Validation: TEST PASS full dirty worktree91 strict C++ tests,22 audit Python
tests (13 new water-tool tests),13 QuestDB Python tests, SQL fixture and8 Lua
fixtures. Results `/tmp/wow-validation-fev2rjt0/results.json`. BUILD PASS in
validation and separate `cmake --build build`; DIFF CHECK PASS.
Isolated intended tree `/tmp/wow-p04-isolated.8Eot1v`: TEST PASS42 published
C++ tests,22 audit Python tests, SQL and3 published Lua fixtures; full DLL/GUI/
loader/testhost BUILD PASS. Results `/tmp/wow-validation-vwqoi5ni/results.json`.
No unpublished dependencies were added. Seven intended audit/tool/test/doc
files only; no broad dirty source hunks staged. Final documentation records
these results without changing the tested implementation. Current checkpoint
hash is discoverable via `git log -1 -- tools/water_client_audit.py`; verify
local/origin/ls-remote HEAD rather than assuming a hash from this document.

## Current P0.3.2 topology reuse checkpoint (2026-10-07)

P0.3 and P0.3.1 RUNTIME PASS in the newest full capture, preserved as
`/tmp/wow-p032-baseline.3WXFZs/wow-internal.log`, session
1792.134358658215650330.128405269.32,14,852 lines,29.6273min. Exact final
counters: movementRecoveries0/runtimeRecoveries1/runtimeIdleDeadlocks0/
runtimeStrategic1/runtimeEscalations0. No false navigation-handoff acquisition
deadlocks. Strategic outcome semantics remain unchanged. Prior sections below
retain historical source checkpoints/gates, not current runtime status.

Dominant latency: nine completed704-tile full-map rebuilds at123–129 seconds
each,1118.984s total; tenth cancelled652tiles/120.125s. All init profiles opened
8330tiles. New read-only `tools/navmesh_cache_audit.py` reports full-log counts,
warm latencies and exact counters; absent baseline cache metrics remain unknown.

SOURCE VERIFIED: each follower owned a mesh; every init tier and new intent
freed/reallocated/reloaded it. Now one compatible world/map topology cache
retains completed tiles across intents/tier failures; only missing tile files
are queued. Identity: map/directory/params/mmap metadata; cached requested tile
metadata/reference checked. Query/node pool and corridor/recovery/provenance
remain private. All mesh operations serialized; temporary hazard flags restored
under lock with RAII. Hazards/terrain validation still apply to each query.

Invalidate on map/directory switch (even unavailable new metadata), world gap,
incompatibility or session teardown. Stale generation fails with typed
mesh_cache_invalidated, clears old corridor refs and releases intent; command-
time generation check rejects stale CTM. No raw-pointer lifetime shortcut.
No optional/mandatory fallback change without endpoint-coverage proof; no GUI
redesign. Existing typed initialization observations now include verified reuse.
Full details/evidence: `docs/NAVMESH_CACHE_AUDIT.md`.

Real-provider offline Wine integration TEST PASS:30coldroute ->60newexpanded
->614newfullmap =704. Warm route2.215–2.877ms, expanded3.626ms, full27.493ms;
zero warm disk reads/addTile calls, unchanged existing poly ref, concurrent
hazard masks restored, terrain outcome retained, stale world/map references
and command callbacks rejected. Temporary probe capture
`/tmp/wow-p032-final-probe.Rxh40f/wow-internal.log`. These are OFFLINE timings,
not WoW performance qualification; cold live loading retains the2tile/tick cap.

TEST PASS:91 strict C++ tests plus nine navigation/cache Python tests,13 local
QuestDB Python tests, SQL and eight Lua fixtures in full validation
`/tmp/wow-validation-in9p4f8p/results.json`. BUILD/DIFF CHECK PASS in full
validation and separate final commands. Isolated intended staged source tree
PASS:42 published strict C++ tests, nine navigation/cache Python tests, SQL and
three Lua fixtures, complete DLL/GUI/loader/testhost build; artifacts
`/tmp/wow-validation-2yk04h_a/results.json`, tree `/tmp/wow-p032-published.IkP9Qz`.
The explicit strict real-Detour probe also built/runs without dirty dependencies;
published-provider capture `/tmp/wow-p032-published-probe.MQUqJc/wow-internal.log`
shows warm route1.581–1.726ms/expanded2.739ms/full14.285ms, zero warm additions.
AFK, combat, acquisition/strategic policies untouched. DeathRecovery's existing
learned-edge handoff carries generation metadata and refuses unknown/stale refs;
its routing/reclaim/budget policy is unchanged. WorldMonitor only adds isolated
mesh lifetime hooks. Budgets2000/4/2 and coldstep2
unchanged. All unrelated dirty work preserved; no new unpublished dependency.

P0.3.2 RUNTIME PENDING: no live WoW process available for the new >=20min normal
Grinding performance gate. Require compatible warm requests without tile
reloads; warm route preferably<5s end-to-end; cold loads/cache hits/misses,
latencies/invalidation reasons; near-zero genuine movement recoveries, zero
false idle debt, AFK prevention/combat stable, natural death regression if it
occurs. Preserve full log and run the cache audit. No swimming or later phase.
Checkpoint commit/push authorized; verify local/remote HEAD after scoped review.

## Current P0.3.1 navigation handoff checkpoint (2026-10-07)

SOURCE VERIFIED: complete newest session1808.134358630204354240.125757707.1448,
13122 lines, preserved `/tmp/wow-p031-baseline.Dow16n/wow-internal.log`.
Start/stop monotonic125758217/127036291:21.3012min. Exact baseline counters:
movementRecoveries0/runtimeRecoveries6/runtimeIdleDeadlocks5/runtimeStrategic1/
runtimeEscalations1. Five optional Roam terminals immediately triggered false
acquisition deadlocks at ticks428/1705/2131/2558/3723, ages427/428/426/427/425.
RuntimeRobustness returned from progressing initialization BEFORE reconciling
acquisitionEpochActive_; the old epoch survived ~125s full-map loads. This is
not another physical movement stall or a reason to raise watchdog limits.

Fix: reconcile actual owner before EVERY initialization return, using follower
OwnsMovement plus explicit initialization and Roaming/ApproachingTarget states.
Navigation/vendor/recovery/first-aid/dialog/death ownership ends acquisition
debt; navigation release starts a fresh epoch. Idle/AcquiringTarget/WaitingFor-
TargetSelection/GUID churn inside acquisition does NOT reset age. Existing
40-tick idle window unchanged, with transition-only ACQUISITION LIVENESS logs.
Death ownership still resets/skips this supervisor in WorldMonitor as before.

Strategic event was real outcome debt1346ticks (gray migration, no new XP/kill),
but fired tick2132 immediately after false idle recovery2131. Keep XP/kill/
level/vendor outcome clock; planning, physical work and alternate sectors are
NOT outcomes. Use existing40-tick acquisition/cooldown opportunity after nav
handoff or tactical reset to avoid duplicate strategic teardown. Do not forgive
outcome debt or suppress sustained work-without-outcome detection.

Known local edge0x10000600000215->0x1000060000021D was suppressed, recovery
unavailable, then reported other_unknown. Now append typed failure
bounded_local_recovery_unavailable BEFORE SetState publishes intent release.
No change to geometry acceptance, recovery budgets, hazards or roam abandonment.
Persistent-hazard/projection/path-validation terminals stay failures, not success.

Focused failing-before-fix replay reproduced acquisition debt during planning.
TEST PASS:89 strict C++ tests, all registered Python/SQL/Lua fixtures, full
validation `/tmp/wow-validation-ynvhy5s5/results.json`. BUILD/DIFF CHECK PASS
in full validation and separate final build/diff commands. Isolated intended
tree PASS:40 published strict C++ tests, six navigation Python tests, SQL and
three Lua fixtures, full DLL/GUI/loader/testhost build; artifacts
`/tmp/wow-validation-5ewtyz9a/results.json`, tree `/tmp/wow-p031-published.c5O3Ad`.
Only9 intended files/hunks staged, inspected and diff-checked; no unrelated
dependencies published. Runtime-after counters UNKNOWN.
P0.3.1 RUNTIME PENDING: fresh normal Grind>=20min, preserve full log, zero false
nav-handoff idle recoveries, still bounded real acquisition detection; no
known-edge exhaustion other_unknown; AFK/combat and natural death remain green.
No WoW process available to exercise this gate. Do not manufacture failures.

Intended changes: RuntimeRobustnessSupervisor, ONLY new WorldMonitor/Grind
ownership/telemetry hunks, navigation failure enum/follower terminal ordering,
acquisition replay and existing attribution test, these two navigation docs.
All unrelated dirty work preserved. Baseline HEADd1a0c91777f5490914e64a65d373ecdd2664fa9b.
Resolve checkpoint via `git log -1 --format='%H %s' -- tests/acquisition_liveness_owner_test.cpp`;
verify local/remote HEAD. Existing path length2000/surface4/backtracks2 and
AFK/DeathRecovery/combat source policies unchanged. No swimming or later V6
phase begins; next action is P0.3.1 runtime qualification.

## Historical P0.2.1 combat checkpoint (2026-10-07)

SOURCE VERIFIED: newest complete session968.134358579023063750.120736874.1760
ends against GUID0xF130000D85003A7A/entry3461. Earlier latch reengage repaired
82->69 damage. Final HP8 stall triggers refresh at4296ms; post-refresh Attack
off/server victim0, reengage only near8638ms, active latch confirmed at8994ms
then immediate Failed/session stop. Only356ms after the last inactive sample:
NOT proof of several failed active swings after final reengage. Initial HP8
pause remains UNKNOWN (no evade/immune/swing data in that capture).

Proven policy defect: same-script toggle assumes synchronous IsCurrentAction;
pending refresh waits for damage even when Attack is observably off; eligibility
counts inactive/unselected time. New explicit one-stop -> fresh off -> bounded
same-target start; active offensive eligibility is distinct from unchanged
no-damage clock. Three repairs/one refresh,1s structural,4s/8s period-aware
windows unchanged; HP decrease/death alone verifies refresh/refunds budget.

Typed optional-Grind terminal verification: authoritative native combat clear,
complete live victim scan/no remembered aggressor, stable HP and safe UI/input
required. One stop-own-Attack/ClearTarget, verify selection0/victim0/Attack off,
existing120s SAME-GUID blacklist, abandoned NOT kill, BeginAcquire. Mandatory,
active hostile, unknown or input-conflicting conditions retain explicit system
failure. No new speculative survival/escape or Failed whitelist. Real WorldMonitor
Failed->stop remains; safe optional target failure never enters Failed. Sparse
replicated flags/victims/execution evidence; attack timer/evade remain UNKNOWN.
Full source/timeline/guards/remaining uncertainty: `docs/COMBAT_RELIABILITY_AUDIT.md`.

TEST PASS:88 strict C++20/Wall/extra/Werror tests, Python/SQL and eight Lua
fixture programs; final full run `/tmp/wow-validation-h3q8sgiv/results.json`.
Isolated intended publication tree also PASS:39 published strict C++ tests,
six navigation-audit Python tests, QuestDB SQL fixture, three published Lua
programs and DLL/GUI/loader/testhost build. Results
`/tmp/wow-validation-_lbw4d7f/results.json`, staged snapshot
`/tmp/wow-combat-p021-published.Cp8Xzu`. Unpublished QuestDB Python fixtures are
reported absent in that snapshot, not silently claimed tested there.
Failing-before-fix replay proves delayed reengage;
new terminal tests, asynchronous Lua latch and own-release guards added.
BUILD/DIFF CHECK PASS in full validation and separate final commands; staged
diff inspected and checked, ONLY11 intended files/hunks. Dirty work remains local.
RUNTIME PENDING: no WoW process available. Normal unattended Grind>=20min,
full capture, natural repair/damage or safe abandonment; never deliberately
create a dangerous stall. Source/test completion does not establish the
initiating damage failure is fixed. P0.2 natural latch repair remains PASS.

P0.3 key navigation fixes RUNTIME PASS in observed~12.5min latest session:
movementRecoveries0/runtimeRecoveries0/runtimeEscalations0, progressing loaders
preserved. Full20–30min gate interrupted by combat failure, still PENDING.
AFK normal/Ghost and complete natural DeathRecovery remain baseline RUNTIME
PASS. No navigation/AFK/death source or budgets2000/4/2 changed here.

Intended files: combat liveness/action/client-evidence/AutoAttack, ONLY new
terminal-related CombatController hunks, new terminal policy/test, extended
liveness/Lua tests, combat audit and this state. Preserve unrelated dirty pull,
quest, WorldMonitor, GUI/services/data work unstaged. Baseline HEAD
03c52b1177d035763d7e71d6aa3fd89af4abe2a9. Resolve checkpoint identity with
`git log -1 --format='%H %s' -- src/Bot/CombatTerminalPolicy.h`; verify remote HEAD.
Combat source checkpoint73990cebf3fe8a4899e6c544be7531547d3faa4a pushed normally;
GitHub remote HEAD equality verified. Follow-up only deduplicates deep-stall
telemetry while terminal release verification is pending and records continuity;
no additional recovery or terminal-policy behavior. Current HEAD is resolved
with `git rev-parse HEAD` / `git ls-remote origin refs/heads/codex/wow-internal-continuation`.
No swimming or other V6 phase begins. Next task is this bounded runtime gate.

## Historical P0.3 navigation checkpoint (2026-10-07)

SOURCE VERIFIED: newest full session684.134358538029914000.116409170.1128
ran20.0032 minutes, movementRecoveries16/runtimeRecoveries42/runtimeEscalations30
(0.800/2.100/1.500 per minute). Read-only full-log audit shows ALL16 movement
hard stalls cancelled a progressing expanded loader with commands0/replans0,
not stalled CTM. The first AutonomySupervisor physical watchdog killed work
before the later RuntimeRobustness planning deferral. Thirteen cancellations
were around one captured position, but corridor/edge identity is UNKNOWN
before a query; do not invent a repeated directed failure.

Independent SOURCE VERIFIED posture bug: full OBJECT descriptor offset0x210
read native display ID (runtime byte51), not UNIT stand state. Binary UNIT
binders add0x18; correct full-base stand-state offset0x228 matches VMaNGOS.
Twenty-four global recoveries used unexpected_seated_idle;18 acquisition
idle_deadlock events are distinct and not claimed fixed. Eleven plan failures
were persistent hazard policy rejection previously other_unknown; five were
destination projection failures previously no_path. Offline exact-position
inspection reproduced destination-poly0 for two roam targets, with a30.83-yard
surface delta in one sample. No guessed height/terrain relaxation added.

Small fixes: actual provider tile-progress snapshot through Grind/WorldMonitor
to AutonomySupervisor; advancing planning is not physical progress. Frozen
work still fails at48 ticks; same intent keeps existing240000-ms absolute
planning ceiling across tiers; regressed work fails explicitly. Execution
watchdog, follower episodes and RuntimeRobustness counters are unchanged.
Correct posture offset and reject unsupported bytes. Typed projection/hazard
failure attribution plus raw query/validation/underlying-hazard diagnostics;
explicit optional-roam abandonment only after shared-nav terminal. Existing
directed attribution, CTM provenance, recovery budgets2000/4/2,14O.1 and
persistent hazard memory are untouched. Full evidence/state-pipeline/runtime
runbook: `docs/NAVIGATION_RELIABILITY_AUDIT.md`.

Latest baseline directly corroborates P0.2 RUNTIME PASS: natural same-target
autoattack-latch desync repaired forGUID0xF130000D58003736, then targetHP100->92;
seven confirmed recovery verification events in a normal20-minute combat regression session.
AFK three confirmed prevention pulses including combat-defer/resume remain
RUNTIME PASS. Deaths0 in this capture; earlier complete natural DeathRecovery
cycles remain RUNTIME PASS, not newly exercised here. AFK/death/combat policies
are unchanged by P0.3.

TEST PASS:87 strict C++ tests, six new offline-audit Python tests, existing
QuestDB/SQL and eight Lua fixture programs; full run
`/tmp/wow-validation-24x50g5h/results.json`, BUILD/DIFF PASS. Focused regressions:
planning progress/freeze/regression/bound, intent/tier continuity, physical
execution/no-displacement, posture descriptor base, precise failure classes,
offline correlation/no fabricated geometry. Isolated intended checkpoint also
PASS:38 published strict C++ tests, six audit Python tests, QuestDB SQL fixture,
three published Lua programs and DLL/loader/GUI/testhost build; results
`/tmp/wow-validation-feq40npk/results.json`, snapshot
`/tmp/wow-navigation-final.7qQhjW`. Missing published QuestDB Python fixtures
are explicitly reported; discovery is registered only when files exist, still
running all13 local fixtures in full-worktree validation. Unrelated tests/code
are not staged to make the checkpoint appear dependency-complete.
RUNTIME PENDING: fresh normal20–30-minute unattended Grind, full capture, loader
completion without false cancellation, valid posture evidence, coherent intents,
counter-rate comparison and preserved AFK/combat/natural-death regressions.
No metric reduction is claimed from source/tests. No later V6 phase starts.

Files: AutonomySupervisor, PlayerPostureController/new evidence policy, shared
follower/initialization telemetry, isolated Grind/WorldMonitor navigation hunks,
two new C++ regressions/extended telemetry test, read-only navigation_log_audit
and Python fixture/validation registration, this document and navigation audit.
Preserve ALL pre-existing broad WorldMonitor/Grind/combat/quest/GUI/service/data
changes unstaged. Baseline HEAD6cbf0cdec1bf3342125cc91b205459dab6ce6945. Resolve
published code identity with `git log -1 --format='%H %s' --
src/Bot/PlayerPostureEvidencePolicy.h`; compare local HEAD with remote branch.

## Historical P0.2 Combat checkpoint (2026-10-07)

SOURCE VERIFIED: CombatController used descriptor UNIT_FIELD_TARGET (server
attack victim) as UI selection. AttackStop clears that victim; native SetTarget
returns early when actual selection B4E2D8/DC already matches. New signature-
validated read-only native selection replaces combat selection guards, without
changing WorldState/AFK victim semantics. Exact audit, sample provenance, state
machine, timing, safe bounded repair ladder and uncertainties are in
`docs/COMBAT_RELIABILITY_AUDIT.md`.

Newest live session352.134358492686284770.111867086.624 has locked
0xF130000D58003734/entry3416 at1.5767yd, target83/100 while player435/536 then
falling. Global recovery stops Attack and subsequent repeated same-GUID
SetTarget returns cannot resolve the falsely labelled zero client target.
Actual UI GUID was not independently captured in that historical episode;
initial cause of no damage remains uncertain, NOT invented from the zero victim.
lowHpHardStallRecoveries only counts <=15% target finishers, not this83% target.

New typed per-target/object watchdog distinguishes selection/latch/facing/range/
cast/ownership/no-progress/unknown evidence. Only target HP decrease/death earns
damage progress; commands, movement and target restore do not refund repairs.
Three repair dispatches max, one no-damage hard refresh, 1s structural verify;
4s urgent/8s normal windows also respect native main-hand period and continuous
eligible evidence. Command-time same-GUID/alive/range/facing/UI/cast guards;
unsupported no-slot refresh fails closed. Global aligned-melee recovery defers
to this watchdog; non-melee recovery and navigation2000/4/2 stay unchanged.
AFK and death code are UNCHANGED. Separate structural vs damage verification
is explicit; no direct combat-state writes, random movement or arbitrary target.

Latest log directly verifies TWO baseline death-to-alive cycles:
DEATH RECLAIM fresh_alive_probes at20440/23844; DEATH RECOVERY EXIT aliveConfirmed
at20468/23872, Grind resume. User reports no manual intervention. DeathRecovery
baseline is now RUNTIME PASS; historical P0.1 pending status below describes
the earlier checkpoint, not the newest evidence. AFK normal/Ghost baseline
remains RUNTIME PASS; post-combat-patch regression and desync recovery PENDING.

TEST PASS:85 strict C++20/Wall/extra/Werror tests, Python/SQL tests and eight
Lua fixtures; final full run `/tmp/wow-validation-2uy50ri0/results.json`.
BUILD PASS and DIFF CHECK PASS: full validation and subsequent separate
`cmake --build build`, `git diff --check -- .`. Isolated intended-checkpoint
DLL/loader/GUI/testhost build PASS in `/tmp/wow-combat-checkpoint-JvOAAX/build`.
That check exposed a pre-existing committed WorldMonitor reference to missing
WaitingForManualVendor: ONLY its already-local enum declaration/state-name case
is included as a mechanical compile dependency, preserving existing enum values
and leaving vendor behavior/AFK unchanged. No broad Grind controller changes.
Full-worktree validation includes pre-existing dirty work, NOT a claim that all
that work is committed.

Files intended: combat controller hunks, AutoAttackController, new
CombatLivenessPolicy/CombatClientEvidence5875/CombatActionEvidenceScript,
combat regression/fixture, validation registration and combat/project docs,
plus the two isolated Grind enum/name compile-dependency hunks above.
Pre-existing dirty combat pull/quest/chase diagnostics and all unrelated
WorldMonitor/GUI/trainer/equipment/quest files must remain unstaged.
Commit/remote identity: resolve this checkpoint with
`git log -1 --format='%H %s' -- src/Bot/CombatLivenessPolicy.h` and compare
`git rev-parse HEAD` with
`git ls-remote origin refs/heads/codex/wow-internal-continuation`.

RUNTIME PENDING: no running WoW. Normal natural Grinding must preserve ordinary
damage, AFK combat-defer/resume and death priority; capture any natural desync
through same-GUID recovery and damage confirmation. Never deliberately kill or
manufacture an unsafe stall. No later V6 subsystem starts in this checkpoint.

## Historical P0.1 DeathRecovery checkpoint (2026-10-07)

SOURCE VERIFIED: distinct missing-anchor and strategic-route defects.
Full state/source/binary/runtime reconstruction: `docs/DEATH_RECOVERY_AUDIT.md`.
Newest session1360.134358440699456960.106923713.436: Ghost start
(-1081.400024,-3478.679932,63.606602), body destination
(-646.515,-3524.180,91.7361), distance438.163. Route/expanded reject unsafe
terrain; sole full-map loader cancelled at188/704 tiles after35061 ms by the
160-tick precision watchdog, WITHOUT any route movement. Later strategic
attempt repeats this cancellation; retry43 fails expanded validation and
reports strategic_route_failed at27667. This does not prove projection failure
or reclaim failure. Terrain rejection remains intact; safe full-map route is
still unproven.

Fixes: pending loader retains the SAME death movement intent across tier work;
planning never resets physical liveness. Original180/300-second death bounds,
18 route attempts, one full-map fallback, 2000/4/2 navigation limits unchanged.
Failure evidence retains projection, exact query error/tier and validation
detail so terminal diagnostics no longer hide the unsafe leg behind strategic
failure. No terrain/global movement mask relaxation.

Bootstrap missing-anchor previously failed immediately after checking only
last healthy position/persisted record, never server corpse cache. New
AwaitingCorpseAnchor holds shared death ownership/no movement for at most12s,
then exact corpse_anchor_pending_timeout. This publication bound is derived
from existing release opportunities, not a measured network timeout. Stale
healthy positions are no longer treated as corpse authority. Observed body
and recent GUID/map/age-validated record are ROUTING ONLY; fresh server location
is required for reclaim. Cross-map dungeon entrance is explicitly rejected.

Read-only 5875 audit: MSG_CORPSE_QUERY0x216 ->48F734 ->492010 stores display
map B4E31C/XYZ B4E284/actual map B4E320. Ghost-change5EEAC1 and world-init490A28
call491F50 to invalidate/query. Reader runs on game thread with signature,
same-player/Ghost/map/finite guards. No packets, writer calls or synthetic GUID;
cache GUID/age unknown. Reproduce `python3 tools/death_client_audit.py
'/home/ludvig/Games/WoW Vanilla/WoW.exe'`. Hash matches AFK audit.

Reclaim retains fresh Ghost/delay/8-yard gate, two fresh post-command alive
probes. Unknown delay cannot grant reclaim or invalidate independent alive
evidence. Release retries no longer reset unearned six-attempt budget; reclaim
commands capped at existing eight per death, independent of local precision
reroutes. World gap cancels follower without a command to an unavailable
player; next valid snapshot records world_state_lost under death ownership.
Failed death ownership intentionally remains while dead/Ghost; movement intent
releases. Existing positive-alive/manual re-arm and both workload gates retained.

Files: DeathRecoveryController/Policy, new EvidencePolicy/CorpseLocation5875,
shared follower diagnostics, death regressions, read-only audit tool and docs.
Reviewed pre-existing death-specific anchor store, strategic terminal policy,
ownership policy and their regressions are included as required dependencies;
unrelated dirty WorldMonitor/gameplay changes remain UNSTAGED. Full validation
is of this dirty worktree, not proof of a clean remote runtime.

Validation: intermediate84-test run found an old FailedMissingAnchor enum
expectation in bot_death_ownership_policy_test; adapted to bounded waiting.
Latest complete validation PASS:84 strict C++20/Wall/extra/Werror tests,
13 Python tests plus SQL fixture, seven Lua fixtures/138 checks; artifact
`/tmp/wow-validation-s6f297pe/results.json` (final rerun after diagnostics).
BUILD PASS: full validation and separate `cmake --build build` after fresh
manager/player-identity guards. DIFF CHECK PASS: `git diff --check -- .`.
Read-only binary audit repeated PASS against the hash above.

RUNTIME PENDING: no running WoW. Need at least one complete natural death ->
Ghost -> server anchor -> validated route/physical progress -> reclaim -> two
alive probes -> death ownership release -> Grind resume, no manual intervention.
Prefer two cycles. AFK normal Grinding/Ghost qualification/prevention RUNTIME
PASS from existing log; new long-route AFK regression gate still needs runtime.
Do NOT intentionally kill, relax unsafe terrain, inflate budgets or start later
V6 phases in this checkpoint. Older missing-anchor log not retained locally;
its exact source path is verified, historical runtime claim remains labelled.

## P0.0.11 checkpoint (historical AFK baseline, 2026-10-07)

AFK edge-case audit: SOURCE VERIFIED. No safe deterministic active mixed-state
reconciliation has been proven; retain KNOWN RESIDUAL EDGE CASE, fail-closed
by design, NOT fixed. Normal Grinding and Ghost qualification are RUNTIME PASS.
Latest directly inspected session `1360.134358440699456960.106923713.436`:
Ghost/RoutingToCorpse pulse at about 193743-ms input age, flags=0x10000000,
mask=0x1000013f, unsupported=0, clock 107704936 -> 107898701, unchanged
life/scene/recovery state, pass then qualified=yes (lines25393-25406).
Later TWO Ghost prevention cycles passed at ages240207/240016 ms,
RoutingToCorpse then Failed recovery, clocks107898701 ->108138936 ->108378979,
paired release and unchanged life/scene/recovery with clear AFK flags
(lines27525-27555,28000-28024). Ghost production prevention RUNTIME PASS;
DeathRecovery strategic_route_failed / path_validation_failed at line27667
is a separate defect. Later world/process loss has unknown cause, not AFK proof.
Alive combat defer/resume prevention confirmed at 293685 and 253868 ms
(lines5731-5783,12261-12313), both before threshold. Earlier four normal
cycles/debt25/51/75/101/planning passes remain valid. No natural mixed state
in the newest capture; no running WoW process available for a new runtime test.

Binary hash and full evidence are in `docs/AFK_5875_AUDIT.md` P0.0.11.
Five direct local-AFK writes in four functions: startup/reset0x4989C0,
mark0x5EB740, clear0x5EB830, server mirror0x5EE990. Native clear's ONE
argument only bypasses the CVar when nonzero; local-clear early-return precedes
the argument check. Active branch always sends the normal EMPTY AFK message,
which local server source proves is a toggle. Client-only could toggle server
ON; server-only returns. Incoming object update0xA9 ->0x4651A0 -> values
0x465330 -> changed-field callback0x465570 ->0x5E2850 ->0x5EE990 copies flags
&2 only on relevant changes ((old XOR current)&0xE), not every server update.
Startup mirror also processes queued chat/animation and is not an approved
standalone utility. No measured natural convergence latency, no active new
call, synthetic old flags, CVar/flag writes or fabricated packets.

Files: new AfkAgreementPolicy.h and afk_agreement_policy_test.cpp;
AfkProductionPolicy.h (typed, side-specific existing bounded failure),
SharedAfkController.h (read-only agreement snapshot and transition/clock-change
reconciliation diagnostic), tools/afk_client_audit.py (repeatable writer/caller/
incoming-update signatures), both continuity docs. BothClear prevention,
BothActive qualified composite, Ghost, debt, safe gaps and all budgets unchanged.
Persistent mixed states fault within the existing total3000-ms verification
budget with client_only_afk_no_safe_reconciliation or
server_only_afk_no_safe_reconciliation. That budget is NOT propagation evidence.
Conditional authoritative convergence still works; no native toggle while mixed.

TEST PASS: full `python3 tools/validate.py --jobs 4`, 83 strict C++ tests,
13 Python tests, QuestDB SQL self-test, seven Lua fixtures /138 checks.
Artifact `/tmp/wow-validation-je4i58rl/results.json`; repeatable binary audit PASS.
BUILD PASS (validator and explicit `cmake --build build`); DIFF CHECK PASS.
Intermediate lexical quiescence-sentinel collision in read-only probe fixed by
equivalent condition ordering; the existing test/qualification behavior is
unchanged. Final complete rerun has zero failures.
New mixed diagnostics RUNTIME PENDING. Dead qualification and native clear
while dead/ghost remain unqualified; no blanket dead-state runtime claim.
This closes the requested implementation checkpoint with a documented residual
edge case. Next bounded phase is DeathRecovery missing_corpse_anchor /
strategic_route_failed; do NOT start it inside this AFK checkpoint.

## P0.0.10 checkpoint (historical; Ghost runtime now passed)

AFK remains highest priority. SOURCE + RUNTIME VERIFIED: newest session
`1892.134358429566633600.105563093.1520` shows early Dead qualification armed
at input age 75103 and Ghost at 76138, correctly blocked in WaitingForGhost.
RoutingToCorpse at age 77179 rejected exactly movementFlags=0x10000000,
allowedMask=0x0000013f, unsupportedBits=0x10000000 (log line 6975). Read-only
local VMaNGOS re-audit confirms this is WATERWALKING, enabled by ApplyGhostForm.
The prior raw-bit evidence gap is closed. This log also confirms another
normal alive Grinding pulse at age 240008 with clock/scene/UI/clear-flag proof.

SOURCE VERIFIED: shared life-aware movement policy admits that one bit only
for Ghost + guarded input-only dead/ghost pulse, making the ordinary pulse
mask 0x1000013f. Alive/Dead/default masks and stationary qualify mode are
unchanged. All other unsupported modes still block. Fresh native life at the
monitor/command guard selects the mask. MOVEMENT ELIGIBLE is logged only for
issued Ghost water-walk pulses; exact rejection telemetry remains. Same-life,
paired-release, unbound-key, scene/UI, recovery-state and native-clock checks
remain mandatory. Early one-shot qualification and failure latching persist.

Normal Grinding/debt/planning coexistence RUNTIME PASS remains established.
New Ghost water-walk pulse and Dead/Ghost production: RUNTIME PENDING until
a natural safe gap produces a verified pulse and qualified=yes before AFK.
Mixed-state reconciliation remains OPEN. No native-clear, navigation-budget,
death-routing/anchor/reclaim or AFK 240/270/300-second policy change. No later
roadmap work. No running WoW process available for the new runtime gate.
Files: AfkQualificationHold.h, AfkClient5875.h, SharedAfkController.h,
afk_ghost_water_walk_test.cpp and both AFK continuity docs. Validation below
applies to the existing dirty worktree, not a clean remote checkout.

TEST PASS: `python3 tools/validate.py --jobs 4`, 82 C++ tests under
`-std=c++20 -Wall -Wextra -Werror`, 13 QuestDB/Python tests plus SQL self-test,
all seven Lua fixtures / 138 checks. BUILD PASS (full validator and separate
`cmake --build build`); DIFF CHECK
PASS. Artifact: `/tmp/wow-validation-h7wgv7he/results.json`. New focused test
first failed compilation before the life-aware API existed, then passed.
Existing early qualification, recovery-debt, mixed-state, native-clear, death
and navigation regressions pass. These do not qualify the new Ghost pulse.

## P0.0.9 checkpoint (historical; raw-bit gap now closed)

AFK remains first. Newest inspected log: session
`1676.134358382414341480.100842662.1712`. Normal Grinding AFK prevention,
recovery-debt decoupling and navigation-planning coexistence are RUNTIME PASS:
four confirmed cycles with debt 25/51/75/101, the latter two with planning=yes.
Qualification remains user-reported RUNTIME PASS. Each production pulse has
paired release, unchanged scene/UI, fresh input advancement and clear AFK flags.

Natural death during fifth-cycle combat deferral exposed the next gap: death
at input age 280643, corpse-routing command gap at 282718 but unspecified
movement bits blocked F12, both flags active at 300229. Dead/ghost gate is
RUNTIME FAIL for that capture; new code is RUNTIME PENDING. The prior harness
only scheduled a first life-state pulse when the normal AFK timer was due.

SOURCE VERIFIED change: natural unqualified Dead/Ghost states now request
early one-shot qualification through SharedAfkController/AfkProductionPolicy.
All existing command/native/UI guards apply; requests wait through command
windows and use the first eligible update. The timer's due bit is not invented;
the pulse's verified native clock governs subsequent scheduling. Dead/Ghost
remain separately session-qualified, pending actions cannot repeat, and failures
latch. Later clock proof now also requires the same issued life state.

Raw movement diagnostics record flags/mask/unsupported bits. Allowed masks
are unchanged. Local server ApplyGhostForm sets water-walking (0x10000000),
but the newest capture lacks the client word; do not infer it. Next natural
run must capture exact MOVEMENT BLOCK evidence if still rejected. No running
WoW process was available for this read. Full audit: AFK_5875_AUDIT.md P0.0.9.

Changed only AFK policy/adapter/controller/tests and continuity docs. Death
and navigation behavior/budgets are unchanged. Native clear during death and
mixed-state reconciliation remain unqualified. No swimming/later V6 work.
TEST PASS: `python3 tools/validate.py --jobs 4`, 81 strict C++20/Wall/extra/Werror
tests, 13 Python tests plus QuestDB fixture, all seven Lua fixtures / 138 checks.
BUILD PASS: validation build and separate `cmake --build build`.
DIFF CHECK PASS. Artifact: `/tmp/wow-validation-u3bdm23x/results.json`.
Focused regression failed before the new qualification request/verification
API existed, then passed. Read-only 5875 binary signature audit also passed.
These validate the dirty worktree; new dead/ghost behavior remains runtime pending.

## P0.0.8 checkpoint (historical source fix; runtime now passed)

SOURCE VERIFIED: before P0.0.8, `WorldMonitor` collapsed
`runtimeRobustness.RecoveriesWithoutProgress()>0` into `afkSafety.fault`.
The newest normal Grinding log shows due at 240595 ms, overdue at 270183 ms,
threshold crossing at 300135 ms and client AFK, all blocked by that generic
fault while Combat=`AcquiringTarget`, Grind=`Roaming`, DeathRecovery/Vendor=`Idle`.
Periodic `runtimeRecoveries=22` is cumulative events, not the exact debt;
the source predicate plus non-Failed controller states attributes the gate.

Changed: `src/Bot/AfkRuntimeFaultPolicy.h`, `AfkProtectionPolicy.h`,
`AfkProductionPolicy.h`, a small AFK-only hunk in dirty `WorldMonitor.h`,
and `tests/afk_recovery_debt_safety_test.cpp`. Historical tactical recovery
debt remains visible but no longer implies an input/terminal fault. Terminal
Combat/Grind failures retain hard gates and typed reasons; all other input,
scene, transaction, combat, death, water, and mixed-state guards remain.
`AFK SAFETY BLOCK` / `ELIGIBLE` are emitted on due-decision changes, not each tick.

Runtime status: **RUNTIME PASS** in four later normal Grinding cycles, debt
25/51/75/101, planning=yes in the last two. The prior threshold-crossing run
remains historical RUNTIME FAIL evidence for the old coupling.
Checkpoint validation: TEST PASS (`python3 tools/validate.py --jobs 4`, 80
strict C++ tests, all QuestDB/Python and Lua fixtures); BUILD PASS (validation
build and separate `cmake --build build`); DIFF CHECK PASS. No WoW process was
running, so these do not upgrade runtime status. Artifact:
`/tmp/wow-validation-_mo8ryla/results.json`.
The next gate is early natural Dead/Ghost qualification and exact rejected
movement bits, plus the separate mixed-state blocker. Do not begin swimming.

# Current architecture

- One `SharedAfkController` in WorldMonitor observes client/server AFK and the
  native input clock for both workloads. P0.0.6 shared production scheduling
  permits qualified paired input in known benign work as well as safe idle;
  hard safety gates and per-command proof remain. The older Grinding liveness mechanism
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

- Latest352.134358492686284770.111867086.624 has two fresh-alive reclaim
  confirmations and death exits (20440/20468,23844/23872), followed by Grind
  resume. User confirms natural zero-intervention recovery. DeathRecovery
  RUNTIME PASS baseline; these are BEFORE the combat checkpoint.
- AFK qualification: RUNTIME PASS per user's P0.0.6 report (full capture not
  available in latest log). Normal Grinding prevention: RUNTIME PASS in four
  newest cycles, input ages 240131/240217/240142/240245 ms, unchanged scene/UI,
  paired release, advanced clock, clear flags. Debt 25/51/75/101 did not block;
  Roaming/AcquiringTarget planning coexisted in cycles 3/4. This does not qualify
  Dead or mixed-state recovery. Earlier long-run failures remain history.
- P0.0.10 Ghost harmlessness/early qualification: RUNTIME PASS, directly
  inspected session `1360.134358440699456960.106923713.436`, lines25393-25406,
  flags10000000/mask1000013f, paired input, fresh native-clock advancement,
  same-life/scene/recovery proof and qualified=yes in RoutingToCorpse before
  threshold. Two later Ghost prevention pulses pass at ages240207/240016 in
  RoutingToCorpse and Failed recovery, same-life/scene/recovery proof and clear
  AFK flags (lines27525-27555,28000-28024). Ghost production RUNTIME PASS;
  corpse recovery was NOT passed in that older AFK capture; it now has the
  separate newest DeathRecovery PASS immediately above.
- Combat defer -> first-safe-gap resume: RUNTIME PASS in that session, alive
  confirmed pulses at input ages293685/253868, before threshold300000.

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

## P0.2 same-target combat liveness

SOURCE VERIFIED / TEST PASS / BUILD PASS / DIFF CHECK PASS. Actual signature-
validated UI selection replaces the server-victim-as-selection predicate.
Bounded typed repair/progress policy and command-time guarded Lua recovery
covered by new strict regression and real-script fixture; full85 C++ suite and
isolated intended-checkpoint build passed. See current checkpoint and combat
audit for exact evidence/limits. RUNTIME PENDING for new recovery behavior.

## P0.0.3 pre-baseline input quiescence

SOURCE VERIFIED: newest `build/wow-internal.log` lines 145-152 prove hold
acquisition, inputClock 57169769 -> first Update 57169793 (24 ms), immediate
Baseline and subsequent `unattributed_input_during_qualification` abort. That
24-ms change precedes the first Update's baseline and was tolerated; the actual
next changed clock triggering the guard was not logged. Startup/GUI residual
input is plausible, not established. The defect is the absence of a stable
input-clock boundary before strict attribution starts. Later normal Grinding
observed AFK at age 300158 ms with continuousSafeIdle=no; not qualification PASS.

Changes: typed AwaitingQuiescence in `AfkQualificationPolicy`, 1500-ms quiet
interval and fixed 10000-ms startup deadline. Safe clock changes reset only the
quiet interval; no input is issued/credited. Timeout => quiescence_not_reached.
After quiet completion capture baseline input/AFK/scene, then retain clear or
already-AFK entry, bounded asynchronous clearing, and both prevention windows.
Input attribution is enforced in the policy after that boundary (including
subsequent synchronization), replacing the premature controller-level abort.
Candidate dispatch still uses independent before/after evidence. The adapter's
fresh pre-command input cannot silently introduce unrelated clock progress.
Unsafe/unknown state releases the existing shared hold; no workload/input-driver,
production permission, navigation, recovery budget or later roadmap changes.

Tests: new quiescence policy regression (failed compilation before new API),
existing qualification tests rerun after quiescence. TEST PASS: full
`python3 tools/validate.py --jobs 4`, 76 strict C++20/Wall/extra/Werror tests,
13 Python tests plus QuestDB SQL fixture, six Lua fixtures / 113 checks.
BUILD PASS; DIFF CHECK PASS. Artifact:
`/tmp/wow-validation-xu73uobf/results.json`. Ninja emitted its existing
`premature end of file; recovering` warning and successfully rebuilt the DLL.
Files: `AfkQualificationPolicy.h`, `SharedAfkController.h`,
`afk_quiescence_policy_test.cpp`, `afk_qualification_policy_test.cpp`, both AFK
continuity documents. RUNTIME PENDING: fresh qualify-mode client on safe land,
hold -> quiet start/reset/complete -> baseline -> F12 delivery -> authoritative
clear -> two prevention windows. F12 and both windows remain runtime-unproven.

## P0.0.2 qualification from an already-AFK character

SOURCE VERIFIED: newest `build/wow-internal.log` aborts at
`initial_afk_state_not_clear`; the following signature-verified native read has
both flags active, clientNow=54969271, lastInput=54968917 (354 ms age). The
explicit startup predicate in `SharedAfkController::AdvanceQualificationHold`
caused the abort. This proves neither failure nor success of F12.

`AfkQualificationPolicy.h` now separates startup synchronization, natural
baseline, candidate delivery, asynchronous two-flag clear and two prevention
windows. Already-active AFK can acquire the unchanged shared qualification
hold. Recent input gets <=1000 ms of observation, not sleep; persistent AFK
tests the candidate, natural clear takes the baseline path. Delivery and clear
share the existing 3000-ms verification deadline. Qualification never uses the
production AFK-chat toggle. Initial clear success resets the baseline with zero
windows; both later windows are mandatory. Unsafe/unknown/side-effect evidence
and timeouts fail closed. Scene/UI guards remain active throughout verification;
position/facing permit only small documented numerical tolerances.

Files: new qualification policy/test; `SharedAfkController.h`,
`AfkQualificationHold.h`, this state file and `AFK_5875_AUDIT.md`. No navigation,
workload, input driver or production active-owner permissions changed. Runtime
snapshot/telemetry distinguish delivery, clear and prevention. The startup
regression failed against the old controller before integration.

TEST PASS: `python3 tools/validate.py --jobs 4`: 75 strict C++ executables,
13 Python tests plus the QuestDB SQL fixture, six Lua fixtures (113 checks).
BUILD PASS and DIFF CHECK PASS. Artifacts:
`/tmp/wow-validation-c5nhun5h/results.json` (final repeat after adding the
pre-dispatch natural-clear race regression). Ninja reported its existing
`premature end of file; recovering` warning and successfully rebuilt the DLL.
RUNTIME PENDING: user must launch the rebuilt fresh client, log in on safe land,
Start Bot with qualify mode and leave input alone. Already-AFK entry is valid.
Need native clock advance, both flags clearing within the bounded deadline,
then prevention windows 1 and 2. Do not label F12 runtime verified before that
evidence. Production active-workload protection remains a later AFK gate, not
silently enabled here. AFK remains first priority; no swimming work.

## P0.0.1 qualification ownership and native AFK flag correction

Latest live evidence: `build/wow-internal.log`, session starting 19:41:43 UTC
2026-10-06. Qualification observed clear flags/input clock, then logged the
generic unsafe abort BEFORE Grinding initialized. Immediately afterward an
attacker targeting the player was selected at 2.1 yards. The existing `continue`
already suppressed workload dispatch; the log does NOT prove Grinding stole
ownership. Exact old abort predicate is unknown. No F12 candidate was issued.

SOURCE VERIFIED separate blocker: native AFK variable B6E5CC has values 0/1
from mark/clear and 0/2 from server synchronization at 5EE9EF..5EE9F2. The
adapter wrongly rejected >1. It now accepts only source-proven 0/1/2, with a
signature check for the server writer. This explains a source path to the
observed known=no after the legacy raw player flag became 2; the old log did
not expose the native raw value. No unsupported encodings are accepted.

Changes: typed AfkQualificationHold Requested/Held/Aborted/Complete, exact
ownership/native/UI abort reasons, read-only UI guard before hold acquisition
and every second during hold, no workload dispatch while held, no silent
qualification restart after abort/world loss. Legacy telemetry is now
AFK FLAG CANDIDATE SET/UNSET, still signalVerified=no. Candidate tests verify
input-clock advancement AND unchanged position/facing/target/movement plus UI
guards. Added explicit start/hold/baseline/candidate/window/complete events.

Production safety classification distinguishes benign work, but does not yet
permit F12 during navigation/acquisition. Requires live harmless-input proof
and a subsequent active-workload dispatch implementation/qualification. Bot
activity still never resets AFK scheduling. This unresolved requirement is
not disguised by the controlled-idle harness.

New regression `afk_qualification_hold_test.cpp` failed before its policy
existed and now passes: both workload gates, abort/release, input verification,
native flag encodings, scene integrity, monitor gate ordering and GUI launch
inheritance. Existing AFK and navigation tests remain protected. No navigation,
recovery budget, 14O.1 or death/pull policy changes in this phase.

Validation: SOURCE VERIFIED / TEST PASS / BUILD PASS / DIFF CHECK PASS.
Final full run: 74 strict C++20/Wall/extra/Werror executables, 13 QuestDB Python
tests plus SQL/catalogue fixture, six Lua fixtures / 113 checks. Artifact:
`/tmp/wow-validation-g9s9c6b2/results.json`. Explicit `cmake --build build` and
`git diff --check -- .` repeated after full validation. These validate the dirty
worktree, not a clean remote checkout and not WoW behavior.

RUNTIME PENDING: both prevention windows and harmless F12. No WoW process is
currently running. Run `env WOW_INTERNAL_AFK_MODE=qualify wine ./build/wow_gui.exe`,
then GUI Start WoW -> login on safe land -> Start Bot; no manual input during
qualification. WoW inherits the GUI environment only when newly launched by
it. See `docs/AFK_5875_AUDIT.md`. If an attacker arrives, abort is required:
choose an actually safe location rather than weakening survival gates.

Earlier P0.0 AFK checkpoint, 2026-10-06:

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

- P0.2 combat watchdog: normal fights and natural desync recovery must qualify
  fresh UI evidence, same-GUID repair, damage verification, AFK and death
  preemption. Baseline DeathRecovery now passed twice; post-combat-patch death
  regression remains pending. No live WoW process available during this phase.

- **AFK residual gates:** Ghost early qualification/harmlessness and two later
  prevention cycles passed; separately qualify Dead if naturally available.
  Never intentionally kill the player. New P0.0.11 mixed
  observation/diagnostics are RUNTIME PENDING (no mixed state in newest log).
  Persistent mixed state is KNOWN RESIDUAL EDGE CASE, bounded fail-closed by
  design; no safe active reconciliation proven. Dead/ghost native clear is
  NOT enabled. Normal alive qualification/Grinding/debt/planning gates passed.

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
- Combat attack continuity and acquisition liveness need live diagnosis.
  AFK normal Grinding and Ghost qualification have separate live proof above.
  Do not infer combat success from arrival events.
- Vendor/repair, equipment and trainer end-to-end qualification remains distinct
  from existing frame/spellbook/equip verification mechanisms and unit tests.
- Water-state/breath/surfacing/shore systems are not implemented by this work.
- Class profiles, automatic capability discovery, Hunter/racials, talent builds,
  talent library, leveling profiles and operational GUI remain queued.
- All unattended 5/15/30/60 minute and fresh Orc->10 gates remain RUNTIME PENDING.

# Known blockers

0. AFK prevention/Ghost early qualification now passed. Mixed-state recovery
   remains an explicit bounded fail-closed residual edge case; no safe active
   command proven. Separate Dead qualification remains pending; Ghost passed.
   Earlier death at 280643 ms and AFK at 300229 remain historical failures.
   Previously inspected DeathRecovery Failed is `strategic_route_failed` with
   path_validation_failed (line22000); older missing_corpse_anchor is separate.
   Those death-autonomy defects were fixed in the previous checkpoint, with
   two newest natural death-to-alive exits now verified. New combat/watchdog
   runtime evidence is the immediate gate; no death rewrite in P0.2.
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
7. AFK: qualification passed per user; four alive Grinding cycles now passed,
   including debt and planning. Dead/ghost remains unqualified after the latest
   threshold crossing. Mixed-state native clear
   is NOT safe: 5EB830 returns for client-clear; with client-active/server-clear
   its empty message toggles server AFK on in local VMaNGOS. Need legitimate
   reconciliation evidence, not removal of that guard. No WoW process running
   during this audit. See P0.0.6 section in AFK_5875_AUDIT.md.
   GUI status transport and a broader water-state model remain unimplemented.
8. Much prior working project code/data/tests is still dirty/untracked. This
   checkpoint publishes only reviewed audit/tooling files, not the complete
   current runtime. A clean remote checkout is NOT this validated worktree.

# Current phase

**P0.3 Navigation reliability:** advancing initialization is separated from
physical movement in the first owner watchdog; full-base posture read corrected;
projection/hazard failures attributed. SOURCE/TEST/BUILD/DIFF PASS; new-fix
RUNTIME PENDING. Latest20-minute baseline verifies Combat P0.2 natural latch
repair/damage and AFK prevention. Prior DeathRecovery/Ghost gates remain PASS;
mixed-state AFK residual stays bounded fail-closed. Preserve2000/4/2 and terrain
safety; no swimming, GUI/profile/talent/trainer expansion in this checkpoint.

## P0.0.7 bounded death/ghost checkpoint

AfkDeadGhostPolicy maintains separate dead and ghost session qualifications,
one pending prevention pulse, existing 3000-ms verification and session-latched
failure. Shared controller/adapter retain binding, held-input, frame, land,
fault and transaction gates; production-only death exception requires an
explicit command gap after synchronous recovery update. Release-spirit and
reclaim confirmation states remain blocked. Failed recovery alone no longer
blocks the prevention candidate. Native and Lua death evidence must agree;
health=1 alone is unknown. No AFK clear permission is added for dead/ghost.
Scene and recovery-state proof brackets paired input; later ordinary ghost
movement cannot counterfeit native input-clock verification.

Files: AfkDeadGhostPolicy, SharedAfkController, AfkClient5875, AfkSafeInputScript,
isolated WorldMonitor AFK block, C++/Lua regression, validation fixture mapping,
both continuity documents. Qualify mode, production scheduler, navigation and
death-controller code/budgets are unchanged. Full audit/runbook:
`docs/AFK_5875_AUDIT.md`, P0.0.7. Runtime remains pending, AFK remains first.

## P0.0.6 bounded production checkpoint

AfkProductionPolicy adds Recent/Due/Deferred/VerifyingInput/RecoveringAfk/
VerifyingClear/Faulted, source-relative 80/90/100% urgency, sticky due cleared
only by native input evidence, and one native clear per bounded recovery.
Either active flag enters recovery, but mixed flags wait for authoritative
convergence and fault within the existing 3000 ms if persistent; no unsafe
toggle. This intentionally does NOT claim full P0.0.6 mixed-state recovery.
Implementation qualification is an explicit reviewed release fact; session
delivery and each action still start unverified. No volatile prior-process
qualification flag is required for classified benign work.

Adapter preserves native/UI/held-input guards; ordinary known land movement
is permitted only with benign-work evidence. Paired-message synchronous scene
proof prevents confusing normal between-tick navigation with key side effects.
Monitor changes only AFK safety classification (first aid/vendor/low health).
Questing shares scheduler but active unclassified Questing owners stay blocked.
No combat, navigation, recovery limits, or later V6 behavior changed.
Files: new AfkProductionPolicy/test; AfkClient5875, AfkQualificationHold,
SharedAfkController, isolated WorldMonitor AFK hunk, both continuity documents.
Runtime gate: normal Grinding >=20 minutes; exact details and mixed-state
source contradiction are in AFK_5875_AUDIT.md. AFK remains highest priority.

## P0.0.4 source/test checkpoint

Latest `build/wow-internal.log`: baselineClock=60801599, stableMs=1541,
natural AFK elapsed=300241, F12 press/release, clockAfter=61101864, unchanged
scene, both flags still active at 3245 ms. Delivery/native clock RUNTIME PASS;
F12-alone clear RUNTIME FAIL; prevention RUNTIME PENDING (not window-1 failure).

SOURCE VERIFIED from local 5875 binary (hash in AFK_5875_AUDIT.md): input
dispatcher 0x765F34 updates timestamp before consumers; clear is 0x5EB830.
Its force=0 branch reads autoClearAFK pointer 0xC4D68C + integer field 0x28,
then updates local state AND sends empty AFK message opcode0x95/type0x14 via
0x5AB630. Movement and normal chat call it separately; an unbound key is not
the same as these semantic actions. Default CVar=1 is source-backed; actual
live value is unknown, not inferred from default or Config.wtf absence.
No evidence proves a missing Wine transport; no OS-global input substitute.

Files: AfkClient5875 adds read-only CVar/signature guards, fresh pre-pulse
clock after UI/window/scene guards, native non-forced clear adapter. SharedAfkController
and AfkQualificationPolicy add ONE qualification-only composite candidate:
paired F12 -> delivery/scene proof -> once-only native clear when both flags
active and CVar enabled -> both flags clear -> two prevention intervals.
The original total 3000-ms deadline is unchanged. Unknown/disabled setting
fails closed; bot never writes clocks/flags/CVar; no forced Lua toggle in
qualification. Production permissions and Questing/Grinding wiring unchanged.
Initial failure now logs a prerequisite failure, NOT a prevention-window failure.
AfkProtectionPolicy only adds typed candidate action/setting identifiers.
tools/afk_client_audit.py validates call targets, CVar/default and clear packet
signatures; tests/afk_native_clear_test.cpp covers the rejected old candidate,
unknown/disabled CVar, mixed flags, bounded timeout, no retry/false window,
scene safety, fresh timestamp ordering, both prevention intervals and no writes.

Validation results and scoped Git checkpoint are recorded in Git state below.
New candidate RUNTIME PENDING; busy-workload protection still blocked pending
qualification and explicit safety review. AFK remains highest priority.

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

Current first gate: P0.2.1 normal unattended Grinding>=20 minutes; preserve FULL
combat episode and require damage after repair, verified safe optional target
abandonment, or evidence explaining an unavoidable active-hostile system failure.
Do not deliberately provoke a stall/death. P0.3 key fixes passed~12.5min;
the full20–30min navigation gate was interrupted by combat and remains pending.
The same longer run can complete that regression gate: preserve FULL
log and run `python3 tools/navigation_log_audit.py CAPTURE`. Compare movement,
runtime recovery and escalation rates to0.800/2.100/1.500 per minute. No progressing
loader falsely killed at48 ticks, impossible posture51, sustained stationary
stall or same-geometry recovery loop; actual kills/XP/movement must continue.
Do not manufacture bad terrain/death/stall. AFK/combat regressions remain live
requirements; naturally occurring death must preserve corpse/reclaim/resume.
P0.2 natural desync/damage baseline passed; prior death-to-alive gate passed twice.

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

- P0.3 last validated code checkpoint:
  `65cb275b06829fe030d8e25b34d2eba65d312531`
  (`navigation: preserve progressing loaders and correct posture evidence`).
  Base `6cbf0cdec1bf3342125cc91b205459dab6ce6945`; branch
  `codex/wow-internal-continuation`, verified origin
  `git@github.com:darkraven92/MyProjects.git`. Full worktree87-test and isolated
  intended-tree38-test validation/build PASS;15 intended files, including only
  five isolated owner hunks. Push PASS; independent remote branch lookup
  returned the identical full code hash. Unrelated dirty work remains unchanged.
  This documentation-only follow-up records actual code/remote identity; resolve
  its own HEAD through `git log -1 --format='%H %s' -- docs/CODEX_PROJECT_STATE.md`
  and compare current local/remote HEAD. No complete dirty-worktree publication
  or new navigation runtime PASS is implied.

- P0.2 validated combat code checkpoint:
  `c88f673ee01102251f4a6860acf04ce4de5a6129`
  (`combat: distinguish UI selection and bound melee stall recovery`).
  Branch `codex/wow-internal-continuation`; verified origin
  `git@github.com:darkraven92/MyProjects.git`. Push PASS; independent remote
  HEAD verification returned that identical full code hash. Eleven intended
  files/hunks only, including the two mechanical Grind enum/name dependency
  hunks; unrelated Combat/Grind/WorldMonitor/GUI/quest changes remain unstaged.
  Full85-test strict validation/build/diff PASS plus isolated intended-checkpoint
  build PASS. New combat recovery RUNTIME PENDING. This documentation-only
  follow-up records actual published code identity; resolve its own HEAD via
  `git log -1 --format='%H %s' -- docs/CODEX_PROJECT_STATE.md` and independently
  compare current local/remote HEAD. Last validated code is the hash above,
  not a claim that the complete dirty runtime is published.

- P0.1 validated DeathRecovery code checkpoint:
  `de57521813256642ce02015f005bd1ba8836fa83`
  (`death: preserve route fallback and acquire server corpse anchors`).
  Base `a9c8c7cac28a12cc549693cac56b79cea04ede13`; branch
  `codex/wow-internal-continuation`, verified origin
  `git@github.com:darkraven92/MyProjects.git`. Push passed; independent
  `git ls-remote origin refs/heads/codex/wow-internal-continuation` returned
  the identical code HEAD. Sixteen reviewed death/shared-diagnostic/tool/test/
  documentation files only. No broad WorldMonitor or unrelated dirty gameplay
  staged; that work remains local and unchanged. Final full validation/build/
  diff passed in the dirty worktree, not a clean remote runtime. This docs-only
  follow-up records the verified code HEAD; its own HEAD is available in git log.
  DeathRecovery RUNTIME PENDING; next action is the natural-death gate above.

- P0.0.11 validated code checkpoint:
  `5c84d0fe8214baff4b9157d0e6d7ebd011f4495a`
  (`afk: audit mixed-state synchronization and retain bounded safe failure`).
  Push passed; independent `git ls-remote origin
  refs/heads/codex/wow-internal-continuation` matched that local code HEAD.
  Branch `codex/wow-internal-continuation`, remote
  `origin=git@github.com:darkraven92/MyProjects.git` verified before edits.
  Seven intended AFK policy/controller/test/tool/documentation files only;
  no unrelated WorldMonitor, DeathRecovery, navigation or data changes staged.
  Final validation83 strict C++/Python/SQL/all Lua, explicit build and diff check
  passed in the dirty worktree. Mixed path remains runtime pending/residual;
  normal alive and Ghost prevention passed from captures. This docs-only
  follow-up records the verified code HEAD; its own HEAD is available in git log.
- P0.0.10 validated AFK code checkpoint:
  `edb5fe7a5fd9f783fc11106c992a87c3c89732f4`
  (`afk: allow source-verified ghost water-walk pulses`). Push passed;
  independent `git ls-remote` matched local HEAD before this documentation
  follow-up. Six intended AFK policy/adapter/controller/test/docs files only.
  Full validation (82 strict C++ tests, Python/SQL and all Lua), explicit build
  and diff check passed in the dirty worktree. Ghost pulse RUNTIME PENDING;
  source/runtime-correlated water-walk bit is verified. Alive production PASS
  and mixed-state/death-recovery blockers retained; unrelated work unpublished.
- P0.0.9 validated AFK code checkpoint:
  `0e53bba5647521d84ea16b762737f98c9b5950e7`
  (`afk: prequalify natural dead and ghost command gaps`). Push passed;
  independent `git ls-remote` matched local HEAD before this continuity follow-up.
  Nine AFK/documentation/test files only; no WorldMonitor or death/navigation
  source changes staged. TEST/BUILD/DIFF CHECK PASS in the existing dirty
  worktree. New early death/ghost qualification and exact movement-bit capture
  are RUNTIME PENDING; alive Grinding/debt/planning gates are RUNTIME PASS.
- P0.0.8 validated AFK code checkpoint:
  `423af3b3a908e9be45b50ad11aef06def88ada22`
  (`afk: separate recovery debt from input safety`). Pushed to
  `origin/codex/wow-internal-continuation`; independent `git ls-remote`
  matched local HEAD. Only six AFK/documentation/test files and the isolated
  `WorldMonitor.h` AFK hunk were staged. Existing unrelated dirty work remains
  local. Runtime was pending at that checkpoint; four later production cycles
  now give RUNTIME PASS for normal Grinding/debt/planning, not for death states.
- P0.0.7 base: `66de9bb614561941829776d02b1d9636fbf0ea31`.
  TEST PASS: 79 strict C++ tests, 13 Python tests plus QuestDB SQL fixture,
  seven Lua fixtures / 138 checks. BUILD PASS; DIFF CHECK PASS.
  Final artifact: `/tmp/wow-validation-loia88my/results.json`.
  Death/ghost qualification and normal two-cycle production are RUNTIME PENDING.
  Scoped code checkpoint: `148cd30620d27a11c6c8f7ea0249117736e5b646`
  (`afk: qualify dead and ghost prevention at safe command gaps`). Push passed;
  independent ls-remote matched local HEAD before this documentation follow-up.
  Only reviewed AFK files and the isolated WorldMonitor hunk were staged.
  Existing unrelated dirty work remains unpublished and unchanged. Resolve
  later documentation HEADs with the commands below.
- P0.0.6 base: `5f91e854cfbfa27923dd8ff88bbca77faccfaeae`.
  TEST PASS: 78 strict C++20/Wall/extra/Werror tests; 13 Python tests plus
  QuestDB SQL fixture; six Lua fixtures / 113 checks. BUILD PASS and
  DIFF CHECK PASS. Final artifact: `/tmp/wow-validation-v9eov92z/results.json`.
  New production behavior RUNTIME PENDING; mixed-state recovery remains an
  explicit protocol blocker, not claimed fixed. Validated scoped checkpoint:
  `9ed1929415c82a01864959cd2a31d08c6f8e9e7b`
  (`afk: retain deferred prevention across benign workload activity`). Pushed
  to origin; independent `git ls-remote` matched that local HEAD before this
  documentation follow-up. Resolve later HEADs using the commands below.
  WorldMonitor contains unrelated prior dirty work: ONLY the two AFK safety
  hunks were staged, not the whole file. Remaining user work is untouched and
  is not included or claimed published by this checkpoint.
- P0.0.4 base: `d012a470d39575ea9e70bb0cee53c41954ef67d0`.
  Current validation: TEST PASS, 77 strict C++ tests, 13 Python tests plus
  QuestDB SQL self-test, six Lua fixtures / 113 checks. Artifact:
  `/tmp/wow-validation-h993csap/results.json` (validation includes build/diff).
  BUILD PASS; DIFF CHECK PASS. Runtime composite/two windows remain pending.
  Validated scoped checkpoint: `61f18c358d8e97584fb198b85a935e26d56c1c64`
  (`afk: qualify native client-server clear after verified input`). Pushed to
  origin; independent `git ls-remote` matched that local HEAD before this
  documentation follow-up. Resolve later HEADs using the commands below.
  Remaining dirty project work is not included or claimed published.
- P0.0.3 base: `543447e8f0d2b60a4c2b786f5f57b15d093723b4`.
  Validated scoped checkpoint: `f04ac4c89d4189b23d0e43b4fa9f31b847a59bfa`
  (`afk: require bounded input quiescence before qualification`). Pushed to
  origin; independent `git ls-remote` matched local HEAD before this
  documentation follow-up. Unrelated dirty work stays unstaged; runtime pending.
- P0.0.2 base: `2eb8889199d6b2fefd2d9c02687e35d785e55120`.
  Validated scoped checkpoint: `a0a241503a9a25bdafe88dea1fe7b28ad86b8f2f`
  (`afk: qualify already-AFK entry with asynchronous clear verification`).
  Pushed to origin; independent `git ls-remote` returned the identical hash
  before this documentation follow-up. Remaining pre-existing dirty work is
  not included. No runtime success is implied; AFK remains first priority.
- P0.0.1 code checkpoint: `afc8cff640068e1f9ce10e787cfed4328d6d6487`
  (`afk: retain qualification hold and decode synchronized AFK state`). Pushed
  to origin; independent remote HEAD matched before this documentation follow-up.
  The remaining dirty work is preserved and not published by this checkpoint.
  F12 and both prevention windows remain RUNTIME PENDING.
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

1. Validate P0.2.1 deep-stall repair/terminal behavior in normal natural Grinding
   >=20 minutes with full capture. No unsafe induced stall. Actual target damage
   verifies refresh; same-target safe abandonment is NOT a kill. Active/unknown
   hostile evidence must never be silently discarded. Then finish the interrupted
   P0.3 normal Grinding20–30-minute regression gate;
   compare recovery rates and inspect progressing initialization/posture/query
   evidence before declaring reduced churn. P0.2 natural same-target latch
   repair and normal20-minute combat regression now PASS. Keep normal fights,
   AFK/death priority unchanged. Earlier DeathRecovery has two confirmed natural
   exits; latest20-minute session had no death, so new navigation regression
   requires a natural cycle if one occurs. Remaining acquisition idle_deadlock
   events and any actual CTM stalls need separate evidence, not broad changes.
   If terrain fails, inspect exact query/tier/geometry without weakening safety.
   AFK normal Grinding/debt/planning/combat gaps and Ghost qualification passed.
   Preserve the P0.0.11 mixed-state residual fail-closed design; capture its new
   diagnostics only if mixed state occurs naturally. No speculative toggles,
   CVar changes, fake flags or W loop. Later natural Dead gate is distinct;
   Ghost prevention passed even while Failed, not proof of corpse recovery.
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
