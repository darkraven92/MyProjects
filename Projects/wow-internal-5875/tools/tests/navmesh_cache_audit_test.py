import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from navmesh_cache_audit import audit


class NavMeshCacheAuditTest(unittest.TestCase):
    def test_baseline_missing_metrics_are_unknown(self):
        result = audit(['NAV 14N.3 INIT PROFILE: mode=full_map tilesOpened=704 '
                        'totalMs=123434.497 success=yes'] * 9)
        self.assertEqual(result['fullMapColdLoadRequests'], 9)
        self.assertEqual(result['diskLoads'], 6336)
        self.assertIsNone(result['cacheHits'])
        self.assertIsNone(result['addTileCalls'])
        self.assertIsNone(result['cacheMisses'])
        self.assertEqual(result['warmInitializationMs'], {})

    def test_additive_and_warm_requests(self):
        result = audit([
            'NAV CACHE map=1 generation=1 requestedTiles=90 alreadyLoaded=30 newLoads=60 mode=expanded',
            'NAV 14N.3 INIT PROFILE: mode=expanded diskLoads=60 cacheHits=30 addTileCalls=60 totalMs=4000 success=yes',
            'NAV 14N.3 INIT PROFILE: mode=full_map diskLoads=614 cacheHits=90 addTileCalls=614 totalMs=90000 success=yes',
            'NAV 14N.3 INIT PROFILE: mode=route diskLoads=0 cacheHits=25 addTileCalls=0 totalMs=12 success=yes',
            'NAV 14N.3 INIT PROFILE: mode=expanded diskLoads=0 cacheHits=90 addTileCalls=0 totalMs=15 success=yes',
            'NAV 14N.3 INIT PROFILE: mode=full_map diskLoads=0 cacheHits=704 addTileCalls=0 totalMs=25 success=yes',
        ])
        self.assertEqual(result['diskLoads'], 674)
        self.assertEqual(result['addTileCalls'], 674)
        self.assertEqual(result['cacheMisses'], 60)
        self.assertEqual(result['cacheHits'], 939)
        self.assertEqual(result['fullMapColdLoadRequests'], 1)
        self.assertEqual(result['warmInitializationMs'], dict(route=[12], expanded=[15], full_map=[25]))

    def test_cancelled_load_not_ready_and_counters_not_added(self):
        result = audit([
            'NAV 14N.3 INIT PROFILE: mode=full_map diskLoads=12 cacheHits=0 addTileCalls=12 totalMs=25 success=no',
            'NAV CACHE INVALIDATE map=1 generation=1 reason=world_unload_or_snapshot_gap',
            'Autonomy14G4: movementRecoveries=0 runtimeRecoveries=1 runtimeIdleDeadlocks=0',
            'Autonomy14G4: movementRecoveries=0 runtimeRecoveries=1 runtimeIdleDeadlocks=0',
        ])
        self.assertEqual(result['fullMapColdLoadRequests'], 0)
        self.assertEqual(result['diskLoads'], 12)
        self.assertEqual(result['counters']['runtimeRecoveries'], '1')
        self.assertEqual(len(result['invalidations']), 1)
