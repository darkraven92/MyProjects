# P0.2 Combat reliability audit

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
