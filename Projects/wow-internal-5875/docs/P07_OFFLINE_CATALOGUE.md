# P0.7 offline quest and leveling-profile catalogue

2026-10-10; branch `codex/p07-world-zone-preparation`; starting checkpoint
`c749f15`. One agent, no runtime acquisition research or controller changes.

The offline foundation is sufficient to **begin P1 Shared Questing + Grinding
Core**. It supplies a validated, versioned data contract and a bounded Orc
fixture for Pickup, MoveToObjective, KillObjective, ItemObjective, TurnIn and
GrindFallback. This is readiness to develop the shared core, not permission to
enable live navigation or a claim that an Orc leveling route runs successfully.
P0.7.4 live location qualification remains BLOCKED by B1–B3 in the
[world/location audit](P07_WORLD_LEVELING_PROFILE_AUDIT.md).

## A. Existing data model inventory

Evidence labels below apply to the *identified repository artifact*. The local
full SQL snapshot/SQLite database used for the historical exports is absent.
`bootstrap.fish` names `world_full_14_june_2021.7z`, patch 10, but does not prove
that these particular committed exports came from those exact archive bytes.
No server-current or exact-client content claim follows from a tracked export.
No online data, coordinate guesses or inferred linear quest chain was added.

| Model / owner | Available content | Authority and limits |
|---|---|---|
| `LevelingProfile` / `LevelingProfileSegment` | Profile/segment IDs, inclusive level band, optional map/zone/race/class/faction constraints, target entries/classification, XYZ grind box, preparation facts, quest IDs, next-segment hint, completion fact | Manually curated definitions. `LevelingProfileValidation` checks structure/links; new overload checks quest-ID membership against a caller-supplied validated catalogue. Completion/selection still require independent evidence. No quest execution steps fit this segment schema. |
| `QuestProfile` / `QuestObjectiveStep` | One quest with multiple objective rows, legacy primary objective, leaderboard slot, giver/turn-in, destinations, restrictions, relationship alternatives, runtime support classification | Mixed source-backed export data, derived classifications and authored mechanics. Live active-objective/leaderboard indices are runtime state, not catalogue data. |
| `ValleyOfTrialsProfiles` / `CrossroadsQuestProfiles` | Hand-authored quest mechanics, route groups, priorities, cooldowns/resource sources and search anchors | Manually curated. `VerifiedExistingSubsystem` labels and comments about runtime-observed seeds are historical claims, not new capture qualification. |
| `QuestProfileMergePolicy` | Generated ordinary content plus explicit mechanics overrides; acquisition enrichment; actor/semantic conflicts | Derived merge policy. Keep special 5441/6394 mechanics; no changes to merging or controller ownership in this task. |
| `tools/questdb/areas/*.json` | Area bounds/map, quest-ID sets or zone discovery, race/class filters, route group, priority, optional/late-wave, hub exit/unlock | Manually curated discovery/routing input. `hub_unlock_quest_id=805` for Sen'jin is bot policy, not a server prerequisite. |
| QuestDB SQL → SQLite → generated JSON | Quest fields, NPC/GO relations, objective slots/IDs/counts, creatures/objects, loot sources, spawn geometry | Source-backed at SQL import when present; import defaults can collapse absent legacy fields to zero. The older tracked JSON alone does not prove column presence. |
| `VanillaQuestDatabase` TSV loader | Q quest, M restrictions, N metadata, R relationships, O objectives, OS locations, G/T/D/S anchors, H enrichment-only, U ambiguity, A triggers, GC gossip | Source-backed values plus derived/exported semantics. Catalogue audit reports classification and graph issues; it is not a field-provenance validator. Legacy absent N fields stay unknown. |
| New offline catalogue v1 | Evidence-bearing quests/entities, explicit external quest references, ordered authored profiles, source manifest, immutable revision | Separate offline input contract, described below. Never automatically loaded by a runtime owner. |

| Fields / concepts | Existing source and new classification |
|---|---|
| Quest ID, title, patch, zone/sort, quest level | QuestDB `upsert_quest` → tracked Valley JSON; source-backed **to artifact**. Zone/sort may be a negative class category; it is not live zone evidence. |
| Minimum/maximum level, race/class masks, required condition | JSON plus agreement with enriched TSV Q/M; source-backed to artifacts. Zero race/class mask means no explicit mask in that artifact, not universal faction eligibility. Maximum 0 is unbounded in this schema. Race bit 2 is Orc; class bit 1 is Warrior in existing project mask tables. |
| Required faction/reputation, unexported conditions and script behavior | Unknown where not exported; preserve condition IDs and script IDs, do not interpret them as satisfied. |
| Giver/turn-in type and entry | JSON creature/GO relation arrays; derived typed references. Reproducer also requires agreement with TSV Q actor entries and N actor types. Multiple references are not collapsed by the canonical schema. |
| Entity names, map, XYZ, spawn GUID, orientation | Named JSON records/spawns, source-backed to artifact; every full spawn array retained. Coordinates are search evidence only, never movement waypoints, live locations or proof of reachability. |
| Item identity/name/template existence | Source-backed ID *occurrences* in objectives/source-item fields. `reference-only` registry scope; item names/template existence unknown. Importer does not import item_template. Internal reference validation is not independent item-template verification. |
| Raw objective kind, slot, creature/item/GO ID, required count, spell ID | JSON objective rows. Count/slot/spell/item fields source-backed. All loot-source IDs retained as derived sorted unique references. Drop rates and loot IDs remain in the pinned input, not required by the new step contract. |
| Execution objective kind / acquisition mode | Manually reviewed against exported rows and existing authored semantics. Distinguish kill, creature loot, world object, supplied item, and special use-item credit. Never infer that every creature-credit row means kill. |
| Source item/count/spell | JSON quest fields; source-backed. Supplied item IDs remain separate from creature credit and loot reward identities. |
| Previous/next quest, next-in-chain, breadcrumb | JSON + TSV agreement; raw signed values preserved. Next-in-chain is only a hint. No implicit prerequisite is added from 4641's next-in-chain=788. |
| Prerequisite alternatives, active/rewarded states, AND groups | Enriched TSV R, with N distinguishing known empty from missing enrichment. Source-backed *exported relationships*, originally derived by `quest_metadata.enrich` from signed previous and reverse-next links plus exclusivity. External alternatives are retained. `resolved=false` remains unresolved. |
| Method/flags/special flags, exclusive group, skill/value, start/complete script, reputation objective, time limit, money, breadcrumb | TSV N; source-backed where present, null where `-`. StartScript=804 for quest 804 is retained; it is not interpreted or cleared. |
| Money reward/required cost | Signed N `RewOrReqMoney` in copper; source-backed. Negative values represent a requirement, not a negative reward choice. |
| Item choices/guaranteed items, XP, spells, reputation rewards | Unknown for the tracked Orc exports. Canonical slots exist with explicit null provenance. New SQL imports preserve raw reward fields and slots with absent columns null. XP computation and reward selection remain P1 work. |
| Route order, profile level band/masks, grind targets, optionality, arrival radii, hub unlock/exit and priority | Manually curated policy, or derived by the existing exporter. Not proof of content prerequisites, live eligibility or route optimality. New profile explicitly cites its authoring artifact. |
| Current map/zone, player-bound position, availability, progress/completion, bag counts, selected reward | Runtime-backed only with separately qualified observations; unknown to this offline catalogue. Existing runtime readers are not imported as facts here. |

Existing `QuestObjectiveType` values are `TalkToNpc`, `KillMob`,
`CollectItemFromMob`, `CollectWorldItem`, `UseItemOnUnit`,
`UseQuestItemAtLocation`, `UseItemAtGameObject`, `InteractGameObject`,
`TravelReport`, `ExploreOrAreaTrigger`, and `Unknown`. The new basic P1 projection
supports kill and item objectives; quest Pickup/TurnIn cover report quests with
no objective rows. `UseItemOnUnit` is retained as catalogue semantics but rejected
as a basic P1 objective step. Escort, arbitrary script/spell and area-trigger
execution are not silently reduced to a supported step.

## B. Bounded Orc provenance dataset

Inputs are pinned by relative path, format and SHA256 in
[`orc_starting_catalogue.json`](../data/leveling/orc_starting_catalogue.json):

- `valley`: `data/questdb/generated/valley_of_trials_orc_warrior.json`.
- `enriched`: `data/questdb/runtime/early_horde.tsv`, strict Q/M/N/R adapter.
- `authoring`: [`orc_starting_authoring.json`](../data/leveling/orc_starting_authoring.json).

The latter is an authored fixture, not extracted server content. Each nontrivial
record field carries `value`, `status`, `refs`, `reason`, `transform`. JSON
pointers identify exact input fields. Grouped values such as a spawn array or
metadata record cite the complete source value, covering its children. Two direct
citations must agree; transforms are limited to direct, ordered collect, sorted
unique and typed actor relation-set agreement. Missing references/values fail validation.
Unknown values require null, a reason, no citations, and the unknown transform.
No field in the new production fixture is labeled runtime-backed.

All 12 quests have source-backed min/quest levels, max=0, race/class masks,
giver/turn-in references, counts and locations, and enriched money metadata.
“None” in prerequisites means no R prerequisite rows in the enriched artifact;
it does not bypass flags, conditions, class/faction or live dialog eligibility.

| Quest | Min / quest level | Giver → turn-in | Objectives retained | Prerequisites (rewarded unless noted) | Money copper |
|---|---:|---|---|---|---:|
| 4641 Your Place In The World | 1 / 1 | 10176 Kaltunk → 3143 Gornek | Report; zero objective rows | None; next-in-chain 788 is only a hint | 0 |
| 788 Cutting Teeth | 1 / 2 | 3143 Gornek → same | Kill creature 3098 Mottled Boar ×10 | None | 0 |
| 789 Sting of the Scorpid | 1 / 3 | 3143 Gornek → same | Item 4862 ×10; exported sources 3124 Scorpid Worker **and** 3281 Sarkoth | 788 | 0 |
| 2383 Simple Parchment | 1 / 1 | 3143 Gornek → 3153 Frang | Item 12635 ×1, supplied by quest; no invented loot creature | 788 | 0 |
| 790 Sarkoth | 1 / 5 | 3287 Hana'zua → same | Item 4905 ×1 from creature 3281 | None; next-in-chain 804 | 0 |
| 804 Sarkoth | 1 / 5 | 3287 Hana'zua → 3143 Gornek | Report; zero objective rows; retain StartScript=804 | 790 | 0 |
| 792 Vile Familiars | 2 / 4 | 3145 Zureetha Fargaze → same | Kill creature 3101 ×12 | None; signed nextQuest=794 and next-in-chain=794 | 0 |
| 794 Burning Blade Medallion | 1 / 5 | 3145 Zureetha Fargaze → same | Item 4859 ×1 from creature 3183 Yarrog Baneshadow | **792 OR 1499**, not AND; next-in-chain 805 | 0 |
| 805 Report to Sen'jin Village | 1 / 5 | 3145 Zureetha Fargaze → 3188 Master Gadrin | Report; zero objective rows | 794 | 0 |
| 4402 Galgar's Cactus Apple Surprise | 1 / 3 | 9796 Galgar → same | Item 11583 ×10 from GO 171938 | 788 | 50 |
| 5441 Lazy Peons | 3 / 4 | 11378 Foreman Thazz'ril → same | Use item 16114 on creature 10556 for credit ×5; supplied item requirement ×1 also retained | None; next-in-chain 6394 | 0 |
| 6394 Thazz'ril's Pick | 3 / 4 | 11378 Foreman Thazz'ril → same | Item 16332 ×1 from GO 178087 | 5441 | 150 |

Race/class masks are 0/0 except 4641 (130/0, Orc/Troll) and 2383
(50/1, Orc/Undead/Tauren Warrior). This does not expand the authored profile beyond
Orc Warrior. External quest 1499 is `Vile Familiars`, race/class masks 130/256
(Orc/Troll Warlock), retained solely to resolve the complete exported OR group.
It is not an executable profile quest. A Warrior cannot use that alternative;
792 remains the relevant branch. Equal titles never merge quest identities.

There are 23 entity records. Representative NPC locations below are the exact
first exported spawn, all on source map 1; full arrays and spawn GUIDs are in the
catalogue. Objective locations resolve through their creature/GO entities.
Supplied-item MoveToObjective resolves to the turn-in actor's location.

| NPC entry | X | Y | Z |
|---|---:|---:|---:|
| 10176 | -607.434 | -4251.33 | 39.0393 |
| 3143 | -600.132 | -4186.19 | 41.2663 |
| 3153 | -639.344 | -4230.19 | 38.5605 |
| 3287 | -397.761 | -4108.99 | 50.2876 |
| 3145 | -629.052 | -4228.06 | 38.2334 |
| 3188 | -825.636 | -4920.76 | 19.7409 |
| 9796 | -561.628 | -4221.8 | 41.6737 |
| 11378 | -611.587 | -4322.08 | 40.0927 |

Reconciled differences: the historical manual sketch says Vile Familiars ×8 and
minimum level 1; both tracked structured exports say ×12 and minimum level 2.
The offline dataset uses the agreeing structured artifacts, with no claim to
resolve a live server mismatch. Simple Parchment is a supplied-item requirement,
even though the handwritten profile describes TalkToNpc and the generic TSV
primary objective is Unknown. Lazy Peons' raw creature credit is explicitly
interpreted as use-item by the existing authored override, not as a kill.

The ordered ten-quest fixture is 4641, 788, 2383, 789, 4402, 790, 804, 792, 794,
805, plus a GrindFallback candidate over creatures 3098/3124. It has 35 steps.
It is an authored offline exercise of the contract, not an optimized route or
a must-execute linear script. Conditions/levels/ownership still govern future
selection. Lazy Peons and its dependent Thazz'ril's Pick remain full catalogue
records but are excluded from this basic profile. The latter's item objective
fits the schema, but its prerequisite requires the unsupported special action.

The next regional candidate IDs already present in `orc_starting_route.json`
are 786, 808, 817, 818, 826, 823. Their authored hub rules and runtime TSV records
are existing evidence, but the bounded rich Valley export does not contain them.
They are expansion candidates, not fabricated fully proven records in this
milestone. Expansion should use a pinned richer export with template/reward
coverage and retain all alternative prerequisites.

## C. Loading and reusable validation

[`catalogue.py`](../tools/leveling/catalogue.py) is the canonical offline v1
loader/validator. It emits stable `Issue(path, code, message)` diagnostics and
fails closed for unsupported versions, fields, source formats, JSON duplicate
keys, nonfinite values, missing provenance or source drift. Source paths must
resolve inside the project. Provenance is validated before semantic data is
used. It reads committed files only and never connects to WoW, VMaNGOS or SQLite.

Semantic checks cover:

- Quest/entity/profile identities, duplicate records, masks, level bands,
  supported objective kinds, counts, slot and spell shapes.
- Giver/turn-in, creature/item/GO references; source-item association; reward
  item references; finite map-associated geometry; objective location closure.
- Signed raw quest references, self/missing prerequisite references, OR-of-AND
  groups, active versus rewarded states. A fixed-point reachability check finds
  unavoidable prerequisite cycles without falsely rejecting an OR escape.
  Unknown/external/unresolved relationships are possible graph roots, **not**
  proof of eligibility. Chain hints are never executed as prerequisites.
- Exact per-step shape, pickup/objective/turn-in order and coverage, objective
  kind compatibility, level/race/class feasibility, duplicate IDs and duplicate
  semantic steps even if renamed, and conflicts with giver/turn-in metadata.
- Source changes, two-source value conflicts and producer/committed-output drift.

`Catalogue` holds immutable serialized content and a SHA256 revision. Reading
`records` returns a fresh copy. Quest/entity/profile identity arrays normalize
their order; authored step order remains significant. No timestamps or absolute
machine paths appear in generated data. Reordering those identity arrays does
not change the snapshot revision. A source/content change does.

`LevelingProfileValidation::ValidCatalogue(profiles, catalogueQuestIds)` adds
native reference closure without changing existing structural-only callers.
`MissingQuestReferences` returns sorted, deduplicated profile/segment/quest
identities. Only a validated snapshot should provide the set; membership alone
does not prove provenance, quest executability or completion. The ordered step
schema is separate from `LevelingProfileSegment`, and no lossy automatic cast is
provided. This keeps existing selection policy and runtime ownership intact.

Tests live in `tools/tests/leveling_catalogue_test.py`,
`tools/questdb/tests/rewards_test.py` and
`tests/leveling_profile_catalogue_reference_test.cpp`. Synthetic malformed
fixtures explicitly repin synthetic source values so reference/type errors can
be tested independently of provenance errors. Production corruption tests never
repin their evidence. Coverage includes source/version drift, immutable revision
invalidation, malformed types, duplicate keys/steps, broken references, actor
conflicts, signed/cyclic/alternative prerequisites, supplied items and unsupported
special-objective projection. `tools/validate.py` discovers all three suites.

Reproduction and targeted checks:

```sh
python3 tools/leveling/build_orc_catalogue.py --check
python3 tools/leveling/catalogue.py data/leveling/orc_starting_catalogue.json
python3 -m unittest discover -s tools/tests -p leveling_catalogue_test.py -v
python3 -m unittest discover -s tools/questdb/tests -p '*_test.py' -v
```

After changing an input, run the builder without `--check`, review the entire
source/data diff and rerun validation. Updating a digest is not approval of new
content. v1 TSV interpretation/provenance rules must be versioned if their
meaning changes. Runtime reload is not implemented: a future consumer must
validate a complete replacement snapshot atomically, compare revisions, and
invalidate selected profile/step/objective bindings and reset
`LevelingProfileObservation` before reuse. Preserve any active controller owner;
reject a failed replacement and prohibit commands based on a stale revision.

## D. Reward/source preservation for subsequent imports

`quest_metadata.py` now preserves raw quest reward columns in `quest_rewards`,
and `questdb.quest_record` exposes `reward_source`. Missing columns remain null,
including older SQLite databases. Preserve six choice-item slots, four guaranteed
item slots, five reputation slots, signed money, maximum-level money, XP,
learn/cast spell distinction and mail metadata. Zero differs from absence. This
is raw source preservation, not a choice policy or a calculated XP promise.
Canonical non-null item rewards carry explicit source slot, item ID and count;
spell rewards distinguish learn from cast, and reputation entries retain slot,
faction and source value. Highest applicable quest patch still owns the reward row. Existing runtime TSV
semantics and controller consumers are unchanged.

`questdb.import_sql` additionally stores the imported SQL SHA256 in `meta`.
Column names were checked against local VMaNGOS `quest_template` migration
INSERT schemas (read only); synthetic tests prove importer behavior. No VMaNGOS
files changed, database download/import of real world content occurred, or
historical export was regenerated. The new hash applies to future imports,
not retroactively to the current Orc artifacts. A future pinned raw export can
fill the current unknown reward slots with separately reviewed evidence.

## E. P1 boundary, evidence and acceptance

**SOURCE VERIFIED:** the offline implementation, artifact references and
reproducible dataset; native reference closure; SQL reward preservation behavior.
**RUNTIME OBSERVED:** no new runtime session or capture in this milestone. The
prior P0.7.4 raw observer PASS remains limited to its historical capture.
**INFERRED / AUTHORED:** reviewed objective semantics, profile sequence and grind
candidates; future executability is not inferred from validation.
**UNKNOWN / SOURCE GAP:** original export SQL digest, independent item templates,
unexported reward choices/XP/spells/reputation, live server match, live progress,
current map/zone/position association, navigation reachability and end-to-end
execution. All remain explicit; no unknown was upgraded to runtime PASS.

The six step records carry the following inputs for a future shared core:

| Step | References / policy |
|---|---|
| Pickup | Quest and relation-checked actor; restrictions, flags and prerequisite alternatives come from quest record |
| MoveToObjective | Quest/objective → all source entities and their map/XYZ candidates; supplied items resolve to turn-in actor |
| KillObjective | Quest/objective → creature identities and required count |
| ItemObjective | Quest/objective → item/count and loot/world-object/supplied mode; inventory and loot observations remain external |
| TurnIn | Quest and relation-checked actor; signed money plus nullable reward metadata; reward choices cannot be guessed |
| GrindFallback | Explicit creature candidates and source location reference; live safety, level suitability and navigation remain existing owners' responsibilities |

P1 can begin with a native immutable adapter and pure state/transition policies
using this contract and synthetic observations. It must gate unknown metadata,
external/special prerequisites, reward choices and movement evidence explicitly.
Do not auto-run the ten-quest fixture, wire a location sampler, bypass existing
quest/death/combat/recovery/vendor ownership, or replace Detour/NavMesh.

Remaining P0.7 live qualification blockers are B1 protected acquisition, B2
lifecycle/identity binding, and B3 player-bound cache freshness. They block live
map-aware selection/travel, not development of the offline shared core. Manual
unload acceptance and unrelated vendor/water/reconnect qualifications retain
their prior statuses. P0.7's offline catalogue/provenance block is complete;
P0.7.4 live location qualification is deliberately not declared complete.

Validation results for this checkpoint are recorded in `docs/AI_HANDOFF.md`.
Publication verification happens after committing; the containing commit is the
checkpoint identity. No merge is performed or authorized by this document.
