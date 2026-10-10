# P0.2 / P0.2.1 Combat reliability audit

## Endurance incident: unconfirmed bootstrap had no failure owner (2026-10-10)

**Verified failure transition FIXED / TEST PASS / BUILD PASS; live combat recovery
RUNTIME PENDING. Stun causality UNKNOWN.** The complete
[endurance audit](GRIND_ENDURANCE_INCIDENT_2026-10-10.md) separates user observation,
raw events, source correlation and unknowns. Starting/deployed source `4019517`;
capture hashes pass, exact loaded-DLL binding is still missing.

Two fights against entry 12856/GUID `0xF130003238007E60` reach selected, aligned
melee and issue a bootstrap (raw 8449/36851). Verification fails at 9235/37762,
target HP stays 100 and player HP drops to death. The full action script returns
`unknown_frame_iteration_limit`, as it also does in successful fights; the
separate bootstrap input probe is ready. No stun-on/off/swing/error trace proves
why Attack was refused. RecoveryController does not own these fights, target
validity/selection and chase handoff are corroborated, and brief health-snapshot
staleness clears. The first failed fight precedes living recovery; an intervening
living defense fight successfully damages the same attacker. This rules out a
blanket living-owner suppression explanation, not every possible client failure.

Source chain: dispatch spends one repair, `attackStarted_` awaits verification;
unknown full action/input blocks ordinary liveness repairs; expiry only clears
the pending boolean and logs failure; initial startup requires zero repairs;
global stationary-melee recovery defers to the watchdog. The structural pending
timer expires normally, but no owner consumes the failed initial attempt.

`CombatBootstrapVerificationPolicy` now freezes the existing max(8 seconds,
two recorded swing periods + 1 second) verification window at dispatch. Failed
or rejected start emits `bounded_terminal_handoff` and calls the existing melee
terminal resolver with `initial_attack_unverified`. Hostile/unknown engagement
retains existing bounded defensive containment; unknown input can correctly
produce terminal input-conflict failure. Fresh same-selected-GUID damage or
known active Attack confirms; stale/unknown observations cannot extend the
deadline. Death, new target and reset discard old verification. Repair budget,
UI/cast/native/range/facing guards and action scripts are unchanged. No blind
retry, synthetic stun bit or read-only-evidence input authorization is added.

Failure logs include existing execution evidence; unknown Attack/cast/GCD state
is no longer rendered as a negative observation. This is a bounded failure fix,
not a promise to resume attacks after stun or survive an unsafe reclaim. Source
gap remains around live loss-of-control/attack acceptance and persistent broad
frame-scan exhaustion; they are explicit release blockers, not guessed APIs.

New deterministic replay covers the observed watchdog/bootstrap mismatch,
deadline and rejection handoff, proof guards, slow-weapon timing, invalidation,
unchanged cast protection and bounded containment failure. Full validation:
113 C++ tests, 55 Python tests, SQL fixtures, 11 Lua suites, DLL build/diff check;
separate full build PASS. Report `/tmp/wow-validation-d_o2gs0_/results.json`.
Historical P0.5.5 and P0.5.8 scoped PASS are retained. **READY FOR P0.7: NO.**

## P0.5.3 post-containment release runtime requalification prep (2026-10-08)

P0.5.8 is now RUNTIME PASS per the user-reported preserved
`wow-p058-runtime-success.log`: natural death, unsafe directed edge learned
and retained across corpse variants, ghost reclaim at 6.301 yd, two fresh
alive probes, resurrection and autonomous Combat/Grind resume. Its full
route→expanded→full-map inheritance remains static-qualified only. P0.5.5
ordinary combat initiation is RUNTIME PASS. P0.5.3 release remains RUNTIME
PENDING; no WoW/GUI is launched for this preparation checkpoint. P0.6 is
STATIC PASS / RUNTIME PENDING; P0.7 is planned, not started.

Call graph: bounded offensive/chase terminal evaluation enters
`BeginDefensiveContainment` when engagement is active or unknown. A successful
structural disengagement observation in `UpdateDefensiveContainment` enters
`BeginPostContainment`, then a fresh later snapshot goes through
`ResolveMeleeTerminal` and `CombatTerminalPolicy::Observe`. Active/renewed
hostility returns to the same bounded containment episode; DeathRecovery
preempts from WorldMonitor. A successful optional release blacklists the same
GUID without kill/loot credit and calls `BeginAcquire`; mandatory release
returns a typed owner failure. Neither route arrival nor command dispatch
is disengagement or release proof.

Source inspection found a provenance gap: the old episode Attack ownership
flag was set by observing any active Attack action, even one selected before
the bot's episode. The containment entry also called `Stop()` without a
fresh active, bot-issued provenance check. Telemetry alone could not satisfy
the requested no-unowned-StopAttack safety invariant. The flag now resets at
each acquisition boundary and is established by a same-GUID bot-issued
Attack command, not by observation or Charge alone. Containment stops Attack
only with that provenance and fresh active readback; an active unowned or
unknown owned latch fails closed. Post-containment stop/clear likewise needs
both provenance and fresh active evidence. A known inactive latch, or unknown
Attack with no bot-issued command, may use the existing exact-GUID guarded
`SetTarget(0)` selection-only path after verified disengagement and the
independent safe UI/cast probe. An active action without bot provenance cannot
be cleared as if it were owned. The client-selected GUID and replicated
`UNIT_FIELD_TARGET` remain separate; both must clear on a later snapshot.

Sparse `COMBAT CONTAINMENT RELEASE` eligibility/verify/command events now
include direct-aggressor count, attack-command provenance, three-state attack
ownership, selection-probe reason, selected GUID, decision, and actual
dispatch result. Existing post-release completion, typed failure and
Combat state transition logs complete the future runtime chain. Broader
legacy `AutoAttackController::Stop()` calls outside this containment/release
branch remain outside this narrow checkpoint; no claim is made that every
manual user Attack interaction is globally ownership-isolated.
P0.5.3 preparation is SOURCE VERIFIED / TEST PASS / BUILD PASS /
DIFF CHECK PASS for the full worktree and isolated intended tree; the rare
selection-only release remains RUNTIME PENDING until naturally observed.

## P0.5.5 offensive input evidence and melee progress (2026-10-08)

Starting HEAD `c59c603233f71bb7d22d67e2f40b0b61eb936665`. Latest live
run: P0.5.4 selection progression, Charge fallback and chase-to-melee handoff
are RUNTIME PASS, but first offensive input is RUNTIME FAIL. At 0.388 yd the
locked native selection and target snapshot were fresh, yet both
`actionEvidence` and reused `alternateInput` were `unknown`; 19+ combat starts,
zero Attack/Charge commands and zero kills. P0.5.4 overall RUNTIME FAIL.

The old Lua script collapsed missing `UIParent`, life APIs, `CastingBarFrame`,
`SpellIsTargeting`, `EnumerateFrames`, generic frame methods, action/cooldown
APIs, frame-iteration exhaustion and pcall/readback failures into `unknown`.
The log cannot select one cause. New typed reasons preserve UNKNOWN as
fail-closed and log only on probe-reason change. Local build-5875 binary
registration strings verify the API names; the preserved
[Vanilla 1.12.1 CastingBarFrame source](https://github.com/MOUZU/Blizzard-WoW-Interface/blob/master/1.12.1/FrameXML/CastingBarFrame.lua)
sets `casting`/`channeling` on SPELLCAST events. Those source facts do not
prove that a particular live probe will be READY; runtime remains pending.

`ProbeCombatBootstrapInput` is separate from release. It checks living player,
visible UIParent, named Vanilla modal frames, targeting cursor and cast-bar
state without requiring the broad `EnumerateFrames`/keyboard-handler scan or
Attack-slot/cooldown readback. Missing evidence remains typed UNKNOWN. The
release probe and P0.5.3 semantics remain strict. The bootstrap command
rechecks native same-GUID selection, fresh living player/target, melee range,
facing and pacify flag on the game thread, and its Lua command rechecks modal,
cast and targeting evidence. The no-slot `bootstrap` command now uses
`AttackTarget` instead of the formerly reachable `StopAttack` branch. An
issued command spends an existing repair attempt and is not success: fresh
active Attack or target HP decrease confirms; otherwise existing bounded
observation/supervisor behavior remains authoritative.

`CombatInitiationPolicy` now allows Fighting physical range reconciliation
when action/bootstrap input is UNKNOWN, while forbidding offensive commands.
The unchanged 5/6-yd hysteresis still controls Fighting-to-Chasing and
Chasing-to-Fighting. Known cast/modal blockers continue to hold input; target
loss/death and player death preempt via existing ownership. No Charge window,
32-tick supervisor threshold, 160-tick owner timeout, three-repair limit,
containment limit or navigation budget was increased. P0.5.5 static:
TEST PASS, BUILD PASS, DIFF CHECK PASS; runtime PENDING. Separate later DeathRecovery observation:
`route_scope_failed` / `surface_recovery_exhausted`, not changed here.

## P0.5.4 combat initiation / Charge-facing regression (2026-10-08)

Starting published HEAD `e74f78679f95ca8f6d8888be3364d2c232a0ed65`.
The latest dirty-worktree natural run selected and locked optional entry
3426/GUID `0xF130000D620037CE` near 19 yd. `WarriorChargeFacing` logged one
aligned sample; Charge and Attack counters stayed zero. At ~8 yd the
unchanged 32-tick supervisor performed recovery 1/3 and forced chase. The
chase physically closed to ~3 yd but did not hand off at the existing 5-yd
envelope. Recovery 2/3 forced Fighting, still with no Attack; recovery 3/3
eventually entered containment. Player/target HP stayed 664/100. The later
post-containment timeout is secondary to this first-action failure. Earlier
P0.5.1 long-run Charge episodes had a second aligned sample and issued Charge.

Source attribution: published `CombatController::Update` invokes
`ObserveMeleeLiveness` before Charge-facing and chase, then returns before
their FSMs when its action, selection, health, or input readback is unknown.
The runtime has no `COMBAT LIVENESS` classification transition, consistent
with the initial `UnknownOrStale` class persisting, but lacks the sub-evidence
needed to identify which read failed. P0.5.3 changed only release-side
selection probing; the normal offensive `ProbeCombatAction` script and this
early return predate it. The dirty worktree changes pull ranking, adds
diagnostics/chase GUID checks, and includes quest handoff work; none creates
the early return. The target remained locked and later native UI selection
matched; the periodic WorldState target=0 was an older snapshot, not proof of
a persistent UI selection failure.

P0.5.4 keeps the melee watchdog observational during Charge-facing/chase
when only Attack readback is unknown. Fresh target and native selection
evidence remain required; known cast/dialog/input blockers still hold.
Offensive input requires a positive safe probe, independent of physical chase
progress. Charge-facing alignment is reset on selection mismatch and requires
two fresh selected/aligned samples; an unavailable Charge falls back to
existing chase without increasing the six-tick window. ChaseController keeps
its 5-yd entry/6-yd resume hysteresis. At verified melee handoff, Fighting
waits for safe input before its existing bounded Attack startup path. Sparse
`COMBAT INIT GATE` telemetry now reports which evidence holds startup and
whether state progress or offensive input was authorized. The 32-tick
supervisor threshold, three recoveries, combat repair budget, water guard,
navigation/AFK/DeathRecovery and P0.6 vendor behavior are unchanged. A
separate `bootstrap` Lua mode is allowed only for the first same-GUID melee
start after a positive UI/cast input probe, fresh game-thread GUID/health/
range/facing checks, and two aligned facing samples. It uses the established
idempotent Attack-slot/`AttackTarget` action path without depending on a full
120-slot cooldown readback; the ordinary `start`/`refresh` and post-containment
release modes are untouched. The bootstrap consumes one existing repair
attempt and dispatch alone is never offensive-progress proof.

P0.5.4 static validation: TEST PASS, BUILD PASS, DIFF CHECK PASS. Full dirty
worktree: 99 strict C++ tests, 26 audit Python, 13 QuestDB Python, QuestDB SQL
fixture and nine Lua fixtures PASS.
Fresh published-HEAD-plus-exact-staged isolated tree: 50 strict C++ tests,
26 audit Python, QuestDB SQL fixture, four published Lua fixtures and complete
MinGW build PASS. P0.5.4
runtime: PENDING; no WoW/GUI launch is authorized here. Current full
dirty-worktree ordinary initiation: RUNTIME FAIL. P0.5.3 remains STATIC PASS,
while release runtime qualification cannot be accepted until normal initiation
works. P0.6 remains STATIC PASS / RUNTIME PENDING. P0.4 remains PAUSED.

## P0.5.3 post-containment release command eligibility (2026-10-08)

Starting published HEAD `d8b88caa333b493da8fae91fee903e7791ac606b`.
The natural P0.5.2 follow-up on optional entry 3426/GUID
`0xF130000D620037CE` entered containment, verified disengagement, and entered
post-containment release. Attack remained unknown, selected GUID and player
`UNIT_FIELD_TARGET` remained that GUID, target HP and player HP stayed 100/664,
and cumulative attack/charge commands stayed zero. No release command was
eligible: the terminal policy required the Attack probe's `inputSafe` and
`attackKnown`, and the old atomic abandon script also required an identified
Attack action slot. Waiting consumed the unchanged four-second release bound
and ended in `post_containment_release_timeout`. P0.5 normal long-run is
RUNTIME PASS; P0.5.1 containment entry/disengagement and P0.5.2 release entry
and bounded unknown observation are RUNTIME PASS; actual release and overall
P0.5.2 are RUNTIME FAIL. P0.5.3 runtime is PENDING.

Source audit: the local WoW.exe SHA256 remains
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
SetTarget 0x493540 accepts GUID zero: 0x4938F3 calls the native clear helper
0x493910, which clears B4E2D8/DC at 0x4939D3/0x4939D9 and serializes the
zero selection GUID through the ordinary selection packet path at
0x493A49..0x493A64. The new command checks the branch and packet-path bytes
before invoking this existing native function on the game thread. It also
rechecks exact native UI selection, live target/player health, complete
aggressor enumeration, no combat flags, no target victim linkage, and a
separate read-only UI/cast input probe at dispatch time. It never clears an
unrelated selection. The 1.12.1 server's HandleSetSelectionOpcode calls
Player::SetSelectionGuid, which writes both selection and UNIT_FIELD_TARGET;
Unit::Attack and AttackStop also write UNIT_FIELD_TARGET. Thus its residual
GUID is multiplexed replicated target state, not exclusive proof of a live
Attack latch. A fresh post-command zero read remains mandatory; a stale
nonzero value is observed then fails typed, never silently accepted.

Per-locked-GUID attack ownership is now established only after this controller
dispatched an Attack command or observed an active Attack action. The captured
chase never reached either; global cumulative counters are not used as an
episode proof. For such an episode, a separately safe selection probe permits
one guarded native clear without an Attack slot. If Attack ownership was
established, the existing action-slot-based stop and Attack-off verification
remain required; unknown Attack is never treated as off, and lack of a
source-verified stop remains fail-closed. A renewed hostile cancels passive
release within the same unchanged 30-second containment episode; death still
hands to DeathRecovery. Optional completion keeps the existing 120-second
same-GUID blacklist with no kill/loot credit; mandatory completion returns
owner-visible failure. Combat 3-repair/1-refresh limits, AFK, P0.6 vendor,
P0.4-TEMP water avoidance, and navigation budgets are unchanged.

P0.5.3 static status: SOURCE VERIFIED, TEST PASS, BUILD PASS, DIFF CHECK
PASS. Full dirty worktree: 98 strict C++ tests, 26 audit Python tests,
13 QuestDB Python tests, SQL fixture and nine Lua fixtures PASS. Fresh
published-HEAD-plus-staged isolated tree: 49 strict C++ tests, 26 audit
Python tests, QuestDB SQL fixture, four published Lua fixtures and complete
MinGW DLL/GUI/loader build PASS. P0.5.3 RUNTIME PENDING; no WoW or GUI was
launched in this checkpoint.

## P0.5.2 post-containment safe release (2026-10-08)

Starting published HEAD `e1625db3b791963194008e6fb878a4295e0531a4`.
The natural P0.6-session chase of optional entry 3426, GUID
`0xF130000D620037CE`, exhausted the existing 3/3 chase-recovery budget.
P0.5.1 entered DefensiveContainment and verified disengagement for the
structural interval: playerCombat=no, targetCombat=no, targetVictimGuid=0,
complete live aggressor scan clear, player HP 664, target HP 100. The native
UI selection and replicated player victim still named that GUID, while the
Lua Attack-action probe was unknown. `ResolveMeleeTerminal` set `inputSafe`
from that unknown probe and the terminal policy immediately returned
`terminal_action_input_conflict`; CombatController Failed and stopped the bot.
Thus P0.5.1 containment entry and disengagement are RUNTIME PASS, but the
post-disengagement release and overall P0.5.1 are RUNTIME FAIL before this
patch. P0.5 normal long-run remains RUNTIME PASS; P0.2.1 hard-refresh timing
remains RUNTIME PASS. P0.6 is STATIC PASS / RUNTIME PENDING.

Build-5875 native UI selection comes from the validated client selection
cache; player `UNIT_FIELD_TARGET` is the replicated attack-victim field, not
UI selection. A residual own victim does not itself prove a live hostile when
combat flags, target victim and complete aggressor enumeration are clear, but
it must clear before optional abandonment. The Attack probe's `unknown` is
neither inactive nor permission to issue input. The new target-bound
post-containment release phase observes fresh evidence for at most four
existing 1-second structural windows, within the unchanged 30-second
containment cap. It waits through unknown action/input evidence, issues the
existing game-thread-guarded own-GUID stop/clear at most once when permitted,
and requires a later snapshot proving native UI selection, replicated own
victim and Attack all clear. An unrelated selected GUID is never cleared.
Renewed hostile evidence returns to the same bounded containment episode;
verified death remains a DeathRecovery handoff. Optional release blacklists
the same GUID without kill/loot credit; mandatory release returns typed owner
failure. Release timeout is explicit, not silent success. P0.4 swimming is
PAUSED and P0.4-TEMP ground-only water avoidance is unchanged.

P0.5.2 static status: SOURCE VERIFIED, TEST PASS, BUILD PASS, DIFF CHECK
PASS. Full dirty worktree: 98 strict C++ tests, 26 audit Python tests,
13 QuestDB Python tests, SQL fixture and nine Lua fixtures PASS; results
`/tmp/wow-validation-e_z2sbcc/results.json`. Fresh published-HEAD-plus-staged
isolated tree: 49 strict C++ tests, 26 audit Python tests, QuestDB SQL fixture,
four published Lua fixtures, and complete MinGW DLL/GUI/loader build PASS;
results `/tmp/wow-validation-jl_zqa0v/results.json`. P0.5.2 runtime remains
RUNTIME PENDING; no WoW or GUI was launched here.

## P0.5.1 active-aggressor defensive containment (2026-10-08)

Starting HEAD `cdaa0486a74d29a5c952acbb072a85e474f51288` was verified.
The newest natural ~98-minute Grind run made 132 kills and 131 successful
loots, with one successful natural DeathRecovery, zero movement recoveries,
zero idle deadlocks and zero runtime escalations before its terminal combat
failure. Target `0xF130000CD400514A` was selected and was the server victim;
the target victim was the player, both combat flags and Attack were active,
range was ~1.68 yd and facing was valid. Target HP stopped at 80 while player
HP fell to 405. One hard refresh stopped Attack, then a same-target reengage
restored the structural latch. A fresh active offensive window still produced
no damage. `bounded_same_target_recovery_exhausted` led to `system_fail`,
CombatController Failed and a whole-bot stop. This is not a recurrence of the
old stale post-reengage clock: the full observation window elapsed.
P0.5 normal long-run is RUNTIME FAIL for this terminal event. The P0.2.1
hard-refresh/post-reengage observation timing is RUNTIME PASS, while this
new active-aggressor containment branch remains RUNTIME PENDING.

The logged `repairAttempts=2` counted dispatched repair actions (hard refresh
and reengage), not a claim that the maximum-three repair-dispatch ceiling was
reached. The one-hard-refresh branch can terminate after a full unsuccessful
post-reengage offensive window while only two dispatches were spent. Telemetry
now says `repairDispatches=2 repairLimit=3 hardRefreshLimit=1`; no limit or
observation window changed. The terminal's `terminal_owner_not_recoverable`
attribution also came from excluding temporary Grind whenever the unrelated
Vile Familiars quest-status flag was active. Temporary Grind ownership now
takes precedence unless a planner quest target actually owns the episode.

An exhausted active/unknown hostile enters target-bound
`DefensiveContainment` before passive abandonment or whole-bot failure. It
preserves the same GUID, stops own attack and CTM once, verifies Attack has
stopped, and uses one short deterministic away-point through the existing
Detour follower. The source-verified live object-manager aggressor count must
match the snapshot positions used for the away vector; incomplete threat
geometry fails closed. The follower uses the production-default living
Ground-only/Water-excluding query and existing terrain/hazard validation;
no water, navigation or combat budget was expanded. Navigation initialization
and route execution own movement liveness while containment is active.
Command dispatch, route arrival or distance alone never proves success.

Authoritative disengagement requires complete native aggressor enumeration,
no player combat flag, no live aggressor and no target victim linkage for a
continuous structural verification window. Only then does the existing
guarded terminal-release protocol clear/verify own Attack and selection,
blacklist a genuinely optional GUID for the existing 120 seconds, or return
a typed mandatory owner failure. A target already selected/victim/Attack-clear
after escape is verified without issuing a duplicate clear. There is no kill
credit. Verified target damage can re-enter the same fight with a fresh
offensive observation clock; mere escape/reselection does not refund repairs
or permit another containment episode. Natural death preempts to the existing
DeathRecovery path without blacklisting. An unsafe/failed route, unconfirmed
Attack stop, water block or elapsed 30-second containment bound remains an
explicit `defensive_containment_exhausted` failure, not silent idle.

This checkpoint is SOURCE VERIFIED, TEST PASS, BUILD PASS and DIFF CHECK PASS
based on earlier validation; final validation of the last ownership hunk is
recorded separately below.
Full dirty-worktree validation passed 96 strict C++ tests, 26 audit Python
tests, 13 QuestDB Python tests, SQL fixture and eight Lua fixtures; results
`/tmp/wow-validation-4jwcmhuh/results.json`. The isolated intended staged
tree passed 47 strict C++ tests, 26 audit Python tests, SQL fixture, three
published Lua fixtures and complete MinGW DLL/GUI/loader build; results
`/tmp/wow-validation-0j4fb46l/results.json`, including the final native-health
freshness guard. A normal GUI launch attempt opened with WoW stopped; its
Start WoW control did not start a client/gameplay session. The GUI launch
replaced `build/wow-internal.log` (previously ~20 MB) with a 253 KB startup
log; no copy of the former full log was found under `runtime-captures/` or
`debug/`. The terminal episode above was inspected before that launch.
The active-aggressor containment runtime branch is RUNTIME PENDING until a
natural episode exercises it. P0.4 autonomous swimming remains PAUSED.

Final post-ownership-hunk validation: full dirty worktree TEST PASS (96 strict
C++ tests, 26 navigation/water audit Python tests, 13 QuestDB Python tests,
SQL fixture, eight Lua fixtures), results
`/tmp/wow-validation-vj7xyv1l/results.json`. Fresh HEAD-plus-exact-staged
isolated tree `/tmp/wow-p051-final.4Yrndf` TEST PASS (47 strict C++ tests,
26 navigation/water audit Python tests, QuestDB SQL fixture, three published
Lua fixtures) and complete MinGW DLL/GUI/loader BUILD PASS; results
`/tmp/wow-validation-h2xbh6j7/results.json`. Route arrival is neither
disengagement nor failure. WorldMonitor attributes movement liveness only
while the defensive follower is actually planning or moving; the policy's
unchanged 30-second bound covers stationary combat/disengagement observation.
DIFF CHECK PASS. P0.4-TEMP living-water avoidance remains preserved.

## P0.5 chase/terminal closure (2026-10-07; static validation in progress)

Starting HEAD `bf08ec65c53edcb751d6c37da5533c5b00e2cb9b` was verified.
The historical natural optional chase of GUID `0xF130000CCD005092`
exhausted three hard-stall recoveries while its distance changed
23.4473 -> 21.1605 -> 14.5037 yards; it received the existing 120-second
GUID blacklist and the bot continued. The old global-recovery branch decided
to abandon from `targetVictim != player` alone. That did not prove the absence
of another aggressor, a native combat flag, mandatory ownership, or a safe
post-command release. It was an unsafe ownership shortcut despite a useful
observed outcome.

P0.5 routes exhausted chase through the same read-only combat execution
evidence and guarded terminal protocol as exhausted offense. Optional Grind
targets are released only after complete known aggressor enumeration, no native
combat/victim linkage, stable player HP, safe input, exact selected GUID, and
verified Attack/selection release. Death, water pause, world/input unknown,
active hostile or another owner cannot silently become optional abandonment.
The same GUID is blacklisted for 120 seconds only after verified release; no
kill is credited. Mandatory planner combat can produce an owner-visible typed
failure after the same safe release; otherwise systemic/active-hostile failure
remains explicit. No repair count or navigation safety budget was enlarged.

Direct chase now uses target-relative physical progress: destination-distance
gain or displacement toward the locked target. CTM dispatch, replanning, and
lateral displacement do not refund the 8/16-tick liveness windows. Navigation
initialization/long approach remain outside the direct ChaseController clock;
the first Combat chase lock starts a new epoch. Typed chase reasons distinguish
invalid/unreachable target, approach-plan rejection, movement-command rejection
and exhausted physical progress. Target unload and invalid max-HP no longer
count as verified kills. The P0.2.1 3-repair/1-refresh and fresh post-reengage
offensive window remain unchanged. Read-only swing timer and evade state remain
UNKNOWN; no synthetic execution evidence was added.

Initial native target selection is also bounded to the existing five-failure
count at the existing eight-tick retry cadence. Reissued SetTarget is never
treated as selection proof: only `ClientSelectedGuid == pendingTargetGuid`
completes it. At the bound, an optional pending target may be blacklisted only
if fresh complete native evidence proves no combat/aggressor/own Attack; a
mandatory planner target returns a typed owner failure under the same guard.
Active/unknown combat fails explicitly instead of abandoning a hostile or
retrying forever. Server victim alone never confirms UI selection.

P0.4 remains PAUSED; P0.4.1 observation and P0.4-TEMP living-water block are
unchanged. Full-tree 95 C++ tests and isolated staged-tree 46 C++ tests,
Python/SQL/Lua fixtures, MinGW build and diff checks passed; exact artifacts
are in the project state. The GUI runtime attempt found no running WoW client
and did not start one, so P0.5 remains RUNTIME PENDING. A new natural
deep-offense terminal episode is required to call that rare branch RUNTIME
PASS.

## P0.2.1 deep-stall checkpoint (2026-10-07)

SOURCE VERIFIED. New refresh/terminal behavior RUNTIME PENDING. Existing P0.2
natural latch repair and normal 20-minute regression remain RUNTIME PASS.

The newest full log is session968.134358579023063750.120736874.1760, preserved
read-only at `/tmp/wow-combat-deep-baseline.z2gL3T/wow-internal.log` before edits.
It ends in combat failure, not a navigation stall. Target entry3461 / GUID
`0xF130000D85003A7A` had HP100->95->82->69->59->47->36->16->8.
Full-episode semantic reconstruction (line numbers in that preserved capture):

| Lines | Target / player HP | UI selection / server victim | Latch / classification | Repair / verification |
| --- | --- | --- | --- | --- |
| 37845-38219 | 100->95->82 / 424->438->421 | same GUID / same GUID | yes once melee; facing correction | ordinary damage |
| 38350-38598 | 82 / 402->385 | same GUID / 0 | no; facing then latch desync; noDamage2112ms | same target retained |
| 38610-38650 | 82->69 / 385 | same GUID / restored same GUID | yes, healthy | reengage attempt1; structural confirmation AND observed damage |
| 38705-39340 | 69->59->47->36->16->8 / 366->348->327->309 | same GUID / same GUID | productive melee | no additional repair needed |
| 39698-39716 | 8 / 291 | same GUID / same GUID | yes; offensive_no_progress4296ms | hard_stall_refresh attempt1, pending |
| 39723-40057 | 8 / 291->253 | same GUID / 0 | no; latch desync4652->8638ms | refresh waiting, no damage |
| 40068-40103 | 8 / 253 | same GUID / restored same GUID | yes; noDamage8994ms | reengage attempt2 structurally confirmed; immediate bounded failure |
| 40105-40126 | 8 / 253 | same lock retained until cleanup | Fighting->Failed | WorldMonitor session stop, DLL unload |

Crucially,8994-8638=356ms between the last inactive sample and the restored
latch/terminal sample. The capture does NOT prove several failed active swings
after the final reengage. Most of that apparent final window was Attack OFF.
Player HP loss establishes urgency, but not the identity of the damaging actor.
`lowHpHardStallRecoveries=0` is expected here: ForceAutonomyCombatRecovery
defers aligned Fighting to the same-target watchdog BEFORE reaching its legacy
finisher counter. The new repair path increments autoAttackLivenessRecoveries,
not that global/finisher counter. Counters are not relabelled or reset to hide
this episode; terminal and damage verification remain separately observable.

### Proven refresh and eligibility defect

Old `CombatActionEvidenceScript` refreshed an active Attack with UseAction,
then tested IsCurrentAction again in the SAME Lua execution to decide whether
to start. That assumes synchronous latch publication. The live post-refresh
latch/victim were OFF/0; this proves the refresh did not leave an active attack,
but does not independently prove the internal same-script publication timing.
A delayed-publication Lua regression reproduces the unsafe assumption.

Old policy waited the entire damage window for RefreshAttack even when fresh
evidence already showed Attack off. Its continuous-eligibility clock required
range/facing/input but NOT selected identity/known active Attack. Thus inactive
time consumed the supposed post-refresh execution opportunity. Once the final
latch was restored, refreshed=true plus old no-damage age immediately selected
Fail. The failing-before-fix C++ replay asserted immediate bounded reengage after
the observed stop; the old implementation failed that assertion.

Minimal correction: one explicit stop stage, then a FRESH inactive observation
authorizes the existing guarded/idempotent start on a later tick. No same-script
toggle pair. The no-damage clock and spent repairs are retained; the execution
eligibility window requires correct selection and known active Attack. No budget
refund for latch restoration. Refresh success still requires target damage/death,
even if the stop stage handed off to structural reengage. MaximumRepairs3,
one refresh,1s structural verification,4s/8s period-aware windows are unchanged.

### What remains UNKNOWN / source-backed execution evidence

The initiating4.296s HP8 pause is not explained by the old capture: no target
victim, pacify/non-attackable flags, dynamic flags or actual swing events were
captured. Do NOT label this target evading/immune/unreachable by guesswork.
Read-only local VMaNGOS5875 protocol evidence:
`Objects/UpdateFields_1_12_1.h` identifies full-descriptor offsets target40,
health58/max70, faction8C, flagsB8, base attack period1F8, dynamic flags23C.
`Objects/UnitDefines.h` identifies IN_COMBAT80000, PACIFIED20000, non-attackable
SPAWNING2/NOT_ATTACKABLE_180/IMMUNE_TO_PLAYER100/NON_ATTACKABLE_210000/
NOT_SELECTABLE02000000/IMMUNE80000000. These bits are diagnostic, not an evade flag.
`AI/CreatureAI.cpp::EnterEvadeMode` clears server threat/combat and routes home;
it does not prove a dedicated replicated client evade bit.

VMaNGOS attack timers are private server state; the replicated base period is
not an executed swing/next-swing timer. Protocol swing/error opcodes exist, but
no verified client event adapter is present in this checkpoint. Telemetry keeps
`attackTimer=unknown evade=unknown`, never fabricates execution or error success.
New game-thread execution readbacks validate manager/player/target identity and
read both victims, HP/maxHP, unit/dynamic/movement flags and faction. Native
object enumeration is bounded4096 and must complete with the actual player
present; unreadable descriptors/links or incomplete enumeration leave aggressor
absence UNKNOWN, not clear. This does not invent hostility from faction IDs.

### Owner-aware terminal result

Optional Grinding alone may abandon only after authoritative combat-clear,
no target/other live victim linkage to the player, no remembered aggressor,
healthy input/UI/cast/world evidence and1s stable player HP. Native combat flag
set, a live aggressor, falling HP or unknown evidence blocks abandonment.
One fresh game-thread guarded stop-own-Attack/ClearTarget is permitted only
while actual UI selection still equals the locked GUID; fresh native safety is
rechecked at dispatch. Then native UI selection0, server victim0 and observed
Attack inactive must confirm within1s. Dispatch is not success. Same GUID gets
the existing120-second blacklist; no kill/loot/success accounting. BeginAcquire
re-arms only after verified release; existing five-consecutive-failure bound
remains. Death suspension/reset clears this pending terminal episode.

Quest-mandatory targets never silently abandon. Existing defensive ownership
remains while the original bounded repairs run. Once those repairs genuinely
exhaust with an active unresolved hostile, there is no source-proven new safe
escape here: explicit `unresolved_hostile_after_bounded_repair` system failure
is retained, with evidence proving why abandonment was unsafe. Unknown/input-
critical failure performs no new target-release input. WorldMonitor's genuine
Failed->session-stop policy is NOT blindly suppressed; recoverable optional
abandonment instead never enters Failed. No random replacement, movement,
packet fabrication, timer writes, extra attack repairs or combat-owner bypass.

### Validation / runtime gate

Strict replay/Lua regressions cover attack-off time, prompt post-stop reengage,
356ms not being an offensive period, sustained active-window terminal failure,
actual damage-only refresh verification, asynchronous latch publication,
idempotent start and guarded one-shot release. `combat_terminal_policy_test`
covers safe optional release, each postcondition, mandatory/unknown/hostile/
input/identity/health blockers, bounded timeout and re-arm. Existing AFK/death/
navigation regressions remain in full validation. Current full validation:
88 strict C++ tests PASS, Python/SQL and eight Lua fixture programs PASS;
Final `/tmp/wow-validation-h3q8sgiv/results.json`. Isolated publication tree:
39 published C++ tests, six audit Python tests, SQL fixture, three published Lua
programs and all build targets PASS (`/tmp/wow-validation-_lbw4d7f/results.json`).
Unpublished local fixtures remain unstaged. Separate build and diff checks PASS.

No WoW process is currently running. New behavior RUNTIME PENDING. Fresh normal
Grinding >=20min, full log preserved, no intentionally induced stall/death.
Need ordinary fights unaffected, natural same-GUID repair followed by damage;
for a deep stall, verified safe abandonment OR explicit evidence that an active
hostile made terminal failure necessary. A working latch is not damage success.

Navigation P0.3 key fixes RUNTIME PASS in this observed~12.5min session:
movementRecoveries0/runtimeRecoveries0/runtimeEscalations0; advancing initializers
survived. Full20–30min gate interrupted by combat, not marked passed. Navigation,
AFK and DeathRecovery files/policies/budgets2000/4/2 are unchanged here.

## Historical P0.2 implementation audit

2026-10-07. SOURCE VERIFIED; focused tests PASS. New behavior RUNTIME PENDING.
No WoW process was running during this implementation. Do not manufacture a
dangerous desync, kill the player, or call a build a runtime qualification.

## Observed mechanism

Latest `build/wow-internal.log`, session
`352.134358492686284770.111867086.624`, contains a failed fight against
GUID `0xF130000D58003734`, entry3416. At lines15069-15077 the player has
435/536 HP, target83/100 HP, distance1.5767, Fighting. The 32-tick global
watchdog subsequently stops Attack, holds position and requests the SAME GUID.
The following server victim samples become zero; repeated SetTarget dispatches
return, but TARGET CONSISTENCY keeps reporting zero/await_selected_target_evidence
(e.g.16927,17487). Player HP continues falling and death recovery eventually
takes over. This is not proof that the *actual UI selection* was zero: the old
telemetry labelled UNIT_FIELD_TARGET as client/UI selection.

The older user-reported successful CLIENT LATCH DESYNC recovery is historical
evidence; that exact event string is absent from this newest session.

`lowHpHardStallRecoveries` counts only the existing <=15%-target-HP finisher
case, not general attack desync; target83/100 does not qualify. `emergencyEvents`
is a separate low-player-HP edge counter, not a liveness detector. Neither
counter is repurposed. New recovery events are attributed explicitly.

## Client/source evidence and identity

Read-only disassembly of local WoW.exe (SHA256
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`):

- Native SetTarget493540 reads selection low/high GUID at B4E2D8/DC:
  493605 `mov ecx,[B4E2D8]`, 49360D `mov eax,[B4E2DC]`, 493617 conditional
  return4938FD if both already match. Writes at4936E2/4936E8; the ordinary
  CMSG_SET_SELECTION path appears at493857/49388D.
- PlayerSnapshot's descriptor+40 is UNIT_FIELD_TARGET, the server attack
  victim. Local VMaNGOS Unit.cpp4526 sets this on Attack;4600 clears it on
  AttackStop. MiscHandler.cpp399-401 processes selection separately using
  SetSelectionGuid. Reading that victim as selection creates a false hold
  after a legitimate AttackStop. Repeating SetTarget against already-selected
  native GUID cannot fix that evidence mismatch.
- UnitAttackSpeed518E50 reads unit+110's field+1E0, copies it to an integer
  temporary and uses FILD at518EB0 before converting to seconds. The descriptor
  byte offset is1F8 (5875 field index7E). It is a period in milliseconds in this
  client, not proof that a swing occurred. Other VMaNGOS internal float layouts
  are NOT used to reinterpret this client integer.
- Native CEAC30 is auto-repeat spell state; CECA88 is pending/targeting spell
  state. Neither is asserted to be an ordinary offensive cast or swing counter.

`CombatClientEvidence5875` signature-validates493605..49361C on the game thread,
checks current manager/player identity, and reads the native selection cache.
It does not write selection memory. Unknown remains unknown, never a fallback
to the server victim field. Existing TargetController invokes native SetTarget.
All CombatController selection guards use actual UI selection; server victim
remains unchanged in WorldState/AFK and is separately labelled in new telemetry.

Fresh health samples require current object GUID/descriptor, matching direct
client health reads and a strictly increasing monitor tick. Target GUID AND
object address key the sample series. Gaps discard timing, not spent repairs.
This proves local sample freshness, not receipt of a new server damage packet.

## Existing state machine and ownership

Idle -> AcquiringTarget -> WaitingForTargetSelection -> WarriorChargeFacing /
WarriorOpening -> Chasing -> Fighting -> PostKillDelay / Looting -> Recovering
or AcquiringTarget. Invalid/missing target uses existing FinishTarget handling;
terminal failures use Failed. Legacy quest pickup/travel/return/turn-in states
are preserved. Commands remain intent, not transition proof.

The monitor preempts combat for death, reconciliation and other execution
owners; the same CombatController executes both workloads. No new owner or
movement writer is introduced. New combat actions require fresh alive/non-Ghost
player evidence, current native selected GUID, native target identity/HP,
current distance/facing and a game-thread Lua guard. Dialog/transaction/loading
UI, spell targeting, edit-box/keyboard-handler ownership, cast/channel and short
live action cooldowns block new input. AFK paired input is synchronous and runs
after gameplay; no outstanding AFK key lifecycle or policy is changed.
Self-owned separation/chase reconciliation can finish their existing bounded
FSMs even when the new watchdog is input-blocked; it cannot deadlock their
cleanup by returning early solely for its own positioning state.

## Progress and bounded recovery

Meaningful damage progress is same-target HP decrease or confirmed death.
Selection restoration and observed Attack action are structural verification,
NOT damage progress. CTM, displacement, ability/target command dispatch, rage
spend and global recovery dispatch never reset this damage budget.

Classes: target_selection_desync, autoattack_latch_desync, facing_block,
range_block, movement_owner_conflict, legitimate_action_wait,
offensive_no_progress, target_invalid_or_dead, unknown_or_stale_evidence.

At most three repair dispatches between observed target-health progress; same
target restoration waits for actual native selection. Attack start is
idempotent on the real 120-slot Attack action: never toggle a healthy latch off
for ordinary re-engagement. Initial no-slot start is allowed once, but the
missing action slot does not become false/true latch evidence. Unsupported
no-slot hard refresh fails rather than ClearTarget/TargetLastTarget guessing.

One hard refresh uses the existing legitimate Attack stop/start path, ONLY
after fresh same-target/range/facing/cast/UI checks. Success requires damage or
death, not merely an active latch. Pending verification blocks repeat commands.
Unproductive refresh or three spent repairs reaches existing terminal Fail
(Attack/chase cleanup), not random target selection, movement or toggle spam.
Global stationary-melee hard-stall recovery defers to this damage watchdog so
it cannot endlessly restart AttackStop/SetTarget/separation against the same
valid melee lock. Non-melee recovery remains unchanged.

Timing derives from existing 250ms polling, 1s Attack probe, global 4s soft/8s
hard windows and the native main-hand period. Healthy no-damage diagnosis uses
max(8000ms, two swing periods+1000ms); falling player HP uses
max(4000ms, one swing period+1000ms). This is urgency, not a low-HP diagnosis.
Continuous eligible melee evidence is also required. Cast/channel/cooldown,
range or facing gaps restart eligibility timing but do not refund repairs.
Structural confirmation uses the existing1s probe window. These timing choices
are deterministic SOURCE/TEST policy, NOT runtime-qualified miss/latency bounds.

The preserved Vanilla [CastingBarFrame source](https://github.com/MOUZU/Blizzard-WoW-Interface/blob/master/1.12.1/FrameXML/CastingBarFrame.lua)
sets casting/channeling in response to SPELLCAST events. The guard observes
those fields and conservatively waits for short live action cooldowns; it does
not invent UnitCastingInfo, cast completion, swing evidence or an evade flag.
Missing/modified UI evidence fails closed. This uncertainty stays visible.

## Validation and runtime gates

`combat_liveness_policy_test.cpp`: same-target sampling, health progress,
structural verification, bounded same-GUID restore/attack repair, stale ticks,
object changes, urgent HP loss, slow weapons, range/facing/cast/ownership gates,
death/invalid/unknown handling and no dispatch-as-success.
Final review added failing-then-passing regressions: target damage while UI
still mismatches must NOT confirm target restoration; unreadable HP represented
by zero must remain UNKNOWN and preserve spent repair budget, not become death.
`combat_action_evidence_fixture.lua`: real production script, inactive/active
Attack, idempotent start, one stop/start refresh, no-slot unknown/fail-closed
fallback, cast/channel/short cooldown, death, missing API, dialog/transaction,
target disappearance, edit-box and exception guards. Full suite still covers
AFK, death and unchanged2000/4/2 navigation budgets.

Run `python3 tools/validate.py --jobs 4`, `cmake --build build`,
`git diff --check -- .`. Results and scoped checkpoint recorded in project state.
An isolated HEAD+intended-combat build exposed an existing WorldMonitor
WaitingForManualVendor enum dependency. Only its already-local enum/name hunks
are published mechanically; no manual vendor behavior or AFK policy is changed.

Runtime remains PENDING. Fresh normal process: `wine ./build/wow_gui.exe`;
normal Grinding, no manual input, no deliberately dangerous stall. Capture:

```sh
tail -n 0 -F build/wow-internal.log | rg --line-buffered \
 'COMBAT|Combat|TARGET|Target|ATTACK|Attack|LIVENESS|DESYNC|HARD STALL|RECOVERY|Death|DEATH|AFK '
```

First verify normal fights/AFK/death ownership have no regression. A natural
desync must preserve the SAME GUID, dispatch bounded repair, verify actual
selection/latch and then damage/death. Hard refresh specifically needs observed
HP decrease/death. Two previously verified natural death exits in this latest
capture (20440/20468 and23844/23872) are baseline evidence, not post-patch combat
qualification. Source-audited selection mismatch does not establish which
input/action initially stopped productive damage in the newest stalled fight.
That initiating cause and live new-watchdog timing remain runtime uncertainty.
