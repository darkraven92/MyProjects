# P0.3 navigation reliability audit

## P0.5.9 terrain-failure provenance audit (2026-10-08)

The latest user-reported multi-death run reconfirmed P0.5.8 directed-link
avoidance and end-to-end resurrection **RUNTIME PASS**. A full-map route with
`rejectedTransitions=1` / `effectiveConstraintMode=directed_links` makes its
route→expanded→full-map inheritance **RUNTIME PASS** as well. A second death
failed closed on an 8.996/10.000 portal-to-portal leg after the first known
unsafe edge was excluded. The new failure's `pointIndex=11` means
`NavPathResult::points[10] -> points[11]`; it does not index corridor polygons.
Their exact refs are not in the preserved rejection line. The repeated point
coordinates suggest reused route geometry, but do not prove which upstream
link was shared across tiers or corpse variants.

Source trace: `findPath` retains the complete ordered polygon corridor;
`findStraightPath(..., DT_STRAIGHTPATH_ALL_CROSSINGS)` emits a point ref naming
the polygon entered. In this Detour fork, `appendPortals` appends only actual
2D portal intersections and `appendVertex` merges coincident points while
updating the ref. Thus adjacent emitted points can correspond to nonadjacent
corridor indices. The old `ValidateTerrainRoute` kept only a transient scan
index and returned an unsafe leg with `fromPoly=toPoly=0` if the indices were
not exactly one apart. Nothing was lost by failure serialization; singular
edge attribution was unavailable at validation. Blocking the first proven
edge caused a new Detour corridor to be checked, exposing a separate unsafe
leg; it does not prove that the second leg is the same directed transition.

The path now retains each straight point's ordered source corridor index.
Validation reports `corridorFromIndex`, `corridorToIndex`, `adjacent`, and
`transitionKnown` on each terrain rejection. The Detour link check precedes
this attribution. Only a one-link span with verified adjacency obtains
nonzero directed refs; those refs continue through the existing bounded
follower and DeathRecovery transition memories. A zero- or multi-link span
cannot be assigned a singular bad edge without guessing, so it remains an
unsafe fail-closed result. No nearest-poly inference, spatial fallback,
large-polygon blacklist, water-policy change, or budget increase was made.
P0.5.9 full-worktree static TEST PASS / BUILD PASS / DIFF CHECK PASS:
102 C++ tests, 26 audit Python tests, 13 QuestDB Python tests, SQL fixture
and Lua fixtures. Runtime is PENDING; the anonymous multi-link branch still
requires source-safe attribution or a separately qualified bounded fallback.

## P0.5.7 generated destination and partial-stage audit (2026-10-08)

The P0.5.6 manual run was PARTIAL PASS / OVERALL FAIL. Portal classification,
verified forward portal progress, bounded recovery reset, unsafe-vertical
rejection and unchanged 2000/4/2 budgets were observed. Of 54 terminal
movement failures, 48 were destination projection failures. A generated
sector at `(2401.633,-2452.717,100.330)` inherited the player's
`100.330` Z from `(2316.780,-2537.570,100.330)`; route, expanded and full-map
each found destination poly zero. The source owner constructed these XY sectors
with `sectorOrigin_.z`, then replaced Z with `world.player.z` before Start.
That Z was a search seed, never a verified ground point.

Generated roam and post-death safe-sector targets now use bounded route-scope
preflight: up to 24 yd vertical seed search in 4-yd samples, 3-yd nearest-poly
horizontal extent, 2-yd nearest-poly vertical extent, and at most 6 yd
projected XY deviation. Projection excludes water and steep flags, rejects a
hard-hazard endpoint, and requires a complete connected Detour route with
the same terrain validator used by the follower. Unavailable optional sectors
cool down before a movement intent exists. There is no full-map escalation
for an unproven generated endpoint and no global nearest-poly extent change.
The persistent session topology is reused; route tile loading remains
incremental. `GRIND ROAM TARGET PREFLIGHT` logs one accept/skip per sector.

Partial staging formerly counted all accepted partial corridors against one
intent-wide three-attempt limit; physical displacement of ~81 yd did not
matter. The same limit now starts a new episode only on a fresh same-generation
validated route whose start poly matches a live player projection and lies
*ahead* on the previous validated directed corridor, with >=4 yd physical
displacement and >=4 yd destination gain. Rejected candidates never reset the
counter. New `NAV PARTIAL STAGE PROGRESS` telemetry distinguishes earned
episodes from retained budgets. The 3-attempt episode limit and all 2000/4/2
navigation limits are unchanged. A later replan exhaustion and one bounded
local recovery are recorded but not expanded into broad changes; two
persistent-hazard rejections remain correct.

P0.5.7 static TEST PASS / BUILD PASS / DIFF CHECK PASS in full worktree
(100 C++/26 audit Python/13 QuestDB Python/SQL/nine Lua fixtures) and exact
isolated staged tree (51 C++/26 audit Python/SQL/five published Lua fixtures).
Runtime PENDING; no WoW or GUI was launched in this checkpoint.

## P0.5.6 complex-terrain portal and vertical-route audit (2026-10-08)

Starting published HEAD `7281743f3a0242be71b2051101a4126b69302702`.
P0.5.5 combat initiation is RUNTIME PASS in the clean manual run (ready
bootstrap input, Attack dispatch, verified damage, multiple fights and kill).
P0.5.6 navigation was later PARTIAL PASS / OVERALL FAIL in the manual run;
no WoW/GUI run is authorized in this coding checkpoint.

The available full log shows complete non-steep Detour corridors but repeated
clearance/raycast rejection near directed edges. Example death route: at
`(266.708,-1951.753,92.153)`, a ray from poly `...D07` toward `...CFB`
stopped in `...D07` near linked edge `...D07 -> ...CDF`. This is a Detour wall
hit, not a successful portal crossing. At `(262.004,-1951.304,91.888)`, an
8.461-yd lateral recovery improved destination distance by 8.240 yd but had
no verified forward poly transition, correctly remaining neutral. Detour's
raycast traverses a directed link only if its edge/filter and clipped tile
interval permit it; proximity to the link alone cannot authorize CTM.

The follower now distinguishes intended portal passage, blocked corridor
boundary, unrelated blocking geometry, unavailable provenance and insufficient
clearance. On a known directed edge it tries the clipped midpoint and then
bounded interior samples of the *filtered destination polygon*. Each candidate
must project to the expected poly, have safe live vertical geometry and step,
clearance >=0.70 yd, and a complete ray visiting exactly the directed pair.
If no candidate passes, existing bounded lateral/backtrack/terminal behavior
remains. The mesh is never mutated and no water fallback is enabled. Crossing
credit for such an issued interior stage requires a fresh physical observation
in the expected destination poly, same route generation, a complete post-move
ray visiting exactly that pair, safe live geometry and >4 yd destination gain; command
dispatch or a new route alone cannot refund attempts. Ordinary lateral
recovery still remains neutral without proof of a forward transition.

`ValidateTerrainRoute` checks actual ALL_CROSSINGS straight-path legs (live
player -> first portal, then portal -> portal); its steep-fallback center check
is separately typed. The reported 3.851/6.040 and 8.996/10.000 horizontal/
vertical samples exceed the unchanged 14O.1 slope gate. Current source already
learns a valid rejected directed edge and tries a bounded polygon exclusion;
an unidentifiable skipped edge or no safe alternative remains fail-closed.
The available local log does not include those two exact later vertical events,
so it cannot prove whether they were real cliffs, tile seams or projection
mismatches. New sparse rejection telemetry records tested point endpoints,
source (live player or portal), projected-start delta and exact tile-seam
identity for the next manual run. No Ashenvale coordinate or DeathRecovery
special case is introduced. Persistent hazard memory and 2000/4/2 budgets are
unchanged.


Updated 2026-10-07. P0.3/P0.3.1/P0.3.2 baseline RUNTIME PASS.
P0.4-TEMP adds a production-default living-only query-time Water exclusion
`0x08` (including mixed Ground|Water polygons), retaining the session topology
and Ghost DeathRecovery exception. Typed `water_traversal_disabled` is a
capability restriction, not persistent hazard evidence. SOURCE/TEST validation
is separate from a pending natural live water encounter. Details are in
WATER_SWIMMING_DROWNING_AUDIT.md. Water-tagged terrain is NOT a waterline.
Historical checkpoint sections below retain their original evidence.

Latest full capture (P0.4 audit): `/tmp/wow-p04-baseline.yBnhXp/wow-internal.log`,
137775lines/38.07115minutes,67kills/67loot successes/0deaths, final movement0/
runtime0/strategic0/idleDeadlocks0/escalations0. Seven confirmed AFK inputs.
Cache cold30+60+614=704 reads/additions;5085hits; four warm full-map requests
704hits/0reads/0adds each,264.276–293.499ms. Only session teardown invalidated.
This supplies live cache performance PASS, superseding the historical pending
section below. No water encounter or water behavior PASS can be inferred.

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
# P0.5.8 DeathRecovery long-route transition avoidance (2026-10-08)

The P0.5.7 manual run gave generated-roam preflight RUNTIME PASS (5 candidates,
2 skipped, 3 accepted, no destination projection failures, one projected
destination reached). Partial-stage episode reset is RUNTIME PENDING. A natural
death then exposed unsafe vertical edges during a long corpse-route replan;
route variants and full-map fallback did not find a safe alternative. The
3.851/6.040 and 3.269/6.562 transitions remain unsafe and must be rejected.

Source audit: `NAV 15B.1` used a proven directed pair A→B but converted it to
temporary whole-polygon B exclusion (`requestedTargets=1`,
`appliedPolygons=1`). This also rejects a potentially safe C→B entry. The
local Detour `findPath` expands each polygon's directed `dtLink` chain and skips
zero refs. A query-scoped mask of only A→B links, under the session-cache lock,
preserves C→B and B→A. Saved link refs are restored before releasing the lock.
The cached topology is not persistently changed. Source-validated 14O.1 terrain
checks still run on every resulting route. A query returning an excluded edge
fails closed. The existing route→expanded→full-map sequence and all attempt
counts remain unchanged.

One DeathRecovery episode now carries a bounded, mesh-generation-tagged set of
proven rejected edges across its followers and corpse-anchor variants. The set
is reset for a new death episode/resurrection or incompatible generation;
unknown generation cannot authorize reuse. Rejected-corridor fingerprints are
tracked for variant-diversity telemetry; no candidate is accepted solely due
to a new offset or fingerprint. A safe route remains unproven until a manual
natural-death runtime. P0.5.8 TEST PASS / BUILD PASS / DIFF CHECK PASS in the
full worktree (101 C++ / 26 audit Python / 13 QuestDB Python / SQL / 10 Lua)
and exact staged tree (52 C++ / 26 audit Python / SQL / five Lua). P0.5.8
RUNTIME PENDING.
