#!/usr/bin/env fish
set here (dirname (status --current-filename))
set root (realpath "$here/..")
set tmp (mktemp -d)

python3 "$root/questdb.py" import-sql \
    --sql "$here/fixture.sql" \
    --db "$tmp/test.sqlite" \
    --patch 10; or exit 1

python3 "$root/questdb.py" area \
    --db "$tmp/test.sqlite" \
    --config "$root/areas/valley_of_trials.json" \
    --class warrior \
    --race orc \
    --level 6 \
    --json-out "$tmp/area.json" >/dev/null; or exit 1

python3 "$root/questdb.py" find-item \
    --db "$tmp/test.sqlite" \
    11583 --map 1 > "$tmp/item.json"; or exit 1

python3 "$root/questdb.py" runtime-catalog \
    --db "$tmp/test.sqlite" \
    --config "$root/areas/valley_of_trials.json" \
    --class warrior \
    --race orc \
    --out "$tmp/runtime.tsv"; or exit 1

python3 -c 'import json,sys; area=json.load(open(sys.argv[1])); item=json.load(open(sys.argv[2])); rt=open(sys.argv[3],encoding="utf-8").read(); ids={q["quest_id"] for q in area["quests"]}; assert {4402,790,804}.issubset(ids), ids; assert any(x["source_type"]=="gameobject" and x["entry"]==171938 for x in item), item; assert "Q\t4402\tGalgar\x27s Cactus Apple Surprise" in rt; assert "CollectWorldItem\t0\t11583\t171938\t10" in rt; assert "Q\t790\tSarkoth" in rt and "CollectItemFromMob\t3281\t4905" in rt; assert "Q\t804\tSarkoth" in rt and "TravelReport\t3143" in rt; print("PHASE 12B QUESTDB RUNTIME SELF-TEST: PASS")' "$tmp/area.json" "$tmp/item.json" "$tmp/runtime.tsv"; or exit 1

rm -rf "$tmp"
