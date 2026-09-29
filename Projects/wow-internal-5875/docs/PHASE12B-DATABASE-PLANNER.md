# Phase 12B — Database-driven Quest Planner

## Architecture

```text
VMaNGOS world SQL
  -> tools/questdb/questdb.py
  -> data/questdb/vanilla.sqlite
  -> data/questdb/runtime/valley_of_trials.tsv
  -> Bot::VanillaQuestDatabase
  -> ValleyOfTrialsProfiles compatibility facade
  -> QuestPlanner / GenericQuestDiscoveryController
  -> generic objective executors / Phase 11C turn-in
```

The runtime DLL does not link sqlite3. SQLite is used by the importer/query tooling; the bot consumes a compact generated snapshot. This keeps the injected x86 DLL dependency surface unchanged.

## What the runtime catalog contains

Each database-derived quest includes, when available:

- quest id/title/min level;
- giver entry and spawn seed;
- turn-in entry and spawn seed;
- objective type inferred from the Vanilla quest + loot tables;
- target creature/gameobject/item IDs and count;
- creature/gameobject objective spawn seeds;
- gameobject type + loot id metadata;
- local-vs-zone-exit route classification.

Runtime live state still wins over DB coordinates. Database coordinates are search seeds only; once the NPC/unit/GameObject is in ObjectManager, its live GUID and XYZ are used.

## Objective inference in Phase 12B

- item whose loot source is a creature -> `CollectItemFromMob`
- item whose loot source is a GameObject -> `CollectWorldItem`
- direct creature objective -> `KillMob`
- direct GameObject objective -> `InteractGameObject`
- zero-objective quest with a turn-in NPC -> `TravelReport`
- unresolved/special semantics -> hand-authored fallback if one exists, otherwise `Unknown`

`UseItemOnUnit` and other special quest mechanics remain explicit overrides until their database semantics are modeled safely.

## Valley exit policy

Because database profiles are visible to the existing giver-audit controller, the audit can visit every eligible database quest giver with a known spawn seed. A `SenjinRoad`/zone-exit objective remains blocked until the local giver sweep is complete.

This is materially different from the old visibility-only approach: an off-screen giver can now be audited using the database spawn seed.

## CollectWorldItem reliability change

The previous executor did this after one successful native function invocation:

```text
right-click -> wait -> slots=0 -> blacklist GUID -> move on
```

Phase 12B changes it to:

```text
right-click -> wait
  -> object disappeared OR loot slots observed: advance
  -> object still exists and slots=0: retry same exact GUID (bounded)
  -> maximum interaction attempts reached: blacklist and continue
```

The live quest-log complete flag remains the final completion truth.

## Runtime catalog path

Default:

```text
data/questdb/runtime/valley_of_trials.tsv
```

The DLL also resolves it relative to `wow_internal.dll` in the project `build/` directory. For diagnostics/overrides, set:

```text
WOW_INTERNAL_QUESTDB_CATALOG=<path-to-catalog>
```

before launching WoW/Wine.

## Validation boundary

The package includes Python/fixture tests and the C++ catalog/planner path was host-syntax-tested against the generated fixture catalog. The user's real `cmake --build build` remains the authoritative project compile test, and actual WoW interaction remains runtime verification.
