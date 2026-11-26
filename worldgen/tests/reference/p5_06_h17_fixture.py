#!/usr/bin/env python3
"""Writes worldgen/tests/data/history-report/h17-figures.toml (P5-06 18.1 item 14) from the 0.27.0 listings.

  python3 -B worldgen/tests/reference/p5_06_h17_fixture.py worldgen/tests/reference \
      worldgen/tests/data/history-report/h17-figures.toml

H17 (format v8, schema 12) is H16 (below, unchanged but for the peace row of c5 and c6 that war H closes) with the
figures of P5-06 18.1 item 14 added: a founder (Aldric, came_to_rule); rulers named by deed (Aldric II by a
declaration, Dain by spoils withheld, Grishnak by a declaration and a betrayal, Cenric and Harold by declarations);
a split (the Kingdom of Cear's crisis of 820: Osric against Edric, who founds the March of Edric) with rivals and two
parent rows; an exile with a host (Borin to the House of Aelin) and one to the wilds (Ilythe); two generals (Garth
falls in the battle of 1572; Ulf is re-appointed in war H); a hero who slays a troll (Kili Trollbane) and one slain
by a dragon (Faelan); a caster with a library, two books and a blight (Iensath the Accursed); a realm conquered with
its last ruler (the March of Edric, Godric the Last); a human line with Aldric and Aldric II; the epithet the Old by
a substantial reign borne by a third and a fourth figure (Harold, Thrain) after two others (Edric, Osric); rulers
at the end by reigned_at_end (Wulfric, Uglak) and by a dispute (Thrain, Elarion); realms with no recorded ruler;
the epithets as 6.10 gives them at E (invariants 2 and 3 do not hold: some realms and deeds have no ruler).
Written by the test writer from the spec (P5-06 9, 10, 18.1 item 14); the H16 docstring follows.

---

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

pe.use_version("0.27.0")
E = 2000
report = {e["kind"]: e for e in pdr.parsed_spec("history")["report"]["events"]}
cat = {k["key"]: k for k in pe.COMPILED_027}
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
           "The Grey Fells", "Nub Marsh", "Ash Vale", "Edric March", "Skarr Crags", "The Cold Peaks"]
# realms: (id, name, race, home region, founding year)
CIVS = [(1, "The House of Aelin", 1, 1, 0), (2, "The Court of Onsae", 2, 2, 50), (3, "The Hold of Durn", 3, 3, 100),
        (4, "The Gub Horde", 4, 4, 200), (5, "The Kingdom of Cear", 5, 5, 300), (6, "The Skreegg Horde", 4, 6, 210),
        (7, "The Urarz Horde", 4, 7, 220)]
CIV8 = (8, "The March of Edric", 5, 11, 820)   # P5-06: seceded from Cear in 820, conquered by it in 1575
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

# ----- P5-06 (H17): figures -----
FIG = []   # figure rows as dicts, ids in creation order


def fig(name, role, civ, race, born, office, start, reign=0, regnal=0, temper=0, skill=0, epithet=""):
    FIG.append(dict(id=len(FIG) + 1, name=name, epithet=epithet, role=role, race=race, culture=race, civ=civ,
                    born=born, office=office, start=start, died=None, death=0, dregion=0, dsettl=0, status=1,
                    reign=reign, regnal=regnal, temper=temper, skill=skill))
    return FIG[-1]


def dies(f, year, event, region, settlement=0, status=3):
    f.update(died=year, death=event, dregion=region, dsettl=settlement, status=status)


REL = []   # figure relation rows (a_type, a, b_type, b, kind, since, until or -1, since_event, until_event)
CAPREG = {1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 6, 7: 7, 8: 11}
# the Kingdom of Cear (human: generation 25): Aldric the Founder (reign 1)
came5 = add(300, "came_to_rule", region=5, parts=[("figure", 1), ("civilization", 5)], causes=[civf[5]])
aldric = fig("Aldric", 1, 5, 5, 300, 300, came5, reign=1, epithet="the Founder")
d_aldric = add(380, "figure_died", region=5, settlement=5, parts=[("figure", 1), ("civilization", 5)], values=[0, 80],
               causes=[came5])
dies(aldric, 380, d_aldric, 5, 5)
REL.append([6, 1, 3, 5, "ruled", 300, 380, came5, d_aldric])
# the Hold of Durn (dwarf: generation 40): Dain the Victorious, named by the spoils withheld of 605 (reign 13);
# wars B and C won in his reign
dain = fig("Dain", 1, 3, 3, 520, 590, whB, reign=13, temper=3, epithet="the Victorious")
d_dain = add(640, "figure_died", region=3, settlement=3, parts=[("figure", dain["id"]), ("civilization", 3)],
             values=[0, 120], causes=[whB])
dies(dain, 640, d_dain, 3, 3)
REL.append([6, dain["id"], 3, 3, "ruled", 605, 640, whB, d_dain])
# Aldric II, named by war E (800, reign 4); the crisis of 820 ends his reign; the split: Osric (reign 5) against
# Edric, the first ruler of the March of Edric (c8)
aldric2 = fig("Aldric", 1, 5, 5, 765, 798, warE, reign=4, regnal=2, temper=1)
stf[10] = add(700, "settlement_founded", region=11, settlement=10, parts=[("settlement", 10), ("founder", 5)],
              values=[400])
crisis5 = add(820, "succession_crisis", region=5, settlement=5, parts=[("civilization", 5)])
civf[8] = add(820, "civ_seceded", region=11, settlement=10, parts=[("civilization", 8), ("from", 5)], values=[1],
              causes=[crisis5])
d_aldric2 = add(820, "figure_died", region=5, settlement=5, parts=[("figure", aldric2["id"]), ("civilization", 5)],
                values=[1, 55], causes=[warE])
dies(aldric2, 820, d_aldric2, 5, 5)
REL.append([6, aldric2["id"], 3, 5, "ruled", 800, 820, warE, d_aldric2])
osric = fig("Osric", 1, 5, 5, 795, 820, 0, reign=5, temper=4, epithet="the Old")
edric = fig("Edric", 1, 8, 5, 795, 820, 0, reign=1, temper=1, epithet="the Old")
sd5 = add(820, "succession_disputed", region=5, settlement=5,
          parts=[("civilization", 5), ("ruler", osric["id"]), ("claimant", edric["id"]), ("new_realm", 8)],
          values=[1], causes=[crisis5, civf[8]])
osric["start"] = edric["start"] = sd5
d_edric = add(870, "figure_died", region=11, settlement=10, parts=[("figure", edric["id"]), ("civilization", 8)],
              values=[0, 75], causes=[sd5])
dies(edric, 870, d_edric, 11, 10)
d_osric = add(900, "figure_died", region=5, settlement=5, parts=[("figure", osric["id"]), ("civilization", 5)],
              values=[0, 105], causes=[sd5])
dies(osric, 900, d_osric, 5, 5)
REL += [[6, osric["id"], 3, 5, "ruled", 820, 900, sd5, d_osric], [6, edric["id"], 3, 8, "ruled", 820, 870, sd5, d_edric],
        [6, osric["id"], 6, edric["id"], "rival", 820, 870, sd5, d_edric],
        [6, aldric2["id"], 6, osric["id"], "parent", 820, -1, sd5, 0],
        [6, aldric2["id"], 6, edric["id"], "parent", 820, -1, sd5, 0]]
# the Urarz Horde (goblin: generation 20): Grishnak the Faithless, named by war F (1000, reign 40), who also betrayed
grishnak = fig("Grishnak", 1, 7, 4, 970, 990, warF, reign=40, temper=3, epithet="the Faithless")
d_grishnak = add(1030, "figure_died", region=7, settlement=7, parts=[("figure", grishnak["id"]), ("civilization", 7)],
                 values=[0, 60], causes=[warF])
dies(grishnak, 1030, d_grishnak, 7, 7)
REL.append([6, grishnak["id"], 3, 7, "ruled", 1000, 1030, warF, d_grishnak])
# monsters: a troll (m1, lair si3 in the Skarr Crags) and a dragon (m2, lair si4 in the Cold Peaks)
app1 = add(1300, "monster_appeared", region=12, parts=[("monster", 1), ("lair", 3)])
app2 = add(1310, "monster_appeared", region=13, parts=[("monster", 2), ("lair", 4)])
raid1 = add(1320, "monster_raid", region=3, settlement=3, parts=[("monster", 1), ("civilization", 3)], values=[20, 0],
            causes=[app1])
kili = fig("Kili", 4, 3, 3, 1290, 1320, 0, epithet="Trollbane")
hr1 = add(1320, "hero_rose", region=3, settlement=3, parts=[("figure", kili["id"]), ("civilization", 3), ("monster", 1)],
          causes=[raid1])
kili["start"] = hr1
raid2 = add(1330, "monster_raid", region=1, settlement=1, parts=[("monster", 2), ("civilization", 1)], values=[40, 0],
            causes=[app2])
faelan = fig("Faelan", 4, 1, 1, 1200, 1330, 0)
hr2 = add(1330, "hero_rose", region=1, settlement=1, parts=[("figure", faelan["id"]), ("civilization", 1), ("monster", 2)],
          causes=[raid2])
faelan["start"] = hr2
hs2 = add(1340, "hero_slain", region=13, parts=[("figure", faelan["id"]), ("civilization", 1), ("monster", 2)],
          causes=[raid2])
dies(faelan, 1340, hs2, 13, 0, status=5)
slain1 = add(1350, "monster_slain_by_hero", region=12, parts=[("monster", 1), ("slayer", 3), ("hero", kili["id"])],
             values=[3], causes=[raid1])
# the caster: Iensath the Accursed of the House of Aelin, a library at Aelinost (si5), two books and a blight
iensath = fig("Iensath", 5, 1, 1, 1100, 1360, 0, epithet="the Accursed")
tf1 = add(1360, "tower_founded", region=1, settlement=1, parts=[("figure", iensath["id"]), ("civilization", 1),
                                                                 ("site", 5)], values=[1], causes=[stf[1]])
iensath["start"] = tf1
bw1 = add(1370, "book_written", region=1, parts=[("figure", iensath["id"]), ("civilization", 1), ("site", 5)],
          causes=[tf1])
bw2 = add(1380, "book_written", region=1, parts=[("figure", iensath["id"]), ("civilization", 1), ("site", 5)],
          causes=[tf1, bw1])
blight1 = add(1390, "blight", region=1, parts=[("figure", iensath["id"]), ("civilization", 1), ("site", 5)], values=[60],
              causes=[tf1])
d_kili = add(1500, "figure_died", region=3, parts=[("figure", kili["id"]), ("civilization", 3)], values=[0, 210],
             causes=[hr1])
dies(kili, 1500, d_kili, 3, 0)
# war I (1570): Cear under Cenric the Conqueror (reign 9) on the March of Edric; two generals; Garth falls in the battle of 1572;
# the March's last ruler Godric (reign 20) falls with it in 1575
warI = add(1570, "war_declared", region=11, parts=[("attacker", 5), ("defender", 8)], causes=[civf[8]])
cenric = fig("Cenric", 1, 5, 5, 1535, 1560, warI, reign=9, temper=0, epithet="the Conqueror")
ulf = fig("Ulf", 3, 5, 5, 1545, 1570, 0, skill=650)
ga_ulf = add(1570, "general_appointed", region=11, parts=[("figure", ulf["id"]), ("civilization", 5), ("enemy", 8)],
             values=[0], causes=[warI])
ulf["start"] = ga_ulf
garth = fig("Garth", 3, 8, 5, 1540, 1570, 0, skill=520)
ga_garth = add(1570, "general_appointed", region=11, parts=[("figure", garth["id"]), ("civilization", 8), ("enemy", 5)],
               values=[0], causes=[warI])
garth["start"] = ga_garth
blI = add(1572, "battle_led", region=11, parts=[("winner", 5), ("loser", 8), ("winner_general", ulf["id"]),
                                                ("loser_general", garth["id"])], values=[50, 300, 0], causes=[warI])
fall_garth = add(1572, "fell_in_battle", region=11, parts=[("figure", garth["id"]), ("civilization", 8), ("enemy", 5)],
                 causes=[blI])
dies(garth, 1572, fall_garth, 11, 0, status=4)
capI = add(1575, "settlement_captured", region=11, settlement=10, parts=[("attacker", 5), ("defender", 8)],
           values=[40, 0], causes=[blI])
conqI = add(1575, "civ_conquered", region=11, parts=[("civilization", 8), ("conqueror", 5)], causes=[capI])
godric = fig("Godric", 1, 8, 5, 1555, 1570, conqI, reign=20, temper=0, epithet="the Last")
dies(godric, 1575, conqI, 11, 0, status=6)
REL += [[6, cenric["id"], 3, 5, "ruled", 1570, 1610, warI, 0], [6, godric["id"], 3, 8, "ruled", 1575, 1575, conqI, conqI],
        [6, ulf["id"], 3, 5, "served", 1570, 1575, ga_ulf, conqI],
        [6, garth["id"], 3, 8, "served", 1570, 1572, ga_garth, fall_garth]]
d_cenric = add(1610, "figure_died", region=5, settlement=5, parts=[("figure", cenric["id"]), ("civilization", 5)],
               values=[0, 75], causes=[warI])
dies(cenric, 1610, d_cenric, 5, 5)
REL[-4][8] = d_cenric
# war H (1620): Cear under Harold (reign 11) on the Skreegg Horde; Ulf takes command again; the Horde wins in 1630
warH = add(1620, "war_declared", region=6, parts=[("attacker", 5), ("defender", 6)], causes=[truceE])
harold = fig("Harold", 1, 5, 5, 1590, 1615, warH, reign=11, temper=1, epithet="the Old")
ga_ulf2 = add(1620, "general_appointed", region=6, parts=[("figure", ulf["id"]), ("civilization", 5), ("enemy", 6)],
              values=[1], causes=[warH])
blH = add(1625, "battle_led", region=6, parts=[("winner", 6), ("loser", 5), ("loser_general", ulf["id"])],
          values=[40, 120, 0], causes=[warH])
peaceH = add(1630, "peace", parts=[("winner", 6), ("loser", 5)], values=[10], causes=[warH])
REL.append([6, ulf["id"], 3, 5, "served", 1620, 1630, ga_ulf2, peaceH])
d_ulf = add(1650, "figure_died", region=5, parts=[("figure", ulf["id"]), ("civilization", 5)], values=[0, 105],
            causes=[ga_ulf])
dies(ulf, 1650, d_ulf, 5, 0)
d_harold = add(1660, "figure_died", region=5, settlement=5, parts=[("figure", harold["id"]), ("civilization", 5)],
               values=[0, 70], causes=[warH])
dies(harold, 1660, d_harold, 5, 5)
REL.append([6, harold["id"], 3, 5, "ruled", 1620, 1660, warH, d_harold])
# the Hold's crisis of 1850: Thrain the Old (reign 52) against Borin the Exile, who goes into exile with the elves
crisis3 = add(1850, "succession_crisis", region=3, settlement=3, parts=[("civilization", 3)])
thrain = fig("Thrain", 1, 3, 3, 1780, 1850, 0, reign=52, temper=0, epithet="the Old")
borin = fig("Borin", 2, 3, 3, 1800, 1850, 0, epithet="the Exile")
sd3 = add(1850, "succession_disputed", region=3, settlement=3,
          parts=[("civilization", 3), ("ruler", thrain["id"]), ("claimant", borin["id"])], values=[0], causes=[crisis3])
thrain["start"] = borin["start"] = sd3
ex3 = add(1850, "exiled", region=3, parts=[("figure", borin["id"]), ("civilization", 3), ("host", 1)], values=[1],
          causes=[sd3])
borin["status"] = 2
# the House of Aelin's crisis of 1900: Elarion (reign 14) against Ilythe the Exile, who flees into the wilds
crisis1 = add(1900, "succession_crisis", region=1, settlement=1, parts=[("civilization", 1)])
elarion = fig("Elarion", 1, 1, 1, 1600, 1900, 0, reign=14, temper=2)
ilythe = fig("Ilythe", 2, 1, 1, 1650, 1900, 0, epithet="the Exile")
sd1 = add(1900, "succession_disputed", region=1, settlement=1,
          parts=[("civilization", 1), ("ruler", elarion["id"]), ("claimant", ilythe["id"])], values=[0], causes=[crisis1])
elarion["start"] = ilythe["start"] = sd1
ex1 = add(1900, "exiled", region=1, parts=[("figure", ilythe["id"]), ("civilization", 1)], values=[0], causes=[sd1])
ilythe["status"] = 2
REL += [[6, thrain["id"], 3, 3, "ruled", 1850, -1, sd3, 0], [6, thrain["id"], 6, borin["id"], "rival", 1850, -1, sd3, 0],
        [6, elarion["id"], 3, 1, "ruled", 1900, -1, sd1, 0], [6, elarion["id"], 6, ilythe["id"], "rival", 1900, -1, sd1, 0]]
# the present (1999): Wulfric of Cear (reign 13), Uglak of the Urarz Horde (reign 90)
wulfric = fig("Wulfric", 1, 5, 5, 1955, 1980, 0, reign=13, temper=0)
rae5 = add(1999, "reigned_at_end", region=5, parts=[("figure", wulfric["id"]), ("civilization", 5)])
wulfric["start"] = rae5
uglak = fig("Uglak", 1, 7, 4, 1975, 1990, 0, reign=90, temper=3)
rae7 = add(1999, "reigned_at_end", region=7, parts=[("figure", uglak["id"]), ("civilization", 7)])
uglak["start"] = rae7
REL += [[6, wulfric["id"], 3, 5, "ruled", 1999, -1, rae5, 0], [6, uglak["id"], 3, 7, "ruled", 1999, -1, rae7, 0]]
BOOKS = [[1, "The Lay of the First Stars", iensath["id"], 5, 1370, bw1], [2, "On the Speech of Trees", iensath["id"], 5,
                                                                           1380, bw2]]

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
for f in FIG:
    f["start"], f["death"] = N(f["start"]), N(f["death"]) or 0
for r in REL:
    r[7], r[8] = N(r[7]), N(r[8]) or 0
for b in BOOKS:
    b[5] = N(b[5])

races = [[1, "elf", "Elf", "Elves", "Elven", 0, 700, 150, 1, 1, 0, 1],
         [2, "faerie", "Faerie", "Faeries", "Faerie", 0, 500, 100, 2, 1, 50, 2],
         [3, "dwarf", "Dwarf", "Dwarves", "Dwarven", 0, 300, 40, 3, 1, 100, 3],
         [4, "goblin", "Goblin", "Goblins", "Goblin", 2, 60, 20, 4, 1, 200, 4],
         [5, "human", "Human", "Humans", "Human", 1, 80, 25, 5, 1, 300, 5]]
STL = {sid: (CAPS[sid], sid, sid, CIVS[sid - 1][4]) for sid in range(1, 8)}
STL[8] = ("Duldar", 3, 8, 150)
STL[9] = ("Nub Pit", 4, 9, 250)
STL[10] = ("Edricsford", 5, 11, 700)
stl_rows, census, sr = [], [], []
for sid in sorted(STL):
    nm, civ, reg, y = STL[sid]
    fe = stf[sid]
    if sid == 9:   # sacked in 603
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, 0, 1, 3, 0, 603, N(sackB)])
        census += [[sid, y, civ, 1, 1, 1200], [sid, 603, 0, 1, 3, 0]]
    elif sid == 10:   # Cear's, the March of Edric's from 820, Cear's again from 1575
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, 5, 2, 1, 500, -1, 0])
        census += [[sid, y, 5, 1, 1, 400], [sid, 820, 8, 2, 1, 600], [sid, 1575, 5, 2, 1, 500]]
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
rc += [[11, 700, 820, 5, stf[10]], [11, 820, 1575, 8, civf[8]], [11, 1575, -1, 5, N(capI)]]
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
    [3, 5, 3, 6, "peace", 810, 1620, n(truceE), n(warH)],
    [3, 5, 3, 6, "war", 1620, 1630, n(warH), n(peaceH)],
    [3, 5, 3, 6, "peace", 1630, -1, n(peaceH), 0],
    [3, 5, 3, 8, "war", 1570, 1575, n(warI), n(conqI)],
    [3, 4, 3, 7, "alliance", 900, 1000, n(al47), n(betF)],
    [3, 7, 3, 4, "war", 1000, 1010, n(warF), n(peaceF)],
    [3, 4, 3, 7, "peace", 1010, -1, n(peaceF), 0],
    [3, 1, 3, 5, "alliance", 1100, 1250, n(al15), n(lap15)],
    [3, 1, 3, 3, "alliance", 1300, 1400, n(al13), n(lap13)],
    [3, 6, 3, 3, "war", 1500, 1510, n(warG), n(peaceG)],
    [3, 3, 3, 6, "peace", 1510, -1, n(peaceG), 0],
]
sites = [[1, "The Mines of Kazad Rune", 3, 3, 30, 30, 3, 3, 0, 450, n(mine1), -1, 0, 0],
         [2, "The Delvings of Duldar", 3, 8, 80, 80, 8, 3, 0, 455, n(mine2), 1505, n(capG), 4],
         [3, "The Troll-Holes of Skarr", 1, 12, 120, 120, 0, 0, 1, 1300, n(app1), 1350, n(slain1), 1],
         [4, "The Wyrm-Hall of the Cold Peaks", 1, 13, 130, 130, 0, 0, 2, 1310, n(app2), -1, 0, 0],
         [5, "The Leaf-House of Iensath", 4, 1, 10, 10, 1, 1, 0, 1360, n(tf1), -1, 0, 0]]
monster_kinds = [[1, "troll", "Troll", "a troll of the hills", 0, 1, 400, "fell upon"],
                 [2, "dragon", "Dragon", "a dragon of the old world", 0, 1, 6000, "burned the fields of"]]
monsters = [[1, "Skarr", 1, 3, 1300, n(app1), 1350, n(slain1), 3, 3, 0],
            [2, "Saelith", 2, 4, 1310, n(app2), -1, 0, 0, 1, 500]]
KINDS = ["awakening", "civ_founded", "settlement_founded", "war_declared", "battle", "siege", "sacked",
         "settlement_captured", "peace", "truce", "war_won", "tribute", "succession_crisis", "civ_seceded",
         "civ_conquered", "monster_appeared", "monster_raid"] + [k["key"] for k in pe.KINDS_026] + [
         k["key"] for k in pe.KINDS_027]


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


o = ["""# P5-06 18.1 item 14: H17 (format v8, schema 12): H16 with the figures of P5-06 (rulers, a split, exiles, generals,
# heroes, a caster with a library, two books and a blight, a conquered realm, Aldric II, the Old by a third bearer);
# generated by p5_06_h17_fixture.py, whose docstring says what each figure event is. H16's comment follows.
# P5-04 18.1 item 16: H16 (format v7, schema 11). Five races (elf 1, faerie 2, dwarf 3, goblin 4, human 5), seven
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
o.append(rows("civilization", [[cid, nm, race, race, reg, cid, y, civf[cid], -1, 0] for cid, nm, race, reg, y in CIVS]
                 + [[8, CIV8[1], 5, 5, 11, 10, 820, civf[8], 1575, N(conqI)]]))
o.append("# Fourteen columns: settlement.ending is derived from the log (P5-02 8.3).\n")
o.append(rows("settlement", stl_rows))
o.append(rows("settlement_census", sorted(census)))
o.append(rows("region_control", sorted(rc)))
o.append(rows("relation", sorted(relations + REL, key=lambda r: (r[0], r[1], r[2], r[3], r[4], r[5]))))
o.append("region_name = []\n")
o.append("beast = []\n")
o.append(rows("monster_kind", monster_kinds))
o.append("beast_population = []\n")
o.append(rows("monster", monsters))
o.append(rows("site", sites))
o.append("race_relation = []\n")
o.append(rows("settlement_race", sorted(sr)))
o.append("# P5-06 9.1, 9.3: figure (died -1 is NULL) and book\n")
o.append(rows("figure", [[f["id"], f["name"], f["epithet"], f["role"], f["race"], f["culture"], f["civ"], f["born"],
                          f["office"], f["start"], f["died"] if f["died"] is not None else -1, f["death"], f["dregion"],
                          f["dsettl"], f["status"], f["reign"], f["regnal"], f["temper"], f["skill"]] for f in FIG]))
o.append(rows("book", BOOKS))
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
