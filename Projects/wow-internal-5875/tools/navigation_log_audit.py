#!/usr/bin/env python3
"""Read-only full-log navigation correlation. Events are not inferred successes."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re


def fields(line):
    return {m[0]: m[1] or m[2] or m[3] for m in re.findall(
        r'(\w+)=(?:\{([^}]*)\}|"([^"]*)"|([^\s]+))', line)}


def audit(lines):
    rows = []
    intents = {}
    current = {}
    pending = {}
    query_error = "unknown"
    state = "unknown"
    position = "unknown"
    last_hard = 0
    counters = {}
    sessions = []
    for number, line in enumerate(lines, 1):
        f = fields(line)
        if "LOGGER SESSION" in line:
            sessions.append(f.get("session", "unknown"))
            current, pending = {}, {}
            query_error, state, position, last_hard = "unknown", "unknown", "unknown", 0
        if "WorldState:" in line:
            position = f.get("pos", "unknown")
        if "Grind14G2:" in line:
            state = f.get("state", "unknown")
        if "GRIND 15A INTERRUPTION" in line and f.get("watchdogReason") in (
                "unexpected_seated_idle", "idle_deadlock") and f.get("event") != "watch":
            rows.append(dict(line=number, event="global_recovery", classification=f["watchdogReason"],
                             **context(current, f.get("grindState", state), position)))
        if "Autonomy14G4:" in line:
            counters.update({k: f[k] for k in ("movementRecoveries", "runtimeRecoveries",
                            "runtimeEscalations") if k in f})
        if "MOVEMENT INTENT intent=" in line:
            current = dict(f)
            intents[f["intent"]] = current
        if "NAV 14N.3.1 INIT BEGIN" in line:
            pending = dict(f)
            pending["progressSamples"] = 0
        if "NAV 14N.3.1 INIT PROGRESS" in line:
            if int(f.get("tilesProcessed", 0)) > int(pending.get("tilesProcessed", 0)):
                pending["progressSamples"] = pending.get("progressSamples", 0) + 1
            pending.update(f)
        if "NAVMESH 11B: path query failed." in line:
            query_error = "awaiting_reason"
        elif query_error == "awaiting_reason" and "Reason: " in line:
            query_error = line.split("Reason: ", 1)[1].strip()
        if "NAV 14N.3 PLAN PROFILE:" in line and f.get("result") == "failed":
            detail = f.get("validationDetail", "none")
            kind = detail if detail != "none" else {
                "No ground polygon was found near the destination.": "destination_projection_failed",
                "No ground polygon was found near the start position.": "start_projection_failed",
                "Could not resolve start/end polygon for blocked-route query.": "avoidance_projection_unresolved",
                "Detour could not build a polygon path.": "detour_no_path",
                "Persistent navigation hazard memory rejected every safe corridor.": "persistent_hazard_rejected",
            }.get(query_error, f.get("reason", "unknown"))
            rows.append(dict(line=number, event="plan_failure", classification=kind,
                             tier=f.get("mode"), error=query_error, **context(current, state, position)))
            query_error = "unknown"
        if "MOVEMENT HARD-STALL RECOVERY" in line:
            last_hard = number
            rows.append(dict(line=number, event="owner_hard_stall", classification="unresolved",
                             pending=dict(pending), **context(current, state, position)))
        if "MOVEMENT INTENT RELEASE" in line:
            record = intents.get(f.get("intent"), current)
            record.update(release=f)
            if f.get("intent") == current.get("intent"):
                current = record
        if "NAV 14N.3.1 INIT CANCELLED" in line:
            associated = bool(last_hard and 0 < number - last_hard <= 12)
            if associated:
                row = next(r for r in reversed(rows) if r["event"] == "owner_hard_stall")
                proven = (row["pending"].get("progressSamples", 0) > 0 and
                          current.get("release", {}).get("commands") == "0")
                row.update(classification=("initialization_killed_as_movement_stall" if proven
                           else "initialization_cancelled_by_watchdog_unresolved"),
                           cancellation=dict(f), release=current.get("release", {}))
            rows.append(dict(line=number, event="init_cancel", classification=(
                "owner_hard_stall" if associated else "other_owner_release"),
                cancellation=dict(f), **context(current, state, position)))
            pending = {}
        if "NAV 14N.3.1 INIT READY" in line:
            pending = {}
    groups = defaultdict(list)
    for row in rows:
        # Approximate repeated position is diagnostic only; not poly/transition identity.
        groups[(row["event"], row["classification"], row["position"])].append(row)
    return dict(sessions=sessions, counters=counters, rows=rows,
                distribution=[dict(event=event, classification=kind, count=count)
                    for (event, kind), count in Counter(
                        (r["event"], r["classification"]) for r in rows).most_common()],
                repeated_positions=[dict(event=k[0], classification=k[1], position=k[2],
                    count=len(v), lines=[r["line"] for r in v])
                    for k, v in groups.items() if len(v) > 1 and k[2] != "unknown"])


def context(intent, state, position):
    return dict(intent=intent.get("intent", "unknown"), owner=intent.get("owner", "unknown"),
                purpose=intent.get("purpose", "unknown"), state=state,
                position=position, destination=intent.get("destination", "unknown"),
                fingerprint="unknown")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    result = audit(args.log.read_text(errors="replace").splitlines())
    if args.json:
        print(json.dumps(result, indent=2))
        return
    print("Sessions: " + ", ".join(result["sessions"]))
    print("Final cumulative counters: " + json.dumps(result["counters"]))
    print("\n| Event unit | Classification | Count |\n| --- | --- | ---: |")
    for row in result["distribution"]:
        print(f"| {row['event']} | {row['classification']} | {row['count']} |")
    print("\n| Hard-stall line | Intent / purpose | State / position | Result | Tiles |\n"
          "| ---: | --- | --- | --- | --- |")
    for row in result["rows"]:
        if row["event"] != "owner_hard_stall":
            continue
        c = row.get("cancellation", {})
        print(f"| {row['line']} | {row['intent']} / {row['purpose']} | "
              f"{row['state']} / {row['position']} | {row['classification']} | "
              f"{c.get('tilesProcessed', '?')}/{c.get('tilesTotal', '?')} |")
    print("\nRoute fingerprints are UNKNOWN for pre-query cancellations; repeated positions "
          "do not prove repeated directed geometry. Counts from different event units must not be added.")


if __name__ == "__main__":
    main()
