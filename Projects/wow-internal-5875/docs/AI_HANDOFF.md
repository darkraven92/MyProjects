# AI Handoff — wow-internal-5875

## Repository state and milestone

Actual branch: `codex/p07-world-zone-preparation`.
Starting checkpoint: `c749f15d1f0821cda0a1c9eb557ed2ac791ef321`.
Worktree: `/home/ludvig/Programming-worktrees/p07-world-zone/Projects/wow-internal-5875`.
Task: P0.7.5 offline leveling-profile catalogue and Orc starting-zone provenance,
2026-10-10. One agent; no subagents. Clean at start. Task files are uncommitted
at handoff preparation; the commit containing this file is the authoritative
checkpoint. Publication verification follows commit and is reported separately.

**Result: offline milestone complete; READY FOR P1: YES.** This means begin
Shared Questing + Grinding Core development with the validated contract and
injected observations. It does not qualify live selection, navigation or quest
execution. P0.7.4 location qualification remains BLOCKED; research was not reopened.

## Implemented foundation

- Strict offline schema v1, deterministic producer, pinned source manifests,
  per-field provenance, immutable loaded snapshot with revision digest/copy access.
- Reusable source/provenance/version/type/reference/relationship/profile validators;
  sorted error codes and paths; raw link versus prerequisite separation; signed
  active/rewarded OR-of-AND groups; unavoidable-cycle detection without flattening
  alternatives; actor conflicts and unsupported step/objective projection rejection.
- Canonical dataset: 12 quests, 23 entity references, external prerequisite 1499,
  ten-quest authored Orc Warrior profile with 35 steps covering Pickup,
  MoveToObjective, KillObjective, ItemObjective, TurnIn and GrindFallback.
- Native `LevelingProfileValidation` overload for catalogue quest-ID closure,
  with sorted missing profile/segment/quest references. Existing callers unchanged.
- QuestDB future-import preservation of reward slots, signed money, XP, spell
  modes, reputation/mail source fields and SQL SHA256; missing columns stay null.
- Full model/field inventory, quest/NPC/objective/chain/coordinate evidence and
  P1 consumption/reload boundaries in `docs/P07_OFFLINE_CATALOGUE.md`.

Quests represented and validated: 788 Cutting Teeth; 789 Sting of the Scorpid;
2383 Simple Parchment; 4641 Your Place In The World; 790/804 Sarkoth;
792 Vile Familiars; 794 Burning Blade Medallion; 805 Report to Sen'jin Village;
4402 Galgar's Cactus Apple Surprise; 5441 Lazy Peons; 6394 Thazz'ril's Pick.
Lazy Peons keeps special UseItemOnUnit semantics and is excluded from the basic
step profile along with its dependent 6394. Both remain full catalogue records.
External 1499 preserves 794's complete alternative prerequisite; it is not a
Warrior execution record. Next regional candidates are 786/808/817/818/826/823,
pending richer pinned data; no invented regional records were added.

## Evidence and limitations

### SOURCE VERIFIED

All canonical source-backed fields resolve to the pinned tracked Valley JSON or
enriched early_horde TSV; shared fields and actors must agree. Every known field
has provenance; authored policy remains separately labeled. Vile Familiars uses
structured count 12/min level 2, not the older handwritten 8/1. Simple Parchment
retains supplied item 12635. Sting retains both exported loot sources. Next-chain
hints do not invent prerequisites; 794 retains 792 OR 1499. Known signed money is
preserved (4402=50, 6394=150 copper); other reward families remain explicit null.
The schema can represent source slots and learn/cast spell distinctions.

### RUNTIME OBSERVED

No WoW run, attach, memory write, input or new capture in this milestone. Prior
**P0.7.4 RAW LOCATION LIFECYCLE OBSERVER — RUNTIME PASS** is preserved only within
`runtime-captures/p074-location-lifecycle-retry-2026-10-10/`, PID 296, session
`296.134360886272945680.57555570.500`, as documented in the world/location audit.
No new inspection or extension of that qualification. Stop/detach telemetry
still does not prove completed module unload.

### INFERRED / MANUALLY CURATED

Objective execution semantics, route order, level band/masks and grind candidates
are authored offline policy. SQL/export existence and source coordinates do not
prove runtime eligibility, navigation reachability, progress or executability.
Catalogue membership is not a completion fact or an ownership grant.

### UNKNOWN / SOURCE GAP

- B1 protected acquisition, B2 lifecycle/identity binding, B3 player-bound cache
  freshness remain unresolved. `ProfileWorldEvidence` map/positionMap/zone/
  worldGeneration remain unqualified; no live sampler or owner integration added.
- Original SQL/archive digest for the historical tracked exports is unavailable.
  The new manifest pins repository artifact bytes, not a current server snapshot.
- Item IDs are reference-only records; independent item templates/names remain
  unknown. Missing reward choices/guaranteed items/XP/spells/reputation are not
  treated as zero/none. The future-import enhancement does not backfill them.
- Special mechanics and script effects, live server match and end-to-end profile
  execution remain unqualified. Manual unload acceptance remains open.

These limit runtime enablement and broader content coverage, not starting P1's
shared core. No concrete offline blocker prevents beginning P1.

## Exact task files

- `data/leveling/orc_starting_authoring.json`
- `data/leveling/orc_starting_catalogue.json`
- `docs/AI_HANDOFF.md`
- `docs/P07_OFFLINE_CATALOGUE.md`
- `docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md`
- `src/Bot/LevelingProfile.h`
- `tests/leveling_profile_catalogue_reference_test.cpp`
- `tools/leveling/build_orc_catalogue.py`
- `tools/leveling/catalogue.py`
- `tools/questdb/quest_metadata.py`
- `tools/questdb/questdb.py`
- `tools/questdb/tests/rewards_test.py`
- `tools/tests/leveling_catalogue_test.py`

No controller/WorldMonitor/Detour source, historical QuestDB artifact, runtime
capture, reliability worktree, VMaNGOS file or sibling project changed.

## Validation

- TARGETED: 23 offline catalogue tests PASS; 15 QuestDB Python tests PASS
  (including two new reward/source tests); native catalogue-reference test PASS
  with C++20 `-Wall -Wextra -Werror`.
- SOURCE/DATA: producer `--check` and standalone catalogue validation PASS.
  All 598 evidence-bearing fields reviewed/validated: 339 source-backed,
  153 manually curated, 32 derived, 74 unknown, zero runtime-backed.
- FULL VALIDATION: `python3 tools/validate.py --jobs 4` PASS, all 249 records,
  including 112 C++ executables, 72 audit/catalogue Python tests, 15 QuestDB
  Python tests, SQL/TSV and Lua fixtures, plus the build. Report:
  `/tmp/wow-validation-22k2yxf4/results.json`.
- SEPARATE BUILD: subsequent `cmake --build build` PASS for DLL, testhost,
  loader and GUI.
- CATALOGUE REVISION: `5a4d0078f477046554e40947d1139a43ab980b1e75947db4ae245e8a97b61f07`.
- REVIEW: full task diff and untracked source/data/document review; no unrelated
  changes found; final staged list/diff checks required before commit.
- RUNTIME: NOT RUN; no new runtime behavior claim.

## Recommended next task

Begin P1 with a native immutable adapter for the validated step/quest contract
and pure shared questing/grinding state-transition policies using injected
observations. Gate unknown eligibility/rewards and unsupported mechanics. On
catalogue revision changes, invalidate bound profile/step/objective selections
and reset the observation tracker; never reuse a previous revision's authority.
Keep active controller ownership intact. Do not implement automatic execution
or travel until its independent evidence and safety boundaries are qualified.

Run `python3 tools/leveling/build_orc_catalogue.py --check` and
`python3 tools/leveling/catalogue.py data/leveling/orc_starting_catalogue.json` to
verify current offline inputs. Read the dedicated catalogue document for exact
schema and source limits. Reopen B1–B3 research only with direct new evidence;
no peripheral UI reverse engineering is recommended.

## Git and integration

Safe to commit: YES; full validation, separate build and task review passed.
Intended as one coherent offline milestone checkpoint; staged checks remain
mandatory before the commit. Safe to merge: NOT
established; no merge-target comparison/whole-branch runtime qualification and
no merge authorized. Manual runtime evidence required: not for these offline
contracts, yes before live location/travel and unresolved runtime acceptance.

Commit containing this handoff is authoritative. Do not recursively amend it to
record its own SHA or assert push verification before publication. After all
checks, commit only the exact task files, push the current branch explicitly,
compare local HEAD, `origin/codex/p07-world-zone-preparation` and live remote
SHA, inspect final worktree status, and report results. No merge/force-push.

VENDOR EPISODE — RUNTIME PENDING
WATER EMERGENCY EGRESS — RUNTIME PENDING
SOURCE GAP — RECONNECT NOT IMPLEMENTED
