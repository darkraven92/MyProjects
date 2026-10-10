#!/usr/bin/env python3
"""Reproducible local validation; never stages, commits, or controls WoW."""
import argparse
import concurrent.futures
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
LUA_PRODUCERS = {
    "living_attack_evidence_fixture.lua": "living_recovery_evidence_test",
    "combat_action_evidence_fixture.lua": "combat_liveness_policy_test",
    "combat_bootstrap_input_fixture.lua": "combat_initiation_policy_test",
    "afk_dead_ghost_fixture.lua": "afk_dead_ghost_policy_test",
    "afk_safe_input_fixture.lua": "afk_protection_policy_test",
    "equipment_upgrade_fixture.lua": "equipment_upgrade_policy_test",
    "equipment_durability_fixture.lua": "equipment_durability_probe_test",
    "quest_maintenance_sale_fixture.lua": "quest_maintenance_policy_test",
    "quest_dialog_diagnostics_fixture.lua": "quest_dialog_diagnostics_test",
    "class_trainer_fixture.lua": "class_trainer_policy_test",
    "vendor_metadata_fixture.lua": "vendor_metadata_full_bag_test",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    if not 1 <= args.jobs <= 16:
        parser.error("--jobs must be between 1 and 16")
    output = Path(tempfile.mkdtemp(prefix="wow-validation-"))
    print(f"Validation artifacts: {output}", flush=True)
    results = []

    def run(name, command, *, data=None, timeout=600):
        try:
            process = subprocess.run(command, cwd=ROOT, input=data,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     timeout=timeout, check=False)
            log = process.stdout
            code = process.returncode
        except (OSError, subprocess.TimeoutExpired) as error:
            code, log = 1, str(error).encode()
        (output / f"{name}.log").write_bytes(log)
        return {"name": name, "pass": code == 0, "returncode": code,
                "command": [str(arg) for arg in command]}, log

    def cpp(source):
        binary = output / source.stem
        result, _ = run(source.stem + "_compile", [args.cxx, "-std=c++20",
            "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)])
        records = [result]
        if result["pass"]:
            result, _ = run(source.stem, [str(binary)], timeout=120)
            records.append(result)
        return source.stem, records

    sources = sorted((ROOT / "tests").glob("*_test.cpp"))
    if not sources:
        raise SystemExit("No C++ tests found")
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for name, records in pool.map(cpp, sources):
            results.extend(records)
            print(f"C++ {'PASS' if all(r['pass'] for r in records) else 'FAIL'} {name}", flush=True)

    commands = [
        ("navigation_audit_python", ["python3", "-m", "unittest", "discover", "-s",
                                   "tools/tests", "-p", "*_test.py", "-v"]),
        ("questdb_fixture", ["fish", "tools/questdb/tests/run.fish"]),
    ]
    if any((ROOT / "tools/questdb/tests").glob("*_test.py")):
        commands.insert(1, ("questdb_python", ["python3", "-m", "unittest", "discover",
            "-s", "tools/questdb/tests", "-p", "*_test.py", "-v"]))
    else:
        print("QuestDB Python: no registered test files; SQL fixture remains required", flush=True)
    for name, command in commands:
        result, log = run(name, command)
        results.append(result)
        print(log.decode(errors="replace"), flush=True)

    lua = shutil.which("lua5.1")
    for fixture in sorted((ROOT / "tests").glob("*.lua")):
        producer = LUA_PRODUCERS.get(fixture.name)
        if not lua or not producer:
            results.append({"name": fixture.name, "pass": False,
                            "reason": "missing Lua 5.1 or registered producer"})
            continue
        result, script = run(fixture.stem + "_generate", [str(output / producer), "--lua"])
        results.append(result)
        if result["pass"]:
            result, log = run(fixture.stem, [lua, str(fixture)], data=script)
            results.append(result)
            print(log.decode(errors="replace"), flush=True)

    for name, command in [
        ("build", ["cmake", "--build", "build"]),
        ("diff_check", ["git", "diff", "--check", "--", "."]),
    ]:
        result, log = run(name, command)
        results.append(result)
        print(log.decode(errors="replace"), flush=True)
    results.sort(key=lambda record: record["name"])
    failures = [record["name"] for record in results if not record["pass"]]
    report = {"cpp_tests": len(sources), "results": results, "failures": failures}
    (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Validation {'FAIL' if failures else 'PASS'}: {len(sources)} C++ tests; failures={failures}")
    print(f"Detailed results: {output / 'results.json'}")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
