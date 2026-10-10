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

### P0.7.4 — notification completion and startup context

Continued on 2026-10-10 from `7ede98b579414841c6112092e2d0ad05904dae7d`,
branch `codex/p07-world-zone-preparation`, initially clean. Three parallel
read-only agents traced notification completion, startup context and packet
ownership. The coordinator checked key instruction paths and loading callbacks.
Only this audit and the handoff change; no production code or observer contract
changes. The same exact-client SHA256 and evidence limits apply.

**Decision: SOURCE GAP remains; profile location population stays BLOCKED.**
Notification return/removal is now connected to a packet pass, and startup is
connected to world setup. Neither establishes complete callback coverage,
world lifetime, coherent samples, generation or current-player cache freshness.

#### SOURCE VERIFIED — packet completion and removal scopes

| Path | New evidence and qualification limit |
|---|---|
| Registered update handlers | `0x465140` registers handlers on the current manager's connection at +0xD0: packet 0xA9 to `0x4651A0` (`0x46514D–157`), 0x1F6 to `0x4672F0` (`0x46516A–174`), and 0xAA to `0x4674A0` (`0x465187–191`). The 0x1F6 path calls `0x660740` at `0x467361`, then directly forwards its buffer to `0x4651A0` at `0x4673B6`; this is not another switch/restore-wrapper entry. |
| Mutation before notification | `0x4651A0` calls mutation passes through `0x466010` at `0x46520E/222`; its type-0 route reaches `0x467160 → 0x466590` at `0x46720F`. It resets the buffer cursor at `0x465231`, then runs the notification pass: type 0 calls `0x465330` at `0x465277`; types 2/3 enter `0x465C50`, which can call `0x465330` at `0x465D8E`. Completion is not an immediate consequence of each DWORD store. |
| Saved subscribed bytes | Before per-DWORD mutation, subscription paths in `0x466590` call `0x4667A0`; it copies subscribed ranges from object+8 storage into object+0xC saved storage at `0x466801–81D`. This supplies the comparison baseline, not an immutable external snapshot or lifetime guarantee. |
| Listener dispatch | `0x465330` parses the field mask and calls `0x465570` at `0x4654A4`. The latter sets listener+0x2C to 1 at `0x46559E`, compares the subscribed bytes from the object's current/saved descriptor storage at `0x4655BB`, and invokes listener+0x10 at `0x4655DC` only on inequality. This reaches the previously traced farsight subscription; it does not certify coherent publication to a concurrent reader. |
| Deferred listener removal | After callback return or equal-byte suppression, `0x4655F9` reads listener+0x2D. If pending, it unlinks and frees the listener through `0x465600–66A`; otherwise it clears +0x2C at `0x465671`. This closes the bounded deferred-removal path from `0x467FB0/0x468057`. Callback-triggered destruction, nested traversal and every cleanup path still require coverage. |
| Packet-owner scope | On the inspected normal queue path, `0x537C50` switches ownership before `0x537AA0` dispatch. A9 mutation, notification and its final root-list drain execute before restore at `0x537CC9`. The drain reloads current root at `0x465290`, walks manager+0xB8 and calls virtual +0xC at `0x4652FF`. Inspected player/unit vtables `0x80AF78/0x80C4F8` bind this slot to `0x5DEC90/0x5FBE00`, further update hooks rather than an established destructor. Notification return, drain completion and owner restoration are separate milestones, not a world generation. Prior saved-owner state and indirect callbacks remain relevant. |
| Object-removal ingress | A9's initial type-4 branch calls `0x465EC0` at `0x4651E9`, before mutation passes. It can call GUID removal `0x464920` at `0x465F4F/5FA6`; packet AA also calls it at `0x4674CF`. This removal path is distinct from listener-node removal. |
| Client-internal object deferral | `0x464920 → 0x614F00` checks object+0xE8. Nonzero calls `0x614F50(true)`, setting object+0xE4 mask 0x10, then returns at `0x46495A`. GUID helper `0x4683E0` increments the counter through `0x614F10`; `0x468410` decrements, checks zero and pending, then retries removal at `0x46844A`. Counter stores are ordinary DWORD operations. These helpers mutate client state and are not an observe-only acquisition mechanism. |
| Teardown limit | Manager teardown `0x467800` calls subscription cleanup `0x464C40` at `0x4678B9`, virtual destruction at `0x4678FE/908`, then a selected free at `0x46791B`. These inspected calls do not establish that teardown honors the separate object counter or waits for all callbacks. No runtime lifetime failure is claimed. |

#### SOURCE VERIFIED — startup context and loading callbacks

| Path | New evidence and qualification limit |
|---|---|
| Concrete scheduler startup | `0x4027F1/F3/F8` calls `0x41F550 → 0x4219D0` with EDX=0, ECX=1. Count decrement `0x421AA0` and zero branch `0x421AA6 → 0x421C1E` bypass the additional-worker loop on this startup path. The previously found worker creation is general capability, not evidence that this path creates more scheduler workers. Other client threads are outside this claim. |
| Context construction | `0x402812–825` supplies ECX=1, setup `0x402840`, cleanup `0x403640` through `0x41F5E0 → 0x422090`. Constructor writes context+0x44 mask 0x2 at `0x422204`, stores allocated TLS data at +0x208 (`0x42221C/227`), registers setup as event 7 (`0x42225E/265`), cleanup as event 4 (`0x422279/280`), and submits context at `0x422287`. The flag enables the inspected message-processing branch; it is not world readiness. |
| Selection and setup | `0x4222A0` assigns the context identifier and inserts it into a scheduler-slot heap (`0x422313–3AC`, `0x422407–473`). Selection `0x421430` removes a heap entry under its slot lock, releases at `0x421551`, returns at `0x421557`. After selected-context TLS installation, `0x420E20` sets context+0x44 mask 1 and dispatches event 7 at `0x420E45`. Context identity is not a qualified world generation. |
| World initialization inside event 5 | Setup calls `0x402AD0` at `0x4029EA`; that invokes `0x46A400` at `0x402B36`, registering `0x46B930` in event 5 (`0x46A52D/532/537`). Its conditional state-8 branch (`0x46C1B8`) calls world setup `0x401570` at `0x46C236`. Setup constructs the manager at `0x4015F3` and registers zone updater `0x401EC0` in event 5 at `0x401684`. Setup also registers packet pump event 6 at `0x402B84–8E`. These explicit paths use the current TLS context; complete intervening-helper effects remain unproved. Event 5 can itself initialize the world, so its label is not a post-initialization sampling boundary. |
| Loading callback coverage | Transfer `0x401BC0` calls `0x443D30` at `0x401BD9` before manager destruction; after reconstruction, loading `0x66FBE0 → 0x6941F0` calls it at `0x69447A`. `0x443D30` calls registered entries at `0x443D5A` and loops through `0x443E70` at `0x443D84`. That helper releases its queue lock at `0x443EF1` before entry callback `0x443EF9`, and invokes further registered callbacks at `0x443F2B`. These are additional callback-coverage obligations, not proof of world reentry. |
| Loading progress callbacks | Transfer installs `0x407F70/0x407FA0` through setters `0x443620/0x66FC70` at `0x401C38/44`. The loader can invoke the latter at `0x69444F/46A`; the former can run at `0x443DDF`. Their conditional path calls `0x406900`, which reaches global `0xC0ED38` virtual +0x68 through `0x58A960`. Indirect-target and window-message effects are not fully resolved. |

#### Evidence limits, reproduction and continuation

**RUNTIME OBSERVED:** no new run, capture inspection, attach, input, native call
or memory write. Preserve the prior raw-observer RUNTIME PASS within its saved
capture limits. Location candidates remain UNQUALIFIED, profile location BLOCKED,
unload acceptance open, vendor/water runtime pending, reconnect unimplemented,
and R0.1 open. Source findings do not extend runtime qualification.

**INFERRED:** the concrete startup chain narrows context ownership, and the later
notification pass explains why returning from a descriptor store is insufficient.
Neither the local callback order nor client-internal deferral proves a safe
external read phase. No torn sample, bad restoration or gameplay fault was observed.

**UNKNOWN / SOURCE GAP:** complete notification/destruction/reentrant traversal
coverage; saved descriptor lifetime; object-counter scope across manager teardown;
window-owner creation/assignment and all helper TLS effects; indirect loading
and world writers; sample coherence, initialization/invalidation, same-address/
character/map ABA, and player-bound zone/area freshness. No new hook, native call
or sampler design is qualified.

Reproduce with `objdump -d -Mintel --start-address=START --stop-address=STOP`
against `/home/ludvig/Games/WoW Vanilla/WoW.exe` (stop exclusive):

| Inspection | START / STOP |
|---|---|
| Handler registration, passes, notification | `0x465140 / 0x465310`; `0x465330 / 0x465690`; `0x465C50 / 0x465EA0`; `0x466010 / 0x46609C`; `0x467160 / 0x46722B`; `0x4672F0 / 0x46742A`; `0x466630 / 0x466704`; `0x4667A0 / 0x466830`; `0x467FB0 / 0x468062`; `0x5DEC90 / 0x5DECC0`; `0x5FBE00 / 0x5FBE74` |
| Removal and counter scopes | `0x465EC0 / 0x465FC7`; `0x4674A0 / 0x4674D4`; `0x464920 / 0x464979`; `0x614F00 / 0x614F7A`; `0x4683E0 / 0x468456`; `0x467800 / 0x467954` |
| Startup arguments and forwarding | `0x4027EC / 0x402835`; `0x41F550 / 0x41F580`; `0x4219D0 / 0x421C25`; `0x41F5E0 / 0x41F5F8` |
| Context construct/select/initialize | `0x422090 / 0x4224E3`; `0x421430 / 0x42155F`; `0x420E20 / 0x420E51` |
| Setup and event-5 world creation | `0x4029E5 / 0x4029F5`; `0x402AD0 / 0x402B93`; `0x46A400 / 0x46A53C`; `0x46C1B8 / 0x46C247`; `0x401570 / 0x401689` |
| Loading and indirect callbacks | `0x401BC0 / 0x401D05`; `0x66FBE0 / 0x66FC7D`; `0x6941F0 / 0x694495`; `0x443620 / 0x44362D`; `0x443D30 / 0x443F44`; `0x407F70 / 0x407FC4`; `0x406900 / 0x40691C`; `0x58A960 / 0x58A96F` |

Use `objdump -s` for jump table `0x465314 / 0x46532C` and vtable rows
`0x80AF78 / 0x80AF88`, `0x80C4F8 / 0x80C508`. These additional
manual paths are not added to the automated 43-anchor/12-string manifest.
All four qualification flags and runtimeObserved remain false.

Next task: trace callback-triggered object/manager destruction against listener
cleanup and saved-descriptor lifetime, including the scope/callers of
`0x4683E0/0x468410` and teardown `0x467800`. Resolve loading callback targets
and window-owner assignment against the startup context, including event-5
world initialization. Require complete lifetime/coherence/ABA coverage before
sampler design; retain SOURCE GAP if closure is unavailable. Manual unload
acceptance remains separate and requires module-absence/responsiveness evidence.

Validation: exact-client offline audit PASS; `python3 tools/validate.py --jobs 4`
PASS, all 247 records (111 C++ executables, 49 audit Python tests, 13 QuestDB
Python tests, 10 Lua fixtures, SQL/TSV fixture, full MinGW build and diff check).
Report: `/tmp/wow-validation-3aepci12/results.json`. Separate `cmake --build build`
PASS (DLL, testhost, loader, GUI). Three read-only reviews completed; corrected
equal-byte-suppression wording, with no remaining actionable findings. Full diff
and worktree status inspected: exactly these two documents, no unrelated changes;
`git diff --check` PASS. Runtime NOT RUN; no qualification upgrade.

### P0.7.4 — descriptor lifetime, bulk teardown and window association

Continued on 2026-10-10 from `5cbd7834402e38ec4115b27a6507ccb1d8802132`,
branch `codex/p07-world-zone-preparation`, initially clean. Three parallel
read-only agents investigated descriptor/listener lifetime, object deferral and
window/TLS association. Coordinator reconciled the results, checked key paths
and resolved the loading progress virtual targets. Only this audit and the
handoff change. The same exact-client hash and bounded-source methodology apply.

**Decision: SOURCE GAP remains; profile location population stays BLOCKED.**
Saved descriptor ownership, bulk cleanup and window creation are now narrower
source facts. No complete callback exclusion, thread affinity, coherent sampling,
world generation or player-bound location freshness was established.

#### SOURCE VERIFIED — allocation and teardown

| Path | New evidence and qualification limit |
|---|---|
| Player descriptor storage | Type-4 construction `0x466D04–45` compares the active GUID and selects size 0x1408 for it, 0x798 otherwise. It passes live base object+0x1D70 and saved base object+0x1D70+size through `0x5DD2A0 → 0x5FAD10 → 0x613980`; `0x613989/98C` store object+8/+0xC. The inspected active-player saved base is object+0x3178, inside the player allocation. It is not an independently retained snapshot. |
| Allocation and free | Active-player allocation `0x466E1F–3F` requests 0x3178 + 4*`0x47D540()` + `0x467040()` bytes; the last helper returns 0x2550. Teardown's active-GUID branch frees the object allocation at `0x46791B`, or in its second pass at `0x467A06`, including the inline saved storage. This establishes storage association, not retention during arbitrary callbacks. |
| Bulk listener cleanup | `0x464C40` enumerates category lists, calls `0x464EC0(0)` and then frees the listener through `0x646430`; player examples are `0x464CFF → 0x464D0E` and active-player extended lists `0x464D53 → 0x464D62`. `0x464EC0–0x464F40` unlinks both intrusive links without checking listener+0x2C/+0x2D. Ordinary GUID removal calls bulk cleanup at `0x464974`; manager teardown at `0x4678B9/0x4679A4`. This path differs from the single-listener pending-removal protocol. Callback reachability to bulk cleanup remains unproved. |
| Destructor forwarding | Inspected player slot 0 follows `0x5DD500 → 0x5DD600 → 0x5FB5E0 → 0x613B40`; unit slot 0 follows `0x5FB1D0 → 0x5FB5E0 → 0x613B40`. Calls at `0x5DD506`, `0x5DD84E`, `0x5FB1D6`, `0x5FB825` bind these paths. In the inspected teardown, bulk listener cleanup precedes this chain; preferred-GUID clears `0x5FB650/655` occur inside it, before the selected outer allocation free. |
| Mutable traversal nodes | `0x465330` builds a stack-local list at `0x4653E6–404`. `0x465970` moves existing subscription nodes into it using the secondary link: unlink `0x4659D4–0x465A02`, insert `0x465A09–54`. It does not clone them. After callback `0x4655DC`, `0x4655DF–5F9` reads the same node's links/pending flag. Nested traversal or destructive callback reachability must be resolved before this becomes a lifetime argument. |
| Counter callers | A linear direct-call scan finds acquisition `0x4683E0` at `0x5F3B27` in `0x5F3930`, releases `0x468410` at `0x5F4114/0x5F41D9` in `0x5F40E0/0x5F4120`. GUIDs come through controller+4 then object+8. Acquisition skips state values 1/3/6 and stores the remembered state at +0x14; release also skips -1. Release diagnostics reference `ObjectClient\GameObject_C.cpp` at `0x860890`. These inspected users are GameObject-related state logic, not an established notification guard. Indirect callers and other counter writers remain open; helpers mutate client state. |
| Teardown extends beyond the prior range | `0x467800` has a second object pass: cleanup `0x4679A4`, virtual destruction `0x4679E9/9F3`, selected free `0x467A06`. Only afterward are A9/1F6/AA handlers removed through `0x537A80` at `0x467A8F/AA5/ABA`; that helper clears handler/context slots at `0x537A88/8C`. Handler removal is not a pre-teardown invalidation boundary. The inspected body supplies no counter-based callback exclusion proof. |

#### SOURCE VERIFIED — loading and window/TLS association

| Path | New evidence and qualification limit |
|---|---|
| Graphics object selection | `0x589AC0/0x589B20` select factories by argument: 1 reaches `0x58BA70 → 0x598CE0`, publishing vtable `0x809EF8` at `0x598D03`; 0 reaches `0x58DD70 → 0x58DDA0`, publishing `0x809AF8` at `0x58DEE7`. Factory return is stored in `0xC0ED38`. These inspected constructors narrow indirect targets, not every possible vtable/global writer or the live backend. |
| Loading progress targets | The previously traced `0x58A960` virtual +0x68 selects `0x5A1910` or `0x59BA10` through table entries `0x809F60/0x809B60`. These call further virtual +0x14 at `0x5A1943/0x59BA2B`; inspected slots `0x809F0C/0x809B0C` bind `0x59A870/0x5956F0` (the former calls the latter at `0x59A88C`). Backend/helper indirect calls remain. Resolving these targets does not prove absence of reentry or qualify a sampling phase. |
| Window callback installation | Startup `0x4027E7 → 0x63A230` passes `0x42CFE0` at `0x63A477/4C4` into `0x589AC0`. Virtual +0x20 entries `0x599A60/0x58CBB0` forward to `0x591F80` at `0x599B05/0x58CC1C`, storing callback at object+0xF28 (`0x591F88`). Window procedures `0x598750/0x58C6D0` are registered through `0x599B68/BA6` and `0x58CC88/CC6`; their inspected forwarding branches invoke object+0xF28 at `0x5989B3/0x58C9E0`. This binds the startup callback to window dispatch, not all downstream effects. |
| HWND creation and assignment | Backend +0x28 paths create a window: `0x599C90 → 0x598B50`, CreateWindowExA at `0x598C88`, stores object+0x3894 at `0x598C90`; `0x58CE40 → 0x58CF10`, CreateWindowExA at `0x58D046`, stores returned HWND at object+0x39E4 (`0x58CEA6`). Getter `0x589C90` uses virtual +0x3C; entries `0x809F34/0x809B34` select `0x599E80/0x58D2A0`. Startup `0x63A6E7/6EE` stores the getter result in global `0x884EA0` through `0x435C50`. This precedes scheduler/context creation at `0x4027F8/0x402825`; HWND assignment is not itself context assignment. |
| Separate worker shares context TLS | Context setup `0x402840` calls `0x443300` at `0x40285D`. It registers `0x443E70` in event 6 at `0x443314`, captures the current TLS block through `0x41DB60` at `0x443331`, stores it in `0x885680` at `0x44333B`, and requests worker `0x443360` through `0x659AC0` at `0x443356`. The worker loads that block at `0x443369` and installs it with `0x41DB50` at `0x443370`. On successful creation a separate worker can use the same TLS block. Thus one scheduler slot does not mean one client thread, and shared context identity cannot identify the executing thread. It releases the queue lock at `0x44340D`, executes `0x648460` at `0x443431` with an optional item lock, reacquires at `0x44346C` and marks completion at `0x4434C9`. This does not establish that the worker writes world location. |

#### Evidence limits and continuation

**RUNTIME OBSERVED:** no new run, capture inspection, attach, input, native call
or memory write. Preserve the prior raw-observer RUNTIME PASS within its saved
capture limits. All location candidates remain UNQUALIFIED; profile location
BLOCKED. Unload acceptance remains open, vendor/water runtime pending, reconnect
unimplemented, and R0.1 open. No production code, observer contract, controller
ownership or navigation changed.

**INFERRED:** shared nodes and inline saved storage explain why a local pending
flag cannot alone establish lifetime through bulk teardown. Shared TLS explains
why context equality cannot substitute for thread ownership. These are proof
limits; they do not show that concurrent teardown, destructive callback reentry,
a torn read or any gameplay failure occurred.

**UNKNOWN / SOURCE GAP:** callback-to-cleanup/nested-traversal reachability,
complete counter and writer coverage, worker/completion callback effects,
window dispatch/TLS interaction, all loading indirect targets, object/manager
retention, initialization/invalidation, sample coherence, same-address/character/
map ABA and player-bound zone/area freshness. No sampler or native invocation
is qualified by this checkpoint.

Reproduce with `objdump -d -Mintel --start-address=START --stop-address=STOP`
against `/home/ludvig/Games/WoW Vanilla/WoW.exe` (stop exclusive):

| Inspection | START / STOP |
|---|---|
| Descriptor placement/allocation | `0x466C70 / 0x466DB7`; `0x466E00 / 0x466E4B`; `0x467040 / 0x467046`; `0x5DD2A0 / 0x5DD2D6`; `0x5FAD10 / 0x5FAD43`; `0x613980 / 0x613993` |
| Bulk cleanup and teardown | `0x464C40 / 0x464E98`; `0x464EC0 / 0x464F43`; `0x467800 / 0x467AC6`; `0x537A80 / 0x537A97` |
| Destructors and shared traversal | `0x5DD500 / 0x5DD52B`; `0x5DD600 / 0x5DD85C`; `0x5FB1D0 / 0x5FB1FB`; `0x5FB5E0 / 0x5FB831`; `0x613B40 / 0x613CED`; `0x465330 / 0x465686`; `0x465970 / 0x465A7E` |
| Counter callers | `0x5F3930 / 0x5F3B3D`; `0x5F40E0 / 0x5F41E1`; `0x4683E0 / 0x468456`; `0x614F00 / 0x614F7A` |
| Graphics factories and progress | `0x589AC0 / 0x589B7C`; `0x58BA70 / 0x58BA94`; `0x598CE0 / 0x598D13`; `0x58DD70 / 0x58DF10`; `0x58A960 / 0x58A96F`; `0x59BA10 / 0x59BA69`; `0x5A1910 / 0x5A1A2F`; `0x59A870 / 0x59A891` |
| Window setup and callback | `0x4027D0 / 0x402835`; `0x63A230 / 0x63A71A`; `0x599A60 / 0x599BB2`; `0x58CBB0 / 0x58CCD2`; `0x591F80 / 0x591F99` |
| Window creation/forwarding/getters | `0x599C90 / 0x599D30`; `0x598B50 / 0x598CB7`; `0x58CE40 / 0x58D052`; `0x598750 / 0x5989D7`; `0x58C6D0 / 0x58CA10`; `0x589C90 / 0x589C9B`; `0x599E80 / 0x599E87`; `0x58D2A0 / 0x58D2A7`; `0x435C30 / 0x435C57` |
| Shared TLS worker | `0x443300 / 0x443510`; `0x41DB50 / 0x41DB6B`; `0x436D80 / 0x436DB9`; `0x659AC0 / 0x659AE7`; `0x64BC20 / 0x64BC70`; `0x64BD20 / 0x64BED0` |

Use `objdump -s` for diagnostic `0x860890 / 0x8608D4`
and the listed vtable entries (4 bytes each); window creation slots are in
`0x809AF8 / 0x809B40`, `0x809EF8 / 0x809F40`. `objdump -p` identifies the
window, thread and TLS imports. These manual paths do not expand
the automated 43-anchor/12-string manifest; its four qualification flags and
runtimeObserved remain false.

Next task: trace registered field callbacks into cleanup/nested notification,
starting from farsight `0x5DE0D0`, and enumerate loading worker/completion paths
from `0x443300/0x443360/0x443E70`. Connect the installed `0x42CFE0` callback to
window dispatch, TLS and world-transition effects. Establish reachability or
exclusion while listener/object references are live before proposing a sampler;
retain SOURCE GAP if incomplete. Manual unload acceptance remains separate.

Validation: exact-client offline audit PASS; `python3 tools/validate.py --jobs 4`
PASS, all 247 records (111 C++ executables, 49 audit Python tests, 13 QuestDB
Python tests, 10 Lua fixtures, SQL/TSV fixture, full MinGW build and diff check).
Report: `/tmp/wow-validation-ynkotj4a/results.json`. Separate `cmake --build build`
PASS (DLL, testhost, loader, GUI). Three read-only reviews found no actionable
corrections. Full diff/status inspected: exactly two task documents, no unrelated
changes; `git diff --check` PASS. Runtime NOT RUN; no qualification upgrade.

### P0.7.4 — script dispatch, loading completion and deferred close

Continued on 2026-10-10 from `d67c9ba4a5e2a584cd0442056d3ac8df333dd9cc`,
branch `codex/p07-world-zone-preparation`, initially clean. Three parallel
read-only agents traced farsight callbacks, loading completion and window
routing. Coordinator checked key paths and reconciled world cleanup with worker
shutdown. Only this audit and the handoff change. The same exact-client hash
and bounded-source methodology apply.

**Decision: SOURCE GAP remains; profile location population stays BLOCKED.**
Synchronous script dispatch and wait-driven loading completion broaden the
callback coverage obligation. A concrete deferred close path narrows one
teardown ingress; it does not exclude destructive reentry through other paths.
No sampler, lifetime/coherence contract or generation is qualified.

#### SOURCE VERIFIED — farsight callback execution

| Path | Evidence and qualification limit |
|---|---|
| State-gated wrappers | `0x5EE270` calls `0x5EE290` when player+0x1C70 mask 1 is clear; `0x5EE590` enters when set. The common body resolves farsight again at `0x5EE2AC–2EF`. Enable sends through `0x5AB630` at `0x5EE3E8` then sets the bit at `0x5EE3FE`; disable clears at `0x5EE4CE` then sends at `0x5EE50F`. These state tests are not a general reentry guard. Send forwarding `0x5AB630 → 0x5379A0` is distinct from inbound `0x537AA0`; it alone does not establish nested packet dispatch. |
| Synchronous event dispatch | After either wrapper, `0x5DE14E/167 → 0x703E50` dispatches event 0xC6 before returning to the outer field-notification executor. It still dispatches when the wrapper's flag test skips the common body; initial failed player lookup returns without it. Event name is `PLAYER_FARSIGHT_FOCUS_CHANGED`: `0x51AD75` stores string `0x853314` at `0xBE14B0 = 0xBE1198 + 0xC6*4`; initialization `0x48FEA1–AB → 0x703D90` installs that name table with count 0x225. |
| Registered script/native execution | `0x703E50` walks listener entries from `[0xCEEF68]+event*16`, calls `0x702690` at `0x703EFB`, then `0x704D50` at `0x7026B7`. That path includes virtual +4 calls (`0x704D93/9E`) and script execution `0x704E79 → 0x6F41A0 → 0x6F6960 → 0x6F5DB0`. Adapter `0x6F4250` reaches `0x6F65A0 → 0x6F6050`; native closure target is called at `0x6F61A8`, or the alternate branch enters interpreter `0x6F8720` at `0x6F65EA`. Registered handlers can therefore execute synchronously before `0x465570` resumes raw listener access. Which installed handlers reach cleanup/nested traversal remains UNKNOWN. |
| Other unresolved callees | `0x5EE483/4AE → 0x4841A0 → 0x50D0F0`, also reached through fallback `0x48EC90`, invokes virtual +0x14/+0x24/+0x18 at `0x50D142/253/263`. The inspected callback/wrapper bodies contain no direct call to `0x464C40`, `0x467800`, `0x465330` or `0x465570`. This bounded direct-body observation is not transitive absence; script/native bindings and virtual targets remain obligations. |

#### SOURCE VERIFIED — loading and close scopes

| Path | Evidence and qualification limit |
|---|---|
| Loading entry protocol | Allocator `0x4438B0` initializes input fields +0/+4/+8, context +0xC, normal callback +0x10, alternate callback +0x14 and optional lock +0x18; +0x1C/+0x28 start zero and +0x1D starts one (`0x44397F–999`). Submission `0x443AE0` queues/signals work. Worker calls fixed helper `0x648460` at `0x443431`, then marks +0x28 at `0x4434C9`; its inspected body does not directly invoke +0x10, and helper closure remains open. Normal drain `0x443E70` sets +0x1C at `0x443EED` before unlocked callback +0x10 at `0x443EF9`. That byte is not a callback-return witness. |
| Wait-driven completion | Helper `0x443BD0` calls completion drain `0x443E70` at `0x443CF4/0x443D10` while waiting. Linear direct-call inventory finds callers at `0x698689`, `0x6A4A70/B6`, `0x706964`, `0x710530`; inspected `0x6A4A50/90` loops repeat until object-associated request pointers clear. Completion is not confined to event 6. These paths do not by themselves prove nested notification or world mutation. |
| Shutdown callbacks | `0x443520` signals worker control/wakeup, waits on the worker handle at `0x44353D`, then calls `0x4435B0` at `0x443542`. Its drain can invoke queued alternate callback +0x14 at `0x44360C`. Event-6 deregistration follows at `0x44359C`. Joining the worker is distinct from completing every callback effect. |
| Concrete callback effects | Submitter `0x71D528–590` installs normal `0x71D5E0` at `0x71D55D`, alternate `0x71D610` at `0x71D567`, optional lock zero. Normal closes input via `0x648730`, recycles through `0x4439B0`, clears owner+0xC and continues into `0x71D640`. Alternate first calls fixed read helper `0x648460` at `0x71D62A`, then jumps to normal at `0x71D632`. Thus shutdown can execute a read and continuation after worker termination; world effects remain unproved. |
| Further completion targets | Inspected installed normal targets include `0x4497F0`, `0x44A500`, `0x6C21F0`, `0x6C3840`, `0x6C3F50` (stores `0x4496E8`, `0x44A401`, `0x6C1F33`, `0x6C3969`, `0x6C3F2D`). Global registration `0x448C00 → 0x443630` installs `0x448C10`, invoked through the consumer array at `0x443F2B`; it can submit more work at `0x448CD0 → 0x443AE0`. Separate `0x448C0A → 0x443770` installs `0x448CF0`, whose inspected body counts staging entries. This is a bounded target inventory, not complete effect coverage. |
| World cleanup precedes loading shutdown | Context cleanup `0x403640` calls `0x401EE0` at `0x403646`; if global `0x882734` permits that body, it removes the updater at `0x401F04` and destroys the manager at `0x402011`. Only later does context cleanup drain loading work (`0x40365A → 0x443D30`) and shut down the worker (`0x40366E → 0x443520`). This local sequence is not proof of unsafe callback contents or a lifetime failure. |
| Window input queue | Installed window callback `0x42CFE0` can submit records through `0x42D780` into a bounded global ring at `0x884CC0`, with 20-byte records and indices `0x884E34/50`. `0x42C9F0` consumes through `0x42CC00`; `0x423920` routes records through `0x4239A0`. The inspected close/system-command case at `0x42D287` with value 0xF060 produces type 5. This route defers processing; it is not direct manager teardown inside the producer. |
| Installed close handler uses TLS | Type 5 reaches optional callback dispatch `0x423A21 → 0x423B50`. Setup `0x402858 → 0x63B460` installs `0x63C9A0` at `0x63B594/599`. It calls `0x63CDF0` then returns zero, bypassing the generic nonzero-result fallback. `0x63CDF0 → 0x41F6E0 → 0x41DB90` reads element 0 of the current context TLS block, then `0x41F9B0` looks up that context ID and conditionally changes context+0x2C from 0 to 1 at `0x41FA44` under lock. The generic fallback separately calls `0x423CA0 → 0x41F5D0 → 0x422030` and sets an output flag; it is not the installed callback's path. |
| Later context cleanup | Main loop checks state at `0x420D79`, removes the selected context from scheduling at `0x420D94`, then calls `0x420E60` at `0x420D9B`. That dispatches event 3 (`0x420E9E`), stores state 2 (`0x420EAF`), drains queued events (`0x420EB9`), then dispatches event 4 (`0x420EC7`), whose registered `0x403640` reaches world cleanup. TLS is cleared afterward at `0x420ED9`. This proves a bounded ordinary-loop deferred close route, not recursive destruction inside the window callback or exclusion of every other reentry. |
| Secondary window route | `0x42CFE0` can invoke global `0x884E6C` at `0x42D372`. Setter `0x42CFD0` receives `0x436210` from `0x42E340` at `0x42E346/34B`; `0x42E370` clears it. The target contains indirect calls at `0x4362E7/30F/349/3B1`. Linear inventory found no direct callers of the install/remove wrappers, so activation during startup is not established. Likewise callable pump `0x41FE50` is a research lead without an established direct caller, not evidence of actual nested pumping. |

#### Evidence limits and continuation

**RUNTIME OBSERVED:** no new run, capture inspection, attach, input, native call
or memory write. Preserve prior raw-observer RUNTIME PASS within saved capture
limits. Location candidates remain UNQUALIFIED, profile location BLOCKED,
unload acceptance open, vendor/water runtime pending, reconnect unimplemented,
and R0.1 open. No production code, observer contract, ownership or navigation
changes; no runtime failure or qualification upgrade is claimed.

**INFERRED:** event labels, request flags and worker termination are insufficient
sampling boundaries because callbacks can execute in additional synchronous
scopes. The deferred close route narrows one ingress without proving that all
field/script/window/loading callbacks exclude cleanup or nested traversal.

**UNKNOWN / SOURCE GAP:** installed script/native listener effects, unresolved
virtual calls, loading normal/alternate callback effects and all submitters,
other window callback routes and TLS interactions, complete writer/counter
coverage, retention through callback execution, initialization/invalidation,
coherence, same-address/character/map ABA and player-bound zone/area freshness.

Reproduce with `objdump -d -Mintel --start-address=START --stop-address=STOP`
against `/home/ludvig/Games/WoW Vanilla/WoW.exe` (stop exclusive):

| Inspection | START / STOP |
|---|---|
| Farsight wrappers/event name | `0x5DE0D0 / 0x5DE178`; `0x5EE270 / 0x5EE5A0`; `0x51AD6B / 0x51AD7F`; `0x48FEA1 / 0x48FEB0`; `0x703D90 / 0x703E42` |
| Listener and script execution | `0x703E50 / 0x703F46`; `0x702690 / 0x7026E4`; `0x704D50 / 0x704EE2`; `0x6F41A0 / 0x6F425C`; `0x6F6960 / 0x6F69FE`; `0x6F5DB0 / 0x6F5E20`; `0x6F65A0 / 0x6F6616`; `0x6F6050 / 0x6F61BB` |
| Virtual and send paths | `0x4841A0 / 0x4841B6`; `0x48EC90 / 0x48ECCA`; `0x50D0F0 / 0x50D310`; `0x5AB630 / 0x5AB647`; `0x5379A0 / 0x537A2B` |
| Cleanup order | `0x403640 / 0x403698`; `0x401EE0 / 0x40209B`; `0x443520 / 0x443613`; `0x420E60 / 0x420EDE` |
| Loading entry/wait/callbacks | `0x443300 / 0x443630`; `0x4438B0 / 0x443F50`; `0x448BF0 / 0x448D21`; `0x71D528 / 0x71D640`; `0x6A4A50 / 0x6A4ACA` |
| Close producer/ring/dispatch | `0x42D287 / 0x42D2AF`; `0x42D780 / 0x42D7F1`; `0x42CC00 / 0x42CC70`; `0x42CBA4 / 0x42CBF8`; `0x423920 / 0x423B0E`; `0x423B50 / 0x423B67`; `0x423BB0 / 0x423BDB` |
| Installed close/TLS/context cleanup | `0x63B570 / 0x63B59E`; `0x41FE40 / 0x41FE45`; `0x424250 / 0x42425D`; `0x63C9A0 / 0x63C9BA`; `0x63CDF0 / 0x63CDFC`; `0x41F6E0 / 0x41F6E7`; `0x41F9B0 / 0x41FA77`; `0x420D11 / 0x420DA5` |
| Secondary window route | `0x42D35C / 0x42D37D`; `0x42CFD0 / 0x42CFD7`; `0x42E340 / 0x42E392`; `0x436210 / 0x4363C8` |

Validation: exact-client offline audit PASS; `python3 tools/validate.py --jobs 4`
PASS, all 247 records (111 C++ executables, 49 audit Python tests, 13 QuestDB
Python tests, 10 Lua fixtures, SQL/TSV fixture, full build and diff check).
Report: `/tmp/wow-validation-p3ma2u5s/results.json`. Separate
`cmake --build build` PASS for DLL, testhost, loader and GUI. Three read-only
reviews reconciled, including corrected context TLS element terminology; no
remaining findings. Complete diff/status review and `git diff --check` PASS;
only the two intended documentation files changed. Runtime NOT RUN.

Use `objdump -s` for event string `0x853314 / 0x853334`, message tables
`0x42D680 / 0x42D6B0` and consumer table `0x423B10 / 0x423B4C`. These manual paths do
not expand the automated 43-anchor/12-string manifest; its four qualification
flags and runtimeObserved remain false.

Next task: identify installed `PLAYER_FARSIGHT_FOCUS_CHANGED` listeners and
script/native bindings, and resolve the outstanding object virtual calls against
cleanup/nested notification. Trace loading normal/alternate callback bodies and
the remaining window dispatch routes to world writes or teardown. Require
reachability/exclusion and a complete lifetime/coherence/ABA contract before
sampler design; retain SOURCE GAP if proof is incomplete. Manual unload
acceptance remains separate and needs module-absence/responsiveness evidence.

### P0.7.4 — declared listener and resource callback effects

Continued on 2026-10-10 from `b3101eb87d4e5532e3b1eabae3a484f794760684`,
branch `codex/p07-world-zone-preparation`, initially clean. Three parallel
read-only agents investigated script bindings, resource completion and secondary
window callbacks. Coordinator reconciled results, checked key ordering/dispatch
sites and resolved selected object/graphics vtable entries. Only this audit and
the handoff change. Exact-client SHA256 and earlier evidence boundaries apply.

**Decision: SOURCE GAP remains; profile location population stays BLOCKED.**
Local asset declarations do not establish the live listener set. Request-pointer
clearing can precede continuation work; resolved virtual slots can lead to further
indirect calls. No complete lifetime/coherence/ABA contract or sampler is qualified.

#### SOURCE VERIFIED — listener assets and object virtuals

Local archive `/home/ludvig/Games/WoW Vanilla/Data/patch.MPQ` has SHA256
`977b63af02fe5d9f64453e18f3cfb7ac077769068d29125e475be8357654aeb4`.
Its `Interface\FrameXML\FrameXML.toc:73` declares `PetActionBarFrame.xml`;
XML line 3 includes the Lua file and lines 196–201 bind OnLoad/OnEvent.
`PetActionBarFrame.lua:17` registers `PLAYER_FARSIGHT_FOCUS_CHANGED`; lines
43–44 call `PetActionBar_Update()`. Lines 89–147 query pet actions and update
textures/buttons/check states/cooldowns, then may call `HidePetActionBar()`.
`ControlReleased()` on line 144 is commented out. Hide's inspected lines
181–190 adjust animation fields conditionally; dynamic UI methods and globals
still require transitive effect coverage. This identifies a stock declaration,
not the live installed listener set or archive precedence.

| Member under `Interface\FrameXML\` | Bytes | SHA256 |
|---|---:|---|
| `PetActionBarFrame.lua` | 10626 | `71aca8048fc626ab4565b0086f31c8bed9cef7f47194bc7b5eb30cdff3de2033` |
| `PetActionBarFrame.xml` | 6270 | `b87958dbf3794e8e4c2b475faa1264069d9293fb03ce56cb0a0149ac3f373b58` |
| `FrameXML.toc` | 1730 | `e41fce21d2dbe598c31999a5b7cdf410222a9e29eeea4be2fe342ee7b643b2f4` |

Extraction used reviewed scratch reader `/tmp/p07_mpq_read.py`, SHA256
`881dcb0ec936a9d2bee2953a8f0af5eb38bc1922d03117b4c4740a01bca1433a`.
It decrypts hash/block tables, decompresses encountered raw/zlib/bz2 sectors and
checks member lengths; it is not the client loader. Coordinator independently
re-read the three members and matched lengths/hashes and listener lines.
Reproduce with that scratch reader, or another MPQ extractor producing exactly
the pinned member bytes (scratch files are not permanent repository tools):

```python
import sys, hashlib
sys.path.insert(0, "/tmp")
from p07_mpq_read import MPQ
archive = MPQ("/home/ludvig/Games/WoW Vanilla/Data/patch.MPQ")
for leaf in ("FrameXML.toc", "PetActionBarFrame.xml", "PetActionBarFrame.lua"):
    data = archive.get("Interface\\FrameXML\\" + leaf)
    print(leaf, len(data), hashlib.sha256(data).hexdigest())
    for line, value in enumerate(data.decode().splitlines(), 1):
        print(line, value)
```

The read-only listfile/literal search covered local interface/patch/patch-2/base/
backup archives and loose Interface Lua/XML. It does not cover unlisted members,
computed names, generated code or runtime hooks. No account/WTF files were read.

Native method name/function pairs at `0x878F48/50/58/60` bind RegisterEvent,
UnregisterEvent, RegisterAllEvents and UnregisterAllEvents to
`0x774A40/0x774B30/0x774C20/0x774CF0`. Their helper calls are
`0x774AE5 → 0x702140`, `0x774BD5 → 0x702280`, `0x774CDA → 0x7023E0`,
`0x774DAA → 0x7024F0`. Registration compares names, suppresses duplicate targets
at `0x7021A8`, stores target in node+8 at `0x7021C7` and appends its event index
at `0x702239–23C`. Removal unlinks/frees at `0x702309/318` and compacts the
vector. Target cleanup `0x701B40` calls unregister-all at `0x701B6C`, then
releases script slot/vector storage at `0x701B77/8D`. Register-all means a literal
search for the event cannot enumerate all listeners. This UI-event node protocol
is distinct from the earlier object-field notification listener protocol.

Pet native pairs at `0x8475A0/A8/B0/B8` bind PetHasActionBar/GetPetActionInfo/
GetPetActionCooldown/GetPetActionsUsable to
`0x4BDC20/0x4BDC50/0x4BDFA0/0x4BE0B0`. Registration `0x4BEB90` iterates 28
entries through `0x704120` (caller `0x490314`); removal `0x4BEBC0` uses
`0x704160` (caller `0x490DA8`). PetHasActionBar reads `0xB714A0/A4` and pushes
a result; usable/cooldown paths call `0x4BCF70/0x6E2EA0`. Native/UI closure and
actual loaded script overrides remain open.

| Path | Evidence and qualification limit |
|---|---|
| Selected unit/player vtables | Construction stores `0x80AF78` at `0x5DD473` and `0x80C4F8` at `0x5FB12B`. Both tables' +0x14/+0x18/+0x24 slots bind `0x5F1F10/0x5F1F30/0x5F1F60`. The last reads object+0x9E0/+0x9E4 without calls. The first two use object+0x118 and forward to `0x7C4930/0x7C4AE0`. These are conditional mappings for inspected constructed objects, not an exhaustive type/vtable inventory. Farsight lookup uses mask 1 at `0x5EE2EA`; it is not restricted to these two tables. |
| Position/orientation helper limits | `0x7C4930` copies three values directly when movement+0x38/+0x3C are zero; otherwise `0x7C49D7 → 0x630AC0` looks up mask-0x20 object and invokes its +0x14/+0x18 slots at `0x630B38/4D`. `0x7C4AE0` similarly reaches `0x630B70`, whose `0x630B90` invokes +0x18 after lookup. Known GameObject construction stores table `0x80C330` at `0x5F7031`; its +0x14/+0x18 map to `0x5F9F50/0x5F9FB0`. These copy descriptor values when object+0x210 is null, otherwise call/tailcall further +0x44/+0x48 at `0x5F9F6F/0x5F9FBE`. These targets remain open; getter naming is not a transitive side-effect or lifetime proof. |

#### SOURCE VERIFIED — loading continuation effects

| Callback | Verified effects and remaining boundary |
|---|---|
| `0x4497F0` | Calls `0x449840` at `0x4497F3`; that initializes BLP2 constant at `0x449877`, reaches decode helpers `0x5A83B0/0x5A8430` at `0x44989E/0x449994`, and writes owner texture fields. After that continuation, the callback closes input, recycles request at `0x449818` and clears owner+0x138 at `0x449825`. Decoder/helper effects are not fully closed. |
| `0x44A500` | Calls `0x44A560`; graphics allocation path is `0x44A715 → 0x448450 → 0x58AC40` (call `0x448654`) `→ 0x58AB10` (call `0x58AC62`) `→ [0xC0ED38]` virtual+0xB4 at `0x58AC22`. Earlier factory tables `0x809AF8/0x809EF8` narrow this slot to `0x594A80/0x5A0810`; the latter forwards to the former at `0x5A083B`. Allocation/helper closure remains open. Error branches invoke owner+0x124 virtual+0xC at `0x44A5DF/0x44A691/0x44A73E`; strings `0x835674–0x835704` identify texture failures, not a guarantee those callbacks are harmless. |
| `0x6C21F0` | Calls `0x6C2010` at `0x6C21F3`; this derives resource pointers from owner+0x678, loads dependencies through `0x6C208F → 0x6C20E0 → 0x6C4C20`, publishes owner at `0xC96318+index*4` (`0x6C20BF`) and clears `0xC89F54+index*8` (`0x6C20CD`). These concrete resource/global writes are not established object-manager or map/zone/area writes. |
| `0x6C3840` | Recycles request, clears owner+0x1D0 at `0x6C3852`, closes input, then tailcalls `0x6C3990`. That invokes parser `0x6C3A60`, clears material fields through `0x6C3D70`, allocates through `0x6A03A0`, and sets owner+0x1D4 only at `0x6C39F7`. Submitter diagnostic `0x86C388` names `WorldClient\MapObjRead.cpp`; parser contains MOHD/MOTX/MOMT constants. Request-null precedes downstream work. |
| `0x6C3F50` | Clears owner+0x160 at `0x6C3F62` before tailcalling `0x6C3F80`. Continuation adjusts resource links/fields, parses through `0x6C405F → 0x6C40C0`, loads material textures through `0x6C408F → 0x6C3DB0 → 0x6C4C20`, then sets owner+0x164 at `0x6C40A4`. Parser path `0x6C419A → 0x6C4530` retains unresolved helpers including `0x692F20/0x6BBD70/0x6BA9B0`. |
| `0x71D640` | After the earlier caller clears owner+0xC, this continuation calls `0x71CDF0/0x71D6C0`. The latter requests textures at `0x71DA72 → 0x449D90`. On success, `0x71D675` sets owner+8 mask 1, then unlinks dependents and calls `0x70EBD0` at `0x71D6A0`. Thus even that bit precedes dependent work. Model-resource helper diagnostic `0x873240` names `Model2\M2Model.cpp`; downstream virtuals `0x70FA6B/95/A5/D5/E5` remain unresolved. |
| Additional submitted work | `0x6C4C20 → 0x449D90` at `0x6C4C77`; `0x449D90` can select `0x44A310` at `0x449E2B`, whose earlier verified submission installs `0x44A500`. Completion can initiate further resource work. Neither a null request nor a local ready bit establishes completion of all descendants or world lifetime. No runtime overlap or destructive reentry is inferred from ordering alone. |

#### SOURCE VERIFIED — secondary window callback bindings

| Path | Evidence and qualification limit |
|---|---|
| HWND property association | `0x436210` gets objects through `0x42E510`, which resolves `GetPropA(HWND, "OsGuiPointer")`: name pointer `0x801638 → 0x834540`, resolver `0x748815–0x74883F`, function slot `0x8824D4`, import string `0x800F70`. Setter `0x42F3B0` uses SetPropA (`0x8824E4`, name `0x800FE0`). Control creation associates HWND/object at `0x430B82–87`; constructor `0x430900` stores table `0x801818` at `0x430914` and parent at +8. Derived construction stores `0x801D40` at `0x4346EF` and associates another HWND at `0x43470C`. |
| Conditional targets and user callbacks | Inspected tables resolve +4 to return `0x436610`, +0x34/+0x38 to `0x431270/0x4312A0`, +0x30 to `0x431200` or derived `0x434BE0`. The base notification methods map values through `0x431230` and can call `0x430CE0`. Its +8 at `0x430CF9` binds to `0x430CA0`, invoking control+0x18 at `0x430CC8` when present. Otherwise parent forwarding `0x430D1A → 0x430760` can invoke parent+0xC at `0x430788`. Setters `0x430C80/0x430360` accept caller-supplied callback/context pairs. Actual targets and effects remain UNKNOWN. |
| Activation boundary | Whole-file little-endian literal searches found no absolute address `0x42E340`, `0x42E370` or `0x41FE50`, supplementing earlier linear direct-reference absence. `0x436210` occurs as the known immediate at `0x42E347`. This does not exclude computed addresses, aliases, interior entries or external invocation. The route's gameplay activation and nested-pump reachability remain UNKNOWN; conditional vtable resolution is not runtime installation evidence. |

#### Evidence limits and continuation

**RUNTIME OBSERVED:** no new run, capture inspection, attach, input, native call
or memory write. Preserve prior raw-observer RUNTIME PASS within saved capture
limits. Location candidates remain UNQUALIFIED, profile location BLOCKED,
unload acceptance open, vendor/water runtime pending, reconnect unimplemented,
and R0.1 open. No source, observer contract, controller or navigation changes.

**INFERRED:** resource requests and local ready bits cannot alone prove all
continuation effects have completed. Concrete asset declarations and vtable
mappings narrow possible paths, but do not prove the live listener set, all
writers, retained references or absence of destructive callback reentry.

**UNKNOWN / SOURCE GAP:** live asset/archive selection and addon listeners,
script/native callback closure, resource-owner retention, decoder/backend/error
callbacks, remaining object virtual targets, secondary GUI activation/callbacks,
helper TLS effects, complete writer/counter and invalidation coverage, coherent
sampling, same-address/character/map ABA and player-bound zone/area freshness.
No runtime failure or safe sampling phase is established.

Reproduce binary inspection with `objdump -d -Mintel --start-address=START
--stop-address=STOP` against `/home/ludvig/Games/WoW Vanilla/WoW.exe` (stop
exclusive). Use `objdump -s` for table/string rows.

| Inspection | START / STOP |
|---|---|
| Event/native bindings | `0x702140 / 0x702590`; `0x701B40 / 0x701BA1`; `0x774A40 / 0x774DB5`; `0x4BEB90 / 0x4BEBDB`; `0x704120 / 0x704195` |
| Unit/player bindings and getter helpers | `0x5DD2E0 / 0x5DD4FF`; `0x5FAD50 / 0x5FB1C5`; `0x5F1F10 / 0x5F1F6D`; `0x7C4930 / 0x7C49F6`; `0x7C4AE0 / 0x7C4B2E`; `0x630AC0 / 0x630B97`; `0x5F6F30 / 0x5F7054`; `0x5F9F50 / 0x5F9FCB`; `0x468460 / 0x468486` |
| Texture continuations/backend | `0x4497F0 / 0x4499C2`; `0x44A500 / 0x44A787`; `0x448450 / 0x44866E`; `0x58AB10 / 0x58AC68`; `0x449D90 / 0x449EA9`; `0x5A0810 / 0x5A0844` |
| Map/model resource continuations | `0x6C2010 / 0x6C2225`; `0x6C3840 / 0x6C3DA3`; `0x6C3DB0 / 0x6C3E19`; `0x6C3F50 / 0x6C41C6`; `0x6C4530 / 0x6C472D`; `0x6C4C20 / 0x6C4C9A`; `0x71D640 / 0x71DAAC` |
| Window property/control association | `0x42E510 / 0x42E51E`; `0x42F3B0 / 0x42F3BF`; `0x748815 / 0x74886B`; `0x430900 / 0x430939`; `0x4309B0 / 0x430B93`; `0x4346C0 / 0x434739` |
| Notification forwarding | `0x431200 / 0x4312CD`; `0x434BE0 / 0x434D00`; `0x430C80 / 0x430D2F`; `0x430760 / 0x430795`; `0x430360 / 0x430373` |
| Event/pet name-function tables | `0x878F48 / 0x878F68`; `0x87939C / 0x8793E4`; `0x8475A0 / 0x8475C0`; `0x847800 / 0x847850` |
| Tables / diagnostics / properties | `0x80AF78 / 0x80AFA0`; `0x80C4F8 / 0x80C520`; `0x80C330 / 0x80C358`; `0x809BAC / 0x809BB0`; `0x809FAC / 0x809FB0`; `0x801818 / 0x801870`; `0x801D40 / 0x801D98`; `0x801638 / 0x80163C`; `0x834540 / 0x83454D`; `0x835674 / 0x835704`; `0x86C388 / 0x86C3BC`; `0x873240 / 0x873274` |

Literal-search absence is reproducible with `data.find(struct.pack('<I', va))`
on the complete executable; `-1` applies only to that representation. Additional
manual paths do not expand the automated 43-anchor/12-string manifest. Its four
qualification flags and runtimeObserved remain false.

Next task: trace the declared stock pet-action update's UI methods and possible
synchronous OnShow/OnHide scripts, and establish FrameXML selection plus the
limits of addon/event-target enumeration. Prioritize callback routes to bulk
object cleanup or nested field notification while outer references remain live.
Resolve resource-owner retention/descendant completion and the remaining
object/GUI callback targets as needed for that proof; avoid treating additional
resource names or local ready bits as lifetime evidence. Retain SOURCE GAP until
writer, invalidation, coherence and ABA obligations are closed. Manual unload
acceptance remains separate and needs module-absence/responsiveness evidence.

Validation: exact-client offline audit PASS; extracted asset lengths/hashes
independently matched. `python3 tools/validate.py --jobs 4` PASS, all 247 records
(111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests, 10 Lua
fixtures, SQL/TSV fixture, full build and diff check). Report:
`/tmp/wow-validation-j9fy1hek/results.json`. Separate `cmake --build build` PASS
for DLL, testhost, loader and GUI. Three read-only reviews found no actionable
corrections. Full diff/status review and `git diff --check` PASS; exactly the two
intended documents changed, with no unrelated changes. Runtime NOT RUN; no
qualification upgrade.

### P0.7.4 — visibility callbacks and FrameXML selection boundaries

Continued on 2026-10-10 from `0c4621dc6639389fa34bbe80bda521ee13533821`,
branch `codex/p07-world-zone-preparation`, initially clean. Three parallel
read-only agents traced stock UI assets, native visibility dispatch and FrameXML
loading. Coordinator checked key chains, re-extracted six asset members and
examined the separate ReloadUI request/consumer route. Only this audit and the
handoff change; no runtime actions or production changes.

**Decision: SOURCE GAP remains; profile location population stays BLOCKED.**
The stock asset path distinguishes immediate widget updates from later bar
animation. Native visibility can synchronously execute scripts, but neither
that capability nor a locally available archive establishes the live callback
set, destructive reentry, retained-reference safety or a sampling boundary.

#### SOURCE VERIFIED — bounded stock visibility paths

All member paths below are under `Interface\FrameXML\`. The preceding section's
patch archive, PetActionBarFrame Lua/XML and extraction-reader hashes still
apply. Additional archive `/home/ludvig/Games/WoW Vanilla/Data/interface.MPQ`
has SHA256 `3f3d739035fc0aa33f4b379e6e046edd31c77509a2414bfb74a17f324c162f6a`.

| Archive/member | Bytes | SHA256 |
|---|---:|---|
| patch/ActionButtonTemplate.xml | 2661 | `2226543118b1fe54af9b998ac5a5cc17f5fa87301168dfc27ae25d4b0824d36f` |
| patch/Cooldown.xml | 560 | `f2207bdc4fb0269a37acd0d0c886f49dacd0e1c07743b12901e527c13942a550` |
| interface/Cooldown.lua | 745 | `619608ac636ac5875e6590c3ad3159c9b4c21183efa2e059b94c4a06c6dd2869` |
| patch/UIParent.lua | 54330 | `eec3b8f2f771f5c42b8db6aca2cdff8953c839f90fa4cda051ab73d72a073e5b` |
| patch/FloatingChatFrame.lua | 42739 | `bcd07daf93ff1f5fe7c879e43e99cb9abad4759eced9791d3347ecf9b8fe498f` |
| patch/ContainerFrame.lua | 24456 | `77fbf69cea3ff7ff685c6102810266eb5b4c78a4811ca63faa0350ab0a91d6a8` |

Coordinator re-extraction matched these lengths/hashes. Reproduce with the
previous section's MPQ extraction snippet, substituting these exact archive and
member names; another extractor must produce the same pinned bytes. The scratch
reader remains an offline research aid, not the client loader or a tracked tool.
Cooldown.lua was absent from the inspected patch archive and available in
interface.MPQ; this does not prove the client's fallback selection.

| Declared path | Evidence and limit |
|---|---|
| Button/model scripts | PetActionBarFrame.xml:4 inherits ActionButtonTemplate; its own scripts at 35–55 have no OnShow/OnHide. ActionButtonTemplate.xml:3–89 has no Scripts or further root inheritance; cooldown child at 63 inherits CooldownFrameTemplate. Cooldown.xml:4–12 has OnUpdateModel/OnAnimFinished, no OnShow/OnHide. Pet AutoCast model XML:25–32 has only OnLoad. These are bounded declarations, not a live absence proof against overrides or native hooks. |
| Deferred parent bar hide | Farsight handler calls PetActionBar_Update, whose conditional HidePetActionBar body (Lua:181–190) changes slide fields/mode. The actual `this:Hide()` is in the separately bound OnUpdate at Lua:79–82 (XML:202–204). Parent bar OnShow/OnHide (XML:205–210) call UIParent_ManageFramePositions. Do not equate farsight notification with immediate parent-bar OnHide. |
| Other widget effects | Pet button OnLeave (XML:53–55) calls PetActionButton_OnLeave (Lua:307–309), which calls GameTooltip:Hide. The conditional native Hide-to-leave path is detailed below; live delivery remains unestablished. UIParent.lua:1500–1510 SetDesaturation calls SetDesaturated or falls back to SetVertexColor. Cooldown.lua:2–12 calls SetSequence(0), Show/Hide; callbacks at 14–34 invoke time/model helpers and may Hide. Their synchronous native effects are not all resolved. |
| Layout expands to other frames | UIParent.lua:1592–1774 uses mutable globals/tables and frame positioning/visibility/size methods, then FCF_DockUpdate at 1772 and updateContainerFrameAnchors at 1773. FloatingChatFrame.lua:1049–1140 iterates DOCKED_CHAT_FRAMES, calls Show at 1069 or Hide at 1075, plus tab helpers. ContainerFrame.lua:472–552 iterates mutable bag-frame names and calls scale/position methods; tooltip block 541–551 is commented out. These paths do not establish a direct bulk object cleanup or nested field-notification call; dynamic closure remains open. |

#### SOURCE VERIFIED — native visibility and reload requests

| Path | Evidence and qualification limit |
|---|---|
| Frame method binding | Show/Hide name-function pairs `0x878FC0/0x878FC8` bind `0x775750/0x775810`. They write frame+0xD0 to 1/0 and invoke virtual +0x88/+0x84 at `0x7757FB/0x7758BB`. Constructor `0x769090` installs table `0x81C498` at `0x7692E5`; its slots `0x81C520/0x81C51C` resolve to `0x76AE10/0x76AD50`. This is conditional resolution for the inspected class, not all widget classes. |
| Transition and synchronous hooks | Show checks requested visibility/parent visibility and skips the transition when already visible; Hide skips when hidden. Writes to +0xD4 at `0x76AE7B/0x76AD93` precede child traversal and scripts. Child virtuals `0x76AED5/0x76ADEE` return before traversal reads node+4 at `0x76AEDB/0x76ADF4`. Parent hooks `0x76AEF5/0x76ADFD` resolve via +0x30/+0x34 to `0x76B260/0x76B290`. Resolver `0x76A14D–17C` binds OnShow/OnHide to +0x130/+0x138. Nonzero script fields and +0x114==0 allow `0x76B27B/0x76B2AB → 0x702690 → 0x704D50 → 0x704E79` synchronous script execution. This does not prove the live pet control has those callbacks or that node/object lifetime is retained across them. |
| Conditional hide-to-leave delivery | Before child traversal and OnHide, `0x76AD9D → 0x764920 → 0x76AFE0` tests frame+0xCC bits 0–3 and calls `0x764BA0` at `0x76B00A`. Its index-2 route, outer flag zero, matching UI-owner+0x7C and frame+0xAC!=3 allow virtual+0x50 at `0x764CD4` after clearing owner+0x7C. Base table binds `0x76B6F0`, whose +0x148 script field (OnLeave per `0x76A1AD–1BC`) reaches `0x702690` at `0x76B753`. Button construction installs table `0x81C7F8` at `0x778733`; its +0x50 override `0x7794E0` calls the base at `0x7794F4` only when button+0x328 is nonzero. Further +0x31C callback at `0x779530` remains indirect. This supplies a conditional native link to the declared pet OnLeave→tooltip Hide, not proof it fires in the live farsight case. |
| Derived and region distinctions | Button OnHide override `0x7791E0` can call virtual+0x9C before jumping to `0x76B290`. Other Show/Hide pairs `0x87C178/180` and `0x87C208/210` bind `0x79B770/0x79B830` and `0x79CDB0/0x79CE70`; these write region+0xC4 and call visibility/list helpers `0x77FCB0/0x77FC60`. The frame script route cannot be assigned indiscriminately to textures/regions. All derived effects remain a coverage obligation. |
| Script bookkeeping is not world retention | `0x702690` increments `0xCEEAC4`, conditionally changes `0xCEEAC0` around script execution, then conditionally restores it and decrements/clamps the counter. These inspected operations do not retain the outer world object, descriptor storage or notification nodes, nor prove exclusion of destructive reentry. |

ReloadUI is a separately inspected candidate, not a call found in the stock
farsight handler. Name/function pair `0x83DE78` binds string `0x83F298` to
`0x4884D0`, which calls `0x491380`. That calls gate `0x494A50(10)` and, only on
nonzero return, sets byte `0xB4B3F4` at `0x49138E`. The wrapper returns without
directly invoking the reload consumer. Initialization clears the byte at
`0x490124` and registers `0x495590` for event 5 at `0x49015A`. The consumer tests
the byte at `0x49565B` and calls `0x490BD0` then `0x48FBF0` at `0x495664/669`.
This narrows one deferred request route, not every possible nested event pump.
The cleanup body itself drains loading at `0x490BE9`, dispatches event 0x10F at
`0x490C2A`, and invokes a UI object's virtual+4 at `0x490C9B`; downstream effects
remain open. It is not evidence of direct object-manager teardown from ReloadUI
or of stock pet-handler reachability to that function.

#### SOURCE VERIFIED — FrameXML load and file-selection limits

| Path | Evidence and limit |
|---|---|
| Signature/content gate | Initialization `0x48FF4D → 0x6F10F0` passes FrameXML.toc (`0x842F7C`), Bindings.xml (`0x842F9C`) and key pointer `0x7FFBB0`. Helper builds `%s.sig` (`0x871330`), opens via `0x6F1131 → 0x648620`, requires size 0x114 at `0x6F1143`, calls verifier `0x6F16F0` at `0x6F1192`, then hashes TOC/dependencies through `0x6F1200` and compares four DWORDs. Return paths are 0 missing signature, 1 malformed/verification failure, 2 content mismatch, 3 match. This task inspected code; it did not execute the client signature verifier or establish actual success. |
| Failure marking and continued load | Table `0x4901C8` sends 0/1/2 through diagnostics `0x48FF5E/6B/78` then `0x48FF90 → 0x401560(10)`. That records `0x882738` and forwards through `0x41F9A0 → 0x41F9B0`, the earlier context-cleanup marking path. Result 3 jumps to `0x48FF95`. Failure marking falls through into subsequent loading; it is not an immediate return. Actual load `0x48FFED → 0x6EDB90` opens again at `0x6EDC28 → 0x648620`, skips # lines at `0x6EDCD9`, prefixes paths and calls `0x6EDE10` at `0x6EDD51`. Lua branch `0x6EDF0F → 0x704BC0`; another branch calls `0x6EDAA0` at `0x6EDF2F`. Later digest compare `0x490038` can mark failure at `0x490043`. No selected archive/member bytes are established by this chain alone. |
| Startup narrows one loose-file probe | `0x40215B → 0x402210` clears global `0x865B44` via `0x40221A → 0x648C20`, resets flags `0x865B40` via `0x402221 → 0x648BE0(0)`. Read chain `0x648620 → 0x6477C0 → 0x647E60` combines flags at `0x647E72` and has earlier mode/index branches. At `0x648116–11F`, zero 0x865B44 strips low two flags before `0x654920 → 0x6549A0`; those bits gate filesystem probe `0x654B76 → 0x654DD0`. This is not universal loose-file exclusion: earlier branches, other callers and actual globals remain open. |
| Archive discovery/open sequence | `0x402366 → 0x403740` enumerates Data\patch-?.MPQ at `0x40377F`, sorts with `0x403AE0` at `0x403796`, appends patch.MPQ via `0x4037AF`, traverses collected entries backward at `0x4037EB`, and opens via `0x4037F4 → 0x648DD0 → 0x655670` (call `0x648E2F`). Numeric arguments start at 0x40 and increase on successful opens at `0x4037FD/FE`. Lower-level insertion/selection, locale/member rules and actual successful opens remain UNKNOWN. Open order/arguments are not a proved archive-priority rule. |

#### Evidence limits and continuation

**RUNTIME OBSERVED:** no new run, capture inspection, attach, input, native call
or memory write. Preserve prior raw-observer RUNTIME PASS within saved capture
limits. Location candidates remain UNQUALIFIED, profile location BLOCKED,
unload acceptance open, vendor/water runtime pending, reconnect unimplemented,
and R0.1 open. No observer contract, controller or navigation changes.

**INFERRED:** declarations, availability and loader ordering cannot establish
the live UI callback set. Conditional synchronous visibility scripts widen the
effect proof obligation; the deferred stock bar hide and ReloadUI request narrow
specific routes. Neither proves absence of every destructive reentry or a safe
sampling phase. No runtime fault or overlap is claimed.

**UNKNOWN / SOURCE GAP:** selected archive/loose member provenance, actual
script/native overrides, reachable visibility/hover/model/layout callbacks and
reference retention, nested pumping, complete resource-owner/descendant effects,
all writer/counter/invalidation paths, coherence and same-address/character/map
ABA, player-bound zone/area freshness. Earlier bulk-cleanup, inline-descriptor,
shared-node and shared-TLS worker limitations remain.

Reproduce binary inspection with `objdump -d -Mintel --start-address=START
--stop-address=STOP` against the exact WoW.exe (stop exclusive); use `objdump -s`
for table/string rows. Asset extraction/line reproduction is described above.

| Inspection | START / STOP |
|---|---|
| Frame methods / visibility scripts | `0x775750 / 0x7758C7`; `0x769090 / 0x769340`; `0x76AD50 / 0x76AEFF`; `0x76B260 / 0x76B2B1`; `0x76A148 / 0x76A1C8`; `0x702690 / 0x7026E4`; `0x704D50 / 0x704EDF` |
| Conditional leave / region distinction | `0x764920 / 0x764A1F`; `0x76AFE0 / 0x76B01C`; `0x764BA0 / 0x764CE0`; `0x76B6F0 / 0x76B75D`; `0x7794E0 / 0x779538`; `0x7791E0 / 0x77920B`; `0x79B770 / 0x79B8E4`; `0x79CDB0 / 0x79CF24`; `0x77FC60 / 0x77FD0A` |
| Reload request / consumer | `0x4884D0 / 0x4884D8`; `0x491380 / 0x491396`; `0x494A50 / 0x494B60`; `0x490113 / 0x49015F`; `0x495590 / 0x495679`; `0x490BD0 / 0x490CDE` |
| FrameXML gates and readers | `0x48FF3A / 0x490048`; `0x6F10F0 / 0x6F1471`; `0x6F16F0 / 0x6F1771`; `0x6EDB90 / 0x6EDDAB`; `0x6EDE10 / 0x6EDF50` |
| File flags / archive discovery | `0x402150 / 0x402226`; `0x403740 / 0x403802`; `0x403AE0 / 0x403AF5`; `0x648620 / 0x6486F2`; `0x647E60 / 0x64817C`; `0x648BE0 / 0x648C28`; `0x654920 / 0x654D70`; `0x654DD0 / 0x654E90`; `0x648DD0 / 0x648E38` |
| Visibility tables | `0x878FC0 / 0x878FD0`; `0x81C498 / 0x81C524`; `0x81C7F8 / 0x81C884`; `0x87C178 / 0x87C188`; `0x87C208 / 0x87C218` |
| Tables / names | `0x83DE78 / 0x83DE80`; `0x83F298 / 0x83F2A1`; `0x4901C8 / 0x4901D8`; `0x842F7C / 0x842FAC` |

Manual paths and archive findings do not expand the automated 43-anchor/12-string
manifest. All four qualification flags and runtimeObserved remain false.

Next task: follow the now-proven conditional pet-button OnLeave→GameTooltip:Hide
route through the actual tooltip class/OnHide and remaining button callbacks;
trace cooldown SetSequence/model callback delivery where reachable from the
stock update. Determine whether these paths can reach bulk cleanup or nested
field notification, and distinguish UI-node mutation from outer world-object
retention. Complete lower-level archive insertion/member selection only as needed
to establish loaded-script provenance; do not assume open order is priority.
Keep SOURCE GAP until writer/lifetime/coherence/ABA coverage closes. Manual unload
acceptance remains separate and needs module-absence/responsiveness evidence.

Validation: exact-client offline audit PASS; six asset lengths/hashes and the
interface archive hash independently matched. `python3 tools/validate.py --jobs 4`
PASS, all 247 records (111 C++ executables, 49 audit Python tests, 13 QuestDB
Python tests, 10 Lua fixtures, SQL/TSV fixture, full build and diff check).
Report: `/tmp/wow-validation-69w5d0v3/results.json`. Separate `cmake --build build`
PASS for DLL, testhost, loader and GUI. Three read-only reviews found no
actionable corrections. Full diff/status review and `git diff --check` PASS;
exactly the two intended documents changed, with no unrelated changes.
Runtime NOT RUN; no qualification upgrade.

### P0.7.4 — tooltip listener removal, model delivery and archive priority

Continued on 2026-10-10 from `090344528c3c6f46e0f51ea7a02b44d991f1b4b9`,
branch `codex/p07-world-zone-preparation`, initially clean. Three parallel
read-only agents traced tooltip effects, cooldown model delivery and archive
selection. Coordinator reconciled the results, independently checked key binary
chains and four extracted members, and narrowed the button OnHide override.
Only this audit and the handoff change; no production or runtime actions.

**Decision: SOURCE GAP remains; profile location population stays BLOCKED.**
A conditional tooltip route reaches field-listener removal, not demonstrated
bulk cleanup or nested field notification. Model references bracket specific
operations; archive open increments an archive count. Neither establishes
retention of the outer world object, descriptor, listener or UI callback context.

#### SOURCE VERIFIED — tooltip clearing and button effects

All member paths are under `Interface\FrameXML\` in the previously pinned
patch.MPQ. Coordinator re-extraction matched these lengths and SHA256 values;
local availability remains distinct from actual client selection.

| Member | Bytes | SHA256 |
|---|---:|---|
| GameTooltip.xml | 2388 | `8e1f985894bb4803dd3d26151ef0689268bcb039a15b363fa7596d039d9cee52` |
| GameTooltip.lua | 3629 | `56c418192b32b0b4553504e7bf0ff9c90a39f4ff67a152ea6691c06ceb66a3ad` |
| GameTooltipTemplate.xml | 41633 | `46f3f595a21a1112c10364e5c3f0486ef303de37c8cadd6b5b2b694d2941ccc5` |
| MoneyFrame.xml | 8073 | `f346ff154e3dbbfa70f702f9e308a7e3d1c51c2206932e25d88018fdc7990007` |

| Path | Evidence and qualification limit |
|---|---|
| Tooltip class and ordering | Registration `0x495953–95D` associates GameTooltip (`0x842F08`) with factory `0x495C60`; `0x495C7D → 0x529240` installs table `0x808F60` at `0x5292F4`. Its +0x84 binds Hide `0x530A60`, +0x34 remains OnHide `0x76B290`. Hide first calls `0x52FFE0(0,0,0,0)` at `0x530A6B`, then tailcalls base `0x76AD50` at `0x530A73`. Clearing `0x52FFE6 → 0x530050` therefore precedes the base visibility gate, including for an already-hidden tooltip. This is conditional class resolution, not live installation evidence. |
| Field-listener removal | Nonzero GUID at tooltip+0x368/+0x36C and nonnull +0x348 allow `0x53008E → 0x467FB0`: category 3, byte offset 0x40, callback `0x529560`, context +0x348. Matching registration `0x52A08D–A8 → 0x467E70` has length 4. Removal follows the earlier active/pending protocol at `0x468023–57`; this callback differs from outer farsight callback `0x5DE0D0`. It is not proof of removing the outer active listener, bulk manager cleanup, or nested notification. The following `0x530093–A1` clears the child's requested visibility and invokes its virtual+0x84. |
| Clearing script and remaining virtuals | Resolver `0x529604–621` maps OnTooltipCleared (`0x854794`) to +0x44C. Nonzero script dispatch `0x53024A–263 → 0x702690` occurs during clearing. Then `0x52FFF4` invokes virtual+0x28, here `0x76A690`, which conditionally traverses lists and calls further virtual+0x20/+0x28 at `0x76A6D0/0x76A702`. Other clear-helper calls/virtuals include `0x5301F0`, `0x771D80`, `0x767C70`, `0x77FC60`; transitive closure remains open. |
| Declared Lua effects | FrameXML.toc:13 includes GameTooltip.xml; XML:4–6 loads Lua/template and declares inheritance. XML:17–21 OnHide calls GameTooltip_OnHide and hides ShoppingTooltip1/2. Lua:101–105 resets backdrop colors and `this.default`. Template:623–625 OnTooltipCleared calls GameTooltip_ClearMoney; Lua:94–99 conditionally hides the named MoneyFrame. Template:598 declares SmallMoneyFrameTemplate inheritance; MoneyFrame.xml:289–294 OnHide conditionally hides CoinPickupFrame then clears `hasPickup`. ShoppingTooltipTemplate:628–1217 has only root OnLoad at 1212–1216, but its native tooltip Hide still clears. Mutable globals, inheritance and overrides prevent a live callback-absence claim. |
| Backdrop and button limits | Backdrop method pairs `0x8790C8/0x8790D8` bind `0x777D30/0x7780D0`; tails use frame+0x1AC and `0x77F410/0x77F440`, not fully closed. Button table `0x81C7F8` +0x9C binds `0x779790`, narrowing the prior OnHide override. It conditionally clears old region+0xC4, calls `0x77FC60`, replaces button+0x4C4, sets new region+0xC4 and calls `0x77FCB0`, then `0x779810` and stores state+0x328. Helper `0x779810` uses button+0x338 and calls `0x772100/0x770C60`; closure remains open. The separate OnLeave callback through button+0x31C at `0x779530` still has an unresolved target; setter `0x779760` accepts caller-supplied values for +0x31C/+0x320/+0x324. |

#### SOURCE VERIFIED — cooldown callbacks and resource retention

The preceding section's pinned Cooldown Lua/XML and executable hashes apply.
No native method was invoked during this offline research.

| Path | Evidence and qualification limit |
|---|---|
| Method bindings | Pairs `0x878978/0x878980/0x8789B8` bind SetSequence/SetSequenceTime/AdvanceTime to `0x76DEC0/0x76DFC0/0x76ECA0`. Valid paths call `0x76CF50/0x76CF80/0x76CFB0` at `0x76DF6A/0x76E08C/0x76ED5A`. The last helper is exactly `mov eax,1; ret`; its Lua wrapper returns zero results. This inspected AdvanceTime route performs no advancement; overrides remain possible. |
| Sequence setter's direct callback | First two helpers use widget+0x318, skip null model and call `0x7121A0`. For model+0x10==0, `0x7121DF–228` queues operation code 4 with arguments; replay remains open. A loaded-path conditional callback at `0x7122C1` passes event code 1, bracketed by model acquire/release `0x7122A5/0x7122C6`. The installed callback below rejects nonzero events, so this particular site does not dispatch OnAnimFinished. This is not exhaustive closure of the whole setter or its downstream helpers. |
| Model ownership and script target | `0x76CD30` acquires incoming model via `0x710390` at `0x76CD40`, releases previous via `0x7103A0` at `0x76CD4F`, and stores widget+0x318. `0x76CD85/0x76CD8A` installs `0x76CDC0` through `0x711BB0`: model+0x70 callback, +0x78 widget user pointer, +0x7C zero. Acquire increments model DWORD+0; release decrements and destroys/frees on zero. Resolver `0x76CCB9–CC8` maps OnAnimFinished to widget+0x3D4. Callback `0x76CDC0` permits event 0 only and dispatches that script via `0x702690` at `0x76CDDE`. Model counting does not establish retained widget or outer world references. |
| Completion update and queue | Constructor installs `0x81C608` at `0x76C98F`; +0x38 binds `0x76D7F0`. Update first calls base OnUpdate `0x76B2C0`, obtains model scene via `0x76CFC0`, then calls `0x7074B0` at `0x76D863`. Its `0x7074D8 → 0x719370` terminal checks `0x719497–4D9` can queue model+0x70 with event 0 via `0x7194FE → 0x70A280`. Record fields are +4 callback, +8 model, +0x14 event, +0x28 widget user pointer; `0x70A3E7` acquires the model. Type-0 dispatch calls callback at `0x7075AF`, then rereads record+8 and releases model at `0x7075BB`. OnAnimFinished can execute synchronously within this update/drain; nesting within the farsight handler is not established. Queue mutation and widget-context lifetime remain separate obligations. |
| OnUpdateModel | Resolver `0x76CC99–CA8` maps script+0x3CC; table+0x98 binds `0x76D1A0`. With a model and argument 2, `0x76D160` registers `0x76D240` via `0x773110` at `0x76D18C`. Its virtual+0x98 call at `0x76D5EA` reaches `0x76D1A0 → 0x702690` at `0x76D1BC`. After the script, widget fields/model+0x318 are reread at `0x76D1D1–1EA` before `0x710650`. Registration is not proof of actual render scheduling, nested delivery or widget retention across scripts. |

#### SOURCE VERIFIED — archive insertion and per-archive selection

This narrows the preceding section's unresolved priority rule without proving
actual successful opens, current selector values or loaded FrameXML bytes.

| Path | Evidence and qualification limit |
|---|---|
| Numeric priority and insertion | `0x655670` forwards its second argument at `0x655679/685`; `0x6559B2/BB` stores it into archive+0x148, then `0x655A27 → 0x655BF0` finalizes. Under lock `0xC54008`, insertion starts list head `0xC53FE0` at `0x656012`, compares existing+0x148 with new at `0x656025`, follows next+4 while existing is greater, and stops on signed `jle` at `0x65602B`. Writes `0x6560AA–BC` insert before that entry, with sentinel `0xC53FDC` fallback. This path orders descending signed priority, newest before existing equals. Related view creation stores +0x148 at `0x655B2F` and repeats ordering at `0x655B60–BB8`. |
| Eligible archive order | `0x6549A0` snapshots selector word/byte `0x866520/0x866522` at `0x654A62/67`, locks `0xC54008`, starts `0xC53FE0` at `0x654AA4` and advances next+4 at `0x654CE7–CF5`. It restricts explicit archive arguments at `0x654AC0–AC6`, skips signed +0x298>0 at `0x654AD0–AD8`, can redirect backing through +0x174 and prefix member names through +0x178. Identity and backing/path may differ. |
| Member selection and stop conditions | Hash checks skip -2 entries at `0x654C75–C85`. Exact word+8/byte+0xA selector matches win at `0x654C87–C97`; compatible zero fallbacks are retained at `0x654C99–CB5`. Only exhaustion without a candidate advances to another archive. A selected entry+0xB bit 0 yields special status 3 through `0x654D0A → 0x654D5C`, without continuing to lower priority. Normal `0x654D18 → 0x653140` maps entry index to backing+0x290 plus index×0x2C and checks member flags. This is per-archive selection in list order, not a global best-selector search. |
| Read/open and reference scope | Read wrapper `0x647AC6 → 0x656690 → 0x6549A0` at `0x6566EC`. Status 0/3 fails this open; status 2 takes a separate filesystem-backed route. Normal archive open stores selected archive at file+0x130 (`0x656924`), copies 11 DWORDs to +0x134 (`0x65693D`), increments archive+0x38 (`0x6569AC`). The increment concerns the archive, not world-object lifetime; archive release/destruction enforcement is not closed here. Successful opens can invoke global callback `0x86663C`, context `0x866640`, at `0x6569FC`; registration/effects remain open. |

#### Evidence limits and continuation

**RUNTIME OBSERVED:** no new run, capture inspection, attach, input, native call
or memory write. Prior raw-observer RUNTIME PASS remains limited to its saved
capture. Location candidates stay UNQUALIFIED; profile location BLOCKED; unload
acceptance open; vendor/water runtime pending; reconnect unimplemented; R0.1 open.
No observer contract, navigation or controller ownership changes.

**INFERRED:** tooltip hiding adds a concrete conditional listener-removal route.
Distinct callback identities and resource-specific counts must not be treated
as proof of destroying or retaining the outer active references. No runtime
fault, unsafe overlap, bulk cleanup or nested field notification is established.

**UNKNOWN / SOURCE GAP:** actual widget classes, loaded assets/overrides, hover
and visibility state, full money/coin/child/layout/button effects; queued model
operation replay, callback-driven queue mutation and widget user-pointer lifetime;
nested pumping, world/descriptor/listener retention; actual archive opens/views/
selectors/mutations, earlier index/mode paths, open callback and signature
acceptance. Earlier resource-descendant/TLS boundaries, writer/invalidation
coverage, coherence, same-map/character/address ABA and zone/area freshness remain.

Reproduce with the pinned executable and `objdump -d -Mintel
--start-address=START --stop-address=STOP` (stop exclusive); use `objdump -s`
for tables. Re-extract these four members with the earlier pinned MPQ reader or
an extractor producing identical bytes; it is not an implementation of client
selection. Manual findings do not expand the automated 43-anchor/12-string
manifest; four qualification flags and runtimeObserved remain false.

| Inspection | START / STOP |
|---|---|
| Tooltip registration, construction and clearing | `0x495951 / 0x495962`; `0x495C60 / 0x495C88`; `0x529240 / 0x5293A5`; `0x5295D0 / 0x52964C`; `0x52A06B / 0x52A0AD`; `0x52FFE0 / 0x53026F`; `0x530A60 / 0x530A78`; `0x467FB0 / 0x468062`; `0x76A690 / 0x76A710` |
| Button and backdrop | `0x7791E0 / 0x77920B`; `0x779760 / 0x779883`; `0x777D30 / 0x777F51`; `0x7780D0 / 0x7782F1` |
| Model bindings, setter and counts | `0x76DEC0 / 0x76E0D5`; `0x76ECA0 / 0x76ED65`; `0x76CF50 / 0x76CFB6`; `0x76CC80 / 0x76CCDC`; `0x76CD30 / 0x76CDE7`; `0x711BB0 / 0x711BC9`; `0x7121A0 / 0x7122D5`; `0x710390 / 0x7103C2` |
| Model delivery | `0x76D7F0 / 0x76D86D`; `0x7074B0 / 0x7075DC`; `0x719370 / 0x719503`; `0x70A280 / 0x70A3F8`; `0x76D160 / 0x76D676`; `0x773110 / 0x77319E` |
| Archive insertion and selection | `0x655670 / 0x655690`; `0x6559AF / 0x655A33`; `0x656007 / 0x6560C4`; `0x655B19 / 0x655BE1`; `0x6549A0 / 0x654DBF`; `0x653140 / 0x653159`; `0x647AB2 / 0x647AEB`; `0x656690 / 0x656A08` |
| Tables / strings | `0x808F60 / 0x808FF0`; `0x854780 / 0x8547C4`; `0x8790C8 / 0x8790E0`; `0x81C7F8 / 0x81C898`; `0x878978 / 0x8789C0`; `0x81C608 / 0x81C6A8` |

Next task: prioritize the newly grounded tooltip listener-removal route. Trace
money/coin Hide scripts and remaining tooltip child/layout virtuals for bulk
cleanup or nested field notification while outer references remain live; resolve
the button+0x31C callback if reachable. Bound model queued-operation replay and
widget callback-context ownership before treating model counts as protection.
Use the conditional archive rule only when actual selection inputs are known;
retain loaded-script UNKNOWN otherwise. Keep SOURCE GAP until writer,
invalidation, lifetime, coherence and ABA obligations close. Manual unload
acceptance remains separate and needs module-absence/responsiveness evidence.

Validation: exact-client offline audit PASS; four asset lengths/hashes
independently matched. `python3 tools/validate.py --jobs 4` PASS, all 247 records
(111 C++ executables, 49 audit Python tests, 13 QuestDB Python tests, 10 Lua
fixtures, SQL/TSV fixture, full build and diff check). Report:
`/tmp/wow-validation-ylzhgbvd/results.json`. Separate `cmake --build build` PASS
for DLL, testhost, loader and GUI. Three read-only reviews reconciled; archive
count wording narrowed to avoid claiming unproved destruction semantics.
Full diff/status review and `git diff --check` PASS; exactly the two intended
documents changed, with no unrelated changes. Runtime NOT RUN; no qualification
upgrade.

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
