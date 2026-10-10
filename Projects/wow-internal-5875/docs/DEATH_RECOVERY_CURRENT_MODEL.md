# DeathRecovery current reliability model

This is the normative current model after the 2026-10-10 endurance incident,
continued from `4019517a40793654e3bef7335b6d1387afbb684d`. The chronological
[DeathRecovery audit](DEATH_RECOVERY_AUDIT.md) retains evidence and historical
verdicts; its earlier proposals describe their source checkpoints. The complete
[endurance audit](GRIND_ENDURANCE_INCIDENT_2026-10-10.md) records runtime observations
and exact binding limits. **READY FOR P0.7: NO.** No P0.7 work.

The endurance run exercised two automatic reclaims/living defense handoffs,
three deaths and one matching repeat-death terminal, but no living egress or
normal-mode release. All capture hashes verify; deployment/local source and
on-disk DLL agree, while session-specific loaded-module binding remains unknown.
No formal runtime PASS is added. Combat's failed bootstrap now transfers to the
existing bounded terminal/containment owner instead of merely logging expiry
and remaining in Fighting. This preserves all defensive/input permissions; an
unknown input probe may still cause terminal failure, not successful escape.
Stun causality and successful live combat recovery remain unproved. Redeath
cleanup has its own death-handoff telemetry; it is not water preemption.

## Current state and ownership

| Priority / state | Commands and ownership | Exit / bound |
| --- | --- | --- |
| Unavailable or mismatched world identity | No commands through stale/foreign player pointers. Living route, proofs and defense intent become invalid. | Living manual-recovery block. Corpse evidence follows existing world-gap policy. |
| Dead/Ghost DeathRecovery | Suspend normal workloads; bounded spirit release, fresh Ghost, current server corpse anchor, validated corpse routing, precision reclaim. | Two fresh alive probes after issued reclaim, or terminal Failed. |
| Living-water block | Existing water neutralization/backtrack/terminal policy runs before living recovery. No Ghost traversal exemption. | Existing bounded backtrack and three non-swimming observations, or manual recovery/death handoff. |
| Living self-defense | Living owner cancels its route/recovery and permits exact observed-attacker adoption or existing active CombatController defense. | Fresh complete disengagement; combat failure remains terminal. |
| Living recovery | Hold ordinary Grind/Quest pulls, roam, vendor, loot and watchdog restart. Observe, preflight, execute bounded NavMesh displacement, recover health. | Controlled release after explicit criteria below, or terminal manual-recovery interlock. |
| Normal Grind/Quest | Existing behavior, including Grind danger memory/post-death escape. | Available only after living completion/manual-alive corpse exit and a fresh subsequent update. |

Water has the existing higher priority even when danger is present: no conflicting
combat chase/route is introduced during its block. Combat intent/autoattack is
not turned into an ordinary pull. This milestone does **not** add a new stationary
combat executor inside a water block or qualify hostile-water survival. That
coincidence retains the existing water manual-recovery behavior. Outside water
ownership, positive engagement retains self-defense even after the living
recovery deadline. Normal modes never use defense permission to acquire work.

A living health-recovery action is cancelable by eligible water safety. The
previous `!Recovering()` handoff gate could force the water policy directly to
failure simply because the living owner was resting; it is removed only for
this owner. Water's existing identity, life, native/observed threat, combat-state,
movement and local-anchor restrictions remain in force. No water policy budget
or qualification changes.

### Corpse flow and historical qualification

`DeathRecoveryController` still owns ReleasingSpirit → WaitingForGhost →
AwaitingCorpseAnchor/RoutingToCorpse → WaitingForReclaim → WaitingForAlive.
Observed/persisted body positions are routing evidence only. Reclaim requires
current same-map server corpse location, fresh Ghost, known expired delay and
the existing eight-yard precision condition. Dispatch is not resurrection:
the same two fresh alive probes remain mandatory. Release attempts 6, retrieves
8, route attempts 18, stationary failures 9, full-map allowance 1, no-progress
180 seconds and total corpse episode 300 seconds are unchanged.

Historical **P0.5.8 RUNTIME PASS** covers natural death, Ghost, retained learned
directed-edge constraints across corpse variants, reclaim at 6.301 yards, two
alive probes and normal resume. Individual full-map tiers and later living
recovery are not retroactively qualified by that run. Existing source/test
regressions continue to guard those mechanics. This new handoff deliberately
inserts living recovery after automatic confirmation; it does not revoke or
broaden the historical result.

Automatic completion arms repeat history and immediately rearms the corpse
controller, but resumes Combat only through the living defense wrapper. Normal
Grind/Quest resume is withheld. Manual-alive completion from terminal corpse
failure keeps its original two-probe confirmation/reset/resume path and never
inherits an automatic living episode.

## Evidence contract

`CombatClientEvidence5875::Living` reads independent health, life/Ghost and native
combat knownness on the game thread. An unavailable combat field cannot erase
an independently observed unit targeting the player; conversely a positive
combat bit may be reported even if life is unknown, but unknown life cannot
authorize commands. Its bounded 4096-object scan checks type-3/type-4 health and
target GUID, reports partial enumeration, and verifies player identity before
and after reads. Target-to-player is conservative engagement evidence, not a
proof of exact hostility. Only a matching type-3 WorldState unit may become an
exact defense target; no new PvP target adapter is invented.

`LivingAttackEvidenceScript` adds a read-only, targetless observation using the
already source/runtime-used Vanilla `IsAttackAction` and `IsCurrentAction` APIs.
It checks at most 120 action slots, resets its result before reading, and catches
missing APIs/errors. It neither selects a target nor issues input. Attack active
is positive attack involvement, not proof of a successful swing, incoming damage,
or the identity of an attacker. Inactive is a known observation of that action,
not proof of safety. A missing Attack action slot/API/read remains **Unknown**;
this deliberately prevents quiet egress/release and reaches the same deadline.
The mode therefore needs a readable Attack action slot to complete automatically.
No action bar is edited to satisfy that requirement.

The evidence type has only `Observed` and `Unknown` danger values—no `Safe`.
Known, complete, non-engaged observations can authorize a **bounded recovery
attempt**, never a statement that the surroundings are threat-free. Native
combat, direct targeting, active Attack and sampled HP loss are positive pressure.
HP loss has no inferred cause and cannot detect damage completely hidden between
samples; combat-log event history and future incoming attacks remain unknown.

All command-time and water-handoff observations now enter the same pressure/
quiet history as the main update. A transient hit followed by a heal, a brief
combat/Attack signal, or an incomplete read cannot disappear just because the
next main-loop snapshot looks calmer. These observations cannot spend candidate
slots or advance the three completion proofs. No observation extends the
absolute deadline. Every movement/recovery/release dispatch still requires a
fresh command-world check.

Living identity is the captured GUID, object manager, local-player address and
descriptor address, not GUID alone. Replacing any member blocks continuation.
Monitor-visible gaps and command-time gaps both invalidate repeat history using
the existing reset policy before the next death entry. Configured map remains
the existing map 1; no new live map or seamless cross-map capability is claimed.

## Egress and terminal contract

- One 90,000-ms monotonic active episode, including defense/water waits. No
  progress, candidate or replan refreshes it.
- At most four candidate/preflight attempts, 18 yards from confirmed-alive XYZ.
  No route is hard-coded. Each accepted candidate must have a full Detour/NavMesh
  preflight and >=12-yard projected displacement, then fresh execution guards.
- Living `AvoidUntilQualified` water restrictions, existing terrain validation,
  episode-local directed-transition avoidance and no full-map fallback remain.
  Execution stops at four cumulative follower replans, not the progress-reset
  stall counter. All searches/loading are also subject to the episode deadline.
- Physical arrival requires >=12-yard actual displacement. Health recovery then
  uses the existing RecoveryController. Release requires alive identity, >=95%
  HP, no positive engagement/defense/water owner, a two-second observed quiet
  interval, three observations >=250 ms apart, and final fresh world/health/
  displacement plus standing-dispatch checks. Completion is committed once.
- Unknown evidence, damage or preemption invalidates partial proof. Exhaustion,
  deadline, world/identity loss or required stop/posture failure stops automatic
  recovery and enters a documented manual-recovery interlock. No fresh search,
  ordinary pull, watchdog reset or alive-only auto-release can restart it.

The 18/12-yard values are finite displacement objectives, **not hostile aggro
radii**. NavMesh reachability is not hostile safety. A reachable destination can
still be dangerous. Defense may move the player; loss of required displacement
after arrival prevents release.

The terminal interlock is intentionally persistent, not an unbounded active
recovery operation. Autonomous recovery work has stopped; normal-mode release
requires explicit session stop/restart or a subsequent death transferring to
the corpse controller. Defense and eligible water policy remain available.
Silently expiring the interlock would resume Grind beside a failed egress.

### Anchor decision

No existing anchor qualifies for preferred general living egress. The corpse
controller's `LastClearlyAlivePosition` stores coordinates/availability from a
positive-life sample, not a timestamped healthy, disengaged, water-qualified
recovery history. Persisted body/server corpse points have a different purpose.
Grind sectors encode destination/danger heuristics, not proof of historical
healthy occupation. Water's recent non-swimming anchor is qualified only for
its local <=12-yard, <=4.5-second backtrack and does not prove dry ground. These
sources remain in their own owners; none is relabeled a safe recovery point.
A reusable living anchor needs explicit same-world/time/health/engagement/water
provenance and fresh NavMesh validation. Adding speculative provenance or
peripheral terrain reverse engineering is intentionally deferred.

## Repeat death and AFK invariants

Automatic reclaim records identity/map/actual confirmed-alive XYZ and time.
A same-player/map next death within 120 seconds and eight yards latches on the
observed body/current server corpse, never the Ghost position. Age freezes at
death. Normal release still runs; fresh Ghost then enters terminal
`recent_reclaim_redeath` before routing/reclaim, with zero new retrieves. Living
completion/reset cannot erase that history. Manual-alive completion, explicit
session reset and world/evidence gaps clear it. Death beyond the original
window/neighborhood is not newly covered by this milestone.

AFK receives the existing recovery guard on every living/terminal tick. Native
combat, water and other safety guards remain unchanged. Dead/Ghost qualification
is separate prevention-only evidence: already-AFK Ghost still blocks with
`dead_ghost_afk_recovery_not_qualified`, and command-in-flight is not a harmless
AFK gap. Historical qualified Ghost prevention is not revoked or generalized.

## Final classification and stop boundary

| Item | Implementation / remaining qualification |
| --- | --- |
| Corpse routing/reclaim, directed-edge memory, two alive probes | IMPLEMENTED; historical P0.5.8 RUNTIME PASS within original scope. |
| Nearby repeat-death breaker | IMPLEMENTED / RUNTIME PENDING. Endurance positive path observed at 24421 ms/2.054 yd; exact loaded-DLL binding is missing. Earlier 8.942-yd redeath correctly falls outside the contract. |
| Living ownership, targetless evidence, defense/water arbitration, bounded NavMesh egress and fail-closed terminal | IMPLEMENTED / RUNTIME PENDING. Two entries/defense handoffs hold normal modes; zero egress attempts, no completion/release or living timeout. |
| Unconfirmed Attack bootstrap failure ownership | IMPLEMENTED / TEST PASS / BUILD PASS; RUNTIME PENDING. Existing bounded combat terminal handles expiry/rejection; successful post-stun attack recovery remains unknown. |
| Manual-alive/reset/world-gap and AFK invariants | IMPLEMENTED; deterministic regression coverage, new integrated runtime path pending. |
| Safe Ghost staging / first unsafe resurrection | SOURCE GAP. No qualified Ghost visibility coverage, unselected exact hostility or positive candidate safety predicate. Empty/partial scans cannot close it. |
| Defense between reclaim dispatch and the second alive probe | Intentionally deferred; existing Ghost/alive confirmation contract is preserved. A separate qualified life/command handoff is required before changing it. |
| Provenance-qualified historical living anchors / combat-event damage history | Intentionally deferred; current records do not supply the necessary evidence. |
| Cross-map recovery, hostile-water stationary defense, new reconnect/terrain readers | Outside this milestone; existing source/runtime blockers remain. |

The earlier `death-repeat-bound-2026-10-10` session contains no death/reclaim
path; its historical `f022de6` mapping attestation cannot be transferred.
The new `grind-endurance-2026-10-10` session contains the repeat path but lacks
exact loaded-module binding. These complementary gaps cannot be combined to
manufacture a qualified run. The full new raw/manifest/lifecycle analysis is
in the endurance audit; earlier hash checks remain historical audit records.

### Acceptance matrix

| Required behavior | Deterministic coverage |
| --- | --- |
| Automatic entry / no normal pulls or roam | `living_death_recovery_policy_test`, `death_recovery_milestone_test` policy and production-owner sentinels |
| Positive evidence / defense / known versus unknown | `living_recovery_evidence_test`, 10-case read-only Lua fixture, existing combat tests |
| Water preemption | Cross-policy water entry/exit scenario and all 16 danger/defense/water/unknown combinations; handoff source guard |
| Bounded reachable egress, release, no route, timeout/replan exhaustion | Living policy tests, command-read versus proof-count tests, follower cumulative-counter integration guard |
| World/object replacement and manual-alive | Four identity-member mismatches, world-gap/rollback/foreign identity tests, manual entry and gap-propagation sentinels |
| Repeat evidence retained after success / terminal latch | Completed living episode followed by nearby redeath; full existing repeat-policy boundary/reset tests |
| AFK during recovery/dead/Ghost | Cross-policy recovery block and Ghost already-AFK/in-flight guards; existing AFK suites |
| P0.5.8 mechanics retained | Existing death ownership/evidence/liveness/transition tests plus historical 6.301-yard reclaim/two-probe and unchanged-budget checks |
| Failed bootstrap cannot silently retain Fighting | `combat_bootstrap_verification_test` replay/proof/deadline/reset scenarios and terminal-wiring sentinels |
| Endurance redeath geometry and correct handoff label | Actual outside/inside-eight-yard coordinates in repeat-policy test; death versus water call-site sentinel |

Source integration sentinels complement executable policy/Lua tests; they do
not emulate the live client, dispatch timing or physical NavMesh traversal.
Validation results are recorded in the handoff and audit for this checkpoint.

## Exact next major project milestone

**Combat execution evidence and bound endurance release qualification.** Close
the persistent full-action frame-limit obstacle without weakening input guards,
correlate natural loss-of-control/Attack activation and validate the corrected
bounded handoff or successful recovery. Use at least 20 minutes of normal
supervised Grind on one fixed, demonstrably loaded source/DLL with full raw/
lifecycle capture and pre/post hashes/mapping (label attestation as such).
Exercise natural living defense → NavMesh egress → health recovery → controlled
Grind release, keeping repeat terminal/no restart and water/vendor/AFK verdicts
separate. Do not provoke death or call absent threats safe to fill a row.

The endurance failure transition and misleading death/water label are corrected;
the remaining combat input and qualification gaps require a coherent milestone,
not another speculative DeathRecovery helper. Runtime paths must either be observed
and qualified or remain explicitly pending. Safe Ghost staging, reconnect and
completed DLL unload cannot be awarded PASS from ordinary living gameplay.
