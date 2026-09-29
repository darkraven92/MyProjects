#!/usr/bin/env fish

if test (count $argv) -ge 1
    set project (realpath $argv[1])
else
    set project (pwd)
end

set tool "$project/tools/questdb/questdb.py"
set area "$project/tools/questdb/areas/durotar.json"
set cache "$project/.cache/questdb"
set data "$project/data/questdb"
set generated "$data/generated"
set archive "$cache/world_full_14_june_2021.7z"
set url "https://raw.githubusercontent.com/brotalnia/database/master/world_full_14_june_2021.7z"

if not test -f "$tool"
    echo "questdb.py not found under $project/tools/questdb"
    exit 1
end
if not command -q python3
    echo "python3 is required."
    exit 1
end
if not command -q curl
    echo "curl is required. On CachyOS/Arch: sudo pacman -S curl"
    exit 1
end

set extractor ""
if command -q 7zz
    set extractor 7zz
else if command -q 7z
    set extractor 7z
else
    echo "7z/7zz is required to extract the VMaNGOS world DB archive."
    echo "On current CachyOS/Arch: sudo pacman -S 7zip"
    exit 1
end

mkdir -p "$cache" "$data" "$generated"
if not test -f "$archive"
    echo "Downloading VMaNGOS world database snapshot..."
    curl -L --fail --show-error --progress-bar "$url" -o "$archive"; or exit 1
end

set unpack "$cache/world_full_14_june_2021"
mkdir -p "$unpack"
$extractor x -y -o"$unpack" "$archive" >/dev/null; or exit 1

set sql (find "$unpack" -maxdepth 2 -type f -name '*.sql' | head -n 1)
if test -z "$sql"
    echo "No .sql file found after extracting $archive"
    exit 1
end

echo "Importing relevant world tables into SQLite..."
python3 "$tool" import-sql --sql "$sql" --db "$data/vanilla.sqlite" --patch 10; or exit 1

echo
echo "Generating Phase 13B Durotar Orc Warrior catalog..."
python3 "$tool" area \
    --db "$data/vanilla.sqlite" \
    --config "$area" \
    --class warrior \
    --race orc \
    --json-out "$generated/durotar_orc_warrior.json" \
    --text-out "$generated/durotar_orc_warrior.txt"; or exit 1

echo
echo "Generating Phase 13B Durotar runtime quest catalog..."
mkdir -p "$data/runtime"
python3 "$tool" runtime-catalog \
    --db "$data/vanilla.sqlite" \
    --config "$area" \
    --class warrior \
    --race orc \
    --out "$data/runtime/durotar.tsv"; or exit 1
cp "$data/runtime/durotar.tsv" "$data/runtime/valley_of_trials.tsv"; or exit 1

python3 "$tool" support-report \
    --db "$data/vanilla.sqlite" \
    --config "$area" \
    --class warrior \
    --race orc \
    --out "$generated/durotar_orc_warrior_support.txt" >/dev/null; or exit 1

echo
echo "Phase 13B quest database ready:"
echo "  SQLite: $data/vanilla.sqlite"
echo "  Durotar JSON: $generated/durotar_orc_warrior.json"
echo "  Durotar report: $generated/durotar_orc_warrior.txt"
echo "  Support report: $generated/durotar_orc_warrior_support.txt"
echo "  Runtime catalog: $data/runtime/durotar.tsv"
echo
echo "Useful queries:"
echo "  python3 tools/questdb/questdb.py area --db data/questdb/vanilla.sqlite --config tools/questdb/areas/durotar.json --class warrior --race orc"
echo "  python3 tools/questdb/questdb.py support-report --db data/questdb/vanilla.sqlite --config tools/questdb/areas/durotar.json --class warrior --race orc"
