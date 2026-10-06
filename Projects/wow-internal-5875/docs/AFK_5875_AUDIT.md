# P0.0 shared AFK protection audit

Status: SOURCE VERIFIED / TEST PASS (policy and Lua guards); RUNTIME PENDING.
No running WoW/Wine process was available during this checkpoint. Neither a
qualifying input nor either prevention window has been demonstrated in WoW.
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
| Mark AFK | 0x5EB740, called by idle check when eligible |
| Explicit clear | Empty AFK chat dispatcher 0x49F4F3–0x49F553 uses local flag to mark/clear |
| TurnLeftStart/Stop | 0x513EE0/0x513F10 read the input timestamp; issuing movement is not proof of refreshing it |

The 300000-ms client threshold is **source evidence, not a measured timeout**.
Server policy, suppressed client checks, manual AFK and flags in transit can
produce different observations. The controller records local and server state
separately. Runtime measurement remains required.

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

An already-AFK character may additionally need a separate guarded empty AFK
chat after input verification (autoClearAFK may be disabled). That request is
confirmed only when **both** live flags become false. Flags/timers are never
written by the bot. A server/client disagreement remains blocked.

`AfkRuntimeStatus` exposes a read-only snapshot with known/active state, input
age, source threshold, independently observed threshold and verification count.
GUI transport/display is not yet integrated. No fabricated measured countdown.

## Controlled runtime gate (fish)

Run on safe stationary land, healthy and out of combat, with dialogs closed.
Ensure F12 is unbound. This deliberately lets natural AFK occur once to measure
the baseline; then tests input and two prevention intervals. Do not manually
press keys or interact with the WoW window after starting the test.

The environment belongs to the **WoW process**, not just the GUI/loader. Launch
a fresh client with it; do not kill/restart an existing client automatically:

```fish
cd ~/Programming/Projects/wow-internal-5875
env WOW_INTERNAL_AFK_MODE=qualify wine '/home/ludvig/Games/WoW Vanilla/WoW.exe'
```

Log in and stand safely. In another terminal, launch the existing GUI and Start
the desired workload:

```fish
cd ~/Programming/Projects/wow-internal-5875
wine build/wow_gui.exe
```

Capture in a third terminal (start before clicking Start):

```fish
cd ~/Programming/Projects/wow-internal-5875
set capture (mktemp -d /tmp/wow-afk-qualification.XXXXXX)
tail -n 0 -F build/wow-internal.log | tee "$capture/afk-live.log" | rg --line-buffered 'AFK |MOVEMENT INTENT|COMBAT|DEATH|WORLD RECONCILIATION'
```

Qualification suppresses voluntary work only while no gameplay owner exists.
Threat, movement, death or another owner aborts it and returns to normal work.
Unknown observations, verification failure, unattributed input after baseline
or a bounded overall deadline also abort. No safe input candidate => BLOCKED,
not PASS. A baseline already AFK at startup is insufficient: begin from clear
state on a fresh run. Default `protect` mode does not intentionally wait for AFK;
`observe` mode never issues AFK input.

Required log sequence:

1. AFK STATE known=yes clear; natural local/server AFK transition and AFK
   THRESHOLD OBSERVATION continuousSafeIdle=yes (inspect actual elapsed value).
2. AFK ACTION unbound_F12; matching AFK INPUT RELEASE; native input clock
   advances. If necessary, separately issued clear_existing_afk followed by
   client_and_server_afk_clear. Do not attribute a toggle's success to the key.
3. Two complete safe/clear intervals, each ending near the safety margin with
   a verified paired pulse. AFK PREVENTION WINDOW 1 then 2. Missing observation,
   intervening AFK, unsafe ownership or early pulses cannot count as windows.
4. `two_prevention_windows_observed`, normal workload resumes, no manual
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
These do not prove Windows/Wine event handling or client AFK prevention.

Native signature mismatch disables the adapter. All held-input guarantees
apply to the synchronous message design, not untested OS keyboard injection.
Continuous busy ownership intentionally defers AFK activity: it cannot be
reported protected until runtime shows eligible safe opportunities. Existing
navigation/death/pull safety and their budgets were not changed.
