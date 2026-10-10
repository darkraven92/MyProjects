# Supervised Grind endurance incident, 2026-10-10

**READY FOR P0.7: NO.** This is a complete-session incident assessment, not
a successful endurance qualification. Three deaths, two automatic reclaims,
two living defense handoffs and one terminal repeat-death latch are observed.
Two Attack bootstrap attempts never produce progress and do not leave Fighting
until death. The verified missing failure transition is corrected in this
checkpoint; the client's original failure to activate Attack and the reported
stun transition remain unresolved. No runtime capture is committed.

## Evidence integrity and binding

All five available files in `runtime-captures/grind-endurance-2026-10-10/` were
inspected. All four manifest entries match their complete file bytes:

| File | SHA256 |
| --- | --- |
| deployment.txt | `43a1930289078108482fd81361f7c7ba5f1da8fd7b8d6654c8b5090ccecf1c0b` |
| wow-internal.log | `50b853fed92fb61b5d66e6df3ad7c199846655c0e9237abac6a01468d58e059c` |
| wow-internal.lifecycle.log | `0cc9cd51ed401fc20e8cb16e0dce24e4dc45429f22821637abcde25bfd11b242` |
| endurance-incident-extract.txt | `cb349447518d425a61a43e1d03ecbbcc081a6d438a0c8ebfc8fab5272c598184` |

The manifest itself hashes to
`0c3510861462d7f893996da61e019e057c660226a15df4871c77b8ebddf1234f`;
there is no independently supplied expected digest for that manifest. All
2,309 extract records match their indicated raw lines. The **40,414-line raw
log is authoritative**; review includes events omitted from the extract and
the entire terminal tail. The 210-line lifecycle journal contains older runs;
only lines 200–210 identify this session. Older entries do not fill its gaps.

Deployment records clean pre/post branch `codex/vendor-afk-long-navigation`,
HEAD `4019517a40793654e3bef7335b6d1387afbb684d`, and unchanged project
`build/wow_internal.dll` SHA256
`6f8cfb5a3119f6726f24a8d65f4220eb3bebc001ddd4efc6fb6b8d7df308e01a`.
Starting checkout/HEAD and on-disk DLL independently matched before this task's
edits/build. Those statements bind the reported deployment and local files.
**The capture does not contain a loaded-module path/hash/mapping or an explicit
user attestation for this session. Exact loaded-DLL binding remains UNKNOWN.**
The prior Linux PID 124315/inode 4980769 attestation concerns a different
`f022de6` session and cannot be reused. A clarification was requested; absent
new session-specific evidence, this audit does not award runtime PASS.

Runtime self-identifies as WoW 1.12.1 build 5875, Wine PID 296, DLL base
`0x74590000`, session `296.134361223514045240.91289047.504`, player GUID
112743 (`0x1B867`), manager `0x093F2A08`, player `0x0F080008`, descriptor
`0x0F081D78`. GUI PID 288 starts loader 484 for PID 296; loader exit is zero
and runtimeAttached=yes. Session identity agrees between raw and lifecycle.
These are useful corroboration, not an on-disk DLL digest read from that process.

BOT START is 91289563 ms; lifecycle BOT STOP is 92176014 ms: **14m46.451s**.
Raw stop is one millisecond earlier. Deployment's 18:11:02–18:28:39 +02:00
envelope is not gameplay duration; the proposed >=20-minute endurance gate was
not met. Raw UTC diagnostic samples span 16:13:10–16:27:12, followed by stop.

## Complete chronological model

Line numbers below refer to the preserved raw file, not this document.

| Raw lines | Observed sequence / ownership |
| --- | --- |
| 1–351 | Attach/start, valid world, normal Grind/Combat acquisition, healthy native AFK observation. Start HP 664/664, level 22. |
| 352–1114 | Grind wide-sector NavMesh intent 1, then approach intent 2 for entry 3928. Intent 1 releases after 11 commands/four replans; approach releases at acquisition range after eight commands. |
| 1143–3729 | Target `0xF130000F58007F13`: selection → Charge-facing → opening → Fighting. Bootstrap dispatch 1599, fresh damage confirmation 1779. Kill/loot at 3504–3559, PostKillDelay, RecoveryController Recovering → Ready, normal acquisition resumes. |
| 3739–7483 | Approach intent 3 arrives; target `0xF130000F58007F33`: selection/Charge/opening/Fighting, bootstrap confirmation 4191, kill/loot 7257–7311, health recovery → Ready → acquisition. Two kills and two successful loots total. |
| 7494–8127 | Approach intent 4 toward entry 3928 is interrupted by direct aggressor entry 12856/GUID `0xF130003238007E60`. Exact-attacker adoption 7669; selection 7701; Charge-facing/opening; chase starts, reaches melee, Fighting 8127. This GUID is the opponent in all three subsequent life episodes. |
| 8134–13674 | Separation and facing correction complete, consecutive alignment releases the facing guard. Bootstrap issued 8449, pending 8450, failed observation 9235. Target stays at 100/100; player loses health. Four global hard-stall attempts defer to the same-target watchdog, which never commands another repair. Death 1, Combat suspended/Idle 13642, death entry 13674 at body `(1807.7665,-2440.3184,88.4978)`. |
| 13690–23244 | ReleaseSpirit attempt 1, positive Ghost 13729, current server corpse anchor, seven corpse routes (intents 5–11), including five precision routes. Retrieve issued once at 23243, distance 6.622 yd; WaitingForAlive. |
| 23283–23337 | Fresh alive probes 1/2 at HP 332 and 2/2 at HP 296. Reclaim verified 23322; repeat armed at `(1814.078,-2438.831,87.153)`, 120000 ms/eight yards. Living owner enters 23334, normal acquisition blocked; exit resumeMode=LivingRecovery. Corpse controller rearms without erasing repeat history. |
| 23364–25942 | Exact same attacker adopted; positive native combat/direct evidence hands control to defense (HP 296). Charge/chase/Fighting, facing correction, Attack issued 24155 and fresh damage confirmed 24193. Target drops 100→96→…→58 while player reaches zero. Living remains attempts=0/navigation=no; no ordinary Grind, loot, vendor, living health recovery or egress release occurs. |
| 25943–26098 | Death 2/body `(1805.453,-2440.467,88.854)`; danger hotspot quarantined. **No repeat latch:** distance from preceding confirmed-alive position is approximately 8.942 yd, outside the eight-yard contract. ReleaseSpirit once, fresh Ghost 26066; second corpse episode starts. |
| 26100–36317 | Nine corpse routes, intents 12–20, including eight precision routes. Retrieve issued once at 36316, distance 6.832 yd. |
| 36354–36411 | Fresh alive probes 1/2 and 2/2 both HP 356. Reclaim verified 36396; new repeat record armed 36397 at `(1812.097,-2439.518,87.578)`. Living owner enters again, normal acquisition blocked. |
| 36412–39385 | HP drops to 323, exact attacker adopted, chase → Fighting. Facing aligned at 36787; bootstrap 36851/pending 36852, failed observation 37762. Living targetless evidence reports native combat/direct aggressor and **Attack inactive** at ticks 2060, 2080, 2100. No target damage, retry or terminal combat transition before death. Living attempts=0, no egress route. |
| 39407–39514 | Combat suspends. Repeat latches at 39417: age 24421 ms, separation 2.054 yd, body `(1810.099,-2439.740,87.999)`. ReleaseSpirit once 39456; current server corpse 39475; fresh positive Ghost 39492. At 39509–39510, Failed/recent_reclaim_redeath, retrieveAttempts=0, strategiesExhausted=yes, normalModeBlocked=yes. AFK recovery blocked. |
| 39515–40382 | Terminal Ghost retained. All later death samples have Failed, retrieveAttempts=0, routeStarts=0; no new reclaim, corpse-route restart, living entry or Grind movement. Native AFK clears at 39756–39757 following a new input-clock value; physical key held is observed. Ghost position then changes modestly, with no bot navigation/AFK delivery proof. |
| 40383–40414 | Explicit GUI stop, AFK pending evidence discarded, AttackStop and HoldPosition dispatched, BOT STOP, navigation cache teardown, unload requested. No completed unload evidence. |

All 20 NavMesh movement intents have release records: four Grind and sixteen
corpse routes, **zero living-egress, vendor or water-egress intents**. Corpse
route 5 exhausts its replan budget; route 6 reaches the approach point after
physical progress and 16 cumulative replans. Precision routes 7–10 fail with
bounded-local/surface recovery outcomes; route 11 closes to reclaim range.
Route 12 progresses from graveyard, then fails surface recovery near the corpse
(19 cumulative replans); routes 13–19 fail bounded local recovery; route 20
closes to reclaim range. Per-progress follower replan counts are not the living
owner's separate cumulative four-replan bound. Each corpse episode remains
within its 18-start/300-second controller bounds and completes. This proves
neither successful living egress nor an infinite route loop. The zero high-level
Grind navFailures counter does not erase these individual corpse route failures.

## Stun / no-attack attribution

**RUNTIME OBSERVED:** the supervisor saw a stun followed by failed attacks.
The raw log has no source-qualified stun-on/stun-off, loss-of-control, swing,
combat-error or server rejection events. It cannot locate a stun interval or
prove which input the client refused. Do not label the entire stationary period
a stun. Dispatch success means Lua ran, not that an attack landed or latched.

**SOURCE VERIFIED:** `CombatActionEvidenceScript` returns
`unknown_frame_iteration_limit` when its bounded 4096-frame traversal is
exhausted. This occurs even in the two successful opening fights. It exits
before Attack-slot/cooldown readback; consequently `attackKnown=no` does not
mean there is no Attack slot, nor does old `attackActive=no` mean inactive.
The independent bootstrap UI/cast probe is repeatedly ready: it excludes named
modal windows, targeting cursor and cast/channel state at those observations,
but is not a qualified stun or complete GCD predicate. No sustained casting,
RecoveryController or target-selection latch is evidenced in the failed fights.
The isolated target-health snapshot mismatch at 10845 resolves at 10870–10872;
it does not explain the preceding/following failure. Selection, target aliveness,
chase identity, melee distance (~2.116/2.144 yd) and facing are corroborated.

The exact verified failure chain in deployed source is:

1. `IssueLivenessAttack(...initialBootstrap=true)` spends one of the existing
   three repair attempts; `attackStarted_` awaits confirmation.
2. The full action probe remains unknown, so ordinary liveness has no permission
   to initiate a latch repair or accrue an active offensive execution window.
3. Bootstrap's >=8-second verification expires and **only logs failure**.
4. Initial startup requires Repairs()==0; another initial start is unavailable.
   The short structural pending timer can expire normally; it is **not** the
   permanent latch. Global recovery explicitly defers aligned stationary melee
   to this watchdog. Neither owner resolves the spent, unconfirmed bootstrap.
5. Combat remains Fighting. Rotation probes continue (including Overpower),
   but these are not proof of successful swings. Both failed episodes end in death.

This is a verified **missing bounded failure transition**, not proof of a
specific stun-resume bug. The first occurrence predates any living recovery;
the third repeats through its defense wrapper. The intervening living defense
actually damages the attacker. There is no evidence that living recovery
globally suppresses combat or incorrectly releases normal Grind.

**INFERRED:** failure to deal damage while losing HP plausibly contributes to
deaths 1 and 3. The missing terminal transition prolongs the stall. It does not
prove that a retry would have succeeded or that death would have been avoided.
**UNKNOWN:** why the initial Attack failed (stun/input/client/server/target
execution), exact stun timing, actual action slot used in these dispatches,
individual swing outcomes and any precise causal damage attribution.

## Corrective scope and bounded outcome

`CombatBootstrapVerificationPolicy` now keeps one dispatch-time observation
window (max of eight seconds or two recorded swing periods plus one second).
Fresh same-selected-GUID damage or known active Attack confirms it. Unknown,
stale, wait or changing attack-period observations cannot extend the deadline.
Rejected dispatch also reaches failure on the next observation. Failure sets
the existing melee terminal action with cause `initial_attack_unverified`.
Hostility/unknown engagement uses existing defensive containment; its 30-second
maximum, input/provenance checks and fail-closed outcomes remain authoritative.
With this capture's unknown full input evidence, containment may fail immediately
with an input conflict: this patch guarantees bounded ownership, **not successful
offense, escape or survival**. No additional bootstrap retry or wider input
permission is introduced. Death, target replacement and normal reset discard
old verification; verified progress retains the normal combat path.

Failure telemetry now records `decision=bounded_terminal_handoff`, current
execution evidence and explicit unknown Attack/cast status. It does not invent
a stun bit/API. Redeath now calls `YieldDeath`, preserving the existing stop/reset
behavior while logging `death_handoff` without stale living HP/evidence. The old
`water_handoff` lines 25941/39384 came from reuse of YieldWater on death, **not
from water ownership**. Water call sites and priority are unchanged.

Deterministic regression replays cover unknown full probe with decreasing player
HP, finite failed handoff, unchanged repair count, containment input failure,
successful damage/latch confirmation, stale/mismatched evidence, rejection,
clock rollback, fixed slow-weapon deadline, death/target/reset invalidation and
unchanged cast/input prohibitions. Production wiring sentinels supplement these
policy tests. Actual observed repeat geometry and distinct death/water handoff
are also covered; existing corpse/living/AFK/water tests remain required.

## Why the same location recurs

Both reclaims intentionally navigate to the **current corpse** and accept the
existing precision/delay/positive-Ghost evidence. That is reclaim eligibility,
not a threat-free staging point. No safe Ghost staging exists. The first alive
pair already loses HP 332→296, before living defense can begin. Both subsequent
living episodes observe positive engagement immediately; defense preempts all
candidate planning. The first defense closes from ~14.915 yd to melee, moving
the body outside the breaker radius; the second closes from ~8.56 yd and dies
inside the new radius. There is no living-egress route that returns the player
to danger, no early completion and no normal Grind repull after resurrection.

Grind's older `postDeathEscape=yes` flag is pending intent, not an executing
route. `TryStartDangerEscape` requires no direct aggressor and >=90% HP; normal
Grind is also held by living recovery. Neither prerequisite occurs. Hotspot
quarantine records deaths but cannot retroactively relocate a corpse or make
the first reclaim safe. The eight-yard repeat bound is an engineering condition,
not an aggro radius: broadening it from this run would change its contract.

## Other components and release gate

| Question / component | Verdict and exact limit |
| --- | --- |
| 1. Living recovery ready for RUNTIME PASS? | **NO / RUNTIME PENDING.** Entry, normal-mode exclusion and positive-evidence defense handoff observed twice. No candidate, navigation, completion, controlled release, timeout or no-route runtime path. Loaded-DLL binding incomplete. |
| 2. Repeat breaker ready for RUNTIME PASS? | **NO / binding incomplete.** The complete positive path is present, including fresh Ghost, terminal zero retrieves, retained block and no restart. One earlier redeath correctly falls outside eight yards. Exact session-specific loaded-DLL evidence is the remaining qualification gap; an old run's mapping cannot close it. |
| 3. Stun/combat recovery fixed and statically validated? | **PARTIAL.** Verified unconfirmed-bootstrap ownership gap fixed and regression tested; stun-to-resume causality and successful live attack recovery remain UNKNOWN/RUNTIME PENDING. |
| 4. New vendor qualification? | **NO.** All vendor samples Idle/trips=0; bags end 32/48, free=16. No episode, sale, return or full-bag path. Prior INSUFFICIENT EVIDENCE unchanged. |
| 5. New water qualification? | **NO.** No water block/emergency/egress. Ghost route traversal and mislabeled redeath are not living-water evidence. Existing emergency-egress RUNTIME PENDING unchanged. |
| 6. New AFK qualification? | **NO input-delivery PASS.** Native AFK detection and guard rejection observed; no bot input-delivery or prevention/recovery confirmation. Existing qualifications unchanged. |
| 7. Return to P0.7? | **READY FOR P0.7: NO.** Combat success/stun attribution, bound living-egress/release evidence and exact runtime binding remain blockers; full proposed endurance duration also missing. |

AFK becomes natively active at 18263 (`clientNow=91578433`, input clock
91278220) while Ghost; prevention at 13805 was blocked by frame_limit. During
dead/reclaim command windows, recovery/native combat and Ghost guards reject
input correctly. Terminal Ghost rejects AFK recovery as
`dead_ghost_afk_recovery_not_qualified` at 39514. At 39756–39757, native AFK
clears and input clock becomes 92078330; then physical_key_held at 39766 and
frame_limit at 39773 still block qualification. The existing prevention-only
Ghost policy permits a new safe-gap evaluation once AFK is no longer active;
that is not native AFK auto-clear or a confirmed bot action. External input is
consistent with clock/key/movement observations, but its exact origin is unknown.
All antiAfkActions counters remain zero. This explains the full terminal tail,
not just the earlier blocked excerpt.

All 15 connection diagnostic samples are healthy/valid with stable identity;
no world gap/disconnect/reconnect is exercised. Stop's
`AFK SESSION RESET reason=world_gap_or_stop` is expected cleanup, not a proved
world gap. GUI stop and unload **request** are understood; `RUNTIME DETACHED`
is a pre-unload message, not independent proof of completed DLL unload.
Historical P0.5.8 PASS remains scoped; reconnect stays SOURCE GAP / NOT
IMPLEMENTED. P0.7 and its worktree are untouched.

## Remaining substantial work

Next MAJOR milestone: **combat execution evidence and bound endurance release
qualification**. Resolve the persistent full-action frame-limit obstacle using
qualified client evidence and guard-preserving tests; obtain natural loss-of-
control/Attack activation outcomes with timestamps and input readback; exercise
the bounded failed handoff or successful combat recovery and the complete living
egress → health recovery → controlled Grind release on one fixed, demonstrably
loaded source/DLL. Preserve raw/lifecycle logs, pre/post hashes, process/module
binding and >=20 minutes of supervised natural play, with separate component
verdicts. Do not provoke death or waive input guards to produce a PASS.

Safe Ghost staging remains **SOURCE GAP**: no qualified Ghost visibility coverage,
exact unselected hostile eligibility or positive candidate-safety predicate.
First unsafe reclaim and defense before the second alive probe remain unresolved.
Healthy-anchor provenance and event-based combat/damage history remain intentionally
deferred until justified. Geometry or absent visible mobs supplies none of those
proofs. This task closes the verified failure transition and misleading telemetry;
it does not manufacture an unobserved fix or broaden any historical PASS.

## Validation and checkpoint scope

`PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4` passes **113 C++
tests, 42 audit Python tests, 13 QuestDB Python tests, SQL fixtures and 11 Lua
suites**, full DLL build and diff check. Report
`/tmp/wow-validation-d_o2gs0_/results.json`, console `/tmp/endurance-validation.log`.
Separate `cmake --build build` also passes. No new client run was launched.
Only related source, deterministic tests and audit/handoff documents belong to
this authorized checkpoint on `codex/vendor-afk-long-navigation`; captures,
build outputs and unrelated projects remain excluded. No merge or P0.7 changes.
