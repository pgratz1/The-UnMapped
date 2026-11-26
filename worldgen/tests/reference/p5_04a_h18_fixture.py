#!/usr/bin/env python3
"""Writes worldgen/tests/data/history-report/h18-v9.toml (P5-04a 16 item 12) and prints the report lines the
reference gives for it (`--lines`).

  python3 -B worldgen/tests/reference/p5_04a_h18_fixture.py worldgen/tests/reference \\
      worldgen/tests/data/history-report/h18-v9.toml [--lines]

H18 (format v9, schema 12; the spec's "H17", named H18 since P5-06 holds H17: reading R10 of
p5_04_alliances_reference.py), from H16's shape: five races, seven realms, the 0.28.0 catalogue (the ten value rows of
14.4) and the 0.28.0 templates of 14.3 written from the spec (3.6, 4.4, 5, 6.2, 7.1, 9.1, 9.2, 10 items 3 to 5,
21.5); the other kinds keep the 0.27.0 listings. What it holds:
  - a soured alliance renewed: c3-c5 (against c4, 480) sours at the share kept in 605 (grievance 1) and renews in
    630, a notable renewal (the war they fought together); then two quiet renewals (780, 930: no event) and it stands
    to E (renewed 10);
  - a spurning at renewal: c1-c5 (after trade, 1100) sours in 1160 and is spurned at its renewal in 1250 (reason 0,
    years 90);
  - a spurning at once: c4-c7 (in fear of c3, 900): a large share (650) kept in 1310, spurned that year (reason 2,
    years 0);
  - an excused refusal (c3 for c5 in 1000, motive 2: a war of its own) and two counted refusals of c7 for c6 (800
    motive 3 count 1, 820 motive 4 count 2) with the break (refusals_to_break 2);
  - a betrayal with motive 2 (c6 of c4 in 1600, an alliance of 200 years);
  - an alliance across hatred (c1-c4, 700, band 0) that lapses at its first due year (850);
  - a thaw (elves and goblins, 1000) and a decline (elves and dwarves, 1200);
  - four same-day settlement_mixed events in two groups (1500: Aelinost and Elmere; Kazad Rune and Kazad Low);
  - the first battle the arms buyer c5 wins after the sale (802) cites the arms_sold; its later win (1152) does not;
  - a realm (c3) with five or more milestones of salience >= 40 and a quiet stretch;
  - a trade (c5-c7) ended by c7's conquest (1800): `; ended e<id>`.
Written by the test writer from the spec; format: README.md in that directory.
"""
import os
import sys

REF = sys.argv[1]
OUT = sys.argv[2]
sys.path.insert(0, REF)
import p3_engine_reference as pe  # noqa: E402
import p3_data_reference as pdr  # noqa: E402

pe.use_version("0.27.0")
report = {e["kind"]: dict(e) for e in pdr.parsed_spec("history")["report"]["events"]}
pe.use_version("0.28.0")
cat = {k["key"]: k for k in pe.COMPILED_028}
E = 2000

# 14.3: the templates of 0.28.0 (from the spec's sections; cause words as P5-03a 3.5 and 10 item 3)
ARMS = "{@causes:arms_sold=new-bought arms|With |, }"
T = {
    "spoils_withheld": "{principal}{principal:under} kept the spoils of {@region}, {share?no captives|one captive|# "
                       "captives}, from {ally}, its ally{grievance?|, which did not forget it|, which would not "
                       "forgive it}.",
    "alliance_spurned": "{reason?Still bitter|Long resentful|Outraged} that the spoils of {@region} had been kept from "
                        "it{years?| the year before| # years before}, {ally} broke its alliance with {principal}.",
    "ally_refused": "{motive?Loath to send its people to war|Weary of war|With a war of its own to fight|Fearing the "
                    "strength of the foe|Grown cold toward its ally|Grasping for its own gain|Nursing a claim on its "
                    "ally's land|Remembering the spoils kept from it}, {ally}{ally:under} would not take up arms for "
                    "{partner} against {enemy}{count?|, and its ally did not forget it|}.",
    "alliance_betrayed": "{motive?Grasping for more|Its friendship long cooled|Seeing its ally weak|Coveting its ally's "
                         "land}, {betrayer}{betrayer:under} broke an alliance of {years} {years?year|years} with "
                         "{betrayed}.",
    "alliance_renewed": "{@causes:ally_joined_war=the war they fought together,spoils_shared=the spoils they shared,"
                        "war_declared=the outbreak of a new war,trade_opened=long years of trade,disposition_turned="
                        "the friendship of their peoples,civ_conquered=the fall of a realm,settlement_captured=the "
                        "conquests of their foe,sacked=the sacks of their foe,region_ceded=the conquests of their foe,"
                        "settlements_ceded=the conquests of their foe|After |, }{ally_a} and {ally_b} renewed their "
                        "alliance{foe:against}.",
    "spoils_shared": "{principal}{principal:under} gave {ally} its share of the spoils of {@region}, {share?no "
                     "captives|one captive|# captives}.",
    "monster_died": "{monster}{monster:aside} {monster:died}{monster:where}.",
    "hero_rose": "{quiet?After the raids of |After the raids of |When, after # quiet years, }{monster}{quiet?|| raided "
                 "again}, {figure} of {civilization} took up the hunt.",
}
_dt = report["disposition_turned"]["template"]
T["disposition_turned"] = _dt[:_dt.index("{band?")] + ("{change?came to hate one another|put aside their hatred of "
                                                       "one another|grew wary of one another|became friends}.")
_af = report["alliance_formed"]["template"]
for old, new in (("settlement_captured=the fall of a neighbour", "settlement_captured=the conquests of their foe"),
                 ("sacked=the sack of a neighbour", "sacked=the sacks of their foe"),
                 ("region_ceded=the loss of a neighbour's land", "region_ceded=the conquests of their foe"),
                 ("settlements_ceded=the loss of a neighbour's land", "settlements_ceded=the conquests of their foe")):
    assert old in _af, old
    _af = _af.replace(old, new)
assert _af.endswith("{foe:against}.")
T["alliance_formed"] = _af[:-1] + "{band?, though their peoples hated one another||}."
for k in ("battle", "battle_at_pass", "allied_battle", "battle_led"):
    T[k] = ARMS + report[k]["template"]
for k, t in T.items():
    report[k]["template"] = t
    p = pe.template_problem(dict(cat[k], templ=t), t)
    assert p is None, (k, p)

ev = []


def add(year, kind, region=0, settlement=0, parts=(), values=(), causes=(), end=None):
    eid = len(ev) + 1
    assert all(c < eid for c in causes), (kind, causes)
    k = cat[kind]
    assert len(values) == len(k["values"]), (kind, values, k["values"])
    ev.append(dict(id=eid, year=year, kind=kind, region=region, settlement=settlement, parts=list(parts),
                   values=list(values), causes=sorted(set(causes)), end=end))
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
CIVS = [(1, "The House of Aelin", 1, 1, 0), (2, "The Court of Onsae", 2, 2, 50), (3, "The Hold of Durn", 3, 3, 100),
        (4, "The Gub Horde", 4, 4, 200), (5, "The Kingdom of Cear", 5, 5, 300), (6, "The Skreegg Horde", 4, 6, 210),
        (7, "The Urarz Horde", 4, 7, 220)]
CAPS = {1: "Aelinost", 2: "Onsaeth", 3: "Kazad Rune", 4: "Gorzhak", 5: "Cearholm", 6: "Skreeggat", 7: "Urarzul"}
aw, civf, stf = {}, {}, {}
for r, y, reg in ((1, 0, 1), (2, 50, 2), (3, 100, 3), (4, 200, 4), (5, 300, 5)):
    aw[r] = add(y, "awakening", region=reg, parts=[("race", r), ("creator", r)])
for cid, nm, race, reg, y in CIVS:
    civf[cid] = add(y, "civ_founded", region=reg, parts=[("civilization", cid), ("race", race)], causes=[aw[race]])
    stf[cid] = add(y, "settlement_founded", region=reg, settlement=cid, parts=[("settlement", cid), ("founder", cid)],
                   values=[500])
# more settlements: Duldar (s8, the Hold's, the Grey Fells), Nub Pit (s9, the Gub Horde's, Nub Marsh), Elmere (s10,
# the elves', Ash Vale), Kazad Low (s11, the Hold's, Kazad Fells)
EXTRA = {8: ("Duldar", 3, 8, 150), 9: ("Nub Pit", 4, 9, 250), 10: ("Elmere", 1, 10, 120), 11: ("Kazad Low", 3, 3, 160)}
for sid, (nm, civ, reg, y) in sorted(EXTRA.items(), key=lambda kv: kv[1][3]):
    stf[sid] = add(y, "settlement_founded", region=reg, settlement=sid, parts=[("settlement", sid), ("founder", civ)],
                   values=[300])

relations = []
peace_open = {}   # (min, max) -> index in relations of the open peace row


def rel(kind, a, b, since, se, until=-1, ue=0):
    relations.append([3, a, 3, b, kind, since, until, se, ue])
    return len(relations) - 1


def close(i, until, ue):
    relations[i][6], relations[i][7 + 1] = until, ue


def war_start(year, att, de, region, causes, joins=()):
    """war_declared and its row; the open peace row of the pair (and of each joining ally with the attacker) closes."""
    w = add(year, "war_declared", region=region, parts=[("attacker", att), ("defender", de)], causes=causes)
    k = (min(att, de), max(att, de))
    if k in peace_open:
        close(peace_open.pop(k), year, w)
    return w, rel("war", att, de, year, w)


def war_end(year, kind, w_row, ev_id, pairs):
    close(w_row, year, ev_id)
    for a, b in pairs:
        k = (min(a, b), max(a, b))
        peace_open[k] = rel("peace", k[0], k[1], year, ev_id)


n = None
# --- peoples: elves and dwarves met in friendship (120); elves and goblins met in hatred (220)
met13 = add(120, "peoples_met", region=1, parts=[("race_a", 1), ("race_b", 3)], values=[200, 2, 0], causes=[civf[3]])
met14 = add(220, "peoples_met", region=1, parts=[("race_a", 1), ("race_b", 4)], values=[-700, 0, 0], causes=[civf[4]])
# --- 50: the origin alliance of the elves and the faeries (term 150; band 2: never met, plain friendship)
al12 = add(50, "alliance_formed", parts=[("ally_a", 1), ("ally_b", 2)], values=[0, 150, 2], causes=[civf[2]])
rel("alliance", 1, 2, 50, al12)
# --- trades: c3-c5 (400), c1-c5 (410), c5-c7 (420)
tr35 = add(400, "trade_opened", parts=[("partner_a", 3), ("partner_b", 5)], causes=[civf[5]])
rel("trade", 3, 5, 400, tr35)
tr15 = add(410, "trade_opened", parts=[("partner_a", 1), ("partner_b", 5)], causes=[civf[5]])
rel("trade", 1, 5, 410, tr15)
tr57 = add(420, "trade_opened", parts=[("partner_a", 5), ("partner_b", 7)], causes=[civf[7]])
tr57_row = rel("trade", 5, 7, 420, tr57)
# --- 450: the Hold's mine at its capital; 451 arms sold to Cear
mine1 = add(450, "mine_opened", region=3, settlement=3, parts=[("civilization", 3), ("site", 1)], causes=[stf[3]])
arms = add(451, "arms_sold", parts=[("seller", 3), ("buyer", 5), ("site", 1)], causes=[tr35, mine1])
# --- 470: war A, the Gub Horde on Cear; 480 the Hold and Cear ally against the Horde (basis 1, band 2: unmet); 490
warA, warA_row = war_start(470, 4, 5, 5, [civf[5]])
al35 = add(480, "alliance_formed", parts=[("ally_a", 3), ("ally_b", 5), ("foe", 4)], values=[1, 150, 2], causes=[warA])
rel("alliance", 3, 5, 480, al35)
peaceA = add(490, "peace", parts=[("winner", 5), ("loser", 4)], values=[20], causes=[warA])
war_end(490, "peace", warA_row, peaceA, [(4, 5)])
# --- 600: war B, the Gub Horde on the Hold over the Grey Fells; Cear defends; an allied battle; Nub Pit sacked; the
# Hold wins; tribute; the share of 200 kept from Cear: the alliance sours (grievance 1)
warB, warB_row = war_start(600, 4, 3, 8, [peaceA])
joinB = add(600, "ally_joined_war", parts=[("ally", 5), ("partner", 3), ("enemy", 4)], values=[0], causes=[warB, al35])
k = (4, 5)
close(peace_open.pop(k), 600, joinB)
joinB_row = rel("war", 5, 4, 600, joinB)
batB = add(601, "allied_battle", region=8, parts=[("winner", 3), ("loser", 4), ("winner_ally", 5)],
           values=[140, 812, 0], causes=[warB])
sgB = add(603, "siege", region=9, settlement=9, parts=[("attacker", 3), ("defender", 4)], values=[1], causes=[batB])
sackB = add(603, "sacked", region=9, settlement=9, parts=[("attacker", 3), ("defender", 4)], values=[300, 900],
            causes=[sgB])
wonB = add(605, "war_won", parts=[("winner", 3), ("loser", 4)], values=[5], causes=[warB])
tribB = add(605, "tribute", parts=[("payer", 4), ("payee", 3)], values=[200], causes=[wonB])
whB = add(605, "spoils_withheld", region=8, parts=[("principal", 3), ("ally", 5), ("enemy", 4)], values=[200, 1],
          causes=[wonB, joinB])
close(warB_row, 605, wonB)
close(joinB_row, 605, wonB)
peace_open[(3, 4)] = rel("peace", 3, 4, 605, wonB)
peace_open[(4, 5)] = rel("peace", 4, 5, 605, wonB)
# --- 630: the renewal of 630 (soured: the draw passed), notable: the war they fought together
ren35 = add(630, "alliance_renewed", parts=[("ally_a", 3), ("ally_b", 5), ("foe", 4)], values=[1], causes=[joinB, al35])
# --- 650: the Skreegg and the Urarz Hordes ally in friendship (band 2: one race)
al67 = add(650, "alliance_formed", parts=[("ally_a", 6), ("ally_b", 7)], values=[3, 150, 2], causes=[civf[7]])
al67_row = rel("alliance", 6, 7, 650, al67)
# --- 690: the Skreegg Horde on the elves; 695 the elves prevail; 700 the elves and the Gub Horde ally against the
# Skreegg Horde across hatred (band 0); it lapses at its first due year (850, no reason left)
warC, warC_row = war_start(690, 6, 1, 1, [civf[6]])
peaceC = add(695, "peace", parts=[("winner", 1), ("loser", 6)], values=[5], causes=[warC])
war_end(695, "peace", warC_row, peaceC, [(1, 6)])
al14 = add(700, "alliance_formed", parts=[("ally_a", 1), ("ally_b", 4), ("foe", 6)], values=[1, 150, 0], causes=[warC])
al14_row = rel("alliance", 1, 4, 700, al14)
# --- 800: war E, Cear on the Skreegg Horde; the Urarz Horde refuses (motive 3, count 1); 802 Cear wins its first
# battle since the sale of arms (cites it); 810 truce
warE, warE_row = war_start(800, 5, 6, 6, [civf[6]])
refE = add(800, "ally_refused", parts=[("ally", 7), ("partner", 6), ("enemy", 5)], values=[3, 1], causes=[warE, al67])
batE = add(802, "battle", region=6, parts=[("winner", 5), ("loser", 6)], values=[40, 120], causes=[warE, arms])
truceE = add(810, "truce", parts=[("parties", 5), ("parties", 6)], values=[30], causes=[batE])
war_end(810, "truce", warE_row, truceE, [(5, 6)])
# --- 820: war F, the Hold on the Skreegg Horde; the Urarz Horde refuses again (motive 4, count 2): broken; 830 peace
warF, warF_row = war_start(820, 3, 6, 6, [truceE])
refF = add(820, "ally_refused", parts=[("ally", 7), ("partner", 6), ("enemy", 3)], values=[4, 2], causes=[warF, al67])
brF = add(820, "alliance_broken", parts=[("ally_a", 6), ("ally_b", 7)], values=[0], causes=[refF])
close(al67_row, 820, brF)
peaceF = add(830, "peace", parts=[("winner", 3), ("loser", 6)], values=[10], causes=[warF])
war_end(830, "peace", warF_row, peaceF, [(3, 6)])
lap14 = add(850, "alliance_lapsed", parts=[("ally_a", 1), ("ally_b", 4)], values=[0], causes=[al14])
close(al14_row, 850, lap14)
# --- 900: the Gub and the Urarz Hordes ally in fear of the Hold (basis 2; the Hold's latest gain, Kazad Low)
al47 = add(900, "alliance_formed", parts=[("ally_a", 4), ("ally_b", 7), ("foe", 3)], values=[2, 150, 2],
           causes=[stf[11]])
al47_row = rel("alliance", 4, 7, 900, al47)
# --- 990: the Skreegg Horde on the Hold (war Y, open until 1020); 1000: the Gub Horde on Cear (war X): the Hold,
# fighting war Y, will not come (excused: motive 2, count 0); 1010 Cear prevails; 1020 the Hold prevails
warY, warY_row = war_start(990, 6, 3, 3, [peaceF])
warX, warX_row = war_start(1000, 4, 5, 5, [wonB])
refX = add(1000, "ally_refused", parts=[("ally", 3), ("partner", 5), ("enemy", 4)], values=[2, 0], causes=[warX, ren35])
# --- 1000: the elves and the goblins put aside their hatred (a thaw, change 1)
turn14 = add(1000, "disposition_turned", parts=[("race_a", 1), ("race_b", 4)], values=[-400, 1, 1], causes=[met14])
peaceX = add(1010, "peace", parts=[("winner", 5), ("loser", 4)], values=[10], causes=[warX])
war_end(1010, "peace", warX_row, peaceX, [(4, 5)])
peaceY = add(1020, "peace", parts=[("winner", 3), ("loser", 6)], values=[30], causes=[warY])
war_end(1020, "peace", warY_row, peaceY, [(3, 6)])
# --- 1100: the elves and Cear ally after long trade (basis 4); 1150 war Z, the Skreegg Horde on Cear; the elves defend;
# 1152 an allied battle won by Cear (no arms cited: the sale was cited in 802); 1160 Cear wins and keeps the elves'
# share of 90 (grievance 1: soured); 1250 the renewal's draw fails: spurned (reason 0, years 90)
al15 = add(1100, "alliance_formed", parts=[("ally_a", 1), ("ally_b", 5)], values=[4, 150, 2], causes=[tr15])
al15_row = rel("alliance", 1, 5, 1100, al15)
warZ, warZ_row = war_start(1150, 6, 5, 5, [truceE])
joinZ = add(1150, "ally_joined_war", parts=[("ally", 1), ("partner", 5), ("enemy", 6)], values=[0], causes=[warZ, al15])
close(peace_open.pop((1, 6)), 1150, joinZ)
joinZ_row = rel("war", 1, 6, 1150, joinZ)
batZ = add(1152, "allied_battle", region=5, parts=[("winner", 5), ("loser", 6), ("winner_ally", 1)],
           values=[30, 300, 0], causes=[warZ])
wonZ = add(1160, "war_won", parts=[("winner", 5), ("loser", 6)], values=[10], causes=[warZ])
tribZ = add(1160, "tribute", parts=[("payer", 6), ("payee", 5)], values=[150], causes=[wonZ])
whZ = add(1160, "spoils_withheld", region=5, parts=[("principal", 5), ("ally", 1), ("enemy", 6)], values=[90, 1],
          causes=[wonZ, joinZ])
close(warZ_row, 1160, wonZ)
close(joinZ_row, 1160, wonZ)
peace_open[(5, 6)] = rel("peace", 5, 6, 1160, wonZ)
peace_open[(1, 6)] = rel("peace", 1, 6, 1160, wonZ)
# --- 1200: the elves and the dwarves grow wary (a decline, change 2), after long years apart; a quiet stretch of the
# Hold (phase 6, 1200-1299)
turn13 = add(1200, "disposition_turned", parts=[("race_a", 1), ("race_b", 3)], values=[100, 1, 2], causes=[turn14])
q3 = add(1200, "quiet_stretch", parts=[("realm", 3)], values=[100, 1], end=1299)
spZ = add(1250, "alliance_spurned", region=5, parts=[("ally", 1), ("principal", 5)], values=[0, 90], causes=[whZ, al15])
close(al15_row, 1250, spZ)
# --- 1300: war G, the Hold on the Gub Horde over Gor Heath; the Urarz Horde defends; an allied battle won by the
# Horde; 1310 the Horde wins, tribute; it keeps the Urarz Horde's share of 650 (large: grievance 2), spurned at once
warG, warG_row = war_start(1300, 3, 4, 4, [peaceY])
joinG = add(1300, "ally_joined_war", parts=[("ally", 7), ("partner", 4), ("enemy", 3)], values=[0], causes=[warG, al47])
joinG_row = rel("war", 7, 3, 1300, joinG)
batG = add(1302, "allied_battle", region=4, parts=[("winner", 4), ("loser", 3), ("winner_ally", 7)],
           values=[200, 600, 0], causes=[warG])
wonG = add(1310, "war_won", parts=[("winner", 4), ("loser", 3)], values=[10], causes=[warG])
tribG = add(1310, "tribute", parts=[("payer", 3), ("payee", 4)], values=[1300], causes=[wonG])
whG = add(1310, "spoils_withheld", region=4, parts=[("principal", 4), ("ally", 7), ("enemy", 3)], values=[650, 2],
          causes=[wonG, joinG])
spG = add(1310, "alliance_spurned", region=4, parts=[("ally", 7), ("principal", 4)], values=[2, 0], causes=[whG, al47])
close(al47_row, 1310, spG)
close(warG_row, 1310, wonG)
close(joinG_row, 1310, wonG)
peace_open[(3, 4)] = rel("peace", 3, 4, 1310, wonG)
peace_open[(3, 7)] = rel("peace", 3, 7, 1310, wonG)
# --- 1400: the Gub and the Skreegg Hordes ally in friendship; 1600 the Skreegg Horde betrays it (motive 2, a weak ally;
# 200 years); 1610 the Skreegg Horde prevails
al46 = add(1400, "alliance_formed", parts=[("ally_a", 4), ("ally_b", 6)], values=[3, 150, 2], causes=[civf[6]])
al46_row = rel("alliance", 4, 6, 1400, al46)
# --- 1500: four same-day mixed events in two groups
mixes = []
for sid, civ, race, ruling, standing in ((1, 1, 3, 1, 2), (10, 1, 3, 1, 2), (3, 3, 1, 3, 1), (11, 3, 1, 3, 1)):
    reg = sid if sid <= 7 else EXTRA[sid][2]
    mixes.append(add(1500, "settlement_mixed", region=reg, settlement=sid,
                     parts=[("settlement", sid), ("civilization", civ), ("race", race), ("ruling", ruling)],
                     values=[100, standing, 0], causes=[met13]))
warH, warH_row = war_start(1600, 6, 4, 4, [al46])
betH = add(1600, "alliance_betrayed", parts=[("betrayer", 6), ("betrayed", 4)], values=[2, 200], causes=[warH, al46])
close(al46_row, 1600, betH)
peaceH = add(1610, "peace", parts=[("winner", 6), ("loser", 4)], values=[10], causes=[warH])
war_end(1610, "peace", warH_row, peaceH, [(4, 6)])
# --- 1790: war I, the Hold on the Urarz Horde; 1800 Urarzul taken; the Urarz Horde conquered: its trade with Cear
# ends with it (`; ended`)
warI, warI_row = war_start(1790, 3, 7, 7, [wonG])
capI = add(1800, "settlement_captured", region=7, settlement=7, parts=[("attacker", 3), ("defender", 7)],
           values=[50, 0], causes=[warI])
conqI = add(1800, "civ_conquered", region=7, parts=[("civilization", 7), ("conqueror", 3)], causes=[capI])   # region_use 1 (integration: the reader refuses region 0)
close(warI_row, 1800, conqI)
close(tr57_row, 1800, conqI)

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
civf = {k: N(v) for k, v in civf.items()}
stf = {k: N(v) for k, v in stf.items()}

races = [[1, "elf", "Elf", "Elves", "Elven", 0, 700, 150, 1, 1, 0, 1],
         [2, "faerie", "Faerie", "Faeries", "Faerie", 0, 500, 100, 2, 1, 50, 2],
         [3, "dwarf", "Dwarf", "Dwarves", "Dwarven", 0, 300, 40, 3, 1, 100, 3],
         [4, "goblin", "Goblin", "Goblins", "Goblin", 2, 60, 20, 4, 1, 200, 4],
         [5, "human", "Human", "Humans", "Human", 1, 80, 25, 5, 1, 300, 5]]
CIV_RACE = {c[0]: c[2] for c in CIVS}
STL = {sid: (CAPS[sid], sid, sid, CIVS[sid - 1][4]) for sid in range(1, 8)}
for sid, (nm, civ, reg, y) in EXTRA.items():
    STL[sid] = (nm, civ, reg, y)
stl_rows, census, sr = [], [], []
for sid in sorted(STL):
    nm, civ, reg, y = STL[sid]
    fe = stf[sid]
    if sid == 9:   # sacked in 603
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, 0, 1, 3, 0, 603, N(sackB)])
        census += [[sid, y, civ, 1, 1, 1200], [sid, 603, 0, 1, 3, 0]]
    elif sid == 7:   # taken by the Hold in 1800
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, 3, 3, 1, 900, -1, 0])
        census += [[sid, y, civ, 3, 1, 1000], [sid, 1800, 3, 3, 1, 900]]
    else:
        pop = {1: 2000, 2: 900, 3: 1500, 4: 1800, 5: 1600, 6: 1100, 8: 400, 10: 500, 11: 450}[sid]
        kind = 3 if sid <= 7 else 2
        stl_rows.append([sid, nm, 10 * sid, 10 * sid, reg, civ, y, fe, civ, kind, 1, pop, -1, 0])
        census.append([sid, y, civ, kind, 1, pop])
    sr.append([sid, CIV_RACE[civ], y, 1000, 3])
# 1500: the mixed shares (P5-03 6.3: the ruling race 900, the other 100)
for sid, race, ruling, standing in ((1, 3, 1, 2), (10, 3, 1, 2), (3, 1, 3, 1), (11, 1, 3, 1)):
    sr += [[sid, ruling, 1500, 900, 3], [sid, race, 1500, 100, standing]]
rc = [[r, CIVS[r - 1][4], -1, r, civf[r]] for r in range(1, 7)]
rc += [[7, 220, 1800, 7, civf[7]], [7, 1800, -1, 3, N(capI)]]
rc += [[8, 150, -1, 3, stf[8]], [9, 250, -1, 4, stf[9]], [10, 120, -1, 1, stf[10]]]
# integration: no row at E that equals the pair's previous row (the reader refuses it: "equals the previous row of
# this pair"); the last row of a pair is its state at E
rr = [[1, 3, 120, 200, 2], [1, 3, 1200, 100, 1],
      [1, 4, 220, -700, 0], [1, 4, 1000, -400, 1]]
sites = [[1, "The Mines of Kazad Rune", 3, 3, 30, 30, 3, 3, 0, 450, N(mine1), -1, 0, 0]]
KINDS = [k["key"] for k in pe.COMPILED_028][:92]
CIV8 = None
civ_rows = [[cid, nm, race, race, reg, cid, y, civf[cid], -1, 0] for cid, nm, race, reg, y in CIVS]
civ_rows[6][8], civ_rows[6][9] = 1800, N(conqI)


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


IDS = dict(al12=al12, al35=al35, whB=whB, ren35=ren35, al67=al67, refE=refE, batE=batE, refF=refF, brF=brF,
           al14=al14, lap14=lap14, al47=al47, refX=refX, turn14=turn14, al15=al15, whZ=whZ, batZ=batZ, turn13=turn13,
           spZ=spZ, whG=whG, spG=spG, al46=al46, betH=betH, conqI=conqI, arms=arms, tr57=tr57, q3=q3,
           mix1=mixes[0], mix2=mixes[1], mix3=mixes[2], mix4=mixes[3])
IDS = {k: N(v) for k, v in IDS.items()}
o = ["""# P5-04a 16 item 12: H18 (format v9, schema 12; the spec's "H17": P5-06 holds that name). Generated by
# p5_04a_h18_fixture.py, whose docstring says what each event is; the 0.28.0 catalogue and templates of 14.3 and 14.4.
# Soured c3-c5 (whB e%(whB)d) renewed (e%(ren35)d) with two quiet renewals; c1-c5 spurned at renewal (e%(spZ)d); c4-c7
# spurned at once (e%(spG)d); refusals e%(refX)d (excused), e%(refE)d, e%(refF)d (counted, broken e%(brF)d); betrayal
# e%(betH)d; across hatred e%(al14)d; thaw e%(turn14)d, decline e%(turn13)d; mixed e%(mix1)d to e%(mix4)d; arms cited
# by e%(batE)d; trade c5-c7 ended by e%(conqI)d. Written by the test writer from the spec; format: README.md.
""" % IDS]
o.append("""[meta]
seed = 1818
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
o.append(rows("civilization", civ_rows))
o.append("# Fourteen columns: settlement.ending is derived from the log (P5-02 8.3).\n")
o.append(rows("settlement", stl_rows))
o.append(rows("settlement_census", sorted(census)))
o.append(rows("region_control", sorted(rc)))
o.append(rows("relation", sorted(relations, key=lambda r: (r[0], r[1], r[2], r[3], r[4], r[5]))))
o.append("region_name = []\n")
o.append("beast = []\nmonster_kind = []\nbeast_population = []\nmonster = []\n")
o.append(rows("site", sites))
o.append(rows("race_relation", sorted(rr)))
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
    # The reference's lines for the fixture: the sentences of the new templates (p3_engine_reference.render at 0.28.0),
    # the [alliances] and allies: lines of 12.3 and 12.5 and the [stats] keys of 12.6 (p5_04_alliances_reference).
    import tempfile
    import p3_report_reference as prr
    import p5_04_alliances_reference as p504
    pe.use_version("0.28.0")
    db = os.path.join(tempfile.mkdtemp(), "h18-v9.db")
    prr.write_fixture("h18-v9", db)
    w = prr.load_world(db)
    pe.use_version("0.28.0")
    for name in ("whB", "ren35", "refE", "refF", "refX", "al14", "spZ", "spG", "betH", "turn14", "turn13", "batE",
                 "batZ", "mix1", "mix3"):
        print("%s e%d: %s" % (name, IDS[name], w.sentence(IDS[name])))
    a = p504.aw_of(db)
    print("--- alliances")
    for r in sorted(a.rows.get("alliance", []), key=lambda r: (r[5], r[1], r[3])):
        print(p504.alliance_line_v9(a, r))
    print("--- trade")
    for r in sorted(a.rows.get("trade", []), key=lambda r: (r[5], r[1], r[3])):
        print(p504.trade_line_v9(a, r))
    print("--- allies")
    for c in sorted(a.civ):
        print(c, p504.civ_alliance_lines_v9(a, c, "summary"))
    print("--- stats")
    for k, v in p504.stats_028(a):
        print("%s %s" % (k, v))
    print("--- reader and invariants")
    print(p504.reader_problems_028(a))
    t28 = dict(pdr.ALLIANCES_TUNING_026, **{n: v for n, _, _, v in pdr.ALLIANCES_NEW_RANGES_028})
    t28.update(pdr.ALLIANCES_CHANGED_028)
    print(p504.invariant_violations_028(a, t28))
