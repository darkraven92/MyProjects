# P0.0 shared AFK protection audit

Status: SOURCE VERIFIED / TEST PASS (policy and Lua guards); RUNTIME PENDING.
The first live qualification started observing but aborted before any F12.
The next live run aborted at `initial_afk_state_not_clear` with authoritative
client/server AFK active and native input age 354 ms. P0.0.2 removes that harness
defect; it does not establish that F12 works.
The P0.0.3 capture successfully acquired the hold but aborted on an input
change after immediately entering baseline. Pre-baseline quiescence is now
required; it remains RUNTIME PENDING on the rebuilt client.
Neither a qualifying input nor either prevention window has been demonstrated.
Do not label the character protected merely because this controller is enabled.

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

The 300000-ms client threshold is **source evidence, not a measured timeout**.
Server policy, suppressed client checks, manual AFK and flags in transit can
produce different observations. The controller records local and server state
separately. The newest post-abort Grinding run observed both flags active at
input age 300158 ms, consistent with the source threshold, but logged
`continuousSafeIdle=no`. This is not a controlled qualification baseline or
prevention proof; the safe-idle measurement gate remains pending.

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

**This input path is a candidate, not runtime verified.** Wine/client event
handling may ignore targeted messages. Delivery is logged as
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
input. **Qualification never uses that toggle**: it tests F12 alone. Otherwise
the toggle could clear AFK and falsely qualify F12. If autoClearAFK is disabled
or the input path does not clear the flags, qualification must fail honestly.
Flags/timers are never written by the bot.

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
   `AFK CLEAR VERIFY result=confirmed`, then `AFK QUALIFICATION BASELINE RESET
   reason=candidate_clear_confirmed`. No AFK-chat toggle during qualification.
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

Native signature mismatch disables the adapter. All held-input guarantees
apply to the synchronous message design, not untested OS keyboard injection.
Continuous busy ownership intentionally defers AFK activity: it cannot be
reported protected until runtime shows eligible safe opportunities. Existing
navigation/death/pull safety and their budgets were not changed.
