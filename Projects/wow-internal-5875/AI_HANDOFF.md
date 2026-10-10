# AI handoff

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
