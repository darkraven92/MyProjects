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
    "waterEvidenceSnapshots", "swimmingTransitions", "submergedTransitions",
    "surfaceTransitions", "breathStartEvents", "breathStopEvents",
    "breathDrainObservations", "breathRefillObservations", "fatigueEvents",
    "unknownWaterStates", "groundExitEvidence",
)
EXISTING_COUNTERS = ("movementRecoveries", "runtimeRecoveries",
                     "runtimeIdleDeadlocks", "runtimeEscalations", "deaths")


def audit(lines):
    counts = dict.fromkeys(EVENT_COUNTS, 0)
    sessions = []
    current = {"session": "unknown", "counters": dict.fromkeys(EXISTING_COUNTERS)}
    events = []
    encounters = 0
    provenance = {"sourceVerified": 0, "runtimeObserved": 0, "inferred": 0}
    previous = {"swimming": None, "submerged": None, "surface": None}
    for number, line in enumerate(lines, 1):
        f = fields(line)
        if "BOT SESSION START " in line:
            if events or any(v is not None for v in current["counters"].values()):
                sessions.append(current)
            current = {"session": f.get("session", "unknown"),
                       "counters": dict.fromkeys(EXISTING_COUNTERS)}
            previous = {"swimming": None, "submerged": None, "surface": None}
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
        if event.startswith("WATER EVIDENCE "):
            counts["waterEvidenceSnapshots"] += 1
            for key in provenance:
                provenance[key] += f.get(key) == "yes"
            state = f.get("classification", "Unknown")
            counts["unknownWaterStates"] += state in ("Unknown", "SwimmingStateUnknown")
            for signal, known_field, value_field, counter in (
                ("swimming", "swimmingKnown", "swimming", "swimmingTransitions"),
                ("submerged", "submergedKnown", "submerged", "submergedTransitions"),
                ("surface", "surfaceKnown", "surface", "surfaceTransitions"),
            ):
                value = f.get(value_field) if f.get(known_field) == "yes" else None
                if value not in ("yes", "no"):
                    previous[signal] = None
                    continue
                if previous[signal] is not None and previous[signal] != value:
                    counts[counter] += 1
                previous[signal] = value
                if signal == "swimming" and value == "yes" and f.get("runtimeObserved") == "yes":
                    encounters += 1
            counts["groundExitEvidence"] += (f.get("groundContactKnown") == "yes" and
                                              f.get("groundContact") == "yes" and
                                              f.get("runtimeObserved") == "yes")
            if f.get("breathKnown") == "yes" and f.get("runtimeObserved") == "yes":
                try:
                    scale = int(f.get("breathScale", ""))
                except ValueError:
                    scale = 0
                counts["breathDrainObservations"] += scale < 0
                counts["breathRefillObservations"] += scale > 0
            counts["fatigueEvents"] += f.get("fatigueKnown") == "yes" and f.get("runtimeObserved") == "yes"
        elif event.startswith("WATER MIRROR TIMER "):
            if f.get("timer") == "BREATH":
                counts["breathStartEvents"] += f.get("event") == "start"
                counts["breathStopEvents"] += f.get("event") == "stop"
            if f.get("timer") == "EXHAUSTION":
                counts["fatigueEvents"] += 1
        elif event.startswith("WATER STATE "):
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
                evidenceProvenance=provenance,
                events=events)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    with args.log.open(errors="replace") as stream:
        print(json.dumps(audit(stream), indent=2))


if __name__ == "__main__":
    main()
