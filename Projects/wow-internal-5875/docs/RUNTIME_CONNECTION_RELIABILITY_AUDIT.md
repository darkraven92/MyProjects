# R0.1 unattended maintenance / connection audit

## R0.1b.1 read-only lifecycle observer (2026-10-09)

Starting restored HEAD: `a329e0bfead01999c7d7938081dc2b1c4e8ab477`, branch
`codex/r01b-connection-observer`, initially clean worktree. This section is
the current checkpoint; the earlier sections below retain their historical
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
