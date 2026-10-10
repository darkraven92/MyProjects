# AI handoff

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
