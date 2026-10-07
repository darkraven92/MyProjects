# P0.2 / P0.2.1 Combat reliability audit

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
