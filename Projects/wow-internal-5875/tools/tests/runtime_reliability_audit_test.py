import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from runtime_reliability_audit import audit


class RuntimeReliabilityAuditTest(unittest.TestCase):
    def test_incident_does_not_invent_disconnect_or_reconnect(self):
        report = audit([
            "22:07:02 AFK PRODUCTION THRESHOLD CROSSED inputAge=300051 blockedReason=vendor",
            "AFK PRODUCTION DEFER owner=Grinding reason=vendor",
            "GRIND 14G.1 VENDOR: FAILED",
            "GRIND FULL BAG BLOCK state=entered freeSlots=0",
            "22:32:05 inputAge=1800329 antiAfkActions=0",
            "22:32:23 DISCONNECT DIAGNOSTIC processAlive=yes snapshot=unavailable incidentAgeMs=0",
            "DISCONNECT DIAGNOSTIC processAlive=yes snapshot=unavailable incidentAgeMs=2000",
            "05:37:17 DISCONNECT DIAGNOSTIC processAlive=yes snapshot=valid incidentAgeMs=25494000 classification=snapshot_recovered_cause_unknown",
            "DISCONNECT DIAGNOSTIC processAlive=no snapshot=unavailable",
        ])
        for key in ("afkThresholdCrossings", "afkDeferred", "vendorFailures",
                    "manualVendorBlocks", "worldGaps", "worldRecoveries", "processExits"):
            self.assertEqual(report[key], 1)
        self.assertEqual(report["longestInputAgeMs"], 1800329)
        self.assertEqual(report["longestWorldGapMs"], 25494000)
        self.assertEqual(report["confirmedDisconnects"], 0)
        self.assertEqual(report["reconnectSuccesses"], 0)
        self.assertFalse(report["runtimeQualified"])

    def test_dispatch_is_not_delivery(self):
        report = audit(["AFK INPUT result=dispatched",
                        "AFK PRODUCTION VERIFY result=pending",
                        "AFK PRODUCTION VERIFY advanced=yes result=confirmed"])
        self.assertEqual(report["afkActions"], 1)
        live = audit(["AFK ACTION RESULT result=confirmed reason=client_input_clock_advanced",
                      "AFK ACTION RESULT result=confirmed reason=client_and_server_afk_clear"])
        self.assertEqual(live["afkActions"], 1)

    def test_retry_and_external_resolution(self):
        report = audit(["MAINTENANCE WAIT state=retry attempt=1",
                        "MAINTENANCE WAIT state=recovered",
                        "CONNECTION LIFECYCLE event=auth_required password=secret"])
        self.assertEqual(report["vendorRetries"], 1)
        self.assertEqual(report["vendorRecoveries"], 1)
        self.assertEqual(report["authRequired"], 1)
        self.assertNotIn("secret", json.dumps(report))
        self.assertNotIn("password", json.dumps(report))

    def test_short_gap_and_loading_are_not_confirmed_disconnect(self):
        report = audit(["DISCONNECT DIAGNOSTIC snapshot=unavailable",
                        "loading=yes",
                        "DISCONNECT DIAGNOSTIC snapshot=valid incidentAgeMs=250"])
        self.assertEqual(report["worldGaps"], 1)
        self.assertEqual(report["confirmedDisconnects"], 0)
        self.assertEqual(report["reconnectAttempts"], 0)


if __name__ == "__main__":
    unittest.main()
