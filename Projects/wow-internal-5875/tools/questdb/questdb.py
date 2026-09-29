#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import os
import re
import sqlite3
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, Iterator, List, Optional, Sequence, Tuple

TARGET_TABLES = {
    "quest_template",
    "creature_questrelation",
    "creature_involvedrelation",
    "gameobject_questrelation",
    "gameobject_involvedrelation",
    "creature",
    "creature_template",
    "gameobject",
    "gameobject_template",
    "creature_loot_template",
    "gameobject_loot_template",
}

CLASS_IDS = {
    "warrior": 1, "paladin": 2, "hunter": 3, "rogue": 4,
    "priest": 5, "shaman": 7, "mage": 8, "warlock": 9, "druid": 11,
}
RACE_IDS = {
    "human": 1, "orc": 2, "dwarf": 3, "nightelf": 4, "night_elf": 4,
    "undead": 5, "tauren": 6, "gnome": 7, "troll": 8,
}


def norm(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.lower())


def first(d: Dict[str, Any], *aliases: str, default: Any = None) -> Any:
    for alias in aliases:
        k = norm(alias)
        if k in d and d[k] is not None:
            return d[k]
    return default


def as_int(v: Any, default: int = 0) -> int:
    if v is None or v == "":
        return default
    try:
        if isinstance(v, str) and v.lower().startswith("0x"):
            return int(v, 16)
        return int(float(v))
    except (ValueError, TypeError):
        return default


def as_float(v: Any, default: float = 0.0) -> float:
    if v is None or v == "":
        return default
    try:
        return float(v)
    except (ValueError, TypeError):
        return default


def split_top_level(text: str, delimiter: str = ",") -> List[str]:
    out: List[str] = []
    start = 0
    depth = 0
    quote = False
    escape = False
    for i, ch in enumerate(text):
        if quote:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == "'":
                quote = False
            continue
        if ch == "'":
            quote = True
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == delimiter and depth == 0:
            out.append(text[start:i].strip())
            start = i + 1
    out.append(text[start:].strip())
    return out


def sql_unescape(s: str) -> str:
    # MySQL-style backslash escapes used by VMaNGOS dumps.
    repl = {
        "0": "\0", "b": "\b", "n": "\n", "r": "\r",
        "t": "\t", "Z": "\x1a", "'": "'", '"': '"', "\\": "\\",
    }
    out: List[str] = []
    i = 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            out.append(repl.get(s[i + 1], s[i + 1]))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def parse_atom(token: str) -> Any:
    token = token.strip()
    if not token:
        return None
    if token.upper() == "NULL":
        return None
    if token.startswith("'") and token.endswith("'"):
        return sql_unescape(token[1:-1])
    if token.startswith('"') and token.endswith('"'):
        return token[1:-1]
    # Preserve integer precision; float only when necessary.
    try:
        if re.fullmatch(r"[-+]?\d+", token):
            return int(token)
        if re.fullmatch(r"[-+]?(?:\d+\.\d*|\d*\.\d+)(?:[eE][-+]?\d+)?", token):
            return float(token)
    except ValueError:
        pass
    return token


def parse_tuples(values_text: str) -> Iterator[List[Any]]:
    i = 0
    n = len(values_text)
    while i < n:
        while i < n and (values_text[i].isspace() or values_text[i] == ","):
            i += 1
        if i >= n:
            break
        if values_text[i] != "(":
            i += 1
            continue
        start = i + 1
        i += 1
        depth = 1
        quote = False
        escape = False
        while i < n and depth:
            ch = values_text[i]
            if quote:
                if escape:
                    escape = False
                elif ch == "\\":
                    escape = True
                elif ch == "'":
                    quote = False
            else:
                if ch == "'":
                    quote = True
                elif ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
            i += 1
        if depth != 0:
            raise ValueError("unterminated VALUES tuple")
        body = values_text[start:i - 1]
        yield [parse_atom(x) for x in split_top_level(body)]


def statements(path: Path) -> Iterator[str]:
    # Streaming semicolon splitter aware of single-quoted MySQL strings.
    buf: List[str] = []
    quote = False
    escape = False
    in_block_comment = False
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            j = 0
            while j < len(line):
                ch = line[j]
                nxt = line[j + 1] if j + 1 < len(line) else ""
                if in_block_comment:
                    if ch == "*" and nxt == "/":
                        in_block_comment = False
                        j += 2
                    else:
                        j += 1
                    continue
                if not quote and ch == "/" and nxt == "*":
                    in_block_comment = True
                    j += 2
                    continue
                if not quote and ch == "-" and nxt == "-":
                    break
                buf.append(ch)
                if quote:
                    if escape:
                        escape = False
                    elif ch == "\\":
                        escape = True
                    elif ch == "'":
                        quote = False
                else:
                    if ch == "'":
                        quote = True
                    elif ch == ";":
                        stmt = "".join(buf).strip()
                        buf.clear()
                        if stmt:
                            yield stmt[:-1].strip()
                j += 1
    tail = "".join(buf).strip()
    if tail:
        yield tail


def parse_create(stmt: str) -> Optional[Tuple[str, List[str]]]:
    m = re.search(r"CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?`?([A-Za-z0-9_]+)`?\s*\((.*)\)\s*(?:ENGINE|TYPE|$)", stmt, re.I | re.S)
    if not m:
        return None
    table = m.group(1).lower()
    if table not in TARGET_TABLES:
        return None
    cols: List[str] = []
    for part in split_top_level(m.group(2)):
        cm = re.match(r"\s*`([^`]+)`", part)
        if cm:
            cols.append(cm.group(1))
    return table, cols


def parse_insert_header(stmt: str) -> Optional[Tuple[str, Optional[List[str]], str]]:
    m = re.match(
        r"\s*(?:INSERT|REPLACE)\s+(?:IGNORE\s+)?INTO\s+`?([A-Za-z0-9_]+)`?\s*(?:\((.*?)\))?\s*VALUES\s*(.*)\s*$",
        stmt, re.I | re.S,
    )
    if not m:
        return None
    table = m.group(1).lower()
    if table not in TARGET_TABLES:
        return None
    cols = None
    if m.group(2):
        cols = [x.strip().strip("`") for x in split_top_level(m.group(2))]
    return table, cols, m.group(3)


SCHEMA = r"""
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS quest(
    quest_id INTEGER PRIMARY KEY,
    patch INTEGER NOT NULL DEFAULT 0,
    title TEXT NOT NULL DEFAULT '',
    zone_or_sort INTEGER NOT NULL DEFAULT 0,
    min_level INTEGER NOT NULL DEFAULT 0,
    max_level INTEGER NOT NULL DEFAULT 0,
    quest_level INTEGER NOT NULL DEFAULT 0,
    required_classes INTEGER NOT NULL DEFAULT 0,
    required_races INTEGER NOT NULL DEFAULT 0,
    required_condition INTEGER NOT NULL DEFAULT 0,
    prev_quest_id INTEGER NOT NULL DEFAULT 0,
    next_quest_id INTEGER NOT NULL DEFAULT 0,
    next_in_chain INTEGER NOT NULL DEFAULT 0,
    breadcrumb_for_quest_id INTEGER NOT NULL DEFAULT 0,
    source_item_id INTEGER NOT NULL DEFAULT 0,
    source_item_count INTEGER NOT NULL DEFAULT 0,
    source_spell INTEGER NOT NULL DEFAULT 0,
    objective_text_1 TEXT NOT NULL DEFAULT '',
    objective_text_2 TEXT NOT NULL DEFAULT '',
    objective_text_3 TEXT NOT NULL DEFAULT '',
    objective_text_4 TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS quest_giver(
    quest_id INTEGER NOT NULL, entry INTEGER NOT NULL, source_type TEXT NOT NULL,
    patch_min INTEGER NOT NULL DEFAULT 0, patch_max INTEGER NOT NULL DEFAULT 10,
    PRIMARY KEY(quest_id, entry, source_type)
);
CREATE TABLE IF NOT EXISTS quest_turnin(
    quest_id INTEGER NOT NULL, entry INTEGER NOT NULL, source_type TEXT NOT NULL,
    patch_min INTEGER NOT NULL DEFAULT 0, patch_max INTEGER NOT NULL DEFAULT 10,
    PRIMARY KEY(quest_id, entry, source_type)
);
CREATE TABLE IF NOT EXISTS quest_objective(
    quest_id INTEGER NOT NULL, slot INTEGER NOT NULL, kind TEXT NOT NULL,
    raw_target_id INTEGER NOT NULL DEFAULT 0, target_entry INTEGER NOT NULL DEFAULT 0,
    item_id INTEGER NOT NULL DEFAULT 0, required_count INTEGER NOT NULL DEFAULT 0,
    spell_id INTEGER NOT NULL DEFAULT 0, text TEXT NOT NULL DEFAULT '',
    PRIMARY KEY(quest_id, slot, kind, raw_target_id, item_id, spell_id)
);
CREATE TABLE IF NOT EXISTS creature_template(
    entry INTEGER PRIMARY KEY, name TEXT NOT NULL DEFAULT '', loot_id INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS creature_spawn(
    guid INTEGER NOT NULL, entry INTEGER NOT NULL, map_id INTEGER NOT NULL,
    x REAL NOT NULL, y REAL NOT NULL, z REAL NOT NULL, orientation REAL NOT NULL DEFAULT 0,
    patch_min INTEGER NOT NULL DEFAULT 0, patch_max INTEGER NOT NULL DEFAULT 10,
    PRIMARY KEY(guid, entry)
);
CREATE TABLE IF NOT EXISTS gameobject_template(
    entry INTEGER PRIMARY KEY, type INTEGER NOT NULL DEFAULT 0, name TEXT NOT NULL DEFAULT '',
    loot_id INTEGER NOT NULL DEFAULT 0, raw_data_json TEXT NOT NULL DEFAULT '{}'
);
CREATE TABLE IF NOT EXISTS gameobject_spawn(
    guid INTEGER NOT NULL, entry INTEGER NOT NULL, map_id INTEGER NOT NULL,
    x REAL NOT NULL, y REAL NOT NULL, z REAL NOT NULL, orientation REAL NOT NULL DEFAULT 0,
    patch_min INTEGER NOT NULL DEFAULT 0, patch_max INTEGER NOT NULL DEFAULT 10,
    PRIMARY KEY(guid, entry)
);
CREATE TABLE IF NOT EXISTS loot_source(
    source_type TEXT NOT NULL, loot_id INTEGER NOT NULL, item_id INTEGER NOT NULL,
    chance REAL NOT NULL DEFAULT 0, min_count INTEGER NOT NULL DEFAULT 0,
    max_count INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(source_type, loot_id, item_id)
);
CREATE INDEX IF NOT EXISTS idx_creature_spawn_entry ON creature_spawn(entry,map_id);
CREATE INDEX IF NOT EXISTS idx_go_spawn_entry ON gameobject_spawn(entry,map_id);
CREATE INDEX IF NOT EXISTS idx_loot_item ON loot_source(item_id,source_type);
CREATE INDEX IF NOT EXISTS idx_giver_entry ON quest_giver(entry,source_type);
CREATE INDEX IF NOT EXISTS idx_turnin_entry ON quest_turnin(entry,source_type);
"""


def rowdict(cols: Sequence[str], values: Sequence[Any]) -> Dict[str, Any]:
    return {norm(c): values[i] if i < len(values) else None for i, c in enumerate(cols)}


def patch_active(d: Dict[str, Any], target_patch: int) -> bool:
    pmin = as_int(first(d, "patch_min", "patchmin"), 0)
    pmax = as_int(first(d, "patch_max", "patchmax"), 10)
    return pmin <= target_patch <= pmax


def upsert_quest(conn: sqlite3.Connection, d: Dict[str, Any], target_patch: int) -> None:
    qid = as_int(first(d, "entry", "questid", "id"))
    if not qid:
        return
    patch = as_int(first(d, "patch"), 0)
    if patch > target_patch:
        return
    existing = conn.execute("SELECT patch FROM quest WHERE quest_id=?", (qid,)).fetchone()
    if existing and existing[0] > patch:
        return
    vals = (
        qid, patch, str(first(d, "title", default="") or ""), as_int(first(d, "zoneorsort")),
        as_int(first(d, "minlevel")), as_int(first(d, "maxlevel")), as_int(first(d, "questlevel")),
        as_int(first(d, "requiredclasses")), as_int(first(d, "requiredraces")), as_int(first(d, "requiredcondition")),
        as_int(first(d, "prevquestid")), as_int(first(d, "nextquestid")), as_int(first(d, "nextquestinchain")),
        as_int(first(d, "breadcrumbforquestid")), as_int(first(d, "srcitemid")), as_int(first(d, "srcitemcount")),
        as_int(first(d, "srcspell")),
        str(first(d, "objectivetext1", default="") or ""), str(first(d, "objectivetext2", default="") or ""),
        str(first(d, "objectivetext3", default="") or ""), str(first(d, "objectivetext4", default="") or ""),
    )
    conn.execute(
        "INSERT OR REPLACE INTO quest VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", vals
    )
    conn.execute("DELETE FROM quest_objective WHERE quest_id=?", (qid,))
    for slot in range(1, 5):
        text = str(first(d, f"objectivetext{slot}", default="") or "")
        item_id = as_int(first(d, f"reqitemid{slot}"))
        item_count = as_int(first(d, f"reqitemcount{slot}"))
        raw = as_int(first(d, f"reqcreatureorgoid{slot}"))
        raw_count = as_int(first(d, f"reqcreatureorgocount{slot}"))
        spell = as_int(first(d, f"reqspellcast{slot}"))
        if raw:
            kind = "creature" if raw > 0 else "gameobject"
            conn.execute(
                "INSERT OR REPLACE INTO quest_objective VALUES (?,?,?,?,?,?,?,?,?)",
                (qid, slot, kind, raw, abs(raw), 0, raw_count, spell, text),
            )
        if item_id:
            conn.execute(
                "INSERT OR REPLACE INTO quest_objective VALUES (?,?,?,?,?,?,?,?,?)",
                (qid, slot, "item", 0, 0, item_id, item_count, spell, text),
            )
        elif spell and not raw:
            conn.execute(
                "INSERT OR REPLACE INTO quest_objective VALUES (?,?,?,?,?,?,?,?,?)",
                (qid, slot, "spell", 0, 0, 0, 1, spell, text),
            )


def import_row(conn: sqlite3.Connection, table: str, d: Dict[str, Any], target_patch: int) -> None:
    if table == "quest_template":
        upsert_quest(conn, d, target_patch)
        return
    if table in {"creature_questrelation", "creature_involvedrelation", "gameobject_questrelation", "gameobject_involvedrelation"}:
        if not patch_active(d, target_patch):
            return
        entry = as_int(first(d, "id", "entry")); qid = as_int(first(d, "quest", "questid"))
        if not entry or not qid:
            return
        source = "gameobject" if table.startswith("gameobject") else "creature"
        dest = "quest_giver" if table.endswith("questrelation") and "involved" not in table else "quest_turnin"
        conn.execute(
            f"INSERT OR REPLACE INTO {dest} VALUES (?,?,?,?,?)",
            (qid, entry, source, as_int(first(d,"patch_min","patchmin"),0), as_int(first(d,"patch_max","patchmax"),10)),
        )
        return
    if table == "creature_template":
        entry = as_int(first(d, "entry"));
        if entry:
            conn.execute("INSERT OR REPLACE INTO creature_template VALUES (?,?,?)", (
                entry, str(first(d,"name",default="") or ""), as_int(first(d,"lootid","loot_id","loot"))
            ))
        return
    if table == "creature":
        if not patch_active(d, target_patch):
            return
        guid = as_int(first(d,"guid")); map_id = as_int(first(d,"map","mapid"))
        if not guid:
            return
        ids = []
        for a in ("id","id1","id2","id3","id4"):
            v = as_int(first(d,a))
            if v and v not in ids: ids.append(v)
        for entry in ids:
            conn.execute("INSERT OR REPLACE INTO creature_spawn VALUES (?,?,?,?,?,?,?,?,?)", (
                guid, entry, map_id, as_float(first(d,"position_x","positionx")), as_float(first(d,"position_y","positiony")),
                as_float(first(d,"position_z","positionz")), as_float(first(d,"orientation")),
                as_int(first(d,"patch_min","patchmin"),0), as_int(first(d,"patch_max","patchmax"),10)
            ))
        return
    if table == "gameobject":
        if not patch_active(d, target_patch):
            return
        guid = as_int(first(d,"guid")); entry = as_int(first(d,"id","entry")); map_id=as_int(first(d,"map","mapid"))
        if guid and entry:
            conn.execute("INSERT OR REPLACE INTO gameobject_spawn VALUES (?,?,?,?,?,?,?,?,?)", (
                guid, entry, map_id, as_float(first(d,"position_x","positionx")), as_float(first(d,"position_y","positiony")),
                as_float(first(d,"position_z","positionz")), as_float(first(d,"orientation","facing")),
                as_int(first(d,"patch_min","patchmin"),0), as_int(first(d,"patch_max","patchmax"),10)
            ))
        return
    if table == "gameobject_template":
        entry = as_int(first(d,"entry")); typ = as_int(first(d,"type")); name = str(first(d,"name",default="") or "")
        if not entry:
            return
        raw_data = {k:v for k,v in d.items() if k.startswith("data")}
        loot_id = as_int(first(d,"lootid","loot_id","loot"))
        # Vanilla GAMEOBJECT_TYPE_CHEST = 3; MaNGOS stores chest loot id in data1.
        if not loot_id and typ == 3:
            loot_id = as_int(first(d,"data1"))
        conn.execute("INSERT OR REPLACE INTO gameobject_template VALUES (?,?,?,?,?)", (
            entry, typ, name, loot_id, json.dumps(raw_data, ensure_ascii=False, separators=(",",":"))
        ))
        return
    if table in {"creature_loot_template", "gameobject_loot_template"}:
        loot_id = as_int(first(d,"entry","lootid","loot_id")); item = as_int(first(d,"item","itemid"))
        if not loot_id or not item:
            return
        chance = as_float(first(d,"chanceorquestchance","chance","chanceorquestchance"))
        min_count = as_int(first(d,"mincountorref","mincount","min_count"), 1)
        max_count = as_int(first(d,"maxcount","max_count"), max(1,min_count))
        conn.execute("INSERT OR REPLACE INTO loot_source VALUES (?,?,?,?,?,?)", (
            "gameobject" if table.startswith("gameobject") else "creature", loot_id, item, chance, min_count, max_count
        ))


def import_sql(sql_path: Path, db_path: Path, target_patch: int) -> None:
    db_path.parent.mkdir(parents=True, exist_ok=True)
    if db_path.exists():
        db_path.unlink()
    conn = sqlite3.connect(db_path)
    conn.executescript(SCHEMA)
    schemas: Dict[str,List[str]] = {}
    seen = {t:0 for t in TARGET_TABLES}
    inserted = {t:0 for t in TARGET_TABLES}
    try:
        for idx, stmt in enumerate(statements(sql_path), 1):
            c = parse_create(stmt)
            if c:
                schemas[c[0]] = c[1]
                continue
            h = parse_insert_header(stmt)
            if not h:
                continue
            table, explicit_cols, values_text = h
            cols = explicit_cols or schemas.get(table)
            if not cols:
                continue
            for values in parse_tuples(values_text):
                seen[table] += 1
                d = rowdict(cols, values)
                before = conn.total_changes
                import_row(conn, table, d, target_patch)
                if conn.total_changes != before:
                    inserted[table] += 1
            if idx % 5000 == 0:
                conn.commit()
        conn.execute("INSERT OR REPLACE INTO meta VALUES (?,?)", ("source_sql", str(sql_path)))
        conn.execute("INSERT OR REPLACE INTO meta VALUES (?,?)", ("target_patch", str(target_patch)))
        conn.execute("INSERT OR REPLACE INTO meta VALUES (?,?)", ("format", "phase12a-v1"))
        conn.commit()
    finally:
        conn.close()
    print(f"Quest DB written: {db_path}")
    for t in sorted(TARGET_TABLES):
        if seen[t]:
            print(f"  {t}: rows parsed={seen[t]} rows affecting catalog={inserted[t]}")


def mask_allows(required_mask: int, token: Optional[str], lookup: Dict[str,int]) -> bool:
    if not required_mask or not token:
        return True
    key = token.lower().replace(" ", "_")
    if key not in lookup:
        raise ValueError(f"unknown token: {token}")
    rid = lookup[key]
    bit = 1 << (rid - 1)
    return (required_mask & bit) != 0


def spawn_rows(conn: sqlite3.Connection, source_type: str, entry: int, map_id: Optional[int] = None) -> List[Dict[str,Any]]:
    table = "creature_spawn" if source_type == "creature" else "gameobject_spawn"
    q = f"SELECT guid,entry,map_id,x,y,z,orientation FROM {table} WHERE entry=?"
    args: List[Any] = [entry]
    if map_id is not None:
        q += " AND map_id=?"; args.append(map_id)
    q += " ORDER BY guid"
    return [dict(zip(("guid","entry","map_id","x","y","z","orientation"), r)) for r in conn.execute(q,args)]


def in_bounds(s: Dict[str,Any], bounds: Dict[str,float]) -> bool:
    return bounds["min_x"] <= s["x"] <= bounds["max_x"] and bounds["min_y"] <= s["y"] <= bounds["max_y"]


def quest_sources_for_item(conn: sqlite3.Connection, item_id: int, map_id: Optional[int]) -> List[Dict[str,Any]]:
    out: List[Dict[str,Any]] = []
    for source_type, loot_id, chance, minc, maxc in conn.execute(
        "SELECT source_type,loot_id,chance,min_count,max_count FROM loot_source WHERE item_id=? ORDER BY source_type,loot_id", (item_id,)
    ):
        if source_type == "creature":
            rows = conn.execute("SELECT entry,name FROM creature_template WHERE loot_id=?", (loot_id,)).fetchall()
        else:
            rows = conn.execute("SELECT entry,name FROM gameobject_template WHERE loot_id=?", (loot_id,)).fetchall()
        for entry,name in rows:
            spawns = spawn_rows(conn, source_type, entry, map_id)
            out.append({
                "source_type": source_type, "entry": entry, "name": name, "loot_id": loot_id,
                "chance": chance, "min_count": minc, "max_count": maxc, "spawns": spawns,
            })
    return out


def quest_record(conn: sqlite3.Connection, qid: int, map_id: Optional[int] = None) -> Optional[Dict[str,Any]]:
    row = conn.execute("SELECT * FROM quest WHERE quest_id=?", (qid,)).fetchone()
    if not row:
        return None
    cols = [d[1] for d in conn.execute("PRAGMA table_info(quest)")]
    q = dict(zip(cols,row))
    q["givers"] = []
    for entry,source_type in conn.execute("SELECT entry,source_type FROM quest_giver WHERE quest_id=? ORDER BY source_type,entry",(qid,)):
        name_row = conn.execute(
            f"SELECT name FROM {'creature_template' if source_type=='creature' else 'gameobject_template'} WHERE entry=?", (entry,)
        ).fetchone()
        q["givers"].append({"source_type":source_type,"entry":entry,"name":name_row[0] if name_row else "","spawns":spawn_rows(conn,source_type,entry,map_id)})
    q["turnins"] = []
    for entry,source_type in conn.execute("SELECT entry,source_type FROM quest_turnin WHERE quest_id=? ORDER BY source_type,entry",(qid,)):
        name_row = conn.execute(
            f"SELECT name FROM {'creature_template' if source_type=='creature' else 'gameobject_template'} WHERE entry=?", (entry,)
        ).fetchone()
        q["turnins"].append({"source_type":source_type,"entry":entry,"name":name_row[0] if name_row else "","spawns":spawn_rows(conn,source_type,entry,map_id)})
    q["objectives"] = []
    for r in conn.execute("SELECT slot,kind,raw_target_id,target_entry,item_id,required_count,spell_id,text FROM quest_objective WHERE quest_id=? ORDER BY slot,kind",(qid,)):
        obj = dict(zip(("slot","kind","raw_target_id","target_entry","item_id","required_count","spell_id","text"),r))
        if obj["kind"] in {"creature","gameobject"}:
            typ = obj["kind"]
            tab = "creature_template" if typ=="creature" else "gameobject_template"
            nr = conn.execute(f"SELECT name FROM {tab} WHERE entry=?",(obj["target_entry"],)).fetchone()
            obj["target_name"] = nr[0] if nr else ""
            obj["spawns"] = spawn_rows(conn,typ,obj["target_entry"],map_id)
        if obj["kind"] == "item":
            obj["loot_sources"] = quest_sources_for_item(conn,obj["item_id"],map_id)
        q["objectives"].append(obj)
    return q


def area_catalog(conn: sqlite3.Connection, cfg: Dict[str,Any], cls: Optional[str], race: Optional[str], level: Optional[int]) -> Dict[str,Any]:
    map_id = int(cfg["map_id"]); bounds=cfg["bounds"]

    # Phase 13B supports complete zone packs. A config can still explicitly
    # enumerate quest IDs, but a Durotar pack can select the whole zone by
    # quest_template.ZoneOrSort and then union in class quests whose sort is
    # negative (for example the Warrior parchment quest).
    configured = cfg.get("quest_ids")
    if configured:
        qids={int(x) for x in configured}
    else:
        qids=set()
        zone_ids=[int(x) for x in (cfg.get("zone_or_sort_ids") or [])]
        if zone_ids:
            placeholders=",".join("?" for _ in zone_ids)
            for (qid,) in conn.execute(
                f"SELECT quest_id FROM quest WHERE zone_or_sort IN ({placeholders}) ORDER BY quest_id",
                zone_ids,
            ):
                qids.add(int(qid))

        for qid in (cfg.get("additional_quest_ids") or []):
            qids.add(int(qid))

        # Backward-compatible spatial discovery when no zone selector exists.
        if not zone_ids and not qids:
            giver_entries = set()
            for row in conn.execute("SELECT DISTINCT entry FROM creature_spawn WHERE map_id=? AND x BETWEEN ? AND ? AND y BETWEEN ? AND ?",(
                map_id,bounds["min_x"],bounds["max_x"],bounds["min_y"],bounds["max_y"]
            )):
                giver_entries.add(("creature",int(row[0])))
            for row in conn.execute("SELECT DISTINCT entry FROM gameobject_spawn WHERE map_id=? AND x BETWEEN ? AND ? AND y BETWEEN ? AND ?",(
                map_id,bounds["min_x"],bounds["max_x"],bounds["min_y"],bounds["max_y"]
            )):
                giver_entries.add(("gameobject",int(row[0])))
            for typ,entry in giver_entries:
                for (qid,) in conn.execute("SELECT quest_id FROM quest_giver WHERE source_type=? AND entry=?",(typ,entry)):
                    qids.add(int(qid))

    quests=[]
    for qid in sorted(qids):
        r=conn.execute("SELECT min_level,max_level,required_classes,required_races FROM quest WHERE quest_id=?",(qid,)).fetchone()
        if not r: continue
        minlvl,maxlvl,cmask,rmask=map(int,r)
        if level is not None and level < minlvl: continue
        if maxlvl and level is not None and level > maxlvl: continue
        if not mask_allows(cmask,cls,CLASS_IDS): continue
        if not mask_allows(rmask,race,RACE_IDS): continue
        q=quest_record(conn,qid,map_id)
        if q: quests.append(q)
    return {"area":cfg,"filters":{"class":cls,"race":race,"level":level},"quest_count":len(quests),"quests":quests}

def summarize_area(cat: Dict[str,Any]) -> str:
    lines=[f"Area: {cat['area'].get('name',cat['area'].get('id','area'))}",f"Quests: {cat['quest_count']}",""]
    for q in cat["quests"]:
        lines.append(f"[{q['quest_id']}] {q['title']} (min {q['min_level']}, level {q['quest_level']})")
        for g in q["givers"]:
            inarea=sum(1 for s in g["spawns"] if in_bounds(s,cat["area"]["bounds"]))
            lines.append(f"  giver: {g['source_type']} {g['entry']} {g['name']} spawns_on_map={len(g['spawns'])} in_area={inarea}")
        for o in q["objectives"]:
            if o["kind"] in {"creature","gameobject"}:
                lines.append(f"  objective {o['slot']}: {o['kind']} {o['target_entry']} {o.get('target_name','')} x{o['required_count']} spawns={len(o.get('spawns',[]))}")
            elif o["kind"]=="item":
                src=", ".join(f"{s['source_type']}:{s['entry']} {s['name']}" for s in o.get('loot_sources',[])[:8]) or "unresolved"
                lines.append(f"  objective {o['slot']}: item {o['item_id']} x{o['required_count']} sources={src}")
            else:
                lines.append(f"  objective {o['slot']}: {o['kind']} spell={o['spell_id']} x{o['required_count']}")
        for t in q["turnins"]:
            lines.append(f"  turn-in: {t['source_type']} {t['entry']} {t['name']}")
        lines.append("")
    return "\n".join(lines)



def tsv_escape(value: Any) -> str:
    s = str(value if value is not None else "")
    return (s.replace("\\", "\\\\")
             .replace("\t", "\\t")
             .replace("\n", "\\n")
             .replace("\r", "\\r"))


def _first_source_with_spawns(sources: List[Dict[str,Any]], bounds: Dict[str,float]) -> Optional[Dict[str,Any]]:
    ranked=[]
    for src in sources:
        spawns=src.get("spawns",[]) or []
        in_area=[x for x in spawns if in_bounds(x,bounds)]
        if in_area:
            ranked.append((0, src, in_area))
        elif spawns:
            ranked.append((1, src, spawns))
    if not ranked:
        return None
    ranked.sort(key=lambda x:(x[0], x[1].get("source_type",""), int(x[1].get("entry",0))))
    src=dict(ranked[0][1])
    src["preferred_spawns"]=ranked[0][2]
    return src


def _preferred_relation(relations: List[Dict[str,Any]], bounds: Dict[str,float]) -> Optional[Dict[str,Any]]:
    ranked=[]
    for rel in relations:
        spawns=rel.get("spawns",[]) or []
        in_area=[x for x in spawns if in_bounds(x,bounds)]
        if in_area:
            ranked.append((0, rel, in_area))
        elif spawns:
            ranked.append((1, rel, spawns))
        else:
            ranked.append((2, rel, []))
    if not ranked:
        return None
    ranked.sort(key=lambda x:(x[0], x[1].get("source_type",""), int(x[1].get("entry",0))))
    out=dict(ranked[0][1]); out["preferred_spawns"]=ranked[0][2]
    return out


def _runtime_objective_steps(conn: sqlite3.Connection, q: Dict[str,Any], cfg: Dict[str,Any]) -> List[Dict[str,Any]]:
    bounds=cfg["bounds"]
    objectives=q.get("objectives",[]) or []

    # Vanilla 1.12 GetQuestLogLeaderBoard walks RequiredNPCOrGo[0..3]
    # first and RequiredItem[0..3] second. Export in the same order so a live
    # completion bit can select the correct database objective without parsing
    # localized text.
    npc_go=sorted(
        [o for o in objectives if o.get("kind") in {"creature","gameobject"}],
        key=lambda o:int(o.get("slot",0)))
    items=sorted(
        [o for o in objectives if o.get("kind")=="item"],
        key=lambda o:int(o.get("slot",0)))
    others=sorted(
        [o for o in objectives if o.get("kind") not in {"creature","gameobject","item"}],
        key=lambda o:(int(o.get("slot",0)),str(o.get("kind",""))))

    ordered=npc_go+items+others
    steps=[]
    for leaderboard_index,o in enumerate(ordered):
        kind=o.get("kind")
        obj_type="Unknown"; target_entry=0; item_id=0; object_entry=0
        required=max(1,int(o.get("required_count",0) or 0))
        target_name=""; source_spawns=[]; go_type=-1; go_loot_id=0

        if kind=="creature" and int(o.get("target_entry",0)):
            target_entry=int(o["target_entry"])
            target_name=str(o.get("target_name","") or q.get("title","") or "creature")
            source_spawns=o.get("spawns",[]) or []
            spell_id=int(o.get("spell_id",0) or 0)
            source_item_id=int(q.get("source_item_id",0) or 0)
            if spell_id and source_item_id:
                # Generic Vanilla pattern used by quests such as Lazy Peons:
                # a quest source item is used on the required creature and the
                # client tracks the result through the objective leaderboard.
                obj_type="UseItemOnUnit"
                item_id=source_item_id
            elif spell_id:
                # Keep cast objectives visible, but do not auto-accept them
                # until a generic spell-cast executor exists.
                obj_type="Unknown"
            else:
                obj_type="KillMob"
        elif kind=="gameobject" and int(o.get("target_entry",0)):
            obj_type="InteractGameObject"
            object_entry=int(o["target_entry"])
            target_name=str(o.get("target_name","") or q.get("title","") or "gameobject")
            source_spawns=o.get("spawns",[]) or []
            row=conn.execute("SELECT type,loot_id FROM gameobject_template WHERE entry=?",(object_entry,)).fetchone()
            if row:
                go_type=int(row[0]); go_loot_id=int(row[1])
        elif kind=="item" and int(o.get("item_id",0)):
            src=_first_source_with_spawns(o.get("loot_sources",[]) or [],bounds)
            if not src:
                # Keep the step visible in the runtime catalogue even when
                # source resolution is incomplete. The planner will log the
                # unsupported objective instead of silently pretending the
                # quest is single-objective.
                item_id=int(o["item_id"])
                obj_type="Unknown"
                target_name=f"item {item_id}"
            else:
                item_id=int(o["item_id"])
                target_name=str(src.get("name","") or q.get("title","") or f"item {item_id}")
                source_spawns=src.get("preferred_spawns",[]) or []
                if src.get("source_type")=="creature":
                    obj_type="CollectItemFromMob"
                    target_entry=int(src.get("entry",0))
                else:
                    obj_type="CollectWorldItem"
                    object_entry=int(src.get("entry",0))
                    row=conn.execute("SELECT type,loot_id FROM gameobject_template WHERE entry=?",(object_entry,)).fetchone()
                    if row:
                        go_type=int(row[0]); go_loot_id=int(row[1])

        steps.append({
            "slot":int(o.get("slot",0)),
            "leaderboard_index":leaderboard_index,
            "objective_type":obj_type,
            "target_entry":target_entry,
            "item_id":item_id,
            "object_entry":object_entry,
            "required_count":required,
            "target_name":target_name,
            "gameobject_type":go_type,
            "gameobject_loot_id":go_loot_id,
            "objective_spawns":source_spawns,
        })
    return steps


def _distance2d(a: Dict[str,Any], b: Dict[str,Any]) -> float:
    return math.hypot(float(a.get("x",0.0))-float(b.get("x",0.0)),
                      float(a.get("y",0.0))-float(b.get("y",0.0)))


def _profile_support_reasons(q: Dict[str,Any], giver_entry: int, turnin_entry: int,
                             giver_spawns: List[Dict[str,Any]], turnin_spawns: List[Dict[str,Any]],
                             steps: List[Dict[str,Any]]) -> List[str]:
    reasons=[]
    if giver_entry <= 0:
        reasons.append("missing giver relation")
    elif not giver_spawns:
        reasons.append("giver has no map spawn")
    if turnin_entry <= 0:
        reasons.append("missing turn-in relation")
    elif not turnin_spawns:
        reasons.append("turn-in has no map spawn")
    for index, step in enumerate(steps,1):
        if step.get("objective_type") == "Unknown":
            reasons.append(f"objective {index} unsupported/unresolved")
        elif step.get("objective_type") in {"KillMob","CollectItemFromMob","CollectWorldItem","InteractGameObject","UseItemOnUnit"} and not step.get("objective_spawns"):
            reasons.append(f"objective {index} has no map spawn seed")
    return reasons


def runtime_profile_for_quest(conn: sqlite3.Connection, q: Dict[str,Any], cfg: Dict[str,Any]) -> Dict[str,Any]:
    bounds=cfg["bounds"]
    giver=_preferred_relation(q.get("givers",[]),bounds)
    turnin=_preferred_relation(q.get("turnins",[]),bounds)
    steps=_runtime_objective_steps(conn,q,cfg)

    giver_entry=int(giver.get("entry",0)) if giver else 0
    turnin_entry=int(turnin.get("entry",0)) if turnin else 0
    giver_spawns=(giver.get("preferred_spawns",[]) if giver else []) or []
    turnin_spawns=(turnin.get("preferred_spawns",[]) if turnin else []) or []

    qid=int(q["quest_id"])
    overrides=(cfg.get("quest_overrides",{}) or {}).get(str(qid),{}) or {}

    default_route = cfg.get("route_group")
    if not default_route:
        default_route = "None" if cfg.get("zone_or_sort_ids") else "ValleyExterior"
    route=str(overrides.get("route_group") or default_route)
    default_priority = cfg.get("priority")
    if default_priority is None:
        default_priority = 600 if cfg.get("zone_or_sort_ids") else 800
    priority=int(overrides.get("priority",default_priority))
    hub_unlock=int(overrides.get("hub_unlock_quest_id",cfg.get("hub_unlock_quest_id",0)) or 0)
    hub_exit=bool(overrides.get("hub_exit",False))
    late_wave=bool(overrides.get("late_wave",False))

    if not steps:
        # Zero-objective Vanilla quests are report/talk steps.
        obj_type="TravelReport"; target_entry=turnin_entry; item_id=0
        object_entry=0; required=1
        target_name=str(turnin.get("name","") if turnin else q.get("title",""))
        source_spawns=[]
        go_type=-1; go_loot_id=0
    else:
        primary=steps[0]
        obj_type=primary["objective_type"]
        target_entry=primary["target_entry"]
        item_id=primary["item_id"]
        object_entry=primary["object_entry"]
        required=primary["required_count"]
        target_name=primary["target_name"]
        source_spawns=primary["objective_spawns"]
        go_type=primary["gameobject_type"]
        go_loot_id=primary["gameobject_loot_id"]

    # Phase 13B: infer report/breadcrumb hub exits from the actual QuestDB
    # giver/turn-in distance. This scales beyond the Valley -> Sen'jin special
    # case without encoding quest-specific travel waypoints in C++.
    auto_exit_distance=float(cfg.get("travel_report_exit_distance",300.0) or 300.0)
    if (not hub_exit and obj_type=="TravelReport" and giver_spawns and turnin_spawns and
            _distance2d(giver_spawns[0],turnin_spawns[0]) >= auto_exit_distance):
        hub_exit=True
        priority=min(priority,100)

    support_reasons=_profile_support_reasons(
        q,giver_entry,turnin_entry,giver_spawns,turnin_spawns,steps)
    automatable=not support_reasons

    return {
        "quest_id":qid, "title":str(q.get("title","") or ""),
        "min_level":int(q.get("min_level",0)), "max_level":int(q.get("max_level",0)),
        "giver_entry":giver_entry, "turnin_entry":turnin_entry,
        "expected_objective_count":len(steps),
        "objective_type":obj_type, "target_entry":target_entry, "item_id":item_id,
        "object_entry":object_entry, "required_count":required,
        "target_name":target_name, "route_group":route, "priority":priority,
        "gameobject_type":go_type, "gameobject_loot_id":go_loot_id,
        "giver_spawns":giver_spawns, "turnin_spawns":turnin_spawns,
        "objective_spawns":source_spawns,
        "objectives":steps,
        "hub_unlock_quest_id":hub_unlock,
        "hub_exit":hub_exit,
        "late_wave":late_wave,
        "quest_level":int(q.get("quest_level",0) or 0),
        "prev_quest_id":int(q.get("prev_quest_id",0) or 0),
        "next_in_chain":int(q.get("next_in_chain",0) or 0),
        "breadcrumb_for_quest_id":int(q.get("breadcrumb_for_quest_id",0) or 0),
        "automatable":automatable,
        "support_note":"; ".join(support_reasons) if support_reasons else "supported",
    }


def _arrival_for_type(obj_type: str) -> float:
    if obj_type=="CollectWorldItem": return 22.0
    if obj_type in {"KillMob","CollectItemFromMob"}: return 28.0
    return 8.0


def export_runtime_catalog(conn: sqlite3.Connection, cfg: Dict[str,Any], cls: Optional[str], race: Optional[str], out_path: Path) -> None:
    cat=area_catalog(conn,cfg,cls,race,None)
    profiles=[runtime_profile_for_quest(conn,q,cfg) for q in cat["quests"]]
    out_path.parent.mkdir(parents=True,exist_ok=True)
    lines=[
        "# wow-internal questdb runtime catalog v2",
        f"# area={tsv_escape(cfg.get('id','area'))}\tmap={int(cfg['map_id'])}\tclass={tsv_escape(cls or '')}\trace={tsv_escape(race or '')}"
    ]
    for p in profiles:
        fields=[
            "Q",p["quest_id"],p["title"],p["min_level"],p["max_level"],p["giver_entry"],p["turnin_entry"],
            p["expected_objective_count"],p["objective_type"],p["target_entry"],p["item_id"],p["object_entry"],
            p["required_count"],p["target_name"],p["route_group"],p["priority"],p["gameobject_type"],p["gameobject_loot_id"],
            p["hub_unlock_quest_id"],1 if p["hub_exit"] else 0,
            p["quest_level"],p["prev_quest_id"],p["next_in_chain"],p["breadcrumb_for_quest_id"],
            1 if p["automatable"] else 0,p["support_note"],
            1 if p.get("late_wave",False) else 0
        ]
        lines.append("\t".join(tsv_escape(x) for x in fields))

        for i,s in enumerate(p["giver_spawns"][:8],1):
            lines.append("\t".join(tsv_escape(x) for x in ["G",p["quest_id"],int(s["map_id"]),s["x"],s["y"],s["z"],5.0,f"QuestDB giver spawn {i}"]))
        for i,s in enumerate(p.get("turnin_spawns",[])[:8],1):
            lines.append("\t".join(tsv_escape(x) for x in ["T",p["quest_id"],int(s["map_id"]),s["x"],s["y"],s["z"],5.0,f"QuestDB turn-in spawn {i}"]))

        # v2 multi-objective records.
        for oi,o in enumerate(p.get("objectives",[])):
            lines.append("\t".join(tsv_escape(x) for x in [
                "O",p["quest_id"],oi,o["slot"],o["leaderboard_index"],o["objective_type"],
                o["target_entry"],o["item_id"],o["object_entry"],o["required_count"],
                o["target_name"],o["gameobject_type"],o["gameobject_loot_id"]
            ]))
            arrival=_arrival_for_type(o["objective_type"])
            for si,spawn in enumerate((o.get("objective_spawns") or [])[:64],1):
                lines.append("\t".join(tsv_escape(x) for x in [
                    "OS",p["quest_id"],oi,int(spawn["map_id"]),spawn["x"],spawn["y"],spawn["z"],
                    arrival,f"QuestDB objective {oi+1} spawn {si}"
                ]))

        # Legacy primary records remain for existing single-objective executors
        # and for backward-compatible diagnostics.
        for i,s in enumerate(p["objective_spawns"][:64],1):
            arrival=_arrival_for_type(p["objective_type"])
            lines.append("\t".join(tsv_escape(x) for x in ["S",p["quest_id"],int(s["map_id"]),s["x"],s["y"],s["z"],arrival,f"QuestDB objective spawn {i}"]))
        if p["objective_spawns"]:
            s=p["objective_spawns"][0]
            arrival=_arrival_for_type(p["objective_type"])
            lines.append("\t".join(tsv_escape(x) for x in ["D",p["quest_id"],int(s["map_id"]),s["x"],s["y"],s["z"],arrival,"QuestDB primary objective seed"]))

    out_path.write_text("\n".join(lines)+"\n",encoding="utf-8")
    supported=sum(1 for p in profiles if p.get("automatable"))
    print(f"runtime catalog: {out_path} quests={len(profiles)} objectives={sum(len(p.get('objectives',[])) for p in profiles)} automatable={supported} unsupported={len(profiles)-supported}")


def support_report(conn: sqlite3.Connection, cfg: Dict[str,Any], cls: Optional[str], race: Optional[str]) -> str:
    cat=area_catalog(conn,cfg,cls,race,None)
    profiles=[runtime_profile_for_quest(conn,q,cfg) for q in cat["quests"]]
    supported=[p for p in profiles if p.get("automatable")]
    unsupported=[p for p in profiles if not p.get("automatable")]
    lines=[
        f"Area: {cfg.get('name',cfg.get('id','area'))}",
        f"Quest profiles: {len(profiles)}",
        f"Automatable with current generic executors: {len(supported)}",
        f"Needs executor/data work: {len(unsupported)}",
        "",
        "Supported quests:",
    ]
    for p in supported:
        lines.append(f"  {p['quest_id']:5d}  L{p['min_level']:2d}/{p['quest_level']:2d}  {p['title']}")
    lines.extend(["", "Needs executor/data work:"])
    for p in unsupported:
        lines.append(f"  {p['quest_id']:5d}  L{p['min_level']:2d}/{p['quest_level']:2d}  {p['title']} :: {p['support_note']}")
    lines.extend(["", "Hub-exit/report quests inferred from QuestDB distance:"])
    for p in profiles:
        if p.get("hub_exit"):
            lines.append(f"  {p['quest_id']:5d}  {p['title']}  giver={p['giver_entry']} turnin={p['turnin_entry']}")
    return "\n".join(lines)


def stats(conn: sqlite3.Connection) -> None:
    for t in ["quest","quest_giver","quest_turnin","quest_objective","creature_template","creature_spawn","gameobject_template","gameobject_spawn","loot_source"]:
        print(f"{t}: {conn.execute(f'SELECT COUNT(*) FROM {t}').fetchone()[0]}")


def main() -> int:
    p=argparse.ArgumentParser(description="Phase 12A Vanilla/VMaNGOS quest database importer and query tool")
    sub=p.add_subparsers(dest="cmd",required=True)
    a=sub.add_parser("import-sql"); a.add_argument("--sql",required=True,type=Path); a.add_argument("--db",required=True,type=Path); a.add_argument("--patch",type=int,default=10)
    a=sub.add_parser("stats"); a.add_argument("--db",required=True,type=Path)
    a=sub.add_parser("quest"); a.add_argument("--db",required=True,type=Path); a.add_argument("quest_id",type=int); a.add_argument("--map",type=int,default=None)
    a=sub.add_parser("area"); a.add_argument("--db",required=True,type=Path); a.add_argument("--config",required=True,type=Path); a.add_argument("--class",dest="cls",default=None); a.add_argument("--race",default=None); a.add_argument("--level",type=int,default=None); a.add_argument("--json-out",type=Path); a.add_argument("--text-out",type=Path)
    a=sub.add_parser("find-item"); a.add_argument("--db",required=True,type=Path); a.add_argument("item_id",type=int); a.add_argument("--map",type=int,default=None)
    a=sub.add_parser("runtime-catalog"); a.add_argument("--db",required=True,type=Path); a.add_argument("--config",required=True,type=Path); a.add_argument("--class",dest="cls",default=None); a.add_argument("--race",default=None); a.add_argument("--out",required=True,type=Path)
    a=sub.add_parser("support-report"); a.add_argument("--db",required=True,type=Path); a.add_argument("--config",required=True,type=Path); a.add_argument("--class",dest="cls",default=None); a.add_argument("--race",default=None); a.add_argument("--out",type=Path)
    args=p.parse_args()
    if args.cmd=="import-sql":
        import_sql(args.sql,args.db,args.patch); return 0
    conn=sqlite3.connect(args.db)
    try:
        if args.cmd=="stats": stats(conn)
        elif args.cmd=="quest":
            q=quest_record(conn,args.quest_id,args.map); print(json.dumps(q,ensure_ascii=False,indent=2) if q else "null")
        elif args.cmd=="find-item": print(json.dumps(quest_sources_for_item(conn,args.item_id,args.map),ensure_ascii=False,indent=2))
        elif args.cmd=="area":
            cfg=json.loads(args.config.read_text(encoding="utf-8")); cat=area_catalog(conn,cfg,args.cls,args.race,args.level); text=summarize_area(cat); print(text)
            if args.json_out:
                args.json_out.parent.mkdir(parents=True,exist_ok=True); args.json_out.write_text(json.dumps(cat,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
            if args.text_out:
                args.text_out.parent.mkdir(parents=True,exist_ok=True); args.text_out.write_text(text+"\n",encoding="utf-8")
        elif args.cmd=="runtime-catalog":
            cfg=json.loads(args.config.read_text(encoding="utf-8")); export_runtime_catalog(conn,cfg,args.cls,args.race,args.out)
        elif args.cmd=="support-report":
            cfg=json.loads(args.config.read_text(encoding="utf-8")); text=support_report(conn,cfg,args.cls,args.race); print(text)
            if args.out:
                args.out.parent.mkdir(parents=True,exist_ok=True); args.out.write_text(text+"\n",encoding="utf-8")
    finally:
        conn.close()
    return 0

if __name__=="__main__":
    raise SystemExit(main())
