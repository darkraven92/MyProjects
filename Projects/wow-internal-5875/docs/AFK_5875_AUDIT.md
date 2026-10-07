# P0.0 shared AFK protection audit

## P0.0.7 death/ghost prevention (current)

Newest production capture (`build/wow-internal.log`, 2026-10-07) confirms
P0.0.6 urgency/sticky deferral: lines 543-545 due/deferred at 240131 ms,
588 overdue at 270185, 633-637 threshold at 300018 followed by client-only
then both-active AFK. Blocker was `death_or_ghost`; no F12 was attempted.
The flag 0x12 and HP=1 show a ghost. This is NOT a failed ghost F12 experiment.
Lines 182-183 separately report `missing_corpse_anchor`; recovery stays Failed
and retains death ownership. Audit that autonomy blocker in the next bounded
death-recovery phase; this checkpoint does not restart or rewrite recovery.

SOURCE VERIFIED: re-audited local 5875 binary with the hash below. Dispatcher
0x765F10..0x765FC1 writes the clock at 0x765F34 before calling registered
consumers at vtable+0x60. Release 0x765FD0..0x76606D writes at 0x765FEC,
calls the key consumer at +0x64, then clears consumer bookkeeping. Neither
dispatcher tests player health, ghost flags, or a death-recovery state.
This establishes no death-specific branch in THIS dispatcher, not a proof
that every registered consumer/addon is harmless. Live unbound-F12 and visible
keyboard/UI guards plus the bounded scene test remain mandatory. The audit
tool signatures still pass. No new API, OS-global key, memory write, packet,
movement command, native clear permission, or timer threshold was introduced.

New AfkDeadGhostPolicy is prevention-only and session-local. At source-derived
due time, a natural dead/ghost state can try ONE paired candidate in a safe
command gap. Dead and ghost qualify independently; delivery, matching release,
scene/UI/target/facing/movement integrity, unchanged native life state and
unchanged recovery state must pass, then fresh native clock advancement and
both clear AFK flags must be observed within the existing 3000-ms deadline.
Failure disables attempts for the session; stop/world loss discards evidence.
The next eligible due pulse uses the same guards and verification, even after
qualification. No cross-tick held key exists. No artificial death is requested.

Command safety: WorldMonitor invokes AFK after DeathRecovery::Update and all
other owners. Recovery commands use synchronous GameThreadDispatcher::Invoke;
they have returned before this point. ReleasingSpirit, WaitingForGhost and
WaitingForAlive remain blocked (including asynchronous release/reclaim
confirmation). Idle, RoutingToCorpse, WaitingForReclaim and Failed can be
eligible, NOT automatically safe. Combat, independent recovery, transactions,
faults, held keys/buttons, visible dialogs, loading/invalid world and unsafe
movement flags still block. Ordinary ghost land movement uses the unchanged
land mask; swim/fall/transport remain blocked. HP=1 with ghost flag is not
misclassified as living low-health recovery. HP=1 without that flag is unknown.
Lua checks UnitIsDeadOrGhost again; all four StaticPopup frames are blocked.

The paired-message scene comparison is synchronous on the game thread.
Monitor recovery state is compared after dispatch, before its next update;
normal ghost route progress on subsequent ticks is not labelled a key effect.
Sparse `AFK DEAD/GHOST STATUS`, `QUALIFICATION`, `ACTION`, `VERIFY` include
pre-pulse clock/scene/recovery state and separate delivery verification.
P0.0.6 240000/270000/300000 bands and native-clock-only scheduling are unchanged.
Full stationary qualify mode stays separate and still rejects death.

AFK already active or threshold crossed while dead/ghost is explicitly blocked
as `dead_ghost_afk_recovery_not_qualified`. This checkpoint does NOT grant native
auto-clear in death states or solve mixed flags. Both-active alive recovery
and the documented mixed-state protocol blocker remain unchanged.

Status: previous qualification RUNTIME PASS (user-reported), prior normal
cycle 1 RUNTIME PASS, prior long-run RUNTIME FAIL. New death/ghost behavior
RUNTIME PENDING; normal two-cycle production gate remains pending. No WoW
process was running during this audit. Run normal mode (no qualify environment):

```fish
wine ./build/wow_gui.exe
# In another terminal, before Start Bot:
tail -n 0 -F build/wow-internal.log | rg --line-buffered 'AFK '
```

Do not intentionally die. Require two natural normal-work prevention cycles
without AFK. If natural death lasts until due, require DEAD/GHOST QUALIFICATION
-> VERIFY advanced=yes sceneUnchanged=yes recoveryStateUnchangedOrValid=yes
result=pass -> PRODUCTION VERIFY confirmed, before 300000 ms, both flags clear,
no manual input/held key/side effect, recovery continuing independently.
No death during the run means that gate stays pending, not failed or passed.

Validation: TEST PASS, 79 strict C++20/Wall/extra/Werror tests, 13 Python tests
plus QuestDB SQL fixture, seven Lua fixtures / 138 checks. BUILD PASS and
DIFF CHECK PASS. Final full artifact: `/tmp/wow-validation-loia88my/results.json`.
New focused test first failed before the policy existed. Existing qualification,
production, navigation and death-recovery regressions pass unchanged. Validation
uses the current dirty worktree, not a clean remote checkout or a live WoW run.

## P0.0.6 production status (historical checkpoint)

Qualification: **RUNTIME PASS, user-reported** in the P0.0.6 request; its full
two-window capture is not retained in the current log. Production Grinding
cycle 1: **RUNTIME PASS** in the inspected log, input 90470165 -> 90713889
(243724 ms), paired delivery, unchanged scene/UI, both flags clear. Production
long-run: **RUNTIME FAIL**. Cycle 2 was deferred; client AFK appeared at
300231 ms and the pulse was not issued until clock 91676232 versus 91169170
(507062 ms). Input delivery succeeded but client stayed AFK, server stayed clear.
`autoClearAFK=enabled` is now runtime-observed at log line 205.

SOURCE VERIFIED: production still used stationary-only AfkProtectionPolicy
and adapter guards, even though the workload classifier already recognized
benign work. The `benign_work_awaiting_runtime_verified_noop` message was an
unconditional relabel, not a persisted qualification lookup. Native auto-clear
was wired only into qualification; the production fallback rejected mixed
flags indefinitely. The configuration's `runtimeVerified=no` described neither
implementation qualification nor command evidence accurately.

Important limit on attribution: at cycle-2 due, log lines 27350–27385 show
locked-target Chasing/Combat ownership; threshold crossing at 30281 occurs
amid locked-target restoration. F12 resumes after Recovering -> AcquiringTarget
at 34548. This does NOT prove every deferral was benign, or justify interrupting
combat. A combat/recovery interval longer than the safety margin can still
cross the threshold. New telemetry explains the exact blocker/urgency band.

Changes (SOURCE VERIFIED, new runtime PENDING): shared AfkProductionPolicy
keeps Due/Deferred until an authoritative native clock change, uses 80% due,
90% overdue and the unchanged 100% source threshold. First known-safe work
opportunity resumes the pending action. No CTM/displacement resets this state.
Reviewed implementation qualification is distinct from session delivery and
per-action proof. Classified Grinding/Roaming/ApproachingTarget with no hard
combat/recovery/fault/transaction owner can use the qualified pulse. Native
guards allow only ordinary 1.12.1 land forward/back/strafe/turn/walk flags;
swim, fall, pitch, transport and unknown flags still block. First aid, manual
vendor and low-health recovery remain hard gates. Questing uses the same
policy but unclassified active Questing owners remain blocked, not guessed.

Scene proof brackets the synchronous paired messages on the game thread; it
does not compare a moving character with its position a tick later and call
normal navigation an input side effect. The next update must still prove the
native clock advanced. UI is rechecked inside the same paired-message bracket;
all held physical keys/buttons block dispatch. Combat/dialog beginning on the
next ordinary gameplay tick is not falsely attributed to the prior key pulse.
Qualification retains its stationary, across-update scene guard.
Production UI re-probe is bounded to one second (not an input interval);
all command verification remains within the original 3000-ms deadline.

Recovery now starts for **either** active authoritative flag, verifies F12,
then uses the native non-forced clear once when both flags are active. Both
must clear before recovery success. Mixed states enter bounded reconciliation,
not an inert retry loop; convergence can proceed to clear or confirm already
clear. Persistent disagreement ends in `mixed_afk_requires_safe_reconciliation`,
latched for the session. This is **NOT a complete mixed-state recovery fix**.

### Mixed-state native-clear blocker — do not remove the guard

Re-audited 0x5EB836–0x5EB840: client clear => native function returns without
sending. With client active/server clear, it sends an empty AFK message, and
the local VMaNGOS ChatHandler.cpp:624 toggles server AFK **on**, not off.
The server also ignores AFK messages during combat. Qualification of the
both-active case does not validate either asymmetric case. Removing the guard
would violate source evidence, not repair it. No flags are patched; no double
toggle or fabricated synchronization was introduced. Need a source-/runtime-
proven legitimate reconciliation operation (or evidence that the live server
has different semantics) before implementing the requested unconditional
OR-flags native dispatch. A comparison capture for the mixed state was
requested. This remains the active AFK blocker; do not advance to swimming.

The other local writer was also inspected: the player-flags update callback
at 0x5EE990 XORs incoming old flags against current flags and gates the mirror
write on changed bits 0xE (0x5EE9B8–0x5EE9C0). It is not an unconditional
reconcile API; calling it with invented old flags would fabricate an update.
VMaNGOS Player::ToggleAFK additionally leaves battlegrounds when toggled on, so
a speculative mark-then-clear sequence is not a harmless general substitute.

Next runtime gate: fresh normal process (`wine ./build/wow_gui.exe`, no qualify
environment), natural Grinding >=20 minutes, no manual input. Capture `AFK `.
Require two pulses before 300000, unchanged synchronous scene/UI, native clock
advance, clear flags, continued Grinding, and DEFER -> RESUME if combat occurs.
Both-active recovery is testable; persistent mixed flags must show bounded
failure, NOT a falsely claimed recovery. New production code is RUNTIME PENDING.

TEST PASS: 78 strict C++ tests, 13 Python tests plus QuestDB SQL fixture,
six Lua fixtures / 113 checks. BUILD PASS; DIFF CHECK PASS. Final artifact:
`/tmp/wow-validation-v9eov92z/results.json`. Regression coverage includes sticky
due/defer/resume, benign land movement versus hard gates, native-clock-only
progress, session versus implementation evidence, OR-flags recovery entry,
once-only both-active clear, bounded mixed-state failure/convergence, unknown
observations and shared workload wiring. Tests do not prove production runtime.

The sections below preserve historical evidence; their earlier pending status
and stationary-only descriptions are superseded by this section.

P0.0.4 (2026-10-07): SOURCE VERIFIED; composite candidate RUNTIME PENDING.
The newest live capture proves qualification hold/quiescence and the natural
300241-ms idle threshold. Targeted F12 delivery and native input-clock advance
are RUNTIME PASS. **F12 alone clearing AFK is RUNTIME FAIL**: both states stayed
active through the 3000-ms deadline (observed failure at 3245 ms). Prevention
is RUNTIME PENDING; no prevention window began. Do not label the bot protected.

## P0.0.4 clear-path control-flow audit

The capture's baseline was 60801599. After quiescence stableMs=1541, natural
AFK was observed, paired F12 was delivered, and the native clock advanced to
61101864 without scene changes. Both AFK states remained active. This is NOT
evidence that Wine dropped the input, and extending the timeout is not a fix.

The binary contains two separate mechanisms:

- Key-event dispatch at 0x765F10 copies event fields, then writes event+0xC
  to 0xCF0BC8 at 0x765F34, BEFORE invoking registered consumers through vtable
  +0x60. Release dispatch 0x765FD0 writes the same clock at 0x765FEC and calls
  the consumer through +0x64. Clock delivery does not imply a bound gameplay
  action, nor a call to the AFK-clear routine.
- Actual clearing is 0x5EB830, same player-this/one-stack-argument ABI as its
  movement callers, `ret 4`. It first tests local 0xB6E5CC and returns if clear.
  At 0x5EB846 it tests the force argument; with force=0 it reads the CVar
  pointer at 0xC4D68C and integer field +0x28, returning if zero. It obtains
  CLEARED_AFK text at 0x5EB86C, displays the normal client notice, clears its
  own local flag at 0x5EB885, constructs opcode 0x95 / chat type 0x14 /
  language 0 / empty text, and calls network send 0x5AB630 at 0x5EB8D0.
  That wrapper obtains the connection via 0x5AB490 and sends via 0x5379A0.
  This is client/server behavior, not just a local flag edit.
- Direct callers: movement at 0x513D36, 0x514E23, 0x514F0B, 0x514FCA pass
  force=0; normal chat at 0x49F3D6 also passes 0. Explicit empty AFK chat
  at 0x49F553 passes 1, bypassing the setting. Jump registration at 0x8500B8
  maps to 0x513BD0; that action's accepted movement branch contains the
  0x513D36 call. MoveForwardStart registration 0x8500D0 maps to 0x513E20.
  Thus real keys that invoke these actions can clear AFK. A **real unbound
  F12** is not proven to clear it either. No human-input trace or full Wine
  dispatch-stack capture was made; no claim that all hardware keys clear AFK.

`autoClearAFK` registration at 0x5E24CC–0x5E24F4 supplies default string `1`
(0x82E748), name 0x8602CC, description 0x8602DC, and stores the returned CVar
pointer at 0xC4D68C. The executable says “Automatically clear AFK when moving
or chatting”; the argument/value branches and packet construction corroborate
that description. Only registration and this clear routine directly reference
that pointer in the disassembly. No `autoClearAFK` override was found in the
local WTF/Config.wtf; that is NOT proof of the live setting. No WoW process was
running during this audit. **Actual runtime CVar remains unknown.**

New read-only `AFK AUTO CLEAR setting=enabled|disabled|unknown` samples this
signature-validated pointer/field. Unknown/non-boolean values fail closed.
No setting is changed. Disabled would block automatic semantic clearing, but
is NOT established as the cause of this run. The proven missing operation is
the semantic client/server clear after the successfully delivered unbound key.

The local VMaNGOS 1.12.1 opcode table confirms CMSG_MESSAGECHAT=149/0x95;
SharedDefines.h confirms CHAT_MSG_AFK=0x14. ChatHandler.cpp:611–628 ignores
AFK in combat and toggles for empty text. Consequently a native-clear dispatch
requires BOTH live flags active, never one mismatched state or already clear.

### One new qualification-only candidate

`paired_F12_then_native_auto_clear` is explicitly a **composite**, NOT evidence
that F12 alone clears AFK. Qualify mode delivers the paired unbound key, verifies
fresh clock advancement and unchanged scene/UI, then requests the audited
0x5EB830 function with force=0 on the game thread once, only when both flags
remain active and autoClearAFK is authoritatively enabled. Entry/gate/packet/
send/return and CVar-registration signatures are checked. The native function
displays its expected AFK-cleared chat notice; no dialog/gameplay movement is
requested. Native code performs its normal state update and server packet;
the bot does not write either flag, input clock or CVar.

The original 3000-ms deadline starts at F12 dispatch and is NOT restarted by
delivery or native clear. Mismatched flags wait boundedly; no second clear,
toggle or key is sent while awaiting confirmation. Both flags must clear, then
two prevention intervals must still pass. Unsafe/unknown/side-effect/failed
dispatch aborts and releases the hold. Initial clear failure is logged as
`AFK QUALIFICATION PREREQUISITE`, not a failed prevention window. Existing
production guards and the separate production policy are unchanged.

Native timestamp is reread immediately before the key pulse, after UI/window/
scene guards. Delivery logs separate `baselineClock`, `inputClockBefore` and
`inputClockAfter`; equality of baseline/before is expected during genuine idle,
not evidence of reusing an old sample.

Transport choice: reuse already-proven targeted message delivery plus the
audited client clear mechanism; do not replace it with SendInput, keybd_event,
X11 events or another function key merely to repeat an unbound event. Those
broader paths have no established additional semantic benefit and may affect
focus/global input. A controlled human comparison is only needed if the new
capture disproves this source-backed model. Production remains unqualified.

## Evidence and cause

The previous `ActiveBotAfkSafeguard` is a Grinding liveness mechanism. It resets
its inactivity counter after displacement and requests reacquisition/roaming.
It does not verify the client's input clock. Questing's idle path only emitted
`no_source_verified_stationary_action`. Thus neither path established actual
AFK prevention. The existing Grinding liveness behavior remains intact.

Local binary: `/home/ludvig/Games/WoW Vanilla/WoW.exe`, image base 0x400000,
SHA256 `b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
Reproduce the read-only audit with `python3 tools/afk_client_audit.py <client>`.
Addresses below are VAs, not offsets into an arbitrary PE file.

| Evidence | Location / interpretation |
|---|---|
| Idle check | 0x482EA0; reads input timestamp 0xCF0BC8, calls clock 0x42C010 |
| Threshold | 0x482ECD subtracts 0x493E0 (300000 ms); immediate at 0x482ECF |
| Input handler | 0x765F34 writes 0xCF0BC8; signature checked before enabling adapter |
| Local AFK | 0xB6E5CC, read by AFK-clear routine 0x5EB830 |
| Server synchronization | 0x5EE9EF masks flags with 2; 0x5EE9F2 writes that result to 0xB6E5CC |
| Mark AFK | 0x5EB740, called by idle check when eligible |
| Explicit clear | Empty AFK chat dispatcher 0x49F4F3–0x49F553 uses local flag to mark/clear |
| TurnLeftStart/Stop | 0x513EE0/0x513F10 read the input timestamp; issuing movement is not proof of refreshing it |

The 300000-ms client threshold is source evidence, corroborated by the newest
controlled 300241-ms observation (not an exact independent server timeout).
Server policy, suppressed client checks, manual AFK and flags in transit can
produce different observations. The controller records local and server state
separately. The newest post-abort Grinding run observed both flags active at
input age 300158 ms, consistent with the source threshold, but logged
`continuousSafeIdle=no`. This is not a controlled qualification baseline or
prevention proof; the newer P0.0.4 baseline supersedes this earlier measurement.

P0.0.1 correction: B6E5CC is **not exclusively 0/1**. Explicit mark writes 1,
but server synchronization writes 0 or 2. The old `client>1` rejection made a
valid AFK update unknown. The adapter now signature-checks that writer and
accepts only source-proven 0/1/2; 1 and 2 mean active. Unknown encodings still
fail closed. New unknown-reason telemetry distinguishes bad signatures, reads,
flags and clock discontinuities. Legacy player-flag-only diagnostics remain
`signalVerified=no`, now labelled `AFK FLAG CANDIDATE SET/UNSET`.

The old log's abort precedes normal Grinding initialization; a direct attacker
at 2.1 yards is selected immediately afterward. The old monitor already had a
`continue` before workload dispatch. Thus ordinary workload preemption is NOT
proven; the exact old safety predicate was not logged. Do not suppress combat
to make the diagnostic pass. P0.0.1 makes qualification ownership explicit and
reports the actual blocking predicate for the next run.

Local VMaNGOS source under `../vmangos-core/src/game`:

- `Objects/UpdateFields_1_12_1.h`: PLAYER_FLAGS word 0xBE (byte 0x2F8).
- `Objects/Player.h`: PLAYER_FLAGS_AFK=2; ghost=0x10; IsAFK reads the flag.
- `Objects/UnitDefines.h`: UNIT_FLAG_IN_COMBAT=0x80000.
- `Objects/MovementInfo.h`: swimming=0x00200000, walk preference=0x100.
- `Handlers/ChatHandler.cpp`, CHAT_MSG_AFK: empty text toggles AFK, combat
  blocks it. Never send this packet/API blindly. Both local and server flags
  must be true immediately before a clear request.

Vanilla registration strings establish GetBindingAction, EnumerateFrames,
IsKeyboardEnabled, GetObjectType, GetScript, UnitAffectingCombat,
UnitIsDeadOrGhost and SendChatMessage. The guard does not assume modern
UnitIsAFK/GetCurrentKeyBoardFocus/HasFocus APIs. Unknown frame/API state blocks.

## Shared implementation

`WorldMonitor` owns one `SharedAfkController` for Questing and Grinding.
Ordinary scheduling runs after gameplay owners/watchdogs. Questing must report
valid no-action/no-owner/no-fault state; Grinding must be target-free in its
Grinding state, not approaching, roaming, vendoring or fault recovery.
Direct attackers, low health, combat lock/state, death, recovery, loot,
navigation and interactions block. The game-thread adapter rechecks health,
ghost/combat/movement flags and Lua UI guards immediately before dispatch.
Movement flags other than stationary walk preference are rejected, including
swimming, falling and transport. This is not a general water-state subsystem.

The policy uses **client input age**, not elapsed bot ticks or displacement.
At 80% of the source threshold, safe idle may issue a candidate pulse. Input
is not repeated while verification is pending. A failed dispatch/three-second
verification is session-latched, not retried indefinitely. UI guard probing
backs off ten seconds without sending a key. Stop/world gaps discard evidence.

`AfkQualificationHold` is diagnostic ownership with Requested/Held/Aborted/
Complete states, separate from production safe-input classification. Both
workloads' acquisition/updates are behind its monitor `continue`. Read-only
Lua guards run before acquiring the hold and every second during it; dispatch
always rechecks guards. Native safety and ownership are checked every snapshot.
UI reads do not send input or change the AFK timer. Abort is terminal for this
test session, releases the hold and leaves the controller observe-only; a fresh
Start Bot explicitly re-arms qualification. No Stop Bot is needed during a run.

Input candidate: unbound F12, using synchronous WM_KEYDOWN/WM_KEYUP on the
current process's visible WoW window, on its owning game thread. Window and
PID/thread are rediscovered/checked; no window-ID constant, foreground
activation, physical mouse movement, global held key or external input loop.
Visible EditBoxes, keyboard handlers, dialogs or an F12 binding block it.
The paired driver attempts release even after a failed/throwing press. No
held-input state survives a tick, stop, exception or client restart.

**Delivery and native clock are runtime verified; F12-only clear failed.**
Delivery is logged as
`paired_message_delivered`, not proof of hardware key state or AFK clearance.
Only a subsequently advanced native input timestamp confirms qualifying
activity. If it does not advance, stop with verification_timeout; do not
silently substitute movement or reset the clock in memory.

P0.0.2 compares freshly read position, facing, target and movement flags
throughout delivery AND AFK-clear verification, plus the read-only UI guard.
Unknown evidence, changed target/flags/UI, displacement above 0.01 yard or
angular change above 0.001 radian fails verification (shortest angle handles
wrap). These are numerical tolerances, not permission to move. This is
bounded observational evidence, not a claim about untested navigation input.

Ordinary roaming/acquisition is classified separately as BenignWork when
evidence supports it, but **production input permission is NOT relaxed yet**.
`benign_work_awaiting_runtime_verified_noop` is explicit. Even a completed idle
qualification requires review of the real capture, then implementation/testing
of navigation-safe production dispatch. Idle-only protection is not sufficient
for indefinite busy-workload AFK prevention; that requirement remains open.

Production retains its separate guarded empty-AFK-chat fallback after verified
input. Qualification does not use that forced Lua toggle: the new explicitly
named composite uses non-forced native auto-clear as described above. It must
never be reported as F12-alone success. Flags/timers are never written by the bot.

## P0.0.3 pre-baseline input quiescence

Latest log: hold acquired at inputClock=57169769, then first qualification
Update read 57169793 (24 ms later) and immediately selected Baseline. The
controller's next-update input-change guard aborted the run. Important trace
detail: those two logged values alone do NOT identify the input that caused
the abort. The first Update had no previous observation and tolerated that
change; a subsequent changed clock was not logged. Residual Start Bot/GUI input
is plausible, not proven. The verified defect is the missing quiet interval
before the strict evidence boundary.

`AfkQualificationPolicy::AwaitingQuiescence` now precedes both clear and active
entry. Central constants: QuietIntervalMs=1500, MaximumQuiescenceMs=10000.
These are diagnostic bounds, not changes to WoW's timer or measured propagation
latency. Each safe native-input timestamp change resets only the quiet interval;
the fixed startup deadline never resets. Reaching it fails with
`quiescence_not_reached`. This uses normal monotonic tick observations, no sleep
or input dispatch. Unsafe/unknown evidence still aborts immediately and releases
the existing hold; both workloads remain inhibited during quiescence.

Only after 1500 ms of unchanged clock does the policy capture baseline input
clock/client-server AFK state; the runtime captures the corresponding valid
scene. `AFK QUIESCENCE START/RESET/COMPLETE` and `AFK QUALIFICATION BASELINE`
are event-only diagnostics. From this boundary forward, uncommanded input
changes fail, including during the following already-AFK synchronization phase.
Candidate commands retain their own independent before/after clock and scene.
The adapter's fresher pre-command snapshot also cannot silently replace the
baseline clock with an unrelated input. Startup input earns no candidate,
clear or prevention credit. Normal production scheduling is unchanged.

## P0.0.2 entry and asynchronous verification (after quiescence)

`AfkQualificationPolicy` is a diagnostic-only state machine behind the existing
shared hold. After quiescence, an authoritative AFK-active start is not itself unsafe. A clear
start observes the natural baseline without input. An active start with input
age <= the existing 3000-ms verification bound observes fresh snapshots for at
most 1000 ms: naturally clear goes to baseline, still active goes to candidate
clear. An older active start goes directly to candidate clear. This short grace
is an engineering observation bound, **not a measured client propagation time**.

The three gates are separate: paired F12/native-clock delivery; authoritative
client AND server AFK clear; two continuously clear prevention intervals.
Delivery and clear share the existing 3000-ms deadline from dispatch, with no
new pulse/toggle while waiting and no timeout restart when the clock advances.
Mixed client/server flags can settle within this window. A clock change alone
never completes a clear or prevention gate. Unsafe state, unknown evidence,
side effects or timeout abort/release the hold; synchronous paired input leaves
no cross-tick key state. Normal work remains blocked while qualification holds.

Successful initial candidate clear arms a fresh baseline using the real input
timestamp, with **zero** completed prevention windows. Each later window still
needs safe/clear observations, age reaching the source safety margin, paired
input delivery and clear verification. AFK during a prevention window fails
instead of resetting and silently trying again. Unattributed input aborts after
quiescence establishes the baseline. Threshold measurement remains independent: an already
AFK entry cannot manufacture a measured timeout.
If AFK clears naturally before dispatch, observation returns to baseline. If
the adapter's fresher pre-command read catches that race only after dispatch,
the run fails rather than crediting F12 with a clear that preceded it.

`AfkRuntimeStatus` exposes a read-only snapshot with known/active state, input
age, source threshold, independently observed threshold, delivery/clear gates
and prevention-window count.
GUI transport/display is not yet integrated. No fabricated measured countdown.

## Controlled runtime gate (fish)

Run on safe stationary land, healthy and out of combat, with dialogs closed.
Ensure F12 is unbound. A clear entry lets natural AFK occur once to measure
the baseline; an already-AFK entry tests clearing first. Both require two later
prevention intervals. Do not manually
press keys or interact with the WoW window after starting the test.

Launch the GUI with the mode, then use **Start WoW**, log in on safe land and
**Start Bot**. The audited `CreateProcessW` call passes a null environment block,
so its newly created WoW process inherits the GUI environment:

```fish
cd ~/Programming/Projects/wow-internal-5875
env WOW_INTERNAL_AFK_MODE=qualify wine ./build/wow_gui.exe
```

An already-running WoW process does not inherit a newly launched GUI's mode.
Use the GUI's newly launched client; do not kill/restart a client automatically.

Capture in a third terminal (start before clicking Start):

```fish
cd ~/Programming/Projects/wow-internal-5875
set capture (mktemp -d /tmp/wow-afk-qualification.XXXXXX)
tail -n 0 -F build/wow-internal.log | tee "$capture/afk-live.log" | rg --line-buffered 'AFK |MOVEMENT INTENT|COMBAT|DEATH|WORLD RECONCILIATION'
```

Require `AFK QUALIFICATION HOLD state=acquired` before waiting. Qualification
suppresses voluntary work only while no conflicting gameplay owner exists.
Threat, movement, death or another owner aborts it and returns to normal work.
Unknown observations, verification failure, unattributed input after quiescence
or a bounded overall deadline also abort. No safe input candidate => BLOCKED,
not PASS. An already-AFK start is now supported, but is not itself a measured
baseline or a successful prevention window. Default `protect` mode does not intentionally wait for AFK;
`observe` mode never issues AFK input.

Required log sequence:

1. AFK QUALIFICATION START/HOLD acquired; AFK QUIESCENCE START, optional RESET,
   then COMPLETE and AFK QUALIFICATION BASELINE. No inputs issued in this phase.
   Clear => natural baseline, active => bounded synchronization/candidate_clear.
   For a clear baseline, inspect the natural AFK THRESHOLD OBSERVATION elapsed.
2. AFK ACTION unbound_F12; matching AFK INPUT RELEASE; native input clock
   advances (`AFK CANDIDATE DELIVERY advanced=yes`). Separately require
   `AFK ACTION action=native_auto_clear inputPath=game_thread_native_5875`,
   `AFK CLEAR VERIFY result=confirmed`, then `AFK QUALIFICATION BASELINE RESET
   reason=candidate_clear_confirmed`. No forced Lua AFK toggle.
3. Two complete safe/clear intervals, each ending near the safety margin with
   a verified paired pulse. AFK PREVENTION WINDOW 1 then 2. Missing observation,
   intervening AFK, unsafe ownership or early pulses cannot count as windows.
4. `AFK QUALIFICATION COMPLETE windows=2 result=pass`, hold released, normal workload resumes, no manual
   input, no displacement, stuck keys or interference. Inspect the complete
   capture, not just counters, before assigning RUNTIME PASS.

Repeat shared integration in both workload modes. If window lookup/guard/input
fails, retain the exact reason and full capture. A targeted Wine/X11 driver is
a later candidate only if this path demonstrably fails; no speculative W loop.

## Tests and limitations

`afk_protection_policy_test.cpp`: known/unknown flags, due threshold, all typed
ownership gates, pending/confirmed/timeout, flag mismatch, clock wrap, reset,
press/release failure/exception, and prevention-interval invalidation.
`afk_safe_input_fixture.lua`: real Lua 5.1 guard execution, APIs, binding,
dialogs, keyboard handlers, visible edit boxes and error fail-closed behavior.
`afk_qualification_hold_test.cpp`: ownership lifecycle, both workload gates,
abort/release, no automatic re-arm, unchanged input/state, raw client flag 2,
scene changes, production classification without permission, monitor wiring
and GUI environment inheritance.
`afk_qualification_policy_test.cpp`: clear/active/unknown entry, bounded startup
sync, natural clear, delayed two-flag clear, delivery != clear, shared deadline,
scene/UI changes, fresh baseline, mandatory first/second windows, prevention
AFK failure and wiring to the shared runtime. Its startup assertion failed
against the old controller before the fix.
`afk_quiescence_policy_test.cpp`: 24-ms startup change, quiet-interval reset,
fixed startup deadline, captured baseline evidence, strict post-baseline input,
already-AFK synchronization, independent candidate delivery, unsafe/unknown
abort, hold release and runtime wiring. The test failed compilation before
the new typed phase/API existed. P0.0.2 tests now run after the prerequisite
quiescence and retain the delivery/clear/two-window regressions.
These do not prove Windows/Wine event handling or client AFK prevention.

`afk_native_clear_test.cpp`: reproduces the rejected input-only candidate;
unknown/disabled CVar fails closed; native clear is requested only after input
proof and both-active flags; issued clear never counts as success; client-only,
server-only and neither-clear timeout without extending the deadline; no repeat
clear; unchanged scene, fresh pre-dispatch sample ordering/no direct flag/CVar
writes; initial success still requires two prevention intervals.

Native signature mismatch disables the adapter. All held-input guarantees
apply to the synchronous message design, not untested OS keyboard injection.
Continuous busy ownership intentionally defers AFK activity: it cannot be
reported protected until runtime shows eligible safe opportunities. Existing
navigation/death/pull safety and their budgets were not changed.
