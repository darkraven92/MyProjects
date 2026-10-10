# P0.1 DeathRecovery reliability audit

## Resurrection location safety investigation (2026-10-10)

**New safety gap: RUNTIME OBSERVED / SOURCE VERIFIED. Safe staging and
DeathRecovery-owned living egress: NOT IMPLEMENTED — evidence gap.**
Inspected clean checkpoint `6abd7ac3ff89e1c96f25e2869e8259499f8134e7` on
`codex/vendor-afk-long-navigation`, AGENTS.md, AI_HANDOFF.md and relevant death,
navigation and world-reader history. Three parallel read-only reviews covered
resurrection architecture, hostile/world evidence, and NavMesh/test design.

Historical **P0.5.8 RUNTIME PASS remains preserved**, with its original scope:
natural death, directed-link route avoidance, reclaim, two fresh alive probes
and Combat/Grind resume. The preserved user-reported qualification is recorded
in [CODEX_PROJECT_STATE.md](CODEX_PROJECT_STATE.md#p053-release-requalification-prep-2026-10-08).
It did not qualify avoiding post-reclaim re-aggro. This is a new safety/design
requirement, not a revocation of successful resurrection or route qualification.
The older scope/status and incidents below remain historical.

### Runtime observation

The user's supervised observation prompted the investigation; source causation
was not assumed. A newer `build/wow-internal.log` contains corroborating evidence.
Its stable bytes and lifecycle log were copied into the Git-excluded directory
`runtime-captures/death-safety-2026-10-10/` before analysis. The earlier
`vendor-runtime-2026-10-10-next` capture ends during the first corpse run and
does not contain its later resurrection/redeath sequence.

Raw log: 36,744 lines / 2,941,252 bytes, SHA256
`53a8cddeb95f64e15f440d525f65afad29959da50fd017c798b5cb5dd54bf223`.
Lifecycle: 136 lines / 34,079 bytes, SHA256
`e5bfea4c987eb65845ef7fc0b5911b027af1d5f31f48a4f5d532638fc25a59cd`.
Relevant session: `300.134361024694534150.71490348.508`, player GUID 112743,
map 1. Lifecycle BOT START/STOP: monotonic 71490867–72351698 (~14m21s),
ending with GUI stop and unload request. Earlier lifecycle sessions are not
part of this episode. No captured DLL hash establishes exact source/binary
identity. No new WoW session was launched by this investigation.

All following references are one-based raw-log lines:

| Lines | Observed sequence |
| --- | --- |
| 7785–7789 | First death anchor (2004.178,-2580.670,94.960); DeathRecovery enters episode 1. |
| 19008–19087 | Reclaim at 6.397 yards; HP 1→332/664. Native combat flag is already set at 19054 while WaitingForAlive owns updates. First alive probe succeeds; HP falls to 273 before the second probe. |
| 19110–19143 | Second fresh alive probe, verified reclaim, Done, Combat/Grind resume. Same position (2010.3558,-2581.5701,96.3555). Direct aggressor GUID `0xF130000F58007F25`, entry 3928, is tracked at 1.9573 yards immediately after the exit records. |
| 21070–21137 | HP reaches zero. Episode 2 records death at (2010.356,-2581.570,96.356), matching the resurrection position at logged precision. |
| 26859–26984 | Second reclaim at 5.310 yards; two alive probes at 357/664 HP; Done and Grind resume. Low-health gate starts passive recovery without usable food. |
| 27002–27055 | At (2015.3982,-2580.8809,97.8714), recovery is preempted by the same entry/GUID, distance 7.7201, explicitly `evidence=live-targeting-player`; defense resumes. |
| 29207–29274 | HP reaches zero again; episode 3 starts at (2015.398,-2580.881,97.871), matching the second resurrection position. |
| 36589–36712 | Third run terminates on unsafe terrain; later two fresh alive probes take the existing `manual_alive_confirmed` exit. This is not a third automatic reclaim success or proof of who caused the alive transition. |

There are two automatic reclaim confirmations, three death starts and no
`post-death danger escape` movement-intent start. The log proves rapid combat
after reclaim and two subsequent deaths in place. It does not prove a mob aggro
radius, exact time-to-redeath from un-timestamped lines, a safe alternative
location, or that a different staging point would have prevented either death.

### Source verification and causal limit

`DeathRecoveryController.h:1638` gates reclaim on the current server corpse
anchor, fresh Ghost/delay evidence, retry bounds and the **8-yard precision
distance**. It performs no nearby-threat or living-ground suitability assessment.
The generated 14-yard ring variants in `RouteDestinationForVariant` are routing
alternatives; they are not safety-ranked resurrection positions. Routes reaching the broad
arrival band are brought closer through precision routing.

`WaitingForAlive` at line 1696 calls `CompleteRecovery` after the established
two fresh alive probes. `CompleteRecovery`/`FinalizeAliveEpisode` at lines
930–959 go directly to Done. `WorldMonitor.h:1453` re-arms the controller and
resumes Combat plus Grind/Quest without a danger/recovery release predicate.
Combat remains suspended while those alive confirmation probes accumulate;
the first trace already shows combat/damage in that interval. Self-defense
must not be delayed further by simply inserting another waiting state.

There **is** existing post-death protection: `RecordDeath` arms
`postDeathEscapePending_`; Grind suppresses ordinary pulls, and
`TryStartDangerEscape` (`GrindModeController.h:1004`) waits for no direct
aggressor and at least 90% HP before choosing a NavMesh sector. The ordinary
recovery and defensive paths therefore run first. This is not a missing
post-death flag or evidence that Grind deliberately pulled a new target.
It cannot protect the pre-reclaim location or provide the requested early
DeathRecovery-owned egress. The observed control flow is consistent with
healing/defending in place before escape becomes eligible. Avoid claiming this
handoff alone caused every death, or lowering the health gate as a purported fix.

### Exact evidence gaps preventing the requested safe-point policy

1. **Ghost threat coverage is unqualified.** `WorldState.h:27` exposes read
   failures and interrupted enumeration, not server/client visibility coverage.
   The vector includes only type-3 units (`:214`), not other players; reaching
   the 4096-object cap does not independently mark truncation. `valid=true`,
   zero failures or an empty vector cannot prove the surroundings threat-free.
   Ghost samples at raw lines 18014/18258 contain zero units, 18564 two, 18834
   four; the alive sample at 19080 contains seven. Different time/position means
   these counts do not prove which units were omitted. There is no complete
   identity/position/visibility record at reclaim or candidate locations.
2. **Hostility and aggression are not equivalent to a creature-shaped object.**
   `PullSafetyPolicy.h:13` explicitly describes a potentially attackable proxy,
   not exact hostility or aggro radius. `GrindTargetPolicy::LooksLikeCombatCreature`
   excludes pets/NPC-flagged units; exclusion cannot certify harmlessness.
   `UnitSnapshot.h:141,167` does not retain read-success evidence for target GUID
   or faction template. Its zero fields cannot authorize a no-threat conclusion.
   Existing selected-target UnitCanAttack checks do not classify every unselected
   Ghost-visible GUID. The 12-yard density heuristic must not become a supposedly
   qualified resurrection aggro radius. Visible threats can inform relative
   ranking or reject a point, but do not supply a positive safety predicate.
3. **Fresh targetless living safety evidence is not an existing adapter.**
   `CombatClientEvidence5875` already supplies native combat-bit semantics and
   a fresh game-thread type-3/type-4 direct-aggressor sweep, explicitly avoiding
   absence inference from the cached vector. Its `Execution` entry requires a
   target. A recovery release needs checked, current, same-player evidence
   without selecting or attacking a target just to obtain that observation.
   A checked scan would close read/truncation gaps, not Ghost visibility coverage.
4. **Living terrain/water and owner identity must be kept distinct.**
   Death route starts choose GhostDeathRecovery once Ghost was confirmed
   (`DeathRecoveryController.h:717`); that historical latch cannot authorize
   living egress. `WorldMonitor.h:1068` also treats `deathShouldOwn` as death
   water ownership. Merely retaining an alive DeathRecovery state would bypass
   the living-water controller. `WaterEvidencePolicy5875.h:29` leaves ground
   contact/submergence/surface unknown; non-swimming alone is not dry-ground
   proof. Existing water guards and uncertainty must remain authoritative.

The first two gaps block the requested "safe corpse → immediate resurrection"
and "safe candidate → resurrect" decisions. A policy that calls unknown safe
would weaken evidence boundaries; a policy that blocks every unqualified Ghost
snapshot would disable historically qualified automatic recovery without
implementing usable safe staging. Therefore this checkpoint uses the user's
explicit **document the exact gap instead of guessing** fallback. No production
policy, ownership change or disconnected placeholder tests are introduced.

### Available navigation and bounded design once evidence is qualified

NavMesh access is available; lack of pathfinding is not the blocker.
`DetourNavigationProvider::ProjectGroundNear` projects to filtered ground,
but projection alone proves neither connected travel nor occupancy safety.
Grind's existing preflight uses projected endpoint identity, connected non-partial
`FindPath`, terrain validation and hazard rejection. GenericNavMesh supports
movement-free planning, `PlanningOnlyReachedDestination` and
`PlanningOnlyProjectedDestination`; a partial stage is not candidate arrival.
Revalidate execution from the current origin and retain learned directed-link
constraints. Use `AvoidUntilQualified` for staging intended to become a living
position and for all living egress; a Ghost-permitted corridor cannot certify it.

The 32-yard policy constant is not permission to broaden current reclaim.
Both this trace (6.397/5.310) and historical P0.5.8 (6.301) qualify close reclaim,
not arbitrary positions at the outer range. Candidate generation can remain
inside the current 8-yard 3D gate, with projection and final live-position
rechecks. Fresh server anchor, known delay zero, Ghost evidence, dispatch and
two alive probes remain mandatory; route arrival never substitutes for them.

Once sensing is qualified, the smallest design should remain in DeathRecovery:
evaluate current position plus a finite generated shortlist (at most nine
candidates total, using the existing variant count), spend existing route-attempt/liveness budgets on probes
and execution, rank only qualified candidates, and stop on no defensible point.
Do not reset the 18-route, nine-stationary-failure, 180-second no-progress or
300-second episode bounds when candidates change. Failed searches must not
fall back to an unsafe reclaim. Density/clearance may rank points but must not
pretend to be client aggro eligibility.

After confirmed resurrection, retain a distinct living recovery owner with
ordinary Grind/Quest acquisition suppressed. It needs a defense-only handoff,
fresh living-route validation, water priority, explicit recovery/safety release
criteria and a finite monotonic deadline. Timeout must stop navigation and
retain a blocked living owner with defense available; reusing today's Failed
state would release after two alive probes without a safety check. Death again
must start a new death episode, and world gaps must invalidate all partial
safety evidence. State entry, candidate rejection/selection, actual route
readiness, reclaim gate, defense/water preemption, deadline and release reasons
need sparse telemetry. A successful build cannot qualify this proposed design.

### Required qualification and deterministic cases

First obtain read-only, same-session Ghost-to-alive evidence for local object
coverage, per-field knownness, relevant living-unit identities/positions and
reaction/attackability semantics at candidate positions. Verify the source of
that visibility contract; sampling counts alone cannot establish it. Qualify a
targetless native combat/aggressor reader and the living water/ownership handoff.
Keep unknown, partial and stale observations explicit. Do not provoke a death
or widen reclaim distance to manufacture acceptance.

| Future deterministic case | Required behavior once the evidence adapter exists |
| --- | --- |
| Qualified safe corpse | Immediate reclaim permitted only by the unchanged fresh anchor/Ghost/delay/8-yard gate; no command-only success. |
| Dangerous corpse, safer candidate | Choose a qualified, fully reachable, living-compatible projected point inside range; recheck danger and range before reclaim. |
| No safe or known candidate | Bounded search/wait then blocked; no unsafe fallback and no unknown-as-safe inference. |
| Successful resurrection | Two fresh alive probes enter temporary living recovery ownership, not normal Grind. |
| Safe recovered living state | Explicit fresh safety and recovery criteria release once; pre-reclaim evidence cannot release it. |
| Combat/water/world priority | Defense can preempt without voluntary acquisition; living water cannot inherit Ghost exemption; world loss invalidates proof. |
| Timeout/retry/redeath | Absolute budgets survive candidate changes and preemptions; timeout cannot loop or release on alive-only proof; another death restarts death handling correctly. |

These are an acceptance plan, **not tests of an implemented feature**. Next
runtime must additionally show actual staging/reclaim location, retained living
ownership, qualified egress or bounded block, self-defense, and safe release.
Historical AFK, water, vendor and reconnect qualifications are unchanged.

### Validation of this investigation checkpoint

`PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4` passed all
108 C++ tests, registered Python/SQL/Lua suites, the full `cmake --build build`
(up to date), and diff check. Report: `/tmp/wow-validation-f45nnesi/results.json`;
console output: `/tmp/death-safety-2026-10-10-validation.log`. These checks cover
the existing implementation, not the unimplemented safety design. No new tests
claim candidate safety, living recovery ownership or runtime success. This
checkpoint changes only this audit, the runtime audit cross-reference and
AI_HANDOFF.md; preserved raw evidence remains local and Git-excluded.

## Scope and status

2026-10-07. SOURCE VERIFIED; deterministic/build verification recorded in
CODEX_PROJECT_STATE.md. New behavior is RUNTIME PENDING. No running WoW
process was available. This checkpoint does not claim successful resurrection,
a safe route through the rejected terrain, or long-run reliability.
AFK code, native-clear permissions, mixed-state policy, Ghost water-walk
allowance and scheduler are unchanged. No swimming/combat-watchdog work.

## Direct runtime evidence

Latest inspected build/wow-internal.log session
`1360.134358440699456960.106923713.436`:

- Lines25290-25311: observed body at (-646.515,-3524.180,91.7361), map1,
  persisted record, death ownership; one spirit-release dispatch.
- Lines25336-25381: Ghost confirmed at (-1081.400024,-3478.679932,63.606602),
  corpse distance438.163. Route intent34 accepted for incremental initialization,
  NOT movement success. RouteStarts=0; RetrieveAttempts=0.
- Lines25442/25551: route and expanded tiers reject unsafe terrain. First
  proven edge ends B84->B82, horizontal8.006, vertical6.639, rise/run0.829,
  portal (-866.933,-3458.133,86.136) to (-874.667,-3456.000,80.136).
  Alternate straight leg has horizontal8.561/vertical7.184, refs unknown;
  rejection `unsafe_terrain_attempt_limit`. Unknown refs stay unknown.
- Lines25570-25744: full-map fallback begins,704 tiles; at188 processed,
  elapsed35061 ms, the160-tick physical watchdog destroys intent34 and
  unfinished loader. It has already consumed the sole full-map allowance.
- Precision retry35 and variants then repeat route/expanded rejection with
  full-map disabled. No CTM route movement. At180209 ms without progress,
  unchanged liveness policy authorizes one strategic continuation using the
  previously proven bad edge. Its full-map loader is also interrupted by the
  precision watchdog; latest retry43 fails expanded validation.
- Line27667: `strategic_route_failed`, navFailure=path_validation_failed,
  routes never became usable, ghost stayed stationary. This is neither a
  reclaim failure nor evidence that the corpse destination was unprojectable.
- Ghost AFK qualification and later prevention in RoutingToCorpse AND Failed
  passed in this same capture. Death failure must not be blamed on AFK.

Older `missing_corpse_anchor` is historical/user-reported evidence preserved
in continuity; its original capture was not found among retained project
runtime-captures. Its current source path is independently verified below.
Do not present a reconstructed timestamp/position as that older live evidence.

## Separate root causes

`missing_corpse_anchor`: Start's bootstrap path previously consulted only
lastClearlyAlivePosition or a recent same-character persisted body record;
absence immediately entered Failed. It did not read the server-fed client
corpse cache. A historical healthy position could also be mistaken for the
current corpse. The fix removes that promotion and allows bounded server
publication observation instead. Missing data is not guessed.

`strategic_route_failed`: legitimate shared terrain rejection PLUS a proven
owner-layer bug: physical-stall rerouting unconditionally destroyed a pending
incremental loader. Planning is not physical progress, but it is also not an
executing movement command that can be fixed by precision rerouting.
The watchdog now preserves a pending loader/intent. The unchanged180000-ms
no-progress and300000-ms episode deadlines still terminate it; loading does
NOT earn progress/reset budgets. Full-map success remains unproven. Expanded
unsafe-terrain failure alone is not evidence a globally safe route exists.

## Build5875 corpse cache audit

Client `/home/ludvig/Games/WoW Vanilla/WoW.exe`, SHA256
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`,
base0x400000. Reproduce read-only:

```sh
python3 tools/death_client_audit.py '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x48f734 --stop-address=0x48f819 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x491f50 --stop-address=0x4920c0 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
```

0x48F494 registers opcode0x216 to dispatcher48F690. Its two-level switch
maps that opcode to48F734. Handler reads found byte; requires the active
player and PLAYER_FLAGS Ghost0x10; parses display map, three float XYZ,
actual corpse map and calls492010. Writer stores display map B4E31C,
XYZ B4E284/B4E288/B4E28C, actual map B4E320. Getter492090 reads the same
fields. No corpse GUID/publication timestamp is supplied; telemetry says
unknown, never a synthetic GUID or age.

491F50 first invalidates maps to -1/XYZ0 via492010, then requests normal
MSG_CORPSE_QUERY only for Ghost. Client world initialization490A28 and active
player Ghost-bit-change callback5EEAC1 call this reset/query. Callback
5EE990 computes old XOR current flags; changed0x10 is the reset gate, on both
entering/leaving Ghost. Thus the cache has a normal reset across life/world
changes; reading happens on the game thread, after callback execution. No
hook, request, native writer, flag patch or packet is added by the bot.

Local VMaNGOS QueryHandler.cpp155-201 corroborates protocol: missing corpse
returns found0; otherwise found1/display map/XYZ/actual corpse map. Cross-map
dungeon corpses can yield entrance coordinates instead of corpse coordinates.
The reader explicitly requires BOTH maps equal the requested/current bot map;
it does not reclaim using an entrance. Cross-map recovery remains unsupported.
This local server is protocol evidence, not proof of the live server revision.

Source-backed object fields CORPSE_FIELD_OWNER=word6 and POS_X/Y/Z=9/10/11
also exist, but this checkpoint does not add a streamed-object/GUID source.
The current WorldState unit list does not enumerate corpse snapshots; a corpse
hundreds of yards from the graveyard need not be streamed. Server-fed cache
is the new source, not a fabricated nearby object.

## Anchor and state model

| State | Evidence/action/next state | Bounds and ownership |
| --- | --- | --- |
| Idle | health0 body, or fresh Lua dead/Ghost at HP1; suspend workload before Start | Shared Questing/Grinding death gate, same-character identity |
| ReleasingSpirit | RepopMe dispatch -> WaitingForGhost; dispatch never proves Ghost | Existing2-second retry; at most6 attempts, no unearned cycle reset |
| WaitingForGhost | fresh UnitIsGhost confirmation; known routing anchor -> precision/ordinary route, or reclaim when near | Existing1-second probes; no ghost release command |
| AwaitingCorpseAnchor | no movement/reclaim; sample server cache until same-map/current-player Ghost anchor | 12000-ms publication bound; timeout corpse_anchor_pending_timeout |
| RoutingToCorpse | shared scoped->expanded->full-map initialization; movement readiness and physical displacement are separate | Keep intent during pending load; original2000/4/2 navigation limits; death180/300-second bounds |
| WaitingForReclaim | current server location + fresh Ghost probe + known nonnegative delay0 + within8-yard precision gate | Unavailable cache -> bounded anchor wait; no inferred-position reclaim |
| WaitingForAlive | RetrieveCorpse dispatch, then two fresh known-not-dead/not-Ghost probes with positive HP | Existing2-second retry/two-attempt precision reposition, total8 reclaim commands per death |
| Done | confirmed automatic alive; invalidate persisted active body record; monitor re-arms and resumes workload | Follower released before normal ownership resumes |
| Failed | exact terminal reason, last query tier/projection/validation retained; follower released | Death ownership deliberately remains while dead/Ghost; only existing two same-character fresh alive probes release it |

Sources: `observed_body_routing_only`, `recent_body_record_routing_only`,
`server_corpse_location`, `unknown`. Old healthy coordinates are never chosen
for Ghost. Persisted records retain existing GUID/map/active/age/finite checks,
are cleared on confirmed alive, and cannot grant reclaim. The cache is sampled
fresh before reclaim; absent data/negative or non-finite delay cannot grant it.
The publication bound is an engineering limit derived from the existing six
two-second release opportunities, NOT measured network latency. No existing
large timeout is increased.

Server MiscHandler.cpp573-605 independently requires Ghost, current corpse,
reclaim delay expiry and within-map radius before resurrection. Request accepted
is not observable from dispatch alone: two fresh alive probes remain required.
Unknown delay does not invalidate independent alive/dead/Ghost observations.
World gaps cancel pending routes without dispatching against an unavailable
player; next valid snapshot records world_state_lost under death ownership.

## Diagnostics and protected behavior

Follower failure evidence now carries the last path-validation detail,
destination-projection evidence and initialization tier. Death terminal events
include anchor source/known, tier, projection and actual validation reason.
Unsafe terrain remains rejected; zero-reference transitions are not invented.
Strategic failures report unsafe_terrain_attempt_limit/no_alternative when
supported, otherwise exact failed tier or explicit projection/search uncertainty.
State/anchor/fallback/plan/reclaim events are transition-based. Physical progress
events are limited to the existing five-second status cadence, not every tick.

No route query, dispatch, state change or tile count resets physical liveness.
Precision watchdog still handles actual stalled movement. Existing strategy
eligibility, full-map allowance1, route attempts18, stationary failures9,
episode300s/no-progress180s remain unchanged. Native AFK and Ghost masks untouched.

## Offline geometry evidence

`build/navmesh_debug` replay of exact captured start/destination, output
`/tmp/wow-death-navmesh-audit`: start projectionZ63.740, endZ92.233,
standard50 polygons/454.428 yards; non-steep42/455.932 yards, projection
identity matches, first unsafe straight leg26. End projects successfully;
this does not validate traversal. Inspector loads12 local/route tiles, not
the cancelled704-tile live full-map attempt, and does not replay persistent
hazards. Production never uses these diagnostic coordinates.

## Tests and runtime gate

New death_recovery_evidence_test covers bounded anchor acquisition, invalid/
foreign/stale records, map/finite/ghost identity, fresh reclaim conditions,
pending-loader preservation, unchanged budgets, no command-only alive success,
shared workload/AFK integration sentinels and no memory writes. Existing death
ownership/terminal/liveness tests remain relevant; the old immediate-failure
entry expectations are updated. The initial new test failed before the policy
existed; the first full run caught the stale enum expectation in an older test.
Final validation status/artifact is recorded in CODEX_PROJECT_STATE.md.

Run normal mode; never intentionally kill or manually rescue:

```sh
wine ./build/wow_gui.exe
tail -n 0 -F build/wow-internal.log | rg --line-buffered 'DEATH|Death|CORPSE|Corpse|RECLAIM|Reclaim|NAV |NAVMESH|MOVEMENT INTENT|AFK '
```

Require natural death -> release -> Ghost -> server corpse anchor -> same
intent through tiers -> validated movement and physical progress -> reclaim
eligibility/dispatch -> two alive probes -> death ownership exit -> Grind
resume. One complete zero-intervention cycle required for RUNTIME PASS;
prefer two for stability. Long Ghost routing must retain existing verified AFK
qualification/prevention. If full-map terrain still fails, keep exact evidence;
do not relax safety or inflate budgets. No later phase begins here.
