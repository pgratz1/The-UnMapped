#!/usr/bin/env python3
"""Independent exact-integer reference of P3-02 names.

Written by the test writer from docs/specs/p3-02-names.md (section 4: the
keyed draws, one attempt, a name with the registry, the retry and the
numeral rule; section 5: the shipped naming data, transcribed here) and
P3-00 4.6, without reading any implementation and without the spec
writer's prototype. Hashing comes from core_reference.py (P0-01), Roman
numerals follow P1-06 RG11 4 (written out here again from that text).

Python 3 standard library only, arbitrary-precision integers, no floating
point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p3_names_reference.py check
        The worked example of 4.6 (stream key, every u(j), pick, symbol and
        root; the collision and fallback cases; the seed-42 vectors), the
        helper values of 9.1 item 2, the capitalization examples of 4.2 and
        9.1 item 5, the grammars and named checks of section 3 on the
        shipped data, a second method for pick and weighted, and the
        invariants of section 6 on the golden lists. Exit status 1 on any
        disagreement.

    python3 -B worldgen/tests/reference/p3_names_reference.py golden [PATH|-]
        Write worldgen/tests/golden/p3-02-names.txt ("-": standard output).

    python3 -B worldgen/tests/reference/p3_names_reference.py samples
        Print the names of the sample protocol of spec 5.1 (seed 0, one
        registry per culture: settlements 1-20, civilizations 1-5, gods 1-3)
        and compare them with the table of 5.1 transcribed below.

    python3 -B worldgen/tests/reference/p3_names_reference.py sweep [COUNT]
        Property sweep of 9.2 item 8: per shipped culture and kind, COUNT
        (default 2000) names into one registry, seeds 0 and 1; prints the
        fallback count and the attempt histogram's maximum.
"""

import hashlib
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402

FatalError = cr.FatalError
ONE = 1 << 32
HALF = 1 << 31
M64 = (1 << 64) - 1


def fatal(msg):
    raise FatalError(msg)


# ---------------------------------------------------------------------------
# 4.1 Keyed draws
# ---------------------------------------------------------------------------

TAG_HIST = 0x545349483A4D4E55
assert TAG_HIST == cr.tag_value("UNM:HIST")

STREAMS = {
    "god": "hist.names.god",
    "civilization": "hist.names.civilization",
    "settlement": "hist.names.settlement",
}
KINDS = ("god", "civilization", "settlement")  # 4.5 table order = enum order of 9


def draw(key, w0, a, b, k):
    """draw with the stream key already computed (the seam of 4.1)."""
    return cr.hash_words(TAG_HIST, [cr.u64(key), cr.u64(w0), cr.u64(a), cr.u64(b), cr.u64(k)])


def draw_named(seed, name, w0, a, b, k):
    return draw(cr.stream_key(seed, name), w0, a, b, k)


def pick(u, n):
    if n < 1 or n > (1 << 32):
        fatal("pick: n outside [1, 2^32]")
    return ((u >> 32) * n) >> 32


def chance(u, p_raw):
    if p_raw < 0 or p_raw > ONE:
        fatal("chance: p outside [0, ONE]")
    return (u >> 32) < p_raw


def weighted(u, w):
    for x in w:
        if x < 0:
            fatal("weighted: negative weight")
    if len(w) == 0:
        fatal("weighted: empty list")
    total = 0
    for x in w:
        total += x
        if total > (1 << 63) - 1:  # 4.1: the sum is formed with overflow checking (P3-00 H6)
            fatal("weighted: sum above 2^63 - 1")
    if total < 1:
        fatal("weighted: weights sum to 0")
    h = u >> 32
    acc = 0
    for i, x in enumerate(w):
        acc += x
        if h * total < (acc << 32):
            return i
    raise AssertionError("unreachable: the last prefix is W > h*W/2^32")


def weighted_p3_00(u, w):
    """Second method: P3-00 4.6's wording, the first index whose prefix sum
    exceeds (u >> 32) * W >> 32."""
    t = ((u >> 32) * sum(w)) >> 32
    acc = 0
    for i, x in enumerate(w):
        acc += x
        if acc > t:
            return i
    raise AssertionError("unreachable")


# ---------------------------------------------------------------------------
# Section 3: naming data, grammars, checks
# ---------------------------------------------------------------------------

FIELDS = ("onsets", "nuclei", "codas", "patterns", "min_letters", "max_letters", "forbidden",
          "god_forms", "civilization_forms", "settlement_forms")


def naming(onsets, nuclei, codas, patterns, min_letters, max_letters, forbidden, god_forms, civilization_forms,
           settlement_forms):
    return {
        "onsets": list(onsets), "nuclei": list(nuclei), "codas": list(codas), "patterns": list(patterns),
        "min_letters": min_letters, "max_letters": max_letters, "forbidden": list(forbidden),
        "god_forms": list(god_forms), "civilization_forms": list(civilization_forms),
        "settlement_forms": list(settlement_forms),
    }


def is_syllable(s):
    return len(s) <= 8 and all("a" <= ch <= "z" for ch in s)


SEP = ("'", "-")


def is_name_pattern(s):
    if not (1 <= len(s) <= 16):
        return False
    if any(ch not in "ONC'-" for ch in s):
        return False
    if "N" not in s:
        return False
    if s[0] in SEP or s[-1] in SEP:
        return False
    for i in range(len(s) - 1):
        if s[i] in SEP and s[i + 1] in SEP:
            return False
    return True


def is_name_form(s):
    if not (1 <= len(s) <= 48):
        return False
    if s.count("{root}") != 1:
        return False
    rest = s.replace("{root}", "", 1)
    for ch in rest:
        if not ("a" <= ch <= "z" or "A" <= ch <= "Z" or ch in " '-"):
            return False
    if s[0] == " " or s[-1] == " " or "  " in s:
        return False
    return True


def validate(d):
    """Grammars and array bounds of section 3, then the named checks in
    order; returns a list of messages (empty: valid)."""
    errs = []
    for f, lo, hi, ok in (("onsets", 1, 64, is_syllable), ("nuclei", 1, 64, is_syllable),
                          ("codas", 1, 64, is_syllable), ("patterns", 1, 16, is_name_pattern),
                          ("forbidden", 0, 32, is_syllable), ("god_forms", 1, 8, is_name_form),
                          ("civilization_forms", 1, 8, is_name_form), ("settlement_forms", 1, 8, is_name_form)):
        if not (lo <= len(d[f]) <= hi):
            errs.append("%s: array length %d outside [%d, %d]" % (f, len(d[f]), lo, hi))
        for i, s in enumerate(d[f]):
            if not ok(s):
                errs.append("%s[%d]: grammar" % (f, i))
    for f in ("min_letters", "max_letters"):
        if not (1 <= d[f] <= 32):
            errs.append("%s: outside [1, 32]" % f)
    for i, s in enumerate(d["nuclei"]):
        if s == "":
            errs.append("naming.nuclei[%d] must not be empty" % i)
    if d["min_letters"] > d["max_letters"]:
        errs.append("naming.min_letters must not be above naming.max_letters")
    for i, s in enumerate(d["forbidden"]):
        if s == "":
            errs.append("naming.forbidden[%d] must not be empty" % i)
    return errs


# Section 5, transcribed.
CULTURES = {
    "dwarven": naming(
        ["d", "b", "th", "k", "g", "br", "dr", "gr", "t", "h", "kh", "n", "m", "thr"],
        ["a", "u", "o", "i", "ai", "ou"],
        ["r", "n", "m", "k", "d", "rn", "rd", "rk", "mm", "nd", "rin", "li", "din"],
        ["ONC", "ONCONC", "ONCONC", "ONCON"],
        4, 10,
        ["mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk"],
        ["{root}"],
        ["The Hold of {root}", "Clan {root}", "The {root} Throne"],
        ["{root}", "{root} Deep", "{root} Hold", "{root}"]),
    "elven": naming(
        ["", "l", "th", "s", "f", "n", "m", "r", "v", "gl", "c", "el", "ith"],
        ["a", "e", "i", "ae", "ie", "o"],
        ["", "", "l", "n", "th", "s", "r", "nd", "ll", "nn"],
        ["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"],
        4, 11,
        ["aea", "iei", "iae", "eae", "aa", "ii", "ee", "thth", "lll", "nnn", "ea", "ia"],
        ["{root}"],
        ["The Court of {root}", "The Realm of {root}", "{root}"],
        ["{root}", "{root}", "{root}", "{root}dell"]),
    "faerie": naming(
        ["", "p", "f", "w", "l", "s", "t", "b", "sh", "wh", "fl", "tw", "m"],
        ["i", "ee", "ie", "ia", "a", "y", "ae", "ou", "o"],
        ["", "", "ll", "n", "sh", "th", "p", "sk", "ss"],
        ["ONON", "ONONON", "ONC", "ONONC", "ONCON"],
        3, 10,
        ["eee", "iee", "yy", "ouou", "shsh"],
        ["{root}"],
        ["The {root} Ring", "The Circle of {root}", "The {root} Court"],
        ["{root}", "{root} Glade", "{root} Hollow", "{root}"]),
    "goblin": naming(
        ["g", "gr", "sn", "k", "kr", "z", "sk", "b", "n", "gn", "dr", "sl", "skr"],
        ["a", "i", "u", "o", "ee"],
        ["k", "g", "b", "z", "t", "x", "nk", "rk", "gg", "zz", "tch"],
        ["ONC", "ONC", "ONCONC", "ONC-ONC", "ONONC"],
        3, 10,
        ["eee", "ggg", "zzz", "xg", "xk"],
        ["{root}"],
        ["The {root} Horde", "The Warrens of {root}", "The {root} Mob"],
        ["{root}", "{root}", "{root} Pit", "{root} Hole"]),
    "human": naming(
        ["b", "c", "d", "g", "h", "m", "r", "s", "t", "w", "st", "br", "al", "ed", "ash"],
        ["a", "e", "i", "o", "u", "ea"],
        ["n", "d", "r", "s", "l", "rd", "ck", "ld", "m", "nt"],
        ["ONC", "ONC", "ONCONC", "ONC"],
        4, 9,
        ["eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm"],
        ["{root}"],
        ["The Kingdom of {root}", "The {root} League", "The Duchy of {root}"],
        ["{root}", "{root}ford", "{root}ton", "{root}bury", "{root}wick", "{root}ham", "{root}stead"]),
    "orcish": naming(
        ["gh", "gr", "ug", "m", "k", "z", "sh", "b", "d", "th", "kr", "ur", "az"],
        ["a", "u", "o", "aa", "uu"],
        ["g", "sh", "k", "rz", "z", "gh", "rk", "l", "m", "", "kh", "ld"],
        ["ONC", "ONC", "ONC'ONC", "ONCONC", "NC'ONC"],
        3, 10,
        ["aaa", "uuu", "ghgh", "shsh"],
        ["{root}"],
        ["The {root} Horde", "Clan {root}", "The Warband of {root}"],
        ["{root}", "{root} Camp", "{root}", "{root} Fort"]),
}
CULTURE_ORDER = ("dwarven", "elven", "faerie", "goblin", "human", "orcish")  # canonical id order, c = 1..6
CULTURE_NUMBER = {cid: i + 1 for i, cid in enumerate(CULTURE_ORDER)}

# Test cultures shared with the C++ tests (worldgen/tests/history/p3_02_names_support.hpp).
# "tiny": exactly two valid names of any kind, "Ba" and "Da".
TINY = naming(["b", "d"], ["a"], [""], ["ON"], 2, 2, [], ["{root}"], ["{root}"], ["{root}"])
# "never": a one-letter root can never reach min_letters 30.
NEVER = naming(["b"], ["a"], [""], ["N"], 30, 32, [], ["{root}"], ["{root}"], ["{root}"])
TEST_CULTURES = {"tiny": TINY, "never": NEVER}


# ---------------------------------------------------------------------------
# 4.2 One attempt, 4.3 a name
# ---------------------------------------------------------------------------

def cap(root):
    out = []
    for i, ch in enumerate(root):
        if "a" <= ch <= "z" and (i == 0 or root[i - 1] == "-"):
            out.append(chr(ord(ch) - 32))
        else:
            out.append(ch)
    return "".join(out)


def root_valid(d, root):
    letters = sum(1 for ch in root if "a" <= ch <= "z")
    if not (d["min_letters"] <= letters <= d["max_letters"]):
        return False
    for f in d["forbidden"]:
        if f in root:
            return False
    if root and (root[0] in SEP or root[-1] in SEP):
        return False
    for i in range(len(root) - 1):
        if root[i] in SEP and root[i + 1] in SEP:
            return False
    return True


def attempt(d, kind, seed, c, e, t, trace=None):
    """(valid, name) of 4.2; trace, if a list, receives (j, u, n, index, symbol, piece)."""
    s_key = cr.stream_key(seed, STREAMS[kind])
    forms = d[kind + "_forms"]

    def u(j):
        return draw(s_key, t, c, e, j)

    u0 = u(0)
    fi = pick(u0, len(forms))
    form = forms[fi]
    if trace is not None:
        trace.append((0, u0, len(forms), fi, "form", form))
    u1 = u(1)
    pi = pick(u1, len(d["patterns"]))
    pattern = d["patterns"][pi]
    if trace is not None:
        trace.append((1, u1, len(d["patterns"]), pi, "pattern", pattern))
    root = ""
    j = 2
    for s in pattern:
        if s in "ONC":
            arr = d[{"O": "onsets", "N": "nuclei", "C": "codas"}[s]]
            uj = u(j)
            i = pick(uj, len(arr))
            root += arr[i]
            if trace is not None:
                trace.append((j, uj, len(arr), i, s, arr[i]))
            j += 1
        else:
            root += s
    valid = root_valid(d, root)
    name = form.replace("{root}", cap(root), 1)
    valid = valid and len(name) <= 48
    return valid, name, root


ROMAN = ((1000, "M"), (900, "CM"), (500, "D"), (400, "CD"), (100, "C"), (90, "XC"), (50, "L"), (40, "XL"),
         (10, "X"), (9, "IX"), (5, "V"), (4, "IV"), (1, "I"))


def roman(k):
    if k < 1 or k > 100000:
        fatal("roman: k outside [1, 100000]")
    out = []
    for v, s in ROMAN:
        while k >= v:
            out.append(s)
            k -= v
    return "".join(out)


def make_name(d, culture_id, kind, seed, c, e, registry):
    """4.3; returns (name, attempt number or 'F'); adds the name to registry (a set)."""
    first = None
    for t in range(64):
        valid, n, _ = attempt(d, kind, seed, c, e, t)
        if not valid:
            continue
        if first is None:
            first = n
        if n not in registry:
            registry.add(n)
            return n, t
    if first is None:
        fatal("names: culture '%s' produced no valid %s name in 64 attempts" % (culture_id, kind))
    k = 2
    while first + " " + roman(k) in registry:
        k += 1
    n = first + " " + roman(k)
    if len(n) > 64:
        fatal("names: a name longer than 64 bytes")
    registry.add(n)
    return n, "F"


def name_shape_ok(d, kind, name):
    """Invariant 1: characters, length, and form text + root within bounds,
    optionally followed by a space and a Roman numeral >= II."""
    if len(name) > 64:
        return False
    if any(not ("a" <= ch <= "z" or "A" <= ch <= "Z" or ch in " '-") for ch in name):
        return False
    candidates = [name]
    if " " in name:
        head, tail = name.rsplit(" ", 1)
        for k in range(2, 4000):
            if roman(k) == tail:
                candidates.append(head)
                break
    for cand in candidates:
        for form in d[kind + "_forms"]:
            pre, post = form.split("{root}")
            if len(cand) >= len(pre) + len(post) and cand.startswith(pre) and cand.endswith(post):
                root = cand[len(pre):len(cand) - len(post)] if post else cand[len(pre):]
                lower = root.lower()
                if root and cap(lower) == root and root_valid(d, lower):
                    return True
    return False


# ---------------------------------------------------------------------------
# Golden vectors
# ---------------------------------------------------------------------------

GOLDEN_SEEDS = (0, 42)
GOLDEN_ENTITIES = 50

# draw tuples: (seed, stream name, w0, a, b, k); signed words as two's complement.
DRAW_TUPLES = [
    (0, "hist.names.settlement", 0, 2, 1, 0),
    (0, "hist.names.settlement", 0, 2, 1, 1),
    (0, "hist.names.settlement", 0, 2, 1, 6),
    (0, "hist.names.settlement", 1, 2, 1, 0),
    (0, "hist.names.god", 0, 0, 0, 0),
    (0, "hist.names.god", 63, 6, 2000, 17),
    (0, "hist.names.civilization", 0, 1, 1, 0),
    (1, "hist.names.civilization", 5, 3, 9, 2),
    (42, "hist.names.god", 0, 1, 1, 0),
    (42, "hist.names.civilization", 1, 4, 3, 0),
    (42, "hist.names.settlement", 0, 5, 17, 3),
    (42, "hist.names.settlement", 0, 6, 200, 1),
    (M64, "hist.names.god", M64, M64, M64, M64),
    (12345678901234567890, "hist.names.settlement", -1, 0, 0, 0),
    (7, "hist.names.civilization", 6000, -5, 1 << 40, 1 << 63),
    (0x0123456789ABCDEF, "hist.names.god", 1, 2, 3, 4),
]

PICK_CASES = [(0, 1), (M64, 1), (0, 6), (M64, 6), (1 << 63, 2), ((1 << 63) - 1, 2), (M64, 1 << 32),
              (0x0000000100000000, 1 << 32), (0xAB9A949FA699954C, 4), (0xF77E9ED830C1A5E0, 5), (0x42F8D74C06F27F04, 13),
              (0x109285B9E64CA453, 6), (0xFFFFFFFF00000000, 7), (0x00000000FFFFFFFF, 7), ((3 << 32) - 1, 3)]
PICK_FATAL = [(0, 0), (M64, (1 << 32) + 1)]

CHANCE_CASES = [(0, 0), (M64, 0), (0, ONE), (M64, ONE), ((HALF - 1) << 32, HALF), (HALF << 32, HALF),
                (((HALF - 1) << 32) | 0xFFFFFFFF, HALF), (0, 1), (1 << 32, 1), (M64, ONE - 1), ((ONE - 2) << 32, ONE - 1)]
CHANCE_FATAL = [(0, -1), (0, ONE + 1)]

# weights (0, 3, 1), W = 4: index 1 for h < 3*2^30, index 2 above.
WEIGHTED_CASES = [
    (0, (0, 3, 1)), ((3 << 30) - 1 << 32, (0, 3, 1)), (((3 << 30) - 1 << 32) | 0xFFFFFFFF, (0, 3, 1)),
    ((3 << 30) << 32, (0, 3, 1)), (M64, (0, 3, 1)),
    (0, (5,)), (M64, (5,)), (0, (0, 0, 7, 0)), (M64, (0, 0, 7, 0)), (M64, (1, 0)), (0, (0, 1)),
    (1 << 63, (1, 1)), ((1 << 63) - 1, (1, 1)), (M64, (1 << 62, (1 << 62) - 2, 1)), (0, (1 << 62, (1 << 62) - 2, 1)),
    (M64, ((1 << 63) - 1,)), (M64, ((1 << 63) - 1, 0)), (0, (1, 1 << 62)), (M64, (1, 1 << 62)),
    (1 << 63, (1 << 31, 1, (1 << 32) - (1 << 31) - 1)), ((1 << 63) - 1, (1 << 31, 1, (1 << 32) - (1 << 31) - 1)),
    (0x8000000100000000, (1, 1, 1)), (0x5555555500000000, (1, 1, 1)), (0x5555555600000000, (1, 1, 1)),
]
WEIGHTED_FATAL = [(0, ()), (0, (0,)), (0, (0, 0, 0)), (0, (1, -1, 3)), (M64, (1 << 62, 1 << 62, 1)),
                  (0, (1 << 62, 1 << 62)), (0, ((1 << 63) - 1, 1))]

CAP_CASES = ["gnink-snunk", "uruld'duuld", "iensath", "a", "a-b-c", "ab'cd-ef", "x-y'z"]

# Attempts with a trace for the worked example: (seed, culture, kind, entity, t).
ATTEMPT_CASES = [(0, "elven", "settlement", 1, t) for t in range(4)] + [
    (42, "dwarven", "god", 1, 0), (42, "goblin", "civilization", 3, 0), (42, "goblin", "civilization", 3, 1),
    (42, "human", "settlement", 17, 0), (42, "orcish", "settlement", 200, 0),
]

# Registry scenarios (seed, culture, kind, preload, [entities...]); one registry per scenario.
def scenarios():
    out = []
    # 4.6: empty, Iensath pre-loaded, all 64 attempts' valid names pre-loaded.
    out.append(("empty", 0, "elven", "settlement", [], [1]))
    out.append(("collide", 0, "elven", "settlement", ["Iensath"], [1]))
    full = []
    for t in range(64):
        v, n, _ = attempt(CULTURES["elven"], "settlement", 0, 2, 1, t)
        if v and n not in full:
            full.append(n)
    out.append(("full", 0, "elven", "settlement", sorted(full), [1]))
    out.append(("full_and_ii", 0, "elven", "settlement", sorted(full + ["Iensath II"]), [1]))
    # 4.6 seed-42 vectors, one registry, in order.
    out.append(("seed42", 42, None, None, [], [("dwarven", "god", 1), ("goblin", "civilization", 3),
                                               ("human", "settlement", 17), ("orcish", "settlement", 200)]))
    # tiny culture: two names, then numerals; then other kinds sharing the registry.
    out.append(("tiny", 0, "tiny", "settlement", [], list(range(1, 9))))
    out.append(("tiny_kinds", 5, None, None, ["Ba"], [("tiny", "god", 1), ("tiny", "civilization", 1),
                                                       ("tiny", "settlement", 1), ("tiny", "god", 2),
                                                       ("tiny", "settlement", 2)]))
    # region names pre-loaded (the P3-03 caller's order), then a shipped culture.
    out.append(("regions", 0, "human", "settlement", ["Central Downs", "East Wood", "East Wood II"], [1, 2, 3]))
    return out


def run_scenario(sc):
    label, seed, cid, kind, preload, items = sc
    reg = set(preload)
    res = []
    for it in items:
        if isinstance(it, tuple):
            icid, ikind, e = it
        else:
            icid, ikind, e = cid, kind, it
        d = CULTURES.get(icid) or TEST_CULTURES[icid]
        c = CULTURE_NUMBER.get(icid, 1)
        n, t = make_name(d, icid, ikind, seed, c, e, reg)
        res.append((icid, ikind, c, e, t, n))
    return res


def name_list(seed):
    reg = set()
    lines = []
    for cid in CULTURE_ORDER:
        for kind in KINDS:
            for e in range(1, GOLDEN_ENTITIES + 1):
                n, t = make_name(CULTURES[cid], cid, kind, seed, CULTURE_NUMBER[cid], e, reg)
                lines.append("%d %s %s %d %s %s" % (seed, cid, kind, e, t, n))
    return lines


def hx(v):
    return "0x%016x" % v


def golden_text():
    o = []
    o.append("# P3-02 names golden vectors (spec docs/specs/p3-02-names.md, approved 2026-11-19),")
    o.append("# generated by worldgen/tests/reference/p3_names_reference.py golden, an independent")
    o.append("# exact-integer reference written from the spec text alone (hashing: core_reference.py).")
    o.append("# Do not edit by hand; never regenerate to make a C++ test pass.")
    o.append("#")
    o.append("# Format: ASCII, LF, tokens separated by one space; a name is the rest of its line.")
    o.append("#   tag TAG_HIST <hex>")
    o.append("#   stream <seed> <name> <key hex>                   stream_key(seed, name)")
    o.append("#   draw <key hex> <w0 hex> <a hex> <b hex> <k hex> <u hex>   draw(key, w0, a, b, k)")
    o.append("#   pick <u hex> <n> <index|FATAL>")
    o.append("#   chance <u hex> <p raw> <true|false|FATAL>")
    o.append("#   weighted <u hex> <w,w,...|-> <index|FATAL>            (- is the empty list)")
    o.append("#   roman <k> <numeral|FATAL>")
    o.append("#   cap <root> <Cap(root)>")
    o.append("#   attempt <seed> <culture> <kind> <c> <e> <t> <valid 0|1> <root> <name>  (- - when invalid:")
    o.append("#       an invalid attempt's root and name are unspecified, spec 4.2)")
    o.append("#   step <seed> <culture> <kind> <c> <e> <t> <j> <u hex> <n> <index> <symbol> <piece|->")
    o.append("#       every draw of the attempt (symbol form, pattern, O, N, C)")
    o.append("#   scenario <label> <seed> <preload count> then preload <label> <name> lines (sorted),")
    o.append("#   then result <label> <culture> <kind> <c> <e> <attempt|F> <name>, in naming order;")
    o.append("#       one registry per scenario; test cultures tiny and never are defined in the reference")
    o.append("#       and in worldgen/tests/history/p3_02_names_support.hpp")
    o.append("#   fatal <culture> <kind> <seed> <c> <e> <message>")
    o.append("#   <seed> <culture> <kind> <entity> <attempt|F> <name>   (spec 9.3 item 10)")
    o.append("#       for seeds 0 and 42, one registry per seed starting empty, cultures in canonical id")
    o.append("#       order (c = 1..6), kinds in the order god, civilization, settlement, entities 1..50")
    o.append("#   sha256 <seed> <hex>  of the seed's name lines joined by LF (no trailing LF)")
    o.append("# Where 4.6 falls: its seed-0 elven settlement 1 tuple is the line '0 elven settlement 1 0")
    o.append("# Iensath' of the list (the registry then holds 250 earlier names,")
    o.append("# dwarven 150, elven god and civilization 100, none of them Iensath);")
    o.append("# its empty-registry, collision and fallback cases are scenarios empty, collide and full;")
    o.append("# its seed-42 vectors are scenario seed42.")
    o.append("tag TAG_HIST %s" % hx(TAG_HIST))
    for name in sorted(set(STREAMS.values())):
        for seed in (0, 1, 42):
            o.append("stream %d %s %s" % (seed, name, hx(cr.stream_key(seed, name))))
    for seed, name, w0, a, b, k in DRAW_TUPLES:
        key = cr.stream_key(cr.u64(seed), name)
        o.append("draw %s %s %s %s %s %s" % (hx(key), hx(cr.u64(w0)), hx(cr.u64(a)), hx(cr.u64(b)), hx(cr.u64(k)),
                                             hx(draw(key, w0, a, b, k))))
    for u, n in PICK_CASES:
        o.append("pick %s %d %d" % (hx(u), n, pick(u, n)))
    for u, n in PICK_FATAL:
        o.append("pick %s %d FATAL" % (hx(u), n))
    for u, p in CHANCE_CASES:
        o.append("chance %s %d %s" % (hx(u), p, "true" if chance(u, p) else "false"))
    for u, p in CHANCE_FATAL:
        o.append("chance %s %d FATAL" % (hx(u), p))

    def wl(w):
        return ",".join(str(x) for x in w) if w else "-"
    for u, w in WEIGHTED_CASES:
        o.append("weighted %s %s %d" % (hx(u), wl(w), weighted(u, w)))
    for u, w in WEIGHTED_FATAL:
        o.append("weighted %s %s FATAL" % (hx(u), wl(w)))
    for k in (1, 2, 3, 4, 5, 9, 14, 40, 49, 90, 400, 1994, 3888, 3999, 4000, 4888, 100000):
        o.append("roman %d %s" % (k, roman(k)))
    for k in (0, 100001):
        o.append("roman %d FATAL" % k)
    for r in CAP_CASES:
        o.append("cap %s %s" % (r, cap(r)))
    for seed, cid, kind, e, t in ATTEMPT_CASES:
        c = CULTURE_NUMBER[cid]
        trace = []
        v, n, root = attempt(CULTURES[cid], kind, seed, c, e, t, trace)
        o.append("attempt %d %s %s %d %d %d %d %s %s" % (seed, cid, kind, c, e, t, 1 if v else 0, (root or "-") if v else "-", n if v else "-"))
        for j, uj, nn, idx, sym, piece in trace:
            if sym in ("form", "pattern"):
                piece = piece.replace(" ", "_")
            o.append("step %d %s %s %d %d %d %d %s %d %d %s %s" % (seed, cid, kind, c, e, t, j, hx(uj), nn, idx, sym,
                                                                   piece or "-"))
    for sc in scenarios():
        label, seed, _, _, preload, _ = sc
        o.append("scenario %s %d %d" % (label, seed, len(preload)))
        for p in sorted(preload):
            o.append("preload %s %s" % (label, p))
        for icid, ikind, c, e, t, n in run_scenario(sc):
            o.append("result %s %s %s %d %d %s %s" % (label, icid, ikind, c, e, t, n))
    for kind in KINDS:
        try:
            make_name(NEVER, "never", kind, 0, 1, 1, set())
            raise AssertionError("never culture produced a name")
        except FatalError as ex:
            o.append("fatal never %s 0 1 1 %s" % (kind, ex))
    for seed in GOLDEN_SEEDS:
        lines = name_list(seed)
        o.extend(lines)
        o.append("sha256 %d %s" % (seed, hashlib.sha256("\n".join(lines).encode("ascii")).hexdigest()))
    return "\n".join(o) + "\n"


# ---------------------------------------------------------------------------
# 5.1 sample table, transcribed from the spec
# ---------------------------------------------------------------------------

SAMPLES = {
    "dwarven": ("Moumka, Thoumdaili, Makmoudin Deep, Brummthu, Gromka, Kaird Deep, Khaili Hold, Kili, Thoum Hold, "
                "Nodin, Gondhark, Turnbruk Deep, Kidinkorn, Tharndramm Hold, Grangrak, Houmm Deep, Doknirk, Gali, "
                "Nailimim Deep, Tairdbra Hold",
                "The Kourdbrar Throne; The Hold of Didin; Clan Durgarin; Clan Drourd; Clan Drailimai",
                "Brommgrai, Groumtoun, Nurnthaimm"),
    "elven": ("Iensath, Onaeme, Ierrael, Elonoma, Fievos, Mielae, Asvathdell, Lafin, Laevo, Leithiedell, Orith, "
              "Voglael, Mievife, Iefann, Cellve, Fimo, Londcae, Rienel, Nathol, Siendnie",
              "Nife; The Court of Nothind; Aesoth; Riefifie; The Court of Thiesaell",
              "Saene, Fiendse, Ithonnfa"),
    "faerie": ("Liapa, Wiatwiesk Glade, Twamae Glade, Lip, Biafloss Glade, Pieshaep Hollow, Teelan, Meeie Hollow, "
               "Sywysh Hollow, Iatysk, Wiepysk, Miemo Hollow, Mapoufee, Feetop Hollow, Showhie Hollow, "
               "Mawhiap Hollow, Athfli Hollow, Whywo, Fliaposk Hollow, Whosy Hollow",
               "The Lyp Ring; The Watiafia Court; The Bouss Ring; The Wosk Ring; The Twiwhiwou Ring",
               "Fiwhall, Fiabo, Biensho"),
    "goblin": ("Krot Pit, Slograb Pit, Gnink-Snunk, Zux, Grunkgnuzz Pit, Grizz-Skeeb, Slank-Skrox, Drobskeet, "
               "Bag Hole, Sluggatch Hole, Bux-Kreeb, Sniz, Slak, Zug-Gok, Gunitch, Slagreeg Hole, Droggkazz Hole, "
               "Slobeerk, Snotgab, Kruskreezz Hole",
               "The Gnazz Horde; The Warrens of Bonk-Gitch; The Grodratch Horde; The Krunkgirk Horde; "
               "The Warrens of Gnonk",
               "Zabskreeb, Kruskux, Bazz"),
    "human": ("Deanham, Alantton, Cickham, Gackwick, Brir, Tackbury, Gurdton, Studford, Ceambury, Buld, "
              "Silstudstead, Tirdwick, Dack, Stastint, Moldstead, Edumford, Tontstead, Ceanbir, Durdton, Durdbury",
              "The Duchy of Ashem; The Kingdom of Ashos; The Duchy of Hick; The Duchy of Wick; The Duchy of Circon",
              "Simbear, Histold, Edostold"),
    "orcish": ("Kaak Fort, Zaarz Camp, Grush, Zaakugaag, Uruld'duuld, Uu'mold Fort, Kark Fort, Azol'urash Fort, "
               "Karzthogh, Uromgha, Maag, Uul'krakh Camp, Og'buurk Camp, Zaalduruz Camp, Mak, Duld'sha Fort, "
               "Shuum Fort, Ghorz Fort, Duuldkrosh Fort, Aag'urog Fort",
               "Clan Kram'krorz; The Azakhdam Horde; The Urogh'krorz Horde; The Burk'zork Horde; "
               "The Warband of Maakurokh",
               "Ugh'kaal, Og'uraag, Uruuk"),
}


def sample_names(cid):
    reg = set()
    d = CULTURES[cid]
    c = CULTURE_NUMBER[cid]
    s = [make_name(d, cid, "settlement", 0, c, e, reg)[0] for e in range(1, 21)]
    v = [make_name(d, cid, "civilization", 0, c, e, reg)[0] for e in range(1, 6)]
    g = [make_name(d, cid, "god", 0, c, e, reg)[0] for e in range(1, 4)]
    return s, v, g


def compare_samples(out):
    bad = 0
    for cid in CULTURE_ORDER:
        s, v, g = sample_names(cid)
        es, ev, eg = SAMPLES[cid]
        for label, got, exp in (("settlements", s, es.split(", ")), ("civilizations", v, ev.split("; ")),
                                ("gods", g, eg.split(", "))):
            for i, (a, b) in enumerate(zip(got, exp)):
                if a != b:
                    bad += 1
                    out.write("DIFF %s %s %d: reference %r, spec 5.1 %r\n" % (cid, label, i + 1, a, b))
            if len(got) != len(exp):
                bad += 1
                out.write("DIFF %s %s: count %d vs %d\n" % (cid, label, len(got), len(exp)))
    return bad


# ---------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------

class Checker:
    def __init__(self):
        self.failures = 0
        self.count = 0

    def eq(self, what, got, exp):
        self.count += 1
        if got != exp:
            self.failures += 1
            sys.stdout.write("FAIL %s: got %r, expected %r\n" % (what, got, exp))


def check_all():
    ck = Checker()
    ck.eq("TAG_HIST", TAG_HIST, cr.tag_value("UNM:HIST"))
    # 4.6 worked example.
    s = cr.stream_key(0, "hist.names.settlement")
    ck.eq("4.6 S", s, 0x969C418DDF3C5EDC)
    exp = [(0, 0xAB9A949FA699954C, 2, 4, "{root}"), (1, 0xF77E9ED830C1A5E0, 4, 5, "NCONC"),
           (2, 0xAF071A40E08E8E51, 4, 6, "ie"), (3, 0x5EAF3E5C2245C62D, 3, 10, "n"),
           (4, 0x42F8D74C06F27F04, 3, 13, "s"), (5, 0x109285B9E64CA453, 0, 6, "a"),
           (6, 0x7347EA42EAED2BB2, 4, 10, "th")]
    trace = []
    v, n, root = attempt(CULTURES["elven"], "settlement", 0, 2, 1, 0, trace)
    ck.eq("4.6 draws", len(trace), 7)
    for (j, u, idx, nn, piece), tr in zip(exp, trace):
        ck.eq("4.6 u(%d)" % j, tr[1], u)
        ck.eq("4.6 pick %d" % j, (tr[3], tr[2]), (idx, nn))
        ck.eq("4.6 piece %d" % j, tr[5], piece)
    ck.eq("4.6 root", (v, root, n), (True, "iensath", "Iensath"))
    reg = set()
    ck.eq("4.6 empty", make_name(CULTURES["elven"], "elven", "settlement", 0, 2, 1, reg), ("Iensath", 0))
    reg = {"Iensath"}
    ck.eq("4.6 collide", make_name(CULTURES["elven"], "elven", "settlement", 0, 2, 1, reg), ("Focoth", 1))
    full = set()
    for t in range(64):
        vv, nn, _ = attempt(CULTURES["elven"], "settlement", 0, 2, 1, t)
        if vv:
            full.add(nn)
    ck.eq("4.6 distinct valid names", len(full), 61)
    ck.eq("4.6 fallback", make_name(CULTURES["elven"], "elven", "settlement", 0, 2, 1, set(full)), ("Iensath II", "F"))
    sc = [x for x in scenarios() if x[0] == "seed42"][0]
    got = [(r[5], r[4]) for r in run_scenario(sc)]
    ck.eq("4.6 seed 42 names", [g[0] for g in got], ["Khork", "The Grork Horde", "Deantwick", "Ol'uraakh Fort"])
    ck.eq("4.6 seed 42 goblin attempt", got[1][1], 1)
    ck.eq("4.6 seed 42 dwarven attempt", got[0][1], 0)
    ck.eq("4.6 seed 42 goblin attempt 0 invalid",
          attempt(CULTURES["goblin"], "civilization", 42, 4, 3, 0)[0], False)
    # 9.1 item 2.
    ck.eq("pick n=1", pick(M64, 1), 0)
    ck.eq("pick u=0", pick(0, 6), 0)
    ck.eq("pick max", pick(M64, 6), 5)
    ck.eq("chance 0", chance(M64, 0), False)
    ck.eq("chance ONE", chance(M64, ONE), True)
    ck.eq("chance HALF 2^31-1", chance((HALF - 1) << 32, HALF), True)
    ck.eq("chance HALF 2^31", chance(HALF << 32, HALF), False)
    b = (3 << 30) << 32
    ck.eq("weighted 0 3 1 below", weighted(b - 1, (0, 3, 1)), 1)
    ck.eq("weighted 0 3 1 at", weighted(b, (0, 3, 1)), 2)
    ck.eq("weighted 0 3 1 zero", weighted(0, (0, 3, 1)), 1)
    # second methods
    for u, w in WEIGHTED_CASES:
        ck.eq("weighted P3-00 form %x %r" % (u, w), weighted(u, w), weighted_p3_00(u, w))
    for h in list(range(0, 64)) + [(1 << 32) - 1 - i for i in range(64)] + [1 << 31, 12345, 3 << 30]:
        for n in (1, 2, 3, 5, 6, 7, 13, 1 << 32):
            u = (h << 32) | 0x5A5A5A5A
            i = pick(u, n)
            # i is the unique index with i * 2^32 <= h * n < (i + 1) * 2^32.
            ck.eq("pick bracket", (i << 32) <= h * n < ((i + 1) << 32), True)
    # 4.1 revised: the sum bound and small shares.
    for w, ok in (([(1 << 63) - 1], True), ([(1 << 63) - 1, 0], True), ([(1 << 63) - 1, 1], False),
                  ([1 << 62, 1 << 62], False), ([1 << 62, (1 << 62) - 1], True)):
        try:
            weighted(0, w)
            ck.eq("weighted bound %r" % w, True, ok)
        except FatalError:
            ck.eq("weighted bound %r" % w, False, ok)
    ck.eq("w[0] tiny share at h=0", weighted(0, (1, 1 << 62)), 0)
    w = (1 << 61, 1, 1 << 61)
    total = sum(w)
    h0 = ((1 << 61 << 32) + total - 1) // total
    ck.eq("tiny middle share: at most one high word",
          sum(1 for h in (h0 - 1, h0, h0 + 1) if weighted(h << 32, w) == 1) <= 1, True)
    w = (1 << 31, 1, (1 << 32) - (1 << 31) - 1)  # middle share exactly 2^-32
    ck.eq("share 2^-32 reachable", weighted((1 << 31) << 32, w), 1)
    # capitalization
    ck.eq("cap 1", cap("gnink-snunk"), "Gnink-Snunk")
    ck.eq("cap 2", cap("uruld'duuld"), "Uruld'duuld")
    ck.eq("cap 3", cap("iensath"), "Iensath")
    # roman (P1-06 RG11 examples)
    for k, r in ((2, "II"), (4, "IV"), (9, "IX"), (14, "XIV"), (40, "XL"), (90, "XC"), (400, "CD"),
                 (1994, "MCMXCIV"), (4000, "MMMM")):
        ck.eq("roman %d" % k, roman(k), r)
    # data
    for cid in CULTURE_ORDER:
        ck.eq("validate %s" % cid, validate(CULTURES[cid]), [])
    ck.eq("validate tiny", validate(TINY), [])
    ck.eq("validate never", validate(NEVER), [])
    # golden list invariants
    for seed in GOLDEN_SEEDS:
        lines = name_list(seed)
        names = [ln.split(" ", 5)[5] for ln in lines]
        ck.eq("unique seed %d" % seed, len(set(names)), len(names))
        for ln in lines:
            parts = ln.split(" ", 5)
            if not name_shape_ok(CULTURES[parts[1]], parts[2], parts[5]):
                ck.eq("shape %s" % ln, False, True)
        ck.eq("4.6 in list", ("%d elven settlement 1 0 Iensath" % 0) in lines, seed == 0)
    sys.stdout.write("%d checks, %d failures\n" % (ck.count, ck.failures))
    return 1 if ck.failures else 0


def sweep(count):
    for seed in (0, 1):
        for cid in CULTURE_ORDER:
            for kind in KINDS:
                reg = set()
                fb = 0
                worst = 0
                for e in range(1, count + 1):
                    n, t = make_name(CULTURES[cid], cid, kind, seed, CULTURE_NUMBER[cid], e, reg)
                    assert name_shape_ok(CULTURES[cid], kind, n), n
                    if t == "F":
                        fb += 1
                    else:
                        worst = max(worst, t)
                sys.stdout.write("seed %d %-8s %-12s names %d fallbacks %d max_attempt %d\n"
                                 % (seed, cid, kind, len(reg), fb, worst))


def main(argv):
    if len(argv) >= 2 and argv[1] == "check":
        return check_all()
    if len(argv) >= 2 and argv[1] == "golden":
        text = golden_text()
        path = argv[2] if len(argv) >= 3 else os.path.join(_HERE, "..", "golden", "p3-02-names.txt")
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    if len(argv) >= 2 and argv[1] == "samples":
        for cid in CULTURE_ORDER:
            s, v, g = sample_names(cid)
            sys.stdout.write("%s (%d)\n  %s\n  %s\n  %s\n" % (cid, CULTURE_NUMBER[cid], ", ".join(s), "; ".join(v),
                                                              ", ".join(g)))
        bad = compare_samples(sys.stdout)
        sys.stdout.write("%d differences from spec 5.1\n" % bad)
        return 1 if bad else 0
    if len(argv) >= 2 and argv[1] == "sweep":
        sweep(int(argv[2]) if len(argv) >= 3 else 2000)
        return 0
    sys.stderr.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
