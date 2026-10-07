# P0.4 water / swimming / drowning source gate

## P0.4.1 observe-only live evidence checkpoint (2026-10-07)

Starting HEAD `9107b2c93d661218c5b8902c42a9e16d63e9ab36` on
`codex/wow-internal-continuation`. This checkpoint adds a read-only
`WaterEvidence5875` adapter and a separate `WOW_INTERNAL_WATER_MODE=observe`
monitor loop. It branches **before** `CombatController::Start`, quest/grind
updates or any gameplay owner. The loop only samples the existing validated
world snapshot and signature-matched native movement word. It never targets,
moves, invokes Jump, installs a packet/UI hook or runs Lua. Stop/unload writes
only the ordinary runtime-control status; it issues no movement-stop command.
The default mode is unchanged. An unrecognized nonempty water mode fails
closed without starting gameplay.

Current live observation capacity is deliberately narrow:

| Signal | Static status | Live adapter status |
| --- | --- | --- |
| swimming | SOURCE VERIFIED, client `0x60E0D4` player+`0x118`, movement+`0x40` mask `0x00200000`; runtime signature checked | IMPLEMENTED read-only; RUNTIME PENDING |
| breath timer current/max/scale/pause, fatigue | SOURCE VERIFIED **packet/event** layout only | SOURCE NOT VERIFIED as a fresh live reader; all fields unknown |
| submerged, surface, waterline/liquid type | SOURCE NOT VERIFIED coherent player evidence | unknown; not inferred from swimming/NavMesh |
| positive ground contact / water exit | SOURCE NOT VERIFIED | `DryOrNonSwimming` only, never `DryGround` |
| ascent start and matching release | SOURCE VERIFIED paired Jump **binding callbacks**, but swim-specific semantics SOURCE NOT VERIFIED | no command path implemented or invoked |

Mirror packet `0x1D9` START at `0x5E7A25..0x5E7A9A` reads type/current/max/
signed scale/paused/spell and forwards `MIRROR_TIMER_START` Lua event. `0x1DA`
PAUSE reads type/paused and forwards event `0x16B`; `0x1DB` STOP reads type
and forwards `0x16C`. `0x5E7B10` is **not** a timer getter: it resolves a spell
icon from a global spell table for the START event's spell argument. No
signature-validated native per-player timer store or polled Lua getter was
found. The preserved FrameXML MirrorTimer frames interpolate received events,
can be hidden/reset on world entry and are not verified as a fresh bootstrap
snapshot in the running client. A read-only parser (`MirrorTimerProtocol5875`)
decodes offline event bytes for tests only; no live packet interception exists.
Server-side scale `-1` drains underwater and `+10` refills at the surface, but
an event may be stale and breathing effects can suppress the timer. Therefore
neither active breath nor an old draining event is a current submersion proof.

Binary audit rechecked exact Jump registration pair at `0x8500B8`:
`Jump` start `0x513BD0` calls `0x60DEA0` -> `0x617930` queued event 7;
`Jump` stop `0x513D50` calls `0x60E080` -> `0x60E060` -> `0x617DE0`,
which queues event 0xE or 0xF depending on player+`0x9E8` bit `0x100`.
Those are source-backed callback/call/queue relationships, **not** a proven
hold-to-ascend / stop-to-release swimming contract. Start/stop remain
SOURCE NOT VERIFIED for *safe sustained ascent*, and no function is called.
No verified current-player liquid surface Z or positive ground-contact source
was found; swimming=false could mean falling, shallow water or transport.

The typed observer reports `Unknown`, `DryOrNonSwimming`, or
`SwimmingStateUnknown`; it requires two consecutive known movement samples
to promote a swim/non-swim transition and returns to Unknown immediately on
world/signature/read loss. This confirmation is diagnostic hysteresis, not a
new game-state claim. `WATER EVIDENCE` logs changes or every 30 seconds, with
raw flags and all unsupported fields explicitly `unknown`. The audit tool
separates source-verified, runtime-observed and inferred counts, and cannot
turn a mere swim-bit observation into surface/submerged/drowning PASS.

Latest existing full log was checked with `tools/water_log_audit.py`: zero water
events/encounters, final movement/runtime/idle/escalation/death counters all
zero. No WoW process was available for a new safe manual shoreline run, so
all **new** water signals remain RUNTIME PENDING. To qualify the observer,
launch a fresh GUI/client with `env WOW_INTERNAL_WATER_MODE=observe wine
./build/wow_gui.exe`, Start WoW, log in, then Start Bot; use only USER-MANUAL
movement at a safe shallow shoreline, then Stop Bot. Inspect `WATER OBSERVE`
and `WATER EVIDENCE` in the preserved full log. The bot supplies no movement,
no target, no input and no drowning test. A known swim transition would verify
the adapter at runtime, **not** surface, submersion, breath or ground exit.

P0.4.2 autonomous water behavior is not started; the source gaps are a safety
gate. The existing AFK, DeathRecovery, combat, navigation/cache and 2000/4/2
budgets are untouched.

Validation of this bounded checkpoint: TEST PASS, full dirty worktree 92
strict C++ tests, 26 audit Python tests, 13 QuestDB Python tests, SQL fixture
and eight Lua fixtures; results
`/tmp/wow-validation-0vostnxd/results.json`. BUILD PASS with both validator
and explicit `cmake --build build`; DIFF CHECK PASS. The staged-only exported
tree independently passed 43 published C++ tests, the same 26 audit Python
tests, SQL fixture, three published Lua fixtures and full MinGW DLL/GUI/loader
build; results `/tmp/wow-validation-vhvuu2a7/results.json`, tree
`/tmp/wow-p041-isolated.eZwvle`. The 13 local QuestDB Python tests belong to
unrelated unpublished work, so they are absent in that isolated tree; SQL
remains present and passed. Neither build constitutes a live water observation.
Follow-up log-audit correction: draining/refilling counts additionally require
`breathActive=yes`, so an inactive timer's stale scale is not live direction
evidence. Final post-correction TEST PASS: full 92 C++/26 audit Python/13
QuestDB Python/SQL/eight Lua, `/tmp/wow-validation-oz2svn1s/results.json`;
isolated 43 C++/26 audit Python/SQL/three Lua and complete MinGW build,
`/tmp/wow-validation-2ixqiaz9/results.json` in
`/tmp/wow-p041-final-isolated.ZmhzAw`. Explicit build and diff checks PASS.

2026-10-07. **Incomplete implementation checkpoint: SOURCE GAP.** The user's
sections31/48 stop gate applies. No production water movement, state reader,
owner, input, filter or watchdog change is enabled. Drowning prevention is NOT
implemented or qualified. Do not treat this audit as completion of P0.4.

## Verified baseline

Starting HEAD exactly `4ff143940b0f3e409742e00ab0e0840e4117b572`, branch
`codex/wow-internal-continuation`. Latest full log preserved before work:
`/tmp/wow-p04-baseline.yBnhXp/wow-internal.log`,137775lines, session
2000.134358700080773630.132596093.1776. Bot132596606->134880875ms:
38.07115minutes, user-requested stop. This is a longer capture than the initial
55-kill user summary: final kills67/lootOk67/deaths0. Final movementRecoveries0,
runtimeRecoveries0/runtimeStrategic0/runtimeIdleDeadlocks0/runtimeEscalations0.
Seven confirmed AFK actions. No new natural death gate occurred; preserve the
independent previously verified DeathRecovery/Ghost baseline.

P0.3 Navigation RUNTIME PASS; P0.3.1 Acquisition liveness RUNTIME PASS;
P0.3.2 NavMesh cache RUNTIME PASS in this capture. Cold route30, expanded60,
full614 actual additions=704;5085 cache hits total. Four warm full-map loads:
276.827/293.499/264.276/264.845ms,704hits each, zero reads/additions. Warm route
67 requests252.304–279.752ms; expanded4 requests253.081–253.503ms. Only session
teardown invalidated generation1. These are real client timings, not the older
offline probe. No water telemetry exists; absence is not a water encounter PASS.

## Signal-by-signal audit

Client SHA256 `b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
Addresses below are VAs in this exact PE32 image, base400000. Reproduce static
instruction/string checks without attaching to WoW:

```sh
python3 tools/water_client_audit.py '/home/ludvig/Games/WoW Vanilla/WoW.exe'
```

Local VMaNGOS files referenced below are READ-ONLY protocol/generator evidence,
not proof of the running server revision or client memory layout. Root:
`/home/ludvig/Programming/Projects/vmangos-core`.

| Candidate | Status / exact evidence | Encoding, limits and read-only use |
| --- | --- | --- |
| A swimming | SOURCE VERIFIED flag consumer: client60E0D4 loads player+118,60E0DA tests movement+40 mask00200000. Existing PlayerSnapshot reads +118; AFK reads +40. Server Objects/MovementInfo.h names the identical mask SWIMMING. | One movement mode, NOT head-under-water. A future reader must validate current identity/signature/read/freshness. No new runtime reader yet. |
| B submerged | SOURCE NOT VERIFIED as a coherent current-player snapshot. Server IsUnderwater is private environmental state, not a discovered replicated field. | Never infer from low Z, swimming alone or absent UI. Unknown remains unknown. |
| C surface / near surface | SOURCE NOT VERIFIED. No audited client waterline/head-height relation. | NOT swimming-without-breath by default; timer can be absent through breathing effects or missing events/UI. |
| D liquid surface Z | SOURCE NOT VERIFIED client field/function. Generator reads map liquid heights, but no live water-height adapter/version correlation. | NavMesh polygon Z cannot substitute, see topology proof below. |
| E current liquid type | SOURCE NOT VERIFIED live player signal. Mesh water/magma/slime flags SOURCE VERIFIED for topology only. | A polygon classification is not authoritative current-player liquid contact. |
| F breath active | SOURCE VERIFIED protocol/event, NOT a validated live snapshot. Client5E7990 handles1D9/1DA/1DB; type1 maps BREATH at5E7AF7. | Start/stop/paused events; active can include refilling at surface. Missing cached event is unknown, not inactive. |
| G breath current/max | SOURCE VERIFIED event payload:5E7A25..5E7A65 reads type,current,max,scale,paused byte,spell;5E7A75..5E7A9A forwards event16A. | Milliseconds from Server/Packets/Misc.h/.cpp. UI uses seconds. No native persistent timer address verified; UI live fields not runtime-validated. |
| H fatigue | SOURCE VERIFIED timer type0 maps EXHAUSTION at5E7AFD, same payload. | Distinct from breath; no ocean/fatigue traversal permission. No live snapshot yet. |
| I drowning damage | SOURCE VERIFIED server protocol: opcode1FC, Player.h DAMAGE_DROWNING=1, EXHAUSTED=0, FALL=2; Player.cpp expiration emits environmental damage. Client adapter SOURCE NOT VERIFIED. | HP loss is not damage-type evidence; no inferred drowning event generated. |
| J swim movement flags | SOURCE VERIFIED00200000 above; existing ghost10000000 evidence retained. | Pitch40/80, jump2000, far-fall4000, transport02000000 are separate server movement bits. Do not promote ambiguous server flying comments to verified client flight semantics. |
| K vertical input | Jump registration8500B8 ->513BD0 ->60DEA0 ->617930 queues event7 through617570. SOURCE VERIFIED call chain only. Safe sustained ascent AND deterministic release semantics SOURCE NOT VERIFIED. | Jump alone is NOT a proven bounded surface action. No native call or Space pulse added. |
| L dry/ground contact | SOURCE NOT VERIFIED positive contact signal. | Swimming=false does not prove grounded: falling/transport/shallow-liquid are counterexamples. No DryGround assertion from absence of swim bit. |

Unknown/stale representation for all unvalidated observations remains unknown;
no synthetic surfaceZ, breath maximum, current map or current timestamp is added.
The offline audit's addresses are NOT runtime function registrations or permission
to call them. No client-memory writes are performed by these tools.

## Breath semantics and the remaining snapshot gate

Disassembled5E7990's switch handles start1D9, pause1DA, stop1DB. Start uses only
packet locals before firing Lua event16A through703F50; this routine does not
publish an independently polled breath cache. Event-name table51B3DD..51B3F1
correlates START/PAUSE/STOP. Decoder5E7AE0 maps0/1/2 to
EXHAUSTION/BREATH/FEIGNDEATH; all other types map UNKNOWN. No modern
GetMirrorTimerInfo/GetMirrorTimerProgress/IsSubmerged API is assumed.

Server Player.cpp SetEnvironmentFlags: breath scale=-1 when underwater with a
breathing interval, +10 otherwise. Timer activation requires underwater plus
an interval. Deactivation requires leaving liquid or no longer alive. Thus a
surface swimmer may still have an ACTIVE REFILLING breath timer. Water-breathing
effects can remove the useful countdown. Positive scale/absence cannot establish
DryGround or a generally safe surface. Paused countdown is not safe breath.

Preserved [Vanilla MirrorTimer.lua](https://raw.githubusercontent.com/MOUZU/Blizzard-WoW-Interface/master/1.12.1/FrameXML/MirrorTimer.lua)
stores timer/value/scale/paused on visible frames, converts milliseconds to
seconds, and interpolates value in OnUpdate. World-entry hides/resets the
frame; stopped events clear it. This provides a candidate READ-ONLY UI probe,
not proof of the currently loaded/addon-modified UI, freshness, missing-event
bootstrap, or underwater status on this client. Its fields need live verification
before policy may rely on them. No event hook or guessed global is installed.

## NavMesh water audit: do not steer toward underwater floor

`Maps/MoveMapSharedDefines.h`: Ground01, Magma02, Slime04, Water08, Steep10.
`contrib/mmap/src/TerrainBuilder.cpp` constructs liquid triangles from map
heights. `TileWorker.cpp:544` rasterizes separate liquid spans;57–97 and635
classify SOLID terrain beneath those spans as water or shallow transition.
The final compact heightfield is built from `tile.solid` at639, not a guaranteed
liquid-surface sheet. Assignment763–778 yields ground01, transition09, water08,
magma02, slime04; steep ground remains11. Therefore a water poly may be submerged
terrain. Its height is NOT a verified breathable surface destination.

Existing provider uses per-query Detour filters: ground first, then ground|water09,
non-steep exclude10 first, validated steep fallback second. Projection commonly
uses09. Magma/slime are not positively included. The waterAwareRoute field means
the query ALLOWED water, not that every/any polygon is water or the player is
safe. The existing `usesLiquids` header is no live depth/surface promise.

P0.3.2 cache identity excludes query filters. Shared topology, private queries,
mutex-protected temporary hazard masking/RAII restore, route-local corridors and
meshGeneration provenance already support filter isolation. No topology reload,
hazard deletion, mask change or path acceptance change is made here. Existing
water fallback is pre-existing behavior, NOT newly qualified surface traversal.

## Implementation boundary and ownership

STOP before behavior per requested evidence gate. No new production FSM or
WaterSafetyOwner exists in this checkpoint. Proposed state names must not hide
missing evidence: a swimming-only observation would need SwimmingDepthUnknown,
not SurfaceSwimming. In particular Unknown cannot authorize ground recovery,
shore exit, underwater loot, movement, or drowning-safety claims.

Future integration must occur through WorldMonitor's shared ownership gates,
after authoritative world/life reads; DeathRecovery/ghost remain observe-only.
Preserve defensive combat target identity, dialog/vendor/input exclusivity and
existing AFK swimming rejection. Do not reuse a gameplay update `continue` that
starves AFK observations or silently resets acquisition/outcome watchdogs.
RecoveryController currently has no verified water gate; do not claim underwater
eating is prevented. Combat chase/loot/shoreline behavior likewise remains
unsupported rather than silently called safe. Capabilities are NOT enabled.

No vertical press is issued, so no new held-key lifecycle exists. Before any
future movement adapter, trace the queued Jump/event7 execution and its matching
release/cancellation through owner change, bot stop, world/DLL teardown. A paired
F12 driver cannot be assumed to provide persistent Space/ascent cleanup.

## Tools, tests and reproducibility

`water_client_audit.py` pins exact client hash/layout and checks the reviewed
instruction paths and event strings; rejects foreign/truncated PE, unbacked
reads and signature mismatch. It does not attach to WoW or invoke any function.
`water_log_audit.py` consumes the FULL preserved log, counts explicit transition,
breath, emergency, press/release, stale, terminal and abandonment events, and
retains session-separated cumulative existing counters (missing stays unknown).
No AFK ghost-waterwalk or NavMesh water message counts as a water encounter.
An ascent dispatch cannot count as surfacing or shore exit. Known transitions
require `known=yes`; confirmed releases require `result=confirmed`.

Automated counts never independently certify runtime safety:
waterRuntimeQualified=false, reason=no_verified_water_encounter or
manual_evidence_review_required. This is deliberate: paired counts alone do not
prove matching command identity, safe movement, release or full episode success.
The log schema is preparatory; no runtime producer is claimed implemented.

```sh
python3 tools/water_log_audit.py /path/to/preserved-full.log
python3 -m unittest discover -s tools/tests -p 'water*_test.py' -v
python3 tools/validate.py --jobs 4
cmake --build build
git diff --check -- .
```

TEST PASS full dirty worktree:91 strict C++ tests,22 audit Python tests (13 new),
13 QuestDB Python tests, SQL and8 Lua fixtures;
`/tmp/wow-validation-fev2rjt0/results.json`. BUILD PASS from full validation
and separate build; DIFF CHECK PASS. The existing Ninja log-recovery warning
appeared and the DLL rebuilt successfully; no build configuration changed.
Isolated staged export `/tmp/wow-p04-isolated.8Eot1v`: TEST PASS42 published
C++ tests,22 audit Python tests, SQL and3 published Lua fixtures; full DLL/GUI/
loader/testhost BUILD PASS. Results `/tmp/wow-validation-vwqoi5ni/results.json`.
QuestDB Python files belonging to unrelated dirty work are absent in the
isolated checkpoint; SQL remains required. Final result-recording doc edits
do not change tested source. No runtime behavior claim follows from these tests.
There are no behavior tests for unimplemented surfacing/shoreline ownership;
do not count parser tests as the user's38 water-behavior regressions.

## Runtime gates and safe continuation

All new water capabilities: RUNTIME PENDING. No running WoW/GUI process was
available during the audit. The38-minute baseline is ordinary pre-checkpoint
regression evidence only; there is no water encounter/submersion evidence.

Next bounded source task: establish coherent current-player submersion evidence
(or validate exact-client mirror event/UI bootstrap and freshness), and prove
the ascent-release path. A read-only probe must retain timer type/current/max/
scale/paused, player identity/life/movement, monotonic sampling, and world epoch;
absence after attachment remains UNKNOWN. It must not install experimental
movement, request packets, fake native flags or classify liquid Z from mesh Z.
Then implement/test the smallest safe subset and rerun isolated validation.

Only AFTER those source gates, runtime can use a user-selected safe shallow
shoreline or natural water. Do not start an automatic water qualification from
this checkpoint: it has no drowning protection. Never intentionally drown,
enter fatigue zones, provoke combat/death, or use a hardcoded test coordinate.
Dry->surface->shore->dry and any safe incidental submersion are independent live
gates. No submersion means drowning prevention RUNTIME PENDING even if surface
traversal later passes. No later roadmap phase begins here.
