# P0.1 DeathRecovery reliability audit

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
