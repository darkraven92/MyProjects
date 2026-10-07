#!/usr/bin/env python3
"""Read-only cache/load performance audit of a preserved full log."""
import argparse
import json
from pathlib import Path
from navigation_log_audit import fields


def audit(lines):
    profiles, requests, invalidations, counters = [], [], [], {}
    warm, cold_full = {}, 0
    for number, line in enumerate(lines, 1):
        values = fields(line)
        if "NAV 14N.3 INIT PROFILE:" in line:
            record = dict(line=number, **values)
            profiles.append(record)
            disk = int(values.get("diskLoads", values.get("tilesOpened", 0)))
            mode = values.get("mode", "unknown")
            if values.get("success") == "yes":
                if disk > 0 and mode == "full_map":
                    cold_full += 1
                if disk == 0 and "cacheHits" in values:
                    warm.setdefault(mode, []).append(float(values["totalMs"]))
        elif "NAV CACHE map=" in line:
            requests.append(dict(line=number, **values))
        elif "NAV CACHE INVALIDATE" in line:
            invalidations.append(dict(line=number, **values))
        elif "Autonomy14G4:" in line:
            counters.update({k: values[k] for k in (
                "movementRecoveries", "runtimeRecoveries", "runtimeIdleDeadlocks",
                "runtimeStrategic", "runtimeEscalations") if k in values})
    return dict(fullMapColdLoadRequests=cold_full,
                coldTileLoads=sum(int(p.get("tilesLoaded", 0)) for p in profiles),
                diskLoads=sum(int(p.get("diskLoads", p.get("tilesOpened", 0))) for p in profiles),
                cacheMisses=(sum(int(p["newLoads"]) for p in requests) if requests else None),
                cacheHits=(sum(int(p["cacheHits"]) for p in profiles)
                           if profiles and all("cacheHits" in p for p in profiles) else None),
                addTileCalls=(sum(int(p["addTileCalls"]) for p in profiles)
                              if profiles and all("addTileCalls" in p for p in profiles) else None),
                warmInitializationMs=warm, counters=counters,
                profiles=profiles, requests=requests, invalidations=invalidations)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    with args.log.open(errors="replace") as stream:
        print(json.dumps(audit(stream), indent=2))


if __name__ == "__main__":
    main()
