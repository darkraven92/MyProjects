# P0.3.2 — session topology cache / incremental tile reuse

Updated 2026-10-07. SOURCE VERIFIED / TEST PASS; live performance RUNTIME
PENDING. This is topology reuse, not route reuse or relaxed path acceptance.

## Baseline full-log audit

Latest preserved full capture: 14,852 lines,
`/tmp/wow-p032-baseline.3WXFZs/wow-internal.log`, session
1792.134358658215650330.128405269.32. Start/stop monotonic
128405778/130183414: 29.6273 minutes. Final counters:
movementRecoveries=0, runtimeRecoveries=1, runtimeIdleDeadlocks=0,
runtimeStrategic=1, runtimeEscalations=0. P0.3 and P0.3.1 baseline RUNTIME PASS.

Nine completed full-map initializations each opened/added all 704 tiles:
123434 / 123237 / 129445 / 124117 / 123917 / 123758 / 123664 / 123633 /
123780 ms. Completed full-map initialization occupied 1118.984 seconds
(18.65 minutes) in this session. A tenth was cancelled after 652 tiles,
120125 ms. All initialization profiles together opened 8330 tiles. These are
initialization events, not nine physical stalls. Baseline has no cache metrics;
the audit reports missing cacheHits/addTileCalls as unknown, not measured zero.

The provider had private owning mesh/query pointers. BeginIncrementalCommon
called Shutdown on every tier; Shutdown freed the mesh. A new follower also
constructed a new provider. Thus even compatible later roam destinations
repeated disk reads and expensive addTile work. Raising the two-tiles-per-tick
budget would merely hide duplicate work and risk responsiveness.

## Lifetime and reference proof

Local primary source: VMaNGOS `dep/recastnavigation/Detour`, read-only audit.
`Source/DetourNavMesh.cpp:908` addTile takes a free tile slot, retains existing
tile indices/salts, joins neighbor links and returns getTileRef. Existing poly
references do not change when missing neighbors are added. removeTile:1233
increments salt; this cache never removes/replaces a tile in a live generation.
getTileRef/getPolyRefBase:1326–1353 encode salt/index. DT_TILE_FREE_DATA transfers
tile data ownership to the mesh. No raw file buffers are shared between meshes.

One function-static MapNavMeshSessionCache in the DLL owns the CURRENT map's
topology through shared entry handles. A provider owns its query/node pool and
borrows the entry's mesh; handles retain memory, not validity after invalidation.
Identity includes map ID, normalized directory, exact dtNavMeshParams bytes and
mmap file timestamp/size. Requested cached tiles must have matching file
timestamp/size and a still-valid Detour tile reference. Mismatches invalidate
the generation. Format/version checks on cold tile reads remain unchanged.

Destination, owner/intent, filter and route failure are NOT cache keys. Shutdown
detaches only query/route-loading work. Completed tiles survive cancellation
and failed path queries. Route points, corridors, blocked directed transitions,
last-safe/recovery state and command provenance remain follower-private and
reset on Start. No previous path is returned as a new query result.
The existing DeathRecovery learned-edge handoff now carries meshGeneration;
only a matching nonzero generation may preserve/seed raw directed poly refs.
Unknown or changed generations reject the seed. This is reference-lifetime
adaptation only, not a change to corpse routing, budgets or reclaim semantics.

Map/directory switches invalidate BEFORE reading the new header, including a
map with unavailable metadata. WorldMonitor invalidates on the first invalid
world snapshot; this deliberately treats an uncertain world gap conservatively.
A standalone scope guard invalidates on EVERY monitor return, including Stop
Bot, fault and teardown. DLL static destruction releases remaining storage.
Provider operations reject stale handles; path results carry meshGeneration;
terrain validation rejects foreign generations. Update/resume reject old
corridors, release intent with mesh_cache_invalidated, clear raw refs and detach
the query. A command-time lock/check prevents stale CTM dispatch.

The current world reader has no independent authoritative current-map field.
Do not substitute corpse-map globals. Map IDs supplied by existing owners are
validated as cache identities; world loading gaps revoke prior topology. This
checkpoint adds no invented map address or teleport inference.

## Loading, concurrency and hazard safety

Collect the EXISTING requested tile set, count compatible hits and queue ONLY
missing files. Route -> expanded -> full_map is additive. The incremental
cursor still allows at most TWO cold tiles per update. An all-hit request has
no artificial 704-tile polling delay; its first step prepares a private query.
Initialization observations count verified hits plus actual cold processing;
they remain initialization work, never physical progress or XP/kill outcome.

Every shared mesh/query operation and addTile step is serialized with the cache
recursive mutex. Queries/node pools are PRIVATE because Detour query scratch
is mutable. Blocking/offline initialization releases the lock between bounded
steps rather than locking an entire continent load. No game-thread sleep added.

Existing directed/hazard avoidance temporarily masks polygon flags. Its entire
mask/query/restore operation now holds the shared lock and uses exception-safe
RAII restoration. Other followers cannot observe temporary flags. Persistent
hazards remain spatial/query-time evidence, reprojected against the CURRENT
topology. Filters, hard-cell rejection, 14O.1 terrain validation and complete
route checks still run on every new plan; cache readiness is NOT path validity.

The optional Roam full-map fallback was audited but retained. A projection
failure alone does not publish proof that every relevant endpoint layer/tile
was covered by that request. No speculative owner-specific shortcut or change
to mandatory DeathRecovery/quest navigation was introduced. Once topology is
warm, this retained fallback has zero tile disk/addition work.

No GUI redesign: existing typed InitializationProgress/InitializationObservation
now include reused ready tiles. New cache statistics and sparse logs expose
requested, reused, missing, diskLoads, addTileCalls, generation and elapsed/work
time. A GUI presentation change is deferred rather than parsing logs for UI.

## Deterministic and real-topology verification

Portable cache tests cover 30 -> 90 -> 704 additive loads, warm routes, changed
destination/intent, detached handles, retained topology, map0/530/1 switch,
world/session invalidation, metadata/directory mismatch and invalid old handles.
Wiring tests protect private queries, route resets, generation checks, exception
restoration, lifecycle hooks and unchanged 2000/4/2 safety limits. Audit Python
tests distinguish unknown baseline metrics, partial cancellation, warm loads
and repeated counter snapshots. Full AFK/combat/death/acquisition tests stay green.

`navmesh_cache_probe` is an explicit EXCLUDE_FROM_ALL offline executable, using
the REAL provider, local Detour and mesh files under Wine. It never attaches to
WoW, issues input or writes gameplay data. Its own source/provider compile under
C++20 -Wall -Wextra -Werror; external Detour keeps the existing build flags
(its legacy memset(dtMeshTile) triggers class-memaccess under added Werror).
Run from a temporary directory after copying the executable so its logger
cannot overwrite the live runtime log.

Final production-margin probe:
`/tmp/wow-p032-final-probe.Rxh40f/wow-internal.log`.

| Request | Reused | New disk/addTile | Offline elapsed |
| --- | ---: | ---: | ---: |
| Cold route | 0 | 30 | 3134.765 ms |
| Warm route | 30 | 0 | 2.877 ms |
| Expanded | 30 | 60 | 10319.663 ms |
| Full map | 90 | 614 | 39701.935 ms |
| Later route | 30 | 0 | 2.215 ms |
| Warm expanded | 90 | 0 | 3.626 ms |
| Warm full map | 704 | 0 | 27.493 ms |

TEST PASS: unchanged existing poly ref after expansion; new provider/intent
retains mesh; simultaneous avoided/unrestricted queries restore masks; terrain
validation unchanged after masking; world invalidation rejects projection,
validation and command callback; failed map0 switch still invalidates map1.
The expected unavailable map0 header is a negative test, not skipped coverage.
After explicit invalidation, a new map1 cold request loaded 30 files, as required.

Full final validation PASS:91 strict C++ tests, nine navigation/cache Python
tests,13 local QuestDB Python tests, SQL and eight Lua fixtures;
`/tmp/wow-validation-in9p4f8p/results.json`. Separate build/diff PASS.
Isolated intended tree PASS:42 published strict C++ tests, nine Python tests,
SQL, three Lua fixtures and full DLL/GUI/loader/testhost build;
`/tmp/wow-validation-2yk04h_a/results.json` in
`/tmp/wow-p032-published.IkP9Qz`. Existing unrelated tests/features were neither
staged nor relied upon. Its standalone strict probe also PASS, log
`/tmp/wow-p032-published-probe.MQUqJc/wow-internal.log`: warm route1.581–1.726ms,
expanded2.739ms/full14.285ms, all with zero tile disk/addition work. The measured
variation is ordinary offline host/cache timing, not a different live gate.

These timings omit WorldMonitor's 250-ms tick spacing and are OFFLINE ONLY.
First cold live load still pays bounded tile work. RUNTIME PENDING for actual
WoW warm latency, session memory/responsiveness and >=20-minute regression.

## Protected budgets and runtime gate

MaximumPathLength=2000; surface recovery attempts=4; last-safe backtracks=2;
MaxTilesPerStep=2. No AFK, combat, DeathRecovery, terrain, hazard lifetime,
acquisition watchdog or strategic-outcome policy changed.

Fresh normal Grinding >=20 minutes; no intentionally bad terrain/death/stall.
Preserve the FULL capture before another GUI launch clears it. Optional view:

```bash
wine ./build/wow_gui.exe
tail -n 0 -F build/wow-internal.log | rg --line-buffered 'NAV CACHE|NAV 14N.3|Autonomy14G4|AFK |COMBAT|DEATH'
python3 tools/navmesh_cache_audit.py /path/to/preserved-full.log
```

Require one cold topology cost per compatible world generation, later same-map
full-map requests diskLoads=0/addTileCalls=0/cacheHits=704; warm route preferably
<5 seconds (INIT READY plus PLAN PROFILE, not just provider work); no false
initialization stall/acquisition idle debt; coherent ownership, strict terrain,
AFK prevention, combat stability and natural death recovery if encountered.
Report invalidation reasons, cold loads/cache hits/misses, warm latencies and
full-map cold-load requests together with counters. Do not hide genuine reloads.
