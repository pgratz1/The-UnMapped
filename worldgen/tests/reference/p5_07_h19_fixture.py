#!/usr/bin/env python3
"""Writes worldgen/tests/data/history-report/h19-v10.toml (P5-07 16 item 11) and prints the report lines the reference
gives for it (`--lines`).

  python3 -B worldgen/tests/reference/p5_07_h19_fixture.py worldgen/tests/reference \\
      worldgen/tests/data/history-report/h19-v10.toml [--lines]

H19 (format v10, schema 13; spec r2 16 item 11), in H17's shape: three races (elves, dwarves, goblins), four realms,
the 0.29.0 catalogue (kinds 93 to 99, r2) and the 0.29.0 templates (the data listings of p3_data_reference, 8.2), nine
monster kinds. It holds:
  - three dwarven mines deepened: si1 (the Hold's, at Gimlin) to depth 3, si2 (Clan Nirgurn's, at Kazmar) and si3
    (the Hold's, at its capital Kazad Rune) to depth 2;
  - a sealed horror: si3 wakes Bolgrim (ymrach) in 1150, sealed at once (lair si4 at Kazad Rune, settlement 0);
  - a lost hold and a realm lost to the deep: si2 wakes the Swarm Beneath Durak (ysgarth) in 1200, which takes Kazmar,
    Clan Nirgurn's only hold (not the dwarves' last: the Hold lives); the clan ends in 1201;
  - (r2) a waking with two aids: at si1's waking in 1500 the House of Aelin (from Aelinost) and the Gub Horde (from
    Gorzhak), allied with the Hold since 1460, send aid (kind 97); Gimlin falls all the same (hold_lost citing the
    waking, then both aids); the other wakings have none;
  - (r2) goblin warrens: the Gub Horde delves the warrens of Gorzhak to depth 3 (1510 to 1530), wakes Grubnak
    Stonegnawer (ymrach) there in 1540 and seals it (warren_sealed); then delves Gnashgut (founded 1450) to depth 2 and
    wakes the Eyeless of Gnashgut (ysgarth) in 1690, which takes it (hold_lost telling 3: 412 die, 618 flee);
  - a later take and a flight: si1 wakes Ugrak (zhakul) in 1500, which takes Gimlin; in 1600 it raids Kazad Rune and
    takes it, the dwarves' last refuge: the survivors flee into hiding and found Brunhal (refugees_settled caused by
    the hold_lost, its refuge_hidden, the capital moved); the sealed mine si3 ends with its town;
  - an alliance with a first withholding: the elves and the Gub Horde (700, in friendship); the Horde keeps the elves'
    share in 805 (grievance 1): `; soured e<id>`;
  - a realm whose top milestones repeat a kind: the Hold of Durn's two war_declared and two hold_lost of salience 70.
Written by the test writer from the spec; format: README.md in that directory.
"""
import os
import sys

REF = sys.argv[1]
OUT = sys.argv[2]
sys.path.insert(0, REF)
import p3_engine_reference as pe  # noqa: E402
import p3_data_reference as pdr  # noqa: E402

pe.use_version("0.29.0")
_root = pdr.load_root("game/data")
report = {e["kind"]: dict(e) for e in dict(_root["history"])["report"]["events"]}
cat = {k["key"]: k for k in pe.COMPILED_029}
E = 2000

ev = []


def add(year, kind, region=0, settlement=0, parts=(), values=(), causes=(), end=None):
    eid = len(ev) + 1
    assert all(c < eid for c in causes), (kind, causes)
    k = cat[kind]
    assert len(values) == len(k["values"]), (kind, values, k["values"])
    ev.append(dict(id=eid, year=year, kind=kind, region=region, settlement=settlement, parts=list(parts),
                   values=list(values), causes=sorted(set(causes)), end=end))
    return eid


AGES = [[1, 1, 0, 99, "The Age of Elves"], [2, 2, 100, 199, "The Age of Dwarves"],
        [3, 3, 200, 1999, "The Age of Goblins"]]
PHASES = [[1, 1, 1, 0, 99, 0], [2, 2, 1, 100, 199, 0], [3, 3, 1, 200, 1699, 0], [4, 3, 2, 1700, 1799, 0],
          [5, 3, 4, 1800, 1999, 0]]


def phase_of(y):
    for p in PHASES:
        if p[3] <= y <= p[4]:
            return p[0]
    raise ValueError(y)


REGIONS = ["Aelin Vale", "Kazad Fells", "The Grey Fells", "Gor Heath", "Iron Downs", "Brun Heights"]
# (id, name, race, region, founded)
CIVS = [(1, "The House of Aelin", 1, 1, 0), (2, "The Hold of Durn", 2, 2, 100), (3, "Clan Nirgurn", 2, 3, 110),
        (4, "The Gub Horde", 3, 4, 200)]
aw, civf, stf = {}, {}, {}
for r, y, reg in ((1, 0, 1), (2, 100, 2), (3, 200, 4)):
    aw[r] = add(y, "awakening", region=reg, parts=[("race", r), ("creator", r)])
for cid, nm, race, reg, y in CIVS:
    civf[cid] = add(y, "civ_founded", region=reg, parts=[("civilization", cid), ("race", race)], causes=[aw[race]])
# settlements: s1 Aelinost (c1), s2 Kazad Rune (c2, capital), s3 Kazmar (c3), s4 Gorzhak (c4), s5 Gimlin (c2)
STL0 = {1: ("Aelinost", 1, 1, 0), 2: ("Kazad Rune", 2, 2, 100), 3: ("Kazmar", 3, 3, 110), 4: ("Gorzhak", 4, 4, 200),
        5: ("Gimlin", 2, 5, 150)}
for sid in (1, 2, 3, 5, 4):
    nm, civ, reg, y = STL0[sid]
    stf[sid] = add(y, "settlement_founded", region=reg, settlement=sid, parts=[("settlement", sid), ("founder", civ)],
                   values=[500])

relations = []


def rel(kind, a, b, since, se, until=-1, ue=0):
    relations.append([3, a, 3, b, kind, since, until, se, ue])
    return len(relations) - 1


def close(i, until, ue):
    relations[i][6], relations[i][8] = until, ue


# --- mines (P5-04 6.12): si1 at Gimlin (400), si2 at Kazmar (420), si3 at Kazad Rune (450)
mine = {1: add(400, "mine_opened", region=5, settlement=5, parts=[("civilization", 2), ("site", 1)], causes=[stf[5]]),
        2: add(420, "mine_opened", region=3, settlement=3, parts=[("civilization", 3), ("site", 2)], causes=[stf[3]]),
        3: add(450, "mine_opened", region=2, settlement=2, parts=[("civilization", 2), ("site", 3)], causes=[stf[2]])}
MINE_AT = {1: (2, 5, 5), 2: (3, 3, 3), 3: (2, 2, 2)}   # site -> (civ, settlement, region)
delve = {}


def deepen(year, sid, d):
    civ, st, reg = MINE_AT[sid]
    cause = delve.get((sid, d - 1), mine[sid])
    delve[(sid, d)] = add(year, "delving_deepened", region=reg, settlement=st, parts=[("civilization", civ),
                                                                                      ("site", sid)],
                          values=[d, (sid + 4 * (d - 1)) % 9], causes=[cause])


for year, sid, d in ((560, 1, 1), (580, 2, 1), (620, 3, 1), (900, 1, 2), (1000, 2, 2), (1100, 3, 2), (1450, 1, 3)):
    deepen(year, sid, d)
# r2: Gnashgut (s7) founded by the Gub Horde in 1450 (after the last r1 event of that year: the r1 ids up to 1450 keep)
stf[7] = add(1450, "settlement_founded", region=4, settlement=7, parts=[("settlement", 7), ("founder", 4)],
             values=[500])
# --- 700: the elves and the Gub Horde ally in friendship (basis 3, term 150, band 2)
al14 = add(700, "alliance_formed", parts=[("ally_a", 1), ("ally_b", 4)], values=[3, 150, 2], causes=[civf[4]])
rel("alliance", 1, 4, 700, al14)
# --- 800: the Hold on the Gub Horde over Gor Heath; the elves defend the Horde; 802 an allied battle; 805 the Horde wins
# and keeps the elves' share of 150 (grievance 1: the alliance sours)
warA = add(800, "war_declared", region=4, parts=[("attacker", 2), ("defender", 4)], causes=[civf[4]])
warA_row = rel("war", 2, 4, 800, warA)
joinA = add(800, "ally_joined_war", parts=[("ally", 1), ("partner", 4), ("enemy", 2)], values=[1], causes=[warA, al14])
joinA_row = rel("war", 1, 2, 800, joinA)
batA = add(802, "allied_battle", region=4, parts=[("winner", 4), ("loser", 2), ("winner_ally", 1)], values=[50, 300, 0],
           causes=[warA])
wonA = add(805, "war_won", parts=[("winner", 4), ("loser", 2)], values=[5], causes=[warA])
tribA = add(805, "tribute", parts=[("payer", 2), ("payee", 4)], values=[100], causes=[wonA])
whA = add(805, "spoils_withheld", region=4, parts=[("principal", 4), ("ally", 1), ("enemy", 2)], values=[150, 1],
          causes=[wonA, joinA])
close(warA_row, 805, wonA)
close(joinA_row, 805, wonA)
rel("peace", 2, 4, 805, wonA, 1300, 0)
# --- 1150: si3 wakes Bolgrim (m1, ymrach) beneath Kazad Rune; the hold seals it (lair si4, settlement 0)
wk1 = add(1150, "deep_woken", region=2, settlement=2, parts=[("monster", 1), ("civilization", 2), ("site", 3),
                                                             ("lair", 4)], values=[2, 1 % 3], causes=[delve[(3, 2)]])
se1 = add(1150, "deep_sealed", region=2, settlement=2, parts=[("civilization", 2), ("monster", 1), ("site", 3),
                                                              ("lair", 4)], values=[(1 + 1) % 3], causes=[wk1])
# --- 1200: si2 wakes the Swarm Beneath Durak (m2, ysgarth) beneath Kazmar, which falls: 480 die, 720 survive; Clan
# Nirgurn has no other hold and ends in 1201
wk2 = add(1200, "deep_woken", region=3, settlement=3, parts=[("monster", 2), ("civilization", 3), ("site", 2),
                                                             ("lair", 5)], values=[2, 2 % 3], causes=[delve[(2, 2)]])
hl2 = add(1200, "hold_lost", region=3, settlement=3, parts=[("monster", 2), ("civilization", 3), ("lair", 5),
                                                            ("site", 2)], values=[480, 720, (2 + 3) % 3], causes=[wk2])
end3 = add(1201, "civ_ended", region=3, settlement=3, parts=[("civilization", 3)], causes=[hl2])
# --- 1300: the Hold on the Gub Horde again; 1310 peace
close(len(relations) - 1, 1300, 0)
warB = add(1300, "war_declared", region=4, parts=[("attacker", 2), ("defender", 4)], causes=[wonA])
warB_row = rel("war", 2, 4, 1300, warB)
pB = add(1310, "peace", parts=[("winner", 4), ("loser", 2)], values=[10], causes=[warB])
close(warB_row, 1310, pB)
relations[-2][6], relations[-2][8] = 1300, warB   # the peace of 805 closes at the declaration of 1300
rel("peace", 2, 4, 1310, pB)
# --- r2 1460: the Hold allies with the elves and with the Gub Horde
alA = add(1460, "alliance_formed", parts=[("ally_a", 1), ("ally_b", 2)], values=[3, 150, 2], causes=[civf[2]])
rel("alliance", 1, 2, 1460, alA)
alB = add(1460, "alliance_formed", parts=[("ally_a", 2), ("ally_b", 4)], values=[3, 150, 1], causes=[pB])
rel("alliance", 2, 4, 1460, alB)
# --- 1500: si1 (depth 3) wakes Ugrak the Unquenched (m3, zhakul) beneath Gimlin; (r2) the elves and the Horde send aid;
# Gimlin falls all the same: 800 die, 1200 survive
wk3 = add(1500, "deep_woken", region=5, settlement=5, parts=[("monster", 3), ("civilization", 2), ("site", 1),
                                                             ("lair", 6)], values=[3, 3 % 3], causes=[delve[(1, 3)]])
# P5-08 12.4 (integration part 2): the aid after the outcome; the outcome cites the waking only, each aid the waking and
# the outcome
hl3 = add(1500, "hold_lost", region=5, settlement=5, parts=[("monster", 3), ("civilization", 2), ("lair", 6),
                                                            ("site", 1)], values=[800, 1200, (3 + 5) % 3],
          causes=[wk3])
aid1 = add(1500, "ally_aided_hold", region=5, settlement=5, parts=[("ally", 1), ("partner", 2), ("monster", 3),
                                                                   ("origin", 1)], values=[(3 + 1) % 3],
           causes=[wk3, hl3])
aid2 = add(1500, "ally_aided_hold", region=5, settlement=5, parts=[("ally", 4), ("partner", 2), ("monster", 3),
                                                                   ("origin", 4)], values=[(3 + 4) % 3],
           causes=[wk3, hl3])
# --- r2: the warrens of Gorzhak (s4) to depth 3; Grubnak Stonegnawer (m4, ymrach) woken in 1540 and sealed (lair si7)
wdl = {}


def delve_warren(year, t, d):
    cause = wdl.get((t, d - 1), stf[t])
    wdl[(t, d)] = add(year, "warren_delved", region=4, settlement=t, parts=[("civilization", 4)],
                      values=[d, (t + 4 * (d - 1)) % 9], causes=[cause])


for year, t, d in ((1510, 4, 1), (1520, 4, 2), (1530, 4, 3)):
    delve_warren(year, t, d)
wk4 = add(1540, "deep_woken", region=4, settlement=4, parts=[("monster", 4), ("civilization", 4), ("lair", 7)],
          values=[3, 3 + 4 % 3], causes=[wdl[(4, 3)]])
ws4 = add(1540, "warren_sealed", region=4, settlement=4, parts=[("civilization", 4), ("monster", 4), ("lair", 7)],
          values=[(4 + 1) % 3], causes=[wk4])
# --- r2: the warrens of Gnashgut (s7) to depth 2; the Eyeless of Gnashgut (m5, ysgarth) woken in 1690 takes it
for year, t, d in ((1550, 7, 1), (1650, 7, 2)):
    delve_warren(year, t, d)
wk5 = add(1690, "deep_woken", region=4, settlement=7, parts=[("monster", 5), ("civilization", 4), ("lair", 8)],
          values=[2, 3 + 5 % 3], causes=[wdl[(7, 2)]])
hl5 = add(1690, "hold_lost", region=4, settlement=7, parts=[("monster", 5), ("civilization", 4), ("lair", 8)],
          values=[412, 618, (5 + 7) % 3 + 3], causes=[wk5])
# --- 1600: Ugrak raids Kazad Rune (the dwarves' last refuge) and takes it: 1206 die, 1810 survive; 1500 flee into
# hiding and found Brunhal (s6) in Brun Heights; the capital moves there; the sealed mine si3 ends with its town
rd = add(1600, "monster_raid", region=2, settlement=2, parts=[("monster", 3), ("civilization", 2)], values=[100, 60],
         causes=[wk3])
hl4 = add(1600, "hold_lost", region=2, settlement=2, parts=[("monster", 3), ("civilization", 2), ("lair", 6),
                                                            ("site", 3)], values=[1206, 1810, (3 + 2) % 3], causes=[rd])
rs = add(1600, "refugees_settled", region=6, settlement=6, parts=[("settlement", 6), ("founder", 2), ("origin", 2)],
         values=[1500], causes=[hl4])
rh = add(1600, "refuge_hidden", region=6, settlement=6, parts=[("settlement", 6), ("founder", 2)], causes=[rs],
         end=1899)
cm = add(1600, "capital_moved", region=6, settlement=6, parts=[("civilization", 2), ("settlement", 6)], causes=[hl4])

# event ids ascend with the year (P3-07 14.2): renumber by (year, order of writing)
_order = sorted(ev, key=lambda e: (e["year"], e["id"]))
_NEW = {e["id"]: i for i, e in enumerate(_order, 1)}
for e in _order:
    e["id"] = _NEW[e["id"]]
    e["causes"] = sorted(_NEW[c] for c in e["causes"])
    assert all(c < e["id"] for c in e["causes"])
ev[:] = _order
N = _NEW.get
for r in relations:
    r[7] = N(r[7])
    r[8] = N(r[8]) if r[8] else 0

races = [[1, "elf", "Elf", "Elves", "Elven", 0, 700, 150, 1, 1, 0, 1],
         [2, "dwarf", "Dwarf", "Dwarves", "Dwarven", 0, 300, 40, 2, 1, 100, 2],
         [3, "goblin", "Goblin", "Goblins", "Goblin", 2, 60, 20, 3, 1, 200, 3]]
# settlement rows (fourteen columns: ending derived from the log)
STL = {1: (1, 1, 3, 1, 2400, None), 2: (2, 2, 3, 3, 0, (1600, hl4)), 3: (3, 3, 2, 3, 0, (1200, hl2)),
       4: (4, 4, 3, 1, 2600, None), 5: (2, 5, 2, 3, 0, (1500, hl3)), 6: (2, 6, 2, 1, 1500, None),
       7: (4, 4, 2, 3, 0, (1690, hl5))}
stl_rows, census = [], []
NAMES = {1: "Aelinost", 2: "Kazad Rune", 3: "Kazmar", 4: "Gorzhak", 5: "Gimlin", 6: "Brunhal", 7: "Gnashgut"}
for sid in sorted(STL):
    civ, reg, kind, status, pop, ruin = STL[sid]
    y = 1600 if sid == 6 else 1450 if sid == 7 else STL0[sid][3]
    fe = N(rs) if sid == 6 else N(stf[sid])
    owner = 0 if ruin else civ
    stl_rows.append([sid, NAMES[sid], 10 * sid, 10 * sid, reg, civ, y, fe, owner, kind, status, pop,
                     ruin[0] if ruin else -1, N(ruin[1]) if ruin else 0])
    census.append([sid, y, civ, kind, 1, 1000 if ruin else pop])
    if ruin:
        census.append([sid, ruin[0], 0, kind, 3, 0])
sr = [[s[0], {1: 1, 2: 2, 3: 2, 4: 3, 5: 2, 6: 2, 7: 3}[s[0]], s[6], 1000, 3] for s in stl_rows]
rc = [[1, 0, -1, 1, N(civf[1])], [2, 100, 1600, 2, N(civf[2])], [3, 110, 1200, 3, N(civf[3])],
      [4, 200, -1, 4, N(civf[4])], [5, 150, 1500, 2, N(stf[5])], [6, 1600, -1, 2, N(rs)]]
mk = []
for i, (k, d) in enumerate(_root["monster"], 1):
    mk.append([i, k, d["name"], d["noun"], 1 if d["grouped"] else 0, pe.WAKE_CODE[d["wake"]], d["might"],
               "|".join(d["raid_phrases"])])
monsters = [[1, "Bolgrim the Deep-Coiled", 7, 4, 1150, N(wk1), -1, 0, 0, 2, 0],
            [2, "The Swarm Beneath Durak", 8, 5, 1200, N(wk2), -1, 0, 0, 1, 0],
            [3, "Ugrak the Unquenched", 9, 6, 1500, N(wk3), -1, 0, 0, 1, 60],
            [4, "Grubnak Stonegnawer", 7, 7, 1540, N(wk4), -1, 0, 0, 2, 0],
            [5, "The Eyeless of Gnashgut", 8, 8, 1690, N(wk5), -1, 0, 0, 1, 0]]
sites = [[1, "The Mines of Gimlin", 3, 5, 50, 50, 5, 2, 0, 400, N(mine[1]), 1500, N(hl3), 4],
         [2, "The Mines of Kazmar", 3, 3, 30, 30, 3, 3, 0, 420, N(mine[2]), 1200, N(hl2), 4],
         [3, "The Mines of Kazad Rune", 3, 2, 20, 20, 2, 2, 0, 450, N(mine[3]), 1600, N(hl4), 4],
         [4, "The Coils of Bolgrim", 1, 2, 20, 20, 0, 0, 1, 1150, N(wk1), -1, 0, 0],
         [5, "The Warrens of Durak", 1, 3, 30, 30, 3, 0, 2, 1200, N(wk2), -1, 0, 0],
         [6, "Ugrak's Furnace", 1, 5, 50, 50, 5, 0, 3, 1500, N(wk3), -1, 0, 0],
         [7, "The Coils of Grubnak", 1, 4, 40, 40, 0, 0, 4, 1540, N(wk4), -1, 0, 0],
         [8, "The Nest of Gnashgut", 1, 4, 70, 70, 7, 0, 5, 1690, N(wk5), -1, 0, 0]]
KINDS = [k["key"] for k in pe.COMPILED_029]
civ_rows = [[cid, nm, race, race, reg, cid, y, N(civf[cid]), -1, 0] for cid, nm, race, reg, y in CIVS]
civ_rows[1][5] = 6                       # the Hold's capital moved to Brunhal in 1600
civ_rows[2][8], civ_rows[2][9] = 1201, N(end3)


def toml_value(v):
    if isinstance(v, str):
        return '"%s"' % v.replace("\\", "\\\\").replace('"', '\\"')
    if isinstance(v, (list, tuple)):
        return "[" + ", ".join(toml_value(x) for x in v) + "]"
    return str(v)


def rows(name, rs_):
    if not rs_:
        return "%s = []\n" % name
    return "%s = [\n%s]\n" % (name, "".join("    %s,\n" % toml_value(r) for r in rs_))


IDS = dict(wk1=wk1, se1=se1, wk2=wk2, hl2=hl2, end3=end3, wk3=wk3, hl3=hl3, rd=rd, hl4=hl4, rs=rs, rh=rh, cm=cm,
           al14=al14, whA=whA, warA=warA, warB=warB, alA=alA, alB=alB, aid1=aid1, aid2=aid2, wk4=wk4, ws4=ws4,
           wk5=wk5, hl5=hl5, f7=stf[7], w41=wdl[(4, 1)], w42=wdl[(4, 2)], w43=wdl[(4, 3)], w71=wdl[(7, 1)],
           w72=wdl[(7, 2)],
           d11=delve[(1, 1)], d12=delve[(1, 2)], d13=delve[(1, 3)], d21=delve[(2, 1)], d22=delve[(2, 2)],
           d31=delve[(3, 1)], d32=delve[(3, 2)])
IDS = {k: N(v) for k, v in IDS.items()}
o = ["""# P5-07 16 item 11: H19 (format v10, schema 13; the spec's "H18": P5-04a holds that name). Generated by
# p5_07_h19_fixture.py, whose docstring says what each event is; the 0.29.0 catalogue and templates (8.1, 8.2).
# Sealed e%(se1)d; lost holds e%(hl2)d (a realm lost, e%(end3)d), e%(hl3)d (two aids e%(aid1)d, e%(aid2)d) and the later
# take e%(hl4)d with its flight e%(rs)d; the first withholding e%(whA)d; (r2) warren Gorzhak sealed e%(ws4)d, warren
# Gnashgut lost e%(hl5)d. Written by the test writer from the spec (r2); format: README.md.
""" % IDS]
o.append("""[meta]
seed = 1919
seed_phrase = ""
map_size = 512
history_span = 2000
history_detail = "normal"
races = "elf,dwarf,goblin"
civs_per_race = "auto"
event_intensity = "normal"
monster_pressure = "normal"
divine_activity = "normal"
divine_script = ""

[tables]
biome = [[1, "grassland", "Grassland", 8421376, 0]]
""")
o.append(rows("region", [[i + 1, nm, 1, 2000, 0, 1500] for i, nm in enumerate(REGIONS)]))
o.append(rows("race", races))
o.append('culture = [[1, "elven", "Elven", 1], [2, "dwarven", "Dwarven", 2], [3, "goblin", "Goblin", 3]]\n')
o.append('god = [[1, "Aelor", 0], [2, "Durin", 1], [3, "Gruzz", 2]]\n')
o.append(rows("age", AGES))
o.append(rows("phase", PHASES))
o.append(rows("civilization", civ_rows))
o.append("# Fourteen columns: settlement.ending is derived from the log (P5-02 8.3; P5-07 6.6: a hold_lost is 2).\n")
o.append(rows("settlement", stl_rows))
o.append(rows("settlement_census", sorted(census)))
o.append(rows("region_control", sorted(rc)))
o.append(rows("relation", sorted(relations, key=lambda r: (r[0], r[1], r[2], r[3], r[4], r[5]))))
o.append("region_name = []\n")
o.append("beast = []\n")
o.append(rows("monster_kind", mk))
o.append("beast_population = []\n")
o.append(rows("monster", monsters))
o.append(rows("site", sites))
o.append("race_relation = []\n")
o.append(rows("settlement_race", sorted(sr)))
o.append("figure = []\nbook = []\n")
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
                                                                             phase_of(e["year"])))
    if e["end"] is not None:
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
print("events %d; ids %s" % (len(ev), " ".join("%s=%d" % kv for kv in sorted(IDS.items()))))

if "--lines" in sys.argv:
    # The reference's v10 report of the fixture (p3_report_reference at 0.29.0), both levels, and its rows' checks.
    import tempfile
    import p3_report_reference as prr
    import p5_07_depths_reference as p507
    prr.use_version("0.29.0")
    db = os.path.join(tempfile.mkdtemp(), "h19-v10.db")
    prr.write_fixture("h19-v10", db)
    for level in ("summary", "full"):
        print("--- report %s" % level)
        sys.stdout.write(prr.report(prr.load_world(db), level).decode("latin-1"))
    t = prr.load_world(db).t
    print("--- reader problems", p507.reader_problems(t))
    print("--- stats", p507.stats_of_tables(t))
