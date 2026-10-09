# R0.1 unattended maintenance / connection audit

## Automatic vendor episode / AFK incident (2026-10-09)

Starting branch `codex/vendor-afk-long-navigation`, clean checkpoint
`e1f938f0372551701256bb6fe8922c136286f411`. Read the complete capture
`runtime-captures/vendor-afk-2026-10-09/wow-internal.log`: 28,187 lines,
2,506,205 bytes, SHA256
`2241aaae32dd590379b437ae3e53395d01a0ce81dee660389532d68c15d30e9c`.
This section supplements the historical incidents below. The incident proves
the old behavior only; the implementation described here is **RUNTIME PENDING**.
The committed living-water egress and full-bag two-read fix are retained.

### RUNTIME OBSERVED — complete episode chronology

The session spans monotonic 20749417 through 21575159 (~13m46s). There are
**seven vendor controller starts**, six direct-aggressor preemptions and six
two-read bag releases, rather than one uninterrupted vendor trip. All starts
request food, with bag pressure, repair and drink false. No completed vendor
trip or verified AFK input delivery is established.

| Capture lines | Observed sequence |
| --- | --- |
| 5472–10547 | First three starts (5472, 7604, 9388) select 3166. Their navigation intents last 9, 39 and 25 ticks. Direct aggressors preempt each; bag-space proof releases each wait. |
| 10566–15382 | Fourth start selects 3166, 3165, 3168, 3167, 3163, 6928, 3881, 3164 and 6027. Merchant UI opens four times. Entries 3166/3165/3168/3167 report no usable food. Other failures include candidate failure, bounded local recovery unavailable, replan budget exhausted and surface recovery exhausted. Nine candidate failures are recorded before 3882 is selected. |
| 15439–17537 | Entry 3882 has selected route cost 1409.503. Intent 18 starts at tick 1292 near (315.343,-4800.490), destination (-560.125,-4217.200). It is released at tick 1830, age 538, near (-500.234,-4677.820), with 62 commands and 12 logged replans. The release result is `owner_released`, followed by direct-aggressor preemption, **not arrival or a final navigation failure**. Local recovery activity precedes the release. |
| 19286–21811 | Bag proof releases the wait; fifth start at 19303 selects 3933, 3187, 7952, 3186, then 5942. First four candidates fail local/surface recovery. 5942's intent 23 lasts 117 ticks before another combat preemption. |
| 19980 / 20844 / 21664 | Native inputAge 240046 → DUE (`movement_or_transport_flags`); 270108 → OVERDUE (`combat_or_ability_owner`); 300140 → THRESHOLD CROSSED (`movement_or_transport_flags`). At threshold, Grind is Vendoring, vendor is NavigatingVendorAnchor, combat is PostKillDelay with locked GUID zero. |
| 23332–24410 | Sixth start at 23349 follows bag-proof release, selects 5942, fails with replan budget exhausted after a 70-tick intent, then selects 10369. Combat preempts that next 23-tick intent. |
| 25863–28184 | Seventh start at 25880 follows another bag-proof release. It selects 10369, later rejects it, then selects 3882 with route cost 795.731. Final intent 28 lasts 214 ticks until user stop; it is not evidence of arrival or completion. |

The capture has 22 selected-hub events and 15 candidate backoffs. There are
no `VENDOR: FAILED` terminal records: candidate failures are handled inside
the trips, and combat preempts six trips. The offline reliability audit's
terminal-vendor-failure count therefore must not be read as zero navigation
failures. No world gap, water block or death-recovery incident appears here.

At line 21662, AFK is authoritatively observed as `known=yes active=yes
clientActive=yes`, not merely inferred from the threshold or candidate flag.
The native input clock also changed earlier: line 211 reports 20735865 and
line 21662 reports 21094506. The capture does not attribute that advance to
bot AFK input, CTM or a user action. InputAge must not be described as rising
continuously from session start. An earlier ordinary Grind route was rejected
with `water_traversal_disabled` (2357/2378); this was not a swimming incident.

### SOURCE VERIFIED — audit answers and root cause

1. **Why hubs change:** `StartAlternateServiceSearch` rejects the current
   entry after unusable service or failed navigation/interaction, increments
   its per-trip candidate-failure count and prepares another selection.
   Food availability is learned at the merchant; generic merchant capability
   alone does not establish usable food stock.
2. **Existing bounds:** route/expanded/full-map planning uses finite tile
   lists (two tiles per incremental step). Selection probes disable full-map
   fallback and shortlist at most four candidates. Generic routes have a
   2000-unit stage/path limit, 1700-unit long-stage target and at most eight
   long stages; ordinary replan budget is four, with separately bounded
   recovery mechanisms. These are not a cumulative intent wall-clock bound
   (the capture's 12 logged replans are not 12 ordinary budget allowances).
   Surface attempts remain four; last-safe backtracks two. Vendor direct
   approach has eight moves, three no-progress moves and three local probes;
   interaction has five attempts, MerchantFrame wait 60 ticks, metadata wait
   240 ticks, search waits 80/48 ticks. A trip has ten candidate failures,
   but no cumulative trip age limit. The failed full-bag wait has two extra
   retries and existing 480/2400-tick cooldowns.
3. **Episode-wide bound:** absent at the starting checkpoint. The supervisor's
   240-tick vendor-owner watchdog watches *lack of progress*: owner state,
   progress serial and physical progress can refresh it. Planning has a
   separate four-minute watchdog deferral; strategic outcome monitoring is
   also not a vendor episode lifetime limit, and kills refresh outcomes.
4. **Immediate distant alternates:** yes, after the hold/preparation/probe
   updates, without a maintenance cooldown between failed candidates.
5. **Failure memory:** per-trip rejected entries were cleared by Reset/Start.
   Entry backoff lasts 2400 ticks and survives Reset. Combat preemption does
   not prove a candidate bad and did not blacklist it. This explains reuse
   of preempted 3166, 5942, 10369 and 3882 without claiming catalogue errors.
6. **Ranking:** known/persisted/live-observed hubs merge with the build-5875
   service catalogue, filtered by map/faction and service suitability. The
   nearest spawn per entry is considered. Shortlisting uses Euclidean
   distance; final selection uses the greater of Euclidean distance and
   probed route length, preferring navigable over direct fallback candidates.
   Discovery has no geographic radius or qualified food-inventory metadata.
   Long routes are allowed source behavior; this log does not justify a
   merchant-coordinate correction, new radius or catalogue rewrite.
7. **Vendoring plus PostKillDelay:** `CombatSafeForVendor` permits that state.
   Once Vendoring owns updates, it calls vendor Update instead of combat
   Update, which normally advances the four-tick post-target delay.
8. **Stale ownership:** the initial delay is intentional, but an expired,
   unlocked delay surviving throughout vendor navigation is stale. WorldMonitor
   maps this non-idle/non-acquiring combat state to an AFK combat blocker.
   SharedAfk logs the native movement rejection when present, otherwise the
   policy combat blocker; neither means an AFK pulse was delivered. Vendor
   ownership independently remains an AFK blocker even after retiring delay.
9. **Why it survives AFK milestones:** there was no absolute vendor attempt
   deadline shared across navigation intents. Combat preemption also resets
   trip candidate accounting; two free-bag observations correctly release
   the bag wait even though food is still missing, enabling a fresh food
   trip. AFK Due/Overdue/threshold are observations, not route cancellation
   triggers, and cannot authorize unsafe input.

**SOURCE ROOT CAUSE:** finite local navigation/search budgets did not compose
into a persistent automatic maintenance attempt bound. The observed restart
path could refill trip state without finishing the food request. The stale
delay is a separate ownership defect. **INFERRED:** these paths contributed
to continued routing and AFK starvation; neither sole causation by vendoring
nor an incorrect native movement guard is established.

### Implementation and ownership

`AutomaticVendorEpisodePolicy` is opted into only by automatic Grind
maintenance. `Begin` is idempotent while active. Its new explicit
`MaximumAttemptTicks=480` is an absolute **monitor-tick** budget (~120 seconds
at nominal cadence), chosen on the existing 480-tick maintenance/global
recovery scale. No previous source constant promised a maximum total trip
duration: this is a new conservative policy, not a recovered client fact or
a derivation from the native 300000-ms AFK threshold. It includes planning,
candidate switches, interaction and return navigation. It allows short local
trips but may stop a legitimate long journey or slow full-map initialization.
Live usefulness at this bound remains to be qualified. It is not a hard
wall-clock deadline if the worker or synchronous client call stalls.

The policy and rejected-entry set survive VendorController Reset/Start,
combat preemption, fallback changes, new movement intents and robustness
resets. Known failures remain excluded for the whole active episode even if
their ordinary timed backoff expires. Quest/manual callers do not opt in;
their selection behavior is unchanged. Ranking, data and navigation budgets
are unchanged. Preemption alone still does not reject a candidate.

Ownership is now: automatic maintenance → persistent episode → vendor
selection/navigation/transaction → either verified completion, or bounded
expiry → destroy vendor route/probe followers → hold current position →
WaitingForManualVendor. Direct aggressors retain priority before expiry
handling; world/death/water owners remain outside and above this path. The
first subsequent eligible maintenance update enforces expiry. Expiry cannot
call vendor Update or reacquire a route in the same tick. `holdIssued` remains
dispatch evidence, not measured stillness or native input-clock advancement.

The wait preserves two extra retries and 480-tick bag/urgent or 2400-tick
ordinary cooldowns. Expiry while already waiting or returning from an ordinary
failure backoff preserves its established retry deadline rather than imposing
a second cooldown. Rejected retry starts retain the existing 480-tick retry
path. Only the wait's explicit retry starts a fresh 480-tick attempt;
the episode also caps total attempts at three so a reset of bag-wait accounting
cannot create nested unlimited retries. After exhaustion, free bags alone
cannot erase an unfinished service-search episode: a separate service-proof
latch requires fresh satisfaction of originally requested services plus the
existing two fresh bag reads >1. Unrequested food/repair/drink needs cannot
extend the hold; a bag-only request needs only its two fresh bag observations,
even if unrelated service metadata is unavailable. Each contributing scheduled
bag read also refreshes maintenance evidence; unavailable requested-service
evidence fails closed. World gaps invalidate
partial bag proof as before. Exhausted retries remain MaintenanceBlocked with
no ordinary acquisition. This deliberately scopes the extra release gate to
the new exhausted-service episode; an ordinary full-bag wait still releases
on bag proof alone. Successful completed maintenance clears the episode;
an existing bag lock still requires its two subsequent scheduled reads.
Completion checks a fresh maintenance snapshot against the original request,
including known durability for repair. A retry's terminal flag alone is not
proof. Outside a vendor/bag-wait owner, newly verified maintenance and a fresh
bag read can also close a preempted episode (for example after acquiring food),
so its old deadline does not affect a later unrelated maintenance need.

The separate combat handoff retires only an elapsed PostKillDelay with no
locked GUID and no deferred corpse work. It changes state to AcquiringTarget
without running combat Update, acquiring a target, resetting a target, or
sending input. Real combat/recovery states and the unexpired delay are retained.

Read-only parallel review (Runtime Forensics, Source Architecture Audit and
Test/Regression Audit) confirmed the chronology and identified two corrections
made by the coordinator. The episode proof predicate now checks only originally
requested services, leaving manual-mode semantics untouched. Expiry during
ReturningToGrind/Done still ends the attempt and cancels navigation, but does
not reject or back off the merchant whose service completed. Only unfinished
candidate/service work receives the expiry penalty. `vendorState` and
`candidatePenalized` report that distinction. The existing VendorState enum
was extracted unchanged into a portable header so tests use production states.

Sparse telemetry: `VENDOR EPISODE state=started`, selected candidate with
previous/next, attempt, failures and episode age; `state=exhausted` with attempt,
candidate-change/failure counts; cancellation/hold and retry deadline in
`MAINTENANCE WAIT`; `state=retry`, `state=completed`; and
`COMBAT MAINTENANCE HANDOFF reason=expired_post_kill_delay`. Existing per-candidate
backoff logs supply each failure reason. No per-tick episode log is added.

### Validation and supervised runtime acceptance

Focused tests cover short success, alternates, retained age through repeated
starts/candidate changes, exact expiry, cooldowns, at most two additional
attempts, terminal no-spin, fresh service/bag proof, unknown/world-gap proof,
controller Reset/Start wiring, no same-tick reacquisition and the isolated
post-kill handoff. Existing navigation, water, death and AFK tests remain
required. Additional executable policy sequences cover repeated aggressor
interruptions and bag release without deadline refill, rejected retry startup,
successful retry followed by fresh bag proof, return-versus-outbound expiry,
and original bag-only/food-only requests with newly appearing unrelated needs.
Windows-bound controller wiring remains covered by narrow source contracts,
including safety-owner priority before post-kill retirement; these are not
full controller execution tests. Static tests/build do not qualify live routing
or AFK delivery.

Final static/build validation after parallel audit corrections: `python3 tools/validate.py --jobs 4` —
**PASS: 108 C++ tests; failures=[]**, including the Python/Lua suites and
MinGW DLL build. Report: `/tmp/wow-validation-ohk53k2d/results.json` (local,
temporary artifact). `cmake --build build` and `git diff --check` also pass.
No WoW runtime was performed and no commit was created.

1. Preserve the previous complete log. Use the rebuilt DLL and the existing
   configured client/Wine prefix. Start the normal GUI from this directory:
   `env -u WOW_INTERNAL_CONNECTION_MODE -u WOW_INTERNAL_WATER_MODE -u WOW_INTERNAL_AFK_MODE wine ./build/wow_gui.exe`.
   Choose normal Grind with automatic vendor enabled. Confirm AFK protect
   mode, alive/world-valid state, and record revision/session/GUID.
2. Supervise a **natural** maintenance occurrence. Do not manufacture vendor
   failure, water, death, network loss or AFK input. Preserve the complete
   session, including candidate probes, movement intent releases, native
   inputAge/blockers and shutdown. If nothing occurs, leave status pending.
3. **Success path:** require episode started, a reasonable selected route,
   matching merchant UI, verified maintenance result, episode completed and
   ordinary Grind resume. Merchant opening or dispatch alone is insufficient.
   If a prior bag lock exists, require two distinct subsequent fresh bag
   observations >1 before resume.
4. **Bounded failure path:** if natural failures/alternates or a long trip
   reach expiry, require one exhausted transition, route/probe cancellation,
   hold dispatch followed by measured stopped state, maintenance wait and no
   same-tick or ongoing automatic candidate route. Correlate candidate changes
   and elapsed age to the original attempt; combat interruptions must not
   create another `state=started`. Observe existing cooldown-qualified retries
   only, at most two extra attempts, each finite. If exhausted, require
   MaintenanceBlocked; free slots without satisfied service must not restart
   food search. Natural verified maintenance plus two fresh bag reads may
   release the hold. Do not change bags/services just to force this test.
5. Where post-kill handoff naturally occurs, require the handoff event after
   the delay deadline with no locked target. Vendor remains AFK-blocking;
   real aggressors must still preempt for defense. Keep the full blocker
   chronology rather than expecting an AFK pulse during active navigation.
6. A naturally stopped, safe wait reaching Due may qualify AFK separately:
   require authoritative native input-clock advancement and verification.
   CTM/hold/AFK dispatch alone is insufficient. Unsafe movement, swimming,
   combat, recovery, UI and unknown evidence must still block. Thresholds
   remain Due 240000, Overdue 270000, native 300000 ms.
7. Stop normally; retain logs through detach/unload. If unsafe routing or
   unintended ownership appears, stop the bot and preserve the evidence.

**UNKNOWN / RUNTIME PENDING:** real short-trip success under the cap, bounded
alternate/exhaustion and retry sequences, combat-preemption persistence,
service-latch release, live expired-delay handoff, and safe stopped-wait AFK
delivery. No runtime PASS is claimed. `runtime_reliability_audit.py` is
unchanged and does not automatically qualify the new episode; inspect these
explicit transitions alongside its summary. Generic connection qualification
and reconnect flags remain false; live disconnect-dialog visibility, Glue
lifetime and safe reconnect eligibility remain source gaps.

WATER EMERGENCY EGRESS — RUNTIME PENDING

SOURCE GAP — RECONNECT NOT IMPLEMENTED

## R0.1 maintenance runtime preparation (2026-10-09)

Branch `codex/r01-maintenance-runtime`; starting checkpoint
`0c6064a4b50f6dad5ff49f8aac149b0e9c01f269`, initially clean. Inspected current
source against `6b4407f9c3b1109f55052d72f1243f9205b28525` and restored checkpoint
`a329e0b`. The complete incident reconstruction below remains the historical
baseline; the excerpt-only account is still superseded. No WoW runtime was
performed for this change. Maintenance qualification is **RUNTIME PENDING**.
R0.1b.1's recorded read-only lifecycle **RUNTIME PASS** is unchanged and does
not qualify maintenance, reconnect, or comprehensive owner reconciliation.

### SOURCE VERIFIED — current target status

| Target | Starting source and disposition |
| --- | --- |
| Automatic full-bag retry episode | Present: `UnattendedMaintenanceWaitPolicy::MaximumRetries=2`; retry count survives `FailedTrip`, including rejected starts. Cooldowns remain 480 ticks for bag pressure/urgent repair and 2400 for ordinary maintenance (nominal 2/10 minutes at 250 ms). Initial start failure retains its existing urgent/ordinary backoff. Exhaustion/automation disable produces MaintenanceBlocked. No retry-limit, merchant, navigation or discovery change. |
| Two-read bag-lock release | Present in the wait owner: scheduled probes every eight ticks; only known freeSlots >1 contributes; stale polls do not count, unknown/full/one-slot reads reset proof, unsafe owners invalidate it. WorldMonitor invalidates bag/maintenance snapshots and partial proof on every world gap. **Concrete terminal-path bypass found and fixed**, described below. |
| Stopped-wait AFK | Present: WaitingForManualVendor is a healthy-idle candidate, while Vendoring blocks as a transaction. Grind navigation ownership is included; the wait does not receive benign-moving-work permission. SharedAfk retains all combat/death/recovery/navigation/UI/water/unknown movement/native gates. Native 300000 ms, Due 240000 ms, Overdue 270000 ms and movement masks unchanged. No AFK behavior change. |
| Passive world recovery | Inspected only: unavailable snapshots suspend updates, invalidate navigation cache on entry, bag/maintenance and terminal-alive evidence, and reset SharedAfk. Return rebaselines RuntimeRobustnessSupervisor. This does not establish intended-player identity or comprehensively clear/reconcile stale combat/vendor/navigation owners. No connection action or reconciliation change. |

The two terminal paths in `GrindModeController::Update` bypassed the wait's
proof gate: a failed retry with one fresh free slot took Grinding, leaving the
episode active outside its wait owner; a completed retry with one read >1
reset the episode before taking Grinding. Both could release an existing
full-bag lock without the two scheduled observations. This is a SOURCE VERIFIED
control-flow defect, not a new runtime diagnosis of the historical incident.

The failed path now holds whenever an episode is active. The completed path
preserves successful-trip accounting and ordinary maintenance suppression,
then keeps the episode/retry count and returns to WaitingForManualVendor.
Its terminal read does not count toward release; it arms the existing 480-tick
bag retry cooldown and requires two subsequent scheduled probes. Initial
trips with no existing episode retain their ordinary completion behavior.
`state=recovered` resets the episode and permits the ordinary scheduler to
resume; remaining food/repair needs cannot retain this bag lock indefinitely.

Added sparse evidence needed to qualify this gate:

- `MAINTENANCE BAG PROOF tick=... fresh=... freeSlots=... observations=...`
  logs changes in the proof count, including a second identical bag read.
  Existing `GRIND 14G.2: bags` logs suppress unchanged contents and could not
  independently show that second read. World gaps log proof reset once when
  partial proof exists, with `observations=0 reason=world_gap`.
- `MAINTENANCE WAIT state=retry attempt=... tick=... retryAtTick=...`
  records the consumed retry and its deadline. A start attempt is not proof
  of merchant interaction or a successful transaction.
- `MAINTENANCE WAIT state=confirming_space reason=vendor_retry_completed`
  marks a completed retry retained under the bag lock. `holdIssued` reports
  dispatch, not measured stillness or AFK input-clock advance.

Focused regressions cover the terminal-path integration contracts, retained
retry budget, zero/one/two-read proof, stale/unknown reads, one-slot boundary,
world-gap wiring, and stopped-wait AFK blockers. Existing AFK tests cover
threshold bands, movement masks and dispatch-versus-clock confirmation.
`ActiveBotAfkSafeguard` remains the legacy acquisition timer; its request/action
counters do not replace SharedAfk's native clock evidence. The offline
`runtime_reliability_audit.py` and its qualification flag remain unchanged:
it counts explicit events and separates passive world return from reconnect.
Its changed-bag summary does not prove the new two-read sequence; inspect the
proof events in the complete log as well.

### RUNTIME OBSERVED / INFERRED / UNKNOWN

RUNTIME OBSERVED: only the previously recorded complete incident and R0.1b.1
read-only lifecycle evidence below. There is no maintenance runtime PASS for
this revision. INFERRED: the corrected branches should prevent premature
release; live qualification must still demonstrate that result.
UNKNOWN: natural failure/retry/exhaustion behavior on this revision, release
after a completed or failed retry, guarded stopped-wait AFK delivery, actual
later unsupported movement flags from the old incident, and comprehensive
same-player owner reconciliation. OM loss remains world evidence only;
historical lastGlueScreen remains historical. Live Glue/dialog visibility,
loading discrimination, interpreter lifetime and safe reconnect eligibility
remain source gaps. `connection_client_audit.py` is unchanged; its generic
runtimeQualified stays false and reconnectImplemented stays false.

### Safe manual qualification runbook

1. Use the rebuilt DLL and normal configured Wine prefix/client. Close the
   previous GUI session, preserve its logs, and start a fresh normal workload
   session from the project directory:

   ```sh
   env -u WOW_INTERNAL_CONNECTION_MODE -u WOW_INTERNAL_WATER_MODE -u WOW_INTERNAL_AFK_MODE wine ./build/wow_gui.exe
   ```

   Choose Grind with automatic vendor enabled in the existing GUI; start only
   in an ordinary safe, alive in-world situation. Confirm normal Grind startup,
   `vendorAutomationEnabled=yes` and `AFK CONFIG mode=protect`. Record session
   time, revision, player GUID and world pointers. Do not use diagnostic AFK
   qualify mode or alter thresholds to create a stopped wait.
2. **Natural automatic retry:** observe an ordinary full-bag vendor failure
   or rejected start. Preserve the entire trip from START through terminal
   reason and wait entry. In that same uninterrupted episode expect attempts
   1 and at most 2, each with `tick >= retryAtTick`; correlate failures with
   their existing cooldown. No third attempt is allowed before a verified
   recovery/reset. If both retries fail and bags stay full, expect
   MaintenanceBlocked and no passive-target acquisition or repeated starts.
   Observe beyond the last deadline plus one bag-probe interval. If a trip
   succeeds or the episode releases sooner, mark exhaustion unexercised.
3. **Release:** during a naturally reached hold, ordinary manual inventory
   maintenance may free space using wanted sales/storage; do not destroy
   items or arrange dangerous travel. Keep the same running session/episode.
   One free slot must not release. With >1 free slots, require two
   `MAINTENANCE BAG PROOF` events with `fresh=yes`, counts 1 then 2 and distinct
   ticks at least eight apart, followed by `state=recovered` and
   `GRIND MANUAL VENDOR CLEARED resuming=grind`. A retry finishing with space
   must still pass this gate; a successful retry first logs confirming_space.
   Observe subsequent ordinary work. Unknown reads or a single transient read
   must not produce recovered. Do not inject read failures to exercise this.
4. **Stopped-wait AFK:** only when the wait occurs naturally in a safe,
   stationary place, finish manual UI activity, close interactive panels and
   leave input alone under supervision. At normal native input age Due
   240000/Overdue 270000 ms, inspect AFK SAFETY QUALIFICATION and production
   decisions: wait is not vendor-owned, allowed stationary mask is 0x100,
   unsupportedBits must be zero, and all other guards still apply. An active
   vendor trip must block AFK. An F12 dispatch/pending result is insufficient:
   require inputClockAfter != inputClockBefore and
   `AFK ACTION RESULT result=confirmed reason=client_input_clock_advanced`.
   AFK clear, if needed, requires its separate client/server evidence. User
   input during that window makes attribution inconclusive. A legitimate
   blocker is not permission to bypass the guard; save its raw evidence.
5. **World boundaries, observation only:** if a world gap occurs naturally,
   preserve before/gap/return logs. Partial bag proof must reset; return needs
   two new reads before release. Compare GUID and manager/local-player
   addresses separately from serverConnection; never label passive return
   reconnect. Do not manufacture a gap while maintenance owns input. For an
   optional safe normal logout/Enter World/same-character check, Stop Bot,
   verify detach, and use the separate action-free R0.1b.1 observer procedure
   below. That check does not qualify active-maintenance reconciliation.
6. Stop Bot and verify BOT SESSION STOP / RUNTIME DETACHED / DLL unload with
   WoW still running. Preserve complete `build/wow-internal.log` and
   `build/wow-internal.lifecycle.log` before another session. Summarize a copy:

   ```sh
   python3 tools/runtime_reliability_audit.py build/wow-internal.log
   rg -n 'MAINTENANCE WAIT|MAINTENANCE BAG PROOF|GRIND FULL BAG BLOCK|GRIND MANUAL VENDOR CLEARED|AFK (SAFETY QUALIFICATION|PRODUCTION|CANDIDATE|ACTION)|DISCONNECT DIAGNOSTIC' build/wow-internal.log
   ```

Mark each actually exercised condition separately. Natural repeated vendor
failure/exhaustion, successful/failed retry release, unknown/transient bag
reads, a gap between partial proof reads, and stopped-wait AFK/native blockers
remain PENDING unless captured. Do not provoke death, water, network loss,
process exit, or authentication errors. No credentials are needed or recorded.

Validation: `python3 tools/validate.py --jobs 4` PASS, 105 C++ tests with
`failures=[]`, 42 audit Python tests, 13 QuestDB Python tests, SQL and all
10 Lua fixtures. Detailed report: `/tmp/wow-validation-1_xhialc/results.json`.
`cmake --build build` PASS (MinGW DLL rebuilt); `git diff --check` PASS.
`git status --short` contains only this document, `GrindModeController.h`,
`UnattendedMaintenanceWaitPolicy.h` and `unattended_maintenance_wait_test.cpp`.
No commit made. These are static/build results, not maintenance runtime proof.
SOURCE GAP — RECONNECT NOT IMPLEMENTED.

## R0.1b.2 live Glue/UI source research (2026-10-09)

Starting HEAD `72267d4ba0fc303c976834da057134fd51bf00f4` verified on
`codex/r01b2-live-glue-observer`; worktree initially clean. The checkpoint's
R0.1b.1 **RUNTIME PASS** remains limited to the normal read-only lifecycle and
cooperative shutdown recorded below. R0.1b.2's **live-state qualification gate
is INCOMPLETE / SOURCE GAP**: no new live UI field passed the identity/lifetime
gate. This checkpoint adds reproducible offline binary checks, separate explicit
unknown dialog fields and regression coverage, not a qualified live Glue reader.

### SOURCE VERIFIED — exact local assets and control flow

Inspected `/home/ludvig/Games/WoW Vanilla/WoW.exe` directly. SHA256 matches
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`;
PE32 i386 image base remains `0x400000`. Disassembly and extraction artifacts
are under ignored `build/r01b2-research/`. All addresses below are image VAs.
The executable was not launched, attached to, patched or called for research.
No credentials or account configuration were read.

Read the installed `Data/interface.MPQ`, `Data/patch.MPQ` and `Data/patch-2.MPQ`
using mpyq 0.2.5 installed only into the ignored research directory. Checked
each named asset in all three archives: patch-2 overrides AccountLogin.lua;
the other assets below are absent from patch-2 and supplied by patch.MPQ.
The older interface.MPQ copies differ and were not used as the current source.
The four previously recorded Lua hashes are reproduced exactly:

| Effective asset | Archive | SHA256 |
| --- | --- | --- |
| GlueParent.lua | patch.MPQ | `78e8976a42de4ca5015555031abb54c48dd6f08eb424d42ddbacce732b9e6ccd` |
| GlueDialog.lua | patch.MPQ | `f5bcc69ac824030b1e05cd2bda7dd1197603b8a45cac3e8046038d7d1248c8b9` |
| CharacterSelect.lua | patch.MPQ | `972bbcb0072750e3827a0b5e725b1ad46891c916fdb4bb88cf56400da69960bc` |
| AccountLogin.lua | patch-2.MPQ | `d7824475393e7b0e21ff19477a83c0f35d0b9849b59571fbdc0b0bbacafe921e` |
| GlueParent.xml | patch.MPQ | `431a30df7b1e5fca0fc9692a567adb24b70a4a14744b3b4afbaad61f3be4be06` |
| GlueDialog.xml | patch.MPQ | `20adcaf153f8dc4335f3008fca3acce37cf7b744715a544985ec38c6f53b6b6d` |
| GlueXML.toc | patch.MPQ | `d7442ddfc49c133aec865d4b6aa2b0c553a9689dbf419c4c91cd676cafca3d95` |

Source conclusions (Lua line numbers refer to those exact extracted files):

- **Existing server predicate and historical buffer preserved.** Registration
  `0x8374a0` → `0x46d380` still tests `[singleton+0x1b00]` via getter
  `0x5ab490` / `0xC28128`. `SetCurrentScreen` still copies to `0xB41478` through
  `0x46b860`. Neither predicate establishes current Glue visibility.
- **Current and pending names have different, incomplete semantics.**
  GlueParent.lua 23–24 initializes both globals to nil. Lines 32–60 hide named
  frames, show the selected frame, call SetCurrentScreen, then assign
  CURRENT_GLUE_SCREEN. Lines 109–120 set PENDING_GLUE_SCREEN for the
  login→character-select fade and later consume it, but do not clear it.
  A pending name can survive completion; its presence is not proof of an active
  transition. Neither string replaces a current native frame/lifetime proof.
- **Dialog type survives hiding.** GlueParent.lua 100–102 handles
  DISCONNECTED_FROM_SERVER by selecting login and requesting the DISCONNECTED
  dialog. GlueDialog.lua 147 assigns `.which` before Show at 181. CLOSE_STATUS_DIALOG
  hides the frame at 212–213; OnHide at 217–219 is empty; OnClick hides before
  executing type-specific behavior at 221–234. None clears `.which` on hiding.
  GlueDialog.xml 30 defines an initially hidden child of GlueParent. A stored
  DISCONNECTED type alone is therefore insufficient to prove a visible dialog.
  Its OnShow also calls StatusDialogClick (Lua 35–47); executing an apparently
  UI-only callback would not preserve this observer's action-free contract.
- **Prior login/character-select provenance preserved.** AccountLogin.lua
  25–47 clears password text on show; 95–104 submits edit-box values through
  DefaultServerLogin and clears password text again. No values were accessed.
  CharacterSelect.lua 43–54 uses IsConnectedToServer at character select;
  356–359 invokes EnterWorld. The native EnterWorld callback at `0x46d3c0`
  delegates to `0x46b500`, with selected-index/count checks and server-predicate
  check at `0x46b55f`. These are action implementations, not observation APIs.
- **Lua state pointer does not establish readiness or generation.**
  `0x7039e0` calls state creation at `0x6f6d20`, publishes the pointer to
  `0xCEEF74` at `0x7039ed`, then continues initialization. `0x703ba0` calls
  state close `0x6f6f80` before clearing `0xCEEF74` at `0x703bab`.
  `0x703b80` closes/recreates that state. Glue setup calls this reset at
  `0x46a87b`; FrameXML setup calls it at `0x48fe97` before loading
  `Interface\\FrameXML\\FrameXML.toc`, and another call exists at `0x491231`.
  Getter `0x7040d0` simply returns the shared pointer. It is not Glue-specific.
- **Native visibility semantics are qualified only for an already qualified
  native frame object.** Registration pairs at `0x878fd0`/`0x878fd8` map
  IsVisible/IsShown to `0x7758d0`/`0x775990`. These callbacks extract a native
  object from Lua table slot 0, validate its type through a virtual call, then
  test different DWORDs: `+0xd4` at `0x775955` versus `+0xd0` at `0x775a15`.
  This does not locate or validate the current GlueParent/GlueDialog object.
  Other registered object classes use different field offsets; there is no
  universal guessed frame pointer/visibility offset in the observer.
- **The existing native name lookup is not read-only.** GlueParent's name is
  passed to lookup `0x76c760` from `0x46ac3a`. Lookup obtains the shared Lua
  state, pushes a name through `0x6f3890` → `0x6f3840`, then accesses globals,
  extracts the object and checks its type. The push path writes a value and
  advances the Lua stack at `0x6f387b`–`0x6f3885`. Calling it would violate the
  no-client-memory-writes boundary. At `0x46ac1d`–`0x46ac44`, `0xCF0C10` is
  lazily filled from an increment of `0xCEEF6C` and passed as a lookup/type
  validation argument; these values are not qualified interpreter generations.
- **Loading candidate is not a complete loading discriminator.** Research
  followed LoadingScreen.cpp references and the query `0x407e70`, used by Glue
  update at `0x46c1c1`. It only tests whether handle `0x882BE0` is nonzero.
  Setup at `0x406800` and cleanup at `0x407e80` manage that handle; `0x4083c0`
  separately changes a flag on it. Handle existence alone has not been proven
  equivalent to a currently visible loading screen or an exclusive lifecycle
  phase. No loading address or classifier is added to production.

`tools/connection_client_audit.py` now verifies 14 exact additional binary
signature regions and the relevant visibility registration names **offline**.
These anchor the research paths above; they are not runtime read addresses or
permission to call any function. Against the exact local client it reports:

```text
sourceSignatures=PASS
glueResearchSignatures=PASS
liveGlueQualified=False
runtimeQualified=False
reconnectImplemented=False
```

Repeat with `python3 tools/connection_client_audit.py '/home/ludvig/Games/WoW Vanilla/WoW.exe'`.
Extracted Blizzard assets, disassembly, and research dependencies remain ignored;
no MPQ/client installation files are modified or committed.

### RUNTIME OBSERVED — inherited qualification only

R0.1b.1 observed `lastGlueScreen=charselect` while actually in world, server
connection remaining yes across manager_missing/active_guid_missing, and the
same player GUID returning with new manager/local-player addresses. Its exact
observations and RUNTIME PASS are retained below. No new WoW run or live Glue
memory sampling was performed for R0.1b.2; none of the binary findings above is
promoted into new RUNTIME PASS evidence.

### INFERRED — rejected promotion paths

A pointer equal before and after a multi-field read could still overlap
initialization/teardown or address reuse. Stable bytes alone do not prove that
the current Lua globals and native frames belong to a live Glue generation.
The proven reset/publication order makes this a relevant lifetime risk, not
evidence that a torn read occurred during the qualified R0.1b.1 session.
The lazy lookup token appears to be a type identifier; no generation semantics
are assigned to it. Loading-related handle/state candidates remain research
leads, not production facts. Absence of a candidate signal is not proof of
connected, disconnected, hidden Glue, or completed loading.

### UNKNOWN / SOURCE GAP — runtime decision

No generation-safe read-only binding from the current interpreter to named
Glue frames and globals was established. A native callback's verified layout
does not supply this missing binding. A future reader needs exact table/object
identity, initialization/teardown and mutation protection, and field-specific
visibility semantics without invoking Lua or modifying its stack. A separate
source-qualified loading lifecycle predicate is also still missing.

Accordingly `glueVisibility`, `currentGlueScreen`, `pendingGlueScreen`,
`dialogVisible`, `dialogType`, `dialogState`, `loading`, `glueGeneration`,
`disconnectConfirmed` and `actionEligibility` all remain explicitly `unknown`.
The observer now prints separate `dialogVisible=unknown dialogType=unknown`
and `liveGlueReason=source_gap_identity_and_lifetime`; its read set is unchanged.
There are no new production offsets, frame scans, native calls, hooks, Lua
execution, gameplay controllers, client-memory writes or actions. Existing
world/server/historical-screen evidence, sparse logging and stop handling are
preserved. No credentials, character selection, reconnect, retry/backoff or
restart implementation exists. R0.1b.2 is not claimed complete as a live reader.

### R0.1b.2 tests and validation

Tests extend the existing evidence matrix across world validity/stages, signature
validity, known/unknown server predicate and historical names. All live fields
must remain unknown, including during the observed loading stage or when the
server predicate is false. Unchanged samples remain suppressed; unknown samples
replace prior success. Existing native-reader fixtures retain missing/changed
owner, connection, screen and signature failures. New offline-audit tests reject
changed, truncated and missing research signatures and assert all live/runtime/
reconnect qualification flags stay false. A transitive local-include boundary
test checks the entire observer dependency closure for action adapters/input/
Lua/dispatcher APIs, in addition to the existing pre-controller entry guards.

Focused tests and exact-client offline audit: **PASS**.
`python3 tools/validate.py --jobs 4`: **PASS**, 105 C++ tests; failures=[];
42 audit Python tests, 13 QuestDB Python tests, SQL and all 10 Lua fixtures pass.
Report: `/tmp/wow-validation-bo7otml5/results.json`.
`cmake --build build`: **PASS**, including the separate requested build after
validation. `git diff --check`: **PASS**. `git status --short` reviewed: six
modified project files, no generated research artifacts staged and no commit.
Runtime of the added log fields: **PENDING**. These checks do not close the
live Glue/UI source gate or upgrade any reconnect qualification.

### Exact manual runtime procedure — normal lifecycle only

Use a fresh GUI/client in the existing configured Wine prefix:

```sh
env WOW_INTERNAL_CONNECTION_MODE=observe wine ./build/wow_gui.exe
```

Use **Start WoW** to retain the injection handle and inherit the environment;
manually enter the intended character using the ordinary client UI, then
**Start Bot**. Require `CONNECTION OBSERVE CONFIG mode=observe`; retain complete
`build/wow-internal.log` and `build/wow-internal.lifecycle.log` with the DLL
session identity and manual timestamps of each visible phase.

1. **Normal in-world:** record `worldSnapshot=valid`, `worldStage=complete`,
   player GUID, manager, localPlayer and independently read serverConnected.
   Historical charselect is allowed. Require all live fields listed above to
   be unknown, including the two separate dialog fields and the source-gap
   reason. Verify stable samples stop logging while the GUI heartbeat advances.
2. **Normal logout to character select:** manually logout. Record visible UI
   time, world-unavailable stage(s), server predicate and historical name.
   `manager_missing` must not imply disconnect, current charselect visibility,
   or safe action eligibility. Require `inputOwner=none commands=none`.
3. **Normal Enter World/loading:** manually choose the same character and click
   Enter World. Record the visible loading interval and sampled world stages.
   `active_guid_missing` must not set `loading=yes` or confirm disconnect;
   loading and live Glue fields remain unknown. A transition between polls may
   be missed; do not invent evidence for it.
4. **Same-character world return:** require a fresh valid snapshot with the
   original GUID, record the new manager/localPlayer pointers, and verify no
   gameplay owner starts. This remains world recovery, not reconnect success.

Throughout, check for no GRIND, MOVEMENT INTENT, CombatController, AFK PROTECTION,
VENDOR, Death14 or QuestPolicy activity and no bot-generated input, Lua or UI
action. Finally press **Stop Bot** and require the observer's session-stop,
runtime-detached and DLL-unload markers while WoW remains running. These checks
qualify the action-free observer/log extension only; they cannot qualify a live
disconnect dialog, a loading classifier, Glue lifetime or safe login actions.
Do not deliberately disconnect the network or provide credentials to this tool.

**SOURCE GAP — RECONNECT NOT IMPLEMENTED**

## R0.1b.1 read-only lifecycle observer (2026-10-09)

Starting restored HEAD: `a329e0bfead01999c7d7938081dc2b1c4e8ab477`, branch
`codex/r01b-connection-observer`, initially clean worktree. This section records
the qualified observer checkpoint; the earlier sections below retain their historical
validation and runtime status.

User-reported restored baseline: Arch Linux, Wine Staging 11.19, MinGW i686,
CMake/Ninja build PASS; validator PASS with 104 C++ tests. GUI retained-handle
injection, ObjectManager/LocalPlayer reads, and Stop Bot → detach → DLL unload
with WoW remaining open are RUNTIME PASS for that baseline. The client remains
WoW 1.12.1 build 5875, SHA256
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
The reported offline connection audit is `sourceSignatures=PASS`,
`runtimeQualified=False`, `reconnectImplemented=False`. These baseline results
alone do not qualify the observer; the manual lifecycle evidence is recorded below.

### Source and ownership boundary

Set `WOW_INTERNAL_CONNECTION_MODE=observe` in the **WoW process environment**.
Bootstrap still verifies client identity, then bypasses `WaitForWorld` and the
one-time ObjectManager probe. `WorldMonitor::Run` branches immediately, before
even the NavMesh session lifetime, and returns after the separate observer.
No Combat, Grind, navigation, DeathRecovery, AFK, vendor, quest, recovery or
other gameplay/input controller is constructed or started on that branch.
The unset variable takes the existing normal path without changes. Empty or
unsupported configured values fail closed and unload; observe takes precedence
over the water diagnostic and other workload settings. A missing GUI control
channel also blocks/unloads, preserving an explicit Stop Bot path.

`ConnectionLifecycleObserver5875` reuses `ConnectionEvidence5875::Observe`
with bounded `ReadProcessMemory` reads and the existing read-only
`WorldStateReader::Read`. No new offsets or signature assumptions were added.
It polls every 250 ms, maintains GUI heartbeat even without a world snapshot,
and reports the GUI active workload as Unknown. GUI IPC and diagnostic files
are the only writes; no client memory write, hook, Lua execution, native action,
login, EnterWorld, dialog click, movement, attack or AFK input is dispatched.
Stop/unload uses its own teardown with no attack-stop or movement-hold call.
It emits `BOT SESSION STOP`, marks detached, and returns to bootstrap unload.

`CONNECTION OBSERVE` sample events appear in the normal log and lifecycle
journal on the first sample and changes only. Their comparison excludes
timestamps, heartbeat, position, health and surrounding units. Fields include:

- `worldSnapshot` and `worldStage`, independent of the connection predicate.
  Manager, player GUID and local-player pointer are logged only for a successful
  current world snapshot; failed snapshots report those values as `unknown`.
  Identity/pointer changes are observable but authorize no resume or action.
- `sourceVerified`, `serverConnected=yes|no|unknown`, and allowlisted
  `lastGlueScreen` with explicit `glueScreenSemantics=historical`.
  Signature/read failure replaces prior success immediately; independent
  historical screen evidence may survive an unavailable server-predicate read.
- `glueVisibility`, `dialogState`, `loading`, `currentGlueScreen`,
  `pendingGlueScreen`, `glueGeneration`, `disconnectConfirmed` and
  `actionEligibility` remain explicitly `unknown`. Even a known false server
  predicate does not identify a disconnect cause or a visible dialog.
- `decision=observe_only inputOwner=none commands=none`, source reason,
  process-alive evidence and sample/session timing.

ObjectManager loss alone cannot become confirmed disconnect. A historical
`charselect` or `login` name may coexist with a valid in-world snapshot and
`serverConnected=yes`. Samples are sequential reads, not an atomic world/glue
generation; short transitions can occur between polls. Unchanged samples are
suppressed indefinitely while the GUI heartbeat continues. No successful sample
is retained as a substitute for unknown evidence.

### R0.1b.1 validation

Focused C++ tests exercise mode parsing, unchanged-sample suppression, independent
world/server/historical-screen evidence, return and identity changes, and failed
reads replacing known values. The native-reader fixtures cover missing/mismatched
signatures, changing owner/connection/screen reads, missing buffers, null and
overflowing owners, relocated images and unrecognized strings. Python source
ownership guards pin the immediate monitor return, bootstrap world-wait bypass,
and observer dependency/action boundary. These guards are static evidence,
not live proof that no input occurs.

`python3 tools/validate.py --jobs 4`: **PASS**, 105 C++ tests, failures=[];
39 audit Python tests, 13 QuestDB Python tests, SQL and all 10 Lua fixtures pass.
Report: `/tmp/wow-validation-qweotz3t/results.json`.
`cmake --build build`: **PASS** (MinGW DLL build in validation, followed by
the separately requested build). `git diff --check`: **PASS**.
After recording the manual qualification, the documentation-only update passed
the same full validator (105 C++ tests; failures=[]), separate
`cmake --build build` and `git diff --check`. Latest report:
`/tmp/wow-validation-mqp8_k78/results.json`. `git status --short` was reviewed;
only this audit was edited in that update, preserving the prior uncommitted
observer implementation/test files. No commit was made.
No WoW/GUI was launched during implementation. Subsequent user-performed runtime
qualification is recorded below. Safe reconnect prerequisites remain unproven.

### R0.1b.1 observed manual runtime qualification

**R0.1b.1 READ-ONLY CONNECTION LIFECYCLE OBSERVER — RUNTIME PASS**

Scope: normal world → character select → loading → same-world lifecycle only,
including the observer's cooperative Stop Bot/detach/unload path. Evidence is
the user's supplied observations from the completed manual session; no raw
capture file, timestamps or session identifier were supplied for independent
log inspection. No implementation defect is exposed by these observations,
and no implementation change is made for this qualification.

| Manually observed phase | World evidence | Independent connection evidence | Ownership evidence |
| --- | --- | --- | --- |
| Initial in-world | `worldSnapshot=valid`, `worldStage=complete`, `manager=0x08C1E008`, `playerGuid=0x000000000001AA56`, `localPlayer=0x143D0008` | `sourceVerified=yes`, `serverConnected=yes`, `lastGlueScreen=charselect`, `glueScreenSemantics=historical` | `mode=connection_observe`, `inputOwner=none`, `commands=none`, `decision=observe_only` |
| Normal logout to character select | `worldSnapshot=unavailable`, `worldStage=manager_missing` | `serverConnected=yes`, `lastGlueScreen=charselect`, `disconnectConfirmed=unknown` | `inputOwner=none`, `commands=none` |
| Normal Enter World/loading transition | `worldSnapshot=unavailable`, `worldStage=active_guid_missing` | `serverConnected=yes`, `disconnectConfirmed=unknown` | `inputOwner=none`, `commands=none` |
| Returned to same character/world | `worldSnapshot=valid`, `worldStage=complete`, `manager=0x08565A08`, `playerGuid=0x000000000001AA56`, `localPlayer=0x16CF0008` | `sourceVerified=yes`, `serverConnected=yes` | `inputOwner=none`, `commands=none` |

The initial sample explicitly reported `glueVisibility=unknown`,
`dialogState=unknown`, `loading=unknown`, `currentGlueScreen=unknown`,
`pendingGlueScreen=unknown`, `glueGeneration=unknown`,
`disconnectConfirmed=unknown` and `actionEligibility=unknown`.
The loading phase in the table is the user's visible lifecycle observation,
not a reader classification of `active_guid_missing` as loading.

The player GUID stayed identical across logout/loading/world return.
ObjectManager changed `0x08C1E008` → `0x08565A08`; LocalPlayer changed
`0x143D0008` → `0x16CF0008`. ObjectManager loss was not classified as disconnect,
and historical `lastGlueScreen` was not treated as current visibility. This
qualifies passive lifecycle observation, not automatic reconnect or gameplay
owner reconciliation/resume.

The user reports that grep for `GRIND`, `MOVEMENT INTENT`, `CombatController`,
`AFK PROTECTION`, `VENDOR`, `Death14` and `QuestPolicy` produced no output in
this observer session. Together with `inputOwner=none`, `commands=none` and
the source ownership boundary, this supports the read-only qualification;
absence of matching log lines alone is not independent proof of no input.

Shutdown evidence, in reported order:

1. GUI `stop_button` observed.
2. `BOT SESSION STOP mode=connection_observe reason=stop_or_unload inputOwner=none commands=none`.
3. `RUNTIME DETACHED` observed.
4. DLL `unload_requested` observed.
5. `wow_gui.exe` and `WoW.exe` remained running; `wow_loader.exe` was no longer running.

Only this normal read-only lifecycle is RUNTIME PASS. Live disconnect-dialog
visibility, loading classification, glue visibility/current or pending screen
generation, safe login/action eligibility and reconnect remain unqualified.
`tools/connection_client_audit.py` remains unchanged with
`runtimeQualified=False` and `reconnectImplemented=False`. No forced network
disconnect is part of this qualification. The supplied observations do not
separately quantify unchanged-sample suppression or heartbeat cadence; their
existing source/test evidence is not upgraded into a new runtime claim.

**SOURCE GAP — RECONNECT NOT IMPLEMENTED**

### Manual runtime qualification procedure (retained for repeat runs)

Keep complete `build/wow-internal.log` and `build/wow-internal.lifecycle.log`
from the same DLL session, plus manual timestamps of the visible UI steps.
Use the existing configured Wine prefix and a fresh GUI/client launched with:

```sh
env WOW_INTERNAL_CONNECTION_MODE=observe wine ./build/wow_gui.exe
```

Use **Start WoW** so the retained handle and environment reach the client.
Setting the variable only on a new loader cannot change an already-running
WoW process's environment. Manually log in and enter the intended character,
then press **Start Bot**. Check `CONNECTION OBSERVE CONFIG mode=observe` before
continuing. No diagnostic marker means this run is not qualified as observe mode.

1. **In-world:** record the initial valid snapshot, manager, player GUID and
   local-player pointer. Expect `sourceVerified=yes` and a readable server
   predicate; record the actual historical name without treating it as visible
   UI. Wait briefly to verify unchanged samples stop while heartbeat advances.
2. **Normal logout to character select:** manually use WoW's normal logout.
   Record the visible screen time and any world-unavailable/stage transitions.
   Server connection is sampled independently and may remain `yes` at character
   select. `lastGlueScreen` is historical; all unproven fields must remain
   `unknown`. A gap must never become a confirmed disconnect.
3. **Normal Enter World/loading:** manually select the same character and click
   Enter World. Record the visible loading interval and emitted changes. The
   observer must neither click nor send input and must keep `loading=unknown`;
   the human observation does not promote this reader into a loading detector.
   A short transition missed between polls is unobserved, not proof it was absent.
4. **Return to the same character/world:** require a new valid world sample and
   compare the GUID with step 1. Record the current manager/local-player pointers
   (they may differ), server predicate and historical name. No gameplay owner
   may start on return. This is observed world recovery, never reconnect success.

Finally press **Stop Bot**: require `BOT SESSION STOP mode=connection_observe`,
`RUNTIME DETACHED` and DLL unload while WoW stays running, with no stop/hold/input
command. Fail qualification if any gameplay controller, Lua, input dispatch or
automatic UI action occurs, or if unknown evidence becomes invented UI/loading
proof. Unknown source/read evidence must be investigated and recorded as an
unqualified read, not accepted as disconnected. Absence of a log alone cannot
prove no input; correlate the ownership source checks and visible behavior.
Do not force a network disconnect. Any naturally encountered dialog remains
manually observed and unqualified by this reader. To resume normal gameplay
later, restart the GUI/client with the variable **unset**, not empty.

**SOURCE GAP — RECONNECT NOT IMPLEMENTED**

## Historical R0.1 baseline

Starting HEAD: `069fe3dc0e859ec0c42539c608c4107ba9fb0458`.
No WoW, GUI, Wine runtime, login action, or credential access was performed.
R0.1 overall is **INCOMPLETE / SOURCE GAP**. This checkpoint addresses the
source-proven automatic full-bag wait and AFK-owner misclassification only.
Automatic reconnect and comprehensive world-boundary reconciliation are not
implemented or qualified. Do not start new gameplay phases on this basis.

## R0.1b complete incident reconstruction (2026-10-09)

R0.1b starting HEAD `6b4407f9c3b1109f55052d72f1243f9205b28525`.
The formerly missing capture is now inspected locally:
`runtime-captures/r01-incident-2026-10-08.log`, 86,702 lines, SHA256
`49096acc37faa2083c4d204fab32b07f87d23046d1c65b02134c747951a5cad3`.
It is not staged. The offline audit consumes the entire file; vendor trips,
AFK changes, world-gap boundaries and recovery were traced back to source.

**Correction to the excerpt-only R0.1 reconstruction:** there were TWO vendor
trips. The 22:07 AFK threshold occurred on a food trip, not on the later
full-bag trip. First-trip failure was return navigation, not MerchantFrame.
Times below are UTC. A range denotes surrounding logged timestamps, never a
fabricated exact time for an untimestamped line. Everything in this timeline
is OBSERVED unless explicitly qualified.

| Log lines / UTC | Observation |
| --- | --- |
| 1–166 / 22:02:02 | 1.12.1.5875 x86; player `0x1AA56`; manager 174610440; bags 13/16, free 3. |
| 10377–10393 / 22:04:02–22:05:02 | Food-only maintenance starts; bagPressure=no. |
| 11318–11381 / 22:05:02–22:06:02 | Wuark, entry 3167, interactions 3.331 then 0.431 yd; MerchantFrame OPEN. |
| 11417 onward | No usable food at this merchant; bounded alternate-service search. Other candidates' frame attempts do not open. |
| 13749–13764 / 22:07:02–03 | inputAge=300051; vendor blocks AFK. One native movement sample is 0x0. AFK candidate becomes set, then client/server flags converge active without a command. |
| 15356–15381 / 22:08:02–22:09:02 | First trip fails returning home (`surface_recovery_exhausted`); Grinding resumes. AFK input subsequently blocked by UI `frame_limit`. |
| 65502 / 22:18:03–22:19:03 | Bags 16/16, free 0. |
| 74322 / 22:20:03–22:21:04 | Last progress counters: 23 kills, lootOk=15; level 10. HP recovery completes before next maintenance. |
| 75992–76008 / 22:21:04–22:22:04 | Full-bag + food trip starts; remembered Wuark hub, intent 38, 57 polys / 447.107 yd, additive cached topology. |
| 77225–77256 / 22:22:04–22:23:04 | Intent 39 releases NavMesh at 15.424 yd; direct approach; Wuark interaction 1/5 at 2.961 yd. |
| 77312–77461 / around 22:23:04 | Interactions 2/5–5/5 all at 0.498 yd. No MerchantFrame-open event on this trip. |
| 77622–77654 / 22:23:04–22:24:04 | MerchantFrame failure; retryAfterTick=4825; CTM hold dispatched on game thread 1392; WaitingForManualVendor, fresh freeSlots=0. |
| 79979–79987 / 22:32:05 | inputAge=1800329; movement_or_transport_flags; no legacy AFK actions; no verified production input clock advance. |
| 80059 / before 22:32:23 | Last logged WorldState: alive 251/251, target zero, same stationary coordinates. First missing sample reports previous successful read only 245 ms old. |
| 80071 / 22:32:23 | manager_missing, process alive, clientState unknown. Controller updates suspend; cache and AFK evidence invalidate. |
| 86020 / next day 05:37:17 | World returns after **25,494,129 ms (7h04m54.129s)**. Same GUID, manager now 171553288; classification remains snapshot_recovered_cause_unknown. |
| 86021–86125 | Robustness timer resets; AFK input clock is recent (271 ms). WaitingForManualVendor persists. Fresh bags report free 7, 14, then 21. |
| 86504–86676 | Free bag capacity reaches 32/34 but wait persists. No bot reconnect/resume event. |
| 86699 / 05:39:48 | Second manager_missing gap. |
| 86702 / 05:39:49 | GUI observes processAlive=no, separately classified process exit of unknown cause. |

Complete audit: two vendor failures, one full-bag block, zero maintenance
retries/recoveries, 129 AFK defer events, one threshold crossing, zero verified
AFK clock-advance actions, two world gaps, one passive world return, one process
exit. Zero *logged authoritative* disconnects/reconnect attempts/successes is
NOT proof no disconnect occurred. End counters include runtimeRecoveries=8,
runtimeStrategic=2, runtimeEscalations=3, runtimeIdleDeadlocks=0,
movementRecoveries=0. Earlier excerpts did not contain all these counters.

### Vendor attribution

OBSERVED: the failed full-bag trip used Wuark/3167, who had already opened
MerchantFrame on the earlier trip. All five distances satisfy the existing
4.5 yd interaction bound. The final four distances are identical; NPC movement
is not independently measured. Source requires the chosen GUID in the live
snapshot and resolves its object before dispatch. The success log is emitted
only after dispatcher completion/IsGameThread; surrounding hook logs identify
thread 1392. Thus dispatch is supported, not a successful client/server gossip
transaction. Current code resolves the pointer before the callback; freshness
at the exact native call is not independently logged.

The interaction log omits GUID and native UI selection. WorldState target=0
does not disprove correct object-specific OnRightClickUnit dispatch. The
post-return selected GUID `0xF130000C5F001C79` is OBSERVED only AFTER world
return; assigning it to the failed earlier calls would be INFERRED. No stale
GUID, facing defect, gossip state, competing movement writer during the wait,
or native failure return is proven. MerchantOpen collapses readback failure
and closed frame; it cannot exclude transient opens between polls. There is
insufficient evidence to choose distance, selection, gossip, facing, or frame
observation as the underlying cause. No selling/interaction behavior changed.

### AFK attribution

SOURCE VERIFIED: movement+0x40 is compared with mask 0x100 for stationary
living safety (0x13f for qualified benign land work). Unsupported bits are
`raw & ~allowedMask`; swimming has an earlier distinct block. This reader has
no transport GUID. `playerFlags` / `afkRawFlags` are NOT movement flags.

OBSERVED: the ONLY raw movement word in the entire capture is 0x0 at line
13753, when vendor was the blocker. The later movement_or_transport_flags
reason does not log its operand. SOURCE NOT VERIFIED: exact later bits,
transport identity, stale versus genuine motion, sitting/falling/turning, or
a bad reader. Stationary coordinates do not prove zero movement flags.
Neither mask nor reader is changed. R0.1's sparse raw/allowed/unsupported
diagnostics are preserved to resolve this on the next natural occurrence.
The earlier frame_limit is an additional OBSERVED UI guard failure, not a
reason to bypass the qualified input guard.

AFK -> missing manager is chronological association, not a proven server
disconnect cause. The complete capture has no login/glue/dialog signal.

## Original R0.1 excerpt-only evidence (superseded above)

User-reported 5875 x86 run: 23 kills, 15 loots, one DeathRecovery, level 10.
At 22:07:02 input age reached 300051 ms with vendor deferral. MerchantFrame
failed to open and the full-bag guard entered WaitingForManualVendor.
At 22:32:05 input age was 1800329 ms with zero reported legacy anti-AFK
requests/actions and a movement_or_transport_flags block. About 18 seconds
later ObjectManager was missing while the process remained alive. At
05:37:17 a world snapshot returned, without evidence of bot-driven reconnect.
Bags had free capacity, but maintenance waiting persisted. Process exit was
observed separately at the end.

At that checkpoint the complete incident log was not available; these were
user excerpts, not a complete reconstruction. R0.1b above corrects the trip
ordering and supplies available merchant/distance evidence. The later raw
movement/transport bits remain absent even in the now-complete capture.

## Source paths

- VendorController::IssueInteraction resolves the selected GUID to an object,
  calls the existing right-click native path through GameThreadDispatcher,
  checks IsGameThread, and records entry/distance/attempt. Callers require
  <=4.5 yd; retries are eight ticks apart, at most five. The logged terminal
  branch means MerchantOpen stayed false through the bounded wait, not why.
  The native function is checked executable here; this audit does not upgrade
  that to new signature qualification. MerchantFrame verification is distinct
  from interaction dispatch or successful sale.
- GrindModeController's early WaitingForManualVendor return ignored
  maintenanceSuppressedUntil_. Its release predicate required all maintenance
  needs to be satisfied even when the hold was caused only by full bags.
  Thus a retry timestamp was logged but never consumed by that owner.
- WorldMonitor classified both Vendoring and WaitingForManualVendor as
  afkSafety.vendor, while healthyIdle excluded the wait. This explicitly
  prevented the proven production AFK pulse in a stopped maintenance owner.
- AfkClient5875::StationarySafetyReason reads movement+0x40. For stationary
  living input, AfkWorkloadSafetyPolicy admits only 0x100 (walk preference);
  any other bit yields movement_or_transport_flags. Normal benign-work
  context separately permits 0x13f. The wait is NOT benign moving work.
  No raw flags were supplied, so actual movement versus stale/unsupported
  bits remains unresolved. No mask is widened here. transportGuid remains
  explicitly unknown; no guessed offset is introduced.
- WorldMonitor suspends updates on invalid snapshots, invalidates topology on
  the first gap, invalidates terminal alive proof and resets SharedAfk. On
  recovery it only resets the robustness timer. This does not establish
  intended-character identity or reconcile all stale owners.

## Implemented maintenance subset

Automatic failed/full-bag trips retain their block and use a separate bounded
episode: at most two additional vendor starts, using existing retry cooldowns.
Each start retains VendorController's existing internal limits and sale policy.
After exhaustion (or explicit automation disable in the dirty worktree), the
typed MaintenanceBlocked substate observes fresh bag space; it does not spin
or acquire passive targets. Two distinct scheduled reads with freeSlots >1
release the automatic bag lock. Unknown, stale, full, one-free-slot, and
single transient observations cannot release it. Food/repair requests then
return to the ordinary maintenance scheduler instead of owning a bag lock.
An explicit pre-existing manual-mode food/repair wait is not converted to
automatic vendoring. Native direct-aggressor ownership preempts the wait/trip;
DeathRecovery still preempts Grind in WorldMonitor.

No new merchant-ID data or broad unpublished service-hub changes are included.
Alternate **verified** merchant qualification is still open: published HEAD's
service discovery can probe NPCs with undecoded npcFlags, whereas dirty source
has a different unpublished service catalogue. This checkpoint does not
publish those prerequisites or broaden discovery merely to retry more NPCs.

## Implemented AFK subset

Only active Vendoring blocks by transaction ownership. A stopped maintenance
wait is an idle candidate; combat, death, recovery, navigation, UI, transport,
swimming, unknown movement, and native input guards remain authoritative.
Actual Grind navigation ownership is included in AFK safety. The existing
paired F12/native AFK implementation and 300000/240000/270000 ms thresholds
are unchanged. Dispatch alone does not reset the input clock.
Sparse AFK SAFETY QUALIFICATION events now preserve raw flags, allowed mask,
unsupported bits, life/owner gates and native reason. Passing this read-only
gate is not proof that the later UI/dispatch qualification passed.

## Connection SOURCE GAP / R0.2 separation

### R0.1b local source research and diagnostic subset

The previous strings-only investigation is superseded by PE registration,
callback disassembly and local MPQ reads. Binary SHA256:
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
PE32 i386 image base `0x400000`. Addresses below are VAs, not file offsets.

| Candidate | Source / semantics / qualification |
| --- | --- |
| IsConnectedToServer | SOURCE VERIFIED registration pair `0x8374a0`, name `0x8377b0`, callback `0x46d380`. Calls getter `0x5ab490` (loads singleton from `0xC28128`), tests DWORD `[singleton+0x1b00]`; nonzero pushes Lua number 1, zero pushes nil. This is a server-connection predicate, NOT world/character identity or proof of a disconnect dialog. CharacterSelect.lua uses it to display SERVER_DOWN. |
| Last glue screen | SOURCE VERIFIED SetCurrentScreen registration `0x8373d8` -> `0x46ce60`; string argument -> `0x46b860` -> bounded 64-byte copy to `0xB41478`. GlueParent.lua invokes it after showing its named screen. The storage has no proven freshness/visibility token: **historical name only**, possibly stale in-world/loading. |
| Login/character-select UI | SOURCE VERIFIED local GlueParent.lua maps `login` to AccountLogin, `charselect` to CharacterSelect; CURRENT_GLUE_SCREEN begins nil; pending screen may differ during fade. Live frame visibility/context reader is SOURCE NOT VERIFIED. Empty/unrecognized native buffer remains unknown. |
| Disconnect dialog | SOURCE VERIFIED GlueParent_OnEvent(DISCONNECTED_FROM_SERVER) sets login and calls GlueDialog_Show(DISCONNECTED). GlueDialog.which stores type; only visible dialog + correct type proves current dialog. Native `0x46cc74` installs event slot 2; NetDisconnectHandler `0x46c540` reaches event emission `0x46c629` on its display branch. The NOT-displaying branch exists: absence of a dialog is not connected proof. No live dialog adapter yet. |
| Login submit | SOURCE VERIFIED native registration DefaultServerLogin `0x837480` -> `0x46d160`; two string arguments, delegates to `0x46afb0`. Local AccountLogin_Login uses edit-box text and clears the password. No proof of credential-free session resumption. No call made. |
| Enter World | SOURCE VERIFIED registration `0x8374a8` -> `0x46d3c0` -> `0x46b500`. Checks selected index against character count, uses 288-byte records; requires server connection. Local CharacterSelect_EnterWorld calls it. GetCharacterInfo exposes name/race/class/level/zone/etc., not a proven realm+GUID identity contract for this adapter. No call made. |
| Loading / interpreter lifetime | SOURCE NOT VERIFIED lifecycle discriminator. `0x704CD0` executes via `0x704AE0`, which obtains current Lua state through `0x7040D0` / `0xCEEF74`. Existing gameplay wrapper checks executable memory and window-thread dispatch, not glue generation/initialization or teardown. A nonnull interpreter or historical screen cannot safely qualify action eligibility. |

Local assets were read from MPQs, not substituted with modern APIs. A temporary
upstream [mpyq 0.2.5 parser](https://github.com/eagleflo/mpyq) in ignored build/
read archive bytes without attaching to WoW. `7z` did not support these MPQs.
Patch-2 overrides AccountLogin.lua; other three files below were absent there
and read from patch.MPQ. Both interface.MPQ and patch.MPQ were checked for
GlueParent/GlueDialog, so the older base copy was not mistaken for latest data.
No game files were modified or copyrighted assets committed.

- patch.MPQ GlueParent.lua: `78e8976a42de4ca5015555031abb54c48dd6f08eb424d42ddbacce732b9e6ccd`
- patch.MPQ GlueDialog.lua: `f5bcc69ac824030b1e05cd2bda7dd1197603b8a45cac3e8046038d7d1248c8b9`
- patch.MPQ CharacterSelect.lua: `972bbcb0072750e3827a0b5e725b1ad46891c916fdb4bb88cf56400da69960bc`
- patch-2.MPQ AccountLogin.lua: `d7824475393e7b0e21ff19477a83c0f35d0b9849b59571fbdc0b0bbacafe921e`

Implemented `ConnectionEvidence5875` observes ONLY the first two native
predicates. It validates registration/callback/getter/copy signatures on each
sample; rejects relocated/unknown code, null/unreadable/overflowing owners,
changing owner/connection reads, unterminated/unrecognized screen strings.
It never caches successful reads. WorldMonitor uses fault-safe bounded
ReadProcessMemory in the existing diagnostic cadence (not every gameplay
tick). No client functions execute; no game thread or Lua state is required
for this read-only observation. Two reads reduce torn observations but do not
prove a frame generation or authorize actions. Screen names are allowlisted;
no arbitrary client memory or credential strings are logged. Output explicitly
keeps glueVisibility/dialogState unknown and decision=observe_only.

`python3 tools/connection_client_audit.py /path/to/WoW.exe` validates the exact
reader signatures and event/API names against this fingerprint. Offline
SOURCE VERIFIED is not live RUNTIME PASS. Nothing in this reader is consumed
by gameplay, AFK eligibility, vendor policy, or a reconnect state machine.

### R0.1b stop boundary: SOURCE GAP — RECONNECT NOT IMPLEMENTED

No GlueActionAdapter, reconnect controller, credentials transport, backoff,
EnterWorld command, or passive-world identity/reinitialization policy is
published here. A dummy state machine fed by guessed booleans would not close
this gap. The existing OM-only diagnostics remain world-gap observations.
The incident's passive return is not labelled reconnectSuccess.

Before actions are enabled, establish ALL of:

1. A generation-safe, read-only live glue snapshot (visibility + typed dialog
   + current/pending screen + connection predicate) with interpreter/window
   lifetime guarded across login/loading/world teardown; prove loading cannot
   be mistaken for a disconnect. Ordinary window dispatch alone is insufficient.
2. Guarded same-thread command preconditions and completion for exactly the
   visible DISCONNECTED dialog, credential submission, character selection and
   EnterWorld. GlueDialog_OnClick(1) has different behavior for other dialog
   types; StatusDialogClick is NOT a generic reconnect button.
3. Intended realm/character binding before EnterWorld and fresh same-GUID world
   verification afterward. A cached character list/selected index is insufficient.
4. Controller-specific no-stale-input suspension/reset APIs: combat identity,
   nav intent/generation, DeathRecovery proof, vendor transaction, fresh bags,
   AFK pending evidence, requested-mode preservation. Passive world return needs
   this contract independently of automatic reconnect.

Next manual qualification should observe (not manufacture dangerous gameplay)
normal login, character select, entering-world/loading, connected state, and a
disconnect dialog if naturally encountered, with sparse native evidence and a
source-qualified glue observer. The current native diagnostic alone is NOT
enough to prove the complete UI sequence. Complete source/context research
before introducing that observer; do not request credentials for this tool.
AccountLogin_OnShow clears password text (patch-2 lines 38–40); login clears it
again (95–98). A remembered account name is not authentication. Future missing
credentials must yield AuthRequired; secrets may only be user-supplied in GUI
memory or an OS-backed credential provider, never logs/XML/plaintext config.

R0.2 remains separate: an external ClientProcessSupervisor would need a
configured executable, bounded launch/window detection, then qualified login,
attach/inject, identity validation and requested-mode restoration. This
injected reader cannot relaunch its own dead process. No launcher is added.

### Earlier R0.1 source-gap assessment (historical)

Existing DisconnectDiagnosticPolicy observes Healthy/SnapshotLost/StillUnavailable/
Recovered only. manager_missing is not confirmed disconnect. Static strings
in the local 5875 PE include GlueParent, DefaultServerLogin, EnterWorld,
GetCharacterInfo, GetNumCharacters, SET_GLUE_SCREEN and DISCONNECTED_FROM_SERVER.
Their presence does NOT validate a callable adapter, storage lifetime, Lua
context, character identity, callback ABI, or safe command eligibility.
No login primitive is called on this evidence. Preserved 1.12.1 UI archive
inspection found FrameXML/AddOns but no GlueXML in that archive.

Required next source gate: validate the Glue lifecycle reader and command
dispatcher on the exact client, including loading versus disconnect, intended
character selection and fresh-world confirmation. Only then wire bounded
reconnect/backoff and AuthRequired (when credentials unavailable). Credentials
must remain outside logs/configs; no credential persistence is implemented.
Process exit is a separate external supervisor observation; R0.2 would need
an explicitly authorized launcher/restart design. An injected DLL cannot
restart a process after it itself has exited.

World-gap reconciliation still requires a dedicated owner-boundary design:
same character and fresh manager proof; stale combat/navigation/vendor cleanup;
DeathRecovery preservation; fresh bags; AFK reset; requested-mode restoration.
The two-read bag wait fixes bag-lock release when its owner is safe, not the
entire connection-resume contract. Every missing world snapshot invalidates
maintenance/bag evidence and partial space proof, forcing new reads on return.

## Validation and runtime acceptance

Final source validation: TEST PASS / BUILD PASS / DIFF CHECK PASS for the
implemented subset, NOT all of R0.1. Full dirty worktree: 103 C++ tests,
30 audit Python tests, 13 QuestDB Python tests, SQL PASS, 10 Lua fixtures PASS.
Isolated exact staged tree: 54 C++ tests, 30 audit Python tests, no tracked
QuestDB Python suite, SQL PASS, 5 Lua fixtures PASS. Vendor metadata is 9/9
in both trees. The difference is unrelated, unstaged test suites.
Full report: build/r01-temp/wow-validation-r81l9fo6/results.json.
Isolated report: build/r01-isolated.l6N7uX/Projects/wow-internal-5875/build/
validation-temp/wow-validation-vjttb3zj/results.json.
The new offline runtime_reliability_audit.py distinguishes world recovery from
verified reconnect, counts explicit events rather than repeated snapshots, and
does not echo raw lines or credentials. Chronology is never labelled causal
proof. Existing legacy antiAfkActions counters are not substituted for verified
production input-clock events.

Runtime remains PENDING. Manual user testing should preserve a complete log,
observe ordinary vendor failure/cooldown or external bag-space resolution,
verify two fresh reads and resumed work, and verify a stationary maintenance
idle receives the existing safe input pulse with native clock advance.
Do not deliberately disconnect, die, enter water, or expose credentials to
manufacture evidence. R0.1 reconnect runtime cannot be accepted until its
source gap and implementation are resolved.
