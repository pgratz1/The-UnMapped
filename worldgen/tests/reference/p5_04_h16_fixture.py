#!/usr/bin/env python3
"""Writes worldgen/tests/data/history-report/h16-alliances.toml (P5-04 18.1 item 16) from the 0.26.0 listings.

  python3 -B worldgen/tests/reference/p5_04_h16_fixture.py worldgen/tests/reference \
      worldgen/tests/data/history-report/h16-alliances.toml

H16 (format v7, schema 11), from H15's shape: five races and seven realms. An origin alliance (elves and faeries,
from the faeries' founding, standing at E); a foe alliance (the Hold of Durn and the Kingdom of Cear against the Gub
Horde) with two joins in defence and two allied battles, two shares withheld and a spurning at its renewal (reason
0); a share granted (the faeries in the elves' war); a refusal that breaks a goblin alliance (refusals_to_break 1,
as alliances-small); a betrayal that also breaks a trade; a lapse (no cause left) and a bloc lapse; three trades,
one broken; a dwarven mine with an arms sale, and a second mine lost with its town. Written by the test writer from
the spec (P5-04 10, 18.1 item 16); format: README.md in that directory.
"""
import os
import sys

REF = sys.argv[1]
OUT = sys.argv[2]
sys.path.insert(0, REF)
import p3_engine_reference as pe  # noqa: E402
import p3_data_reference as pdr  # noqa: E402

pe.use_version("0.26.0")
E = 2000
report = {e["kind"]: e for e in pdr.parsed_spec("history")["report"]["events"]}
cat = {k["key"]: k for k in pe.COMPILED_026}
ev = []


def add(year, kind, region=0, settlement=0, parts=(), values=(), causes=()):
    eid = len(ev) + 1
    assert all(c < eid for c in causes), (kind, causes)
    k = cat[kind]
    assert len(values) == len(k["values"]), (kind, values)
    ev.append(dict(id=eid, year=year, kind=kind, region=region, settlement=settlement, parts=list(parts),
                   values=list(values), causes=sorted(causes)))
    return eid


AGES = [[1, 1, 0, 49, "The Age of Elves"], [2, 2, 50, 99, "The Age of Faeries"], [3, 3, 100, 199, "The Age of Dwarves"],
        [4, 4, 200, 299, "The Age of Goblins"], [5, 5, 300, 1999, "The Age of Humans"]]
PHASES = [[1, 1, 1, 0, 49, 0], [2, 2, 1, 50, 99, 0], [3, 3, 1, 100, 199, 0], [4, 4, 1, 200, 299, 0],
          [5, 5, 1, 300, 1199, 0], [6, 5, 2, 1200, 1299, 0], [7, 5, 4, 1300, 1999, 0]]


def phase_of(y):
    for p in PHASES:
        if p[3] <= y <= p[4]:
            return p[0]
    raise ValueError(y)


REGIONS = ["Aelin Vale", "Onsae Wood", "Kazad Fells", "Gor Heath", "Cear Downs", "Skree Moor", "Urarz Waste",
           "The Grey Fells", "Nub Marsh", "Ash Vale"]
# realms: (id, name, race, home region, founding year)
CIVS = [(1, "The House of Aelin", 1, 1, 0), (2, "The Court of Onsae", 2, 2, 50), (3, "The Hold of Durn", 3, 3, 100),
        (4, "The Gub Horde", 4, 4, 200), (5, "The Kingdom of Cear", 5, 5, 300), (6, "The Skreegg Horde", 4, 6, 210),
        (7, "The Urarz Horde", 4, 7, 220)]
aw, civf, stf = {}, {}, {}
for r, y, reg in ((1, 0, 1), (2, 50, 2), (3, 100, 3), (4, 200, 4), (5, 300, 5)):
    aw[r] = add(y, "awakening", region=reg, parts=[("race", r), ("creator", r)])
CAPS = {1: "Aelinost", 2: "Onsaeth", 3: "Kazad Rune", 4: "Gorzhak", 5: "Cearholm", 6: "Skreeggat", 7: "Urarzul"}
for cid, nm, race, reg, y in CIVS:
    civf[cid] = add(y, "civ_founded", region=reg, parts=[("civilization", cid), ("race", race)], causes=[aw[race]])
    stf[cid] = add(y, "settlement_founded", region=reg, settlement=cid, parts=[("settlement", cid), ("founder", cid)],
                   values=[500])
# two more settlements: Duldar (s8, the dwarves', in the Grey Fells), Nub Pit (s9, the Gub Horde's, Nub Marsh)
stf[8] = add(150, "settlement_founded", region=8, settlement=8, parts=[("settlement", 8), ("founder", 3)], values=[300])
stf[9] = add(250, "settlement_founded", region=9, settlement=9, parts=[("settlement", 9), ("founder", 4)], values=[300])

# 50: the origin alliance of the elves and the faeries (basis 0, the younger's founding)
al_origin = add(50, "alliance_formed", parts=[("ally_a", 1), ("ally_b", 2)], values=[0], causes=[civf[2]])
# 400-420: three trades
tr35 = add(400, "trade_opened", parts=[("partner_a", 3), ("partner_b", 5)], causes=[civf[5]])
tr15 = add(410, "trade_opened", parts=[("partner_a", 1), ("partner_b", 5)], causes=[civf[5]])
tr47 = add(420, "trade_opened", parts=[("partner_a", 4), ("partner_b", 7)], causes=[civf[7]])
# 450: the Hold opens the Mines of Kazad Rune (si1) at its capital; 451 sells arms to Cear; 455 a second mine (si2)
# at Duldar
mine1 = add(450, "mine_opened", region=3, settlement=3, parts=[("civilization", 3), ("site", 1)], causes=[stf[3]])
arms = add(451, "arms_sold", parts=[("seller", 3), ("buyer", 5), ("site", 1)], causes=[tr35, mine1])
mine2 = add(455, "mine_opened", region=8, settlement=8, parts=[("civilization", 3), ("site", 2)], causes=[stf[8]])
# 480: war A, the Gub Horde on Cear; the Hold and Cear ally against the Horde (basis 1); 490 Cear prevails
warA = add(480, "war_declared", region=5, parts=[("attacker", 4), ("defender", 5)], causes=[civf[5]])
al35 = add(480, "alliance_formed", parts=[("ally_a", 3), ("ally_b", 5), ("foe", 4)], values=[1], causes=[warA])
peaceA = add(490, "peace", parts=[("winner", 5), ("loser", 4)], values=[10], causes=[warA])
# 600: war B, the Gub Horde on the Hold over the Grey Fells; Cear comes to its defence; an allied battle; the Hold
# sacks Nub Pit and wins; tribute; the share of the spoils withheld
warB = add(600, "war_declared", region=8, parts=[("attacker", 4), ("defender", 3)], causes=[peaceA])
joinB = add(600, "ally_joined_war", parts=[("ally", 5), ("partner", 3), ("enemy", 4)], values=[0], causes=[warB, al35])
batB = add(601, "allied_battle", region=8, parts=[("winner", 3), ("loser", 4), ("winner_ally", 5)],
           values=[140, 812, 0], causes=[warB])
sgB = add(603, "siege", region=9, settlement=9, parts=[("attacker", 3), ("defender", 4)], values=[1], causes=[batB])
sackB = add(603, "sacked", region=9, settlement=9, parts=[("attacker", 3), ("defender", 4)], values=[300, 900],
            causes=[sgB])
wonB = add(605, "war_won", parts=[("winner", 3), ("loser", 4)], values=[5], causes=[warB])
tribB = add(605, "tribute", parts=[("payer", 4), ("payee", 3)], values=[200], causes=[wonB])
whB = add(605, "spoils_withheld", region=8, parts=[("principal", 3), ("ally", 5), ("enemy", 4)], values=[200],
          causes=[wonB, joinB])
# 620: war C, the Horde again; Cear joins again; an allied battle; the Hold wins; the share withheld again
warC = add(620, "war_declared", region=8, parts=[("attacker", 4), ("defender", 3)], causes=[wonB])
joinC = add(620, "ally_joined_war", parts=[("ally", 5), ("partner", 3), ("enemy", 4)], values=[0], causes=[warC, al35])
batC = add(622, "allied_battle", region=8, parts=[("winner", 3), ("loser", 4), ("winner_ally", 5)],
           values=[60, 500, 0], causes=[warC])
wonC = add(630, "war_won", parts=[("winner", 3), ("loser", 4)], values=[10], causes=[warC])
tribC = add(630, "tribute", parts=[("payer", 4), ("payee", 3)], values=[150], causes=[wonC])
whC = add(630, "spoils_withheld", region=8, parts=[("principal", 3), ("ally", 5), ("enemy", 4)], values=[60],
          causes=[wonC, joinC])
# 630: at the alliance's renewal (480 + 150), two shares withheld in its term: Cear spurns it (reason 0)
spurn = add(630, "alliance_spurned", region=8, parts=[("ally", 5), ("principal", 3)], values=[0], causes=[whC, al35])
# 650: the Skreegg and the Urarz Hordes ally in friendship
al67 = add(650, "alliance_formed", parts=[("ally_a", 6), ("ally_b", 7)], values=[3], causes=[civf[7]])
# 700: war D, the Skreegg Horde on the elves; the faeries come to their defence; an allied battle; the elves win and
# give the faeries their share
warD = add(700, "war_declared", region=1, parts=[("attacker", 6), ("defender", 1)], causes=[civf[6]])
joinD = add(700, "ally_joined_war", parts=[("ally", 2), ("partner", 1), ("enemy", 6)], values=[0],
            causes=[warD, al_origin])
batD = add(702, "allied_battle", region=1, parts=[("winner", 1), ("loser", 6), ("winner_ally", 2)], values=[3, 40, 0],
           causes=[warD])
wonD = add(710, "war_won", parts=[("winner", 1), ("loser", 6)], values=[10], causes=[warD])
tribD = add(710, "tribute", parts=[("payer", 6), ("payee", 1)], values=[100], causes=[wonD])
shD = add(710, "spoils_shared", region=1, parts=[("principal", 1), ("ally", 2), ("enemy", 6)], values=[10],
          causes=[wonD, joinD])
# 800: war E, Cear on the Skreegg Horde; the Urarz Horde refuses; the alliance is broken (refusals_to_break 1);
# 810 a truce
warE = add(800, "war_declared", region=6, parts=[("attacker", 5), ("defender", 6)], causes=[civf[6]])
refE = add(800, "ally_refused", parts=[("ally", 7), ("partner", 6), ("enemy", 5)], causes=[warE, al67])
brE = add(800, "alliance_broken", parts=[("ally_a", 6), ("ally_b", 7)], values=[0], causes=[refE])
truceE = add(810, "truce", parts=[("parties", 5), ("parties", 6)], values=[30], causes=[warE])
# 900: the Gub and the Urarz Hordes ally in fear of the Hold (basis 2, the Hold's latest gain of land)
al47 = add(900, "alliance_formed", parts=[("ally_a", 4), ("ally_b", 7), ("foe", 3)], values=[2], causes=[stf[8]])
# 1000: the Urarz Horde betrays the Gub Horde; their trade ends with the war; 1010 the Urarz Horde prevails
warF = add(1000, "war_declared", region=4, parts=[("attacker", 7), ("defender", 4)], causes=[al47])
betF = add(1000, "alliance_betrayed", parts=[("betrayer", 7), ("betrayed", 4)], causes=[warF, al47])
tbF = add(1000, "trade_broken", parts=[("partner_a", 4), ("partner_b", 7)], values=[0], causes=[warF])
peaceF = add(1010, "peace", parts=[("winner", 7), ("loser", 4)], values=[10], causes=[warF])
# 1100: the elves and Cear ally after long trade (basis 4); 1250 the alliance lapses (no cause left, quiet phase)
al15 = add(1100, "alliance_formed", parts=[("ally_a", 1), ("ally_b", 5)], values=[4], causes=[tr15])
lap15 = add(1250, "alliance_lapsed", parts=[("ally_a", 1), ("ally_b", 5)], values=[0], causes=[al15])
# 1300: the elves and the Hold ally in friendship; 1400 the bloc is too great: it lapses (reason 1)
al13 = add(1300, "alliance_formed", parts=[("ally_a", 1), ("ally_b", 3)], values=[3], causes=[civf[3]])
lap13 = add(1400, "alliance_lapsed", parts=[("ally_a", 1), ("ally_b", 3)], values=[1], causes=[al13])
# 1500: war G, the Skreegg Horde on the Hold; Duldar is taken (the second mine lost, ending 4); 1510 peace
warG = add(1500, "war_declared", region=8, parts=[("attacker", 6), ("defender", 3)], causes=[peaceA])
batG = add(1503, "battle", region=8, parts=[("winner", 6), ("loser", 3)], values=[20, 90], causes=[warG])
sgG = add(1505, "siege", region=8, settlement=8, parts=[("attacker", 6), ("defender", 3)], values=[1], causes=[batG])
capG = add(1505, "settlement_captured", region=8, settlement=8, parts=[("attacker", 6), ("defender", 3)],
           values=[30, 0], causes=[sgG])
peaceG = add(1510, "peace", parts=[("winner", 6), ("loser", 3)], values=[5], causes=[warG])

# event ids ascend with the year (P3-07 14.2): renumber by (year, order of writing)
_order = sorted(ev, key=lambda e: (e["year"], e["id"]))
_NEW = {e["id"]: i for i, e in enumerate(_order, 1)}
for e in _order:
    e["id"] = _NEW[e["id"]]
    e["causes"] = sorted(_NEW[c] for c in e["causes"])
    assert all(c < e["id"] for c in e["causes"])
ev[:] = _order
N = _NEW.get
civf = {k: N(v) for k, v in civf.items()}
stf = {k: N(v) for k, v in stf.items()}

races = [[1, "elf", "Elf", "Elves", "Elven", 0, 700, 150, 1, 1, 0, 1],
         [2, "faerie", "Faerie", "Faeries", "Faerie", 0, 500, 100, 2, 1, 50, 2],
         [3, "dwarf", "Dwarf", "Dwarves", "Dwarven", 0, 300, 40, 3, 1, 100, 3],
         [4, "goblin", "Goblin", "Goblins", "Goblin", 2, 60, 20, 4, 1, 200, 4],
         [5, "human", "Human", "Humans", "Human", 1, 80, 25, 5, 1, 300, 5]]
STL = {sid: (CAPS[sid], sid, sid, CIVS[sid - 1][4]) for sid in range(1, 8)}
STL[8] = ("Duldar", 3, 8, 150)
STL[9] = ("Nub Pit", 4, 9, 250)
stl_rows, census, sr = [], [], []
for sid in sorted(STL):
    nm, civ, reg, y = STL[sid]
    fe = stf[sid]
    if sid == 9:   # sacked in 603
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, 0, 1, 3, 0, 603, N(sackB)])
        census += [[sid, y, civ, 1, 1, 1200], [sid, 603, 0, 1, 3, 0]]
    elif sid == 8:   # taken by the Skreegg Horde in 1505
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, 6, 2, 1, 370, -1, 0])
        census += [[sid, y, civ, 2, 1, 400], [sid, 1505, 6, 2, 1, 370]]
    else:
        pop = {1: 2000, 2: 900, 3: 1500, 4: 1800, 5: 1600, 6: 1100, 7: 1000}[sid]
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, civ, 3, 1, pop, -1, 0])
        census.append([sid, y, civ, 3, 1, pop])
    sr.append([sid, CIVS[civ - 1][2], y, 1000, 3])
rc = [[r, CIVS[r - 1][4], -1, r, civf[r]] for r in range(1, 8)]
rc += [[8, 150, 1505, 3, stf[8]], [8, 1505, -1, 6, N(capG)], [9, 250, -1, 4, stf[9]]]
n = N
relations = [
    [3, 1, 3, 2, "alliance", 50, -1, n(al_origin), 0],
    [3, 3, 3, 5, "trade", 400, -1, n(tr35), 0],
    [3, 1, 3, 5, "trade", 410, -1, n(tr15), 0],
    [3, 4, 3, 7, "trade", 420, 1000, n(tr47), n(tbF)],
    [3, 4, 3, 5, "war", 480, 490, n(warA), n(peaceA)],
    [3, 3, 3, 5, "alliance", 480, 630, n(al35), n(spurn)],
    [3, 4, 3, 5, "peace", 490, 600, n(peaceA), n(joinB)],
    [3, 4, 3, 3, "war", 600, 605, n(warB), n(wonB)],
    [3, 5, 3, 4, "war", 600, 605, n(joinB), n(wonB)],
    [3, 3, 3, 4, "peace", 605, 620, n(wonB), n(warC)],
    [3, 4, 3, 5, "peace", 605, 620, n(wonB), n(joinC)],
    [3, 4, 3, 3, "war", 620, 630, n(warC), n(wonC)],
    [3, 5, 3, 4, "war", 620, 630, n(joinC), n(wonC)],
    [3, 3, 3, 4, "peace", 630, -1, n(wonC), 0],
    [3, 4, 3, 5, "peace", 630, -1, n(wonC), 0],
    [3, 6, 3, 7, "alliance", 650, 800, n(al67), n(brE)],
    [3, 6, 3, 1, "war", 700, 710, n(warD), n(wonD)],
    [3, 2, 3, 6, "war", 700, 710, n(joinD), n(wonD)],
    [3, 1, 3, 6, "peace", 710, -1, n(wonD), 0],
    [3, 2, 3, 6, "peace", 710, -1, n(wonD), 0],
    [3, 5, 3, 6, "war", 800, 810, n(warE), n(truceE)],
    [3, 5, 3, 6, "peace", 810, -1, n(truceE), 0],
    [3, 4, 3, 7, "alliance", 900, 1000, n(al47), n(betF)],
    [3, 7, 3, 4, "war", 1000, 1010, n(warF), n(peaceF)],
    [3, 4, 3, 7, "peace", 1010, -1, n(peaceF), 0],
    [3, 1, 3, 5, "alliance", 1100, 1250, n(al15), n(lap15)],
    [3, 1, 3, 3, "alliance", 1300, 1400, n(al13), n(lap13)],
    [3, 6, 3, 3, "war", 1500, 1510, n(warG), n(peaceG)],
    [3, 3, 3, 6, "peace", 1510, -1, n(peaceG), 0],
]
sites = [[1, "The Mines of Kazad Rune", 3, 3, 30, 30, 3, 3, 0, 450, n(mine1), -1, 0, 0],
         [2, "The Delvings of Duldar", 3, 8, 80, 80, 8, 3, 0, 455, n(mine2), 1505, n(capG), 4]]
KINDS = ["awakening", "civ_founded", "settlement_founded", "war_declared", "battle", "siege", "sacked",
         "settlement_captured", "peace", "truce", "war_won", "tribute"] + [k["key"] for k in pe.KINDS_026]


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


o = ["""# P5-04 18.1 item 16: H16 (format v7, schema 11). Five races (elf 1, faerie 2, dwarf 3, goblin 4, human 5), seven
# realms. Origin alliance c1-c2 (e%d, standing at E); foe alliance c3-c5 against c4 (e%d) with two joins in defence
# (e%d, e%d), two allied battles (e%d, e%d), two shares withheld (e%d, e%d) and a spurning at its renewal (e%d,
# reason 0); a share granted to the faeries (e%d); a refusal (e%d) that breaks c6-c7 (e%d); a betrayal (e%d) that
# breaks the trade c4-c7 (e%d); a lapse (e%d) and a bloc lapse (e%d); trades c3-c5, c1-c5, c4-c7; mines si1 (open)
# and si2 (lost with Duldar, e%d); arms sold (e%d). Joined war rows: (c5, c4) twice, (c2, c6).
# Written by the test writer from the spec (P5-04 10); generated by p5_04_h16_fixture.py from the 0.26.0 listings
# of p3_data_reference.py; format: README.md in this directory.
""" % tuple(n(x) for x in (al_origin, al35, joinB, joinC, batB, batC, whB, whC, spurn, shD, refE, brE, betF, tbF,
                            lap15, lap13, capG, arms))]
o.append("""[meta]
seed = 1616
seed_phrase = ""
map_size = 512
history_span = 2000
history_detail = "normal"
races = "elf,faerie,dwarf,goblin,human"
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
o.append('culture = [[1, "elven", "Elven", 1], [2, "faerie", "Faerie", 2], [3, "dwarven", "Dwarven", 3], '
         '[4, "goblin", "Goblin", 4], [5, "human", "Human", 5]]\n')
o.append('god = [[1, "Aelor", 0], [2, "Onsa", 0], [3, "Durin", 1], [4, "Gruzz", 2], [5, "Wealstuld", 1]]\n')
o.append(rows("age", AGES))
o.append(rows("phase", PHASES))
o.append(rows("civilization", [[cid, nm, race, race, reg, cid, y, civf[cid], -1, 0] for cid, nm, race, reg, y in CIVS]))
o.append("# Fourteen columns: settlement.ending is derived from the log (P5-02 8.3).\n")
o.append(rows("settlement", stl_rows))
o.append(rows("settlement_census", sorted(census)))
o.append(rows("region_control", sorted(rc)))
o.append(rows("relation", sorted(relations, key=lambda r: (r[0], r[1], r[2], r[3], r[4], r[5]))))
o.append("region_name = []\n")
o.append("beast = []\nmonster_kind = []\nbeast_population = []\nmonster = []\n")
o.append(rows("site", sites))
o.append("race_relation = []\n")
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
                                                                             phase_of(e["year"])))
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
