# P0.0 shared AFK protection audit

## P0.0.11 mixed-state source audit (current, 2026-10-07)

### Runtime evidence and scope

Latest directly inspected `build/wow-internal.log`, session
`1360.134358440699456960.106923713.436`, confirms the new Ghost path:
lines 25393-25406 record WATERWALKING=0x10000000, allowedMask=0x1000013f,
unsupportedBits=0, paired F12 dispatch, clock 107704936 -> 107898701,
same life/scene/recovery state, `result=pass`, then Ghost `qualified=yes` in
RoutingToCorpse. Input age was about 193743 ms, BEFORE Due/AFK threshold.
Ghost harmlessness/early qualification: RUNTIME PASS. The same capture's
later lines 27525-27555 and 28000-28024 prove TWO Ghost prevention pulses at
ages 240207 and 240016 ms: RoutingToCorpse clock107898701 ->108138936, then
Failed recovery clock108138936 ->108378979, paired release, life/scene/recovery
proof and clear flags. Ghost production prevention: RUNTIME PASS, including
Failed recovery without AFK starvation. This is not proof of Dead qualification,
dead-state native clear or successful corpse recovery. Line27667 separately
records DeathRecovery strategic_route_failed / path_validation_failed. The
client later exits/world disappears; exit cause is unknown, not attributed to AFK.

The same session confirms combat defer -> first-safe-gap resume at input ages
293685 and 253868 ms, both before 300000, with unchanged scene/UI, native clock
advance and confirmed alive prevention (lines 5731-5783, 12261-12313).
Normal Grinding, scheduler/defer/resume, recovery debt and planning coexistence
remain RUNTIME PASS, including the earlier four-cycle/debt capture below.
Qualification PASS remains user-reported; no new qualification run is claimed.
No natural mixed state appears in this newest capture; no WoW process was
available during this audit. New mixed-path diagnostics: RUNTIME PENDING.

### Reproducible binary audit

Client `/home/ludvig/Games/WoW Vanilla/WoW.exe`, image base 0x400000,
SHA256 `b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
Read-only `tools/afk_client_audit.py` now validates writer signatures,
decoded direct-reference inventory, six clear callers/arguments, incoming
object-update registration, changed-field callback dispatch and AFK mirror
gate. Reproduce with:

```sh
python3 tools/afk_client_audit.py '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x5eb830 --stop-address=0x5eb900 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x5ee990 --stop-address=0x5eea50 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
```

The decoded .text inventory contains FIVE direct absolute write instructions
to 0xB6E5CC in FOUR routines. This is not a proof that arbitrary indirect memory
aliases cannot write the address. All direct writers and relevant call paths:

| Routine / write sites | Values and cause | Synchronization semantics |
| --- | --- | --- |
| 0x4989C0 / 0x4989EB, 0x4989F7 | Active-player lookup 0x468550/0x468460; PLAYER_FLAGS AFK bit sets 1, otherwise zero (including no player). Direct caller 0x490974 in broader 0x4908C0 initialization. | Copies the descriptor state without an AFK send, but clears chat-processing global 0x8435FC and tail-calls 0x49D230. That routine consumes queued chat records, displays chat via 0x49A870, may invoke unit animation via 0x60BB30, frees/unlinks records. NOT a proven standalone reconciliation utility. |
| 0x5EB740 / 0x5EB7AE | Writes 1 only when local AFK was zero. Idle caller 0x482FBA and AFK chat caller 0x49F525. Null/empty text gets a localized default. | Mark/display path then sends normal AFK chat through 0x5AB630. Not an explicit server clear. |
| 0x5EB830 / 0x5EB885 | Writes 0 after local-active and CVar/argument guards. | Displays CLEARED_AFK and sends EMPTY AFK chat; server effect is toggle, not set-clear. |
| 0x5EE990 / 0x5EE9F2 | Writes current PLAYER_FLAGS & 2, hence 0 or 2, for the active player's GUID only. | Incoming descriptor-change callback via 0x5E2850; conditional mirror update, no AFK send or input timestamp write. |

Other direct references at 0x482FAD, 0x49F3C7, 0x49F4FC, 0x5EB768 and
0x5EC9FD are reads/comparisons (0x5EB836 is the native-clear entry read).
They are not additional clear or synchronization functions.

### Native clear: complete argument semantics

0x5EB830 has ONE stack argument and `ret 4` at 0x5EB8FB. The incoming ECX
player convention is supplied by movement callers; no branch uses it to
choose AFK semantics. Local AFK zero returns at 0x5EB840 BEFORE testing the
argument. Argument zero at 0x5EB846 reads autoClearAFK pointer 0xC4D68C,
integer field +0x28, and returns if disabled. ANY nonzero argument skips ONLY
that CVar test. It is a CVar-bypass parameter, not clear-vs-toggle, not a
server-only clear mode, and not a way to bypass the local-zero return.
Historical references below call it `force`; that name has this limited meaning.

Direct callers: 0x513D36 (jump), 0x514E23/0x514F0B/0x514FCA (movement),
0x49F3D6 (normal chat) all push 0. 0x49F553 (explicit empty AFK command when
locally active) pushes 1. The active branch displays the clear message, writes
local zero, builds CMSG_MESSAGECHAT 0x95 / type AFK 0x14 / language 0 / empty
string 0x882748, and sends via 0x5AB630 at 0x5EB8D0. There is no branch that
clears local-only without this send. No new call mode is enabled.

### Incoming server synchronization and limits

Local VMaNGOS source, read-only protocol evidence (not proof of the running
server's exact version): `Objects/Player.cpp:1734` ToggleAFK -> Object
SetFlag/RemoveFlag -> MarkForClientUpdate on value change -> Map
SendObjectUpdates -> BuildUpdateData/UpdateData::Send -> ObjectUpdate packet.
`Server/Packets/ObjectUpdate.cpp` chooses SMSG_UPDATE_OBJECT 0xA9 or compressed
0x1F6; constants are in `Server/Protocol/Opcodes_1_12_1.h`.

Client 0x465140 registers opcode 0xA9 to 0x4651A0 and compressed 0x1F6 to
0x4672F0; decompression calls 0x4651A0 at 0x4673B6. Values-update dispatch
0x465277 -> 0x465330 reads the update mask, snapshots callback fields through
0x465970, applies incoming values and calls 0x465570. Its byte comparison
0x4655BB skips callbacks when old/current values are equal; changed fields
invoke the registered callback at 0x4655DC. Player field registration
0x5E25D7/0x5E25E8 -> 0x468070 registers 0x5E2850 for four bytes at relative
offset 8 in the PLAYER block (full descriptor PLAYER_FLAGS=0xBE, byte 0x2F8).
0x5E2850 resolves the player by GUID and passes OLD flags to 0x5EE990.

Within 0x5EE990, 0x5EE9B8 XORs old/current flags; 0x5EE9BA tests 0xE
(AFK/DND/GM). Zero skips the mirror write. For the active player's GUID,
0x5EE9EF/0x5EE9F2 copies currentFlags & 2 into the local mirror. Therefore a
relevant descriptor CHANGE can reconcile either mixed state. An unchanged
AFK value, unrelated server update, ordinary input, or movement does NOT
guarantee a refresh. No reliable convergence latency is established. Neither
callback nor broader startup/reset is exposed as a new bot command; fabricating
an oldFlags argument to force this branch is not legitimate incoming evidence.

Server `Handlers/ChatHandler.cpp:611-628`: combat rejects AFK chat; empty AFK
ALWAYS toggles; non-empty AFK marks ON if clear and updates text if active.
There is no explicit clear payload. DND mutual exclusion also clears AFK
but changes DND and is not neutral reconciliation. Whole-source AFK writer
search found initialization/stat cleanup (`Player::InitStatsForLevel`, line
3385) and battleground entry clear (`BattleGroundMgr.cpp:1466`), not ordinary
movement, combat entry, resurrection, or key-input clears. These special
transitions are not AFK utilities. MasterPlayer chat-tag state is separate
from authoritative PLAYER_FLAGS. No server files are changed.

### Safe result: explicit residual edge case, not fixed

SOURCE VERIFIED: native clear in ClientOnlyActive can turn the clear server
ON. In ServerOnlyActive it returns and leaves server AFK ON. Nonzero bypass
does not solve either. Non-empty mark is not a neutral clear operation: it can
mark a clear server ON. Replaying callbacks with fabricated old flags or
calling broad startup/chat initialization is not an approved narrow command.
No deterministic active path satisfying this phase's complete safety gate
has been proven. This does not assert that no such path can exist.

`AfkAgreementPolicy` now explicitly classifies BothClear, BothActive,
ClientOnlyActive, ServerOnlyActive, Unknown (invalid evidence is Unknown).
BothClear prevention and BothActive qualified composite recovery are unchanged.
Mixed states retain the existing single paired-input/delivery path, then
read-only observation within the ORIGINAL total 3000-ms command verification
budget. This is a fail-closed budget, NOT a measured propagation timeout;
there is no new wait, extended timeout or convergence guarantee. If both flags
converge clear within that budget, confirmation is allowed. If they converge
active, the existing once-only native-clear/CVar/fresh-flags guards apply.
No extra toggle is sent while mixed. Persistent mixed state session-latches:

- `client_only_afk_no_safe_reconciliation`
- `server_only_afk_no_safe_reconciliation`

Safety can defer even the harmless input; terminal/death/command guards retain
their existing meaning. Both flags must be known and clear for clear success.
The shared read-only status includes agreement. Transition-based START,
OBSERVE and RESULT telemetry captures flags, input clock, life, movement,
owner/death state and terminal reason. Flag/clock-change probes are read-only;
sampled descriptor changes are NOT claimed as hooked incoming packets or
known manual-input provenance. Natural convergence after a latched fault is
logged as an observed state change, not an automatically resumed controller.

KNOWN RESIDUAL EDGE CASE: mixed-state recovery is fail-closed by design,
not fixed. Preserve captures if it occurs naturally; do NOT manufacture it.
If a single ordinary human-input comparison is later necessary, capture mixed
state BEFORE the user input and compare subsequent flags/clock using these
events. A user input invalidates unattended-run qualification and is never
credited to the bot. No automated experimental toggle, CVar/flag write or
packet construction is added.

TEST PASS: focused agreement and production regressions; full validation
`python3 tools/validate.py --jobs 4`: 83 strict C++ tests, 13 Python tests,
QuestDB SQL self-test, all seven Lua fixtures / 138 checks. Artifact:
`/tmp/wow-validation-je4i58rl/results.json`. BUILD PASS (validator and separate
`cmake --build build`); DIFF CHECK PASS. An intermediate run caught a lexical
quiescence sentinel colliding with the read-only probe expression; equivalent
condition ordering preserves the unchanged sentinel and quiescence behavior.
Final rerun has zero failures. Existing Ghost, debt, due bands, combat and
navigation/death tests pass.
New telemetry/mixed observation runtime gate remains RUNTIME PENDING.

AFK is sufficiently closed for normal autonomous prevention: alive Grinding,
early Ghost qualification and two Ghost prevention cycles PASS, residual
mixed/dead-only limitations explicit. Next bounded phase: DeathRecovery `missing_corpse_anchor` and
`strategic_route_failed`. That phase is NOT implemented in this checkpoint.

Validated code checkpoint `5c84d0fe8214baff4b9157d0e6d7ebd011f4495a`
(`afk: audit mixed-state synchronization and retain bounded safe failure`)
was pushed to `origin/codex/wow-internal-continuation`; independent remote
HEAD lookup matched local code HEAD. Seven intended AFK files only;
unrelated dirty work remains unpublished. This docs-only follow-up records
that result; its own commit is discoverable through scoped git history.

## P0.0.10 Ghost-only water-walk allowance (historical; Ghost runtime now passed)

SOURCE + RUNTIME VERIFIED correlation: newest `build/wow-internal.log`, session
`1892.134358429566633600.105563093.1520`, confirms normal alive prevention at
240008 ms (clock 105567275 -> 105807308, paired release, scene/UI unchanged,
clear flags). Natural Dead/WaitingForGhost armed early qualification at age
75103; Ghost/WaitingForGhost armed its separate request at 76138. Both correctly
waited for the release/transition command window. At Ghost/RoutingToCorpse,
line 6975 records flags=0x10000000, mask=0x0000013f, unsupported=0x10000000;
the defer age was 77179. Later ordinary turn/forward/back combinations also
had only that same unsupported bit. No Ghost pulse was dispatched by that code.

Read-only re-audit of local VMaNGOS source confirms MovementInfo.h defines
MOVEFLAG_WATERWALKING=0x10000000, Player::ApplyGhostForm calls
SetWaterWalking(true), and Unit::SetWaterWalkingReal adds/removes that bit.
This closes P0.0.9's raw-bit evidence gap; it is no longer a guessed flag.

SOURCE VERIFIED change: AllowedMovementMask(life,ordinaryLandMovement,
deadGhostPulse) adds only water-walk for Ghost + guarded dead/ghost F12 pulse.
The ordinary Ghost pulse mask becomes 0x1000013f. Alive and Dead retain
0x0000013f; stationary qualification retains 0x00000100. Generic/default land
checks cannot acquire this exception. Swimming, pitch, jump/fall, transport,
flying, levitation, root and unknown bits remain blocked. The adapter applies
the typed policy to a fresh native life read at both monitoring and dispatch.

Exact MOVEMENT BLOCK telemetry remains. Accepted command-time Ghost water-walk
evidence emits MOVEMENT ELIGIBLE once per issued pulse, not every tick, with
flags/mask/unsupported bits and reason=source_verified_ghost_water_walk. This
means movement was eligible, not that delivery or qualification succeeded.
The following release, same-life, clock, unchanged scene/UI and recovery-state
proofs are still required. Dead/Ghost qualifications remain independent and
early; only actual input-clock advancement governs scheduling. WaitingForGhost,
WaitingForAlive, release/reclaim windows, transactions, loading, held input,
bound F12 and all existing faults remain blocked. No corpse-route, anchor,
reclaim, budget, 240/270/300-second band or native-clear permission changed.
Mixed-state reconciliation stays OPEN; no dead-state native clear is enabled.

Normal Grinding, recovery-debt separation and navigation-planning coexistence
remain RUNTIME PASS from the four-cycle capture documented below. P0.0.9 early
scheduling is SOURCE VERIFIED / TEST PASS and now runtime-observed arming before
Due. New Ghost pulse and Dead/Ghost production are RUNTIME PENDING, not passed
by mask tests. No running WoW process was available during this checkpoint.

Next live gate: normal mode (`wine ./build/wow_gui.exe`), natural death only,
capture `AFK ` before Start Bot. Require MOVEMENT ELIGIBLE life=ghost
flags=0x10000000 unsupportedBits=0 -> QUALIFICATION phase=dispatch -> VERIFY
advanced=yes sceneUnchanged=yes recoveryStateUnchangedOrValid=yes result=pass
-> STATUS qualified=yes state=ghost, before AFK threshold crossing. Do not
intentionally kill the character. DeathRecovery failures remain separate work.

TEST PASS: full `python3 tools/validate.py --jobs 4`, 82 strict C++ tests,
13 Python tests plus QuestDB SQL self-test, seven Lua fixtures / 138 checks;
BUILD PASS; DIFF CHECK PASS. Artifact:
`/tmp/wow-validation-h7wgv7he/results.json`. The new regression covers Ghost
only/context scoping, Alive/Dead rejection, exact mask fields, every other
unsupported mode, early RoutingToCorpse eligibility, command windows,
same-life/clock/release/scene/recovery requirements, and adapter wiring.
Existing binding/UI, alive/debt, native-clear/mixed-state and navigation/death
tests remain passing. Scoped Git checkpoint is recorded in project state.

## P0.0.9 early life-state qualification and movement evidence (historical)

Newest inspected production log is `build/wow-internal.log`, session
`1676.134358382414341480.100842662.1712`, ending at GUI unload on 2026-10-07.
Normal Grinding prevention is RUNTIME PASS: four paired F12 pulses at native
input ages 240131/240217/240142/240245 ms; each has release delivery, unchanged
scene/UI, fresh clock advancement, both AFK flags clear and confirmed action.
Recovery-debt separation is directly RUNTIME PASS with debt 25/51/75/101.
Cycles 3/4 also prove navigation-planning coexistence (Roaming/AcquiringTarget,
navigationPlanning=yes). Qualification remains RUNTIME PASS per user report.
This supersedes P0.0.8's pending alive-production gate; historical failures below
remain failures of their earlier implementations.

Fifth cycle: due 240225 ms and overdue 270121 ms legitimately blocked by native
combat; loot/combat ownership continued. Natural death at 280643 ms entered
WaitingForGhost; ghost RoutingToCorpse at 282718 ms was then blocked by
`movement_or_transport_flags`. No dead/ghost pulse was dispatched. Both AFK
flags were active at 300229 ms, after which prevention-only qualification was
correctly forbidden. The exact rejected movement word is absent from this
capture. This is a dead/ghost gate RUNTIME FAIL, not a harmful-F12 experiment.

SOURCE VERIFIED scheduling gap: `AfkDeadGhostPolicy` tracked per-life evidence,
but its first pulse could only be selected by the production timer's ordinary
due gate. Natural life-state entry now requests an early qualification through
the same scheduler and safety/dispatch/verification path. Dead and Ghost each
need one successful pulse per session. Requests wait through genuine command,
UI/input, combat and transaction blocks, then become eligible on the first safe
update, independently of input age. A request does not set the timer's due bit;
only native input evidence changes future scheduling. No pulse while pending;
failure remains session-latched. Startup qualify mode and alive prevention are
unchanged. AFK-active/threshold-crossed death states still cannot qualify, and
no native-clear permission is added. The original 3000-ms deadline remains.

Verification now additionally requires the issued life state on the later
clock-verification snapshot. The existing game-thread scene/UI/release bracket
and synchronous DeathRecovery-state comparison remain. Telemetry distinguishes
`QUALIFICATION DUE/DEFER/RESUME`, action reason `first_safe_dead_gap` or
`first_safe_ghost_gap`, and verify purpose `life_state_qualification` versus
`prevention`. No death is induced; corpse routing is not changed.

### Ghost movement audit: exact live bits still required

The land masks are still 0x0000013f (ordinary movement) and 0x00000100
(stationary qualification). `AFK DEAD/GHOST MOVEMENT BLOCK` now records the
freshly read word, mask and exact unsupported bits, together with life and
DeathRecovery state, at blocker changes and command-time rejection. It does
not guess the old log's flags or grant unknown bits.

Read-only local VMaNGOS 1.12.1 sources: `Objects/MovementInfo.h:30-67` names
pitch 0x40/0x80, levitation 0x400, fixed-Z 0x800, root 0x1000, jumping 0x2000,
far falling 0x4000, swimming 0x00200000, spline 0x00400000, transport
0x02000000, water-walking 0x10000000, safe-fall 0x20000000 and hover 0x40000000.
These are distinct from forward/strafe/turn/walk flags. `Player.cpp:4561-4568`
ApplyGhostForm calls SetWaterWalking(true); BuildPlayerRepop uses that path,
sets HP=1 and unroots the player. `Unit.cpp:7224-7245` queues the water-walk
movement change for a player-controlled unit and sets/removes that bit.
Water-walking is therefore a plausible explanation for the ghost rejection,
but no current raw client word proves it. Do not expand the mask on that guess.
Unknown, swim, jump/fall, pitch and transport modes remain rejected. No WoW
process was running to obtain a fresh read during this checkpoint.

New implementation RUNTIME PENDING: normal mode, natural death only. Require
one early DEAD/GHOST qualification verify with advanced clock, unchanged
scene/UI/recovery state and same life, before threshold crossing. If movement
blocks, retain the exact MOVEMENT BLOCK event for the next targeted audit.
This is an explicit external-evidence blocker; it does not reopen already
passed alive Grinding gates. Mixed-state reconciliation remains OPEN.

Separate death-autonomy evidence: newest terminal event is
`strategic_route_failed`, navFailure=path_validation_failed, strategies exhausted
(line 22000). The earlier `missing_corpse_anchor` failure remains queued too;
neither is fixed by AFK qualification.

TEST PASS: new strict early-qualification regression and full validation,
81 C++ tests (C++20, Wall/extra/Werror), 13 Python tests plus QuestDB fixture,
seven Lua fixtures / 138 checks. BUILD PASS, including the explicit separate
build; DIFF CHECK PASS. Artifact: `/tmp/wow-validation-u3bdm23x/results.json`.
The read-only 5875 binary signature audit also passed. New dead/ghost runtime
verification and exact rejected bits remain pending.

## P0.0.8 recovery debt versus AFK input safety (2026-10-07)

SOURCE VERIFIED: newest normal Grinding log reached `AFK PRODUCTION DUE`
at input age 240595 ms, remained blocked by `fault_or_unknown_subsystem`
at 270183 ms and 300135 ms, then the client AFK flag activated. Nearby
world snapshots show Grind `Roaming`, Combat `AcquiringTarget` with no locked
GUID, DeathRecovery `Idle`, Vendor `Idle`, valid/alive player, and navmesh
expanded initialization/repeated movement recovery. `runtimeRecoveries=22`
in the periodic log is the cumulative `RecoveryEvents()` counter, **not**
the exact `RecoveriesWithoutProgress()` value. The source predicate is
exhaustive: Combat/Grind `Failed()` were false in those states, leaving
`RecoveriesWithoutProgress()>0` as the cause of `afkSafety.fault` in the
observed window. The counter increments on tactical recovery and clears on
earned progress; it does not report a keyboard or world-observation fault.

The shared WorldMonitor AFK gate now assesses only terminal Combat/Grind
failure as `fault`. It carries the recovery debt separately for sparse
`AFK SAFETY BLOCK` / `AFK SAFETY ELIGIBLE` diagnostics, with the actual controller states and
navigation-initialization flag. A terminal controller is labelled
`terminal_combat_fault` or `terminal_grind_fault`; an otherwise unknown fault
still fails closed. The production scheduler, qualified paired F12 transport,
fresh native clock and scene/UI verification, death/ghost qualification,
mixed-state no-toggle rule, 240/270/300-second bands and navigation recovery
semantics are unchanged. This is not a navigation fix.

TEST PASS: a strict deterministic regression first failed to compile before
the typed assessment existed, then passed. It covers recovery debt 0/1/22
with benign navigation, terminal failure classification, hard blockers,
due/overdue/threshold bands and the live WorldMonitor wiring. The full suite
and build results are recorded with this checkpoint in project state.
RUNTIME PASS for normal Grinding prevention/recovery-debt separation in the
newest capture: four confirmed pulses with debt 25/51/75/101, including two
with navigationPlanning=yes. See P0.0.9 above for exact evidence and the
separate dead/ghost failure. The earlier threshold-crossing run remains
historical RUNTIME FAIL; do not re-label it.

## P0.0.7 death/ghost prevention (historical checkpoint)

Newest production capture (`build/wow-internal.log`, 2026-10-07) confirms
P0.0.6 urgency/sticky deferral: lines 543-545 due/deferred at 240131 ms,
588 overdue at 270185, 633-637 threshold at 300018 followed by client-only
then both-active AFK. Blocker was `death_or_ghost`; no F12 was attempted.
The flag 0x12 and HP=1 show a ghost. This is NOT a failed ghost F12 experiment.
Lines 182-183 separately report `missing_corpse_anchor`; recovery stays Failed
and retains death ownership. Audit that autonomy blocker in the next bounded
death-recovery phase; this checkpoint does not restart or rewrite recovery.

SOURCE VERIFIED: re-audited local 5875 binary with the hash below. Dispatcher
0x765F10..0x765FC1 writes the clock at 0x765F34 before calling registered
consumers at vtable+0x60. Release 0x765FD0..0x76606D writes at 0x765FEC,
calls the key consumer at +0x64, then clears consumer bookkeeping. Neither
dispatcher tests player health, ghost flags, or a death-recovery state.
This establishes no death-specific branch in THIS dispatcher, not a proof
that every registered consumer/addon is harmless. Live unbound-F12 and visible
keyboard/UI guards plus the bounded scene test remain mandatory. The audit
tool signatures still pass. No new API, OS-global key, memory write, packet,
movement command, native clear permission, or timer threshold was introduced.

New AfkDeadGhostPolicy is prevention-only and session-local. At source-derived
due time, a natural dead/ghost state can try ONE paired candidate in a safe
command gap. Dead and ghost qualify independently; delivery, matching release,
scene/UI/target/facing/movement integrity, unchanged native life state and
unchanged recovery state must pass, then fresh native clock advancement and
both clear AFK flags must be observed within the existing 3000-ms deadline.
Failure disables attempts for the session; stop/world loss discards evidence.
The next eligible due pulse uses the same guards and verification, even after
qualification. No cross-tick held key exists. No artificial death is requested.

Command safety: WorldMonitor invokes AFK after DeathRecovery::Update and all
other owners. Recovery commands use synchronous GameThreadDispatcher::Invoke;
they have returned before this point. ReleasingSpirit, WaitingForGhost and
WaitingForAlive remain blocked (including asynchronous release/reclaim
confirmation). Idle, RoutingToCorpse, WaitingForReclaim and Failed can be
eligible, NOT automatically safe. Combat, independent recovery, transactions,
faults, held keys/buttons, visible dialogs, loading/invalid world and unsafe
movement flags still block. Ordinary ghost land movement uses the unchanged
land mask; swim/fall/transport remain blocked. HP=1 with ghost flag is not
misclassified as living low-health recovery. HP=1 without that flag is unknown.
Lua checks UnitIsDeadOrGhost again; all four StaticPopup frames are blocked.

The paired-message scene comparison is synchronous on the game thread.
Monitor recovery state is compared after dispatch, before its next update;
normal ghost route progress on subsequent ticks is not labelled a key effect.
Sparse `AFK DEAD/GHOST STATUS`, `QUALIFICATION`, `ACTION`, `VERIFY` include
pre-pulse clock/scene/recovery state and separate delivery verification.
P0.0.6 240000/270000/300000 bands and native-clock-only scheduling are unchanged.
Full stationary qualify mode stays separate and still rejects death.

AFK already active or threshold crossed while dead/ghost is explicitly blocked
as `dead_ghost_afk_recovery_not_qualified`. This checkpoint does NOT grant native
auto-clear in death states or solve mixed flags. Both-active alive recovery
and the documented mixed-state protocol blocker remain unchanged.

Status: previous qualification RUNTIME PASS (user-reported), prior normal
cycle 1 RUNTIME PASS, prior long-run RUNTIME FAIL. New death/ghost behavior
RUNTIME PENDING; normal two-cycle production gate remains pending. No WoW
process was running during this audit. Run normal mode (no qualify environment):

```fish
wine ./build/wow_gui.exe
# In another terminal, before Start Bot:
tail -n 0 -F build/wow-internal.log | rg --line-buffered 'AFK '
```

Do not intentionally die. Require two natural normal-work prevention cycles
without AFK. If natural death lasts until due, require DEAD/GHOST QUALIFICATION
-> VERIFY advanced=yes sceneUnchanged=yes recoveryStateUnchangedOrValid=yes
result=pass -> PRODUCTION VERIFY confirmed, before 300000 ms, both flags clear,
no manual input/held key/side effect, recovery continuing independently.
No death during the run means that gate stays pending, not failed or passed.

Validation: TEST PASS, 79 strict C++20/Wall/extra/Werror tests, 13 Python tests
plus QuestDB SQL fixture, seven Lua fixtures / 138 checks. BUILD PASS and
DIFF CHECK PASS. Final full artifact: `/tmp/wow-validation-loia88my/results.json`.
New focused test first failed before the policy existed. Existing qualification,
production, navigation and death-recovery regressions pass unchanged. Validation
uses the current dirty worktree, not a clean remote checkout or a live WoW run.

## P0.0.6 production status (historical checkpoint)

Qualification: **RUNTIME PASS, user-reported** in the P0.0.6 request; its full
two-window capture is not retained in the current log. Production Grinding
cycle 1: **RUNTIME PASS** in the inspected log, input 90470165 -> 90713889
(243724 ms), paired delivery, unchanged scene/UI, both flags clear. Production
long-run: **RUNTIME FAIL**. Cycle 2 was deferred; client AFK appeared at
300231 ms and the pulse was not issued until clock 91676232 versus 91169170
(507062 ms). Input delivery succeeded but client stayed AFK, server stayed clear.
`autoClearAFK=enabled` is now runtime-observed at log line 205.

SOURCE VERIFIED: production still used stationary-only AfkProtectionPolicy
and adapter guards, even though the workload classifier already recognized
benign work. The `benign_work_awaiting_runtime_verified_noop` message was an
unconditional relabel, not a persisted qualification lookup. Native auto-clear
was wired only into qualification; the production fallback rejected mixed
flags indefinitely. The configuration's `runtimeVerified=no` described neither
implementation qualification nor command evidence accurately.

Important limit on attribution: at cycle-2 due, log lines 27350–27385 show
locked-target Chasing/Combat ownership; threshold crossing at 30281 occurs
amid locked-target restoration. F12 resumes after Recovering -> AcquiringTarget
at 34548. This does NOT prove every deferral was benign, or justify interrupting
combat. A combat/recovery interval longer than the safety margin can still
cross the threshold. New telemetry explains the exact blocker/urgency band.

Changes (SOURCE VERIFIED, new runtime PENDING): shared AfkProductionPolicy
keeps Due/Deferred until an authoritative native clock change, uses 80% due,
90% overdue and the unchanged 100% source threshold. First known-safe work
opportunity resumes the pending action. No CTM/displacement resets this state.
Reviewed implementation qualification is distinct from session delivery and
per-action proof. Classified Grinding/Roaming/ApproachingTarget with no hard
combat/recovery/fault/transaction owner can use the qualified pulse. Native
guards allow only ordinary 1.12.1 land forward/back/strafe/turn/walk flags;
swim, fall, pitch, transport and unknown flags still block. First aid, manual
vendor and low-health recovery remain hard gates. Questing uses the same
policy but unclassified active Questing owners remain blocked, not guessed.

Scene proof brackets the synchronous paired messages on the game thread; it
does not compare a moving character with its position a tick later and call
normal navigation an input side effect. The next update must still prove the
native clock advanced. UI is rechecked inside the same paired-message bracket;
all held physical keys/buttons block dispatch. Combat/dialog beginning on the
next ordinary gameplay tick is not falsely attributed to the prior key pulse.
Qualification retains its stationary, across-update scene guard.
Production UI re-probe is bounded to one second (not an input interval);
all command verification remains within the original 3000-ms deadline.

Recovery now starts for **either** active authoritative flag, verifies F12,
then uses the native non-forced clear once when both flags are active. Both
must clear before recovery success. Mixed states enter bounded reconciliation,
not an inert retry loop; convergence can proceed to clear or confirm already
clear. Persistent disagreement ends in `mixed_afk_requires_safe_reconciliation`,
latched for the session. This is **NOT a complete mixed-state recovery fix**.

### Mixed-state native-clear blocker — do not remove the guard

Re-audited 0x5EB836–0x5EB840: client clear => native function returns without
sending. With client active/server clear, it sends an empty AFK message, and
the local VMaNGOS ChatHandler.cpp:624 toggles server AFK **on**, not off.
The server also ignores AFK messages during combat. Qualification of the
both-active case does not validate either asymmetric case. Removing the guard
would violate source evidence, not repair it. No flags are patched; no double
toggle or fabricated synchronization was introduced. Need a source-/runtime-
proven legitimate reconciliation operation (or evidence that the live server
has different semantics) before implementing the requested unconditional
OR-flags native dispatch. A comparison capture for the mixed state was
requested. This remains the active AFK blocker; do not advance to swimming.

The other local writer was also inspected: the player-flags update callback
at 0x5EE990 XORs incoming old flags against current flags and gates the mirror
write on changed bits 0xE (0x5EE9B8–0x5EE9C0). It is not an unconditional
reconcile API; calling it with invented old flags would fabricate an update.
VMaNGOS Player::ToggleAFK additionally leaves battlegrounds when toggled on, so
a speculative mark-then-clear sequence is not a harmless general substitute.

Next runtime gate: fresh normal process (`wine ./build/wow_gui.exe`, no qualify
environment), natural Grinding >=20 minutes, no manual input. Capture `AFK `.
Require two pulses before 300000, unchanged synchronous scene/UI, native clock
advance, clear flags, continued Grinding, and DEFER -> RESUME if combat occurs.
Both-active recovery is testable; persistent mixed flags must show bounded
failure, NOT a falsely claimed recovery. New production code is RUNTIME PENDING.

TEST PASS: 78 strict C++ tests, 13 Python tests plus QuestDB SQL fixture,
six Lua fixtures / 113 checks. BUILD PASS; DIFF CHECK PASS. Final artifact:
`/tmp/wow-validation-v9eov92z/results.json`. Regression coverage includes sticky
due/defer/resume, benign land movement versus hard gates, native-clock-only
progress, session versus implementation evidence, OR-flags recovery entry,
once-only both-active clear, bounded mixed-state failure/convergence, unknown
observations and shared workload wiring. Tests do not prove production runtime.

The sections below preserve historical evidence; their earlier pending status
and stationary-only descriptions are superseded by this section.

P0.0.4 (2026-10-07): SOURCE VERIFIED; composite candidate RUNTIME PENDING.
The newest live capture proves qualification hold/quiescence and the natural
300241-ms idle threshold. Targeted F12 delivery and native input-clock advance
are RUNTIME PASS. **F12 alone clearing AFK is RUNTIME FAIL**: both states stayed
active through the 3000-ms deadline (observed failure at 3245 ms). Prevention
is RUNTIME PENDING; no prevention window began. Do not label the bot protected.

## P0.0.4 clear-path control-flow audit

The capture's baseline was 60801599. After quiescence stableMs=1541, natural
AFK was observed, paired F12 was delivered, and the native clock advanced to
61101864 without scene changes. Both AFK states remained active. This is NOT
evidence that Wine dropped the input, and extending the timeout is not a fix.

The binary contains two separate mechanisms:

- Key-event dispatch at 0x765F10 copies event fields, then writes event+0xC
  to 0xCF0BC8 at 0x765F34, BEFORE invoking registered consumers through vtable
  +0x60. Release dispatch 0x765FD0 writes the same clock at 0x765FEC and calls
  the consumer through +0x64. Clock delivery does not imply a bound gameplay
  action, nor a call to the AFK-clear routine.
- Actual clearing is 0x5EB830, same player-this/one-stack-argument ABI as its
  movement callers, `ret 4`. It first tests local 0xB6E5CC and returns if clear.
  At 0x5EB846 it tests the force argument; with force=0 it reads the CVar
  pointer at 0xC4D68C and integer field +0x28, returning if zero. It obtains
  CLEARED_AFK text at 0x5EB86C, displays the normal client notice, clears its
  own local flag at 0x5EB885, constructs opcode 0x95 / chat type 0x14 /
  language 0 / empty text, and calls network send 0x5AB630 at 0x5EB8D0.
  That wrapper obtains the connection via 0x5AB490 and sends via 0x5379A0.
  This is client/server behavior, not just a local flag edit.
- Direct callers: movement at 0x513D36, 0x514E23, 0x514F0B, 0x514FCA pass
  force=0; normal chat at 0x49F3D6 also passes 0. Explicit empty AFK chat
  at 0x49F553 passes 1, bypassing the setting. Jump registration at 0x8500B8
  maps to 0x513BD0; that action's accepted movement branch contains the
  0x513D36 call. MoveForwardStart registration 0x8500D0 maps to 0x513E20.
  Thus real keys that invoke these actions can clear AFK. A **real unbound
  F12** is not proven to clear it either. No human-input trace or full Wine
  dispatch-stack capture was made; no claim that all hardware keys clear AFK.

`autoClearAFK` registration at 0x5E24CC–0x5E24F4 supplies default string `1`
(0x82E748), name 0x8602CC, description 0x8602DC, and stores the returned CVar
pointer at 0xC4D68C. The executable says “Automatically clear AFK when moving
or chatting”; the argument/value branches and packet construction corroborate
that description. Only registration and this clear routine directly reference
that pointer in the disassembly. No `autoClearAFK` override was found in the
local WTF/Config.wtf; that is NOT proof of the live setting. No WoW process was
running during this audit. **Actual runtime CVar remains unknown.**

New read-only `AFK AUTO CLEAR setting=enabled|disabled|unknown` samples this
signature-validated pointer/field. Unknown/non-boolean values fail closed.
No setting is changed. Disabled would block automatic semantic clearing, but
is NOT established as the cause of this run. The proven missing operation is
the semantic client/server clear after the successfully delivered unbound key.

The local VMaNGOS 1.12.1 opcode table confirms CMSG_MESSAGECHAT=149/0x95;
SharedDefines.h confirms CHAT_MSG_AFK=0x14. ChatHandler.cpp:611–628 ignores
AFK in combat and toggles for empty text. Consequently a native-clear dispatch
requires BOTH live flags active, never one mismatched state or already clear.

### One new qualification-only candidate

`paired_F12_then_native_auto_clear` is explicitly a **composite**, NOT evidence
that F12 alone clears AFK. Qualify mode delivers the paired unbound key, verifies
fresh clock advancement and unchanged scene/UI, then requests the audited
0x5EB830 function with force=0 on the game thread once, only when both flags
remain active and autoClearAFK is authoritatively enabled. Entry/gate/packet/
send/return and CVar-registration signatures are checked. The native function
displays its expected AFK-cleared chat notice; no dialog/gameplay movement is
requested. Native code performs its normal state update and server packet;
the bot does not write either flag, input clock or CVar.

The original 3000-ms deadline starts at F12 dispatch and is NOT restarted by
delivery or native clear. Mismatched flags wait boundedly; no second clear,
toggle or key is sent while awaiting confirmation. Both flags must clear, then
two prevention intervals must still pass. Unsafe/unknown/side-effect/failed
dispatch aborts and releases the hold. Initial clear failure is logged as
`AFK QUALIFICATION PREREQUISITE`, not a failed prevention window. Existing
production guards and the separate production policy are unchanged.

Native timestamp is reread immediately before the key pulse, after UI/window/
scene guards. Delivery logs separate `baselineClock`, `inputClockBefore` and
`inputClockAfter`; equality of baseline/before is expected during genuine idle,
not evidence of reusing an old sample.

Transport choice: reuse already-proven targeted message delivery plus the
audited client clear mechanism; do not replace it with SendInput, keybd_event,
X11 events or another function key merely to repeat an unbound event. Those
broader paths have no established additional semantic benefit and may affect
focus/global input. A controlled human comparison is only needed if the new
capture disproves this source-backed model. Production remains unqualified.

## Evidence and cause

The previous `ActiveBotAfkSafeguard` is a Grinding liveness mechanism. It resets
its inactivity counter after displacement and requests reacquisition/roaming.
It does not verify the client's input clock. Questing's idle path only emitted
`no_source_verified_stationary_action`. Thus neither path established actual
AFK prevention. The existing Grinding liveness behavior remains intact.

Local binary: `/home/ludvig/Games/WoW Vanilla/WoW.exe`, image base 0x400000,
SHA256 `b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
Reproduce the read-only audit with `python3 tools/afk_client_audit.py <client>`.
Addresses below are VAs, not offsets into an arbitrary PE file.

| Evidence | Location / interpretation |
|---|---|
| Idle check | 0x482EA0; reads input timestamp 0xCF0BC8, calls clock 0x42C010 |
| Threshold | 0x482ECD subtracts 0x493E0 (300000 ms); immediate at 0x482ECF |
| Input handler | 0x765F34 writes 0xCF0BC8; signature checked before enabling adapter |
| Local AFK | 0xB6E5CC, read by AFK-clear routine 0x5EB830 |
| Server synchronization | 0x5EE9EF masks flags with 2; 0x5EE9F2 writes that result to 0xB6E5CC |
| Mark AFK | 0x5EB740, called by idle check when eligible |
| Explicit clear | Empty AFK chat dispatcher 0x49F4F3–0x49F553 uses local flag to mark/clear |
| TurnLeftStart/Stop | 0x513EE0/0x513F10 read the input timestamp; issuing movement is not proof of refreshing it |

The 300000-ms client threshold is source evidence, corroborated by the newest
controlled 300241-ms observation (not an exact independent server timeout).
Server policy, suppressed client checks, manual AFK and flags in transit can
produce different observations. The controller records local and server state
separately. The newest post-abort Grinding run observed both flags active at
input age 300158 ms, consistent with the source threshold, but logged
`continuousSafeIdle=no`. This is not a controlled qualification baseline or
prevention proof; the newer P0.0.4 baseline supersedes this earlier measurement.

P0.0.1 correction: B6E5CC is **not exclusively 0/1**. Explicit mark writes 1,
but server synchronization writes 0 or 2. The old `client>1` rejection made a
valid AFK update unknown. The adapter now signature-checks that writer and
accepts only source-proven 0/1/2; 1 and 2 mean active. Unknown encodings still
fail closed. New unknown-reason telemetry distinguishes bad signatures, reads,
flags and clock discontinuities. Legacy player-flag-only diagnostics remain
`signalVerified=no`, now labelled `AFK FLAG CANDIDATE SET/UNSET`.

The old log's abort precedes normal Grinding initialization; a direct attacker
at 2.1 yards is selected immediately afterward. The old monitor already had a
`continue` before workload dispatch. Thus ordinary workload preemption is NOT
proven; the exact old safety predicate was not logged. Do not suppress combat
to make the diagnostic pass. P0.0.1 makes qualification ownership explicit and
reports the actual blocking predicate for the next run.

Local VMaNGOS source under `../vmangos-core/src/game`:

- `Objects/UpdateFields_1_12_1.h`: PLAYER_FLAGS word 0xBE (byte 0x2F8).
- `Objects/Player.h`: PLAYER_FLAGS_AFK=2; ghost=0x10; IsAFK reads the flag.
- `Objects/UnitDefines.h`: UNIT_FLAG_IN_COMBAT=0x80000.
- `Objects/MovementInfo.h`: swimming=0x00200000, walk preference=0x100.
- `Handlers/ChatHandler.cpp`, CHAT_MSG_AFK: empty text toggles AFK, combat
  blocks it. Never send this packet/API blindly. Both local and server flags
  must be true immediately before a clear request.

Vanilla registration strings establish GetBindingAction, EnumerateFrames,
IsKeyboardEnabled, GetObjectType, GetScript, UnitAffectingCombat,
UnitIsDeadOrGhost and SendChatMessage. The guard does not assume modern
UnitIsAFK/GetCurrentKeyBoardFocus/HasFocus APIs. Unknown frame/API state blocks.

## Shared implementation

`WorldMonitor` owns one `SharedAfkController` for Questing and Grinding.
Ordinary scheduling runs after gameplay owners/watchdogs. Questing must report
valid no-action/no-owner/no-fault state; Grinding must be target-free in its
Grinding state, not approaching, roaming, vendoring or fault recovery.
Direct attackers, low health, combat lock/state, death, recovery, loot,
navigation and interactions block. The game-thread adapter rechecks health,
ghost/combat/movement flags and Lua UI guards immediately before dispatch.
Movement flags other than stationary walk preference are rejected, including
swimming, falling and transport. This is not a general water-state subsystem.

The policy uses **client input age**, not elapsed bot ticks or displacement.
At 80% of the source threshold, safe idle may issue a candidate pulse. Input
is not repeated while verification is pending. A failed dispatch/three-second
verification is session-latched, not retried indefinitely. UI guard probing
backs off ten seconds without sending a key. Stop/world gaps discard evidence.

`AfkQualificationHold` is diagnostic ownership with Requested/Held/Aborted/
Complete states, separate from production safe-input classification. Both
workloads' acquisition/updates are behind its monitor `continue`. Read-only
Lua guards run before acquiring the hold and every second during it; dispatch
always rechecks guards. Native safety and ownership are checked every snapshot.
UI reads do not send input or change the AFK timer. Abort is terminal for this
test session, releases the hold and leaves the controller observe-only; a fresh
Start Bot explicitly re-arms qualification. No Stop Bot is needed during a run.

Input candidate: unbound F12, using synchronous WM_KEYDOWN/WM_KEYUP on the
current process's visible WoW window, on its owning game thread. Window and
PID/thread are rediscovered/checked; no window-ID constant, foreground
activation, physical mouse movement, global held key or external input loop.
Visible EditBoxes, keyboard handlers, dialogs or an F12 binding block it.
The paired driver attempts release even after a failed/throwing press. No
held-input state survives a tick, stop, exception or client restart.

**Delivery and native clock are runtime verified; F12-only clear failed.**
Delivery is logged as
`paired_message_delivered`, not proof of hardware key state or AFK clearance.
Only a subsequently advanced native input timestamp confirms qualifying
activity. If it does not advance, stop with verification_timeout; do not
silently substitute movement or reset the clock in memory.

P0.0.2 compares freshly read position, facing, target and movement flags
throughout delivery AND AFK-clear verification, plus the read-only UI guard.
Unknown evidence, changed target/flags/UI, displacement above 0.01 yard or
angular change above 0.001 radian fails verification (shortest angle handles
wrap). These are numerical tolerances, not permission to move. This is
bounded observational evidence, not a claim about untested navigation input.

Ordinary roaming/acquisition is classified separately as BenignWork when
evidence supports it, but **production input permission is NOT relaxed yet**.
`benign_work_awaiting_runtime_verified_noop` is explicit. Even a completed idle
qualification requires review of the real capture, then implementation/testing
of navigation-safe production dispatch. Idle-only protection is not sufficient
for indefinite busy-workload AFK prevention; that requirement remains open.

Production retains its separate guarded empty-AFK-chat fallback after verified
input. Qualification does not use that forced Lua toggle: the new explicitly
named composite uses non-forced native auto-clear as described above. It must
never be reported as F12-alone success. Flags/timers are never written by the bot.

## P0.0.3 pre-baseline input quiescence

Latest log: hold acquired at inputClock=57169769, then first qualification
Update read 57169793 (24 ms later) and immediately selected Baseline. The
controller's next-update input-change guard aborted the run. Important trace
detail: those two logged values alone do NOT identify the input that caused
the abort. The first Update had no previous observation and tolerated that
change; a subsequent changed clock was not logged. Residual Start Bot/GUI input
is plausible, not proven. The verified defect is the missing quiet interval
before the strict evidence boundary.

`AfkQualificationPolicy::AwaitingQuiescence` now precedes both clear and active
entry. Central constants: QuietIntervalMs=1500, MaximumQuiescenceMs=10000.
These are diagnostic bounds, not changes to WoW's timer or measured propagation
latency. Each safe native-input timestamp change resets only the quiet interval;
the fixed startup deadline never resets. Reaching it fails with
`quiescence_not_reached`. This uses normal monotonic tick observations, no sleep
or input dispatch. Unsafe/unknown evidence still aborts immediately and releases
the existing hold; both workloads remain inhibited during quiescence.

Only after 1500 ms of unchanged clock does the policy capture baseline input
clock/client-server AFK state; the runtime captures the corresponding valid
scene. `AFK QUIESCENCE START/RESET/COMPLETE` and `AFK QUALIFICATION BASELINE`
are event-only diagnostics. From this boundary forward, uncommanded input
changes fail, including during the following already-AFK synchronization phase.
Candidate commands retain their own independent before/after clock and scene.
The adapter's fresher pre-command snapshot also cannot silently replace the
baseline clock with an unrelated input. Startup input earns no candidate,
clear or prevention credit. Normal production scheduling is unchanged.

## P0.0.2 entry and asynchronous verification (after quiescence)

`AfkQualificationPolicy` is a diagnostic-only state machine behind the existing
shared hold. After quiescence, an authoritative AFK-active start is not itself unsafe. A clear
start observes the natural baseline without input. An active start with input
age <= the existing 3000-ms verification bound observes fresh snapshots for at
most 1000 ms: naturally clear goes to baseline, still active goes to candidate
clear. An older active start goes directly to candidate clear. This short grace
is an engineering observation bound, **not a measured client propagation time**.

The three gates are separate: paired F12/native-clock delivery; authoritative
client AND server AFK clear; two continuously clear prevention intervals.
Delivery and clear share the existing 3000-ms deadline from dispatch, with no
new pulse/toggle while waiting and no timeout restart when the clock advances.
Mixed client/server flags can settle within this window. A clock change alone
never completes a clear or prevention gate. Unsafe state, unknown evidence,
side effects or timeout abort/release the hold; synchronous paired input leaves
no cross-tick key state. Normal work remains blocked while qualification holds.

Successful initial candidate clear arms a fresh baseline using the real input
timestamp, with **zero** completed prevention windows. Each later window still
needs safe/clear observations, age reaching the source safety margin, paired
input delivery and clear verification. AFK during a prevention window fails
instead of resetting and silently trying again. Unattributed input aborts after
quiescence establishes the baseline. Threshold measurement remains independent: an already
AFK entry cannot manufacture a measured timeout.
If AFK clears naturally before dispatch, observation returns to baseline. If
the adapter's fresher pre-command read catches that race only after dispatch,
the run fails rather than crediting F12 with a clear that preceded it.

`AfkRuntimeStatus` exposes a read-only snapshot with known/active state, input
age, source threshold, independently observed threshold, delivery/clear gates
and prevention-window count.
GUI transport/display is not yet integrated. No fabricated measured countdown.

## Controlled runtime gate (fish)

Run on safe stationary land, healthy and out of combat, with dialogs closed.
Ensure F12 is unbound. A clear entry lets natural AFK occur once to measure
the baseline; an already-AFK entry tests clearing first. Both require two later
prevention intervals. Do not manually
press keys or interact with the WoW window after starting the test.

Launch the GUI with the mode, then use **Start WoW**, log in on safe land and
**Start Bot**. The audited `CreateProcessW` call passes a null environment block,
so its newly created WoW process inherits the GUI environment:

```fish
cd ~/Programming/Projects/wow-internal-5875
env WOW_INTERNAL_AFK_MODE=qualify wine ./build/wow_gui.exe
```

An already-running WoW process does not inherit a newly launched GUI's mode.
Use the GUI's newly launched client; do not kill/restart a client automatically.

Capture in a third terminal (start before clicking Start):

```fish
cd ~/Programming/Projects/wow-internal-5875
set capture (mktemp -d /tmp/wow-afk-qualification.XXXXXX)
tail -n 0 -F build/wow-internal.log | tee "$capture/afk-live.log" | rg --line-buffered 'AFK |MOVEMENT INTENT|COMBAT|DEATH|WORLD RECONCILIATION'
```

Require `AFK QUALIFICATION HOLD state=acquired` before waiting. Qualification
suppresses voluntary work only while no conflicting gameplay owner exists.
Threat, movement, death or another owner aborts it and returns to normal work.
Unknown observations, verification failure, unattributed input after quiescence
or a bounded overall deadline also abort. No safe input candidate => BLOCKED,
not PASS. An already-AFK start is now supported, but is not itself a measured
baseline or a successful prevention window. Default `protect` mode does not intentionally wait for AFK;
`observe` mode never issues AFK input.

Required log sequence:

1. AFK QUALIFICATION START/HOLD acquired; AFK QUIESCENCE START, optional RESET,
   then COMPLETE and AFK QUALIFICATION BASELINE. No inputs issued in this phase.
   Clear => natural baseline, active => bounded synchronization/candidate_clear.
   For a clear baseline, inspect the natural AFK THRESHOLD OBSERVATION elapsed.
2. AFK ACTION unbound_F12; matching AFK INPUT RELEASE; native input clock
   advances (`AFK CANDIDATE DELIVERY advanced=yes`). Separately require
   `AFK ACTION action=native_auto_clear inputPath=game_thread_native_5875`,
   `AFK CLEAR VERIFY result=confirmed`, then `AFK QUALIFICATION BASELINE RESET
   reason=candidate_clear_confirmed`. No forced Lua AFK toggle.
3. Two complete safe/clear intervals, each ending near the safety margin with
   a verified paired pulse. AFK PREVENTION WINDOW 1 then 2. Missing observation,
   intervening AFK, unsafe ownership or early pulses cannot count as windows.
4. `AFK QUALIFICATION COMPLETE windows=2 result=pass`, hold released, normal workload resumes, no manual
   input, no displacement, stuck keys or interference. Inspect the complete
   capture, not just counters, before assigning RUNTIME PASS.

Repeat shared integration in both workload modes. If window lookup/guard/input
fails, retain the exact reason and full capture. A targeted Wine/X11 driver is
a later candidate only if this path demonstrably fails; no speculative W loop.

## Tests and limitations

`afk_protection_policy_test.cpp`: known/unknown flags, due threshold, all typed
ownership gates, pending/confirmed/timeout, flag mismatch, clock wrap, reset,
press/release failure/exception, and prevention-interval invalidation.
`afk_safe_input_fixture.lua`: real Lua 5.1 guard execution, APIs, binding,
dialogs, keyboard handlers, visible edit boxes and error fail-closed behavior.
`afk_qualification_hold_test.cpp`: ownership lifecycle, both workload gates,
abort/release, no automatic re-arm, unchanged input/state, raw client flag 2,
scene changes, production classification without permission, monitor wiring
and GUI environment inheritance.
`afk_qualification_policy_test.cpp`: clear/active/unknown entry, bounded startup
sync, natural clear, delayed two-flag clear, delivery != clear, shared deadline,
scene/UI changes, fresh baseline, mandatory first/second windows, prevention
AFK failure and wiring to the shared runtime. Its startup assertion failed
against the old controller before the fix.
`afk_quiescence_policy_test.cpp`: 24-ms startup change, quiet-interval reset,
fixed startup deadline, captured baseline evidence, strict post-baseline input,
already-AFK synchronization, independent candidate delivery, unsafe/unknown
abort, hold release and runtime wiring. The test failed compilation before
the new typed phase/API existed. P0.0.2 tests now run after the prerequisite
quiescence and retain the delivery/clear/two-window regressions.
These do not prove Windows/Wine event handling or client AFK prevention.

`afk_native_clear_test.cpp`: reproduces the rejected input-only candidate;
unknown/disabled CVar fails closed; native clear is requested only after input
proof and both-active flags; issued clear never counts as success; client-only,
server-only and neither-clear timeout without extending the deadline; no repeat
clear; unchanged scene, fresh pre-dispatch sample ordering/no direct flag/CVar
writes; initial success still requires two prevention intervals.

Native signature mismatch disables the adapter. All held-input guarantees
apply to the synchronous message design, not untested OS keyboard injection.
Continuous busy ownership intentionally defers AFK activity: it cannot be
reported protected until runtime shows eligible safe opportunities. Existing
navigation/death/pull safety and their budgets were not changed.
