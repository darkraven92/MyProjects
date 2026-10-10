# P0.7 — World / Zone Preparation + Leveling Profile Foundation

This checkpoint implements isolated, advisory policies and synthetic regression
tests. It does not integrate profile selection into the running bot, add a live
map/zone reader, or qualify any new behavior in WoW.

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
