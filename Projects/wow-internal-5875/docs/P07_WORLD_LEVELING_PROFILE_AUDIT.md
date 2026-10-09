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
deferred. No production file includes the new headers.

Applying restrictions or workload changes is a later, separate ownership
integration. WorldMonitor's deathRecoveryOwnedTick dispatch before
grindMode.Update (baseline lines 1484-1489) is where workload handoff is
coordinated, not permission to interrupt active combat, vendor or recovery.
GrindModeController and CombatController must continue to enforce their
existing target safety and aggressor rules. Quest owners retain quest
acquisition/execution; Detour/NavMesh retains long-distance navigation.

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
