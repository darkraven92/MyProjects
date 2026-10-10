# Combat execution evidence and reliability release gate — 2026-10-10

**READY TO LEAVE ACTIVE RELIABILITY WORK: YES.** This is a source-bounded
engineering release to the offline P0.7 foundation, not a new runtime PASS or
an unattended-play qualification. Continued clean `4e9a312` on
`codex/vendor-afk-long-navigation`, one agent. P0.7 remains untouched.

## Evidence and attribution

**RUNTIME OBSERVED:** the preserved endurance log still hashes to
`50b853fed92fb61b5d66e6df3ad7c199846655c0e9237abac6a01468d58e059c`.
Raw lines 1236/3974/7794 report `unknown_frame_iteration_limit`, including
successful opening fights. Lines 8449–8450 dispatch/pending and 9235 failure
show unconfirmed bootstrap; 9431/10502/11574/12693 show global recovery deferring
to the melee owner. Lines 23400 and 36527 show living defense with independently
readable Attack inactive; 39417 records the nearby repeat latch. See the
[complete incident audit](GRIND_ENDURANCE_INCIDENT_2026-10-10.md) for chronology,
all capture hashes, two reclaims, three deaths, two opening kills and binding
limits. These observations predate both the previous bootstrap fix and this
checkpoint. No new WoW run was performed.

**SOURCE VERIFIED:** the full action script previously returned at its
4,096-frame observation limit before action-slot readback. That combines an
input-eligibility failure with unavailable Attack evidence, even though a
separate read-only action scan can succeed. The previous checkpoint already
transfers failed/rejected bootstrap to bounded terminal ownership. However,
after a confirmed start, unknown full input can still prevent all liveness
repairs while the global stationary watchdog defers. Stale native target or
selection evidence can similarly hold active-state reconciliation. During
living recovery the ordinary external watchdogs deliberately do not run;
the living deadline blocks normal-mode release but still permits defense.
Thus a finite living deadline alone does not bound a stale Combat defense owner.

Read-only inspection of the same local client binary confirms SHA256
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
The registration at `0x872e7c` pairs the `EnumerateFrames` string at `0x872eac`
with function `0x705f60`. Its frame-argument branch loads the next list entry
at `0x705fe8`; absent argument loads the list head at `0x705ff0`; the
null/tagged termination path is `0x706031`. Reproduce with:

```sh
objdump -d -Mintel --start-address=0x705f60 --stop-address=0x706040 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
```

These are audit addresses only, not new runtime offsets. The iterator does
accept a preceding frame and has a termination path. This does not establish
the captured session's frame count, addons, hooks, or repeated-frame identity.

**INFERRED:** a complete UI tree between 4,097 and 16,384 frames would explain
and now avoid the previous limit. It is not proved to be this session's cause.

**UNKNOWN:** exact loaded DLL binding for that session; original client Attack
rejection and stun/loss-of-control causality; actual frame count/cycle; executed
swings; changed adapter performance and gameplay outcomes. An Attack latch is
not a hit, and target HP decrease does not attribute damage to the bot.

## Implemented together

- Full UI observation permits at most **16,384 frames**, with an explicit
  visited-frame cycle rejection. Every traversed frame receives the existing
  visibility/type/keyboard/script checks; only natural list completion allows
  input. A late EditBox/keyboard handler blocks, truncation remains unknown,
  and exceptions still fail closed. The larger count is a finite observation
  budget, not an asserted client-frame count. Bootstrap, AFK and DeathRecovery
  permissions are unchanged.
- Combat Attack readback uses the existing Vanilla action APIs, independently
  of full input eligibility. The read-only script catches errors and resets its
  result; strict slot/state parsing rejects contradictory or trailing data.
  Missing action slot is unknown, not inactive. Independent readback can verify
  a structural bootstrap latch but cannot authorize repair, release or movement.
  Containment now explicitly retains known/safe/non-wait input requirements at
  every consumer that previously relied on coupled evidence.
- A target-bound **40,000-ms no-progress deadline** starts with active locked
  combat (Charge-facing/opening, chase, Fighting), using the existing 160-tick
  combat-owner timeout scale. It is evaluated before initiation gates and
  transfers to the existing terminal/containment owner, including during living
  defense. Fresh selected same-object target HP decrease renews it. Unknown or
  repeated snapshots, object replacement, selection/latch/facing changes and
  chase transitions do not. No health comparison crosses a freshness gap.
  Death/target retirement resets it. Water entry discards the preceding age
  and HP baseline, and water ownership suspends combat updates; if the retained
  target returns after a long water block, expiry may fail closed immediately.
  Verified damage during containment permits reengagement with a renewed window. No extra Attack attempt, owner, route, radius or retry is added.
- Telemetry separates `COMBAT ATTACK READBACK` from `COMBAT INPUT PROBE`, logs
  changed readback states/reasons, and emits `COMBAT OWNERSHIP deadline=expired`
  with the terminal decision. Execution telemetry reports the actual liveness
  classification instead of always calling it offensive no-progress. Invalid
  target snapshots retire as non-kills. Existing target-disappearance and
  confirmed-death ownership remain.
- Regression coverage exercises a confirmed bootstrap followed by unreadable
  input/no damage; stale, foreign selection, cast, missing Attack, facing, range,
  object and repeated-snapshot cases; damage renewal, water pause, clock/reset
  behavior and terminal living blocking. It also checks complete large UI trees,
  blockers beyond the old limit, exact cap, cap exhaustion/cycles in all guarded
  modes, read-only API errors, parser failures, and the final living-to-Grind
  resume wiring. The earlier bootstrap, containment, release, living, repeat,
  water and AFK tests remain authoritative within their source-test scope.

## Release decision

| Standard | Current assessment |
| --- | --- |
| No known critical combat ownership deadlock | Reviewed failure chain is source bounded: rejected/unconfirmed bootstrap retains its existing fixed verification window; all active locked combat has a no-damage deadline independent of input/freshness; containment remains 30 seconds and release remains four seconds within that episode. Unknown commands fail closed. |
| No known unbounded death/reclaim loop | Existing finite corpse budgets and nearby repeat circuit breaker retained. Observed endurance ends at the matching repeat latch. This does not guarantee protection outside its 120-second/eight-yard contract or across world gaps. |
| Repeat protection intact | No production change to repeat geometry/history/rearm. Existing boundary tests and success-then-redeath cross-policy case retained. |
| Normal Grind resumes after successful recovery where evidence supports it | SOURCE VERIFIED: fresh disengagement retires living defense; bounded NavMesh egress and health/quiet proofs permit completion; monitor resumes Combat then Grind, resets living ownership, and continues to the next fresh snapshot before acquisition. Existing executable policy tests plus strengthened wiring assertions cover this; current living runtime success remains unobserved. |
| Remaining issues bounded and non-blocking for continued development | Unknown UI/action evidence ends in bounded terminal/manual recovery rather than an input bypass. Living automatic work is 90 seconds, with a persistent passive interlock on failure. These limitations do not block offline catalogue/provenance work. |

No concrete critical ownership blocker remains known in the reviewed source.
This does not assert complete verification of every possible client behavior.
Stop active reliability implementation here; do not use absent runtime paths as
an excuse to keep changing bounded owners without new evidence.

Safe Ghost staging / first unsafe resurrection remain **SOURCE GAP**. There
is no qualified Ghost threat-coverage/positive safe-point predicate. No aggro
radius, safe anchor, new map/zone data, new offset, or weaker reclaim/water/AFK
condition was introduced. Defense before the second alive probe remains pending.
Vendor, water emergency egress, AFK delivery, reconnect and unload retain their
separate prior statuses; none blocks the offline next phase.

## One batched runtime qualification, when live reliability is next evaluated

A new run **is necessary for runtime acceptance of this batch**, but is not a
prerequisite to P0.7 offline development. Use one fixed, demonstrably loaded DLL,
source revision, pre/post hashes and full raw/lifecycle capture during supervised
natural Grind. The existing >=20-minute endurance target remains; do not force
death/stun or waive a guard to populate a case.

1. Record full action probe readiness or its exact remaining blocker alongside
   independent active/inactive/unknown Attack. If frame limit/cycle persists,
   retain it as unknown; do not repeatedly enlarge the scan without evidence.
   Check responsiveness under the larger finite scan.
2. Observe natural Attack dispatch then latch/HP evidence. If a repair is needed,
   verify existing safe input guards and finite repair attempts; after lost
   progress or stale evidence, verify deadline -> terminal/containment -> bounded
   outcome, with no renewed Fighting loop. Loss-of-control remains unattributed
   unless the capture actually supplies qualified evidence.
3. When naturally exercised, observe reclaim -> two fresh alive probes -> living
   defense -> disengagement -> reachable egress -> health/quiet proofs -> complete
   -> next-snapshot normal Grind. A failed defense must retain the manual interlock,
   not silently resume normal work. Preserve the matching repeat-death terminal
   no-reclaim/no-route contract if that case occurs. Absent cases stay pending.

A capture contradicting one of these bounds reopens that concrete blocker.
Terminal failure by itself is not proof of successful combat recovery and must
not be reported as runtime PASS. Historical P0.5.8 scope remains unchanged.

## Validation and checkpoint

Targeted C++/Lua policy, parser, frame-guard and cross-owner tests passed.
Full validation PASS: **115 C++ tests, 42 audit Python + 13 QuestDB Python tests,
SQL fixtures and 12 Lua suites**, DLL build and diff check. Command:
`PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4`; report
`/tmp/wow-validation-yq29id0c/results.json`, console
`/tmp/combat-release-validation.log`. Separate `cmake --build build` PASS
(up to date). These results are not physical-motion or live recovery proof.
This coherent evidence/ownership/release-gate milestone merits one GitHub
checkpoint. No intermediate push, merge, forced push, capture commit or P0.7
work is part of it.

Next major task: switch to the existing P0.7 worktree/branch, complete the offline
leveling-profile catalogue, validate Orc starting-zone quest/profile provenance,
and consolidate P0.7. Then begin P1 Shared Questing + Grinding Core.
