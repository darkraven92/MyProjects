#!/usr/bin/env python3
"""Inventory originals through the C reader; extract traceable research artifacts."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=Path('SveaRike'))
    parser.add_argument('--output', type=Path, default=Path('analysis'))
    parser.add_argument('--inspector', type=Path, default=Path('build/svea-inspect'))
    parser.add_argument('--projectorrays', type=Path, help='Optional locally built decompiler executable')
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve() or args.input.resolve() in args.output.resolve().parents:
        parser.error('Output must be outside the original game directory')
    args.output.mkdir(parents=True, exist_ok=True)
    inventory = []
    totals = Counter()
    failures = []
    for path in sorted(args.input.rglob('*')):
        if not path.is_file():
            continue
        data = path.read_bytes()
        entry = {'file': str(path.relative_to(args.input)), 'bytes': len(data),
                 'sha256': hashlib.sha256(data).hexdigest()}
        inventory.append(entry)
        if path.suffix.upper() not in ('.DIR', '.CST'):
            continue
        result = subprocess.run([str(args.inspector.resolve()), str(path)], capture_output=True, text=True)
        if result.returncode:
            entry['error'] = result.stderr.strip()
            failures.append(entry['file'])
            continue
        archive = json.loads(result.stdout)
        entry.update({k: v for k, v in archive.items() if k != 'resources'})
        counts = Counter(r['tag'] for r in archive['resources'] if r['active'])
        entry['chunks'] = dict(sorted(counts.items()))
        totals.update(counts)
        folder = args.output / 'resources' / path.relative_to(args.input)
        folder.mkdir(parents=True, exist_ok=True)
        (folder / 'map.json').write_text(json.dumps(archive, indent=2) + '\n')
        for resource in archive['resources']:
            tag = resource['tag']
            if not resource['active'] or tag not in ('Lscr', 'Lnam', 'STXT', 'Lctx', 'KEY*', 'CAS*', 'MCsL', 'VWCF', 'VWLB'):
                continue
            start = resource['offset'] + 8
            payload = data[start:start + resource['size']]
            stem = f"{resource['id']:05d}_{tag.replace('*', '_')}"
            (folder / (stem + '.bin')).write_bytes(payload)
            if tag == 'STXT':
                if len(payload) < 12:
                    raise ValueError(f'{path}:{stem}: truncated STXT')
                offset, length, _ = struct.unpack_from('>III', payload)
                if offset != 12 or offset + length > len(payload):
                    raise ValueError(f'{path}:{stem}: invalid STXT')
                # Windows text observed in DATA.CST (e.g. V\xe4sterg\xf6tland).
                # Raw .bin remains authoritative; font-specific remaps are future work.
                text = payload[offset:offset+length].decode('cp1252', errors='replace').replace('\r', '\n')
                (folder / (stem + '.txt')).write_text(text, encoding='utf-8')
            elif tag == 'Lnam':
                if len(payload) < 20:
                    raise ValueError(f'{path}:{stem}: truncated Lnam')
                pos, count = struct.unpack_from('>HH', payload, 16)
                names = []
                for _ in range(count):
                    if pos >= len(payload) or pos + 1 + payload[pos] > len(payload):
                        raise ValueError(f'{path}:{stem}: invalid Lnam string')
                    n = payload[pos]
                    names.append(payload[pos+1:pos+1+n].decode('cp1252', errors='replace'))
                    pos += 1+n
                (folder / (stem + '.json')).write_text(json.dumps(names, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        if args.projectorrays:
            destination = args.output / 'decompiled'
            destination.mkdir(exist_ok=True)
            cmd = [str(args.projectorrays.resolve()), 'decompile', str(path), '--dump-scripts', '--output', str(destination)]
            run = subprocess.run(cmd, capture_output=True)
            (folder / 'decompiler.log').write_bytes(run.stdout + run.stderr)
            entry['decompiler_exit'] = run.returncode
            if run.returncode:
                failures.append(entry['file'])
        print(f"{path.name}: {counts['Lscr']} script chunks, {counts['STXT']} text chunks")
    report = {'files': inventory, 'total_chunks': dict(sorted(totals.items())), 'failures': failures}
    (args.output / 'inventory.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f"Inventoried {len(inventory)} files; {totals['Lscr']} compiled script chunks; {len(failures)} failures")
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
