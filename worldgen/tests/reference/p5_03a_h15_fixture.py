#!/usr/bin/env python3
"""Writes worldgen/tests/data/history-report/h15-v6.toml (P5-03a 14 item 11) from the 0.25.0 listings.

  python3 -B worldgen/tests/reference/p5_03a_h15_fixture.py worldgen/tests/reference \
      worldgen/tests/data/history-report/h15-v6.toml
"""
import os
import sys
REF = sys.argv[1]
OUT = sys.argv[2]
sys.path.insert(0, REF)
import p3_conflict_reference as pcr  # noqa: E402
pcr.use_version("0.25.0")
import p3_engine_reference as pe  # noqa: E402
import p3_data_reference as pdr  # noqa: E402

E = 2000
report = {e["kind"]: e for e in pdr.parsed_spec("history")["report"]["events"]}
cat = {k["key"]: k for k in pe.COMPILED_025}
KINDS = ["awakening", "civ_founded", "settlement_founded", "peoples_met", "disposition_turned", "war_declared",
         "settlement_captured", "peace", "settlement_mixed", "people_expelled", "plague_outbreak",
         "settlement_abandoned", "refugees_arrived", "quiet_stretch"]

ev = []


def add(year, kind, phase, region=0, settlement=0, parts=(), values=(), causes=(), end=None):
    eid = len(ev) + 1
    assert all(c < eid for c in causes)
    k = cat[kind]
    assert len(values) == len(k["values"]), (kind, values)
    ev.append(dict(id=eid, year=year, end=year if end is None else end, kind=kind, phase=phase, region=region, settlement=settlement,
                   parts=list(parts), values=list(values), causes=sorted(causes)))
    return eid


def phase_of(y):
    for p in PHASES:
        if p[3] <= y <= p[4]:
            return p[0]
    raise ValueError(y)


# ages: elves, dwarves, goblins, humans; phases: three openings, then active, quiet, final in the humans' age
AGES = [[1, 1, 0, 99, "The Age of Elves"], [2, 2, 100, 199, "The Age of Dwarves"],
        [3, 3, 200, 299, "The Age of Goblins"], [4, 4, 300, 1999, "The Age of Humans"]]
PHASES = [[1, 1, 1, 0, 99, 0], [2, 2, 1, 100, 199, 0], [3, 3, 1, 200, 299, 0], [4, 4, 1, 300, 599, 0],
          [5, 4, 2, 600, 1399, 0], [6, 4, 4, 1400, 1999, 0]]
REGIONS = ["Aelin Vale", "Kazad Fells", "Gor Heath", "Cear Downs", "Fiev Wood", "Thief Moor", "Duld Hills",
           "Bruzg Marsh", "Ash Vale"]

# 1-12 awakenings, realms and capitals
aw, civf, cap = {}, {}, {}
for r, (y, reg, nm) in enumerate(((0, 1, "Aelinost"), (100, 2, "Kazad Rune"), (200, 3, "Gorzhak"),
                                  (300, 4, "Cearholm")), 1):
    aw[r] = add(y, "awakening", phase_of(y), region=reg, parts=[("race", r), ("creator", r)])
    civf[r] = add(y, "civ_founded", phase_of(y), region=reg, parts=[("civilization", r), ("race", r)],
                  causes=[aw[r]])
    cap[r] = add(y, "settlement_founded", phase_of(y), region=reg, settlement=r,
                 parts=[("settlement", r), ("founder", r)], values=[500])
# settlements 5.. : elf Fievos (5), elf Thiefann (6), dwarf Duldar (7), dwarf hamlet Duldum (8),
# then sixteen goblin hamlets (9 to 24) that are ruined at sixteen distinct years with sixteen distinct sizes
STL = {1: ("Aelinost", 1, 1, 0, cap[1]), 2: ("Kazad Rune", 2, 2, 100, cap[2]), 3: ("Gorzhak", 3, 3, 200, cap[3]),
       4: ("Cearholm", 4, 4, 300, cap[4])}
founds = {}
for sid, nm, civ, reg, y in ((5, "Fievos", 1, 5, 50), (6, "Thiefann", 1, 6, 60), (7, "Duldar", 2, 7, 150),
                             (8, "Duldum", 2, 7, 160)):
    founds[sid] = add(y, "settlement_founded", phase_of(y), region=reg, settlement=sid,
                      parts=[("settlement", sid), ("founder", civ)], values=[300])
    STL[sid] = (nm, civ, reg, y, founds[sid])
GOB = [("Bruzg", 230), ("Gash", 240), ("Krok", 250), ("Mugg", 260), ("Nazg", 270), ("Pulg", 280), ("Rakh", 290),
       ("Snag", 305), ("Thokk", 315), ("Urzh", 325), ("Vrog", 335), ("Yagg", 345), ("Zogg", 355), ("Blik", 365),
       ("Drub", 375), ("Frag", 385)]
for i, (nm, y) in enumerate(GOB):
    sid = 9 + i
    founds[sid] = add(y, "settlement_founded", phase_of(y), region=8, settlement=sid,
                      parts=[("settlement", sid), ("founder", 3)], values=[100])
    STL[sid] = (nm, 3, 8, y, founds[sid])
# 310 the elves and goblins meet (wary); 395 war; 400 Fievos taken; 401 turn to hatred; 405 Thiefann taken
met_eg = add(310, "peoples_met", phase_of(310), region=3, parts=[("race_a", 1), ("race_b", 3)], values=[-300, 1, 0],
             causes=[civf[3]])
war = add(395, "war_declared", phase_of(395), region=5, parts=[("attacker", 3), ("defender", 1)], causes=[met_eg])
cap5 = add(400, "settlement_captured", phase_of(400), region=5, settlement=5, parts=[("attacker", 3), ("defender", 1)],
           values=[40, 0], causes=[war])
mix5 = add(400, "settlement_mixed", phase_of(400), region=5, settlement=5,
           parts=[("settlement", 5), ("civilization", 3), ("race", 1), ("ruling", 3)], values=[850, 0, 1],
           causes=[cap5])
turn1 = add(401, "disposition_turned", phase_of(401), parts=[("race_a", 1), ("race_b", 3)], values=[-520, 0],
            causes=[cap5])
cap6 = add(405, "settlement_captured", phase_of(405), region=6, settlement=6, parts=[("attacker", 3), ("defender", 1)],
           values=[20, 0], causes=[war])
mix6 = add(405, "settlement_mixed", phase_of(405), region=6, settlement=6,
           parts=[("settlement", 6), ("civilization", 3), ("race", 1), ("ruling", 3)], values=[700, 0, 1],
           causes=[cap6])
peace = add(410, "peace", phase_of(410), parts=[("winner", 3), ("loser", 1)], values=[15], causes=[war])
# 600 the dwarves have word of the goblins (sealed, hatred); 650 plague in Duldum, 651 abandoned, 652 the dwarves
# shelter in Gorzhak, held as subjects (1 arrival); the mixed sentence of the coming of refugees
met_dg = add(600, "peoples_met", phase_of(600), region=3, parts=[("race_a", 2), ("race_b", 3)],
             values=[-650, 0, 1], causes=[civf[3]])
plg = add(650, "plague_outbreak", phase_of(650), region=7, settlement=8, parts=[("settlement", 8), ("civilization", 2)],
          values=[90, 120, 90, 1], causes=[])
ab8 = add(651, "settlement_abandoned", phase_of(651), region=7, settlement=8,
          parts=[("settlement", 8), ("civilization", 2)], values=[0], causes=[plg])
arr = add(652, "refugees_arrived", phase_of(652), region=3, settlement=3,
          parts=[("settlement", 3), ("civilization", 3), ("origin", 8), ("race", 2)], values=[30, 0],
          causes=[plg, ab8])
mix3 = add(652, "settlement_mixed", phase_of(652), region=3, settlement=3,
           parts=[("settlement", 3), ("civilization", 3), ("race", 2), ("ruling", 3)], values=[100, 0, 0],
           causes=[arr])
# 700 and 1000 two quiet stretches of the goblins (the second the longer)
q1 = add(700, "quiet_stretch", phase_of(700), parts=[("realm", 3)], values=[120, 1], end=819)
q2 = add(1000, "quiet_stretch", phase_of(1000), parts=[("realm", 3)], values=[300, 1], end=1299)
# 900 the goblins drive the elves out of Fievos after 500 years of subjection (1 time); no shelter
exp5 = add(900, "people_expelled", phase_of(900), region=5, settlement=5,
           parts=[("settlement", 5), ("civilization", 3), ("race", 1)], values=[340, 0, 500], causes=[cap5, turn1])
# 1200 the elves and goblins grow wary again after long years of peace (a warmer turn citing the meeting)
turn2 = add(1200, "disposition_turned", phase_of(1200), parts=[("race_a", 1), ("race_b", 3)], values=[-440, 1],
            causes=[met_eg])
# 1300 the goblins take Duldar from the dwarves: the second dwarf-goblin mixed event
war2 = add(1290, "war_declared", phase_of(1290), region=7, parts=[("attacker", 3), ("defender", 2)], causes=[met_dg])
cap7 = add(1300, "settlement_captured", phase_of(1300), region=7, settlement=7,
           parts=[("attacker", 3), ("defender", 2)], values=[30, 0], causes=[war2])
mix7 = add(1300, "settlement_mixed", phase_of(1300), region=7, settlement=7,
           parts=[("settlement", 7), ("civilization", 3), ("race", 2), ("ruling", 3)], values=[900, 0, 1],
           causes=[cap7])
# the sixteen goblin hamlets abandoned at distinct years (1410 + 31 i); Duldum is the seventeenth ruin
RUIN = {8: (651, ab8)}
for i in range(16):
    sid = 9 + i
    y = 1410 + 31 * i
    RUIN[sid] = (y, add(y, "settlement_abandoned", phase_of(y), region=8, settlement=sid,
                        parts=[("settlement", sid), ("civilization", 3)], values=[0], causes=[founds[sid]]))
assert len(RUIN) == 17

# sizes of the goblin hamlets: distinct, not in founding order
SIZE = {9 + i: 100 + 37 * ((5 * i + 3) % 16) for i in range(16)}
SIZE[8] = 330

# tables
# race rows [id, key, name, plural, adjective, camp, lifespan_years, generation_years, culture, present,
# awakening_year, creator_god]
races = [[1, "elf", "Elf", "Elves", "Elven", 0, 700, 150, 1, 1, 0, 1],
         [2, "dwarf", "Dwarf", "Dwarves", "Dwarven", 0, 300, 40, 2, 1, 100, 2],
         [3, "goblin", "Goblin", "Goblins", "Goblin", 2, 60, 20, 3, 1, 200, 3],
         [4, "human", "Human", "Humans", "Human", 1, 80, 25, 4, 1, 300, 4]]
owner_end = {1: 1, 2: 2, 3: 3, 4: 4, 5: 3, 6: 3, 7: 3}
stl_rows, census, sr = [], [], []
for sid in sorted(STL):
    nm, civ, reg, y, fe = STL[sid]
    if sid in RUIN:
        ry, re_ = RUIN[sid]
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, 0, 1, 3, 0, ry, re_])
        census += [[sid, y, civ, 1, 1, SIZE[sid]], [sid, ry, 0, 1, 3, 0]]
        sr.append([sid, races[civ - 1][0], y, 1000, 3])
    else:
        o = owner_end[sid]
        kind = 3 if sid <= 4 else 2
        pop = {1: 2000, 2: 1500, 3: 1800, 4: 1600, 5: 400, 6: 500, 7: 600}[sid]
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, o, kind, 1, pop, -1, 0])
        census.append([sid, y, civ, kind, 1, 300 if sid > 4 else 500])
        if o != civ:
            cy = {5: 400, 6: 405, 7: 1300}[sid]
            census.append([sid, cy, o, kind, 1, pop])
# settlement_race: the founders' race at 1000 from the founding; the conquests and arrivals
sr += [[1, 1, 0, 1000, 3], [2, 2, 100, 1000, 3], [4, 4, 300, 1000, 3],
       [3, 2, 652, 100, 0], [3, 3, 200, 1000, 3], [3, 3, 652, 900, 3],
       [5, 1, 50, 1000, 3], [5, 1, 400, 850, 0], [5, 1, 900, 0, 0], [5, 3, 400, 150, 3], [5, 3, 900, 1000, 3],
       [6, 1, 60, 1000, 3], [6, 1, 405, 700, 0], [6, 3, 405, 300, 3],
       [7, 2, 150, 1000, 3], [7, 2, 1300, 900, 0], [7, 3, 1300, 100, 3]]
rr = [[1, 3, 310, -300, 1], [1, 3, 401, -520, 0], [1, 3, 1200, -440, 1], [1, 3, 1999, -430, 1],
      [2, 3, 600, -650, 0], [2, 3, 1999, -640, 0]]
rc = [[1, 0, -1, 1, cap[1]], [2, 100, -1, 2, cap[2]], [3, 200, -1, 3, cap[3]], [4, 300, -1, 4, cap[4]],
      [5, 50, 400, 1, founds[5]], [5, 400, -1, 3, cap5], [6, 60, 405, 1, founds[6]], [6, 405, -1, 3, cap6],
      [7, 150, 1300, 2, founds[7]], [7, 1300, -1, 3, cap7], [8, 230, 1875, 3, founds[9]]]
relations = [[3, 1, 3, 2, "enmity", 500, -1, civf[2], 0], [3, 3, 3, 1, "war", 395, 410, war, peace], [3, 3, 3, 2, "enmity", 600, -1, met_dg, 0],
             [3, 3, 3, 2, "war", 1290, -1, war2, 0]]


def toml_value(v):
    if isinstance(v, str):
        return '"%s"' % v.replace("\\", "\\\\").replace('"', '\\"')
    if isinstance(v, (list, tuple)):
        return "[" + ", ".join(toml_value(x) for x in v) + "]"
    return str(v)


def rows(name, rs):
    if not rs:
        return "%s = []\n" % name
    return "%s = [\n%s]\n" % (name, "".join("    %s,\n" % toml_value(r) for r in rs))


o = ["""# P5-03a 14 item 11: H15 v6 (format v6, schema 10), from H14. Four races (elf 1, dwarf 2, goblin 3, human 4) and
# four realms. The elves and goblins meet wary (e%d), come to hate one another after the goblins take Fievos (e%d,
# e%d) and grow wary again after long years of peace (e%d, a warmer turn citing the meeting: "from hatred"). The
# dwarves have word of the goblins (e%d, sealed 1: "by word"); elves and dwarves never meet; the humans meet no one.
# Four settlement_mixed events of two race pairs (elf-goblin e%d, e%d; dwarf-goblin e%d, e%d): only the first of each
# pair is a summary candidate (9.5). One arrival (e%d, the dwarves of plague-struck Duldum: causes the plague and the
# abandonment, 7.3) and one expulsion (e%d, standing 0, years 500). Seventeen ruins of seventeen ruined years and
# sizes (9.3). The goblin realm has more than eight milestone events, two of them quiet stretches (e%d, e%d; the
# longer, e%d, is the one milestone of its kind, P3-07 24.5), two wars; the elves an enmity without war with the
# dwarves (9.2: no "without war" line).
# Written by the test writer from the spec (P5-03a 9); generated by a script from the 0.25.0 listings of
# p3_data_reference.py; format: README.md in this directory.
""" % (met_eg, cap5, turn1, turn2, met_dg, mix5, mix6, mix3, mix7, arr, exp5, q1, q2, q2)]
o.append("""[meta]
seed = 1515
seed_phrase = ""
map_size = 512
history_span = 2000
history_detail = "normal"
races = "elf,dwarf,goblin,human"
civs_per_race = "auto"
event_intensity = "normal"
monster_pressure = "normal"
divine_activity = "normal"
divine_script = ""

[tables]
biome = [[1, "grassland", "Grassland", 8421376, 0]]
""")
o.append(rows("region", [[i + 1, n, 1, 2000, 0, 1500] for i, n in enumerate(REGIONS)]))
o.append(rows("race", races))
o.append("culture = [[1, \"elven\", \"Elven\", 1], [2, \"dwarven\", \"Dwarven\", 2], [3, \"goblin\", \"Goblin\", 3], "
         "[4, \"human\", \"Human\", 4]]\n")
o.append("god = [[1, \"Aelor\", 0], [2, \"Durin\", 1], [3, \"Gruzz\", 2], [4, \"Wealstuld\", 1]]\n")
o.append(rows("age", AGES))
o.append(rows("phase", PHASES))
o.append(rows("civilization", [[1, "The House of Aelin", 1, 1, 1, 1, 0, civf[1], -1, 0],
                               [2, "The Hold of Kazad", 2, 2, 2, 2, 100, civf[2], -1, 0],
                               [3, "The Krurz Horde", 3, 3, 3, 3, 200, civf[3], -1, 0],
                               [4, "The Kingdom of Cear", 4, 4, 4, 4, 300, civf[4], -1, 0]]))
o.append("# Fourteen columns: settlement.ending is derived from the log (P5-02 8.3).\n")
o.append(rows("settlement", stl_rows))
o.append(rows("settlement_census", sorted(census)))
o.append(rows("region_control", rc))
o.append(rows("relation", relations))
o.append("region_name = []\n")
o.append("beast = []\nmonster_kind = []\nbeast_population = []\nmonster = []\nsite = []\n")
o.append(rows("race_relation", sorted(rr)))
o.append(rows("settlement_race", sorted(sr)))
for k in KINDS:
    d = cat[k]
    r = report[k]
    o.append("\n[[kind]]\nkey = \"%s\"\ncaused = %d\nregion_use = %d\nsettlement_use = %d\nsalience = %d\n"
             "template = %s\nroles = %s\n" % (k, d["caused"], d["region_use"], d["settlement_use"], r["salience"],
                                             toml_value(r["template"]),
                                             toml_value([[x["role"], x["type"], x["min"], x["max"]]
                                                         for x in d["roles"]])))
    if d["values"]:
        o.append("values = %s\n" % toml_value(d["values"]))
for e in ev:
    o.append("\n[[event]]\nid = %d\nyear = %d\nkind = \"%s\"\nphase = %d\n" % (e["id"], e["year"], e["kind"],
                                                                             e["phase"]))
    if e["end"] != e["year"]:
        o.append("end = %d\n" % e["end"])
    if e["region"]:
        o.append("region = %d\n" % e["region"])
    if e["settlement"]:
        o.append("settlement = %d\n" % e["settlement"])
    o.append("parts = %s\n" % toml_value([list(p) for p in e["parts"]]))
    if e["values"]:
        o.append("values = %s\n" % toml_value(e["values"]))
    if e["causes"]:
        o.append("causes = %s\n" % toml_value(e["causes"]))
with open(OUT, "w", newline="\n") as f:
    f.write("".join(o))
print("events", len(ev))
