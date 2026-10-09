#!/usr/bin/env fish
# Reuse locally imported data; never downloads or changes the original SQLite.
set root (realpath (dirname (status --current-filename))/../..)
if test (count $argv) -ne 1
    echo "Usage: fish tools/questdb/refresh-early-horde.fish /path/to/build-5875/AreaTrigger.dbc"
    exit 1
end
set work (mktemp -d /tmp/wow-questdb-authored-XXXXXX)
g++ -std=c++20 -Wall -Wextra -Werror "$root/tools/quest_authored_ids.cpp" -o "$work/authored-ids"; or exit 1
"$work/authored-ids" "$work/ids.txt"; or exit 1
python3 "$root/tools/questdb/questdb.py" runtime-catalog \
    --db "$root/data/questdb/vanilla-enriched.sqlite" \
    --config "$root/tools/questdb/areas/early_horde.json" --race orc \
    --out "$root/data/questdb/runtime/early_horde.tsv" --area-trigger-dbc "$argv[1]" \
    --authored-ids "$work/ids.txt"
