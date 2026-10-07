import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from water_log_audit import audit


class WaterLogAuditTest(unittest.TestCase):
    def test_empty_is_not_runtime_pass_and_counters_unknown(self):
        result = audit([])
        self.assertFalse(result["waterRuntimeQualified"])
        self.assertFalse(result["waterEncounterObserved"])
        self.assertIsNone(result["counters"]["deaths"])

    def test_no_water_in_ground_or_ghost_logs(self):
        result = audit(["NAVMESH WATER-AWARE COMPLETE CORRIDOR RECOVERED",
                        "AFK DEAD/GHOST MOVEMENT ELIGIBLE reason=source_verified_ghost_water_walk",
                        "NAV WATER STATE to=SurfaceSwimming known=yes"])
        self.assertFalse(result["waterEncounterObserved"])
        self.assertEqual(result["events"], [])

    def test_completed_state_evidence_counts_not_dispatch(self):
        result = audit([
            "[INFO] WATER STATE from=DryGround to=EnteringWater known=yes",
            "[INFO] WATER STATE from=EnteringWater to=SurfaceSwimming known=yes",
            "[INFO] WATER STATE from=SurfaceSwimming to=Submerged known=yes",
            "[INFO] WATER MOVEMENT mode=ascend result=dispatched",
            "[INFO] WATER STATE from=Submerged to=SurfaceSwimming known=yes",
            "[INFO] WATER STATE from=SurfaceSwimming to=LeavingWater known=yes",
            "[INFO] WATER STATE from=LeavingWater to=DryGround known=yes",
        ])
        self.assertEqual(result["waterTransitions"], 6)
        self.assertEqual(result["surfaceSwimmingEntries"], 2)
        self.assertEqual(result["submergedEntries"], 1)
        self.assertEqual(result["surfaceRecoveries"], 1)
        self.assertEqual(result["shoreExits"], 1)
        self.assertTrue(result["waterEncounterObserved"])
        self.assertFalse(result["waterRuntimeQualified"])  # Safety/episode review still needed.

    def test_unknown_and_duplicate_state_are_not_success(self):
        result = audit(["WATER STATE from=Unknown to=SurfaceSwimming known=no",
                        "WATER STATE from=Submerged to=SurfaceSwimming",
                        "WATER STATE from=SurfaceSwimming to=SurfaceSwimming known=yes"])
        self.assertEqual(result["surfaceSwimmingEntries"], 0)
        self.assertEqual(result["surfaceRecoveries"], 0)
        self.assertFalse(result["waterEncounterObserved"])

    def test_press_release_and_no_fabricated_progress(self):
        result = audit(["WATER INPUT phase=press result=dispatched",
                        "WATER INPUT phase=release result=failed",
                        "WATER INPUT phase=release result=confirmed"])
        self.assertEqual(result["waterInputCommands"], 1)
        self.assertEqual(result["waterInputReleases"], 1)
        self.assertEqual(result["surfaceRecoveries"], 0)

    def test_failures_stale_abandon_and_breath(self):
        result = audit(["WATER SAFETY result=failed reason=stale_water_evidence",
                        "WATER EMERGENCY decision=fail_closed",
                        "WATER BREATH active=yes remaining=40000 maximum=60000 scale=10",
                        "WATER OBJECTIVE ABANDON owner=Grinding reason=water_traversal_unsupported"])
        for key in ("waterRecoveryFailures", "staleWaterEvidence", "waterEmergencyEvents",
                    "breathEvents", "waterObjectiveAbandons"):
            self.assertEqual(result[key], 1)
        self.assertEqual(result["submergedEntries"], 0)  # Active/refilling is NOT submerged.

    def test_cumulative_snapshots_not_summed_or_crossed_between_sessions(self):
        result = audit(["BOT SESSION START session=one",
                        "Autonomy14G4: movementRecoveries=4 runtimeRecoveries=5 deaths=2",
                        "Autonomy14G4: movementRecoveries=4 runtimeRecoveries=5 deaths=2",
                        "BOT SESSION START session=two",
                        "Autonomy14G4: movementRecoveries=0 runtimeRecoveries=0"])
        self.assertEqual(result["sessions"][0]["counters"]["runtimeRecoveries"], 5)
        self.assertEqual(result["counters"]["runtimeRecoveries"], 0)
        self.assertIsNone(result["counters"]["deaths"])

    def test_malformed_numbers_do_not_invent_zero(self):
        result = audit(["Autonomy14G4: runtimeRecoveries=unknown deaths=-1"])
        self.assertIsNone(result["counters"]["runtimeRecoveries"])
        self.assertIsNone(result["counters"]["deaths"])


if __name__ == "__main__":
    unittest.main()
