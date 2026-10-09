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
               passiveWorldRecoveries=0, vendorTrips=[], movementSamples=[],
               bagObservations=[], runtimeQualified=False, disconnectCause="unproven")
    gap = False
    process_exited = False
    trip = None
    last_utc = None
    last_bags = None
    failure_pending = False
    for number, line in enumerate(lines, 1):
        fields = dict(re.findall(r"(\w+)=([^\s]+)", line))
        utc = re.search(r"\butc=(\d{4}-\d{1,2}-\d{1,2}T\d{1,2}:\d{2}:\d{2}Z)\b", line)
        if utc:
            last_utc = utc.group(1)
        if "LOGGER SESSION" in line:
            gap = process_exited = False
            trip = None
            failure_pending = False
            last_bags = last_utc = None
        if "GRIND 14G.1 VENDOR: START" in line:
            trip = dict(startLine=number, precedingUtc=last_utc, interactions=[],
                        merchantOpenObserved=False, failureReason=None)
            out["vendorTrips"].append(trip)
        if trip is not None:
            if "VENDOR: interaction issued" in line:
                entry = fields.get("entry", "")
                distance = fields.get("distance", "")
                attempt = fields.get("attempt", "")
                if (entry.isdigit() and re.fullmatch(r"\d+(?:\.\d+)?", distance)
                        and re.fullmatch(r"\d+/\d+", attempt)):
                    trip["interactions"].append(dict(line=number, entry=int(entry),
                        distance=float(distance), attempt=attempt, precedingUtc=last_utc))
            if "VENDOR: MerchantFrame open." in line:
                trip["merchantOpenObserved"] = True
            if "GRIND 14G.1 VENDOR: FAILED" in line:
                trip["failureLine"] = number
                failure_pending = True
            elif failure_pending and "Reason:" in line:
                # Closed vocabulary: never copy raw text / potential secrets.
                trip["failureReason"] = next((reason for text, reason in (
                    ("MerchantFrame did not open after bounded interaction retries.", "merchant_frame_not_open"),
                    ("NavMesh return from vendor to grind home failed.", "return_navigation_failed"),
                ) if text in line), "unclassified")
                failure_pending = False
        if "movementFlags" in fields:
            flags = fields["movementFlags"]
            if re.fullmatch(r"0x[0-9a-fA-F]{1,8}", flags):
                # No carried-forward inference: each sample belongs to THIS line.
                out["movementSamples"].append(dict(line=number, raw=int(flags, 16),
                    precedingUtc=last_utc))
        if "GRIND 14G.2: bags used=" in line:
            bag = tuple(fields.get(k, "") for k in ("used", "free", "total"))
            if all(v.isdigit() for v in bag) and bag != last_bags:
                out["bagObservations"].append(dict(line=number, used=int(bag[0]),
                    free=int(bag[1]), total=int(bag[2]), precedingUtc=last_utc))
                last_bags = bag
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
                # This observer has not proved ANY bot reconnect action chain.
                out["passiveWorldRecoveries"] += 1
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
                time=stamp.group() if stamp else None,
                utc=utc.group(1) if utc else None, precedingUtc=last_utc))
    return out


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    with args.log.open(errors="replace") as stream:
        print(json.dumps(audit(stream), indent=2))
