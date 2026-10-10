# P0.7 — World / Zone Preparation + Leveling Profile Foundation

This checkpoint implements isolated, advisory policies, synthetic regression
tests and the P0.7.4 unqualified raw location lifecycle observer. It does not
integrate profile selection into the running bot or qualify current location
or any new gameplay behavior in WoW. The P0.7.4 runtime reconciliation below
records the raw observer PASS only. Earlier checkpoint sections are historical.

Baseline: 75cadc00b8343837b8fbb40c29a648d5dbc56298, branch
codex/p07-world-zone-preparation. The World Evidence, Existing Profile / Quest
Architecture, and Design / Test audits ran in parallel and completed before
implementation by this coordinator. Their source findings are reconciled below.

The four inherited headers came from an independently interrupted session;
their provenance was established by a byte-for-byte comparison with its
2026-10-09 23:31:51 CEST creation patch. The user transferred ownership to this
coordinator. That session was not resumed.

## Source evidence and limits

"Source verified" here means the stated behavior was traced in the current
implementation. Guarded memory reads are not automatically independent
build-5875 disassembly qualification, and no new runtime verification is claimed.

| Field | Existing acquisition / authority | P0.7 treatment |
|---|---|---|
| Current map | UNKNOWN. WorldState has no authoritative current-map field. GrindModeController map 1 and destination maps are configuration. NAVMESH_CACHE_AUDIT.md explicitly records the gap. | Optional evidence defaults unknown. Never populate from configured map, mesh identity, corpse map or authored data. |
| Zone/area | UNKNOWN. No qualified live reader found. Quest zoneOrSort and discovery bounds describe source data. | Constraints can be represented but always remain unresolved, even if a zoneId is supplied. |
| Level | PlayerSnapshot requires descriptor +0x88; QuestPlannerStateReader separately calls UnitLevel, with zero fallback. Existing memory reader lacks plausible-range validation. | Absent is unknown; supplied 0 or greater than 60 is invalid. No clamping. |
| Position | PlayerSnapshot requires finite XYZ from the movement structure. | Coordinates alone lack map association. Spatial selection additionally requires observed mapId and matching positionMapId from the same qualified sample. |
| GUID | WorldState reads a nonzero active GUID and finds the matching type-4 object. It lacks a final identity recheck. | GUID alone does not make a sample fresh/coherent. Require explicit freshForPlayer qualification and monitor-session identity. |
| World generation | UNKNOWN. WorldMonitor increments a Run-session counter; mesh generation identifies cache lifetime. | Separate optional worldGeneration. Neither session nor mesh generation substitutes for it. |
| Race/class | Separate QuestPlannerStateReader Lua tokens exist; no strong player/session binding. Its Same comparison omits race. | Only future coherently bound observations may populate single-bit identity masks. Unknown/malformed masks cannot satisfy restrictions. |
| Faction group | UNKNOWN. Ad-hoc faction-template reads do not establish Horde/Alliance identity. | Reserved constraint/evidence fields always remain unresolved when constrained. |
| Target identity/level/geometry | UnitSnapshot has guarded reads, but ordinary target GUID and faction reads can ignore failure. World enumeration may be partial. CombatClientEvidence5875 separately qualifies current UI selection. | Descriptive target restrictions only. Do not reinterpret replicated target GUID as UI selection, infer hostility, or infer enemy absence from partial enumeration. |

Source locations at this baseline:

- [WorldState.h](../src/Objects/WorldState.h): WorldStateReader::Read, output reset,
  active GUID/type match, optional enumeration completeness.
- [PlayerSnapshot.h](../src/Objects/PlayerSnapshot.h): required level and finite
  position reads; unchecked replicated target GUID read.
- [QuestPlannerStateReader.h](../src/Bot/QuestPlannerStateReader.h): Lua
  UnitClass/UnitRace/UnitLevel, Parse and Same.
- [NAVMESH_CACHE_AUDIT.md](NAVMESH_CACHE_AUDIT.md): current-map source gap.
- [CombatClientEvidence5875.h](../src/Bot/CombatClientEvidence5875.h): stronger
  command-time identity and UI selection qualification.
- [ClassTrainerController.h](../src/Bot/ClassTrainerController.h): checked
  faction-template read, distinct from faction-group identity.

## Existing architecture reused

QuestProfile is an individual quest, not a leveling segment. The offline
VMaNGOS-to-SQLite pipeline exports TSV consumed by VanillaQuestDatabase;
ValleyOfTrialsProfiles merges authored and generated profiles. Preserve
QuestProfileMergePolicy's separation of authored mechanics and structured
acquisition restrictions. Unknown prerequisites remain unknown.

The Orc starting-route JSON supplies quest coverage and hub-exit rules, not
level segments, live location, or navigation waypoints. No production leveling
catalogue is invented here. Tests use explicitly synthetic inputs only.

The new policies use stable identities, explicit unknown states, native
header-only test patterns and existing Vanilla identity masks (race 0xff,
class 0x5df from QuestAcquisitionPolicy/QuestGraph). They do not depend on the
quest runtime or duplicate its loaders, objective execution or graph ownership.
Quest IDs are references only; validation checks their shape, not whether the
production quest catalogue contains them.

## Draft reconciliation and chosen semantics

These are explicit P0.7 policy choices, not claims about client behavior:

| Inherited assumption | Final decision |
|---|---|
| Every segment requires a map | Removed. A mapless segment is unconstrained. A constrained spatial box or reserved zone constraint must declare its map. |
| One mandatory linear chain, increasing level bands | Removed. Independent segments, multiple terminals and shared successors are allowed. Optional same-profile nextSegment hints must resolve and cannot cycle. They never authorize movement or force selection. |
| Level above maximum implies completion | Removed. Out-of-range means NoMatch. An optional completionFact explicitly declares the owner-supplied fact needed for advisory profile completion. |
| Overlapping matches fail closed | Retained deliberately. More than one definite match is Ambiguous; no vector-order winner or implicit priority. This replaces the audit's proposed priority/tie-break alternative with the requested deterministic ambiguity contract. |
| Caller must select a profile | Relaxed. Default considers the catalogue. Optional explicit scope filters by profile ID but bypasses no evidence checks. Unknown scope returns UnknownProfile. |
| Raw finite position suffices | Strengthened. It also needs independently observed, coherent map association. |
| Caller merely promises freshness in a comment | Strengthened with explicit GUID/session and freshForPlayer gates, defaulting unavailable. No live sampler asserts these gates yet. |

The whole catalogue is validated before scoping, so malformed or duplicate
definitions cannot be hidden by an explicit scope. An empty catalogue is valid
input with NoMatch, not a default Orc profile.

## Components and schema

- ProfileWorldEvidence.h: optional level, map, XYZ and position-map association,
  reserved zone/faction fields, optional race/class single-bit masks, identity,
  freshness qualification and named optional Boolean facts.
- LevelingProfile.h: stable profile/segment IDs, inclusive levels [min,max],
  optional map/zone/race/class/faction constraints, target entries/classification
  metadata, optional finite XYZ region, preparation keys, quest references,
  optional next-segment hint and opt-in profile completion fact; strict
  structural validation.
- WorldPreparationPolicy.h: sorted unknown/unmet requirement keys. Unknown
  differs from false; all true means ready. Stale/unbound facts stay unknown.
- LevelingProfileSelector.h: const inputs and returned values only. Returns
  status/reason, sorted per-candidate identities/diagnostics, optional selected
  identity and next hint, preparation result, and observed completed profiles.
- LevelingProfileObservation.h: separate observation tracker. Re-evaluates
  every sample and emits Initial, Unchanged, ContextChanged or SelectionChanged.
  It never applies a transition or supplies cached successful evidence.

All components are portable C++20 and depend only on each other and the standard
library. There is no memory/Lua reader, clock, callback, logger, controller,
movement/input API or disk loader in these headers.

## Selection and completion

1. Validate definitions and scope.
2. Require nonzero player GUID and monitorSession plus freshForPlayer. A future
   sampler must clear failed fields and bind every populated observation/fact
   to the current evaluation and same player. world.valid, a tick counter or
   a separate cached quest snapshot cannot establish this assertion.
3. Require a supplied level in 1..60. World generation may remain unknown.
4. Exclude profiles whose declared completion fact is freshly true. False
   means not completed; absent/nullopt remains unknown. An unknown completion
   fact prevents an otherwise matching segment from becoming qualified.
5. Evaluate each remaining segment. A known contradiction dominates unknown
   constraints; otherwise any required unknown makes the candidate unresolved.
   Optional unconstrained fields do not demand evidence.
6. More than one definite match yields Ambiguous. Any unresolved contender
   prevents selecting a single known match; there is no provisional authority.
7. A unique match is Selected only when all declared preparation facts are
   true; otherwise it is PreparationPending. Both are advisory results.

Complete is returned only when every profile in the requested scope has a
declared, freshly observed true completion fact. It asserts only that authored
predicate, never quest success, arrival, training or readiness for another
zone. No level boundary implicitly creates a fact. No completion facts are
currently wired to a production owner.

nextSegment is a declared hint on a uniquely matched segment, including when
preparation is pending. It is not the next executable segment. On a level jump,
map change or other evidence change, selection is recomputed from scratch.
There is no progress store, sticky previous segment or implicit chain walk.

## Deterministic observations

Candidate diagnostics and completion/preparation lists are sorted. Catalogue
permutations yield identical results. The tracker compares stable IDs/results,
scope, GUID, monitorSession, optional worldGeneration and observed map.
Context changes take precedence over selection changes. New positions or
levels within the same outcome do not create repeated transitions.

Loss of required evidence immediately returns an unqualified result; recovery
reselects from the new sample. Identity/session changes cannot reuse a cached
successful result. Reset clears observation history. The tracker assumes an
immutable catalogue during an observation session; a future catalogue reload
must explicitly reset it. It does not authenticate evidence or make execution
safe merely by seeing a new GUID.

## Exact future runtime boundary — NOT implemented

All reliability-sensitive files remain untouched.

The minimal future observe-only hook belongs in WorldMonitor::Run immediately
after WorldStateReader::Read and nowMs acquisition (baseline lines 600-602),
before the invalid-snapshot early-return/continue path at line 711 and before
survival/workload ownership branches. A separate sampler would construct a
fresh evidence value, recheck same-player identity and clear unknown fields;
the tracker would then produce sparse status diagnostics even on failed reads.
Run's counter at line 158 may label monitorSession only. No current map,
positionMapId, zone or worldGeneration should be filled from configuration.

That hook requires modifying protected WorldMonitor.h, so it is explicitly
deferred. No runtime translation unit includes the profile policy headers.

Applying restrictions or workload changes is a later, separate ownership
integration. WorldMonitor's deathRecoveryOwnedTick dispatch before
grindMode.Update (baseline lines 1484-1489) is where workload handoff is
coordinated, not permission to interrupt active combat, vendor or recovery.
GrindModeController and CombatController must continue to enforce their
existing target safety and aggressor rules. Quest owners retain quest
acquisition/execution; Detour/NavMesh retains long-distance navigation.

## Isolated evidence adapter checkpoint — 2026-10-10

Continues checkpoint `386ddaf0a2574637e34c02884ffa3a2417cc3a50`.
Three parallel read-only subagents completed Snapshot Evidence Mapping,
Identity/Freshness Audit and Regression/Test Audit before coordinator edits.
All agreed that existing world/player validity does not establish freshness,
that quest tokens are unbound, and that the adapter must require explicit
acquisition provenance. Only the coordinator changed files.

`ProfileWorldEvidenceAdapter.h` adds two pure operations:

1. `Capture(world, stamp)` copies the relevant WorldState/PlayerState values
   into a portable `ProfileWorldSnapshot`. The member-based template accepts
   the actual WorldState without importing Windows readers into the policy.
   Capture requires world/player validity, nonzero manager/local-player and
   descriptor pointers, and `player.address == localPlayer`. It does not read
   memory or establish freshness. The stamp must be assigned at acquisition,
   never added retrospectively to cached data. Its timestamp is the start of
   the world read, conservatively bounding the age of every copied field.
2. `Adapt(snapshot, current, maximumAgeMs)` returns a new evidence value.
   The caller must supply an independent post-acquisition identity recheck:
   successful reads of the current manager, active GUID and matching type-4
   local-player object, tagged for the current evaluation. Copying identity
   from the input snapshot does not meet this contract. GUID, manager and
   local-player address must match and be nonzero. Capture/recheck monitor
   sessions and sample sequences must match and be nonzero. Recheck time must
   not precede acquisition; elapsed age must be within the explicit caller
   budget. Evaluation time is taken after the identity recheck using the same
   monotonic clock. A zero budget permits equal timestamps only. Timestamp
   zero itself is valid. A prior evaluation is rejected even within the age budget.

Any validity, identity, session, sample or freshness failure returns default
evidence: no identity, `freshForPlayer=false`, every optional field Unknown
and no facts. No previous success is retained. On coherent input, level and
position are validated independently; invalid level does not discard valid
XYZ, and invalid XYZ does not discard valid level. There is no clamping.

| Evidence output | Exact adapter source / semantics |
|---|---|
| Player GUID | `WorldState.activePlayerGuid`, nonzero and equal to independently rechecked identity. |
| Monitor session | Caller acquisition stamp, equal to the current evaluation session. Run's monitor counter may label this field only. |
| Freshness | Explicit acquisition/recheck qualification and bounded age, never `world.valid` alone. |
| Player level | Embedded `world.player.level`, required descriptor +0x88 read in `PlayerSnapshot.h:226`; accept 1..60 only, otherwise Unknown. No quest-level fallback. |
| Position | Embedded `world.player.x/y/z`, required movement reads in `PlayerSnapshot.h:326`; recheck every coordinate with `isfinite`. Finite zero/negative values are accepted. |
| Map / position map | Always Unknown: no qualified current-map reader. Coordinates have no proven map association. |
| Zone/area / faction group | Always Unknown: no qualified sources. |
| Class/race | Always Unknown in this step. QuestPlannerSnapshot has no GUID/session/sample/time binding and is not an accepted adapter input. |
| World generation | Always Unknown. Neither monitor session nor sample sequence is world/map identity. |
| Preparation/completion facts | Empty: no qualified fact producer is connected. |

The initial GUID/object association is in `WorldState.h:108-120,203-209`;
the embedded player read is at `:248-253`. These reads have no final identity
recheck. `QuestPlannerTypes.h:399-415` has tokens/level but no binding;
`QuestPlannerRuntimeController.h:614-628` retains its previous snapshot after
a refresh failure, and `QuestPlannerStateReader.h:417-428` omits race from
its equality check. These limits are why quest data is excluded entirely.
No location is inferred from quests, profiles, navigation configuration,
race, faction-template values or session/sample counters.

The adapter has only const/value inputs and returned values, with no clock,
reader, Lua, callbacks, logging, cache, commands or controller dependencies.
It validates supplied provenance; it cannot authenticate a caller that labels
old data as a new acquisition. A real sampler and runtime qualification remain
future work. Class/race support likewise requires a separately qualified
same-player/session acquisition before expanding this adapter.

The future hook remains immediately after `WorldStateReader::Read` and
`nowMs` acquisition in `WorldMonitor::Run` (`WorldMonitor.h:596-602`), before
the invalid-read early continue and before ownership dispatch. That future
work must stamp the start of the world read, perform the independent identity
recheck, choose a freshness budget and emit observe-only diagnostics even
for unavailable evidence. The monitor Run counter is session identity only;
the caller must maintain a separate per-evaluation sample sequence. This
checkpoint does not implement that hook or edit any protected controller.

`tests/profile_world_evidence_adapter_test.cpp` covers valid mapping, level
endpoints/implausible values, zero/mismatched GUIDs, invalid snapshots and
pointer associations, missing/mismatched sessions and sample sequences,
inclusive age/zero-age/future/stale/large timestamps, NaN and both infinities
in every coordinate, zero coordinates, explicit Unknown fields, repeated
calls after failure, const-input preservation, and selector propagation for
level-only versus map/class-constrained profiles. Under MinGW the same test
instantiates Capture against the actual `Objects::WorldState`; native tests
use a small value fixture. Test discovery needs no harness changes.

Adapter checkpoint validation:

- Focused native C++20 compile/run with `-Wall -Wextra -Werror`: PASS.
- MinGW i686 compile with the same warnings and actual WorldState: PASS.
- `python3 tools/validate.py --jobs 4`: PASS, 110 C++ test executables,
  42 audit Python tests, 13 QuestDB Python tests, SQL/TSV fixture, 10 Lua
  fixtures, full MinGW build and diff check. All 245 validation records passed.
  Results: `/tmp/wow-validation-4jlucjwz/results.json`.
- Separate `cmake --build build`: PASS for DLL, testhost, loader and GUI.
- `git diff --check` and untracked-file `--no-index --check`: PASS.
- All eight protected files match checkpoint `386ddaf` with no diff.
- `git status --short`: only this document modified, the adapter header and
  its focused test untracked. No staged files or commit.
- Runtime: NOT RUN / PENDING. No new WoW behavior is enabled or qualified.

## P0.7.3 — source-qualified location investigation

Continued from `93dad5c6bb02c1cd9ec981e5a12792ba547d5475` on
2026-10-10. Three parallel read-only agents completed before coordinator edits:
A traced native current-map sources, B traced zone/area and local assets, and
C audited lifetime, reader isolation and tests. All three recommend **CASE B**.
The coordinator reconciled their reports and independently inspected the map
getter, constructor/destructor, area-row selection, ID publication and cleanup.

Exact client: `/home/ludvig/Games/WoW Vanilla/WoW.exe`, PE32 i386, preferred
base `0x400000`, SHA256
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
The hash and focused instruction/string signatures pass the new offline audit.
All addresses below are absolute VAs in this exact executable. Research names
describe disassembled behavior; they are not asserted debug symbols.

### Reconciled qualification decision

- **SOURCE VERIFIED:** the bounded instruction-path facts below: numeric map
  getter/writer/consumers, numeric zone/child-area cache provenance, parent/child
  relationship, text API return types, publication ordering and teardown gaps.
  This label applies to those facts only. **No candidate satisfies the complete
  SOURCE VERIFIED current-world reader standard**, because current-player and
  lifetime qualification remains incomplete.
- **RUNTIME OBSERVED:** none in this task. No WoW launch, attach, native function
  call, Lua command, memory write, patch, input or gameplay action was performed.
- **INFERRED:** patch-2 > patch > base asset precedence (not traced through
  the exact client's archive mount path). These candidates could support a
  future diagnostic observer; repeated stable values would be useful research
  telemetry. They would not establish a fully initialized, current, coherent
  world lifetime.
- **UNKNOWN:** current map, zone, area/subzone, position-map association and
  world generation as usable profile evidence. No new production reader,
  policy, schema field, adapter input, WorldMonitor hook or controller change
  is introduced. Area has no separate field in the existing evidence schema;
  it remains unsupported, never folded into zone. Existing optional map/zone
  and generation outputs remain empty. Actual runtime archive selection is
  also Unknown; the asset inspection below does not establish it.

### A — current-map sources and rejected alternatives

| Candidate | Exact origin / consumer path | Validity and decision |
|---|---|---|
| Object-manager map `[0xB41414]+0xCC` | Getter `0x468580`, wrapper jump `0x5EA700`, setter `0x4685A0` (ECX -> owner+CC). Constructor `0x464FF0` receives map in ECX, zeroes +CC at `0x4650ED`, publishes root at `0x46510E`, sets map at `0x46512C`. | Strong numeric map provenance, but publication precedes completion. Null getter returns 0, indistinguishable from valid map 0 without separately qualified availability. Unknown as profile evidence. |
| Current-map consumer | `SetMapToCurrentZone` registration `0x845090` -> `0x4A7E20` -> `0x4A6650`; active-player lookup precedes map calls at `0x4A6697/0x4A66F1`. Diagnostic `0x5AC256` uses the same getter, map table `0xC0DAA8/AC`, and `Map:\t\t%u (%s)\n` at `0x85D71C`. | Establishes map identity semantics beyond a suggestive name or historical offset. Does not establish safe sampling. SetMapToCurrentZone writes selection state and must not be invoked by a read-only observer. |
| Pending transfer map `0x88262C` | Registration `0x40174A` binds opcode `0x3E` to `0x401B00`; packet read fills global, validates map-table membership, schedules `0x401BC0`. Diagnostic `Bad SMSG_NEW_WORLD zoneID\n` at `0x82E3B0` is historical wording, not a zone/area contract. Another packet callback `0x401DE0` compares incoming map with getter at `0x401E36`, then writes pending map at `0x401E5C`. | Destination/transition value can differ from the installed owner map. Rejected. |
| Transfer ordering | `0x401BC0` destroys old owner at `0x401BF7`, constructs replacement from pending map at `0x401C22`, then loads terrain at `0x401C5C`. | New owner/map exists before terrain loading completes. Matching player/root/map cannot by itself classify loading. |
| Terrain map `0x86A2CC` | Pending map at `0x401C49` -> loader `0x66FBE0` -> EDX at `0x66FC14` -> `0x6941F0` -> store at `0x69441E`. Consumers `0x670269/0x6703B3` access map records. Startup `0x691BB7` initializes -1. | Unload `0x697AC0` does not reset this map; it clears resource markers `0xC9E380/384` only at `0x697BE8/ED`. Retained terrain map and late-cleared markers do not qualify current readiness/generation. Rejected. |
| Selected world-map indices `0x84506C/70` | `GetCurrentMapContinent` registration `0x8450A0` -> `0x4A7ED0`; `GetCurrentMapZone` `0x8450A8` -> `0x4A7F00`. Each returns stored index +1. Selection path `0x4A7E14` -> `0x4A67A0` writes supplied indices. | UI selection indices, not current map or AreaTable IDs. Rejected. |
| `GetMapInfo` | Registration `0x845098` -> `0x4A7E30` -> `0x4A6CF0/0x4A6D50`, selected-map information/string/texture dimensions. | No accepted current numeric identity. Rejected. |
| World-initialized flag `0x882734` | Set at `0x4017E5`; cleared at `0x40204D`, after object teardown at `0x402011`. Transfer `0x401BC0` replaces owner without writing this flag. | Not a per-transition readiness guard or generation. Rejected. |
| Existing repository inputs | Offsets.h supplies root/list/GUID only; WorldState has no map. Controller configuration, navmesh destination/mesh generation, corpse map, quest zoneOrSort, authored profile data and XYZ have their existing separate meanings. | None may substitute for observed current-world identity. Creature composition and minimap text likewise provide no accepted evidence. |

Mechanically, bounded memory reads of a scalar need not mutate the client.
That fact does not make these scalars current. Calling map selection, terrain
or packet functions would violate this task's boundary and was not attempted.

### B — numeric zone/area, text and local assets

`0x494780` writes ECX to `0xB4E314` (zone) at `0x49479A` and EDX to
`0xB4E318` (child area) at `0x4947A0`. These are separate DWORD stores before
text updates/events, with no qualified publication sequence protecting them.

The direct caller at `0x67E645`, within `0x67E510`, establishes numeric
provenance: `0x6703A0` supplies a spatial area ID; `0xC0E048` is the
AreaTable row-pointer table, bounded by `0xC0E04C`. Row +8 is the parent
ID (`0x67E57D`). A nonzero parent selects its row as zone and retains the
child row. With no parent, the original row becomes zone and child is null.
`0x67E62F–0x67E645` passes row +0 IDs to the writer. **The child cache is zero
for a top-level row; it is not a guaranteed copy of zone ID.** Localized text
comes separately from row +0x2C + locale*4 (`0xC0E080` locale index).

This establishes stable numeric cache semantics, but the source is not a
qualified fresh local-player sample:

- Upstream `0x5DB900` increments `0xC4D790`, skips work until threshold 10,
  and prefers GUID `0xC4DA98/9C` with type mask 8. Only on lookup failure does
  it fall back to active-player GUID/type mask 0x10. The selected object's
  +0xE0 supplies the spatial input. Alternate-owner meaning remains unresolved.
- Null input, failed spatial resolution, missing/out-of-range rows and
  unchanged-update suppression can return without clearing published IDs.
  A stable cache can therefore be unavailable/stale rather than newly sampled.
- `0x67E7F0` compares prior zone/area/map markers `0x8685FC/600/604`;
  these are deduplication values, not a monotonic generation. `0x67E450`
  resets only markers `0x868608/604` to -1.
- Teardown calls marker reset at `0x401FCB`, object-manager destruction at
  `0x402011`, then UI cleanup `0x491180` at `0x402039`. Only within that
  later cleanup do `0x491266/26C` zero zone/area. Thus cached IDs survive
  part of teardown, including object-manager destruction.

| Text candidate | Registration -> callback -> cached pointer | Result / rejection |
|---|---|---|
| GetZoneText | `0x83E0A8` -> `0x48A0A0` -> `0xB4B3F8` | Localized string, no numeric ID. |
| GetRealZoneText | `0x83E0B0` -> `0x48A0C0` -> `0xB4B404` | Localized string, no numeric ID. |
| GetSubZoneText | `0x83E0B8` -> `0x48A0E0` -> `0xB4E280` | Localized string, no numeric ID. |
| Zone-change events | `0x51ADA7–C4` registers ZONE_CHANGED / ZONE_CHANGED_INDOORS / ZONE_CHANGED_NEW_AREA; writer emits 0xCD for changed zone, else text-change 0xCB/0xCC according to indoor flag. | Update notification, not polled generation/freshness proof. |

Each text callback substitutes empty string `0x882748` for null and calls
Lua push-string `0x6F3890`, which mutates the Lua stack. The setter allocates,
copies and frees cached strings; cleanup frees/clears them after IDs. Neither
these pointers nor text-to-ID conversion is an accepted read-only ID source.
Native spatial resolver/setter invocation would also write state/emit events.

Local archives in `/home/ludvig/Games/WoW Vanilla/Data` were read without
extraction or client execution using the already installed `mpyq` from
`/home/ludvig/Programming/Projects/wow-internal-5875/build/r01b2-research/python`.
Patch-2, patch and base archives were checked separately. The table identifies
highest-patch inspected assets; archive precedence is inferred, not qualified.
These assets interpret IDs and UI semantics only; their presence cannot
establish current location or prove which asset the running client loaded.

| Asset and inspected archive | SHA256 / interpretation |
|---|---|
| DBFilesClient\\AreaTable.dbc, patch-2.MPQ | `ac4c33c2ae65d37ab801050adf489ea5286d7dd9f9ebf78e6e5c7120418a3528`; WDBC, 1081 rows, 25 fields, 100-byte records, 15838-byte string block. |
| DBFilesClient\\Map.dbc, patch-2.MPQ | `4bb6b53841b113eb0ec4fa624ef42d7de7ebdd5169aac17d3e6f9a4f73654245`; 44 rows, 42 fields, 168-byte records. |
| AreaTable.dbc, patch.MPQ | `e3d588f4da646d817672e58205bf61a0019f86c7af2fd9d2d4bb06edafd0d96c`; base dbc.MPQ has older 21-field/84-byte rows. Do not qualify against base rows. |
| Interface\\FrameXML\\WorldMapFrame.lua, patch.MPQ (absent patch-2) | `74ef7d6506bf76190679ad02f3f4a4d08efcf3d626cfd0e7cd27ba21d6ee0e05`; lines 245–265 call SetMapZoom from dropdown handlers and restore selected GetCurrentMapZone index. |
| Interface\\FrameXML\\Minimap.lua, patch.MPQ | `73700cf18fe1b3554a5b3c852982d0426bebe0a5eb9e2f5eaa867f049bdfa9e1`; displays GetMinimapZoneText, not numeric evidence. |
| Interface\\FrameXML\\ZoneText.lua, patch.MPQ | `4c21094a538292df722e7a4772bf8984413275cfb19be359ecf9a11a537ae73a`; inspected content handles autofollow status, no numeric source. |

The binary loader near `0x53FCE0` checks WDBC, field count 25 at `0x53FD6D`
and record size 100 at `0x53FD9B`; `0x53FEC8–E6` builds a row-ID-indexed
table. `0x574040` returns path `0x857ECC` (AreaTable.dbc). Example patch-2
rows: 14=(map1,parent0), 362=(map1,parent14), 363=(map1,parent14),
1637=(map1,parent0). Names Durotar/Razor Hill/Valley of Trials/Orgrimmar
corroborate schema interpretation only, never player location.

### C — lifetime, reader contract and future hook

Root assignment `0x464FA0`, save/switch `0x464FB0` using `0xB41418`, and
restore `0x464FD0` prove `0xB41414` is a context pointer, not a generation.
Network helper `0x538020` obtains a temporary owner from connection +0x1AD8.
Constructor publication precedes map setup. Destructor `0x467700` calls
object cleanup `0x467800` at `0x467711` before clearing root at `0x46771E`.
GUID getter `0x468550` returns owner +0xC0/C4; cleanup uses it without first
clearing GUID. GUID publication at `0x466245/0x466254` uses two DWORD stores
on i386. Root/GUID/map equality cannot exclude torn identity, switched/restored
context, same-address reuse or an entire same-character transition between reads.
The loading query `0x407E70` tests resource handle `0x882BE0`, not a qualified
complete loading-phase contract.

The unresolved source gap is a proven read-side sampling/lifetime mechanism
covering initialization, teardown, context switching, map changes, same-map
reload and same-character/address reuse. A true generation with publication
and invalidation ordering, or an equally rigorous serialized sampling boundary,
is needed. The area source additionally needs current-player ownership and
freshness proof, independent from map availability. Matching area/map DBC
values cannot repair stale evidence.

ConnectionEvidence5875 supplies reusable architecture only: injectable bounded
read callback, exact signature comparisons, owner overflow checks, repeated
reads and default-Unknown output. Server-connected state, historical Glue,
connection tracker and reconnect policy cannot qualify world lifetime. Existing
ClientIdentity checks version/name, not the exact executable SHA. A future
reader needs its own explicit exact-client provenance contract. Prefer a
fault-safe ReadProcessMemory callback to Core::Memory's VirtualQuery + memcpy
check/use race. No connection or generic-memory code is changed here.

Any future reader must reject wrong signatures, null/unreadable/overflowing
owners, mismatched/torn identity, loading/unavailable lifetime and stale cache;
recheck manager/GUID/type-4 local-player association after acquisition; bind all
accepted fields to the same qualified sample; and discard prior successes on
failure. Map, zone and area validity must be independent. Neither monitor
session nor sampleSequence is a world generation. Same-value double reads alone
are not sufficient. No new successful-value cache is authorized.

The future adapter input should be an isolated source-qualified location
sample with its lifetime and identity binding. The eventual WorldMonitor hook
remains around `WorldStateReader::Read` / `nowMs` in `WorldMonitor::Run`
(`WorldMonitor.h:596–602`), with read-start stamp beforehand, independent
post-read identity/lifetime checks, and diagnostics before the invalid-read
early continue or any owner dispatch. No such hook is implemented here.

A **controlled raw diagnostic observer is justified as a future research task**
to characterize pending/owner/terrain map and zone/area caches across login,
loading, map transfer, same-map reload, zone-only/area-only changes and teardown.
It must label values unqualified, include unavailable samples, invoke no client
functions/actions and never feed profile selection. Observing matching values
cannot alone close the static lifetime gap; a profile-populating observer is
not yet justified. No live test is needed or performed in this checkpoint.

### P0.7.3 offline audit and regression coverage

Repeat against the exact file:

```sh
python3 tools/profile_world_location_client_audit.py '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x464fa0 --stop-address=0x465140 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x67e510 --stop-address=0x67e670 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
```

The audit checks the SHA256, PE32/i386/base layout and a fixed manifest of
43 instruction anchors and 12 strings for the documented paths. It is not a
general disassembler or proof of the whole call graph. Asset hashes above are
a research record, not checks performed by this executable-only tool. PASS explicitly
retains `currentMapQualified=false`, `currentZoneQualified=false`,
`currentAreaQualified=false`, `worldLifetimeQualified=false`,
`readerImplemented=false`, `runtimeObserved=false`.

Python regressions reject wrong fingerprints before reading addresses, malformed
PE fixtures, and every mutated/truncated/missing signature or string. They test
deterministic audit results and prohibit a PASS from granting location/runtime
qualification. The adapter's complete local include closure remains only the
adapter and evidence headers, without memory readers or action adapters.

C++ regression extends the existing adapter test with deliberately unqualified
map, position-map, zone, area and generation fields. Independent candidate
changes, changed coordinates, valid/unavailable/restored states, new sessions,
sequences and identities never fabricate location or generation. Equal identity
after a synthetic gap tests only preserved Unknown, **not ABA detection**.
Native execution and MinGW compilation against actual WorldState both pass.
No implemented-field reader tests are claimed: there is no new reader or field.
Future Case A must add all signature, owner/read failure, coherence, loading,
lifetime, map-change and independent zone/area-change cases before acceptance.

P0.7.3 validation, 2026-10-10:

- Exact local executable offline audit: PASS (SHA256, PE layout, 43 instruction
  anchors, 12 strings). SOURCE path verification only; no runtime qualification.
- Focused native C++20 compile/run and MinGW i686 compile against WorldState:
  PASS with `-Wall -Wextra -Werror`.
- Five focused Python audit/boundary tests: PASS.
- `python3 tools/validate.py --jobs 4`: PASS; 110 C++ executables, 47 audit
  Python tests, 13 QuestDB Python tests, SQL/TSV fixture, 10 Lua fixtures,
  full MinGW build and diff check. All 245 validation records passed.
  Report: `/tmp/wow-validation-20foukj7/results.json`.
- Separate `cmake --build build`: PASS (DLL, testhost, loader, GUI).
- `git diff --check` and untracked-file `--no-index --check`: PASS.
- Source comparison against checkpoint `93dad5c`: no changes anywhere in
  `src`, including all eight protected files. Read-only review: no blockers.
- `git status --short`: only this document and the adapter test modified;
  new location audit tool and its Python test untracked. No staging or commit.
- Runtime: NOT RUN. Production location fields remain Unknown.

## P0.7.4 — read-only world location lifecycle observer

Continues clean checkpoint `71e2ed6` on 2026-10-10. Three parallel read-only
subagents completed before any implementation; only the coordinator edited:

- A audited architecture, bootstrap, logging and unload. The existing connection
  observer is the smallest safe integration; partial world-read identities must
  not be hidden behind full-snapshot validity.
- B rechecked the exact executable and source anchors. Raw scalar reads are
  feasible with exact-file provenance, live signatures and repeated bounded
  reads. Globals must remain observable when the manager disappears.
- C audited tests and action boundaries. Existing portable fixtures and include
  closure guards can cover the new reader without a second observer framework.

The reports agree that readable/stable storage is not current-world evidence.
The implementation uses independent owner/map and zone/area failure groups,
plus acquisitions bracketing the world read. It preserves the existing
`WOW_INTERNAL_CONNECTION_MODE=observe` route, 250 ms cadence, GUI heartbeat,
first/change-only logging and input-free teardown. `WorldMonitor.h`,
`dllmain.cpp`, `WorldState.h`, all gameplay controllers and profile headers are
unchanged. Normal Grind execution and its stop path are unchanged.

### SOURCE VERIFIED

This label applies to instruction provenance and code boundaries, **not** to
current map/zone/area qualification. The runtime implementation consists of:

- `LocationClientFingerprint5875.h`: observer-start SHA256 of the executable
  path returned by Windows. Requires the exact P0.7.3 digest; path truncation,
  unreadable/empty/over-64-MiB files, partial reads and crypto errors fail closed.
  Reads in 16-KiB chunks, opens only for reading, closes all handles. This uses
  Windows CryptoAPI (`advapi32`), no client function. This startup provenance
  result is session-local; no candidate values are cached across samples.
- `LocationCandidates5875.h`: each acquisition requires that file fingerprint,
  base `0x400000`, and exact live signatures for full map getter `0x468580`,
  setter `0x4685A0`, zone/area publication `0x494787`, and reset `0x491266`.
  These four anchors already exist in the offline audit; no new signature or
  address research was needed. Fingerprint/signature failure exposes no raw
  values and performs no candidate dereferences.
- `ConnectionLifecycleObserver5875.h`: uses the existing fault-safe
  `ReadProcessMemory` callback with exact byte-count checks. Acquires location
  before and after the existing world read; formatting rejects detected changes
  across that bracket. No native client call, Lua or client-memory write occurs.
- `ConnectionObservationPolicy.h`: includes raw candidates in the existing
  change comparison and correlates partial world reads. No action policy,
  profile adapter or controller is reachable in its local include closure.

| Raw output | Exact storage / bounded read | Failure and numeric contract |
|---|---|---|
| `locationOwner` | DWORD at `0xB41414`, repeated around map reads | Missing/change -> unknown; stable null prints `0x0`. This is a raw pointer observation, not a world generation. |
| `mapCandidate` | DWORD at that nonnull owner `+0xCC`, read twice | Require owner <= `UINT32_MAX-0xD0`, two readable equal values and unchanged owner. Otherwise unknown. |
| `zoneCandidate` | First DWORD of 8-byte pair at `0xB4E314` | Two readable equal pairs required; either changed field/failure makes both globals unknown. Independent of manager availability. |
| `areaCandidate` | Second DWORD of that pair, `0xB4E318` | Same paired contract; no parent/child substitution. |

All three candidates print unsigned decimal DWORDs: 0 through 4294967295.
Zero is a raw value, not an unknown sentinel. There is no ID-table membership
filter, clamping or assertion of a valid map/AreaTable ID. Each invocation
starts Unknown; a read failure cannot reuse a previous successful candidate.
Across the world-read bracket, changed/missing owner or map suppresses map;
changed/missing zone or area suppresses the global pair. These independent
groups preserve useful teardown telemetry without manufacturing coherence.

World snapshot stage/validity is still exactly the existing reader's result.
Stage allowlists expose successful partial reads: manager includes a read-null
`manager_missing`; `active_guid_missing` exposes manager and read-zero GUID;
later first-object/local-player failures retain read GUID; a local-player
pointer is exposed only for a complete snapshot or `player_snapshot_unreadable`.
Incomplete enumeration is not proof of local-player absence. Root unreadable,
unknown and future stages fail closed. These identities retain the existing
reader's limitations, including no final GUID/type recheck; they are correlation
observations, not newly qualified coherent identity evidence.

Each emitted `CONNECTION OBSERVE` row contains all location candidates, raw
location owner, world stage/snapshot, manager/GUID/local player, independently
sampled server predicate, historical glue name and `processAlive=yes`.
`sampleStartMs`/`sampleMs` bound the polling acquisition; timestamps never
enter the change comparison. Fixed fields include:

```text
locationQualification=unqualified locationReason=source_lifetime_gap
worldCorrelation=sequential inputOwner=none commands=none
glueScreenSemantics=historical loading=unknown disconnectConfirmed=unknown
```

Existing `sourceVerified` is explicitly scoped by
`sourceVerifiedScope=connection_instructions`. `locationSignatures=pass` means
the raw reader's live anchors matched, not SOURCE VERIFIED current location.
`lastGlueScreen` never becomes current UI visibility. OM loss never becomes
a disconnect. No row uses `currentMap=`, `currentZone=` or `currentArea=`.

The offline audit retains its existing signatures and runtime-independent
behavior. Only implementation metadata changes to `readerImplemented=true`
with `readerScope=unqualified_raw_observe_only`, because an actual runtime raw
reader now exists. Every current-location/world-lifetime qualification and
`runtimeObserved` remains false. An offline PASS cannot qualify runtime reads.

### RUNTIME OBSERVED

At implementation time, no WoW launch, injection, logout, Enter World or
stop/unload test was performed. The later capture reconciliation below supersedes
that historical NOT RUN status for raw observation only.

#### Capture reconciliation — 2026-10-10

**P0.7.4 RAW LOCATION LIFECYCLE OBSERVER — RUNTIME PASS**

The user-supplied result in AI_HANDOFF.md is corroborated by independently
inspecting the saved retry capture. No new WoW run was performed in this task.
Source, runtime-evidence and validation research ran in three parallel read-only
subagents before coordinator edits; their conclusions agree on the limited
qualification below.

Provenance: `runtime-captures/p074-location-lifecycle-retry-2026-10-10/`,
main `wow-internal.log` lines 11–21 and `wow-internal.lifecycle.log` lines 19–34;
PID `296`, session `296.134360886272945680.57555570.500`.
The lifecycle file is cumulative: lines 1–18 belong to the earlier PID 300
attempt. That earlier capture has only one in-world candidate row and cannot
substitute for the retry transitions. Capture files are ignored local artifacts;
the following sanitized record preserves the relevant evidence in this audit.

| File | SHA256 |
|---|---|
| Retry `wow-internal.log` | `5914e5adda888f0719eeb952503e4171ed358c3f0038c6c81639f53a347a7dd9` |
| Retry `wow-internal.lifecycle.log` | `cb058a9adbfe462e260e9e316b1388e914686dfc4c1d6afea99ab7e63130f380` |

| sampleMs | worldStage | manager | playerGuid | localPlayer | mapCandidate | zoneCandidate | areaCandidate |
|---|---|---|---|---|---|---|---|
| 57556117 | complete | `0x14fe508` | `0x1aa56` | `0xde68008` | 1 | 14 | 363 |
| 57607710 | manager_missing | `0x0` | unknown | unknown | unknown | 14 | 363 |
| 57607967 | manager_missing | `0x0` | unknown | unknown | unknown | 0 | 0 |
| 57628493 | active_guid_missing | `0x8d35d08` | `0x0` | unknown | 1 | 0 | 0 |
| 57633505 | complete | `0x8d35d08` | `0x1aa56` | `0x19da8008` | 1 | 14 | 363 |

The full capture supplies explicit `unknown` identity values in later teardown,
where the previous handoff excerpt said “not supplied.” Startup reports
`fileFingerprint=pass`; each of the five rows reports `locationSignatures=pass`,
`locationQualification=unqualified`, `locationReason=source_lifetime_gap`,
`worldCorrelation=sequential`, and `inputOwner=none commands=none`.
This confirms the fingerprint helper succeeded in this recorded session only.
Every row retains `serverConnected=yes` and historical `lastGlueScreen=charselect`;
current UI visibility, loading, disconnect and action eligibility remain unknown.

Zone/area storage remains 14/363 after manager loss, then clears. The new manager
exposes map 1 while its active GUID is zero. The same GUID returns with new
manager/local-player pointers and matching candidates. These are observed
storage transitions, not qualified location samples or exact visible-screen
times. The source paths in sections A–C explain why the observer permits these
rows: zone/area reads are independent of manager availability, and raw map reads
do not require an active player. No gameplay owner or profile transition occurs.

GUI `stop_button` targets PID 296 at monotonicMs `57660593`; observer STOP
follows at `57660823`, then logical `RUNTIME DETACHED` and bootstrap
`unload_requested` at `57660824`. The capture contains no action-command markers.
Source boundary checks separately establish the observer's input-free path;
absence of logged commands alone is not an exhaustive input trace.

**UNKNOWN:** completed OS module unload, post-stop client responsiveness,
visible-screen/loading annotations and heartbeat continuity during unchanged
dwell. **SOURCE GAP (completion telemetry):** current `DllMain` handles attach
only, and `UnloadSelf` emits `unload_requested` before `FreeLibraryAndExitThread`.
Logical detach/request telemetry therefore cannot prove completed unload.
Absence of a nonexistent completion marker is not evidence of unload failure.
The observer PASS is preserved; complete manual-runbook acceptance is not
claimed. Map/zone/area remain **UNQUALIFIED** and profile location population
remains **BLOCKED**.

### INFERRED

Values persisting across positive world identity loss are evidence of stale
storage, not current-world validity. Relative changes in emitted rows can
characterize publication/teardown ordering at this sampling resolution.
Unchanged dwell produces no extra row: retain GUI heartbeat and manually timed
screen observations to distinguish suppression from an observer that stopped.

### UNKNOWN

Current-world lifetime, initialization completion, current-player ownership of
the zone/area cache, cross-field atomicity, world generation, same-address reuse
and changes between polls remain unknown. Two equal pairs can straddle a writer
paused between DWORD stores; repeated equal owner/map reads cannot detect ABA.
The map constructor publishes its owner before setting map, teardown starts
before clearing it, and zone/area reset occurs later. Sampling every 250 ms plus
read duration can miss short stages. Human-observed loading remains a manual
annotation, never an automatic loading classifier.

`ProfileWorldEvidence` and its adapter are untouched and unreachable from the
observer. Map/zone remain absent, area remains unsupported, and position-map
association remains absent. No single lifecycle run, matching value, changed
value, stable dwell or successful world return promotes these fields.

### Controlled manual lifecycle runbook

Keep both `build/wow-internal.log` and `build/wow-internal.lifecycle.log` for one
session, plus manually recorded visible-screen times. Use the existing Wine
prefix. Normal user UI actions are the only transition source.

1. Start a fresh GUI/client from this build with
   `env WOW_INTERNAL_CONNECTION_MODE=observe wine ./build/wow_gui.exe`.
   Use **Start WoW** so the client inherits the variable; setting it only on a
   loader cannot change an already-running client's environment.
2. Manually log in normally and enter the intended character in a safe ordinary
   location. No credentials are requested or handled by the observer.
3. Press **Start Bot**. Require `CONNECTION OBSERVE CONFIG mode=observe` and
   `LOCATION OBSERVE CONFIG fileFingerprint=pass`, with `inputOwner=none
   commands=none`. If these are absent, stop and diagnose configuration.
4. Record the first in-world row: candidates, location owner, world stage,
   manager, GUID, local-player pointer, server predicate and historical screen.
   Require unqualified semantics. Wait briefly; unchanged rows should suppress
   while GUI heartbeat continues. Unknown raw reads are a finding, not a PASS.
5. Manually use normal **Logout**. Record the action and visible transition
   times and every emitted candidate/identity change before and during teardown.
6. At character select, dwell approximately 10–15 seconds. Note retained or
   cleared candidates and actual server predicate. Do not infer disconnect,
   current-screen visibility or candidate validity from OM absence.
7. Manually choose the same character and press normal **Enter World**.
8. Record visible loading start/end and all emitted rows, including unknowns,
   partial manager/GUID publication and any raw candidate changes. A missed
   short phase is unobserved, not evidence that it did not happen.
9. After world return, record the fresh complete snapshot and compare GUID,
   pointers and candidates with step 4. No gameplay may start. Record an optional
   natural zone transition only if one happens without manufacturing it.
10. Press **Stop Bot**. Require observer `BOT SESSION STOP`, logical
    `RUNTIME DETACHED` and bootstrap `DLL LIFECYCLE event=unload_requested`,
    with no stop/hold, attack, AFK or other input operation. The current source
    emits no DLL_PROCESS_DETACH completion marker; these are pre-unload records.
11. Independently confirm module absence and that WoW remains running and
    responsive after the request; record the observation method and time. Keep
    the complete logs and screen annotations together. Static control-flow
    checks alone cannot prove successful runtime unload.

Do not force disconnect, provoke death, enter water deliberately, reconnect,
automate character selection or request credentials. For subsequent ordinary
gameplay, restart GUI/client with the variable unset rather than empty.

### Acceptance evidence and falsification

Useful evidence covers stable in-world -> normal logout -> character-select
dwell -> Enter World/loading -> same-character world return -> Stop Bot/detach.
Every emitted change must retain raw/unqualified labels and the full lifecycle
correlation fields. All action-eligibility/loading/disconnect gaps remain
unknown. Any action adapter/input or automatic promotion fails acceptance.

Candidate freshness would be falsified by retained zone/area numbers after
manager/local-player loss, readable map storage after active-player loss, a
new map value before usable world/player publication, or observed delayed
updates after return. These observations concern staleness/ordering; they do
not justify globally classifying all raw values as valid or invalid. Changes
during loading, same numbers across different owners, and independent natural
zone/area changes are useful even without proving freshness.

Before profile consumption, source research must establish an explicit lifetime
and initialization predicate, current-player binding for the zone/area updater,
coherence with GUID/local player/position, and a defensible generation/ABA
strategy. Independently qualified runtime cases must cover loading, teardown,
same-map reload, map transfer and zone/area-only changes. The adapter must then
be deliberately extended with qualified provenance and regression tests in a
separate task. This phase cannot satisfy that acceptance by itself.

### P0.7.4 validation

Focused fixtures cover fingerprint/base/signature rejection before data reads,
every mutated signature byte, missing/truncated signatures, null/unreadable/
overflowing owner, unreadable field/pair, failed repeat reads, owner/map/pair
changes, cross-world-read changes, zero/full-width values and no reuse after
failure. Tracker fixtures cover partial stages, 10,000 unchanged samples,
independent candidate changes, stable candidates across teardown/dwell/return,
unknown replacement and immutable unqualified semantics. Source checks pin
bootstrap/client validation, gameplay exclusion, read/log/IPC dependency closure,
profile exclusion, diagnostic-only callers and input-free detach/unload flow.

Validation on 2026-10-10:

- Exact local executable offline audit: PASS (SHA256, PE32/i386/base and the
  unchanged 43 instruction anchors / 12 strings). Runtime raw anchor bytes and
  fingerprint literal are checked against that audit by a Python regression.
- Focused native raw-reader and observation-policy compile/run: PASS with
  C++20 `-Wall -Wextra -Werror`. Standalone MinGW i686 compilation of the actual
  observer, including Windows CryptoAPI and world reader: PASS with those flags.
- `python3 tools/validate.py --jobs 4`: PASS; 111 C++ executables, 49 audit
  Python tests, 13 QuestDB Python tests, SQL/TSV fixture, 10 Lua fixtures, full
  MinGW build and diff check. All 247 validation records passed.
  Report: `/tmp/wow-validation-vvyf3kod/results.json`.
- Separate `cmake --build build`: PASS (DLL, testhost, loader and GUI).
- `git diff --check` and new-file `--no-index --check`: PASS. Reviewed the
  complete tracked diff and all three new files. Final status contains only
  this task's 11 files; no staging or commit.
- Source comparison with `71e2ed6`: no edits to WorldMonitor, bootstrap,
  WorldState, profile evidence/adapter, Grind or any protected gameplay owner.
- Runtime at implementation time: NOT RUN / PENDING. The later capture
  reconciliation above confirms fingerprint/raw observation/stop telemetry;
  completed unload and post-stop responsiveness remain unverified.

### Source research continuation — 2026-10-10

Starting checkpoint: `f10daba3d955808de2c55f789153ee2730cb5031`.
The exact executable offline audit was rerun successfully against
`/home/ludvig/Games/WoW Vanilla/WoW.exe` (same SHA256 recorded above).
All 43 instruction anchors and 12 strings still match. Its four location/lifetime
qualification flags and `runtimeObserved` correctly remain false: the offline
tool cannot incorporate runtime evidence. Manual disassembly additionally
narrows the preferred zone-updater GUID provenance:

| SOURCE VERIFIED bounded instruction path | Consequence / remaining limit |
|---|---|
| `0x5DB900` increments a counter and updates at threshold 10; lookup of `0xC4DA98/9C` with type mask 8 precedes the fallback active-player lookup with mask 0x10. Selected object +0xE0 and map getter feed `0x67E510`. | Cache updates are conditional and can use another source. Invocation count is not a freshness timestamp. |
| `0x6006E0` compares its input pair with `0xC4DA98/9C`; equality branches to return at `0x6006FB`. Stores at `0x600791/0x60079A` publish the input as separate DWORDs. The new-pair type-8 lookup at `0x600829` follows publication. | Published pair equality cannot establish a freshly validated object, atomic identity or world generation. |
| Caller `0x4958C3` reads the active GUID through `0x468550`, then passes it to the setter at `0x4958D1`. Another caller at `0x5EE477` passes the pair from the object's descriptor pointer `[esi+8]`, offsets +0/+4. | One active-player caller does not prove that all updater GUIDs belong to the current player. Alternate-object semantics remain UNKNOWN. |
| `0x6006C7/0x6006D1` zero the preferred pair in separate stores. | This is a clear path, not a monotonic generation or a proven pre-teardown invalidation boundary. |

These new paths were inspected with `objdump`, independently reconciled by the
coordinator and source researcher, and are not added to the offline tool's
43-anchor manifest. Reproduction commands (read-only):

```sh
objdump -d -Mintel --start-address=0x5db900 --stop-address=0x5db980 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x6006b0 --stop-address=0x600860 '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x4958c3 --stop-address=0x4958da '/home/ludvig/Games/WoW Vanilla/WoW.exe'
objdump -d -Mintel --start-address=0x5ee443 --stop-address=0x5ee48a '/home/ludvig/Games/WoW Vanilla/WoW.exe'
```

**INFERRED:** the runtime stale-cache and early-map observations are consistent
with the previously traced late zone/area reset and early manager publication.
They do not identify exact client instructions executing at each poll.
**SOURCE GAP:** no source-qualified lifetime/initialization predicate, serialized
sampling boundary, player-bound zone/area freshness or generation/ABA mechanism
was established. This bounded inspection is not an exhaustive writer/call-graph
audit. Root, GUID, candidate equality and server connection remain insufficient.
No production source, test, reader contract or controller ownership changed.

Next source task: trace caller/update scheduling and owner switch/destroy paths
to determine whether a defensible serialized read boundary exists; finish the
alternate-object ownership analysis before designing a location sampler. If no
boundary can be established, retain the explicit SOURCE GAP. Profile population
must remain blocked pending both source qualification and the independent
runtime cases listed above. A separate manual module-absence/responsiveness
observation is still needed to close the existing runbook's unload acceptance.

Reconciliation validation on 2026-10-10:

- Exact-client offline audit: PASS; additional disassembly manually reviewed
  independently. Capture hashes verified and all five documented rows/fixed
  qualification labels compared successfully with the saved retry log.
- `python3 tools/validate.py --jobs 4`: PASS, all 247 records; 111 C++
  executables, 49 audit Python tests, 13 QuestDB Python tests, 10 Lua fixtures,
  SQL/TSV fixture, full MinGW build and diff check.
  Report: `/tmp/wow-validation-gkxuejoo/results.json`.
- Separate `cmake --build build`: PASS (DLL, testhost, loader, GUI).
- Independent read-only source/evidence reviews: no corrections. Complete diff
  inspected; `git diff --check -- .`: PASS. Only this audit and AI_HANDOFF.md
  changed; no production source/tests changed and nothing staged or committed.
- Runtime: existing capture inspected; no new run and no qualification upgrade.

### P0.7.4 — scheduling, lifetime and alternate GUID continuation

Continued on 2026-10-10 from `5cd22ed596a9adceb5483d1b746d31730b36804b`,
branch `codex/p07-world-zone-preparation`, initially clean. Three parallel
read-only agents examined client lifetime/scheduling, alternate GUID ownership
and the repository sampling boundary. The coordinator independently reviewed
the bounded paths below. Only this audit and AI_HANDOFF.md changed during
research. The subsequent user-authorized checkpoint also updates AGENTS.md
with the permanent Git workflow; final checkpoint scope/status is in the handoff.

**Decision: SOURCE GAP remains; profile location population stays BLOCKED.**
The new evidence establishes an event-update path and identifies an alternate
GUID source through the exact client's PLAYER_FARSIGHT field. Neither yields
a qualified serialized acquisition or current-player zone/area cache. No
sampler, hook, native invocation, memory write or gameplay change is introduced.

#### SOURCE VERIFIED — publication and scheduling

All VAs refer to the same SHA256-pinned executable documented in P0.7.3.
These are bounded instruction facts, not a complete writer/indirect-call audit.

| Path | Observed ordering and remaining limit |
|---|---|
| Constructor `0x464FF0` | Root publication `0x46510E` and connection+0x1AD8 publication `0x465114` precede connection backpointer setup `0x465120`, handler registration `0x465125` and map setter `0x46512C`. Matching both manager pointers cannot certify initialization. |
| Destructor `0x467700` | Object cleanup `0x467711` precedes root clear `0x46771E`, connection+0x1AD8 clear `0x467734`, and conditional saved-owner clear `0x46773A–742`. The saved pointer is cleared only when equal to this manager; it supplies no generation. |
| Temporary owner switching | `0x464FB0` skips when the saved slot is nonzero or the supplied owner already equals the root; otherwise it saves the root at `0x464FC2` then assigns the supplied owner. `0x464FD0` restores a nonzero saved root before clearing the slot at `0x464FDF`. This is a single saved pointer, not a nesting/generation counter. |
| Packet wrapper `0x537C50` | Switch `0x537C59` via connection+0x1AD8 → state check → dispatch `0x537CA0` → restore `0x537CC9`. `0x537AA0` invokes a packet handler indirectly at `0x537AE5`. Packet callbacks can execute within temporary ownership; their complete thread/reentrancy closure is unproved. |
| Zone updater scheduling | Setup `0x40167A–684` registers callback `0x401EC0` in event slot 5 through `0x41FC90`. The callback calls `0x6C5D50`, then `0x5DB900` at `0x401ECE`. Teardown removes the same callback/slot at `0x401EFA–0x401F04` before marker reset and manager teardown. This removal path is not a registration. |
| Event context | Registration `0x41FCCC → 0x41DB90 → 0x436D80` obtains the calling thread's context through imported TlsGetValue (`0x7FF338`). This does not establish that every object/cache writer executes on that thread. |
| Event execution | `0x420F70` acquires context+0x10 at `0x420F80`, checks state+0x2C, and releases at `0x420F92`. When state differs from 1 it computes elapsed time and dispatches event 5 at `0x420FD5`. `0x4245B0` indexes context+0x5C+12*event and calls the registered callback at `0x4246AD`. That inspected lock is released before callbacks; it is not a proven world-lifetime read lock. |
| Transfer paths | `0x401B00` submits `0x401BC0` through `0x4200A0` at `0x401BAE`; another packet path calls `0x401BC0` directly at `0x401EA4`. Both deferred and direct execution need coverage before any sampling phase can be accepted. |

#### SOURCE VERIFIED — preferred GUID ownership

A linear disassembly search found five direct calls to `0x6006E0`. Each was
inspected; this inventory does not exclude indirect callers or other writers.

| Setter call | Input provenance and conditions |
|---|---|
| `0x4958D1` | Active GUID returned by `0x468550` at `0x4958C3`. |
| `0x5EE35B` | In `0x5EE290`, active-object GUID matches, its alternate GUID is nonzero, but alternate lookup fails. Input is the original object's descriptor GUID if `0x5FA5F0` succeeds, otherwise zero. |
| `0x5EE477` | Resolved alternate object becomes ESI at `0x5EE44A`; its descriptor type mask 0x8 and `0x5FA5F0` must pass. Its own descriptor GUID is passed to the setter. |
| `0x5EE573` | Fallback/reset branch in `0x5EE290` passes the original object's descriptor GUID if `0x5FA5F0` succeeds, otherwise zero. |
| `0x5FA696` | `0x5FA600` changes object+0xC58 bit 0x400. If its GUID matches the owner from `0x4818F0` at +0x88/+0x8C, enabling passes that object's GUID; disabling clears the preferred pair only if it still matches that object. |

`0x5FA5F0` reads object+0xC58 bit 0x400; no gameplay meaning for that bit is
assumed. At `0x5EE2B8–2C4`, the input object's descriptor GUID is compared
with the active GUID. On equality, `0x5EE2CC–2D8` reads the alternate pair
from `[object+0xE68]+0x830/+0x834`, then `0x5EE2EF` resolves it with mask 1.
The later setter branch checks the resolved object's type mask 0x8 and bit 0x400.
Equality with the separate owner GUID at `0x5EE383–396` can skip that branch.
Thus even this path's active-GUID check does not make the setter input the
active player's GUID.

The separate owner returned by `0x4818F0` is `[[0xB4B2BC]+0x65B8]`.
After preferred-pair publication, `0x5EE483 → 0x4841A0 → 0x50D0F0`
copies the selected descriptor GUID into that owner's +0x88/+0x8C at
`0x50D223/22C`. These are separate publications with intervening calls,
not an atomic owner switch. Another path compares an object's descriptor
GUID to the preferred pair at `0x5FBC26–37`, then calls the clear helper
`0x6006B0` at `0x5FBC3B`. Earlier helper calls precede that clear; no
pre-destruction invalidation contract is established.

Exact-client field provenance, including the relative descriptor base:

- `0x5DD2A0 → 0x5FAD10 → 0x613980` passes the same descriptor argument;
  `0x613989` stores it at object+8. `0x5DD2B5/2BB` stores descriptor+0x2F0
  at object+0xE68. The alternate pair is therefore descriptor+0xB20/+0xB24,
  not descriptor+0x830/+0x834.
- The 20-byte row at `0x83B484` contains name pointer `0x83C44C`
  (`PLAYER_FARSIGHT`), relative index `0x20C`, count 2, and remaining words
  4/2. `0x20C*4 = 0x830`. `0x47F8DF–8E9` supplies table `0x83AA48` and
  destination `0xB44A18` to `0x47F960`, whose loop expands each row's count
  into 20-byte records. The destination is `0xBC` records beyond `0xB43B68`,
  the base destination at `0x47F8CE`, consistent with descriptor byte base
  `0x2F0`. Counts preceding the PLAYER_FARSIGHT row sum to `0x20C`.

This identifies a named alternate-object field in the current binary. It does
not establish gameplay triggers, all field writers, observation-time ownership,
or a cache invalidation/freshness contract. Preferred-GUID equality suppression,
separate stores and delayed/conditional cache updates still apply.

#### Repository boundary and qualification

**SOURCE VERIFIED:** DllMain creates BootstrapThread (`dllmain.cpp:389–397`),
which calls WorldMonitor. Observe mode routes to
`ConnectionLifecycleObserver5875::Run`, acquiring connection/raw location,
WorldState, then raw location again, followed by Sleep(250). Those sequential
reads do not synchronize with the client event callback. Connection/location
reads use ReadProcessMemory; intervening WorldStateReader still uses
Core::Memory's readability-check-then-memcpy path. The whole observer must not
be described as having a fault-safe lifetime contract.

GameThreadDispatcher explicitly identifies its target as the window-owner
thread. Its WH_CALLWNDPROC/SendMessageA path and bookkeeping mutex do not prove
writer thread affinity, an eligible world phase, or exclusion of reentrant
transitions. Merely moving these reads into Invoke would not close the gap.
This is a qualification limit, not a demonstrated runtime dispatcher defect.
The pure profile adapter still has no qualified location/generation input or
XYZ-map association; no production reader or controller ownership changes.

**RUNTIME OBSERVED:** no new run. Preserve the earlier raw-observer RUNTIME
PASS, limited to the reconciled capture above. No manual module-absence or
post-stop responsiveness evidence was acquired; unload acceptance stays open.

**INFERRED:** the alternate field and owner comparisons are consistent with a
viewpoint-related source choice. That interpretation is not needed to reject
the cache as unconditional current-player evidence, and is not a gameplay claim.

**UNKNOWN / SOURCE GAP:** full writer/indirect-callback coverage, writer thread
affinity, reentrancy, initialization/invalidation across all transitions,
same-address/character/map ABA, and player-bound zone/area freshness. Event
registration, a released event-context lock, matching pointers/GUIDs, and window
thread dispatch are each insufficient. All candidates remain UNQUALIFIED.

Reproduction uses `objdump -d -Mintel --start-address=START
--stop-address=STOP '/home/ludvig/Games/WoW Vanilla/WoW.exe'` (stop exclusive):

| Inspection | START / STOP |
|---|---|
| Publication, switching; teardown | `0x464FA0 / 0x465198`; `0x467700 / 0x467954` |
| Packet handler and owner wrappers | `0x537AA0 / 0x537AFC`; `0x537C50 / 0x537D60`; `0x538020 / 0x538035` |
| Register, callback, remove | `0x401670 / 0x401689`; `0x401EC0 / 0x401F09` |
| TLS context and event dispatch | `0x41FC90 / 0x41FD70`; `0x41DB90 / 0x41DBAB`; `0x436D80 / 0x436DA3`; `0x420F70 / 0x420FE1`; `0x4245B0 / 0x424710` |
| Transfer submission/direct path | `0x401B00 / 0x401BC0`; `0x401DE0 / 0x401EB4` |
| Preferred setter callers | `0x4958C3 / 0x4958DA`; `0x5EE290 / 0x5EE590`; `0x5FA5F0 / 0x5FA6B0` |
| Separate owner publication and preferred clear | `0x4818F0 / 0x4818FC`; `0x4841A0 / 0x4841B6`; `0x50D0F0 / 0x50D2C4`; `0x5FBB60 / 0x5FBC60` |
| Descriptor base and field-table loader | `0x5DD2A0 / 0x5DD2C1`; `0x5FAD10 / 0x5FAD2E`; `0x613980 / 0x613993`; `0x47F8C4 / 0x47F8EE`; `0x47F960 / 0x47F9B9` |

Use `objdump -s` for row `0x83B484 / 0x83B498` and string
`0x83C44C / 0x83C45C`; `objdump -p` identifies the TLS/critical-section imports.
These additional manual paths are **not** added to the existing automated
43-instruction-anchor/12-string manifest. Its PASS still qualifies only its
bounded source scope, with all four location/lifetime flags false.

Next source task: close event-context execution/thread and reentrancy coverage
around `0x420F70/0x4245B0`, queued transfer `0x4200A0`, direct packet dispatch
and temporary-owner restoration. Trace writers/reset ordering for
PLAYER_FARSIGHT, object+0xC58 bit 0x400 and preferred GUID against zone/area
publication. Require a defensible lifetime/coherence/ABA contract before sampler
design; retain SOURCE GAP if unavailable. Manual unload acceptance remains a
separate evidence task. Future qualified consumption still needs loading,
teardown, same-map reload, map-transfer and zone/area-only runtime cases.

Continuation validation on 2026-10-10:

- Exact-client offline audit: PASS; additional bounded disassembly/data paths
  independently reviewed without expanding the automated manifest.
- `python3 tools/validate.py --jobs 4`: PASS, all 247 records; 111 C++
  executables, 49 audit Python tests, 13 QuestDB Python tests, 10 Lua fixtures,
  SQL/TSV fixture, full MinGW build and diff check.
  Report: `/tmp/wow-validation-wey3vd5_/results.json`.
- Separate `cmake --build build`: PASS (DLL, testhost, loader, GUI).
- Three read-only reviews: no factual corrections; expanded one reproduction
  range to include field-table base setup. Complete diff inspected and scoped
  `git diff --check -- .` PASS. Only the two documented files changed; nothing
  staged or committed.
- Runtime: NOT RUN in this task; prior raw-observer qualification preserved.

### P0.7.4 — queued transfer, packet dispatch and field notifications

Continued on 2026-10-10 from `fb9b574b5188e5774f8d5a5222351b0a6659853f`,
branch `codex/p07-world-zone-preparation`, initially clean. Three parallel
read-only agents traced scheduling, field writers and packet ownership. The
coordinator reconciled their reports, checked key instruction paths and traced
cache publication/suppression. This task changes only this audit and the handoff.

**Decision: SOURCE GAP remains; profile population stays BLOCKED.** The previous
source gaps are narrowed by concrete queue, callback and writer paths below.
No eligible sampling boundary, complete writer coverage, world generation or
current-player cache freshness was established. No production code, observer
contract, controller ownership or navigation changed.

#### SOURCE VERIFIED — local scheduling and nested dispatch

These facts use the same exact-client SHA256 as P0.7.3. Addresses describe
bounded disassembly; names such as queue/context are research interpretations
of the inspected structure, not debug symbols or runtime thread identities.

| Path | New evidence and qualification limit |
|---|---|
| Deferred transfer queue | `0x4200A0 → 0x428B30` stores callback at node+0x18 (`0x428C62`) and argument at +0x1C (`0x428C68`). Consumer `0x428510` loads callback at `0x428575`, checks due time at `0x4285EF–5F6`, removes a due entry, releases context+0x10 at `0x42864E`, invokes callback at `0x428659`, then reacquires at `0x4286C9`. The queue lock does not cover the transfer callback's manager replacement/loading. |
| Main loop order | In the inspected branch of `0x420C00`: due callbacks `0x420D0C` → optional message pump `0x420D23` → event 6 path `0x420D55` → queued event drain `0x420D5C` → event 5 path `0x420D63`. `0x420FF0` selects event 6 at `0x421016/1D`; event 5 reaches the previously traced zone updater. This is conditional local order, not a globally safe sampling phase. |
| Intervening callbacks | Event drain `0x424AD0` detaches the pending list while locked, releases at `0x424B6F`, then dispatches queued events at `0x424B8A`. Message pump `0x423920 → 0x42C9F0` reaches PeekMessageA (`0x42CB0A`) and DispatchMessageA (`0x42CB68`). These paths can run callbacks between due transfers and event 5; which handlers reenter world transitions remains unproved. |
| Context/thread scope | `0x420CD3–CE3` installs the selected context through TLS helpers. The loop has a direct entry through `0x41F5C0/0x420BE0` and a created-thread path: `0x421B5B/60 → 0x659AC0 → 0x64BD20/40`, CreateThread at `0x64BE9A`, wrapper invocation at `0x64BC5A`. This does not identify the thread/context owning all world writers. An analogous body `0x422540` has similar ordering, but these searches established no caller; it is not evidence of an active second pump or recursive execution. |
| Packet execution in event 6 | `0x402B84–8E` registers `0x403620` in event 6; `0x403624 → 0x5B3DB0` holds global critical section `0xC2A314` while walking connections and calling `0x538040`. The latter uses connection+0x1A54 and enters `0x5384D0`, which holds queue+4 from `0x5384E0` through handler execution to `0x538604`. Queued type 0x12 dispatches connection virtual slot +0x30 at `0x53853E`; inspected table entries `0x8090A0/0x80A3C8/0x80A688` bind it to `0x537C50`. These locks cover this bounded path, not every world/cache writer. |
| Transfer within temporary ownership | On the inspected packet chain, different-valid-map handler `0x401DE0` calls `0x401BC0` at `0x401EA4`, destroys manager at `0x401BF7` and constructs at `0x401C22`, before wrapper restore `0x537CC9`. Same-map branch skips that direct call. A nonzero saved-owner slot can affect restoration; the root after return cannot be assumed to be the new manager without qualifying prior context. No actual erroneous restoration was observed. |
| Nested handler dispatch | Registration `0x6038B6–C0` binds packet 0x2FB to `0x603CE0`. Its embedded-buffer loop obtains a connection via `0x5AB490` and calls `0x537AA0` again at `0x603DA6`. This establishes nested handler dispatch in source, not nested entry to the switch/restore wrapper, a particular embedded transfer packet, or observed runtime reentrancy. |

The context lock released before event 5, the timed-queue lock released before
callbacks, and the connection locks held during packet dispatch have distinct
scopes. None is an authorized reader lock. A source path that runs before event 5
cannot establish that all initialization, deferred work and notification delivery
have completed by event 5, especially with conditional updater/cache suppression.

#### SOURCE VERIFIED — field writers, cleanup and cache publication

| Path | New evidence and qualification limit |
|---|---|
| Farsight subscription | `0x5DDA30–4F` registers callback `0x5DE0D0`, GUID, category 4, relative offset 0x830 and length 8 through `0x467E70`. Its category translation through `0x465690` adds base 0x2F0, covering descriptor+0xB20/+0xB24. `0x5DE708–723` requests removal through `0x467FB0`; removal can defer via node+0x2D when node+0x2C is nonzero (`0x468023–057`). Registration/removal is not proof of callback completion or lifetime. |
| Farsight notification body | `0x5DE0D0` resolves the notified GUID with mask 0x10, checks active GUID, reads PLAYER_FARSIGHT, mask-1 resolves the alternate object, then calls `0x5EE270` (`0x5DE144`) or `0x5EE590` (`0x5DE15D`). This supplies a concrete field-notification path; completion ordering against field writes and destruction remains open. |
| Generic descriptor mutation | Masked loop `0x466590` calls `0x466A00` at `0x4666F0`, which calls `0x6142E0`. Store `0x6142EC` writes one DWORD to `[object+8]+index*4`; index increments at `0x4666FB`. PLAYER_FARSIGHT indices 0x2C8/0x2C9 can be separate loop iterations. No coherent two-DWORD publication follows from this path; displacement searches cannot exclude generic/bulk writers. |
| Preferred-GUID invalidation | In `0x5FB5E0`, several cleanup calls precede GUID comparisons `0x5FB631/63C`, then equality clears the pair at `0x5FB650/655`, bypassing `0x6006B0`. Wrapper `0x5FB1D0` calls this destructor before conditional free at `0x5FB1EF`. Thus this clear precedes that free but follows earlier cleanup. Initialization also clears at `0x6039A9/9AF`. The linear direct-store inventory now contains four pairs: these two, `0x6006C7/6D1`, and `0x600791/79A`; it is not an exhaustive alias/bulk-writer proof. |
| Flag mutation ingress | Object+0xC58 is initialized to zero at `0x5FAE7A`. Setter `0x5FA600` changes bit 0x400. One direct caller `0x5DEB56` supplies true; handler `0x603EA0`, registered for packet 0x159 at `0x6038C6–D0`, parses a GUID and byte, mask-8 resolves and calls the setter at `0x603EEB`. The packet/flag gameplay names remain UNKNOWN; neither is a world-ready predicate. |
| Cache suppression and publication | Branch helper `0x67E670` returns false at `0x67E6A0–6BC` when mode `0x868608` is positive and spatial helper output equals `0x86860C`; that comparison contains no map/GUID binding. False skips numeric writer via `0x67E602`. Other branch `0x67E7F0` writes deduplication map/zone/area markers at `0x67E835/83F/845` before helper calls and numeric publication at `0x67E645 → 0x494780`. Neither marker set certifies completed numeric publication or world generation. |

#### Evidence limits, reproduction and next task

**RUNTIME OBSERVED:** no new run, capture inspection, attach, input, native call
or memory write. Preserve the prior raw-observer RUNTIME PASS within its saved
capture limits. All location candidates remain UNQUALIFIED. Unload acceptance,
vendor episode and water emergency egress remain runtime pending; reconnect
remains unimplemented and R0.1 remains open.

**INFERRED:** released callback locks, per-field stores and late clears explain
why the newly traced ordering is insufficient as a sampling contract. They do
not prove a torn sample, erroneous restore or gameplay failure occurred in WoW.

**UNKNOWN / SOURCE GAP:** actual world-writer context/thread ownership, complete
writer and callback coverage, field-notification completion/removal ordering,
window/loading reentrancy, initialization/invalidation, pair/sample coherence,
same-address/character/map ABA, and current-player zone/area freshness. No
source-qualified sampler design or hook is justified by this checkpoint.

Reproduce with `objdump -d -Mintel --start-address=START --stop-address=STOP`
against `/home/ludvig/Games/WoW Vanilla/WoW.exe` (stop exclusive):

| Inspection | START / STOP |
|---|---|
| Transfer enqueue/consume | `0x4200A0 / 0x420164`; `0x428B30 / 0x428CF8`; `0x428510 / 0x4286F7` |
| Pump order, event 6, queued events/messages | `0x420C00 / 0x420E1F`; `0x420FF0 / 0x421026`; `0x424AD0 / 0x424BD8`; `0x423920 / 0x423995`; `0x42C9F0 / 0x42CBE4` |
| TLS installation/thread entry | `0x41DB50 / 0x41DBCA`; `0x436DB0 / 0x436DBB`; `0x421B52 / 0x421B65`; `0x659AC0 / 0x659AE7`; `0x64BD20 / 0x64BEA0`; `0x64BC20 / 0x64BC5F` |
| Packet pump and nested dispatch | `0x402B79 / 0x402B93`; `0x403620 / 0x403634`; `0x5B3DB0 / 0x5B3DF2`; `0x538040 / 0x53804B`; `0x5384D0 / 0x538624`; `0x6038B5 / 0x6038D5`; `0x603CE0 / 0x603DFB` |
| Field subscription/notification/removal | `0x5DDA30 / 0x5DDA54`; `0x5DE708 / 0x5DE728`; `0x467E70 / 0x468062`; `0x465690 / 0x4656B1`; `0x5DE0D0 / 0x5DE180` |
| Generic field mutation | `0x466590 / 0x466704`; `0x466A00 / 0x466A11`; `0x6142E0 / 0x6142F8` |
| Cleanup, initialization and flag ingress | `0x5FB5E0 / 0x5FB670`; `0x5FB1D0 / 0x5FB1FB`; `0x6033C0 / 0x6039FB`; `0x5FAD50 / 0x5FAE80`; `0x5DEB49 / 0x5DEB6A`; `0x603EA0 / 0x603EFB` |
| Cache branch and marker ordering | `0x67E510 / 0x67E668`; `0x67E670 / 0x67E7E8`; `0x67E7F0 / 0x67E935` |

Use `objdump -s` for field-category table `0x4656B4 / 0x4656D4` and the three
packet virtual-slot entries listed above (4 bytes each); `objdump -p` identifies
critical-section, TLS, thread and window-message imports. Manual paths are not
added to the automated 43-anchor/12-string manifest. Its four location/lifetime
qualification flags and runtimeObserved remain false.

Next task: connect the masked descriptor-update loop to notification execution,
including deferred removal, and prove which event context executes it. Trace
context creation/selection through `0x421430` and window-context assignment
against event 6, event 5 and transfer callbacks. A qualified sampling proposal
must cover loading/window callbacks, initialization/invalidation and ABA; retain
SOURCE GAP if closure is unavailable. Manual unload acceptance is separate.

Validation: exact-client audit PASS; `python3 tools/validate.py --jobs 4` PASS,
247 records (111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests,
10 Lua fixtures, SQL/TSV fixture, full MinGW build and diff check), report
`/tmp/wow-validation-o3yxzmhz/results.json`. Separate `cmake --build build`
PASS (DLL, testhost, loader, GUI). Three read-only reviews found no actionable
corrections. Full diff reviewed; `git diff --check` PASS. Worktree changes are
exactly the audit and handoff documents, with no unrelated files.
No new runtime qualification follows from these results.

## Regression and validation

leveling_profile_selector_test.cpp covers level boundaries and gaps, level
jumps, invalid/missing levels, known map zero, wrong/unknown maps, mapless
segments, unsupported zone/faction matching, race/class unknown/mismatch,
identity/freshness failures, missing/nonfinite/map-unassociated positions,
inclusive spatial boundaries, overlaps, unresolved contenders, input-order
independence, preparation truth states, explicit completion, optional scope,
invalid catalogues/links/cycles, independent segments, shared successors,
transition repetition/loss/restoration/context changes/reset, and const-input
purity. It compiles without linking any runtime controller or platform API.

Validation on 2026-10-09:

- Focused native C++20 test: PASS with -Wall -Wextra -Werror.
- New test/header MinGW i686 cross-compilation: PASS with the same warnings.
  This separately compiles the new headers because the DLL intentionally does
  not include them yet.
- python3 tools/validate.py --jobs 4: PASS, 109 C++ test executables,
  42 audit Python tests, 13 QuestDB Python tests, SQL/TSV fixture, 10 Lua
  fixtures, full MinGW build and diff check. All 243 validation records passed.
- Results: /tmp/wow-validation-617lk51_/results.json.
- Separate cmake --build build: PASS for DLL, testhost, loader and GUI.
- All eight protected files were compared byte-for-byte with HEAD: unchanged.
- New-file diff review and whitespace checks: PASS. git diff --check does not
  cover untracked files, so each added file was also checked with --no-index.
- Final intended status: only these five headers, the focused C++ test and
  this document are untracked; no tracked files changed or staged. No commit.

Runtime: NOT RUN / PENDING; no WoW process is launched by this checkpoint.

## Remaining gaps

- Qualified current-map, zone/area, world-generation and faction-group readers.
- A coherent sampler that binds identity, position-map association, race/class
  and named owner facts with explicit freshness. The new gate is a contract,
  not a replacement for that work.
- Production profile data, source provenance validation for catalogue
  references, an external format/loader and reload/version semantics.
- Observe-only runtime hook, telemetry and WoW qualification.
- Owner-safe quest/grind integration, target restriction enforcement and
  completion/preparation fact producers.
- Automatic zone travel, travel readiness, service/class progression and
  unattended Orc 1-10 / 1-60 remain unimplemented.

This foundation can merge independently of reliability ownership code. It
does not change existing gameplay behavior or qualify unrelated runtime work.

VENDOR EPISODE — RUNTIME PENDING

WATER EMERGENCY EGRESS — RUNTIME PENDING

SOURCE GAP — RECONNECT NOT IMPLEMENTED
