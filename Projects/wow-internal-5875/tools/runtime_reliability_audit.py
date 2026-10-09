#!/usr/bin/env python3
"""Read-only event audit. Chronology is not proof of disconnect causation."""
import argparse
import json
from pathlib import Path
import re


def audit(lines):
    names = ("afkThresholdCrossings afkDeferred afkActions vendorFailures "
             "manualVendorBlocks vendorRetries vendorRecoveries worldGaps "
             "confirmedDisconnects reconnectAttempts reconnectSuccesses "
             "reconnectFailures authRequired worldRecoveries processExits").split()
    out = dict.fromkeys(names, 0)
    out.update(longestInputAgeMs=0, longestWorldGapMs=0, timeline=[],
               runtimeQualified=False, disconnectCause="unproven")
    gap = False
    process_exited = False
    for number, line in enumerate(lines, 1):
        fields = dict(re.findall(r"(\w+)=([^\s]+)", line))
        if "LOGGER SESSION" in line:
            gap = process_exited = False
        event = None
        for field in ("inputAge", "inputAgeMs"):
            value = fields.get(field, "")
            if value.isdigit():
                out["longestInputAgeMs"] = max(out["longestInputAgeMs"], int(value))
        if "AFK PRODUCTION THRESHOLD CROSSED" in line:
            event = "afkThresholdCrossings"
        elif "AFK PRODUCTION DEFER" in line:
            event = "afkDeferred"
        elif "AFK PRODUCTION VERIFY" in line and fields.get("result") == "confirmed":
            event = "afkActions"  # verified clock advance, never mere dispatch
        elif ("AFK ACTION RESULT" in line and fields.get("result") == "confirmed"
              and fields.get("reason") == "client_input_clock_advanced"):
            event = "afkActions"
        elif "GRIND 14G.1 VENDOR: FAILED" in line:
            event = "vendorFailures"
        elif "GRIND FULL BAG BLOCK state=entered" in line:
            event = "manualVendorBlocks"
        elif "MAINTENANCE WAIT state=retry" in line:
            event = "vendorRetries"
        elif "MAINTENANCE WAIT state=recovered" in line:
            event = "vendorRecoveries"
        if "DISCONNECT DIAGNOSTIC" in line:
            if fields.get("processAlive") == "no":
                if not process_exited:
                    event = "processExits"
                process_exited = True
            elif fields.get("snapshot") == "unavailable":
                if not gap:
                    event = "worldGaps"
                gap = True
            elif fields.get("snapshot") == "valid" and gap:
                event = "worldRecoveries"
                gap = False
            value = fields.get("incidentAgeMs", "")
            if value.isdigit():
                out["longestWorldGapMs"] = max(out["longestWorldGapMs"], int(value))
        # Future adapter events must be explicit. OM loss is NOT a reconnect.
        if "CONNECTION LIFECYCLE" in line:
            event = {
                "confirmed_disconnected": "confirmedDisconnects",
                "reconnect_attempt": "reconnectAttempts",
                "reconnect_verified": "reconnectSuccesses",
                "reconnect_failed": "reconnectFailures",
                "auth_required": "authRequired",
            }.get(fields.get("event"), event)
        if event:
            out[event] += 1
            # Never copy raw lines, credentials, or arbitrary field values.
            stamp = re.search(r"\b\d{2}:\d{2}:\d{2}\b", line)
            out["timeline"].append(dict(line=number, event=event,
                time=stamp.group() if stamp else None))
    return out


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    with args.log.open(errors="replace") as stream:
        print(json.dumps(audit(stream), indent=2))
