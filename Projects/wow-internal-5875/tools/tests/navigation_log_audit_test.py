import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from navigation_log_audit import audit, fields


class NavigationLogAuditTest(unittest.TestCase):
    def test_fields_preserve_owner_and_destination(self):
        self.assertEqual(fields('owner={full owner name} destination=-1,2,3 mode="full_map"'),
                         dict(owner="full owner name", destination="-1,2,3", mode="full_map"))

    def test_progressing_loader_not_ctm_stall(self):
        report = audit([
            'LOGGER SESSION session=one',
            'MOVEMENT INTENT intent=1 owner={Grind owner} purpose={roam} destination=1,2,3',
            'NAV 14N.3.1 INIT BEGIN mode=expanded tilesTotal=82 tilesProcessed=0',
            'NAV 14N.3.1 INIT PROGRESS mode=expanded tilesTotal=82 tilesProcessed=26',
            'AUTONOMY: MOVEMENT HARD-STALL RECOVERY state=Roaming',
            'MOVEMENT INTENT RELEASE intent=1 commands=0 replans=0 reason=no_path',
            'NAV 14N.3.1 INIT CANCELLED mode=expanded tilesProcessed=66 tilesTotal=82',
        ])
        stalls = [r for r in report['rows'] if r['event'] == 'owner_hard_stall']
        self.assertEqual(len(stalls), 1)
        self.assertEqual(stalls[0]['classification'], 'initialization_killed_as_movement_stall')
        self.assertEqual(stalls[0]['fingerprint'], 'unknown')
        self.assertEqual(stalls[0]['destination'], '1,2,3')

    def test_no_progress_evidence_is_not_invented(self):
        report = audit([
            'NAV 14N.3.1 INIT BEGIN mode=expanded tilesTotal=82 tilesProcessed=0',
            'AUTONOMY: MOVEMENT HARD-STALL RECOVERY state=Roaming',
            'NAV 14N.3.1 INIT CANCELLED mode=expanded tilesProcessed=0 tilesTotal=82',
        ])
        self.assertEqual(report['rows'][0]['classification'],
                         'initialization_cancelled_by_watchdog_unresolved')

    def test_projection_hazard_and_validation_are_distinct(self):
        report = audit([
            'NAVMESH 11B: path query failed.',
            'Reason: No ground polygon was found near the destination.',
            'NAV 14N.3 PLAN PROFILE: mode=route result=failed reason=no_path validationDetail=none',
            'NAVMESH 11B: path query failed.',
            'Reason: Persistent navigation hazard memory rejected every safe corridor.',
            'NAV 14N.3 PLAN PROFILE: mode=route result=failed reason=other_unknown validationDetail=none',
            'NAV 14N.3 PLAN PROFILE: mode=route result=failed reason=path_validation_failed '
            'validationDetail=unsafe_terrain_no_alternative',
        ])
        self.assertEqual([r['classification'] for r in report['rows']], [
            'destination_projection_failed', 'persistent_hazard_rejected',
            'unsafe_terrain_no_alternative'])

    def test_repeated_counter_snapshots_are_not_events(self):
        report = audit(['Autonomy14G4: movementRecoveries=16 runtimeRecoveries=42'] * 10)
        self.assertEqual(report['distribution'], [])
        self.assertEqual(report['counters']['movementRecoveries'], '16')

    def test_session_boundary_does_not_reuse_command(self):
        report = audit([
            'LOGGER SESSION session=one',
            'MOVEMENT INTENT intent=1 owner={Grind owner} destination=1,2,3',
            'LOGGER SESSION session=two',
            'AUTONOMY: MOVEMENT HARD-STALL RECOVERY state=Roaming',
        ])
        self.assertEqual(report['rows'][0]['intent'], 'unknown')


if __name__ == '__main__':
    unittest.main()
