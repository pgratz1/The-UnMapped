#!/usr/bin/env python3
"""Reference of P5-12 "Regions renamed by history" (docs/specs/p5-12-renamed-regions.md r0), written from the
spec alone, without the engineer's branch.

The renaming subsystem of section 6 runs inside p3_conflict_reference.py's replay (its World calls
Renaming.run after the conflict subsystem in T6 and Q4, P5-00 4.1), so every history golden of the conflict
reference carries it. This module holds the subsystem, its names (5.1 to 5.4), the land noun (3), the
invariants of section 8 on rows (check-world mode), the hand-made units of 13 and the golden file
worldgen/tests/golden/p5-12-renaming.txt.

Commands (from the repository root):
  python3 -B worldgen/tests/reference/p5_12_renaming_reference.py check [--quick] [--inputs DIR]
  python3 -B worldgen/tests/reference/p5_12_renaming_reference.py golden [PATH|-] [--inputs DIR]
  python3 -B worldgen/tests/reference/p5_12_renaming_reference.py check-world DB [DB ...]
  python3 -B worldgen/tests/reference/p5_12_renaming_reference.py rows DB      the region_name rows recomputed

--inputs DIR: the world inputs (default ~/.cache/the-unmapped/out/p5-12-tests/inputs: the P5-01 inputs with
P1-06 r11's region names, p1_regions_reference.py rename-db).
"""

import hashlib
import os
import sqlite3
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p3_conflict_reference as pcr  # noqa: E402
import p3_engine_reference as pe  # noqa: E402
import p3_names_reference as pnr  # noqa: E402
import p3_chronology_reference as pchr  # noqa: E402
import p1_regions_reference as rg  # noqa: E402

GOLDEN = os.path.join(pe.REPO, "worldgen", "tests", "golden", "p5-12-renaming.txt")
DEFAULT_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-12-tests", "inputs")
STREAMS = ("hist.renaming.chance", "hist.renaming.name")   # 3: after hist.conflict.toll, in this order
CIV, SETTLEMENT, REGION = pe.CIV, pe.SETTLEMENT, pe.REGION

READINGS = """\
R1 (3, 5.1) {noun}: P1-06 RG11 3 rules a to d on the stored rows. A synthetic geography (p3_peoples_reference
   make_synthetic) has no landmass or biome column: every region is on landmass 1 (rule a never applies) and its
   dominant biome is the region_biome row with the most cells among non-water biomes, ties to the smaller code.
R2 (6 item 1) the window: entries with year <= year(e) - window_years are removed after appending e, so with
   window_years 1 only the events of e's year remain.
R3 (6 item 1) the chance draw is keyed (e, r, k) with k the rule number from 1; the name draw (e, r, 0); both
   with w0 = the step's year (a quiet step's start), through the World's draw (its extra draw included).
R4 (6 item 2) every candidate of a region but the chosen one is dropped with it; a dropped candidate never
   comes back; windows are kept unless a renaming is written.
R5 (6 item 3) the settlement of the renaming event is e's settlement when that settlement's region is r.
R6 (5.1) a settlement role's value is the first participant of that role; `settlement` names the event's
   settlement (event.settlement), also for a kind that has a role named settlement.
R7 (5.2 item 3) V4 and V5 as P1-06 name_valid: words split at spaces, lower-cased for V4; V5 compares the whole
   name and each word with the one-word entries, bytes as stored.
R8 (5.2 item 4) V8's parts are the form's literal runs and the two slot values in form order; an empty literal
   run (two slots adjacent) is no part.
R9 (5.4) the namer is the first participant of namer_role; region_name.culture is the namer's culture id.
R10 (6 item 2.5) a quiet step's namer check reads quiet_events_left before the event is written; the event is
   then charged to the namer by the event writer (W9).
R11 (peoples-only) the renaming subsystem is installed only in the shipped configuration (after conflict);
   the peoples-only test configuration of p3-05-peoples.txt has none.
"""


# ---------------------------------------------------------------------------
# 3: the land noun of a region from stored rows (P1-06 RG11 3 a to d)
# ---------------------------------------------------------------------------

def land_nouns(inp):
    """{region id: noun} by P1-06 RG11 3 rules a to d (R1)."""
    t = rg.shipped_tuning()
    defs = rg.shipped_defs()   # (code, key, water, region_noun)
    noun = {c: nn for c, _, _, nn in defs}
    water = {c: wat for c, _, wat, _ in defs}
    code_of = {k: c for c, k, _, _ in defs}
    own = set(code_of[k] for k in t["upland_own_noun"])
    words = t["names"]
    isle_word, upland_word = words[9], words[10]
    mere = rg.lake_noun(defs)
    land = getattr(inp, "region_land", None)
    rb = {}
    for r, b, n in inp.region_biomes:
        rb.setdefault(r, []).append((b, n))
    out = {}
    lm_of, biome_of = {}, {}
    for r in inp.regions:
        if land is not None:
            lm_of[r["id"]], biome_of[r["id"]] = land[r["id"]]
        else:
            lm_of[r["id"]] = 1
            dry = [(b, n) for b, n in rb.get(r["id"], []) if not water.get(b, False)]
            pool = dry or rb.get(r["id"], [])
            biome_of[r["id"]] = min(pool, key=lambda bn: (-bn[1], bn[0]))[0] if pool else min(noun)
    per_lm, lm_cells = {}, {}
    for r in inp.regions:
        per_lm[lm_of[r["id"]]] = per_lm.get(lm_of[r["id"]], 0) + 1
        lm_cells[lm_of[r["id"]]] = lm_cells.get(lm_of[r["id"]], 0) + r["cells"]
    largest = min(lm_cells.items(), key=lambda kv: (-kv[1], kv[0]))[0]
    for r in inp.regions:
        rid, b = r["id"], biome_of[r["id"]]
        if per_lm[lm_of[rid]] == 1 and lm_of[rid] != largest:
            out[rid] = isle_word
        elif 2 * r["lake_cells"] >= r["cells"]:
            out[rid] = mere
        elif 4 * r["walk_cells"] < r["cells"] and b not in own:
            out[rid] = upland_word
        else:
            out[rid] = noun[b]
    return out


# ---------------------------------------------------------------------------
# 5: names
# ---------------------------------------------------------------------------

def realm_root(name, civilization_forms):
    """5.3: the first form A{root}B (list order) such that name starts with A, ends with B and is longer than
    |A| + |B|; the root is name without A and B; name itself when no form matches."""
    for f in civilization_forms:
        a, b = f.split("{root}", 1)
        if name.startswith(a) and name.endswith(b) and len(name) > len(a) + len(b):
            return name[len(a):len(name) - len(b)]
    return name


# P5-02 21.2 (generator 0.23.0): the slot {stem} and the possessive rule. R23 follows the conflict reference's served
# rules (pcr.FLAGS["monsters"]); with R23 off the 0.22.0 rules hold exactly.
def r23():
    return pcr.FLAGS.get("monsters", False)


def form_pieces(form, name_value, noun_value, stem_value=None):
    """[(kind, text)]: ("lit", run) and ("slot", value) in form order; P5-02 21.2's possessive rule applied (a slot
    value ending in s or S followed by a literal starting with 's: that s dropped)."""
    out = []
    i = 0
    lit = ""
    slots = (("{name}", name_value), ("{noun}", noun_value)) + ((("{stem}", stem_value),) if r23() else ())
    while i < len(form):
        for tok, val in slots:
            if form.startswith(tok, i):
                if lit:
                    out.append(("lit", lit))
                    lit = ""
                out.append(("slot", val if val is not None else ""))
                i += len(tok)
                break
        else:
            lit += form[i]
            i += 1
    if lit:
        out.append(("lit", lit))
    if r23():
        for j in range(1, len(out)):
            if out[j][0] == "lit" and out[j - 1][0] == "slot" and out[j - 1][1][-1:] in ("s", "S") and \
                    out[j][1].startswith("'s"):
                out[j] = ("lit", "'" + out[j][1][2:])
    return out


def fill(form, name_value, noun_value, stem_value=None):
    """The form's text (21.2's possessive rule included at 0.23.0)."""
    return "".join(x for _k, x in form_pieces(form, name_value, noun_value, stem_value))


def form_parts(form, name_value, noun_value, stem_value=None):
    """The parts of 5.2 item 4 (R8): literal runs and slot values in form order (P5-02 21.2: after the possessive
    rule)."""
    return [x for k, x in form_pieces(form, name_value, noun_value, stem_value) if k == "slot" or x]


_COMMON = None


def placename_common():
    global _COMMON
    if _COMMON is None:
        _COMMON = rg.placename_data()["common"]
    return _COMMON


def name_problem(n, parts, common=None, filt=None):
    """5.2: the first failing item ("V6", "V2", "V4", "V5", "V8") or None."""
    common = common or placename_common()
    if len(n.encode()) > 48:
        return "V6"
    low = n.lower()
    for i in range(len(low) - 2):
        if low[i] == low[i + 1] == low[i + 2] and "a" <= low[i] <= "z":
            return "V2"
    for wd in low.split(" "):
        if any(f in wd for f in common["forbidden"]):
            return "V4"
    if n in common["excluded"]:
        return "V5"
    one = set(e for e in common["excluded"] if " " not in e)
    if any(wd in one for wd in n.split(" ")):
        return "V5"
    if rg.v8_spans(parts, filt if filt is not None else rg.v8_filter()):
        return "V8"
    return None


def build_name(forms, start, name_value, noun_value, registry, common=None, filt=None, trace=None, stem_value=None):
    """5.1: (name, form index) of the first valid form from start, cycling; (None, None) when none."""
    n = len(forms)
    for j in range(n):
        fi = (start + j) % n
        f = forms[fi]
        nm = fill(f, name_value, noun_value, stem_value)
        parts = form_parts(f, name_value or "", noun_value, stem_value)
        prob = name_problem(nm, parts, common, filt)
        if prob is None and nm in registry:
            prob = "registry"
        if trace is not None:
            trace.append((fi, nm, prob))
        if prob is None:
            return nm, fi
    return None, None


# ---------------------------------------------------------------------------
# 6: the subsystem
# ---------------------------------------------------------------------------

class Renaming:
    """RenamingSubsystem (6) over a p3_conflict_reference World w."""

    def __init__(self, w, dat, inp, tuning=None):
        self.w = w
        self.t = tuning if tuning is not None else dat.renaming
        self.inp = inp
        for s in STREAMS:
            w.keys[s] = cr.stream_key(inp.seed, s)
        self.rules = self.t["rules"]
        self.cursor = 1
        self.win = [dict() for _ in self.rules]     # rule index -> region -> [(year, magnitude)]
        self.by_region = {}                           # region -> [(year, weight)]
        self.years = []                               # every renaming's year, ascending
        self.rows = []                                # (region, from_year, name, culture, event) in creation order
        self.nouns = land_nouns(inp)
        self.culture_key = {c[0]: c[1] for c in inp.cultures}
        self.R = len(inp.regions)
        # logs (not state), for checks and statistics
        self.log = []          # (year, region, rule key, cause, name, form index, namer)
        self.dropped = []      # (year, region, rule key, cause, reason)
        self.candidates = []   # (year, region, rule key, cause)

    # --- reads (StepContext, P3-04 r21 23.3)
    def ev(self, e):
        return self.w.events[e - 1]

    def value(self, e, key):
        ev = self.ev(e)
        k = self.w.cat[ev["kind"] - 1] if self.w.cat[ev["kind"] - 1]["id"] == ev["kind"] else \
            [x for x in self.w.cat if x["id"] == ev["kind"]][0]
        if key not in k["values"]:
            pcr.fatal("history: state: event %d has no value '%s'" % (e, key))
        return ev["values"][k["values"].index(key) + 1]

    def kind_of(self, ev):
        return [x for x in self.w.cat if x["id"] == ev["kind"]][0]

    def first_of_role(self, ev, role):
        k = self.kind_of(ev)
        rs = [r["seq"] for r in k["roles"] if r["role"] == role]
        if not rs:
            return None
        for (_s, rseq, t, x) in sorted(ev["participants"]):
            if rseq == rs[0]:
                return (t, x)
        return None

    @staticmethod
    def first_civ(ev):
        for (_s, _rs, t, x) in sorted(ev["participants"]):
            if t == CIV:
                return x
        return 0

    # --- caps
    def cap(self, permille):
        return max(1, self.R * permille // 1000)

    def run(self, quiet):
        w = self.w
        Y = w.year
        cands = {}   # region -> [(weight, e, rule index)]
        last = len(w.events)
        for e in range(self.cursor, last + 1):
            ev = self.ev(e)
            r = ev["region"]
            if r == 0:
                continue
            for i, rule in enumerate(self.rules):
                if ev["key"] not in rule["kinds"]:
                    continue
                m = sum(self.value(e, v) for v in rule["values"]) if rule["values"] else 1
                lst = self.win[i].setdefault(r, [])
                lst.append((ev["year"], m))
                lst[:] = [x for x in lst if x[0] > ev["year"] - rule["window_years"]]
                S = sum(x[1] for x in lst)
                if S < rule["min_magnitude"] or ev["salience"] < rule["min_salience"]:
                    continue
                if rule["capital"]:
                    c = self.first_civ(ev)
                    if ev["settlement"] == 0 or c == 0 or w.civ(c)["capital"] != ev["settlement"]:
                        continue
                if not pnr_chance(w.draw("hist.renaming.chance", e, r, i + 1), rule["chance_permille"]):
                    continue
                cands.setdefault(r, []).append((rule["weight"], e, i))
                self.candidates.append((Y, r, rule["key"], e))
        self.cursor = last + 1
        for r in sorted(cands):
            weight, e, i = min(cands[r], key=lambda x: (-x[0], x[1]))
            rule = self.rules[i]
            why = self.refusal(r, Y, rule, e, quiet)
            nm = None
            if why is None:
                nm, fi, namer, start = self.name_for(r, rule, e)
                if nm is None:
                    why = "name"
            if why is not None:
                self.dropped.append((Y, r, rule["key"], e, why))
                continue
            self.write(r, Y, rule, e, nm, fi, namer)

    def refusal(self, r, Y, rule, e, quiet):
        mine = self.by_region.get(r, [])
        if len(mine) >= self.t["region_max"]:
            return "region_max"
        if mine and not (Y - mine[-1][0] >= self.t["region_gap_years"] and rule["weight"] > mine[-1][1]):
            return "gap" if Y - mine[-1][0] < self.t["region_gap_years"] else "weight"
        if len(self.years) >= self.cap(self.t["world_max_permille"]):
            return "world"
        if sum(1 for y in self.years if y > Y - self.t["century_years"]) >= self.cap(self.t["century_max_permille"]):
            return "century"
        if quiet:
            namer = self.namer(rule, self.ev(e))
            if namer and self.w.quiet_events_left(namer) <= 0:
                return "quiet"
        return None

    def namer(self, rule, ev):
        if rule["namer_role"] == "":
            return 0
        p = self.first_of_role(ev, rule["namer_role"])
        return p[1] if p is not None and p[0] == CIV else 0

    def name_value(self, rule, ev):
        w = self.w
        nr = rule["name_role"]
        if nr == "":
            return None
        if nr == "settlement":
            return w.stl(ev["settlement"])["name"]
        p = self.first_of_role(ev, nr)
        if p is None:
            pcr.fatal("history: renaming: event %d has no %s" % (ev["id"], nr))
        t, x = p
        if t == SETTLEMENT:
            return w.stl(x)["name"]
        if t == pe.MONSTER:   # P5-02 9.3: {name} is the monster's root, read through the monsters subsystem
            return w.monsters.monster_root_of(x)
        c = w.civ(x)
        forms = w.dat.naming[self.culture_key[c["culture"]]]["civilization_forms"]
        return realm_root(c["name"], forms)

    def stem_value(self, rule, ev):
        """P5-02 21.2 {stem}: for a settlement name_role, the name without the first separate-word suffix B (a form
        {root}B of the founder's culture's settlement_forms, list order, B starting with a space, the name ending
        with B and longer than |B|); for any other name_role {name}'s value."""
        w = self.w
        nr = rule["name_role"]
        nv = self.name_value(rule, ev)
        if nr == "settlement":
            st = w.stl(ev["settlement"])
        else:
            p = self.first_of_role(ev, nr)
            if p is None or p[0] != SETTLEMENT:
                return nv
            st = w.stl(p[1])
        cul = self.culture_key[w.civ(st["founder_civ"])["culture"]]
        for f in w.dat.naming[cul]["settlement_forms"]:
            a, b = f.split("{root}", 1)
            if a == "" and b.startswith(" ") and nv.endswith(b) and len(nv) > len(b):
                return nv[:len(nv) - len(b)]
        return nv

    def name_for(self, r, rule, e, trace=None):
        ev = self.ev(e)
        start = pnr.pick(self.w.draw("hist.renaming.name", e, r, 0), len(rule["forms"]))
        stem = self.stem_value(rule, ev) if r23() and any("{stem}" in f for f in rule["forms"]) else None
        nm, fi = build_name(rule["forms"], start, self.name_value(rule, ev), self.nouns[r], self.w.registry,
                            trace=trace, stem_value=stem)
        return nm, fi, self.namer(rule, ev), start

    def write(self, r, Y, rule, e, nm, fi, namer):
        w = self.w
        ev = self.ev(e)
        s = ev["settlement"]
        if s and w.stl(s)["region"] != r:
            s = 0
        parts = [("namer", CIV, namer)] if namer else []
        eid = w.append("region_renamed", year=Y, end_year=Y, region=r, settlement=s, participants=parts,
                       causes=(e,))
        w.registry.add(nm)
        culture = w.civ(namer)["culture"] if namer else 0
        self.rows.append((r, Y, nm, culture, eid))
        self.by_region.setdefault(r, []).append((Y, rule["weight"]))
        self.years.append(Y)
        for i in range(len(self.rules)):
            self.win[i].pop(r, None)
        self.log.append((Y, r, rule["key"], e, nm, fi, namer))


def pnr_chance(u, permille):
    """chance_pm of P3-02 4.6 / P3-00 (p3_peoples_reference.chance_pm)."""
    return pcr.pp.chance_pm(u, permille)


# ---------------------------------------------------------------------------
# 8: invariants on rows (and the stage check of P3-01 r16 5.4)
# ---------------------------------------------------------------------------

def stage_check_message(rules, catalogue):
    """P3-01 r16 5.4 stage check renaming_rules: the first problem, rules in order, or None."""
    by = {k["key"]: k for k in catalogue}
    for rule in rules:
        def bad(p):
            return "history: renaming rule '%s': %s" % (rule["key"], p)
        for kk in rule["kinds"]:
            if kk not in by:
                return bad("kind '%s' is not in the catalogue" % kk)
            k = by[kk]
            if k["region_use"] != 1:
                return bad("kind '%s' has no region" % kk)
            for v in rule["values"]:
                if v not in k["values"]:
                    return bad("value '%s' is not a value of kind '%s'" % (v, kk))
            nr = rule["name_role"]
            if nr == "settlement":
                if k["settlement_use"] == 0:
                    return bad("name_role 'settlement' is not a civilization or settlement role of kind '%s'" % kk)
            elif nr != "":
                # P5-02 9.3: from 0.23.0 a monster role too (the message is unchanged: READINGS of p5_02 R1)
                types = (CIV, SETTLEMENT, pe.MONSTER) if r23() else (CIV, SETTLEMENT)
                rs = [r for r in k["roles"] if r["role"] == nr and r["type"] in types]
                if not rs:
                    return bad("name_role '%s' is not a civilization or settlement role of kind '%s'" % (nr, kk))
            if rule["namer_role"] != "":
                rs = [r for r in k["roles"] if r["role"] == rule["namer_role"] and r["type"] == CIV]
                if not rs:
                    return bad("namer_role '%s' is not a civilization role of kind '%s'" % (rule["namer_role"], kk))
            if rule["capital"] and (k["settlement_use"] == 0 or not any(r["type"] == CIV for r in k["roles"])):
                return bad("capital needs a kind with a settlement and a civilization role ('%s')" % kk)
    return None


def rows_violations(res, t, geo_names, other_names, nouns=None):
    """Invariants 1 to 5 of section 8 on the history tables (res as p3_conflict_reference.history_stage gives
    them, or read from a world). geo_names: {region: region.name}; other_names: every other name of the world
    (ages, gods, civilizations, settlements). Returns a list of messages."""
    out = []
    rows = sorted(res["region_name"])
    kinds = {k[0]: k[1] for k in res["event_kind"]}
    ev = {e[0]: e for e in res["event"]}
    causes = {}
    for x, c in res["event_cause"]:
        causes.setdefault(x, []).append(c)
    k43 = [e for e in res["event"] if kinds[e[3]] == "region_renamed"]
    by_event = {r[4]: r for r in rows}
    # 1
    for r in rows:
        e = ev.get(r[4])
        if e is None or kinds[e[3]] != "region_renamed" or e[5] != r[0] or e[1] != r[1]:
            out.append("1: row %r has no region_renamed of its region and year" % (r[:2],))
    for e in k43:
        if e[0] not in by_event:
            out.append("1: event %d has no row" % e[0])
    if len(by_event) != len(rows):
        out.append("1: two rows share an event")
    # 2
    rule_by_kind = {}
    for i, rule in enumerate(t["rules"]):
        for kk in rule["kinds"]:
            rule_by_kind.setdefault(kk, []).append(rule)
    per = {}
    for r in rows:
        per.setdefault(r[0], []).append(r)
    for rid, rs in per.items():
        if len(rs) > t["region_max"]:
            out.append("2: region %d has %d rows" % (rid, len(rs)))
        for a, b in zip(rs, rs[1:]):
            if b[1] - a[1] < t["region_gap_years"]:
                out.append("2: region %d rows %d and %d closer than the gap" % (rid, a[1], b[1]))
            wa = max_weight(rule_by_kind, kinds, ev, causes.get(a[4], []))
            wb = max_weight(rule_by_kind, kinds, ev, causes.get(b[4], []))
            if not (wb > wa):
                out.append("2: region %d: the later row's rule weight %r is not above %r" % (rid, wb, wa))
    # 3
    R = len(geo_names)
    wmax = max(1, R * t["world_max_permille"] // 1000)
    cmax = max(1, R * t["century_max_permille"] // 1000)
    if len(rows) > wmax:
        out.append("3: %d rows above the world cap %d" % (len(rows), wmax))
    ys = sorted(r[1] for r in rows)
    for y in ys:
        n = sum(1 for z in ys if y - t["century_years"] < z <= y)
        if n > cmax:
            out.append("3: %d rows in the century ending %d, cap %d" % (n, y, cmax))
            break
    # 4
    names = [r[2] for r in rows]
    if len(set(names)) != len(names):
        out.append("4: a renamed name repeats")
    allother = set(geo_names.values()) | set(other_names)
    for r in rows:
        if r[2] in allother:
            out.append("4: %s equals another name of the world" % r[2])
        if len(r[2].encode()) > 48 or not all(ch.isalpha() and ch.isascii() or ch in " '-" for ch in r[2]):
            out.append("4: %s is not a renamed name's shape" % r[2])
        low = r[2].lower()
        if any(low[i] == low[i + 1] == low[i + 2] and low[i].isalpha() for i in range(len(low) - 2)):
            out.append("4: %s has three equal letters" % r[2])
    # 5
    for e in k43:
        cs = causes.get(e[0], [])
        if len(cs) != 1:
            out.append("5: event %d has %d causes" % (e[0], len(cs)))
            continue
        c = ev[cs[0]]
        if not (c[0] < e[0] and c[5] == e[5] and kinds[c[3]] in rule_by_kind):
            out.append("5: event %d's cause %d is not an earlier event of a rule kind in its region" % (e[0], c[0]))
    return out


def max_weight(rule_by_kind, kinds, ev, cs):
    """The weight of the rule a renaming came from (read through its cause's kind; the largest weight of the
    rules that read that kind, which is the chosen rule's when only one qualifies)."""
    ws = [rule["weight"] for c in cs for rule in rule_by_kind.get(kinds[ev[c][3]], [])]
    return max(ws) if ws else None


def rule_of(res_w, row):
    """The rule key of a row from the reference's own log."""
    for y, r, key, e, nm, fi, namer in res_w.renaming.log:
        if (r, y) == (row[0], row[1]):
            return key, e
    return "?", 0


# ---------------------------------------------------------------------------
# Worlds and the golden file
# ---------------------------------------------------------------------------

def table_hash(rows):
    return pchr.table_hash(5, sorted(rows))


def rename_lines(name, res):
    w = res["_world"]
    rows = res["region_name"]
    o = ["%s region_name %d %s" % (name, len(rows), table_hash(rows))]
    for row in rows:
        key, e = rule_of(w, row)
        o.append("%s rename %d %d %s culture %d e%d cause e%d %s" % (name, row[0], row[1], key, row[3], row[4], e,
                                                                     row[2]))
    rn = w.renaming
    if rn is not None:
        by = {}
        for _y, _r, key, _e, why in rn.dropped:
            by[why] = by.get(why, 0) + 1
        o.append("%s rename_stats candidates %d taken %d dropped %s" % (
            name, len(rn.candidates), len(rn.log),
            ",".join("%s:%d" % kv for kv in sorted(by.items())) or "-"))
    return o


def rename_small_data(dat=None):
    """13 fixture rename-small: the stress data of p3_conflict_reference (wars, sacks and conquests on a small
    synthetic map) with every rule's min_magnitude 1 and chance_permille 1000. From 0.23.0 (P5-02) on the
    monsters-small edits too (p5_02_monsters_reference.monsters_small_data), so that rule 7 ravage can rename."""
    if r23():
        import p5_02_monsters_reference as p502
        d = p502.monsters_small_data(dat or pcr.data())
    else:
        d = pcr.stress_data(dat or pcr.data())
    d.renaming = __import__("copy").deepcopy((dat or pcr.data()).renaming)
    for rule in d.renaming["rules"]:
        rule["min_magnitude"] = 1
        rule["chance_permille"] = 1000
    # READING: the caps of 6 items 3 and 4 opened too (on a map of 2 to 60 regions they allow one or two
    # renamings, and the six rules could not all appear)
    d.renaming["world_max_permille"] = 1000
    d.renaming["century_max_permille"] = 1000
    return d


def off_data(dat=None):
    """13: region_max 0 (the subsystem off)."""
    d = pcr.edited(dat or pcr.data())
    d.renaming["region_max"] = 0
    return d


RENAME_SMALL_SEEDS = pcr.STRESS_SEEDS   # stress geographies (p3_conflict_reference stress cases' grid)


def fixture_case(seed, dat):
    span, races, civs, intensity = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
    inp, _g = pcr.synthetic_inputs(seed, dat, (span, races, civs, intensity))
    res = pcr.history_stage(dat, inp)
    return res


HEADER = """\
# P5-12 renaming golden values (docs/specs/p5-12-renamed-regions.md r0 sections 6, 7.2, 12, 13), generator
# 0.22.0 (schema 8), written by worldgen/tests/reference/p5_12_renaming_reference.py golden, an independent
# reference written from the spec (the subsystem runs inside p3_conflict_reference.py's replay). Do not edit by
# hand; never regenerate to make a C++ test pass.
#
# Format: ASCII, LF, tokens separated by one space.
#   world <case> <seed> <map size> <span> <races> <detail> <civs> <intensity>     a world case of p3-06-conflict.txt (the shipped
#       history of the world input, ~/.cache/the-unmapped/out/p5-12-tests/inputs)
#   fixture <case> <seed> <span> <races> <civs> <intensity> <data>   a synthetic geography (make_synthetic) with
#       data rename-small (every rule min_magnitude 1, chance_permille 1000), off (region_max 0) or stress
#   <case> region_name <rows> <table hash>        the table's hash as hash --detail computes it (P0-04 8)
#   <case> rename <region> <from_year> <rule key> culture <c> e<event> cause e<cause> <name>   one per row,
#       in primary-key order (the rule is the reference's record of the candidate taken)
#   <case> rename_stats candidates <n> taken <n> dropped <reason>:<n>,...|-   reasons region_max, gap, weight,
#       world, century, quiet, name (6 item 2)
#   rules_seen <rule key>,...     the rules that renamed at least once over the rename-small fixtures
"""


def golden_text(inputs):
    o = [HEADER.rstrip("\n")]
    dat = pcr.data()
    for name, seed, size, span, races, detail, civs, intensity in pcr.world_cases():
        res = pcr.world_case(name, seed, size, span, races, detail, inputs, dat, civs, intensity)
        o.append("world %s %d %d %d %s %s %s %s" % (name, seed, size, span, races, detail, civs, intensity))
        o += rename_lines(name, res)
    seen = set()
    small = rename_small_data(dat)
    for seed in RENAME_SMALL_SEEDS:
        res = fixture_case(seed, small)
        span, races, civs, intensity = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
        name = "rename-small-%d" % seed
        o.append("fixture %s %d %d %s %s %s rename-small" % (name, seed, span, races, civs, intensity))
        o += rename_lines(name, res)
        seen.update(x[2] for x in res["_world"].renaming.log)
    o.append("rules_seen %s" % ",".join(r["key"] for r in small.renaming["rules"] if r["key"] in seen))
    for seed in (0, 1):
        res = fixture_case(seed, off_data(dat))
        span, races, civs, intensity = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
        name = "off-%d" % seed
        o.append("fixture %s %d %d %s %s %s off" % (name, seed, span, races, civs, intensity))
        o += rename_lines(name, res)
    return "\n".join(o) + "\n"


def world_names_of(res):
    other = [c[1] for c in res["civilization"]] + [s[1] for s in res["settlement"]]
    return other


def check_res(c, label, res, dat):
    inp = res["_inputs"]
    geo = {r["id"]: r["name"] for r in inp.regions}
    other = world_names_of(res) + [a[4] for a in inp.ages] + [g[1] for g in inp.gods]
    for m in rows_violations(res, dat.renaming, geo, other):
        c.ok("%s invariant %s" % (label, m), False)
    w = res["_world"]
    rn = w.renaming
    for (y, r, key, e, nm, fi, namer), row in zip(sorted(rn.log, key=lambda x: (x[1], x[0])), res["region_name"]):
        rule = [x for x in rn.rules if x["key"] == key][0]
        stem = rn.stem_value(rule, rn.ev(e)) if r23() and "{stem}" in rule["forms"][fi] else None
        c.ok("%s 5.2 %s valid" % (label, nm), name_problem(nm, form_parts(rule["forms"][fi], rn.name_value(
            rule, rn.ev(e)) or "", rn.nouns[r], stem)) is None)
    # 5 (qualifies, from the rows): the cause's year and the window
    c.ok("%s every renaming in its cause's step" % label,
         all(row[1] >= w.events[x[3] - 1]["year"] for x, row in zip(sorted(rn.log, key=lambda x: (x[1], x[0])),
                                                                     res["region_name"])))


# ---------------------------------------------------------------------------
# 13: hand-made units
# ---------------------------------------------------------------------------

def check_units(c):
    dat = pcr.data()
    t = dat.renaming
    # 5.3 roots, every shipped culture's three forms
    for cid, d in sorted(dat.naming.items()):
        for f in d["civilization_forms"]:
            nm = f.replace("{root}", "Cear")
            c.eq("5.3 root %s (%s)" % (nm, cid), realm_root(nm, d["civilization_forms"]), "Cear")
    c.eq("5.3 Clan Uk'ghaam", realm_root("Clan Uk'ghaam", dat.naming["orcish"]["civilization_forms"]), "Uk'ghaam")
    c.eq("5.3 The Krurz Horde", realm_root("The Krurz Horde", dat.naming["orcish"]["civilization_forms"]), "Krurz")
    c.eq("5.3 no form matches", realm_root("Nowhere Realm", dat.naming["human"]["civilization_forms"]),
         "Nowhere Realm")
    c.eq("5.3 numbered fallback", realm_root("The Kingdom of Cear II", dat.naming["human"]["civilization_forms"]),
         "Cear II")
    c.eq("5.3 a name only the prefix (not longer)", realm_root("The Kingdom of ", ["The Kingdom of {root}"]),
         "The Kingdom of ")
    # 5.1 cycle and registry
    forms = t["rules"][5]["forms"]   # heartland
    for start in range(4):
        nm, fi = build_name(forms, start, "Nife", "Downs", set())
        c.eq("5.1 heartland start %d" % start, (nm, fi), ([
            "Nifeland", "The Heart of Nife", "The Nife Reach", "Nife's Hearth"][start], start))
    nm, fi = build_name(forms, 1, "Nife", "Downs", {"The Heart of Nife"})
    c.eq("5.1 a name in the registry skips to the next form", (nm, fi), ("The Nife Reach", 2))
    nm, fi = build_name(forms, 3, "Nife", "Downs", {"Nife's Hearth"})
    c.eq("5.1 the cycle wraps", (nm, fi), ("Nifeland", 0))
    nm, fi = build_name(forms, 0, "Nife", "Downs", {"Nifeland", "The Heart of Nife", "The Nife Reach",
                                                    "Nife's Hearth"})
    c.eq("5.1 every form taken: none", (nm, fi), (None, None))
    # V8 at a join. The spec's example ({name}land, a root ending tar, the entry tard) cannot span: tar + land
    # holds no tard. The shipped entry lol does: a root ending lo + land (and fas + 's: fass).
    nm, fi = build_name(forms, 0, "Thalo", "Downs", set())
    c.eq("5.2 V8 Thalo + land rejected (lol across the join), next form", (nm, fi), ("The Heart of Thalo", 1))
    nm, fi = build_name(["{name}'s Fall", "The Ruin of {name}"], 0, "Ufas", "Downs", set())
    if r23():   # P5-02 21.2: the possessive rule drops the s, so no fass across the join
        c.eq("5.2 V8 Ufas + 's Fall: Ufas' Fall at 0.23.0 (21.2's possessive)", (nm, fi), ("Ufas' Fall", 0))
    else:
        c.eq("5.2 V8 Ufas + 's Fall rejected (' removed: fass across the join)", (nm, fi), ("The Ruin of Ufas", 1))
    tr = []
    build_name(forms, 0, "Thalo", "Downs", set(), trace=tr)
    c.eq("5.2 V8 the rejection reason", tr[0][2], "V8")
    nm, fi = build_name(["{name}'s Fall"], 0, "Tardbury", "Downs", set())
    c.eq("5.2 V8 an entry inside a value is not tested", nm, "Tardbury's Fall")
    nm, fi = build_name(["{name}land"], 0, "Thalo", "Downs", set())
    c.eq("5.2 all forms invalid: none", nm, None)
    c.eq("5.2 V2 three equal letters", name_problem("Faaa Waste", ["Faaa", " Waste"]), "V2")
    c.eq("5.2 V6 49 bytes", name_problem("A" * 49, ["A" * 49]), "V6")
    c.eq("5.2 48 bytes valid", name_problem("Ab" * 24, ["Ab" * 24]), None)
    # {noun} by RG11 3 (rules a to d) on a hand-made row set
    class Inp:
        pass
    inp = Inp()
    inp.regions = [dict(id=1, cells=1000, lake_cells=0, walk_cells=900), dict(id=2, cells=100, lake_cells=0,
                   walk_cells=90), dict(id=3, cells=100, lake_cells=50, walk_cells=90),
                   dict(id=4, cells=100, lake_cells=0, walk_cells=20), dict(id=5, cells=100, lake_cells=0,
                   walk_cells=20)]
    defs = rg.shipped_defs()
    code = {k: cc for cc, k, _, _ in defs}
    inp.region_land = {1: (1, code["temperate_forest"]), 2: (2, code["grassland"]), 3: (1, code["grassland"]),
                       4: (1, code["temperate_forest"]), 5: (1, code["alpine"])}
    inp.region_biomes = []
    nn = land_nouns(inp)
    noun = {k: n for _, k, _, n in defs}
    c.eq("3 RG11 3 a/b/c/d nouns", nn, {1: noun["temperate_forest"], 2: "Isle", 3: "Mere", 4: "Fells", 5: noun["alpine"]})
    # the stage check (P3-01 r16 5.4) on the shipped rules and on edited ones
    cat = pe.event_catalogue(pe.spec_report())
    c.eq("stage check renaming_rules passes the shipped rules", stage_check_message(t["rules"], cat), None)
    import copy
    for edit, msg in (
            (lambda rs: rs[0].update(kinds=["nothing"]), "history: renaming rule 'conquest': kind 'nothing' is not "
                                                         "in the catalogue"),
            (lambda rs: rs[0].update(kinds=["tribute"]), "history: renaming rule 'conquest': kind 'tribute' has no "
                                                         "region"),
            (lambda rs: rs[2].update(values=["toll"]), "history: renaming rule 'great_sack': value 'toll' is not a "
                                                       "value of kind 'sacked'"),
            (lambda rs: rs[3].update(name_role="holder"), "history: renaming rule 'slaughter': name_role 'holder' is "
                                                          "not a civilization or settlement role of kind 'battle'"),
            (lambda rs: rs[0].update(name_role="settlement"), "history: renaming rule 'conquest': name_role "
                                                              "'settlement' is not a civilization or settlement role "
                                                              "of kind 'civ_conquered'"),
            (lambda rs: rs[1].update(namer_role="defenderx"), "history: renaming rule 'capital_sack': namer_role "
                                                              "'defenderx' is not a civilization role of kind "
                                                              "'sacked'"),
            (lambda rs: rs[3].update(capital=True), "history: renaming rule 'slaughter': capital needs a kind with a "
                                                    "settlement and a civilization role ('battle')")):
        rs = copy.deepcopy(t["rules"])
        edit(rs)
        c.eq("stage check: %s" % msg[-60:], stage_check_message(rs, cat), msg)
    # windows, chance key, choice, caps: on a hand-made world driven through Renaming with a stub World
    check_subsystem_units(c, dat)


class StubWorld:
    """A minimal World for the subsystem: events, civs, settlements, registry, draw, append, quiet budget."""

    def __init__(self, dat, regions=12, seed=7):
        self.dat = dat
        self.cat = pe.event_catalogue(pe.spec_report())
        self.cat_by_key = {k["key"]: k for k in self.cat}
        self.events = []
        self.civs = [dict(id=1, name="The Kingdom of Cear", culture=5, capital=1),
                     dict(id=2, name="The Krurz Horde", culture=6, capital=3)]
        self.settlements = [dict(id=1, name="Edadbury", region=1, founder_civ=1),
                            dict(id=2, name="Gol", region=2, founder_civ=2),
                            dict(id=3, name="Krurzpit", region=3, founder_civ=2)]
        self.registry = set()
        self.year = 0
        self.keys = {}
        self.seed = seed
        self.quiet_left = {}
        self.draws = []

        class Inp:
            pass
        inp = Inp()
        inp.seed = seed
        inp.regions = [dict(id=i, name="Region %d" % i, cells=100, lake_cells=0, walk_cells=90)
                       for i in range(1, regions + 1)]
        inp.region_land = {i: (1, rg.shipped_defs()[0][0]) for i in range(1, regions + 1)}
        inp.region_biomes = []
        inp.cultures = [(i + 1, k, k, 0) for i, k in enumerate(pnr.CULTURE_ORDER)]
        self.inp = inp

    def civ(self, c):
        return self.civs[c - 1]

    def stl(self, s):
        return self.settlements[s - 1]

    def draw(self, stream, a, b, k):
        self.draws.append((stream, self.year, a, b, k))
        return pnr.draw(self.keys[stream], self.year, a, b, k)

    def quiet_events_left(self, civ):
        return self.quiet_left.get(civ, 3)

    def append(self, key, year=None, end_year=None, region=0, settlement=0, participants=(), values=(), causes=(),
               salience=None):
        k = self.cat_by_key[key]
        parts = []
        for i, (role, t, x) in enumerate(participants):
            rs = [r for r in k["roles"] if r["role"] == role][0]
            parts.append((i + 1, rs["seq"], t, x))
        ev = dict(id=len(self.events) + 1, year=self.year if year is None else year, end_year=self.year,
                  kind=k["id"], region=region, settlement=settlement,
                  salience=k["salience"] if salience is None else salience, participants=parts,
                  values={i + 1: v for i, v in enumerate(values)}, causes=list(causes), key=key)
        self.events.append(ev)
        if key == "region_renamed" and getattr(self, "charge", False):
            self.quiet_left[parts[0][3] if parts else 0] = self.quiet_events_left(parts[0][3] if parts else 0) - 1
        return ev["id"]


def stub(dat, tuning=None, regions=12):
    w = StubWorld(dat, regions)
    rn = Renaming(w, dat, w.inp, tuning=tuning)
    return w, rn


def sack(w, region, settlement, dead, salience=75, attacker=2, defender=1):
    return w.append("sacked", region=region, settlement=settlement,
                    participants=(("attacker", CIV, attacker), ("defender", CIV, defender)), values=(dead, 0),
                    salience=salience)


def battle(w, region, wd, ld, winner=2, loser=1):
    return w.append("battle", region=region, participants=(("winner", CIV, winner), ("loser", CIV, loser)),
                    values=(wd, ld))


def tuned(dat, **kw):
    import copy
    t = copy.deepcopy(dat.renaming)
    for r in t["rules"]:
        r["chance_permille"] = 1000
    t.update({k: v for k, v in kw.items() if k != "rules"})
    return t


def check_subsystem_units(c, dat):
    # magnitude and window (slaughter: battles within 100 years, 5000)
    w, rn = stub(dat, tuned(dat))
    w.year = 1000
    battle(w, 4, 1000, 1500)
    rn.run(False)
    c.eq("13.1 one battle of 2500 does not qualify", rn.candidates, [])
    w.year = 1099
    battle(w, 4, 1000, 1500)
    rn.run(False)
    c.eq("13.1 2500 + 2500 within the window (Y - 99) qualifies", [x[2] for x in rn.candidates], ["slaughter"])
    w, rn = stub(dat, tuned(dat))
    w.year = 1000
    battle(w, 4, 1000, 1500)
    rn.run(False)
    w.year = 1100
    battle(w, 4, 1000, 1500)
    rn.run(False)
    c.eq("13.1 an entry at Y - window_years drops", rn.candidates, [])
    # min_salience (capital_sack 90) and great_sack's magnitude
    w, rn = stub(dat, tuned(dat))
    w.year = 500
    sack(w, 1, 1, 100, salience=89)
    rn.run(False)
    c.eq("13.1 salience 89 below capital_sack's 90", rn.candidates, [])
    w.year = 501
    sack(w, 1, 1, 100, salience=90)
    rn.run(False)
    c.eq("13.1 salience 90 qualifies for capital_sack", [x[2] for x in rn.candidates], ["capital_sack"])
    c.eq("13.4 the row: region 1, year 501, Edadbury's form, the attacker's culture", rn.rows[0][:2] + rn.rows[0][3:4],
         (1, 501, 6))
    ev = w.events[-1]
    c.eq("13.4 kind 43 event: region, settlement in r, namer, cause", (ev["key"], ev["region"], ev["settlement"],
                                                                      ev["participants"], ev["causes"]),
         ("region_renamed", 1, 1, [(1, 1, CIV, 2)], [2]))
    # the chance key: (e, r, rule number), w0 the step's year
    w, rn = stub(dat, tuned(dat))
    w.year = 700
    e = battle(w, 5, 3000, 3000)
    rn.run(False)
    ch = [d for d in w.draws if d[0] == "hist.renaming.chance"]
    c.eq("13.1 chance draw key (e, r, 4) at w0 = 700", ch, [("hist.renaming.chance", 700, e, 5, 4)])
    nmd = [d for d in w.draws if d[0] == "hist.renaming.name"]
    c.eq("13.3 name draw key (e, r, 0)", nmd, [("hist.renaming.name", 700, e, 5, 0)])
    # capital true and false (heartland): became_city at the capital qualifies, elsewhere not
    w, rn = stub(dat, tuned(dat))
    w.year = 300
    w.append("became_city", region=2, settlement=2, participants=(("settlement", SETTLEMENT, 2),
                                                                  ("civilization", CIV, 1)), values=(9000,))
    rn.run(False)
    c.eq("13.1 capital: not the capital", rn.candidates, [])
    w.append("became_city", region=1, settlement=1, participants=(("settlement", SETTLEMENT, 1),
                                                                  ("civilization", CIV, 1)), values=(9000,))
    w.year = 301
    rn.run(False)
    c.eq("13.1 capital: the capital (the step after)", [x[2] for x in rn.candidates], ["heartland"])
    # two candidates in one region: weight first
    w, rn = stub(dat, tuned(dat))
    w.year = 900
    sack(w, 1, 1, 2000, salience=75)     # great_sack (80)
    sack(w, 1, 1, 10, salience=90)       # capital_sack (90) on a later event
    rn.run(False)
    c.eq("13.2 two candidates: the larger weight", rn.log[0][2], "capital_sack")
    w, rn = stub(dat, tuned(dat))
    w.year = 900
    e1 = sack(w, 1, 1, 10, salience=90)
    sack(w, 1, 1, 10, salience=90)
    rn.run(False)
    c.eq("13.2 equal weight: the smaller event", rn.log[0][3], e1)
    # gap 299 refused, 300 taken; equal weight refused, larger taken; region_max (caps opened)
    w, rn = stub(dat, tuned(dat, world_max_permille=1000, century_max_permille=1000))
    w.year = 1000
    sack(w, 1, 1, 2000)
    rn.run(False)
    c.eq("13.2 first renaming great_sack", [x[2] for x in rn.log], ["great_sack"])
    w.year = 1299
    sack(w, 1, 1, 10, salience=90)
    rn.run(False)
    c.eq("13.2 gap 299 refused", rn.dropped[-1][4], "gap")
    w.year = 1300
    sack(w, 1, 1, 2000)
    rn.run(False)
    c.eq("13.2 gap 300, equal weight refused", rn.dropped[-1][4], "weight")
    w.year = 1301
    sack(w, 1, 1, 10, salience=90)
    rn.run(False)
    c.eq("13.2 gap 301, larger weight taken", [x[2] for x in rn.log], ["great_sack", "capital_sack"])
    w.year = 1700
    w.append("civ_conquered", region=1, participants=(("civilization", CIV, 1), ("conqueror", CIV, 2)))
    rn.run(False)
    c.eq("13.2 region_max 2 refuses a third", rn.dropped[-1][4], "region_max")
    # the world cap at R = 12 (1), 49 (7), 200 (30)
    for R, want in ((12, 1), (49, 7), (200, 30)):
        w, rn = stub(dat, tuned(dat), regions=R)
        c.eq("13.2 world cap at R = %d" % R, rn.cap(rn.t["world_max_permille"]), want)
    # the century cap (R = 12: 1) and its window
    w, rn = stub(dat, tuned(dat, world_max_permille=1000))
    w.year = 1000
    sack(w, 1, 1, 10, salience=90)
    rn.run(False)
    w.year = 1099
    sack(w, 3, 3, 10, salience=90, attacker=1, defender=2)
    rn.run(False)
    c.eq("13.2 century cap: a second within 99 years refused", rn.dropped[-1][4], "century")
    w.year = 1100
    sack(w, 3, 3, 10, salience=90, attacker=1, defender=2)
    rn.run(False)
    c.eq("13.2 century cap: 100 years later taken", len(rn.log), 2)
    # a quiet step whose namer has no quiet events left drops the candidate; windows kept after a drop
    w, rn = stub(dat, tuned(dat))
    w.year = 2000
    battle(w, 6, 2500, 2500)
    w.quiet_left[2] = 0
    rn.run(True)
    c.eq("13.2 quiet: namer without quiet events drops", rn.dropped[-1][4], "quiet")
    c.eq("13.2 windows kept after a drop", rn.win[3].get(6), [(2000, 5000)])
    w.year = 2001
    battle(w, 6, 50, 50)
    rn.run(False)
    c.eq("13.2 the kept window qualifies later", [x[2] for x in rn.log], ["slaughter"])
    c.eq("13.2 windows cleared after a renaming", [rn.win[i].get(6) for i in range(6)], [None] * 6)
    # the slaughter name: the loser's root; the noun
    c.ok("13.3 slaughter name uses the loser's root or the noun", any(x in rn.rows[-1][2] for x in ("Cear", rn.nouns[6])))
    c.ok("13.3 the new name enters the registry", rn.rows[-1][2] in w.registry)
    # namer absent: a rule without namer_role
    t2 = tuned(dat)
    t2["rules"][4]["namer_role"] = ""
    w, rn = stub(dat, t2)
    w.year = 50
    w.append("plague_outbreak", region=2, settlement=2, participants=(("settlement", SETTLEMENT, 2),
                                                                      ("civilization", CIV, 1)),
             values=(10, 100, 3500, 0))
    rn.run(False)
    c.eq("13.4 no namer: no participant, culture 0", (w.events[-1]["participants"], rn.rows[-1][3]), ([], 0))
    # settlement 0 when e's settlement lies outside r
    w, rn = stub(dat, tuned(dat))
    w.year = 60
    w.append("plague_outbreak", region=4, settlement=2, participants=(("settlement", SETTLEMENT, 2),
                                                                      ("civilization", CIV, 1)),
             values=(10, 100, 3500, 0))
    rn.run(False)
    c.eq("13.4 settlement outside r gives 0", w.events[-1]["settlement"], 0)


# ---------------------------------------------------------------------------
# check-world: invariants 1 to 6 on a world file
# ---------------------------------------------------------------------------

def read_world(path):
    conn = sqlite3.connect("file:%s?mode=ro" % path, uri=True)
    res = {}
    for t in ("event_kind", "event", "event_cause", "civilization", "settlement", "region_name"):
        res[t] = [tuple(r) for r in conn.execute('SELECT * FROM "%s" ORDER BY 1, 2' % t)]
    geo = {r[0]: r[1] for r in conn.execute("SELECT id, name FROM region ORDER BY id")}
    other = [r[0] for r in conn.execute("SELECT name FROM age")] + [r[0] for r in conn.execute("SELECT name FROM god")]
    meta = dict(conn.execute("SELECT key, value FROM meta"))
    conn.close()
    return res, geo, other, meta


def cmd_check_world(paths):
    bad = 0
    dat = pcr.data()
    for p in paths:
        res, geo, other, meta = read_world(p)
        other = other + [c[1] for c in res["civilization"]] + [s[1] for s in res["settlement"]]
        msgs = rows_violations(res, dat.renaming, geo, other)
        # 4: validity by 5.2 items 1 to 3 (V8 needs the parts: the reference checks it on its own rows)
        for r in res["region_name"]:
            p2 = name_problem(r[2], [r[2]])
            if p2 is not None:
                msgs.append("4: %s fails %s" % (r[2], p2))
        print("%s: %d region_name rows, %d problems" % (os.path.basename(p), len(res["region_name"]), len(msgs)))
        for m in msgs[:20]:
            print("  " + m)
        bad += len(msgs)
    return 1 if bad else 0


class Checker:
    def __init__(self):
        self.count = 0
        self.failures = 0

    def ok(self, what, cond):
        self.count += 1
        if not cond:
            self.failures += 1
            print("FAIL:", what)

    def eq(self, what, got, want):
        self.count += 1
        if got != want:
            self.failures += 1
            print("FAIL: %s: got %r, want %r" % (what, got, want))


def cmd_check(argv):
    c = Checker()
    check_units(c)
    dat = pcr.data()
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS
    small = rename_small_data(dat)
    seen = set()
    for seed in RENAME_SMALL_SEEDS:
        res = fixture_case(seed, small)
        check_res(c, "rename-small-%d" % seed, res, small)
        seen.update(x[2] for x in res["_world"].renaming.log)
    # READING: heartland needs a city at a capital, which no synthetic map of 2 to 60 regions grows; it renames in
    # the world cases (geo-phrase-512, hist-dense-1024, hist-conflict-73-1024), checked by the golden file
    c.eq("13 rename-small: every rule but heartland renames at least once", sorted(seen),
         sorted(r["key"] for r in small.renaming["rules"] if r["key"] != "heartland"))
    off = fixture_case(0, off_data(dat))
    on = fixture_case(0, dat)
    c.eq("13 region_max 0: no row", off["region_name"], [])
    c.eq("13 region_max 0: no kind 43", [e for e in off["event"] if e[3] == 43], [])
    if r23():   # P5-02: the world without the renaming subsystem is the 0.23.0 rules with rename off
        pcr.FLAGS["rename"] = False
        try:
            before = fixture_case(0, dat)
        finally:
            pcr.FLAGS["rename"] = True
    else:
        with pcr.rules("0.21.0"):
            before = fixture_case(0, dat)
    for t in ("event", "event_participant", "event_cause", "event_value", "civilization", "settlement",
              "settlement_census", "region_control", "relation", "phase"):
        c.eq("13 region_max 0: table %s equals the world without the subsystem" % t, off[t], before[t])
    c.ok("13 shipped data renames on synthetic 0 or not (no claim)", on is not None)
    if "--quick" not in argv:
        for name, seed, size, span, races, detail, civs, intensity in pcr.world_cases():
            res = pcr.world_case(name, seed, size, span, races, detail, inputs, dat, civs, intensity)
            check_res(c, name, res, dat)
    if os.path.exists(GOLDEN) and "--quick" not in argv:
        with open(GOLDEN, "rb") as f:
            c.eq("committed golden file is current", f.read().decode(), golden_text(inputs))
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    version = "0.23.0"   # P5-02: rule 7 ravage, {stem}, the possessive rule; --version 0.22.0 keeps P5-12's file
    if "--version" in argv:
        i = argv.index("--version")
        version = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    pcr.use_version(version)
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS
    if argv[0] == "check":
        return cmd_check(argv[1:])
    if argv[0] == "golden":
        rest = [a for a in argv[1:] if not a.startswith("--") and a != inputs]
        text = golden_text(inputs)
        path = rest[0] if rest else GOLDEN
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    if argv[0] == "check-world":
        return cmd_check_world(argv[1:])
    if argv[0] == "readings":
        print(READINGS)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
