# R0.1 unattended maintenance / connection audit

Starting HEAD: `069fe3dc0e859ec0c42539c608c4107ba9fb0458`.
No WoW, GUI, Wine runtime, login action, or credential access was performed.
R0.1 overall is **INCOMPLETE / SOURCE GAP**. This checkpoint addresses the
source-proven automatic full-bag wait and AFK-owner misclassification only.
Automatic reconnect and comprehensive world-boundary reconciliation are not
implemented or qualified. Do not start new gameplay phases on this basis.

## Incident evidence and limits

User-reported 5875 x86 run: 23 kills, 15 loots, one DeathRecovery, level 10.
At 22:07:02 input age reached 300051 ms with vendor deferral. MerchantFrame
failed to open and the full-bag guard entered WaitingForManualVendor.
At 22:32:05 input age was 1800329 ms with zero reported legacy anti-AFK
requests/actions and a movement_or_transport_flags block. About 18 seconds
later ObjectManager was missing while the process remained alive. At
05:37:17 a world snapshot returned, without evidence of bot-driven reconnect.
Bags had free capacity, but maintenance waiting persisted. Process exit was
observed separately at the end.

The incident log is not present in the inspected project captures; the local
build/wow-internal.log is an older capture. The selected NPC, exact live
interaction distance, raw movement flags, and transport identity cannot be
reconstructed from the supplied excerpts. AFK followed by world loss is a
chronological association, NOT proof of the disconnect cause.

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
