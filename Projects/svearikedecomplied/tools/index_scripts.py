#!/usr/bin/env python3
"""Index recovered Lingo handlers without modifying decompiler output."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path('analysis/decompiled'))
    parser.add_argument('--output', type=Path, default=Path('analysis/script-index.json'))
    args = parser.parse_args()
    scripts = []
    for path in sorted(args.root.rglob('*.ls')):
        raw = path.read_bytes()
        # Preserve the original bytes; this decoding is only for searchable metadata.
        text = raw.decode('cp1252', errors='replace')
        handlers = [{'name': match.group(1), 'line': line}
                    for line, content in enumerate(text.splitlines(), 1)
                    if (match := re.match(r'^(?:on|method|macro)\s+(\S+)', content))]
        scripts.append({'file': str(path.relative_to(args.root)),
                        'sha256': hashlib.sha256(raw).hexdigest(),
                        'lines': len(text.splitlines()), 'handlers': handlers})
    if not scripts:
        parser.error('No recovered .ls files found; run ProjectorRays first')
    counts = Counter(s['file'].split('/')[0] for s in scripts)
    report = {'scripts': scripts, 'by_archive': dict(sorted(counts.items())),
              'script_count': len(scripts),
              'handler_count': sum(len(s['handlers']) for s in scripts)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(f"Indexed {report['script_count']} scripts and {report['handler_count']} handlers")


if __name__ == '__main__':
    main()
