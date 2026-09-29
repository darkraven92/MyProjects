#!/usr/bin/env fish

if test (count $argv) -ge 1
    set project (realpath $argv[1])
else
    set project (pwd)
end

set tool "$project/tools/questdb/questdb.py"
set db "$project/data/questdb/vanilla.sqlite"
set area "$project/tools/questdb/areas/durotar.json"
set runtime "$project/data/questdb/runtime/durotar.tsv"
set compat "$project/data/questdb/runtime/valley_of_trials.tsv"
set generated "$project/data/questdb/generated"
set support "$generated/durotar_orc_warrior_support.txt"

if not test -f "$db"
    echo "QuestDB SQLite not found: $db"
    echo "Run: fish tools/questdb/bootstrap.fish $project"
    exit 1
end

mkdir -p (dirname "$runtime") "$generated"

python3 "$tool" runtime-catalog \
    --db "$db" \
    --config "$area" \
    --class warrior \
    --race orc \
    --out "$runtime"; or exit 1

# Keep the historic filename synchronized for old diagnostics/scripts while
# the DLL now prefers durotar.tsv.
cp "$runtime" "$compat"; or exit 1

python3 "$tool" support-report \
    --db "$db" \
    --config "$area" \
    --class warrior \
    --race orc \
    --out "$support" >/dev/null; or exit 1

echo "Phase 13B Durotar runtime catalog refreshed:"
echo "  $runtime"
echo "Compatibility mirror:"
echo "  $compat"
echo "Support report:"
echo "  $support"
