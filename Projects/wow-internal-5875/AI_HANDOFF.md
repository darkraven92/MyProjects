# AI handoff

## 2026-10-10 DeathRecovery reliability milestone consolidated

**IMPLEMENTED / TEST PASS / BUILD PASS; new behavior RUNTIME PENDING.** One
coherent milestone from clean `dd33c4f18207b8d70051a3208f25e8f99ba2cb9b`, on
`codex/vendor-afk-long-navigation`, single coordinator, no subagents. Read
AGENTS.md, source/history and the complete DeathRecovery audit chain. No new
WoW run, runtime capture commit, merge or P0.7 work.

The authoritative current ownership, evidence, budgets, acceptance matrix and
remaining blockers are now consolidated in
[DEATH_RECOVERY_CURRENT_MODEL.md](docs/DEATH_RECOVERY_CURRENT_MODEL.md). Earlier
sections below are historical checkpoint descriptions. The main bounded living
owner/egress was already implemented; this milestone closes source-verified
integration gaps rather than replacing that architecture:

- Health, life and native combat have independent knownness. A failed unrelated
  read no longer hides positively observed engagement; unknown life still cannot
  authorize a command. Danger is Observed or Unknown, never Safe.
- Targetless, read-only Attack-action evidence reuses the qualified Vanilla
  action APIs, with finite scanning, explicit unknown and caught script errors.
  No Attack action slot/read means unknown and bounded failure, not permission
  to release. No target selection or action-bar changes are made to obtain it.
- Command-time and water-handoff reads update the same pressure/quiet history;
  brief damage followed by healing or a brief combat signal cannot be forgotten
  by the next loop snapshot. These reads never count as completion proofs.
- Same-character manager/player/descriptor replacement invalidates the living
  episode. Command-time gaps propagate to the existing repeat-history reset
  before the next death entry. Living completion still preserves that history.
- Eligible water safety can cancel living health recovery; resting alone no
  longer makes water handoff fail. Existing water priority/guards remain.

Corpse/reclaim, two alive probes, repeat bounds, four living candidates, four
cumulative replans per execution and 90-second living deadline remain. Defense
and existing water arbitration preempt living work; normal Grind/Quest remain
blocked until controlled release. Terminal recovery has no autonomous retries
or alive-only release; its passive manual-recovery interlock does not expire
into normal Grind. Existing Grind post-death escape is retained.

First unsafe reclaim and the interval before the second alive probe remain
unresolved. Safe Ghost staging is SOURCE GAP: no qualified visibility coverage,
exact unselected hostility or positive candidate safety predicate. Existing
last-alive/body/Grind/water anchors cannot be promoted to safe recovery anchors;
provenance-qualified anchor history and event-based damage are intentionally
deferred. No guessed aggro radius, terrain API or reconnect work is introduced.
Historical P0.5.8 PASS retains its scope; repeat-death and living recovery remain
RUNTIME PENDING. Vendor/water/reconnect/unload statuses are unchanged.

Validation PASS: **112 C++ tests, 42 audit Python tests, 13 QuestDB Python tests,
SQL fixtures and 11 Lua fixture suites**, including 10 new read-only Attack cases.
Command: `PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4`.
Report `/tmp/wow-validation-vcq1mg47/results.json`; console
`/tmp/death-milestone-validation.log`. The cross-policy milestone test was rerun
after the final gap-propagation sentinel; separate `cmake --build build` PASS
(up to date). Tests/source assertions are not live adapter/physical-motion proof.

**Next MAJOR project milestone: Bound supervised Grind endurance qualification
and release gate.** One fixed source/DLL-bound campaign of >=20 minutes normal
supervised Grind, with full raw/lifecycle logs and pre/post hashes/mapping, to
qualify natural death → living defense/egress/recovery → resume and naturally
encountered failure/redeath while correlating water/vendor/AFK ownership. Keep
component verdicts separate when paths are absent. No further small DeathRecovery
source task is justified without new failure evidence or a closed source gap.

## 2026-10-10 bounded living post-resurrection recovery

**IMPLEMENTED / RUNTIME PENDING.** Single coordinator; no subagents. Continued
`af8e3b84b7088ea617e41bca1c86903524638dd0` on
`codex/vendor-afk-long-navigation`. Read AGENTS.md, this handoff, source/history
and the preceding audits. No new WoW session or runtime qualification. The
older sections below describe their respective checkpoints, not current source.

Automatic reclaim still requires the existing two fresh alive probes and arms
the unchanged repeat-death history. Instead of immediately resuming Grind,
WorldMonitor starts a separate `LivingDeathRecoveryController`. It holds normal
Grind/Quest acquisition, roam, vendor, loot and watchdog activity. Manual-alive
completion retains its existing resume path. A subsequent death returns to
DeathRecovery without resetting the repeat record.

The living owner uses fresh, identity-checked native combat/life fields and a
bounded targetless object scan. Positive target-to-player evidence permits only
exact-attacker defense through CombatController; absence of observed attackers
is never called safe. HP loss renews the quiet interval without assigning a
cause. Existing living-water arbitration runs first; no Ghost water exemption
is reused. AFK runs through its existing recovery guard.

After two quiet seconds with qualified observations, at most four candidates
18 yards from the confirmed-alive position are tested with planning-only
Detour/NavMesh reachability. Full destination reachability and at least 12 yards
projected/actual displacement are required. Living water avoidance, directed
transition memory and a four-replan limit apply. These are displacement budgets,
not hostile aggro radii or safety guarantees. After arrival, existing health
recovery runs; >=95% HP, three spaced fresh calm observations, final fresh
identity/life/water/engagement checks and a standing dispatch gate release normal
ownership on the next snapshot.

The entire active attempt has a 90-second steady deadline, including preemption.
No route, exhausted candidates/replans, identity/world loss, failed stop/posture
or deadline leads to a terminal manual-recovery interlock. It performs no new
automatic navigation/retry/release; defense and the existing water owner remain
available. This passive normal-mode block intentionally has no automatic expiry;
stop/restart or a new death ends the living episode. Merely remaining alive does
not clear it. Explicit world gaps invalidate route, evidence and stale defense
intent; configured map remains the existing map 1, not a new live-map capability.

See [implementation, limits and runtime acceptance](docs/DEATH_RECOVERY_AUDIT.md#bounded-living-recovery-and-navmesh-egress-2026-10-10).
The first unsafe resurrection (including danger during the two alive probes)
is unresolved. Safe pre-resurrection staging remains unimplemented. Living
egress and the repeat-death breaker remain RUNTIME PENDING. Historical P0.5.8
PASS, vendor INSUFFICIENT EVIDENCE, water emergency egress RUNTIME PENDING,
reconnect SOURCE GAP / NOT IMPLEMENTED and unload evidence limits are unchanged.
P0.7 untouched; no captures staged, no merge.

Validation PASS: `PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4`
completed **110 C++ tests**, registered Python/SQL/Lua suites, full DLL build and
diff check. Report: `/tmp/wow-validation-jwp2i55y/results.json`; console:
`/tmp/death-living-recovery-final-validation.log`. The living policy/integration
test was also compiled and run again after the cumulative-replan guard change.
These are static/build results, not runtime evidence.

## 2026-10-10 bound DeathRecovery runtime qualification

**INSUFFICIENT EVIDENCE / repeat-death circuit breaker RUNTIME PENDING.** The
complete `runtime-captures/death-repeat-bound-2026-10-10/` raw session did not
exercise any death/reclaim path. All five capture-manifest entries verified;
all 21 extract records match raw lines. Single coordinator, no subagents,
clean starting HEAD `f022de67b3e30ef0202afd2c8ab19819de329b08` on
`codex/vendor-afk-long-navigation`. No source/test changes or new WoW run.

Deployment binding is accepted as explicitly **user-attested**: Linux PID
124315 mapped the exact project `build/wow_internal.dll`, device `103:05`,
inode 4980769. Local stat agrees (device 66309); pre/post HEAD and DLL SHA256
are unchanged, with clean Git state and an explicit no-replacement attestation.
DLL SHA256: `bc2a148d9f251121831f0400f10961af4fafbc894300b47e0db1cb2121268745`.
The interactive `/proc` observation was not automatically saved;
`loaded_dll_mappings:` is blank. Do not describe it as an independently captured
process-map artifact or apply this binding retroactively to the earlier run.

Raw SHA256: `af3475df0c1c0a5e6558c421c3d7bed9c5aec6ebe0e915b2f8a2f8beab77ec9f`.
Raw session `492.134361144754615960.83558443.700` uses Wine PID 492, GUID
112743. Its 6,463 lines cover BOT START/STOP 83558961–83837304 (4m38.343s),
not the whole interval of the deployment notes' multiple start/stop entries.
Lifecycle 189–199 identifies this run and its preceding log clear; earlier
journal sessions do not supply missing gameplay. All 44 DeathRecovery samples
are Idle with zero attempts/recoveries/routes; all 44 normal-mode samples have
`normalModeUpdate=yes deathOwner=no`. Player HP stays positive (664→185),
one target dies and is looted, then GUI stop performs cleanup/unload request.
Completed DLL unload is not independently proved. Living AFK action is blocked
by `frame_limit`, then native combat; terminal Ghost AFK is not exercised.

Missing for PASS: one bound natural reclaim→two fresh alive probes→armed→
nearby redeath→body/server latch→release→fresh Ghost→`recent_reclaim_redeath`
terminal with zero retrieves, retained normal-mode block, no reclaim/route
restart, correct Ghost AFK gating and understood stop. Do not borrow the
earlier unbound 8567-ms sequence or provoke death. Full hashes, raw chronology,
acceptance matrix and source checks are in the
[bound runtime audit](docs/DEATH_RECOVERY_AUDIT.md#bound-runtime-qualification-2026-10-10-repeat-path-not-exercised).

Recommended next substantial engineering task: **bounded living recovery and
NavMesh egress after resurrection**, delivering the fresh targetless evidence
adapter, distinct living owner, defense-only handoff, living-water/AFK
arbitration, finite deadline/blocked outcome, release criteria, telemetry and
integration tests together. The audit defines acceptance. Existing Grind
post-death escape is present but waits for >=90% HP and no aggressor. Safe
Ghost staging still lacks qualified threat coverage; this proposal must not
claim the first unsafe reclaim safe or reuse Ghost water exemptions/alive-only
terminal release for a living safety owner. No implementation is added here.

Historical P0.5.8 PASS is unchanged. First unsafe resurrection unresolved;
safe staging/living egress NOT IMPLEMENTED; vendor INSUFFICIENT EVIDENCE;
water emergency egress RUNTIME PENDING; reconnect SOURCE GAP / NOT IMPLEMENTED.
Full validation PASS: **109 C++ tests**, registered Python/SQL/Lua suites,
full build (up to date) and diff check, using
`PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4`.
Report: `/tmp/wow-validation-4ityd74_/results.json`; console:
`/tmp/death-repeat-bound-validation.log`. Capture hashes and whole-session
counts were cross-checked again. These checks do not qualify the absent
runtime path. Authorized checkpoint scope is this handoff and the two audit
documents only; captures remain local. P0.7 untouched. No automatic merge.

## 2026-10-10 DeathRecovery continuation: bounded repeat-death block

**IMPLEMENTED / RUNTIME PENDING.** Continued clean checkpoint `602bf2b` on
`codex/vendor-afk-long-navigation`; no uncommitted task changes existed. Three
parallel read-only reviews covered architecture, Ghost threat evidence and
NavMesh/egress. P0.7 worktree untouched. Historical P0.5.8 RUNTIME PASS remains
qualified for its original scope.

The new `DeathRecoveryRepeatDeathPolicy` records actual confirmed-alive XYZ,
player GUID/map and steady time after automatic reclaim's two fresh alive
probes. A next death within 120 seconds and eight yards (3D, inclusive) latches
a repeat verdict using observed body or current server corpse evidence. Ghost
graveyard/persisted/last-healthy positions are never used. Age is frozen at
death, so a corpse run cannot expire a positive verdict. Normal bounded release
still runs; fresh Ghost confirmation enters existing `Failed` with reason
`recent_reclaim_redeath`, before another automatic reclaim. Cached Ghost evidence
waits for a fresh probe without routing. No safety retry, added navigation,
new living owner or combat/water/AFK priority changes.
Map matching uses the existing controller-supplied map (currently map 1), not
a newly qualified live map reader; cross-map support remains out of scope.

Automatic rearm preserves only this short session-local history. Explicit reset,
world gap or confirmed manual-alive completion clears it; terminal ownership
still requires its existing two fresh same-character alive probes to release.
Telemetry: `DEATH SAFETY repeatDeath=armed/latched`, identity/positions, age at
death, separation, source, unknown threat coverage, then existing terminal log.

This is a circuit breaker against the observed repeating outcome, **not a fix
for the first unsafe resurrection**. The 120-second/eight-yard neighborhood is
an engineering bound, not an aggro measurement. It may stop after deaths from
other causes and cannot protect outside the window/radius, before the initial
alive confirmation, or across world/session gaps. Safe staging and living egress
remain unimplemented: Ghost visibility is unqualified, and a living owner needs
defense-only handoff plus correct water/AFK arbitration. The prior design and
seven future acceptance cases remain in the [DeathRecovery audit](docs/DEATH_RECOVERY_AUDIT.md).

Deterministic coverage added for repeat geometry, time/3D boundaries, unknown/
foreign/stale evidence, Ghost bootstrap, sticky verdict and reset/rearm lifetime,
with production integration sentinels. Full validation PASS: **109 C++ tests**,
registered Python/SQL/Lua suites, full build and diff check. Report:
`/tmp/wow-validation-dw90ybrq/results.json`; log:
`/tmp/death-repeat-safety-validation.log`. Final guard against strategic budget
renewal for a latched verdict received another targeted test and full build.
No new WoW session launched; capture hashes below were rechecked. Next runtime
must bind DLL/revision and show natural reclaim→nearby redeath→latch→release→
fresh Ghost→terminal block without another reclaim/navigation restart. Also
verify retained Ghost AFK and two-probe manual-alive release if that occurs.
Do not intentionally kill the character to qualify it. All earlier vendor,
water and reconnect verdicts remain unchanged.

## 2026-10-10 DeathRecovery location safety investigation

**Safety gap observed and source verified; safe staging/living egress NOT
IMPLEMENTED because the required sensing is unqualified.** This uses the user's
explicit documentation fallback. See [DEATH_RECOVERY_AUDIT.md](docs/DEATH_RECOVERY_AUDIT.md)
for raw-line evidence, exact source gaps, bounded design and future test cases.
Starting checkpoint `6abd7ac3ff89e1c96f25e2869e8259499f8134e7`, same requested
branch `codex/vendor-afk-long-navigation`, initially clean. Three requested
parallel agents performed read-only architecture, hostile-evidence and NavMesh
reviews. No production source or test behavior changed.

A newer build log was preserved at `runtime-captures/death-safety-2026-10-10/`
(raw and lifecycle, locally Git-excluded). Raw SHA256:
`53a8cddeb95f64e15f440d525f65afad29959da50fd017c798b5cb5dd54bf223`.
It contains two automatic reclaims at 6.397/5.310 yards, immediate/nearby defense
against entry 3928 GUID `0xF130000F58007F25`, and two subsequent deaths at the
resurrection positions. First resurrection already has combat/damage during
alive confirmation. Third death ends in unsafe-terrain failure followed by
`manual_alive_confirmed`; that does not prove a third automatic reclaim.
No new WoW session was launched and no exact deployed DLL hash was captured.

Verified control flow: reclaim uses current server anchor + fresh Ghost/delay
evidence + <=8-yard precision; two fresh alive probes go straight to Done and
Combat/Grind resume. Existing Grind post-death escape is armed but requires
>=90% HP and no direct aggressor, so recovery/defense runs first. Do not describe
this as absent post-death protection or a voluntary Grind pull.

Blockers: Ghost WorldState does not establish living-threat visibility coverage;
type-3-only enumeration/read diagnostics do not certify absence of danger;
target GUID/faction lack individual read-knownness; attackable/density proxies
are not verified hostility or aggro radii. Positive threats support rejection
or relative ranking, not a positive safe-point predicate. NavMesh planning and
projection already exist, but cannot certify threat occupancy. Do not broaden
reclaim to the 32-yard coarse policy constant.

Before implementing retained living DeathRecovery ownership, add qualified
targetless combat/aggressor evidence, a defense-only handoff and living water
arbitration. Current `deathShouldOwn` exempts death from living-water handling;
the persistent Ghost-confirmed latch also selects Ghost route permissions.
Neither may authorize living egress. Timeout must block without surrendering
defense or releasing through Failed's existing alive-only reset path. Preserve
all finite death/nav budgets and invalidate safety proof across world loss.

Historical P0.5.8 RUNTIME PASS is preserved for its original reclaim/route/resume
scope. Safer resurrection is a new gap, not runtime-fixed. AFK/water/reconnect
and the earlier vendor INSUFFICIENT EVIDENCE verdict are unchanged. User
authorizes only task-related checkpoint/push on this branch; never merge.

Validation of unchanged production behavior: 108 C++ tests, registered
Python/SQL/Lua suites, full build (up to date) and diff check PASS via
`PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4`.
Report: `/tmp/wow-validation-f45nnesi/results.json`. This is not verification
of an implemented safe-resurrection policy; the seven requested deterministic
cases are documented as future acceptance criteria, not claimed as passing
feature tests. Checkpoint scope: this handoff and the death/runtime audit docs.

## 2026-10-10 vendor runtime qualification

**INSUFFICIENT EVIDENCE** for the bounded automatic vendor episode. See the
current section of [the runtime audit](docs/RUNTIME_CONNECTION_RELIABILITY_AUDIT.md).
Inspected source checkpoint: `65719e3942db12c90e73eff32b10d2dc37a1f4fb`, branch
`codex/vendor-afk-long-navigation`. No verified vendor bug or source/test change.

Capture: `runtime-captures/vendor-runtime-2026-10-10/`. Raw log, lifecycle log
and all 240 extract records were checked. The requested `runtime-audit.json`
was absent and has been regenerated with the unchanged audit tool; hashes and
reproduction command are in the audit. Captures remain locally Git-excluded.
This handoff file did not previously exist in the scoped project.

Food episode starts at tick 509. Candidate 7952 fails five interactions;
3933 opens MerchantFrame but food is unaffordable (19 copper versus price
4000). Return completes at 804 with maintenance unmet, so no episode completion
is proved. Existing 2400-tick Grind cooldown explains expiry at 3204 while
Vendor is Idle. One allowed retry starts at 3212 and is preempted by an
aggressor at 3270. The subsequent stopped wait keeps its service latch despite
19 free bags and only one food. Last explicit session tick is 3600; second
attempt deadline 3692 is not observed. User stops before any recorded terminal
outcome. This proves neither successful restock nor the full bounded retry
sequence; do not label the late Idle expiry an overlong navigation intent.

Next runtime needs a complete natural success or bounded failure/retry outcome,
including active-route deadline cancellation, stopped-state evidence, cooldowns,
and terminal MaintenanceBlocked or verified maintenance/two fresh bag reads.
Record the deployed DLL hash/revision; this capture lacks exact binary binding.
No new WoW runtime was launched for this audit.

Validation: `PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4`
passed all 108 C++ tests, registered Python/SQL/Lua suites, full project build
(up to date), and diff check. Local report:
`/tmp/wow-validation-eqe71496/results.json`. Static/build PASS does not change
the runtime verdict. The checkpoint contains only this handoff and the two
updated runtime/vendor audits.

AFK: zero verified actions, one threshold crossing, clientActive=yes with
server active=no; safe stopped-workload logs are not delivery proof. Keep all
guards/thresholds. Water emergency egress stays RUNTIME PENDING. Reconnect stays
SOURCE GAP / NOT IMPLEMENTED; audit runtimeQualified=false. No boundaries were
weakened and earlier independent qualifications are not expanded.

Git scope is only this project. On-disk AGENTS.md names an older branch and
says no automatic commit unless explicitly requested; it contains no automatic
checkpoint/push procedure. The user's current request explicitly authorizes a
checkpoint and push on the current branch. Never merge automatically or include
unrelated files, generated captures, build/state/cache artifacts.
