# P0.4 water / swimming / drowning source gate

## Living-water emergency egress (2026-10-09)

Branch `codex/living-water-emergency-egress`, starting HEAD `937fea2`
(`maintenance: enforce two-read bag release after vendor retries`), clean
before edits. That maintenance change is retained. This section supersedes
the manual-only response below for an eligible, already-swimming living Grind
player. Ordinary water traversal and autonomous swimming remain disabled.
**Emergency egress RUNTIME PENDING**; the incident proves the old failure,
not successful operation of this implementation.

### Complete incident and attribution

Read the complete capture
`runtime-captures/water-afk-2026-10-09/wow-internal.log`: 46,303 lines,
3,674,882 bytes, SHA256
`bd78451871b7d8d9b892b81e6812b908f7e843875e7587660b45aa2895c1ef06`.
Whole-file chronology/counts were inspected before narrowing the final
combat/loot/water transition. One session contains 21 verified kills,
16 Loot PASS completions, four NoLoot completions, and one final unfinished
Approaching loot operation. There is one water-block entry, no water exit,
no logged DeathRecovery, no unavailable world snapshot, and no confirmed
AFK input-clock action. The session ends in requested stop/detach/unload.
The initial and final BOT SESSION monotonic clocks span 1,871,775 ms.

| Lines | OBSERVED chronology |
| --- | --- |
| 149 onward | Session begins in world at 19:28:48 UTC, player GUID `0x1AA56`, manager 146923528. Earlier combat/loot work completes normally. |
| 14768 | Earlier, separate AFK threshold crossing: inputAge=300009, movement/transport blocker. AFK trouble predates the final water entry; do not attribute the entire session to swimming. |
| 45755 | Last periodic pre-water WorldState: alive 241/251 HP, position `(216.5681,-5141.9634,1.2569)`, target `0xF130000C22002837`, 14 units. |
| 45930–46079 | Combat resumes chase as target leaves melee range; direct approach CTM destinations include `(218.131,-5145.386,-0.316)` then `(220.075,-5150.063,-1.501)`. Final chase dispatch completes on game thread 300. |
| 46146–46168 | Last pre-kill AFK movement word is zero; target reaches 0/100 HP. Kill 21 is verified; AutoAttack STOP dispatch completes, chase stops, corpse is handed to LootController. |
| 46175–46199 | Loot starts at corpse distance 9.108; Idle→Approaching. `IssueApproach` requests `(222.383,-5155.736,-1.717)`, dispatch completes on thread 300. Writer changes from `MoveToApproachPoint` to `LootController::IssueApproach`; Combat becomes Chasing→Looting. |
| 46201–46204 | Still alive, movement word `0x1`; AFK deferred under combat/ability owner, inputAge=83632. |
| 46205–46230 | Known swimming enters water block. Hold CTM to `(221.786,-5154.271,-1.771)` completes; writer changes Loot approach→HoldPosition. Manual-recovery wait starts. AFK reports life=alive, raw/unsupported `0x00200000`, stationary mask `0x100`, reason=swimming; inputAge=83914. |
| 46231–46258 | HP reaches 251/251; valid-world diagnostics continue. Water entry lies between surrounding UTC diagnostics 19:47:53 and 19:48:53, not at a separately timestamped exact instant. |
| 46259–46263 | Native Due=240143, Overdue=270229, threshold crossed=300062; blocker remains swimming. Due/Overdue lie between 19:54:54–19:55:55 diagnostics; threshold between 19:55:55–19:56:55. Native clock values are not interchangeable with elapsed water-episode time. |
| 46271–46303 | Last world diagnostic at 19:59:55 remains valid with movementAgeMs=702907. User Stop Bot dispatches cleanup at `(221.818,-5154.350,-1.771)`, then RUNTIME DETACHED and DLL unload_requested. No automatic water exit is logged. |

**SOURCE VERIFIED ownership path:** normal WorldMonitor→Grind→Combat chase;
`CombatController` handles verified target death, stops chase/attack and
starts `LootController`. `LootController::IssueApproach` computes a corpse
stand-off point and directly dispatches existing CTM Move, without a Detour
corridor. On the next known-swimming monitor observation, the earlier-priority
LivingWaterBlock branch neutralizes CTM, releases Grind/quest/navigation owners,
pauses combat movement, and continues the loop before normal owner updates.
SharedAfk observes under `LivingWaterBlocked`; its swimming guard rejects input.

**OBSERVED:** LootController was the last dispatched movement writer before
the swimming observation. **INFERRED, not proven:** that particular approach
caused the water entry. Chase was also moving toward the same area. The log
does not measure the exact shoreline/contact instant, command-time liquid
state, or exclude external movement. No change to ordinary chase/loot approach
geometry is justified from this capture alone.

**SOURCE VERIFIED root cause of persistent stuck ownership:** the block had
no automatic exit movement; only three externally obtained non-swimming
observations could release it. The AFK rejection was correct. A second
source defect would affect later release: `PauseMovementForLivingWater` resets
LootController to Idle but leaves CombatState::Looting; its update waits for
active/finished loot, neither of which Idle supplies. The incident never exits
water, so that later deadlock is a source finding, not a runtime observation.

### Source-qualified implementation and limits

`LivingWaterEmergencyPolicy` composes the existing LivingWaterBlockPolicy.
Known swimming still blocks ordinary movement immediately, including when
life is uncertain; emergency commands additionally require positively living
evidence. Existing three-observation release is reused, with duplicate sample
times rejected and partial proof invalidated on world gaps.

The follower's existing projected `LastSafeNavState` has no recorded native
non-swimming/life evidence and is per-route. Its surface/backtrack validation
also uses water-excluding queries. Neither is promoted to a verified water
egress route. Instead, the small emergency policy retains the latest finite
position from a valid, positively living, known non-swimming world observation,
bound to GUID, manager and local-player identity. Unknown readings never create
an anchor. Gaps, identity changes and death invalidate it. It freezes on water
entry and must be at most 4500 ms old and within the existing local recovery
limits of 12 horizontal / 4 vertical game units. This is explicitly
`recent_same_world_non_swimming_observation`, not positive dry-ground proof.

Egress owns only the existing water block's stopped movement. It is enabled
for normal Grind after complete threat enumeration excludes live aggressors,
the combat lock is absent or observed dead, and no death/reconciliation,
health recovery, First Aid, active vendor, quest/dialog or terminal Grind
owner prevents handoff. Eligible combat states are Idle, AcquiringTarget,
PostKillDelay and Looting. Live/unknown combat targets do not authorize it.
Other workloads or unsafe owners remain manual recovery; no new water combat
behavior is introduced. DeathRecovery preempts even if a transient life read
disagrees with its already-authoritative owner.

The existing hold runs before any egress dispatch; navigation owners release.
For the eligible handoff, the stale combat target, loot FSM and deferred corpse
queue are cleared and acquisition is deferred. Two local CTM backtracks at
most may target only the frozen observation. There is no destination search,
water-polygon opt-in, new route, jump, key hold or ascent controller. CTM uses
the observation's existing XYZ, with 0.1 precision for a nearby last observation;
dispatch is not proof of displacement. It does not promise safe terrain along
that short segment, breathable surface, ground contact or drowning prevention.

The budget of two comes from shared `LocalRecoveryLimits`; compile assertions
preserve follower surface max=4 and backtrack max=2 without changing their
existing values or navigation budgets. Each attempt has a 4500 ms window
(existing 18×250 ms backtrack window), with an absolute 9000 ms episode
deadline checked at monitor updates. Progress cannot refund time or attempts.
There are no surface-recovery attempts in this emergency path. A new attempt
after transient non-swimming consumes the same budget. Progress is logged at
most once per attempt; reattempt records no_progress/progress_without_exit.

Before each command, a new WorldState, known swimming, living state, identity,
threat check, local distance and episode deadline must still authorize it.
Missing/expired/out-of-bounds anchors, unsafe owners, observation/identity loss,
failed dispatch, exhausted attempts or deadline produce a stopped terminal
`decision=manual_recovery`; no automatic retry/reset loop follows. During a
world gap no stale object is commanded; a pending stop uses the next valid
player, or yields to DeathRecovery. Failure to neutralize CTM faults the session.

First known non-swimming observation stops egress. Proof 1/3 and 2/3 keep the
block; 3/3 releases it. A failed episode can also exit after manual recovery,
explicitly labelled `manual_non_swimming_confirmed`. Eligible combat intent
is re-cleared on exit, robustness timing rebaselined, and the confirmation tick
ends before any workload update. Fresh world acquisition/navigation begins on
the next tick; cancelled route and loot intents are not replayed. Ordinary
freshly evaluated objectives still use existing ground-only navigation.

AFK input remains blocked by swimming and by recovery ownership through exit
confirmation. Due/Overdue/native thresholds stay 240000/270000/300000 ms;
movement masks and DeathRecovery ghost-water filters are untouched. Egress
CTM is never counted as AFK delivery. Reconnect work is completely separate.

Regression coverage also rejects vertical-only destinations with less than
0.1 horizontal displacement. Invalid coordinates cannot bypass a known
swimming block or contribute to exit proof; neutralization failure remains a
session fault. Focused tests cover false/unknown swimming, dead/ghost handoff,
missing/expired/changed-identity anchors, world-gap proof loss, transient exit
samples, command-time invalidation, sparse progress, bounded failure, stale
loot cleanup, unchanged water filters/budgets and AFK thresholds/input guards.

### Manual runtime acceptance — PENDING

Use the rebuilt DLL with the normal configured GUI/Grind session, with
connection/water observer and AFK qualification modes unset. Keep automatic
behavior/settings and native thresholds unchanged. Do not deliberately enter
water, provoke death or disable guards. Prefer a natural recurrence during
ordinary Grind under supervision; if none occurs, this phase stays PENDING.

1. Preserve the **complete** normal and lifecycle logs before the next launch.
   Record revision, player identity, visible living state and any manual input.
2. On a natural encounter require `swimmingKnown=yes swimming=yes`, WATER BLOCK
   entry and WATER EMERGENCY entry. Inspect previousOwner/previousWriter,
   lastSafeKnown, and navigation/stale-intent invalidation. A live threat,
   missing/old anchor or unknown observation must end in manual recovery with
   no speculative backtrack.
3. For an eligible encounter inspect `state=attempt`, attempt=1 (at most 2),
   maxAttempts=2 and destinationSource. The destination must be the frozen
   observed position, not a corpse/ordinary travel objective. Observe physical
   movement independently of dispatch. Require progress or a bounded terminal
   failure; no continued backtracks after the second attempt/deadline. Stop Bot
   if the situation becomes unsafe; do not wait for drowning or death.
4. An automatic success needs non_swimming_candidate proof=1/3, 2/3, 3/3,
   state=recovered with `three_non_swimming_observations`, and stopped egress.
   One/two samples are insufficient. Record subsequent normal work from a new
   world observation and fresh target/route, with no replay of the old corpse
   approach. Manual movement makes automatic success attribution inconclusive;
   `manual_non_swimming_confirmed` is not automatic egress PASS.
5. While swimming, require AFK reason=swimming and no speculative AFK input.
   Do not wait five minutes solely to test the thresholds: bounded egress or
   manual intervention takes priority. Movement dispatch alone cannot satisfy
   native AFK input-clock verification. Ordinary living navigation must retain
   water rejection and Ghost DeathRecovery must retain its separate policy.
6. Stop Bot normally; verify detach/unload while WoW remains running. Preserve
   complete logs and correlate sparse events, for example:

   ```sh
   rg -n 'WATER (BLOCK|EMERGENCY)|MOVEMENT COMMAND WRITER|LOOT:|AFK (SAFETY QUALIFICATION|PRODUCTION|ACTION)|BOT SESSION|RUNTIME DETACHED' build/wow-internal.log
   ```

Natural evidence still needed: usable anchor retention, actual CTM egress,
three-read automatic exit and fresh-work resume; no-progress exhaustion;
safe rejection on missing/unknown evidence, gaps or owner conflicts. Static
tests cover these branches but do not qualify shoreline physics or automatic
water safety. Broader surface/submersion/breath/ground readers remain UNKNOWN.
Validation: `python3 tools/validate.py --jobs 4` **PASS**, 106 C++ tests,
`failures=[]`; 42 audit Python tests, 13 QuestDB Python tests, SQL and all
10 Lua fixtures PASS. Final report:
`/tmp/wow-validation-1lkuh3ea/results.json` (full output
`/tmp/water-egress-final-validation.log`). Explicit `cmake --build build`
**PASS**; MinGW DLL rebuilt. `git diff --check` **PASS**. Final status contains
only this audit, CombatController, LivingWaterBlockPolicy, WorldMonitor,
GenericNavMeshPathFollower, and the new LivingWaterEmergencyPolicy,
LocalRecoveryLimits and living_water_emergency_policy_test files. No commit
made. These results qualify source/tests/build only, not live egress.

SOURCE GAP — RECONNECT NOT IMPLEMENTED.

## P0.4-TEMP production living-water avoidance (2026-10-07)

Autonomous swimming remains PAUSED; P0.4.2 NOT STARTED. This is a temporary
capability restriction, not water navigation or drowning prevention. The
production-default provider uses Ground include `0x01` and Water exclude
`0x08` for living/unknown routes. The exclude is required for mixed `0x09`
polygons. Query-time filters cover route/expanded/full-map/replan, projection,
wall/ray/surface and recovery. A read-only diagnostic query may prove a
complete water-dependent corridor, but it never supplies movement; typed
`water_traversal_disabled` is returned instead. The same cached topology and
hazard memory are retained. Ghost-confirmed DeathRecovery explicitly opts into
its existing Ground|Water filter; living DeathRecovery does not.

A source-verified live SWIMMING bit `0x00200000` now blocks living autonomous
work immediately. One verified existing CTM-to-current-position command
neutralizes forward movement, ordinary movement owners release, and the loop
observes read-only until three consecutive known non-swimming snapshots.
Unknown readings cannot release the block. The event says
`non_swimming_confirmed`, never DryGround. There is no ascent/Space command,
surface inference, timer reset, or hazard mark. AFK observes but candidate
dispatch defers while blocked; defensive combat GUID remains held but water
combat is not attempted. Manual recovery is required. Failure of the stop
command is an explicit session fault. No water runtime PASS is claimed.

Optional Grind objectives cool down/blacklist after typed terminal routing;
mandatory quest/death objectives cannot be silently completed by this policy.
The query-time safety guard is SOURCE VERIFIED / TEST PASS. Full dirty tree:
93 strict C++ tests plus 26 audit Python, 13 local QuestDB Python, SQL and
eight Lua fixtures PASS (`/tmp/wow-validation-s6hlrrkm/results.json`). Isolated
staged tree: 44 C++ tests, 26 audit Python, SQL, three published Lua fixtures
and all MinGW build targets PASS (`/tmp/wow-validation-v6h1_lbq/results.json`).
DIFF CHECK PASS. New production behavior is RUNTIME PENDING: no WoW process
was running and no natural water encounter was manufactured. The P0.4.1
observe-only loop below remains unchanged. Breath/submersion/surface/ground
contact and safe ascent/release remain SOURCE NOT VERIFIED.
The real offline Detour/cache probe additionally PASSed with cold30 +
expanded60 + full614 tiles, total704; warm query-mode switching caused zero
new disk reads or `addTile` calls and did not change the generation. It did
not simulate a real water-required route or a live WoW shoreline encounter.
Short direct combat/interaction CTM commands do not consume a Detour corridor;
the verified swim-bit pause contains an accidental entry on the next monitor
observation but cannot guarantee that the first water contact is prevented.
This remains an explicit residual risk until a source-qualified surface-water
policy exists; do not claim absolute water-entry prevention from filter tests.

## P0.4.1 observe-only live evidence checkpoint (2026-10-07)

Starting HEAD `9107b2c93d661218c5b8902c42a9e16d63e9ab36` on
`codex/wow-internal-continuation`. This checkpoint adds a read-only
`WaterEvidence5875` adapter and a separate `WOW_INTERNAL_WATER_MODE=observe`
monitor loop. It branches **before** `CombatController::Start`, quest/grind
updates or any gameplay owner. The loop only samples the existing validated
world snapshot and signature-matched native movement word. It never targets,
moves, invokes Jump, installs a packet/UI hook or runs Lua. Stop/unload writes
only the ordinary runtime-control status; it issues no movement-stop command.
The default mode is unchanged. An unrecognized nonempty water mode fails
closed without starting gameplay.

Current live observation capacity is deliberately narrow:

| Signal | Static status | Live adapter status |
| --- | --- | --- |
| swimming | SOURCE VERIFIED, client `0x60E0D4` player+`0x118`, movement+`0x40` mask `0x00200000`; runtime signature checked | IMPLEMENTED read-only; RUNTIME PENDING |
| breath timer current/max/scale/pause, fatigue | SOURCE VERIFIED **packet/event** layout only | SOURCE NOT VERIFIED as a fresh live reader; all fields unknown |
| submerged, surface, waterline/liquid type | SOURCE NOT VERIFIED coherent player evidence | unknown; not inferred from swimming/NavMesh |
| positive ground contact / water exit | SOURCE NOT VERIFIED | `DryOrNonSwimming` only, never `DryGround` |
| ascent start and matching release | SOURCE VERIFIED paired Jump **binding callbacks**, but swim-specific semantics SOURCE NOT VERIFIED | no command path implemented or invoked |

Mirror packet `0x1D9` START at `0x5E7A25..0x5E7A9A` reads type/current/max/
signed scale/paused/spell and forwards `MIRROR_TIMER_START` Lua event. `0x1DA`
PAUSE reads type/paused and forwards event `0x16B`; `0x1DB` STOP reads type
and forwards `0x16C`. `0x5E7B10` is **not** a timer getter: it resolves a spell
icon from a global spell table for the START event's spell argument. No
signature-validated native per-player timer store or polled Lua getter was
found. The preserved FrameXML MirrorTimer frames interpolate received events,
can be hidden/reset on world entry and are not verified as a fresh bootstrap
snapshot in the running client. A read-only parser (`MirrorTimerProtocol5875`)
decodes offline event bytes for tests only; no live packet interception exists.
Server-side scale `-1` drains underwater and `+10` refills at the surface, but
an event may be stale and breathing effects can suppress the timer. Therefore
neither active breath nor an old draining event is a current submersion proof.

Binary audit rechecked exact Jump registration pair at `0x8500B8`:
`Jump` start `0x513BD0` calls `0x60DEA0` -> `0x617930` queued event 7;
`Jump` stop `0x513D50` calls `0x60E080` -> `0x60E060` -> `0x617DE0`,
which queues event 0xE or 0xF depending on player+`0x9E8` bit `0x100`.
Those are source-backed callback/call/queue relationships, **not** a proven
hold-to-ascend / stop-to-release swimming contract. Start/stop remain
SOURCE NOT VERIFIED for *safe sustained ascent*, and no function is called.
No verified current-player liquid surface Z or positive ground-contact source
was found; swimming=false could mean falling, shallow water or transport.

The typed observer reports `Unknown`, `DryOrNonSwimming`, or
`SwimmingStateUnknown`; it requires two consecutive known movement samples
to promote a swim/non-swim transition and returns to Unknown immediately on
world/signature/read loss. This confirmation is diagnostic hysteresis, not a
new game-state claim. `WATER EVIDENCE` logs changes or every 30 seconds, with
raw flags and all unsupported fields explicitly `unknown`. The audit tool
separates source-verified, runtime-observed and inferred counts, and cannot
turn a mere swim-bit observation into surface/submerged/drowning PASS.

Latest existing full log was checked with `tools/water_log_audit.py`: zero water
events/encounters, final movement/runtime/idle/escalation/death counters all
zero. No WoW process was available for a new safe manual shoreline run, so
all **new** water signals remain RUNTIME PENDING. To qualify the observer,
launch a fresh GUI/client with `env WOW_INTERNAL_WATER_MODE=observe wine
./build/wow_gui.exe`, Start WoW, log in, then Start Bot; use only USER-MANUAL
movement at a safe shallow shoreline, then Stop Bot. Inspect `WATER OBSERVE`
and `WATER EVIDENCE` in the preserved full log. The bot supplies no movement,
no target, no input and no drowning test. A known swim transition would verify
the adapter at runtime, **not** surface, submersion, breath or ground exit.

P0.4.2 autonomous water behavior is not started; the source gaps are a safety
gate. The existing AFK, DeathRecovery, combat, navigation/cache and 2000/4/2
budgets are untouched.

Validation of this bounded checkpoint: TEST PASS, full dirty worktree 92
strict C++ tests, 26 audit Python tests, 13 QuestDB Python tests, SQL fixture
and eight Lua fixtures; results
`/tmp/wow-validation-0vostnxd/results.json`. BUILD PASS with both validator
and explicit `cmake --build build`; DIFF CHECK PASS. The staged-only exported
tree independently passed 43 published C++ tests, the same 26 audit Python
tests, SQL fixture, three published Lua fixtures and full MinGW DLL/GUI/loader
build; results `/tmp/wow-validation-vhvuu2a7/results.json`, tree
`/tmp/wow-p041-isolated.eZwvle`. The 13 local QuestDB Python tests belong to
unrelated unpublished work, so they are absent in that isolated tree; SQL
remains present and passed. Neither build constitutes a live water observation.
Follow-up log-audit correction: draining/refilling counts additionally require
`breathActive=yes`, so an inactive timer's stale scale is not live direction
evidence. Final post-correction TEST PASS: full 92 C++/26 audit Python/13
QuestDB Python/SQL/eight Lua, `/tmp/wow-validation-oz2svn1s/results.json`;
isolated 43 C++/26 audit Python/SQL/three Lua and complete MinGW build,
`/tmp/wow-validation-2ixqiaz9/results.json` in
`/tmp/wow-p041-final-isolated.ZmhzAw`. Explicit build and diff checks PASS.

2026-10-07. **Incomplete implementation checkpoint: SOURCE GAP.** The user's
sections31/48 stop gate applies. No production water movement, state reader,
owner, input, filter or watchdog change is enabled. Drowning prevention is NOT
implemented or qualified. Do not treat this audit as completion of P0.4.

## Verified baseline

Starting HEAD exactly `4ff143940b0f3e409742e00ab0e0840e4117b572`, branch
`codex/wow-internal-continuation`. Latest full log preserved before work:
`/tmp/wow-p04-baseline.yBnhXp/wow-internal.log`,137775lines, session
2000.134358700080773630.132596093.1776. Bot132596606->134880875ms:
38.07115minutes, user-requested stop. This is a longer capture than the initial
55-kill user summary: final kills67/lootOk67/deaths0. Final movementRecoveries0,
runtimeRecoveries0/runtimeStrategic0/runtimeIdleDeadlocks0/runtimeEscalations0.
Seven confirmed AFK actions. No new natural death gate occurred; preserve the
independent previously verified DeathRecovery/Ghost baseline.

P0.3 Navigation RUNTIME PASS; P0.3.1 Acquisition liveness RUNTIME PASS;
P0.3.2 NavMesh cache RUNTIME PASS in this capture. Cold route30, expanded60,
full614 actual additions=704;5085 cache hits total. Four warm full-map loads:
276.827/293.499/264.276/264.845ms,704hits each, zero reads/additions. Warm route
67 requests252.304–279.752ms; expanded4 requests253.081–253.503ms. Only session
teardown invalidated generation1. These are real client timings, not the older
offline probe. No water telemetry exists; absence is not a water encounter PASS.

## Signal-by-signal audit

Client SHA256 `b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
Addresses below are VAs in this exact PE32 image, base400000. Reproduce static
instruction/string checks without attaching to WoW:

```sh
python3 tools/water_client_audit.py '/home/ludvig/Games/WoW Vanilla/WoW.exe'
```

Local VMaNGOS files referenced below are READ-ONLY protocol/generator evidence,
not proof of the running server revision or client memory layout. Root:
`/home/ludvig/Programming/Projects/vmangos-core`.

| Candidate | Status / exact evidence | Encoding, limits and read-only use |
| --- | --- | --- |
| A swimming | SOURCE VERIFIED flag consumer: client60E0D4 loads player+118,60E0DA tests movement+40 mask00200000. Existing PlayerSnapshot reads +118; AFK reads +40. Server Objects/MovementInfo.h names the identical mask SWIMMING. | One movement mode, NOT head-under-water. A future reader must validate current identity/signature/read/freshness. No new runtime reader yet. |
| B submerged | SOURCE NOT VERIFIED as a coherent current-player snapshot. Server IsUnderwater is private environmental state, not a discovered replicated field. | Never infer from low Z, swimming alone or absent UI. Unknown remains unknown. |
| C surface / near surface | SOURCE NOT VERIFIED. No audited client waterline/head-height relation. | NOT swimming-without-breath by default; timer can be absent through breathing effects or missing events/UI. |
| D liquid surface Z | SOURCE NOT VERIFIED client field/function. Generator reads map liquid heights, but no live water-height adapter/version correlation. | NavMesh polygon Z cannot substitute, see topology proof below. |
| E current liquid type | SOURCE NOT VERIFIED live player signal. Mesh water/magma/slime flags SOURCE VERIFIED for topology only. | A polygon classification is not authoritative current-player liquid contact. |
| F breath active | SOURCE VERIFIED protocol/event, NOT a validated live snapshot. Client5E7990 handles1D9/1DA/1DB; type1 maps BREATH at5E7AF7. | Start/stop/paused events; active can include refilling at surface. Missing cached event is unknown, not inactive. |
| G breath current/max | SOURCE VERIFIED event payload:5E7A25..5E7A65 reads type,current,max,scale,paused byte,spell;5E7A75..5E7A9A forwards event16A. | Milliseconds from Server/Packets/Misc.h/.cpp. UI uses seconds. No native persistent timer address verified; UI live fields not runtime-validated. |
| H fatigue | SOURCE VERIFIED timer type0 maps EXHAUSTION at5E7AFD, same payload. | Distinct from breath; no ocean/fatigue traversal permission. No live snapshot yet. |
| I drowning damage | SOURCE VERIFIED server protocol: opcode1FC, Player.h DAMAGE_DROWNING=1, EXHAUSTED=0, FALL=2; Player.cpp expiration emits environmental damage. Client adapter SOURCE NOT VERIFIED. | HP loss is not damage-type evidence; no inferred drowning event generated. |
| J swim movement flags | SOURCE VERIFIED00200000 above; existing ghost10000000 evidence retained. | Pitch40/80, jump2000, far-fall4000, transport02000000 are separate server movement bits. Do not promote ambiguous server flying comments to verified client flight semantics. |
| K vertical input | Jump registration8500B8 ->513BD0 ->60DEA0 ->617930 queues event7 through617570. SOURCE VERIFIED call chain only. Safe sustained ascent AND deterministic release semantics SOURCE NOT VERIFIED. | Jump alone is NOT a proven bounded surface action. No native call or Space pulse added. |
| L dry/ground contact | SOURCE NOT VERIFIED positive contact signal. | Swimming=false does not prove grounded: falling/transport/shallow-liquid are counterexamples. No DryGround assertion from absence of swim bit. |

Unknown/stale representation for all unvalidated observations remains unknown;
no synthetic surfaceZ, breath maximum, current map or current timestamp is added.
The offline audit's addresses are NOT runtime function registrations or permission
to call them. No client-memory writes are performed by these tools.

## Breath semantics and the remaining snapshot gate

Disassembled5E7990's switch handles start1D9, pause1DA, stop1DB. Start uses only
packet locals before firing Lua event16A through703F50; this routine does not
publish an independently polled breath cache. Event-name table51B3DD..51B3F1
correlates START/PAUSE/STOP. Decoder5E7AE0 maps0/1/2 to
EXHAUSTION/BREATH/FEIGNDEATH; all other types map UNKNOWN. No modern
GetMirrorTimerInfo/GetMirrorTimerProgress/IsSubmerged API is assumed.

Server Player.cpp SetEnvironmentFlags: breath scale=-1 when underwater with a
breathing interval, +10 otherwise. Timer activation requires underwater plus
an interval. Deactivation requires leaving liquid or no longer alive. Thus a
surface swimmer may still have an ACTIVE REFILLING breath timer. Water-breathing
effects can remove the useful countdown. Positive scale/absence cannot establish
DryGround or a generally safe surface. Paused countdown is not safe breath.

Preserved [Vanilla MirrorTimer.lua](https://raw.githubusercontent.com/MOUZU/Blizzard-WoW-Interface/master/1.12.1/FrameXML/MirrorTimer.lua)
stores timer/value/scale/paused on visible frames, converts milliseconds to
seconds, and interpolates value in OnUpdate. World-entry hides/resets the
frame; stopped events clear it. This provides a candidate READ-ONLY UI probe,
not proof of the currently loaded/addon-modified UI, freshness, missing-event
bootstrap, or underwater status on this client. Its fields need live verification
before policy may rely on them. No event hook or guessed global is installed.

## NavMesh water audit: do not steer toward underwater floor

`Maps/MoveMapSharedDefines.h`: Ground01, Magma02, Slime04, Water08, Steep10.
`contrib/mmap/src/TerrainBuilder.cpp` constructs liquid triangles from map
heights. `TileWorker.cpp:544` rasterizes separate liquid spans;57–97 and635
classify SOLID terrain beneath those spans as water or shallow transition.
The final compact heightfield is built from `tile.solid` at639, not a guaranteed
liquid-surface sheet. Assignment763–778 yields ground01, transition09, water08,
magma02, slime04; steep ground remains11. Therefore a water poly may be submerged
terrain. Its height is NOT a verified breathable surface destination.

Existing provider uses per-query Detour filters: ground first, then ground|water09,
non-steep exclude10 first, validated steep fallback second. Projection commonly
uses09. Magma/slime are not positively included. The waterAwareRoute field means
the query ALLOWED water, not that every/any polygon is water or the player is
safe. The existing `usesLiquids` header is no live depth/surface promise.

P0.3.2 cache identity excludes query filters. Shared topology, private queries,
mutex-protected temporary hazard masking/RAII restore, route-local corridors and
meshGeneration provenance already support filter isolation. No topology reload,
hazard deletion, mask change or path acceptance change is made here. Existing
water fallback is pre-existing behavior, NOT newly qualified surface traversal.

## Implementation boundary and ownership

STOP before behavior per requested evidence gate. No new production FSM or
WaterSafetyOwner exists in this checkpoint. Proposed state names must not hide
missing evidence: a swimming-only observation would need SwimmingDepthUnknown,
not SurfaceSwimming. In particular Unknown cannot authorize ground recovery,
shore exit, underwater loot, movement, or drowning-safety claims.

Future integration must occur through WorldMonitor's shared ownership gates,
after authoritative world/life reads; DeathRecovery/ghost remain observe-only.
Preserve defensive combat target identity, dialog/vendor/input exclusivity and
existing AFK swimming rejection. Do not reuse a gameplay update `continue` that
starves AFK observations or silently resets acquisition/outcome watchdogs.
RecoveryController currently has no verified water gate; do not claim underwater
eating is prevented. Combat chase/loot/shoreline behavior likewise remains
unsupported rather than silently called safe. Capabilities are NOT enabled.

No vertical press is issued, so no new held-key lifecycle exists. Before any
future movement adapter, trace the queued Jump/event7 execution and its matching
release/cancellation through owner change, bot stop, world/DLL teardown. A paired
F12 driver cannot be assumed to provide persistent Space/ascent cleanup.

## Tools, tests and reproducibility

`water_client_audit.py` pins exact client hash/layout and checks the reviewed
instruction paths and event strings; rejects foreign/truncated PE, unbacked
reads and signature mismatch. It does not attach to WoW or invoke any function.
`water_log_audit.py` consumes the FULL preserved log, counts explicit transition,
breath, emergency, press/release, stale, terminal and abandonment events, and
retains session-separated cumulative existing counters (missing stays unknown).
No AFK ghost-waterwalk or NavMesh water message counts as a water encounter.
An ascent dispatch cannot count as surfacing or shore exit. Known transitions
require `known=yes`; confirmed releases require `result=confirmed`.

Automated counts never independently certify runtime safety:
waterRuntimeQualified=false, reason=no_verified_water_encounter or
manual_evidence_review_required. This is deliberate: paired counts alone do not
prove matching command identity, safe movement, release or full episode success.
The log schema is preparatory; no runtime producer is claimed implemented.

```sh
python3 tools/water_log_audit.py /path/to/preserved-full.log
python3 -m unittest discover -s tools/tests -p 'water*_test.py' -v
python3 tools/validate.py --jobs 4
cmake --build build
git diff --check -- .
```

TEST PASS full dirty worktree:91 strict C++ tests,22 audit Python tests (13 new),
13 QuestDB Python tests, SQL and8 Lua fixtures;
`/tmp/wow-validation-fev2rjt0/results.json`. BUILD PASS from full validation
and separate build; DIFF CHECK PASS. The existing Ninja log-recovery warning
appeared and the DLL rebuilt successfully; no build configuration changed.
Isolated staged export `/tmp/wow-p04-isolated.8Eot1v`: TEST PASS42 published
C++ tests,22 audit Python tests, SQL and3 published Lua fixtures; full DLL/GUI/
loader/testhost BUILD PASS. Results `/tmp/wow-validation-vwqoi5ni/results.json`.
QuestDB Python files belonging to unrelated dirty work are absent in the
isolated checkpoint; SQL remains required. Final result-recording doc edits
do not change tested source. No runtime behavior claim follows from these tests.
There are no behavior tests for unimplemented surfacing/shoreline ownership;
do not count parser tests as the user's38 water-behavior regressions.

## Runtime gates and safe continuation

All new water capabilities: RUNTIME PENDING. No running WoW/GUI process was
available during the audit. The38-minute baseline is ordinary pre-checkpoint
regression evidence only; there is no water encounter/submersion evidence.

Next bounded source task: establish coherent current-player submersion evidence
(or validate exact-client mirror event/UI bootstrap and freshness), and prove
the ascent-release path. A read-only probe must retain timer type/current/max/
scale/paused, player identity/life/movement, monotonic sampling, and world epoch;
absence after attachment remains UNKNOWN. It must not install experimental
movement, request packets, fake native flags or classify liquid Z from mesh Z.
Then implement/test the smallest safe subset and rerun isolated validation.

Only AFTER those source gates, runtime can use a user-selected safe shallow
shoreline or natural water. Do not start an automatic water qualification from
this checkpoint: it has no drowning protection. Never intentionally drown,
enter fatigue zones, provoke combat/death, or use a hardcoded test coordinate.
Dry->surface->shore->dry and any safe incidental submersion are independent live
gates. No submersion means drowning prevention RUNTIME PENDING even if surface
traversal later passes. No later roadmap phase begins here.
