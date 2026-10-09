"""Source-preserving metadata enrichment; absent SQL columns remain NULL.

Relationships follow Player::SatisfyQuestPreviousQuest and ObjectMgr's
NextQuestId inversion. Conditions/scripts are preserved, not interpreted.
"""
import json
import math
import struct

def client_triggers(path):
    if not path:
        return {}
    with open(path,"rb") as source:
        magic, count, fields, size, strings = struct.unpack("<4s4I",source.read(20))
        if magic != b"WDBC" or fields != 10 or size != 40 or count > 10000:
            raise ValueError("unverified AreaTrigger DBC layout")
        records = [struct.unpack("<II8f",source.read(size)) for _ in range(count)]
        if len(source.read()) != strings:
            raise ValueError("AreaTrigger DBC size mismatch")
        return {r[0]:r for r in records}

def verified_spheres(q, dbc):
    result=[]
    for t in q.get("area_triggers",[]):
        r=dbc.get(t["id"])
        if not r or t.get("scripted") or t.get("radius",0)<=0:
            continue
        sql=[t.get(k) for k in ("mapid","x","y","z","radius","boxx","boxy","boxz","boxorientation")]
        if any(v is None or not math.isfinite(v) for v in sql): continue
        if sql[0]!=r[1] or any(abs(a-b)>0.001 for a,b in zip(sql[1:],r[2:])): continue
        if any(sql[i]!=0 for i in (5,6,7)): continue
        result.append(t)
    return sorted(result,key=lambda t:t["id"])[:8]

FIELDS = ("Method", "QuestFlags", "SpecialFlags", "ExclusiveGroup",
          "RequiredSkill", "RequiredSkillValue", "StartScript", "CompleteScript",
          "RepObjectiveFaction", "LimitTime", "RewOrReqMoney", "BreadcrumbForQuestId")
TABLES = {"conditions", "areatrigger_template", "areatrigger_involvedrelation",
          "scripted_areatrigger", "gossip_menu", "gossip_menu_option"}
SCHEMA = """
CREATE TABLE IF NOT EXISTS quest_metadata(quest_id INTEGER PRIMARY KEY, payload TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS source_metadata(kind TEXT NOT NULL, identity TEXT NOT NULL,
    payload TEXT NOT NULL, PRIMARY KEY(kind,identity));
"""

def preserve(conn, table, d):
    if table not in TABLES and table != "creature_template":
        return False
    if table == "areatrigger_template":
        identity = f"{d.get('id')}:{d.get('build')}"
    elif table == "gossip_menu":
        identity = f"{d.get('entry')}:{d.get('textid')}"
    elif table == "gossip_menu_option":
        identity = f"{d.get('menuid')}:{d.get('id')}"
    else:
        identity = str(d.get("conditionentry", d.get("entry", d.get("id"))))
    if table == "creature_template":
        # Only metadata needed to prove a simple gossip credit action.
        d = {k: d.get(k) for k in ("entry", "gossipmenuid", "npcflags", "scriptname")}
    conn.execute("INSERT OR REPLACE INTO source_metadata VALUES (?,?,?)",
                 (table, identity, json.dumps(d, sort_keys=True)))
    return table != "creature_template"

def quest(conn, qid, d):
    conn.execute("INSERT OR REPLACE INTO quest_metadata VALUES (?,?)",
                 (qid, json.dumps({f: d.get(f.lower()) for f in FIELDS}, sort_keys=True)))

def available(conn):
    return conn.execute("SELECT 1 FROM sqlite_master WHERE name='quest_metadata'").fetchone() is not None

def rows(conn, kind):
    return [json.loads(r[0]) for r in conn.execute(
        "SELECT payload FROM source_metadata WHERE kind=? ORDER BY identity", (kind,))]

def enrich(conn, q):
    if not available(conn):
        q["metadata"] = {}
        return
    row = conn.execute("SELECT payload FROM quest_metadata WHERE quest_id=?", (q["quest_id"],)).fetchone()
    q["metadata"] = json.loads(row[0]) if row else {}
    # Keep signed alternatives, including reverse NextQuestId links. Each
    # alternative can expand to an ALL group, never flattened to a single ID.
    previous = {int(q["prev_quest_id"])} - {0}
    for qid, nxt in conn.execute("SELECT quest_id,next_quest_id FROM quest WHERE abs(next_quest_id)=?", (q["quest_id"],)):
        previous.add(qid if nxt > 0 else -qid)
    clauses = []
    all_meta = {qid: json.loads(payload) for qid, payload in conn.execute("SELECT * FROM quest_metadata")}
    for prev in sorted(previous):
        meta = all_meta.get(abs(prev), {})
        group = meta.get("ExclusiveGroup")
        members = sorted(qid for qid, m in all_meta.items() if group is not None and group < 0 and m.get("ExclusiveGroup") == group)
        clauses.append({"active": prev < 0, "resolved": group is not None,
                        "ids": members if members else [abs(prev)]})
    q["prerequisite_clauses"] = clauses
    group = q["metadata"].get("ExclusiveGroup")
    q["exclusive_peers"] = sorted(qid for qid, m in all_meta.items()
        if group is not None and group > 0 and m.get("ExclusiveGroup") == group and qid != q["quest_id"])
    relations = [r for r in rows(conn, "areatrigger_involvedrelation") if r.get("quest") == q["quest_id"]]
    templates = rows(conn, "areatrigger_template")
    scripted = {r["entry"] for r in rows(conn, "scripted_areatrigger")}
    q["area_triggers"] = []
    for relation in relations:
        candidates = [t for t in templates if t.get("id") == relation["id"] and t.get("build", 99999) <= 5875]
        if candidates:
            t = max(candidates, key=lambda t: t["build"])
            q["area_triggers"].append(dict(t, scripted=t["id"] in scripted))
    q["area_trigger_ids"] = sorted(r["id"] for r in relations)

def npc_metadata(conn, entry):
    if not available(conn):
        return {}
    row = conn.execute("SELECT payload FROM source_metadata WHERE kind='creature_template' AND identity=?", (str(entry),)).fetchone()
    return json.loads(row[0]) if row else {}


def gossip_credit(conn, entry):
    """Only one unconditional terminal gossip option; never a dialogue script."""
    npc = npc_metadata(conn, entry)
    if npc.get("scriptname") != "" or not npc.get("gossipmenuid"):
        return None
    menu = npc["gossipmenuid"]
    menus = [r for r in rows(conn, "gossip_menu") if r.get("entry") == menu]
    options = [r for r in rows(conn, "gossip_menu_option") if r.get("menuid") == menu]
    if not menus or any(r.get("scriptid") != 0 or r.get("conditionid") != 0 for r in menus) or len(options) != 1:
        return None
    o = options[0]
    if (o.get("optionid") != 1 or o.get("actionmenuid", 0) >= 0 or
        any(o.get(k) != 0 for k in ("actionscriptid", "actionpoiid", "boxcoded", "boxmoney", "conditionid")) or
        not o.get("optiontext")):
        return None
    return o["optiontext"]
