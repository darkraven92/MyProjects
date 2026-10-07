#!/usr/bin/env python3
"""Read-only water event accounting; never infer a water PASS from silence.

Counters are event counts, not commands-as-progress. Runtime qualification is
deliberately not automated: a full same-episode safety/physical/input review is
required. This tool supplies evidence counts and line references for that review.
"""
import argparse
import json
from pathlib import Path
from navigation_log_audit import fields


EVENT_COUNTS = (
    "waterTransitions", "surfaceSwimmingEntries", "submergedEntries",
    "surfaceRecoveries", "shoreExits", "waterRecoveryFailures",
    "waterEmergencyEvents", "breathEvents", "waterInputCommands",
    "waterInputReleases", "staleWaterEvidence", "waterObjectiveAbandons",
)
EXISTING_COUNTERS = ("movementRecoveries", "runtimeRecoveries",
                     "runtimeIdleDeadlocks", "runtimeEscalations", "deaths")


def audit(lines):
    counts = dict.fromkeys(EVENT_COUNTS, 0)
    sessions = []
    current = {"session": "unknown", "counters": dict.fromkeys(EXISTING_COUNTERS)}
    events = []
    encounters = 0
    for number, line in enumerate(lines, 1):
        f = fields(line)
        if "BOT SESSION START " in line:
            if events or any(v is not None for v in current["counters"].values()):
                sessions.append(current)
            current = {"session": f.get("session", "unknown"),
                       "counters": dict.fromkeys(EXISTING_COUNTERS)}
        if "Autonomy14G4:" in line or "deaths=" in line:
            for key in EXISTING_COUNTERS:
                value = f.get(key, "")
                if value.isdecimal():
                    current["counters"][key] = int(value)
        # Anchor to actual WATER events, not AFK ghost-waterwalk or NAV water queries.
        marker = line.find("WATER ")
        if marker < 0 or line[:marker].strip() not in ("", "[INFO]", "[WARN]", "[ERROR]"):
            continue
        event = line[marker:].strip()
        events.append({"line": number, "session": current["session"], "event": event})
        if event.startswith("WATER STATE "):
            before, after = f.get("from"), f.get("to")
            if before and after and before != after:
                counts["waterTransitions"] += 1
                if f.get("known") == "yes":
                    if after in ("SurfaceSwimming", "Submerged", "EnteringWater"):
                        encounters += 1
                    counts["surfaceSwimmingEntries"] += after == "SurfaceSwimming"
                    counts["submergedEntries"] += after == "Submerged"
                    # Completed transitions, never ascent/CTM dispatches.
                    counts["surfaceRecoveries"] += before == "Submerged" and after == "SurfaceSwimming"
                    counts["shoreExits"] += before == "LeavingWater" and after == "DryGround"
        elif event.startswith("WATER EMERGENCY "):
            counts["waterEmergencyEvents"] += 1
        elif event.startswith("WATER BREATH "):
            counts["breathEvents"] += 1
        elif event.startswith("WATER INPUT "):
            counts["waterInputCommands"] += f.get("phase") == "press"
            counts["waterInputReleases"] += f.get("phase") == "release" and f.get("result") == "confirmed"
        elif event.startswith("WATER OBJECTIVE ABANDON "):
            counts["waterObjectiveAbandons"] += 1
        if f.get("reason") in ("stale_evidence", "stale_water_evidence", "mesh_generation_changed"):
            counts["staleWaterEvidence"] += 1
        if event.startswith("WATER SAFETY ") and f.get("result") == "failed":
            counts["waterRecoveryFailures"] += 1
    sessions.append(current)
    return dict(**counts, counters=current["counters"], sessions=sessions,
                waterEncounterObserved=encounters > 0,
                waterRuntimeQualified=False,
                qualificationReason="manual_evidence_review_required" if encounters else "no_verified_water_encounter",
                events=events)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    with args.log.open(errors="replace") as stream:
        print(json.dumps(audit(stream), indent=2))


if __name__ == "__main__":
    main()
