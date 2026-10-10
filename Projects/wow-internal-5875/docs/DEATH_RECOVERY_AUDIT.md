# P0.1 DeathRecovery reliability audit

## Consolidated reliability milestone (2026-10-10)

**IMPLEMENTED / TEST PASS / BUILD PASS; integrated runtime qualification PENDING.**
The single current-state model is
[DEATH_RECOVERY_CURRENT_MODEL.md](DEATH_RECOVERY_CURRENT_MODEL.md). It supersedes
historical implementation/proposal descriptions below while retaining their
runtime verdicts. One coordinator continued clean `dd33c4f` on
`codex/vendor-afk-long-navigation`; no subagents, WoW session or P0.7 work.

The existing bounded living owner already delivered the main post-resurrection
behavior. Source review of the full corpse → living → normal/redeath chain found
four related gaps, closed together here:

| Source-verified gap | Change and deterministic regression |
| --- | --- |
| Living native reads used one short-circuit aggregate; an unavailable unrelated field could hide independent positive engagement. | Independent health/life/combat knownness; positive engagement survives partial reads, but unknown identity/life never authorizes commands. Evidence tests cover each signal and incomplete combination. |
| No targetless Attack-action read in living recovery. | Bounded read-only existing IsAttackAction/IsCurrentAction APIs, fresh result reset and pcall; active/inactive/unknown. Missing slot/API/read cannot establish quiet release. Ten Lua cases prove bounded/error/unknown behavior without input. |
| Command-time rejected evidence did not always enter the main quiet history. | Shared Observe path for every command/water read; transient hit/heal, combat/Attack and unknown samples reset quiet/proofs without consuming attempts or manufacturing proofs. |
| GUID-only living identity and gaps occurring inside command reads could outlive old context; resting also blocked water handoff. | GUID/manager/player/descriptor continuity, propagated evidence-gap reset before subsequent death, and cancelable living health recovery under eligible water ownership. Existing water guards and corpse manual/reset semantics stay authoritative. |

The new cross-policy milestone test covers the historical 6.301-yard reclaim
eligibility/two probes and unchanged corpse bounds, automatic living entry,
reachable egress, actual water-policy preemption/exit, eventual release, retained
repeat history and fresh-Ghost latch, all 16 danger/defense/water/unknown owner
combinations, terminal/no-retry behavior and AFK recovery/Ghost guards. Existing
living, repeat, corpse, navigation, combat, water and AFK suites remain required.
Production-owner sentinels supplement these executable tests; they are not a
live client or traversal simulation.

The bound capture manifest's five entries were reverified, as was the earlier
safety log digest. The bound final session still has no death/reclaim/latch and
cannot qualify this code. Its exact mapping was user-attested for the historical
`f022de6` DLL, not independently captured `/proc` evidence or this build's binding.
No earlier unbound run is used to manufacture PASS.

Remaining items are classified in the current model: corpse mechanics
IMPLEMENTED/historical P0.5.8 PASS; repeat and living paths IMPLEMENTED/RUNTIME
PENDING; safe Ghost staging and first unsafe resurrection SOURCE GAP; early
alive-probe defense, provenance-qualified historical recovery anchors and
combat-event history intentionally deferred. Unselected exact hostility, Ghost
visibility coverage and a positive candidate safety predicate are still missing.
No absent-mob/healthy-coordinate/geometry heuristic fills those gaps. Water,
vendor, reconnect and completed-unload qualification remain unchanged.

Validation PASS: **112 C++ tests, 42 audit Python + 13 QuestDB Python tests,
SQL fixtures, 11 Lua suites**. Full validator:
`PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4`;
`/tmp/wow-validation-vcq1mg47/results.json`, console
`/tmp/death-milestone-validation.log`. Final milestone test rerun PASS after the
gap-propagation sentinel; separate `cmake --build build` PASS (up to date).
No runtime claims follow from these results. Only task source/tests/docs and
the new fixture registration belong in the checkpoint; captures stay local.

The exact next major project milestone is **Bound supervised Grind endurance
qualification and release gate**, specified in the current model. Further small
DeathRecovery source tasks require concrete new evidence; this milestone closes
the defensible source/test scope without guessing through the remaining gaps.

## Bounded living recovery and NavMesh egress (2026-10-10)

**IMPLEMENTED / RUNTIME PENDING.** This section supersedes the earlier proposal's
“living egress unimplemented” description; all historical runtime verdicts retain
their original scope. Single coordinator implementation from `af8e3b8`, branch
`codex/vendor-afk-long-navigation`; no new WoW session. The repeat-death circuit
breaker remains RUNTIME PENDING: the bound session below did not exercise it.

### Verified gap and evidence limits

The previously audited safety capture's raw hash was rechecked:
`runtime-captures/death-safety-2026-10-10/wow-internal.log`, SHA256
`53a8cddeb95f64e15f440d525f65afad29959da50fd017c798b5cb5dd54bf223`.
Its lines 19110–19143 show confirmed resurrection followed immediately by normal
ownership/defense; 21070–21137 show death at the same logged position. The earlier
audit documents its missing exact source/DLL binding. This is motivation and
observed chronology, not runtime qualification of this new code.

Current-source control flow confirmed the gap: `CompleteRecovery` accepted two
fresh alive probes, `FinalizeAliveEpisode` set Done, and WorldMonitor immediately
resumed Combat/Grind. Existing Grind post-death escape waits for health and is
not a dedicated ownership boundary. A separate living owner now bridges this
transition; it does not change reclaim eligibility or the two alive probes.
Danger before/during those probes and the first unsafe reclaim remain unresolved.

### Ownership and source-qualified evidence

`LivingDeathRecoveryPolicy` is deterministic; `LivingDeathRecoveryController`
implements its bounded actions. WorldMonitor enters it only for automatic Done,
rearms the corpse controller immediately, and withholds ordinary resume. Its
branch runs after existing water arbitration and before normal quest/grind/
combat/watchdog dispatch. The completion tick also returns early. Manual-alive
completion does not acquire this owner. Redeath stops its route and returns to
the existing corpse controller without clearing repeat history. Grind records
the next death independently; defense-earned corpses remain deferred until release.

`CombatClientEvidence5875::Living` is read-only and runs on the game thread.
It uses existing signature/manager/player identity verification from `Selection`,
checks player GUID/address/descriptors before and after enumeration, reads
health/max health (`+0x58/+0x70`), native unit combat flag (`+0xb8`, mask
`0x80000`) and player Ghost flag (`+0x2f8`, mask `0x10`). These fields were already
used by `Execution`/`FreshHealth`; no new Lua API is presumed. The bounded
4096-object walk uses the existing type/GUID/next/descriptor layout and reads
live unit target GUID (`+0x40`). It requires complete termination and the same
local player before absence of engagement can contribute to a quiet observation.

Positive live type-3/type-4 target-to-player evidence blocks egress even if a
later read fails. Only a matching type-3 WorldState unit can become an exact
CombatController defense target; this does not invent player/PvP targeting.
Native combat without a target still blocks normal movement/release. A decrease
in sampled HP records pressure and restarts the quiet interval, without asserting
combat damage or its cause. No event-based damage history, unseen-attacker
coverage, hostile aggro radius or Ghost threat visibility is established. A
unit targeting the player is conservatively treated as engagement evidence;
this adapter does not prove that every such unit is hostile. Missing/partial
reads are unknown, never a safety verdict. Hidden or non-targeting threats remain
possible even after successful recovery.

CombatController's scoped defense entry permits exact-attacker adoption and
existing active defense states only. Ordinary acquisition is gated inside the
FSM too; finishing defense queues loot without starting a loot transaction.
Fresh complete disengagement releases defense. A world gap discards the old
intent before any later defensive adoption. Combat terminal failure blocks
normal resume. An unknown attacker cannot authorize a guessed target or a new
pull; existing defense failure policies are not weakened.

Living-water arbitration retains its existing priority and gates ahead of this
owner. Its handoff is additionally checked against fresh targetless engagement
while living recovery owns control. It may cancel the living route; ordinary
water emergency/terminal behavior remains authoritative. Swimming without an
eligible water owner holds living navigation instead of applying Ghost traversal
permissions. After water release the original living deadline still applies.
No water emergency qualification is awarded. Existing AFK recovery safety is
passed on every living-owner tick, including its terminal state; AFK native
combat/water/dead/Ghost gates are unchanged.

### Finite egress and completion contract

| Bound or condition | Implemented behavior |
| --- | --- |
| Active lifetime | 90,000 ms steady deadline from automatic alive completion, including defense/water waits; never renewed by progress or replanning. |
| Evidence before movement | Same identity, alive, complete fresh engagement reads, known non-swimming movement, no active defense, two seconds since observed engagement/HP loss/unknown evidence. |
| Candidate set | At most four attempt slots; direction uses current orientation plus successive quarter turns, requested 18 yards from resurrection XYZ. No hard-coded route or historical “safe” anchor. |
| Route acceptance | Planning-only GenericNavMeshPathFollower/Detour must reach the full destination; projected displacement >=12 yards. Then a fresh command guard precedes a separate execution request. |
| Route constraints | Living `AvoidUntilQualified`, no full-map fallback, existing path validation and episode-local directed-transition avoidance. Each execution stops on reaching four lifetime replans (not the follower's progress-reset stall counter); the absolute deadline also bounds loading/planning. |
| Interruptions/failures | A canceled planning/execution attempt consumes its slot. Failed/partial/near projections advance only within four slots. Exhaustion blocks. No standalone retry timer renews the episode. |
| Arrival/recovery | Actual displacement >=12 yards; existing RecoveryController may recover health after arrival. Further pressure yields to defense. Falling back inside 12 yards after arrival blocks release. |
| Release | Alive, >=95% HP, no positive engagement/defense/water owner, quiet interval, three fresh observations >=250 ms apart, then fresh command-world/health/displacement checks and standing dispatch. Only committed completion resumes normal modes on the next snapshot. |
| World/identity loss | Drop route/evidence and require manual recovery. Do not issue commands to a foreign/unknown player or resume from stale proofs. |
| Terminal policy | No autonomous route/search/release; passive manual-recovery interlock keeps normal modes blocked while defense and eligible existing water ownership remain possible. |

The 18/12-yard distances are engineering displacement bounds, **not aggro or
safety radii**. Reachability and absence of observed engagement do not prove a
safe destination. No healthy historical anchor is used because its threat/water
provenance is insufficient here. A candidate may remain dangerous and cause
another death; repeat-death semantics remain unchanged (120 seconds/eight yards),
so a death outside that original neighborhood is not newly covered.

A terminal block is deliberately not an infinite active recovery loop: automatic
search, movement and recovery stop, and no retry budget resets. Its normal-mode
interlock persists until explicit session stop/restart or a new death hands off
to corpse recovery. Auto-expiring that interlock would blindly restart beside a
failed egress. Deadline evaluation continues before water ownership; a world gap
blocks immediately. Map identity uses the existing configured map (currently 1),
not a newly qualified live map reader; seamless cross-map support is not claimed.

### Deterministic coverage and runtime gate

`living_death_recovery_policy_test.cpp` covers automatic versus manual entry,
positive aggressor/native-combat/defense priority, water preemption including
terminal defense availability, full reachable egress/arrival, quiet/HP recovery,
spaced proofs and committed release, no route, partial/near projections, finite
candidate/replan failure, unchanged deadline across interruptions, unknown life,
HP pressure, incomplete observations, world/identity/map loss, time rollback,
post-arrival displacement loss and repeat-policy preservation. Production wiring
sentinels check that normal dispatch is bypassed, exact defense does not use a
selected-target fallback, defense loot is deferred, and living NavMesh options
retain water restrictions/full-reachability/replan guards. These are policy and
source integration tests, not a simulated live client or runtime qualification.

Validation PASS: `PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4`
completed **110 C++ tests**, registered Python/SQL/Lua suites, full DLL build and
diff check. Report: `/tmp/wow-validation-jwp2i55y/results.json`; console:
`/tmp/death-living-recovery-final-validation.log`. The living policy/integration
test was also compiled and run again after the cumulative-replan guard change.
These are static/build results, not runtime evidence.

A future supervised run must bind source/branch/DLL and runtime mapping before
relying on execution. Require a natural reclaim with unchanged two fresh alive
probes, `repeatDeath=armed`, `DEATH RECOVERY EXIT resumeMode=LivingRecovery` and
`DEATH LIVING entered=automatic_alive_confirmed`. Trace all commands/owners:
no normal pull/roam/vendor/loot while held; exact positive aggressor gets defense;
qualified water preemption follows its existing policy; otherwise preflight →
routing → actual displacement → recovery → three calm fresh observations →
`complete` → next-snapshot normal resume. Sparse `DEATH LIVING` transitions and
five-second status include reason, attempts, engagement/read completeness, HP,
attacker and normal-mode blocking; NavMesh logs supply destination/motion detail.

Also require a naturally encountered failure/preemption/world gap to stop within
the budgets and retain `blocked_manual_recovery`, without route restarts or
normal acquisition. If nearby redeath occurs, independently verify the original
release/fresh-Ghost/repeat-terminal/zero-retrieve/AFK requirements below. Do not
provoke death or treat a successful build, absent attacker, unload request or
unbound old run as PASS. First unsafe resurrection, safe Ghost staging, vendor,
water emergency egress, reconnect and completed DLL unload remain unqualified
as previously recorded. Historical P0.5.8 PASS is not broadened; P0.7 untouched.

## Bound runtime qualification (2026-10-10): repeat path NOT EXERCISED

**INSUFFICIENT EVIDENCE; the repeat-death circuit breaker remains IMPLEMENTED /
RUNTIME PENDING.** All supplied hashes verify, and the deployment binding is
accepted with the explicit user attestation described below. However, the
complete supplied raw session contains no player death, automatic reclaim,
armed record, repeat latch or terminal DeathRecovery ownership. Binding does
not supply missing execution evidence. No runtime PASS is awarded or revoked.
The earlier unbound `death-repeat-pass-2026-10-10/` sequence is not substituted
for this run, and this attestation is not applied retroactively to it.

Single coordinator review of clean source checkpoint
`f022de67b3e30ef0202afd2c8ab19819de329b08` on
`codex/vendor-afk-long-navigation`; AGENTS.md, AI_HANDOFF.md, relevant history
and current source were inspected. No production source/test changes or new
WoW session were needed. P0.7 was not accessed.

### Capture integrity and deployment binding

Directory: `runtime-captures/death-repeat-bound-2026-10-10/` (local only; not
committed). Before relying on its contents, `sha256sum -c` verified all five
entries in `capture-sha256.txt`. The manifest's own digest was computed for
this audit; it has no separately supplied expected digest. All **21** numbered
extract records match the raw lines. The entire raw log has **6,463 lines /
511,282 bytes**; the append-only lifecycle log has **199 lines / 48,987 bytes**.

| File | SHA256 of original bytes |
| --- | --- |
| deployment.txt | `136686b6b18fbb74f16cc991d2be04e45876397ec71492329ec6a140dbc6311f` |
| binding-confirmation.txt | `088524a0058dd3f1876c256e4999b6df1cca82ddfff7a16bdb1f31b421f3d975` |
| capture-sha256.txt | `37b28b829baa8c437b4724352a50dd490700b70dec3927830b556c2bb2671f9b` |
| wow-internal.log | `af3475df0c1c0a5e6558c421c3d7bed9c5aec6ebe0e915b2f8a2f8beab77ec9f` |
| wow-internal.lifecycle.log | `c86b09ec124803ec0439d137d5ded78f9762c930ad7585a0043c083155b70d78` |
| death-repeat-extract.txt | `504983e64be49434c199d157234c3a1cc932295e7958b07f7534702c6cfb9ac3` |

Deployment records identify the above branch/HEAD and
`/home/ludvig/Programming/Projects/wow-internal-5875/build/wow_internal.dll`,
SHA256 `bc2a148d9f251121831f0400f10961af4fafbc894300b47e0db1cb2121268745`.
Pre/post records retain that HEAD/hash and clean Git state. Local verification
agrees: device 66309, inode 4980769, size 23,180,065 bytes, mtime
2026-10-10 13:21:28 +02:00. The user explicitly attests that Linux WoW PID
124315 mapped this exact path on device `103:05`, inode 4980769, and that no
build, branch switch, pull or DLL replacement occurred during the recorded
interval. This is **user-attested runtime mapping**, not an automatically
preserved `/proc/124315/maps` artifact: `loaded_dll_mappings:` is empty in
deployment.txt. The supplied confirmation fills that attestation gap; this
audit does not claim to have independently sampled the running process.

The raw log identifies Wine/Win32 PID **492**, DLL base `0x74590000`, player
GUID `0x1B867` (112743), client 1.12.1.5875 and session
`492.134361144754615960.83558443.700`. Linux PID 124315 is the attested host
process identity, not a literal PID field in the raw log. Deployment notes
contain multiple start/stop timestamps (14:39/14:40 starts and 16:04/16:09
stops, +02:00). They must not be treated as one uninterrupted captured bot
session. Lifecycle lines **189–199** delimit the supplied final session;
line 192 records a successful raw-log clear before its attach. Earlier journal
sessions, including PID 300, do not supply their missing gameplay chronology.
`Logger.h` derives the long session-ID component from process creation time;
it is not the bot-start timestamp. Raw/lifecycle BOT START/STOP agree at
monotonic **83558961–83837304**, a **278,343 ms (4m38.343s)** bot session.
The first raw diagnostic is 14:04:18Z (16:04:18 local), consistent with the
final short run rather than the entire deployment-note interval.

### Complete session chronology

All references are one-based raw lines unless marked lifecycle.

| Lines | Observation |
| --- | --- |
| 1–149, 207–210 | Fresh attach and Grind start; level 22, HP 664/664, position (2210.2495,-2534.4614,82.7275). DeathRecovery Idle with all attempt/recovery counters zero. |
| 171 | Danger memory loads six historical deaths. This is persisted history, not six deaths during this session. |
| 379–1286 | Grind gray-migration intent 1 runs ticks 48–251 and is owner-released. No DeathRecovery route. |
| 1291–1886 | Grind approach to entry 3819, intent 2, ticks 253–592, fails `path_validation_failed` with zero movement commands. The interruption is `target_unreachable`, with death ownership false. |
| 1962–2525 | Grind roam intents 3 and 4 arrive, ticks 609–619 and 639–723. |
| 2629–2808 | Grind roam intent 5 starts at 751 and is owner-released at 758. AFK becomes due at inputAge 240193, then its action is blocked by `frame_limit` at 2721. Combat selection begins; native combat blocks AFK at 2804; entry 12856 GUID `0xF130003238007E6A` is tracked as an aggressor at 2808. |
| 2883–6297 | Charge/opening/chase/fight transitions. Fifteen player-health changes decrease 664 to 185, never zero. AFK overdue at 5771 uses `native_combat_flag`, not a Ghost gate. |
| 6333–6426 | Verified target kill, loot window/slot observations, `LOOT: PASS`, then PostKillDelay. Final WorldState at 6374 is alive, HP 185/664; DeathRecovery at 6377 is still Idle. |
| 6432–6463; lifecycle 197–199 | AFK pending evidence reset at stop, GUI stop request, game-thread autoattack stop and hold-position dispatch, BOT STOP, nav-cache teardown and unload request. Lifecycle GUI stop precedes BOT STOP by 189 ms. No observed terminal death state is involved. |

There are **44** WorldState samples (HP 185–664), **44** identical DeathRecovery
Idle summaries with zero recoveries/releases/retrieves/routes, and **44** mode
summaries with `normalModeUpdate=yes deathOwner=no`. The three session counters
at 1246/1706/2509 all have `deaths=0 successfulDeathRecoveries=0`; final status
corroborates continued Idle. Five navigation intents all belong to Grind.
All 29 AFK life diagnostics say alive. Five disconnect diagnostics say healthy;
no world-unavailable transition is logged. None of this qualifies reconnect,
vendor, living-water egress or AFK delivery. `AFK ACTION BLOCKED` is not a
verified input action.

### Acceptance result and exact missing evidence

| Required repeat-death evidence | Bound-session result |
| --- | --- |
| Automatic RetrieveCorpse and two fresh alive probes | Absent; zero `DEATH RECLAIM` or `DEATH RECOVERY` event lines. |
| `repeatDeath=armed` at actual confirmed-alive position | Absent; zero `DEATH SAFETY` lines. |
| Nearby redeath and `repeatDeath=latched` | Absent; no player death or repeat-policy observation. |
| Normal release and fresh Ghost confirmation | Absent; no release attempts or dead/Ghost life diagnostics. |
| `recent_reclaim_redeath`, `strategiesExhausted=yes`, `normalModeBlocked=yes` | Absent; no terminal entry. |
| Terminal `retrieveAttempts=0`, no later reclaim/corpse-route restart | Only Idle counters are zero. With no latch or terminal episode, absence of commands does not test the veto. |
| Retained terminal Ghost ownership and correct Ghost AFK blocking | Not exercised; normal mode runs throughout and no `dead_ghost_afk_recovery_not_qualified` occurs. |
| Understood session stop | Observed GUI-driven stop and teardown requests; completed DLL unload is not independently proved. |

`RUNTIME DETACHED` is logged before the bootstrap's unload request, not after
verified unmapping. Raw attach/unload monotonic values differ from their
journal copies by 1 ms because `Logger::Event` writes them separately. No
post-unload module map or independently verified completed detach is supplied.
The stop is understood at the logged request/cleanup boundary only.

Current source still arms only after Ghost + issued reclaim + two fresh alive
probes (`DeathRecoveryController.h:972,1450,1750`), preserves history through
`RearmAfterConfirmedAlive` (`:1948`), and evaluates the 120-second/eight-yard
same-player/map guard using body/current-server evidence (`:1265,1412`). Fresh
Ghost gates terminal `recent_reclaim_redeath` before route/reclaim dispatch
(`:1499`); terminal failure holds normal ownership. `AfkDeadGhostPolicy.h:31`
retains its independent command-in-flight and unqualified recovery guards.
These source checks explain the expected path; they do not turn Idle samples
into runtime execution of it.

Still needed is **one bound natural session** containing the whole sequence:
automatic reclaim → two fresh alive probes → armed actual position → same
player/map redeath within 120,000 ms and <=8 yards 3D → body/current-server
latch → ordinary release → fresh Ghost → terminal `recent_reclaim_redeath`,
zero retrieve attempts and normal-mode block → continued stopped Ghost
ownership with no reclaim or route restart, correct AFK gating, then understood
stop. Preserve raw logs across any later GUI starts. If manual resurrection
occurs, verify two fresh same-player alive probes before release. Do not provoke
death. The earlier unbound run's 8567-ms/zero-separation result is not evidence
that this bound session traversed that path.

First unsafe resurrection remains unresolved. Safe resurrection staging and
DeathRecovery-owned living egress remain NOT IMPLEMENTED. Historical P0.5.8
PASS retains its original scope; vendor stays INSUFFICIENT EVIDENCE, water
emergency egress RUNTIME PENDING, and reconnect SOURCE GAP / NOT IMPLEMENTED.

### Next substantial engineering block

Recommend **bounded living recovery and NavMesh egress after resurrection**
as one integrated task, including its evidence adapter, ownership transitions,
defense/water arbitration, tests and telemetry. This is an engineering proposal,
not an implementation or an instruction to alter behavior in this checkpoint.
It addresses the current immediate Done→normal-mode handoff without pretending
that Ghost sensing can certify the first reclaim safe. The pending repeat-path
runtime qualification can be collected during natural supervised use; another
documentation callback is not a substitute for that run.

Current source supports this priority: `WorldMonitor.h:1453` resumes normal
owners immediately at Done. Grind's `RecordDeath` already arms escape;
`TryStartDangerEscape` (`GrindModeController.h:1004`) requires no direct
aggressor and >=90% HP. Existing protection is present, yet cannot provide
early living egress. Deliver the following together:

- A fresh, same-character, targetless living observation of combat/aggressors,
  identity, life state and read completeness. Reuse verified 5875 semantics
  from `CombatClientEvidence5875`, whose current `Execution` requires a target;
  never select/attack merely to obtain evidence. Unknown/stale/partial reads
  cannot authorize safety release. Qualify the observation's source semantics
  and practical coverage before using absence of threats as a release claim.
- A distinct living recovery owner after the existing two-probe automatic
  confirmation, suppressing voluntary Grind/Quest acquisition but providing
  defense-only CombatController handoff. Separate living life state from the
  historical Ghost-confirmed latch. Death again must immediately return to
  ordinary death handling and preserve the repeat breaker.
- Bounded generated NavMesh egress with living terrain/water constraints,
  current-origin path validation, preserved hazard/directed-link knowledge and
  one monotonic deadline across candidate changes and preemptions. Existing
  `deathWaterOwner` must not exempt the living owner from water handling;
  AFK/native guards remain authoritative. Non-swimming alone is not dry-ground
  proof. Unknown terrain or unavailable routes must produce an explicit block.
- Explicit fresh recovery/release criteria and a stopped living-blocked outcome
  on timeout, with defense available. Do not reuse Failed's current two-alive-
  probe release as a safety predicate. World gaps invalidate partial proof and
  navigation before reacquisition. Log evidence, transitions, preemptions,
  deadline, blocked outcomes and release reasons in the production path.

Acceptance for that single delivery must cover successful bounded egress and
one-time release; no voluntary pulls while retained; defense during handoff;
living-water preemption; stale/foreign/incomplete evidence; world loss; blocked
or unsafe routes; timeout without budget renewal; and redeath through the
existing circuit breaker. Use meaningful policy and production integration
tests plus full build, then a bound runtime demonstrating the new owner. Do not
delay self-defense during alive confirmation by adding an unprotected wait.

Safe **pre-reclaim staging** remains a separate sensing blocker: current
Ghost WorldState is type-3-only and does not establish living-threat visibility
coverage, per-field knownness or a positive safe-point predicate. An empty scan,
attackability proxy, density score or projected ground point cannot close it.
The proposed living task must not broaden the eight-yard reclaim gate, claim
the first unsafe resurrection fixed, or add guessed offsets/APIs. The earlier
full safety design and seven acceptance cases below remain future work.

### Static/build validation for this audit

`PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4` passed all
**109 C++ tests**, registered Python/SQL/Lua suites, full `cmake --build build`
(up to date) and diff check. Report: `/tmp/wow-validation-4ityd74_/results.json`;
console: `/tmp/death-repeat-bound-validation.log`. Capture hashes, documented
digests, device/inode, whole-session counts and duration were independently
cross-checked again. No new tests were added for this documentation-only audit;
static/build PASS does not change its runtime verdict. Only this audit, the
runtime audit and AI_HANDOFF.md belong to this checkpoint; capture files,
build outputs and state remain uncommitted.

## Repeat-death circuit breaker (2026-10-10 continuation)

**Bounded mitigation IMPLEMENTED; runtime qualification PENDING. Safe-point
staging and DeathRecovery-owned living egress remain NOT IMPLEMENTED.** This
continues checkpoint `602bf2b` on `codex/vendor-afk-long-navigation`; the worktree
was clean (no uncommitted changes to discard). Three parallel read-only reviews
rechecked recovery architecture, Ghost threat evidence, and NavMesh/egress.
The P0.7 worktree was not accessed. Historical **P0.5.8 RUNTIME PASS is preserved**
for its original route/reclaim/alive-confirmation/resume scope.

### Evidence and bounded scope

The preserved capture and its hashes below were rechecked. Lines 19110–19143
confirm automatic resurrection and defense; lines 21070–21137 show the next
death at the same position. The second sequence repeats at lines 26859–29274.
These observed outcomes justify a repeat-death circuit breaker without assuming
that Ghost object visibility is complete or that a different candidate is safe.
They do not establish an exact wall-clock interval: the first confirmation is
between ticks 1472/1485, with the next death before tick 1541; the second is after
2005 with the next death before 2091. Tick spacing is not elapsed-time proof.

The source path still permits the first close reclaim without assessing nearby
hostiles. In addition, automatic completion previously called `Reset()` through
`RearmAfterConfirmedAlive()` without retaining the confirmed resurrection
location. A subsequent death was therefore a fresh automatic recovery episode,
even at that location. The new policy retains only the previous verified
automatic-alive location/identity/time across that rearm and detects a repeat
outcome. This is a deliberate bounded fallback, not safe resurrection staging.

### Implementation and ownership

`DeathRecoveryRepeatDeathPolicy.h` defines a session-local, one-record guard:

- Record actual player XYZ, same player GUID/map and steady-clock time only
  after the existing Ghost + issued RetrieveCorpse + two fresh alive probes.
- At the next confirmed death, freeze eligibility if it occurred within
  **120,000 ms inclusive** of that confirmation. Compare the observed dead body
  or subsequently read current server corpse location in **3D, <=8 yards**.
  The window/radius are conservative engineering choices, not an aggro radius
  or measured runtime safety limits. Neither changes reclaim eligibility.
- Ghost bootstrap never compares the graveyard, old healthy position or
  persisted routing anchor. It waits for current server corpse evidence.
  Identity mismatch, rollback, expired time or invalid coordinates cannot
  establish a repeat. Missing evidence leaves the historical recovery path.
- A positive match latches for that death. Normal bounded spirit release runs;
  after a fresh Ghost confirmation, enter the existing `Failed` owner with
  `recent_reclaim_redeath`. A latched match plus cached Ghost evidence waits for
  a fresh probe without starting a route. There is no automatic safe-point
  search, retry cooldown, reclaim fallback, or timer-based release of the block.
- Explicit reset/new session, world gap and confirmed manual-alive completion
  clear the record. Clearing history does not release `Failed`; its existing
  two fresh same-character alive probes are still required. Ordinary automatic
  rearm preserves the record. Healthy observations do not refresh its age.

No new navigation or living owner is introduced. Combat, water and AFK dispatch
and priorities are unchanged, including the already qualified Failed-Ghost AFK
path. A block is reached before another automatic reclaim, while dead/Ghost;
it does not retain a living character in a new defenseless state. The existing
release attempts, anchor wait, route/reclaim bounds and monotonic liveness remain
authoritative if fresh evidence cannot be obtained. The safety verdict does
not earn a strategic route continuation or reset any budget.

Sparse `DEATH SAFETY repeatDeath=armed/latched` telemetry records GUID, map,
confirmed position, corpse position, age at death, separation, observation
source and `threatCoverage=unknown`; the existing terminal line records the
final block. No API, memory writer, hostile classification or aggro assumption
is added. NavMesh remains authoritative for all existing corpse routes.
Map identity here uses the existing caller-supplied controller map (currently
map 1 in WorldMonitor); this patch adds no live map reader or cross-map support.
World-gap invalidation remains necessary; map comparison is not new map-sensing
qualification.

### Verification and remaining acceptance

`death_recovery_repeat_death_policy_test.cpp` covers first death, captured
repeat-position geometry with synthetic time, inclusive time/distance boundaries,
vertical/distant death, identity/map mismatch, unknown identity, nonfinite
coordinates, rollback, expired history, delayed server anchor, sticky verdict,
rearm/reset lifetime and Ghost-only blocking. Integration sentinels check the
production reclaim gate and the source/reset lifetime of evidence. Existing
death ownership, liveness, terminal, AFK, water and combat suites remain required.
These are deterministic/source checks, not execution of the Windows controller
against a live client.

`PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4` passed all
**109 C++ tests**, registered Python/SQL/Lua suites, full `cmake --build build`
and diff check. Report: `/tmp/wow-validation-dw90ybrq/results.json`; console:
`/tmp/death-repeat-safety-validation.log`. After the final explicit guard against
strategic budget renewal for a latched verdict, the new deterministic test was
recompiled with `-Wall -Wextra -Werror` and passed; the full build was rerun.
These checks do not qualify runtime behavior or safe staging/living egress.

Next supervised normal runtime must capture the deployed DLL hash/revision and:

1. A natural first death/reclaim with unchanged fresh eligibility and two alive
   probes; `repeatDeath=armed` must name the actual confirmed-alive position.
2. If a natural nearby redeath occurs within the window, a matching latch using
   body/current-server evidence, normal release, fresh Ghost confirmation and
   terminal `recent_reclaim_redeath`, with no subsequent automatic reclaim or
   restarted route. Record stopped navigation and continued Ghost AFK behavior.
3. If manual resurrection occurs, two fresh same-player alive probes before
   normal ownership resumes; no old automatic-reclaim record carried forward.

A reclaim after a positive latch, release on timer expiry, a route restarted
while awaiting fresh Ghost confirmation, or a latch from a foreign/stale anchor
would disprove the implementation's contract. Do not provoke death to test it.
No new WoW session was launched for this patch.

This guard can stop unattended resurrection after a nearby death for reasons
other than hostile mobs. It cannot prevent the first unsafe resurrection,
deaths outside the window/radius, a death before automatic alive confirmation,
or repeats across world gaps/session reset. It is not a positive safety verdict.
Safe staging and living egress still need the evidence and ownership work in
the prior investigation below; their seven acceptance cases remain unimplemented.

## Resurrection location safety investigation (2026-10-10, prior checkpoint 602bf2b)

**New safety gap: RUNTIME OBSERVED / SOURCE VERIFIED. Safe staging and
DeathRecovery-owned living egress: NOT IMPLEMENTED — evidence gap.**
Inspected clean checkpoint `6abd7ac3ff89e1c96f25e2869e8259499f8134e7` on
`codex/vendor-afk-long-navigation`, AGENTS.md, AI_HANDOFF.md and relevant death,
navigation and world-reader history. Three parallel read-only reviews covered
resurrection architecture, hostile/world evidence, and NavMesh/test design.

Historical **P0.5.8 RUNTIME PASS remains preserved**, with its original scope:
natural death, directed-link route avoidance, reclaim, two fresh alive probes
and Combat/Grind resume. The preserved user-reported qualification is recorded
in [CODEX_PROJECT_STATE.md](CODEX_PROJECT_STATE.md#p053-release-requalification-prep-2026-10-08).
It did not qualify avoiding post-reclaim re-aggro. This is a new safety/design
requirement, not a revocation of successful resurrection or route qualification.
The older scope/status and incidents below remain historical.

### Runtime observation

The user's supervised observation prompted the investigation; source causation
was not assumed. A newer `build/wow-internal.log` contains corroborating evidence.
Its stable bytes and lifecycle log were copied into the Git-excluded directory
`runtime-captures/death-safety-2026-10-10/` before analysis. The earlier
`vendor-runtime-2026-10-10-next` capture ends during the first corpse run and
does not contain its later resurrection/redeath sequence.

Raw log: 36,744 lines / 2,941,252 bytes, SHA256
`53a8cddeb95f64e15f440d525f65afad29959da50fd017c798b5cb5dd54bf223`.
Lifecycle: 136 lines / 34,079 bytes, SHA256
`e5bfea4c987eb65845ef7fc0b5911b027af1d5f31f48a4f5d532638fc25a59cd`.
Relevant session: `300.134361024694534150.71490348.508`, player GUID 112743,
map 1. Lifecycle BOT START/STOP: monotonic 71490867–72351698 (~14m21s),
ending with GUI stop and unload request. Earlier lifecycle sessions are not
part of this episode. No captured DLL hash establishes exact source/binary
identity. No new WoW session was launched by this investigation.

All following references are one-based raw-log lines:

| Lines | Observed sequence |
| --- | --- |
| 7785–7789 | First death anchor (2004.178,-2580.670,94.960); DeathRecovery enters episode 1. |
| 19008–19087 | Reclaim at 6.397 yards; HP 1→332/664. Native combat flag is already set at 19054 while WaitingForAlive owns updates. First alive probe succeeds; HP falls to 273 before the second probe. |
| 19110–19143 | Second fresh alive probe, verified reclaim, Done, Combat/Grind resume. Same position (2010.3558,-2581.5701,96.3555). Direct aggressor GUID `0xF130000F58007F25`, entry 3928, is tracked at 1.9573 yards immediately after the exit records. |
| 21070–21137 | HP reaches zero. Episode 2 records death at (2010.356,-2581.570,96.356), matching the resurrection position at logged precision. |
| 26859–26984 | Second reclaim at 5.310 yards; two alive probes at 357/664 HP; Done and Grind resume. Low-health gate starts passive recovery without usable food. |
| 27002–27055 | At (2015.3982,-2580.8809,97.8714), recovery is preempted by the same entry/GUID, distance 7.7201, explicitly `evidence=live-targeting-player`; defense resumes. |
| 29207–29274 | HP reaches zero again; episode 3 starts at (2015.398,-2580.881,97.871), matching the second resurrection position. |
| 36589–36712 | Third run terminates on unsafe terrain; later two fresh alive probes take the existing `manual_alive_confirmed` exit. This is not a third automatic reclaim success or proof of who caused the alive transition. |

There are two automatic reclaim confirmations, three death starts and no
`post-death danger escape` movement-intent start. The log proves rapid combat
after reclaim and two subsequent deaths in place. It does not prove a mob aggro
radius, exact time-to-redeath from un-timestamped lines, a safe alternative
location, or that a different staging point would have prevented either death.

### Source verification and causal limit

`DeathRecoveryController.h:1638` gates reclaim on the current server corpse
anchor, fresh Ghost/delay evidence, retry bounds and the **8-yard precision
distance**. It performs no nearby-threat or living-ground suitability assessment.
The generated 14-yard ring variants in `RouteDestinationForVariant` are routing
alternatives; they are not safety-ranked resurrection positions. Routes reaching the broad
arrival band are brought closer through precision routing.

`WaitingForAlive` at line 1696 calls `CompleteRecovery` after the established
two fresh alive probes. `CompleteRecovery`/`FinalizeAliveEpisode` at lines
930–959 go directly to Done. `WorldMonitor.h:1453` re-arms the controller and
resumes Combat plus Grind/Quest without a danger/recovery release predicate.
Combat remains suspended while those alive confirmation probes accumulate;
the first trace already shows combat/damage in that interval. Self-defense
must not be delayed further by simply inserting another waiting state.

There **is** existing post-death protection: `RecordDeath` arms
`postDeathEscapePending_`; Grind suppresses ordinary pulls, and
`TryStartDangerEscape` (`GrindModeController.h:1004`) waits for no direct
aggressor and at least 90% HP before choosing a NavMesh sector. The ordinary
recovery and defensive paths therefore run first. This is not a missing
post-death flag or evidence that Grind deliberately pulled a new target.
It cannot protect the pre-reclaim location or provide the requested early
DeathRecovery-owned egress. The observed control flow is consistent with
healing/defending in place before escape becomes eligible. Avoid claiming this
handoff alone caused every death, or lowering the health gate as a purported fix.

### Exact evidence gaps preventing the requested safe-point policy

1. **Ghost threat coverage is unqualified.** `WorldState.h:27` exposes read
   failures and interrupted enumeration, not server/client visibility coverage.
   The vector includes only type-3 units (`:214`), not other players; reaching
   the 4096-object cap does not independently mark truncation. `valid=true`,
   zero failures or an empty vector cannot prove the surroundings threat-free.
   Ghost samples at raw lines 18014/18258 contain zero units, 18564 two, 18834
   four; the alive sample at 19080 contains seven. Different time/position means
   these counts do not prove which units were omitted. There is no complete
   identity/position/visibility record at reclaim or candidate locations.
2. **Hostility and aggression are not equivalent to a creature-shaped object.**
   `PullSafetyPolicy.h:13` explicitly describes a potentially attackable proxy,
   not exact hostility or aggro radius. `GrindTargetPolicy::LooksLikeCombatCreature`
   excludes pets/NPC-flagged units; exclusion cannot certify harmlessness.
   `UnitSnapshot.h:141,167` does not retain read-success evidence for target GUID
   or faction template. Its zero fields cannot authorize a no-threat conclusion.
   Existing selected-target UnitCanAttack checks do not classify every unselected
   Ghost-visible GUID. The 12-yard density heuristic must not become a supposedly
   qualified resurrection aggro radius. Visible threats can inform relative
   ranking or reject a point, but do not supply a positive safety predicate.
3. **Fresh targetless living safety evidence is not an existing adapter.**
   `CombatClientEvidence5875` already supplies native combat-bit semantics and
   a fresh game-thread type-3/type-4 direct-aggressor sweep, explicitly avoiding
   absence inference from the cached vector. Its `Execution` entry requires a
   target. A recovery release needs checked, current, same-player evidence
   without selecting or attacking a target just to obtain that observation.
   A checked scan would close read/truncation gaps, not Ghost visibility coverage.
4. **Living terrain/water and owner identity must be kept distinct.**
   Death route starts choose GhostDeathRecovery once Ghost was confirmed
   (`DeathRecoveryController.h:717`); that historical latch cannot authorize
   living egress. `WorldMonitor.h:1068` also treats `deathShouldOwn` as death
   water ownership. Merely retaining an alive DeathRecovery state would bypass
   the living-water controller. `WaterEvidencePolicy5875.h:29` leaves ground
   contact/submergence/surface unknown; non-swimming alone is not dry-ground
   proof. Existing water guards and uncertainty must remain authoritative.

The first two gaps block the requested "safe corpse → immediate resurrection"
and "safe candidate → resurrect" decisions. A policy that calls unknown safe
would weaken evidence boundaries; a policy that blocks every unqualified Ghost
snapshot would disable historically qualified automatic recovery without
implementing usable safe staging. Therefore this checkpoint uses the user's
explicit **document the exact gap instead of guessing** fallback. No production
policy, ownership change or disconnected placeholder tests are introduced.

### Available navigation and bounded design once evidence is qualified

NavMesh access is available; lack of pathfinding is not the blocker.
`DetourNavigationProvider::ProjectGroundNear` projects to filtered ground,
but projection alone proves neither connected travel nor occupancy safety.
Grind's existing preflight uses projected endpoint identity, connected non-partial
`FindPath`, terrain validation and hazard rejection. GenericNavMesh supports
movement-free planning, `PlanningOnlyReachedDestination` and
`PlanningOnlyProjectedDestination`; a partial stage is not candidate arrival.
Revalidate execution from the current origin and retain learned directed-link
constraints. Use `AvoidUntilQualified` for staging intended to become a living
position and for all living egress; a Ghost-permitted corridor cannot certify it.

The 32-yard policy constant is not permission to broaden current reclaim.
Both this trace (6.397/5.310) and historical P0.5.8 (6.301) qualify close reclaim,
not arbitrary positions at the outer range. Candidate generation can remain
inside the current 8-yard 3D gate, with projection and final live-position
rechecks. Fresh server anchor, known delay zero, Ghost evidence, dispatch and
two alive probes remain mandatory; route arrival never substitutes for them.

Once sensing is qualified, the smallest design should remain in DeathRecovery:
evaluate current position plus a finite generated shortlist (at most nine
candidates total, using the existing variant count), spend existing route-attempt/liveness budgets on probes
and execution, rank only qualified candidates, and stop on no defensible point.
Do not reset the 18-route, nine-stationary-failure, 180-second no-progress or
300-second episode bounds when candidates change. Failed searches must not
fall back to an unsafe reclaim. Density/clearance may rank points but must not
pretend to be client aggro eligibility.

After confirmed resurrection, retain a distinct living recovery owner with
ordinary Grind/Quest acquisition suppressed. It needs a defense-only handoff,
fresh living-route validation, water priority, explicit recovery/safety release
criteria and a finite monotonic deadline. Timeout must stop navigation and
retain a blocked living owner with defense available; reusing today's Failed
state would release after two alive probes without a safety check. Death again
must start a new death episode, and world gaps must invalidate all partial
safety evidence. State entry, candidate rejection/selection, actual route
readiness, reclaim gate, defense/water preemption, deadline and release reasons
need sparse telemetry. A successful build cannot qualify this proposed design.

### Required qualification and deterministic cases

First obtain read-only, same-session Ghost-to-alive evidence for local object
coverage, per-field knownness, relevant living-unit identities/positions and
reaction/attackability semantics at candidate positions. Verify the source of
that visibility contract; sampling counts alone cannot establish it. Qualify a
targetless native combat/aggressor reader and the living water/ownership handoff.
Keep unknown, partial and stale observations explicit. Do not provoke a death
or widen reclaim distance to manufacture acceptance.

| Future deterministic case | Required behavior once the evidence adapter exists |
| --- | --- |
| Qualified safe corpse | Immediate reclaim permitted only by the unchanged fresh anchor/Ghost/delay/8-yard gate; no command-only success. |
| Dangerous corpse, safer candidate | Choose a qualified, fully reachable, living-compatible projected point inside range; recheck danger and range before reclaim. |
| No safe or known candidate | Bounded search/wait then blocked; no unsafe fallback and no unknown-as-safe inference. |
| Successful resurrection | Two fresh alive probes enter temporary living recovery ownership, not normal Grind. |
| Safe recovered living state | Explicit fresh safety and recovery criteria release once; pre-reclaim evidence cannot release it. |
| Combat/water/world priority | Defense can preempt without voluntary acquisition; living water cannot inherit Ghost exemption; world loss invalidates proof. |
| Timeout/retry/redeath | Absolute budgets survive candidate changes and preemptions; timeout cannot loop or release on alive-only proof; another death restarts death handling correctly. |

These are an acceptance plan, **not tests of an implemented feature**. Next
runtime must additionally show actual staging/reclaim location, retained living
ownership, qualified egress or bounded block, self-defense, and safe release.
Historical AFK, water, vendor and reconnect qualifications are unchanged.

### Validation of this investigation checkpoint

`PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py --jobs 4` passed all
108 C++ tests, registered Python/SQL/Lua suites, the full `cmake --build build`
(up to date), and diff check. Report: `/tmp/wow-validation-f45nnesi/results.json`;
console output: `/tmp/death-safety-2026-10-10-validation.log`. These checks cover
the existing implementation, not the unimplemented safety design. No new tests
claim candidate safety, living recovery ownership or runtime success. This
checkpoint changes only this audit, the runtime audit cross-reference and
AI_HANDOFF.md; preserved raw evidence remains local and Git-excluded.

## Scope and status

2026-10-07. SOURCE VERIFIED; deterministic/build verification recorded in
CODEX_PROJECT_STATE.md. New behavior is RUNTIME PENDING. No running WoW
process was available. This checkpoint does not claim successful resurrection,
a safe route through the rejected terrain, or long-run reliability.
AFK code, native-clear permissions, mixed-state policy, Ghost water-walk
allowance and scheduler are unchanged. No swimming/combat-watchdog work.

## Direct runtime evidence

Latest inspected build/wow-internal.log session
`1360.134358440699456960.106923713.436`:

- Lines25290-25311: observed body at (-646.515,-3524.180,91.7361), map1,
  persisted record, death ownership; one spirit-release dispatch.
- Lines25336-25381: Ghost confirmed at (-1081.400024,-3478.679932,63.606602),
  corpse distance438.163. Route intent34 accepted for incremental initialization,
  NOT movement success. RouteStarts=0; RetrieveAttempts=0.
- Lines25442/25551: route and expanded tiers reject unsafe terrain. First
  proven edge ends B84->B82, horizontal8.006, vertical6.639, rise/run0.829,
  portal (-866.933,-3458.133,86.136) to (-874.667,-3456.000,80.136).
  Alternate straight leg has horizontal8.561/vertical7.184, refs unknown;
  rejection `unsafe_terrain_attempt_limit`. Unknown refs stay unknown.
- Lines25570-25744: full-map fallback begins,704 tiles; at188 processed,
  elapsed35061 ms, the160-tick physical watchdog destroys intent34 and
  unfinished loader. It has already consumed the sole full-map allowance.
- Precision retry35 and variants then repeat route/expanded rejection with
  full-map disabled. No CTM route movement. At180209 ms without progress,
  unchanged liveness policy authorizes one strategic continuation using the
  previously proven bad edge. Its full-map loader is also interrupted by the
  precision watchdog; latest retry43 fails expanded validation.
- Line27667: `strategic_route_failed`, navFailure=path_validation_failed,
  routes never became usable, ghost stayed stationary. This is neither a
  reclaim failure nor evidence that the corpse destination was unprojectable.
- Ghost AFK qualification and later prevention in RoutingToCorpse AND Failed
  passed in this same capture. Death failure must not be blamed on AFK.

Older `missing_corpse_anchor` is historical/user-reported evidence preserved
in continuity; its original capture was not found among retained project
runtime-captures. Its current source path is independently verified below.
Do not present a reconstructed timestamp/position as that older live evidence.

## Separate root causes

`missing_corpse_anchor`: Start's bootstrap path previously consulted only
lastClearlyAlivePosition or a recent same-character persisted body record;
absence immediately entered Failed. It did not read the server-fed client
corpse cache. A historical healthy position could also be mistaken for the
current corpse. The fix removes that promotion and allows bounded server
publication observation instead. Missing data is not guessed.

`strategic_route_failed`: legitimate shared terrain rejection PLUS a proven
owner-layer bug: physical-stall rerouting unconditionally destroyed a pending
incremental loader. Planning is not physical progress, but it is also not an
executing movement command that can be fixed by precision rerouting.
The watchdog now preserves a pending loader/intent. The unchanged180000-ms
no-progress and300000-ms episode deadlines still terminate it; loading does
NOT earn progress/reset budgets. Full-map success remains unproven. Expanded
unsafe-terrain failure alone is not evidence a globally safe route exists.

## Build5875 corpse cache audit

Client `/home/ludvig/Games/WoW Vanilla/WoW.exe`, SHA256
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`,
base0x400000. Reproduce read-only:

```sh
python3 tools/death_client_audit.py '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x48f734 --stop-address=0x48f819 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x491f50 --stop-address=0x4920c0 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
```

0x48F494 registers opcode0x216 to dispatcher48F690. Its two-level switch
maps that opcode to48F734. Handler reads found byte; requires the active
player and PLAYER_FLAGS Ghost0x10; parses display map, three float XYZ,
actual corpse map and calls492010. Writer stores display map B4E31C,
XYZ B4E284/B4E288/B4E28C, actual map B4E320. Getter492090 reads the same
fields. No corpse GUID/publication timestamp is supplied; telemetry says
unknown, never a synthetic GUID or age.

491F50 first invalidates maps to -1/XYZ0 via492010, then requests normal
MSG_CORPSE_QUERY only for Ghost. Client world initialization490A28 and active
player Ghost-bit-change callback5EEAC1 call this reset/query. Callback
5EE990 computes old XOR current flags; changed0x10 is the reset gate, on both
entering/leaving Ghost. Thus the cache has a normal reset across life/world
changes; reading happens on the game thread, after callback execution. No
hook, request, native writer, flag patch or packet is added by the bot.

Local VMaNGOS QueryHandler.cpp155-201 corroborates protocol: missing corpse
returns found0; otherwise found1/display map/XYZ/actual corpse map. Cross-map
dungeon corpses can yield entrance coordinates instead of corpse coordinates.
The reader explicitly requires BOTH maps equal the requested/current bot map;
it does not reclaim using an entrance. Cross-map recovery remains unsupported.
This local server is protocol evidence, not proof of the live server revision.

Source-backed object fields CORPSE_FIELD_OWNER=word6 and POS_X/Y/Z=9/10/11
also exist, but this checkpoint does not add a streamed-object/GUID source.
The current WorldState unit list does not enumerate corpse snapshots; a corpse
hundreds of yards from the graveyard need not be streamed. Server-fed cache
is the new source, not a fabricated nearby object.

## Anchor and state model

| State | Evidence/action/next state | Bounds and ownership |
| --- | --- | --- |
| Idle | health0 body, or fresh Lua dead/Ghost at HP1; suspend workload before Start | Shared Questing/Grinding death gate, same-character identity |
| ReleasingSpirit | RepopMe dispatch -> WaitingForGhost; dispatch never proves Ghost | Existing2-second retry; at most6 attempts, no unearned cycle reset |
| WaitingForGhost | fresh UnitIsGhost confirmation; known routing anchor -> precision/ordinary route, or reclaim when near | Existing1-second probes; no ghost release command |
| AwaitingCorpseAnchor | no movement/reclaim; sample server cache until same-map/current-player Ghost anchor | 12000-ms publication bound; timeout corpse_anchor_pending_timeout |
| RoutingToCorpse | shared scoped->expanded->full-map initialization; movement readiness and physical displacement are separate | Keep intent during pending load; original2000/4/2 navigation limits; death180/300-second bounds |
| WaitingForReclaim | current server location + fresh Ghost probe + known nonnegative delay0 + within8-yard precision gate | Unavailable cache -> bounded anchor wait; no inferred-position reclaim |
| WaitingForAlive | RetrieveCorpse dispatch, then two fresh known-not-dead/not-Ghost probes with positive HP | Existing2-second retry/two-attempt precision reposition, total8 reclaim commands per death |
| Done | confirmed automatic alive; invalidate persisted active body record; monitor re-arms and resumes workload | Follower released before normal ownership resumes |
| Failed | exact terminal reason, last query tier/projection/validation retained; follower released | Death ownership deliberately remains while dead/Ghost; only existing two same-character fresh alive probes release it |

Sources: `observed_body_routing_only`, `recent_body_record_routing_only`,
`server_corpse_location`, `unknown`. Old healthy coordinates are never chosen
for Ghost. Persisted records retain existing GUID/map/active/age/finite checks,
are cleared on confirmed alive, and cannot grant reclaim. The cache is sampled
fresh before reclaim; absent data/negative or non-finite delay cannot grant it.
The publication bound is an engineering limit derived from the existing six
two-second release opportunities, NOT measured network latency. No existing
large timeout is increased.

Server MiscHandler.cpp573-605 independently requires Ghost, current corpse,
reclaim delay expiry and within-map radius before resurrection. Request accepted
is not observable from dispatch alone: two fresh alive probes remain required.
Unknown delay does not invalidate independent alive/dead/Ghost observations.
World gaps cancel pending routes without dispatching against an unavailable
player; next valid snapshot records world_state_lost under death ownership.

## Diagnostics and protected behavior

Follower failure evidence now carries the last path-validation detail,
destination-projection evidence and initialization tier. Death terminal events
include anchor source/known, tier, projection and actual validation reason.
Unsafe terrain remains rejected; zero-reference transitions are not invented.
Strategic failures report unsafe_terrain_attempt_limit/no_alternative when
supported, otherwise exact failed tier or explicit projection/search uncertainty.
State/anchor/fallback/plan/reclaim events are transition-based. Physical progress
events are limited to the existing five-second status cadence, not every tick.

No route query, dispatch, state change or tile count resets physical liveness.
Precision watchdog still handles actual stalled movement. Existing strategy
eligibility, full-map allowance1, route attempts18, stationary failures9,
episode300s/no-progress180s remain unchanged. Native AFK and Ghost masks untouched.

## Offline geometry evidence

`build/navmesh_debug` replay of exact captured start/destination, output
`/tmp/wow-death-navmesh-audit`: start projectionZ63.740, endZ92.233,
standard50 polygons/454.428 yards; non-steep42/455.932 yards, projection
identity matches, first unsafe straight leg26. End projects successfully;
this does not validate traversal. Inspector loads12 local/route tiles, not
the cancelled704-tile live full-map attempt, and does not replay persistent
hazards. Production never uses these diagnostic coordinates.

## Tests and runtime gate

New death_recovery_evidence_test covers bounded anchor acquisition, invalid/
foreign/stale records, map/finite/ghost identity, fresh reclaim conditions,
pending-loader preservation, unchanged budgets, no command-only alive success,
shared workload/AFK integration sentinels and no memory writes. Existing death
ownership/terminal/liveness tests remain relevant; the old immediate-failure
entry expectations are updated. The initial new test failed before the policy
existed; the first full run caught the stale enum expectation in an older test.
Final validation status/artifact is recorded in CODEX_PROJECT_STATE.md.

Run normal mode; never intentionally kill or manually rescue:

```sh
wine ./build/wow_gui.exe
tail -n 0 -F build/wow-internal.log | rg --line-buffered 'DEATH|Death|CORPSE|Corpse|RECLAIM|Reclaim|NAV |NAVMESH|MOVEMENT INTENT|AFK '
```

Require natural death -> release -> Ghost -> server corpse anchor -> same
intent through tiers -> validated movement and physical progress -> reclaim
eligibility/dispatch -> two alive probes -> death ownership exit -> Grind
resume. One complete zero-intervention cycle required for RUNTIME PASS;
prefer two for stability. Long Ghost routing must retain existing verified AFK
qualification/prevention. If full-map terrain still fails, keep exact evidence;
do not relax safety or inflate budgets. No later phase begins here.
