# P0.3 navigation reliability audit

Updated 2026-10-07. Current phase P0.3.2: session topology reuse. P0.3 and
P0.3.1 baseline RUNTIME PASS; new cache performance RUNTIME PENDING.
Historical checkpoint sections below retain their original evidence.

## P0.3.2 current baseline and cache

Latest full capture `/tmp/wow-p032-baseline.3WXFZs/wow-internal.log`,14,852lines,
29.6273min. Exact final counters movement0/runtime1/idleDeadlocks0/strategic1/
escalations0. This closes the P0.3.1 false-acquisition runtime gate. No watchdog
threshold/counter/outcome semantic change is made here.

Nine separate successful704-tile full-map loads each took123–129s; total
1118.984s. A tenth cancelled after652tiles/120125ms. This is duplicate topology
cost, not renewed movement-stall evidence. Every init freed its private mesh;
compatible same-map routes could not reuse previously loaded tiles.

New cache owns one current world/map topology with checked identity/generation;
route -> expanded -> full-map retains loaded tiles, queues only missing files.
Private queries/corridors, query-time hazards/filters, RAII temporary-mask
restoration, stale-generation rejection, map/world/session invalidation preserve
safety. Two cold tiles per update remains unchanged. No Roam-specific fallback
shortcut without endpoint-coverage proof and no GUI redesign.

Real Detour/Wine OFFLINE probe PASS:30 +60 +614 =704 actual cold additions.
Warm route2–3ms, expanded4ms, full27ms with ZERO disk reads/addTile calls;
existing refs stable and concurrent hazard masks restored. This is not a live
performance PASS. Complete audit, tests and >=20min runtime gate are in
`docs/NAVMESH_CACHE_AUDIT.md`; reproduce full-log metrics with
`python3 tools/navmesh_cache_audit.py /path/to/preserved-full.log`.

Unchanged:2000/4/2 budgets, terrain/hazard safety, progressing-initializer
liveness, acquisition ownership, strategic outcome policy, combat/AFK/death.
New source performance RUNTIME PENDING until warm topology is exercised in WoW.

## P0.3.1 authoritative full-log audit

Snapshot `/tmp/wow-p031-baseline.Dow16n/wow-internal.log` (temporary), session
`1808.134358630204354240.125757707.1448`,13122lines. Bot monotonic start125758217,
stop127036291:21.3012 minutes, normal user Stop (not combat-system failure).
Exact final counters: movementRecoveries0/runtimeRecoveries6/runtimeIdleDeadlocks5/
runtimeStrategic1/runtimeEscalations1. Rates:0/0.2817/0.0469 movement/runtime/
escalations per minute. Final kills2/loot2, session XP0 during gray migration;
five confirmed AFK protection inputs. No new complete natural death in this
capture; prior DeathRecovery qualification remains independently PASS.

| Terminal line / intent | Owner terminal | Deadlock tick / age | Outcome |
| --- | --- | --- | --- |
| 919 / 1 | persistent_hazard_rejected | 428 / 427 | false acquisition reset |
| 6012 / 7 | persistent_hazard_rejected | 1705 / 428 | false acquisition reset |
| 6751 / 8 | persistent_hazard_rejected | 2131 / 426 | false acquisition reset |
| 7540 / 9 | path_validation_failed | 2558 / 427 | false acquisition reset |
| 11864 / 14 | other_unknown (known edge exhaustion) | none | optional roam abandoned |
| 12573 / 15 | destination_projection_failed | 3723 / 425 | false acquisition reset |

Offline query-event distribution (not unique objectives): persistent hazard9,
destination projection5, unsafe_terrain_attempt_limit3, other_unknown1. Each of
the five false deadlocks occurred FIVE log lines after optional abandonment.
Initializer advanced704tiles and finished e.g.125706ms, while acquisition epoch
remained active through the whole Roaming ownership. No physical hard-stall
recovery occurred. Reproduce full read with `tools/navigation_log_audit.py`.

## Proven acquisition ownership defect and fix

RuntimeRobustnessSupervisor::Update performed owner tracking, then returned
early for bounded initialization BEFORE running acquisition-epoch reconciliation.
The previously active Grinding/AcquiringTarget epoch therefore survived Roaming
and accumulated wall/polling age. On loader completion, Roam failed correctly;
Grind returned to Grinding and looked like the SAME old acquisition epoch.
The old40-tick idle guard immediately fired with425–428ticks of borrowed debt.

Reconcile acquisition FIRST, every sample, including the first and pending init
samples. Resolve actual follower ownership from approach/roam OwnsMovement and
shared return navigator, plus initialization and Roaming/ApproachingTarget.
Vendor/recovery/first-aid/dialog/death are explicitly not acquisition. Ending
an epoch clears its acquisition debt only, not global recovery counters or
strategic outcome debt. Actual handoff starts a fresh40-tick epoch. UI target,
GUID/SetTarget churn and movement while still genuinely acquiring do not reset
that epoch. Combat handoff ends it. World loss/death retain existing reset/skip.
ACQUISITION LIVENESS is emitted on start/pause/resume/reset/deadlock only.

## Strategic work versus outcome liveness

At tick2131 a false idle recovery reset ownership. Tick2132 immediately issued
StrategicNoOutcome with outcome age1346ticks. Unlike acquisition debt, outcome
age is genuine: kills/XP/level/vendor completion did not advance during lengthy
level-band migration. Initialization cursor and physical displacement prove work,
not productivity. Keep the strategic1200-tick window and existing cooldowns.

A navigation-to-acquisition handoff gets the existing40-tick acquisition window
before another strategic teardown; a tactical recovery gets the existing40-tick
owner-recovery cooldown. Outcome age remains unchanged and strategic detection
still fires after that opportunity when no actual outcome occurs. This is event
ordering/repair opportunity, not fake progress or arbitrary timeout inflation.

## Typed repeated-geometry exhaustion

Intent14 local failure0x10000600000215->0x1000060000021D, fingerprint
16377577323959940644, candidate2095319634814919648, failureAnchorDistance9.998.
Portal staging reached a lateral target; earnedProgress=no was correctly retained.
Hysteresis matched the SAME directed edge and suppressed ordinary CTM. Surface
recovery/backtracking then unavailable; current code wrote OtherUnknown AFTER
SetState released intent. Corrected to BoundedLocalRecoveryUnavailable BEFORE
terminal publication: release/profile/owner all report
`bounded_local_recovery_unavailable`. Existing suppression/escalation still fails
closed. No geometric masks, hazard storage, route safety or budgets changed.

## P0.3.1 validation and next runtime gate

Focused failing-before-fix replay reproduced the exact epoch defect. Added
deterministic initialization/active-movement ownership, terminal fresh epoch,
real40-tick deadlock, no GUID-churn refund, death/vendor/recovery/dialog exclusions,
combat completion, strategic handoff/repair cooldown and unchanged outcome clock
tests. Attribution test requires typed reason BEFORE intent publication.
TEST PASS:89 strict C++20/Wall/extra/Werror tests, six navigation Python tests,
13 local QuestDB Python tests, SQL fixture and all eight registered Lua programs.
BUILD/DIFF CHECK PASS in full validation; artifacts
`/tmp/wow-validation-ynvhy5s5/results.json`. Separate final build/diff PASS.
Isolated staged tree PASS:40 published strict C++ tests, six navigation Python
tests, SQL fixture, three Lua fixture programs and full DLL/GUI/loader/testhost
build; `/tmp/wow-validation-5ewtyz9a/results.json`, tree
`/tmp/wow-p031-published.c5O3Ad`. QuestDB Python discovery explicitly reports no
published test files (the13 local tests belong to unrelated dirty work); SQL
remains mandatory. Only9 intended files/hunks staged. Runtime-after counters
UNKNOWN, not zero by inference.

Fresh NORMAL unattended Grind>=20minutes. Preserve full log. Expect no immediate
GLOBAL RECOVERY after optional NAV OBJECTIVE ABANDON, no false nav-owned idle
debt, still bounded actual idle acquisition, unchanged tactical/strategic outcome
visibility. No known repeated-edge OtherUnknown. Do not induce bad geometry,
combat faults or death. Combat P0.2/P0.2.1, AFK and any natural DeathRecovery must
remain operational. Source/test pass does not establish runtime counter reduction.

## Historical P0.3 baseline

## Baseline and reproducible offline audit

Authoritative full capture: logger session
`684.134358538029914000.116409170.1128`, original `build/wow-internal.log`,
3,184,196 bytes, preserved before modification in
`/tmp/wow-p03-baseline.yklizL/wow-internal.log` (temporary, not committed).
Bot start/stop monotonic times 116409685/117609877: 20.0032 minutes.
Final cumulative movementRecoveries=16 (0.800/min), runtimeRecoveries=42
(2.100/min), runtimeEscalations=30 (1.500/min). Escalations are a subset of
runtime recoveries, not 30 additional failures. Session XP gain=1563;
final periodic kill count=17; natural deaths=0. Productive does not mean
navigation healthy.

Read-only reproduction, using a preserved FULL log, not a filtered tail:

```bash
python3 tools/navigation_log_audit.py /tmp/wow-p03-baseline.yklizL/wow-internal.log
python3 tools/navigation_log_audit.py /tmp/wow-p03-baseline.yklizL/wow-internal.log --json
```

JSON includes line, owner, owner state, intent, destination, approximate last
world-snapshot position, loader work, cancellation and release evidence.
Fingerprints are UNKNOWN for pre-query cancellations. Repeated position is
diagnostic correlation, NOT proof of a repeated directed transition. No
coordinate is added to production. Different event units must not be summed.

| Event unit | Source-correlated class | Count | Result / owner |
| --- | --- | ---: | --- |
| Global watchdog recovery | unexpected_seated_idle | 24 | Combat + Grind reset; bad posture reader implicated |
| Global watchdog recovery | idle_deadlock | 18 | Acquisition resets; distinct class, no speculative fix |
| Owner movement hard stall | progressing initialization cancelled | 16 | Roaming, commands=0, intent released before movement |
| Initialization cancellation | same 16 owner hard stalls | 16 | Same episodes, not additional recoveries |
| Plan failure | persistent_hazard_rejected | 11 | Previously other_unknown; retain hazard exclusion |
| Plan failure | destination_projection_failed | 5 | Previously no_path; distinct from Detour search failure |
| Initialization cancellation | other owner release | 3 | Do not attribute to hard stall without evidence |
| Validation failure | unsafe_terrain_attempt_limit | 1 | Retain rejection |
| Validation failure | unsafe_terrain_no_alternative | 1 | Retain rejection |

### All 16 hard stalls

State=Roaming throughout; release commands=0/replans=0; expanded work advanced
before cancellation. Approximate player positions A=(-658.8647,-3644.9119,95.1144),
B=(-623.6596,-3736.1262,92.4810), C=(-579.7523,-3724.7053,85.9776).
These are diagnostic capture coordinates only. No corridor exists yet, so
same-geometry directed-edge identity remains UNKNOWN.

| Full-log line | Intent | Objective | Position | Expanded tiles at cancellation |
| ---: | ---: | --- | --- | --- |
| 986 | 1 | post-death danger escape | A | 70/79 |
| 5063 | 6 | roam sector15 | B | 70/73 |
| 5372 | 7 | roam sector5 | B | 66/82 |
| 5595 | 8 | roam sector14 | B | 70/73 |
| 5816 | 9 | roam sector16 | B | 66/82 |
| 6037 | 10 | roam sector13 | B | 70/73 |
| 6253 | 11 | roam sector23 | B | 70/73 |
| 6495 | 12 | roam sector24 | B | 66/82 |
| 6717 | 13 | roam sector12 | B | 66/82 |
| 6939 | 14 | roam sector22 | B | 70/73 |
| 7162 | 15 | roam sector17 | B | 60/92 |
| 7389 | 16 | roam sector6 | B | 66/82 |
| 7604 | 17 | roam sector31 | B | 66/82 |
| 7827 | 18 | roam sector15 | B | 70/73 |
| 13536 | 26 | roam sector32 | C | 60/92 |
| 13760 | 27 | roam sector16 | C | 60/92 |

## Proven root causes and smallest fixes

### Planning incorrectly classified as physical movement

WorldMonitor mapped Roaming/ApproachingTarget to AutonomyActivity::Movement
without checking incremental initialization. AutonomySupervisor's unchanged
0.90-yard physical threshold / 48-tick hard deadline killed progressing
expanded loads after about 10–12 seconds. ForceAutonomyMovementRecovery then
destroyed the follower, cooled down the sector and selected another objective.
The later RuntimeRobustness planning deferral could not prevent this earlier
watchdog. Shared route->expanded fallback itself retained intent; the outer
owner terminated it prematurely. No CTM was issued in these 16 episodes.

New read-only NavigationInitializationObservation carries actual provider
processed/total tiles, current tier and intent. The owner feeds this to the
first watchdog. Increasing processed count or a forward internal tier is
PLANNING liveness only. It is not displacement, offensive progress, follower
episode progress or a recovery-budget refund. Same intent keeps one absolute
planning window across tiers. Frozen work still fails at the existing 48-tick
window; regressed work fails explicitly; continuously advancing work remains
bounded by the EXISTING shared 240000-ms initialization ceiling. Unknown
pending evidence earns no exemption. Combat/death preemption remains intact.
After planning ends, execution starts its unchanged physical watchdog.

Sparse NAV STALL CLASSIFICATION records intent/tier/work and distinguishes
initialization_pending, initialization_frozen, initialization_budget_exhausted,
initialization_evidence_regressed from no_physical_or_hp_progress. No watchdog
is disabled and no threshold is increased. No RuntimeRobustness code/counter
reset was changed.

### Posture read used the wrong descriptor base

PlayerSnapshot.descriptors is the full OBJECT update-field base, from object+8.
The old posture read used byte offset0x210, copied from a client UNIT-relative
view. This actually reads UNIT_FIELD_NATIVEDISPLAYID, not stand state. Repeated
runtime standState=51 was not evidence of sitting, but was treated as nonzero
seated posture and caused SitOrStand recovery. A toggle could itself sit an
already-standing player. Twenty-four runtime recovery events use this class;
18 acquisition idle_deadlock events are recorded separately, not all claimed
fixed by posture correction.

Primary local evidence:

- VMaNGOS `src/game/Objects/UpdateFields_1_12_1.h`: full UNIT_FIELD_BYTES_1
  index0x08A, byte offset0x228; NATIVEDISPLAYID index0x084, byte offset0x210.
  Stand state is byte0; valid Vanilla stand states0..9.
- Build5875 binary SHA256
  `b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`:
  descriptor binder0x613980 writes the supplied full base at object+8 (0x613989).
  UNIT binders0x5F6EF0/0x5FAD10 add0x18 at0x5F6F05/0x5FAD25 and store the
  UNIT base at object+0x110 at0x5F6F08/0x5FAD28.
  SitOrStand handler0x48B941 reads object+0x110 then byte+0x210 at0x48B947,
  and dispatches state1 for zero/state0 for nonzero through0x5ED430.
  Thus full-base stand offset=0x18+0x210=0x228, independently matching server
  update fields. This is control-flow evidence, not inference from names.

PlayerPostureEvidencePolicy now reads0x228 and rejects unsupported bytes as
UNKNOWN. Existing posture action, game-thread ownership, watchdog cadence and
bounded stand recovery remain unchanged. No descriptor writes are introduced.

## Shared pipeline and failure taxonomy

| Stage / owner | Evidence and success | Bounded failure / continuation |
| --- | --- | --- |
| Grind selects optional sector / target | current world + selected objective | existing cooldown/target policy; no synthetic destination change inside follower |
| Follower acquires intent, provider loads scope | increasing actual tile cursor; loaded query | route->expanded->eligible full-map, same intent; frozen/absolute init limits above |
| Detour projects start/end | nonzero polygon refs + source query result | typed start/destination/avoidance projection failures, not search no_path |
| Detour path search | status/corridor, node/buffer/partial evidence | no_path only for actual query no-path; query errors retained |
| Follower validates | path length, terrain, steep-transition and portal rules | typed PathValidationDetail;14O.1 fail-closed unchanged |
| Steering / CTM | ray/clearance/portal proof, issued command provenance | existing exact/UNKNOWN transition attribution; bounded alternate staging/recovery |
| Physical execution | actual displacement; distance/corridor advancement | dispatch/replan is not physical progress; original stall windows |
| Local recovery / backtrack | verified forward portal + destination gain | lateral recovery target arrival alone cannot refund episode;4/2 limits |
| Hysteresis / hazard exclusion | exact directed pair only when proven; retained spatial hazard risk | same known edge suppression; unknown remains unknown; no broad blacklist |
| Optional roam terminal / Grind | terminal shared-nav result | explicit NAV OBJECTIVE ABANDON; existing cooldown and new owner objective |
| Mandatory death/quest intent | original destination remains owned | existing terminal attribution, never silently convert to new roam |

New NavigationPlanFailure values are appended to preserve existing enum
ordinals: start_projection_failed, destination_projection_failed,
avoidance_projection_unresolved, persistent_hazard_rejected. Genuine unknown
query strings remain other_unknown. NAV PLAN FAILURE supplies intent, tier,
start/end polys, raw Detour status, tile/polygon counts, buffer/node/partial
flags, terrain filters, requested avoidance, query error and validation detail.
The hazard rejection wrapper previously overwrote its underlying query error;
that original error is now logged separately. Policy rejection alone does not
prove a physically disconnected mesh or that every alternative crossed a hazard.

Existing command-time directed-pair attribution, local hysteresis, portal
staging and episode accounting are unchanged. No new oscillation detector:
baseline pre-query cancellations provide no corridor/polygon alternation proof.
New offline tooling correlates repeated positions without inventing geometry.
Actual CTM-no-displacement and displacement-without-destination-gain remain
distinct from this proven zero-command loader failure, and need exact live
episodes if still frequent after the fix.

## Geometry inspection and hazards

Existing navmesh_debug inspected exact start B and recorded sector15 destination
(-567.021,-3866.64,92.481), sector5 (-778.865,-3644.91,92.481), map1.
Sixteen tiles loaded; start projected to Z92.902, destination polygon was0 in
both reports. Sector5's local horizontal ray was complete but sampled surface
Z61.651, about30.83 yards below requested Z. This reproduces projection failure,
not a safe walkable path at requested height. Runtime sector targets inherit
current-player Z; a replacement elevation policy is not yet source/runtime
proven. Do not widen projection height, guess new Z, or weaken terrain safety.
Reports are local `/tmp/wow-p03-baseline.yklizL/sector15*` / `sector5*`, not
production coordinates or runtime proof. The existing optional owner terminal
abandonment is retained and now explicitly logged.

Persistent hazards were not cleared or reweighted. Existing spatial cells,
confidence/exclusion, success credit and critical-corpse fallback are unchanged.
False global escalations can accumulate hazard debt, but this checkpoint does
not claim any current hazard entry is invalid or remove evidence to improve
metrics. Remaining hazard-vs-query ambiguity is exposed in telemetry.

## Validation and protected behavior

TEST PASS:87 strict C++20/Wall/extra/Werror tests, six new offline-audit Python
tests, existing QuestDB/SQL suites and eight Lua fixture programs. Full run
`/tmp/wow-validation-24x50g5h/results.json`; full build/diff passed. New focused
tests cover progressing/frozen/regressing/bounded initialization, same-intent
tier continuity, actual post-planning stall, real displacement, unknown evidence,
combat preemption, descriptor-base posture and supported-byte decoding. Existing
recovery episode, command provenance, hysteresis, terrain, hazard, death, combat,
AFK tests remain green. No coordinates or quest/NPC-specific branch added.

MaximumPathLength2000, surface recovery4, last-safe backtracks2,14O.1, persistent
hazard storage and RuntimeRobustness recovery semantics remain unchanged.
New source checkpoint has no AFK/death/combat policy changes. Latest baseline
directly shows natural P0.2 same-target latch repair:GUID0xF130000D58003736,
range2.9297, target100->92 after re-engage, player472->453 before recovery.
Seven confirmed combat recovery verification events in the normal20-minute session; combat
baseline RUNTIME PASS. AFK three confirmed prevention pulses including deferred
combat resume at input age249165; baseline RUNTIME PASS. No natural death in
this session; prior complete natural DeathRecovery cycles remain PASS, new
navigation-checkpoint regression is PENDING if death occurs naturally.

Isolated intended staged tree also PASS:38 published strict C++ tests, six
offline-audit Python tests, QuestDB SQL fixture, three published Lua fixture
programs and full DLL/loader/GUI/testhost build, results
`/tmp/wow-validation-feq40npk/results.json`, tree exported under
`/tmp/wow-navigation-final.7qQhjW`. The first isolated run exposed zero published
QuestDB Python test files (the13 tests exist only in unrelated local work).
Validation now registers that discovery only when files actually exist and
explicitly reports absence; SQL remains mandatory. It still ran all13 Python
tests in the full worktree. No failing test is skipped or unrelated file added.

## New runtime gate

RUNTIME PENDING for loader/posture fixes and reduced churn. Fresh normal process,
Start Bot through existing GUI;20–30 minutes natural unattended Grinding.
Preserve full capture before another launch clears it. Do not manufacture bad
terrain, combat stall or death. Compare counter rates to0.800/2.100/1.500 per
minute, but do not require zero genuine recoveries. Expect progressing loaders
to finish or fail with their exact query reason, no owner cancellation at48
ticks while tiles advance, no impossible posture51, coherent intents and actual
movement/XP/kills. Verify AFK/combat and any natural death independently.

```bash
wine ./build/wow_gui.exe
tail -n 0 -F build/wow-internal.log | rg --line-buffered 'NAV |NAVMESH|MOVEMENT INTENT|HARD-STALL|STALL|CHURN|ROUTE|Grind14G2|Autonomy14G4|DEATH|COMBAT|AFK '
```

Run the audit against the preserved NEW full log. Remaining18 acquisition
idle-deadlock resets and any actual issued-command stalls are next evidence
targets, not silently marked resolved. No swimming or later V6 phase begins
inside this checkpoint.
