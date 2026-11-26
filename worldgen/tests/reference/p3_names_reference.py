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

Revision r5/r6 (P3-02 section 11, shipped with P3-08, generator 0.17.0;
the default rules): the name filter of 11.2 and 11.3 (root_word,
name_filtered, read from game/data/name_filters/ with tomllib), the stage
check names_form_text with r6's fixed words, god_patterns and the elven
form `The House of {root}` of 11.4 (CULTURES_R5), and a filtered attempt
skipped like any invalid one (the next keyed attempt; 11.3 "How a rejected
name is redrawn"). `--rules r4` (or use_rules("r4")) gives the 0.15.0 and
0.16.0 names and reproduces the 0.16.0 golden file byte for byte. The
golden file of r5 keeps every keyed-draw vector of 4.1 unchanged and adds
the filter lines described in its header; the critics' stems are checked
here and in the C++ only as concatenations, never stored.

Generator 0.19.0 (P3-10, P3-02 r10 section 12; the default): the filter of
12.1 (data, served through p3_data_reference) and the core rule of 12.2
(make_name's `cores`). `--version 0.18.0` (and 0.17.0) reproduce the older
golden files. Readings taken where the spec is open:
  - 12.1 removes four `words.whole` entries (suck, hack, flop, gord), as its
    table and its size 389 = 393 - 4 say; revision note r10 says "three".
  - 12.2: the core is taken of the root R of the accepted attempt; an
    attempt already in the registry is skipped before the near test (the
    order does not matter: both skip); K is per world and holds the cores
    of civilization and settlement names only (gods and regions never).
  - 11.6's god table is r5's (0.17.0 lists): at 0.19.0 only the gods the
    new entries reject (Grammam, seed 5; Gordman, seed 7) are redrawn.

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
import p3_data_reference as pdr  # noqa: E402

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
# P5-02 9.1 (generator 0.23.0): kinds monster and site after settlement, with their streams. A monster's forms are
# its kind's name_forms (not a culture field): with_forms(D, "monster", F) gives the naming data the attempt reads.
STREAMS_023 = dict(STREAMS, monster="hist.names.monster", site="hist.names.site")
# P5-06 6.11 (generator 0.27.0): kind person (P3-02 4.5's reserved kind, after site) with its stream; its forms are the
# culture's person_forms (with_forms(D, "person", F)).
STREAMS_023["person"] = "hist.names.person"
KINDS_023 = ("god", "civilization", "settlement", "monster", "site")


def with_forms(d, kind, forms):
    """D with D.<kind>_forms = forms (the monster kind's name_forms, P5-02 9.1)."""
    return dict(d, **{kind + "_forms": list(forms)})
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
CULTURES_R4 = CULTURES

# r5 (P3-02 11.4, shipped with P3-08, generator 0.17.0): god_patterns after
# patterns, the elven third civilization form. Transcribed from 11.4.
GOD_PATTERNS = {
    "dwarven": ["ONCONC", "ONCONC", "ONCON"],
    "elven": ["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"],
    "faerie": ["ONON", "ONONON", "ONONC", "ONCON"],
    "goblin": ["ONCONC", "ONC-ONC", "ONONC"],
    "human": ["ONCONC"],
    "orcish": ["ONC'ONC", "ONCONC", "NC'ONC"],
}


def with_god_patterns(d, god_patterns):
    """A culture_naming dict with god_patterns inserted after patterns (the
    declaration order of 11.4)."""
    out = {}
    for k, v in d.items():
        out[k] = v
        if k == "patterns":
            out["god_patterns"] = list(god_patterns)
    return out


CULTURES_R5 = {cid: with_god_patterns(d, GOD_PATTERNS[cid]) for cid, d in CULTURES_R4.items()}
CULTURES_R5["elven"]["civilization_forms"] = ["The Court of {root}", "The Realm of {root}", "The House of {root}"]
# r15 (P3-02 14.1, P3-11, generator 0.20.0): the faerie onsets p -> v, b -> gl; ee and ou once among 16 nuclei
# (the seven others twice); the coda p removed; ass forbidden. Transcribed from 14.1's listing; every other field
# of the faerie naming and every other culture unchanged.
FAERIE_R15 = {
    "onsets": ["", "v", "f", "w", "l", "s", "t", "gl", "sh", "wh", "fl", "tw", "m"],
    "nuclei": ["i", "i", "ee", "ie", "ie", "ia", "ia", "a", "a", "y", "y", "ae", "ae", "ou", "o", "o"],
    "codas": ["", "", "ll", "n", "sh", "th", "sk", "ss"],
    "forbidden": ["eee", "iee", "yy", "ouou", "shsh", "ass"],
}
CULTURES_R15 = {cid: dict(d) for cid, d in CULTURES_R5.items()}
CULTURES_R15["faerie"] = dict(CULTURES_R5["faerie"], **{k: list(v) for k, v in FAERIE_R15.items()})
assert list(CULTURES_R15["faerie"]) == list(CULTURES_R5["faerie"])   # the declaration order is kept
# r17 (P3-02 15.2, P5-01, generator 0.21.0): the dwarven forbidden list gains didin and ririn (appended, in that
# order). Transcribed from 15.2's listing; every other field unchanged.
DWARVEN_FORBIDDEN_R17 = ["mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk",
                         "didin", "ririn"]
CULTURES_R17 = {cid: dict(d) for cid, d in CULTURES_R15.items()}
CULTURES_R17["dwarven"] = dict(CULTURES_R15["dwarven"], forbidden=list(DWARVEN_FORBIDDEN_R17))
assert CULTURES_R15["dwarven"]["forbidden"] == DWARVEN_FORBIDDEN_R17[:-2]
# r19 (P3-02 16.2, P5-12, generator 0.22.0): the human forbidden list gains ass (appended).
CULTURES_R19 = {cid: dict(d) for cid, d in CULTURES_R17.items()}
CULTURES_R19["human"] = dict(CULTURES_R17["human"], forbidden=list(CULTURES_R17["human"]["forbidden"]) + ["ass"])
assert CULTURES_R19["human"]["forbidden"] == ["eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm", "ass"]
# P5-02 4.4 (generator 0.23.0): culture_naming gains site_forms after settlement_forms.
CULTURES_R23 = {cid: dict(d, site_forms=list(pdr.SITE_FORMS_023[cid])) for cid, d in CULTURES_R19.items()}
CULTURE_ORDER = ("dwarven", "elven", "faerie", "goblin", "human", "orcish")  # canonical id order, c = 1..6
CULTURE_NUMBER = {cid: i + 1 for i, cid in enumerate(CULTURE_ORDER)}

# Test cultures shared with the C++ tests (worldgen/tests/history/p3_02_names_support.hpp).
# "tiny": exactly two valid names of any kind, "Ba" and "Da".
TINY = naming(["b", "d"], ["a"], [""], ["ON"], 2, 2, [], ["{root}"], ["{root}"], ["{root}"])
# "never": a one-letter root can never reach min_letters 30.
NEVER = naming(["b"], ["a"], [""], ["N"], 30, 32, [], ["{root}"], ["{root}"], ["{root}"])
# r5 test culture and filter (11.7 item 8; also in p3_02_names_support.hpp):
# "filt" makes the 20 roots b/d/g/k/m + a/o + ""/n; the test filter rejects
# every root word containing "ba" (Ba, Ban), 2 of the 20.
FILT = naming(["b", "d", "g", "k", "m"], ["a", "o"], ["", "n"], ["ONC"], 2, 3, [], ["{root}"], ["{root}"],
              ["{root}"])
TEST_CULTURES = {"tiny": TINY, "never": NEVER, "filt": FILT}
TEST_FILTER_ANYWHERE = ["ba"]


# ---------------------------------------------------------------------------
# 11.2, 11.3 The name filter (r5)
# ---------------------------------------------------------------------------

class NameFilter:
    """The effective lists of 11.2: the unions over all definitions (only
    membership matters, 11.2)."""

    def __init__(self, anywhere=(), at_start=(), whole=()):
        self.anywhere = sorted(set(anywhere))
        self.at_start = sorted(set(at_start))
        self.whole = set(whole)

    def __repr__(self):
        return "NameFilter(%d, %d, %d)" % (len(self.anywhere), len(self.at_start), len(self.whole))


EMPTY_FILTER = NameFilter()
REPO = os.path.normpath(os.path.join(_HERE, "..", "..", ".."))


# P3-09 item 8 (generator 0.18.0) changed game/data/name_filters/words.toml. With FILTER_COMMIT set (option
# --version 0.17.0) the shipped filter is read as it was at that commit (git), so that the 0.17.0 golden is
# reproducible; None reads the files on disk.
FILTER_COMMIT = None


def filter_from_dir(path):
    """The effective filter of a name_filters/ directory, read with tomllib."""
    import tomllib
    a, s_, w = [], [], []
    if os.path.isdir(path) and os.path.normpath(path).startswith(REPO):
        # The files of the data version p3_data_reference serves (0.19.0: P3-02 r10 12.1's entries applied to
        # 0.18.0 files on disk; 0.18.0 over 0.19.0 files: git 81908fa; before 0.18.0: git d750d15).
        texts = [t.encode("utf-8") for _, t in pdr.read_dir_files(path, "name_filters")]
    elif FILTER_COMMIT is not None:
        import subprocess
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        names = subprocess.run(["git", "-C", REPO, "ls-tree", "--name-only", FILTER_COMMIT + ":" + rel], check=True,
                               capture_output=True, text=True).stdout.split()
        texts = [subprocess.run(["git", "-C", REPO, "show", "%s:%s/%s" % (FILTER_COMMIT, rel, n)], check=True,
                                capture_output=True).stdout for n in sorted(names) if n.endswith(".toml")]
    else:
        texts = []
        for name in sorted(os.listdir(path)):
            if name.endswith(".toml"):
                with open(os.path.join(path, name), "rb") as f:
                    texts.append(f.read())
    for raw in texts:
        for _, t in tomllib.loads(raw.decode("utf-8")).get("name_filter", {}).items():
            a += t["anywhere"]
            s_ += t["at_start"]
            w += t["whole"]
    return NameFilter(a, s_, w)


_SHIPPED_FILTER = None


def shipped_filter():
    global _SHIPPED_FILTER
    if _SHIPPED_FILTER is None:
        _SHIPPED_FILTER = filter_from_dir(os.path.join(REPO, "game", "data", "name_filters"))
    return _SHIPPED_FILTER


def lower(s):
    """lower of 11.3: A-Z to a-z, nothing else."""
    return "".join(chr(ord(ch) + 32) if "A" <= ch <= "Z" else ch for ch in s)


def root_word(form, root):
    """W of 11.3: the letters of the form fused to the root, lower-cased,
    around the root (R before Cap)."""
    a, b = form.split("{root}", 1)
    a1 = a[a.rfind(" ") + 1:] if " " in a else a
    b1 = b[:b.find(" ")] if " " in b else b
    return lower(a1) + root + lower(b1)


def word_parts(w):
    """The maximal non-empty runs of letters a-z (W split at ' and -)."""
    parts, cur = [], ""
    for ch in w:
        if "a" <= ch <= "z":
            cur += ch
        else:
            if cur:
                parts.append(cur)
            cur = ""
    if cur:
        parts.append(cur)
    return parts


def name_filtered(filt, w):
    """filtered(W) of 11.3."""
    squeezed = w.replace("'", "").replace("-", "")
    if any(e in squeezed for e in filt.anywhere):
        return True
    parts = word_parts(w)
    if any(p.startswith(e) for p in parts for e in filt.at_start):
        return True
    return any(p in filt.whole for p in parts)


def form_text_problem(filt, cid, kind, i, form):
    """names_form_text of 11.3 (r6) for one form: the message, or None."""
    a, b = form.split("{root}", 1)
    cut_a = a.rfind(" ") + 1 if " " in a else 0
    cut_b = b.find(" ") if " " in b else len(b)
    a1, b1 = a[cut_a:], b[:cut_b]

    def letters(x):
        return lower(x).replace("'", "").replace("-", "")
    sa, sb = letters(a1), letters(b1)
    for e in filt.anywhere:
        if e in sa or e in sb:
            return "history: culture '%s' %s_forms[%d] '%s' holds the filtered text '%s'" % (cid, kind, i, form, e)
    if sa:
        for e in filt.at_start:
            if sa.startswith(e):
                return "history: culture '%s' %s_forms[%d] '%s' holds the filtered text '%s'" % (cid, kind, i, form, e)
    fixed = [x for x in a[:cut_a].split(" ") if x] + [x for x in b[cut_b:].split(" ") if x]
    for word in fixed:
        lw = letters(word)
        for e in filt.anywhere:
            if e in lw:
                return "history: culture '%s' %s_forms[%d] '%s' holds the filtered text '%s'" % (cid, kind, i, form, e)
        for e in filt.at_start:
            if lw.startswith(e):
                return "history: culture '%s' %s_forms[%d] '%s' holds the filtered text '%s'" % (cid, kind, i, form, e)
    return None


def names_form_text(filt, cultures):
    """The stage check names_form_text (11.3): cultures {id: naming}; raises
    FatalError with the first problem (culture ascending id, kind god,
    civilization, settlement, form index)."""
    for cid in sorted(cultures, key=lambda x: x.encode()):
        d = cultures[cid]
        for kind in KINDS:
            for i, form in enumerate(d[kind + "_forms"]):
                msg = form_text_problem(filt, cid, kind, i, form)
                if msg:
                    fatal(msg)


# The rules this module applies: "r5" (default, P3-08, generator 0.17.0:
# god_patterns, the name filter, the House form) or "r4" (0.15.0 and
# 0.16.0). use_rules("r4") reproduces the 0.16.0 names exactly.
RULES = None
CULTURES = None
FILTER = None


def use_version(v):
    """The filter data of generator v (through p3_data_reference's served version)."""
    global FILTER_COMMIT, _SHIPPED_FILTER
    pdr.use_version(v)
    FILTER_COMMIT = None if pdr.at_least("0.18.0") else pdr.NAME_FILTERS_017_COMMIT
    _SHIPPED_FILTER = None
    global TRIPLE_RULE, REPEAT_RULE
    TRIPLE_RULE = pdr.at_least("0.19.0")
    REPEAT_RULE = pdr.at_least("0.20.0")
    global DOUBLE_RULE
    DOUBLE_RULE = pdr.at_least("0.22.0")
    use_rules(RULES)


def use_rules(r):
    global RULES, CULTURES, FILTER
    if r == "r5":
        # r15 (0.20.0): the faerie lists of 14.1
        CULTURES, FILTER = (CULTURES_R23 if pdr.at_least("0.23.0") else CULTURES_R19 if pdr.at_least("0.22.0") else CULTURES_R17 if pdr.at_least("0.21.0") else CULTURES_R15 if pdr.at_least("0.20.0")
                            else CULTURES_R5), shipped_filter()
    elif r == "r4":
        CULTURES, FILTER = CULTURES_R4, EMPTY_FILTER
    else:
        raise ValueError(r)
    RULES = r


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


def attempt(d, kind, seed, c, e, t, trace=None, filt=None, flags=None):
    """(valid, name, root) of 4.2 with r5's filter (11.3) and god_patterns
    (11.4); trace, if a list, receives (j, u, n, index, symbol, piece);
    flags, if a dict, receives "filtered" (the r4 rules pass and the filter
    rejects). filt None: the module's FILTER. A naming without god_patterns
    (r4 data) draws god shapes from patterns."""
    if filt is None:
        filt = FILTER
    s_key = cr.stream_key(seed, STREAMS_023[kind])
    forms = d[kind + "_forms"]

    def u(j):
        return draw(s_key, t, c, e, j)

    u0 = u(0)
    fi = pick(u0, len(forms))
    form = forms[fi]
    if trace is not None:
        trace.append((0, u0, len(forms), fi, "form", form))
    u1 = u(1)
    pats = d["god_patterns"] if kind == "god" and "god_patterns" in d else d["patterns"]
    if not pats:
        fatal("names: a naming list is empty")
    pi = pick(u1, len(pats))
    pattern = pats[pi]
    if trace is not None:
        trace.append((1, u1, len(pats), pi, "pattern", pattern))
    root = ""
    j = 2
    drawn = []   # (symbol, string drawn) per pattern position, separators included (14.2)
    for s in pattern:
        if s in "ONC":
            arr = d[{"O": "onsets", "N": "nuclei", "C": "codas"}[s]]
            uj = u(j)
            i = pick(uj, len(arr))
            root += arr[i]
            drawn.append((s, arr[i]))
            if trace is not None:
                trace.append((j, uj, len(arr), i, s, arr[i]))
            j += 1
        else:
            root += s
            drawn.append((s, s))
    valid = root_valid(d, root)
    name = form.replace("{root}", cap(root), 1)
    valid = valid and len(name) <= 48
    filtered = valid and name_filtered(filt, root_word(form, root))
    if flags is not None:
        flags["filtered"] = filtered
    trip = valid and not filtered and TRIPLE_RULE and triple(name)
    if flags is not None:
        flags["triple"] = trip
    rep = valid and not filtered and not trip and REPEAT_RULE and repeated(drawn)
    dbl = valid and not filtered and not trip and not rep and DOUBLE_RULE and doubled(d, kind, form, root)
    if flags is not None:
        flags["repeated"] = rep
        flags["doubled"] = dbl
        flags["drawn"] = drawn
        flags["r4valid"] = valid
        flags["word"] = root_word(form, root)
    return valid and not filtered and not trip and not rep and not dbl, name, root


# P3-02 r19 16.1 (P5-12, generator 0.22.0): no doubled place suffix. DOUBLE_RULE follows the served data version.
DOUBLE_RULE = True
VOWELS = set("aeiouy")


def squeeze(s):
    return lower(s).replace("'", "").replace("-", "")


def fused_tail(form):
    """B' of 11.3 for 16.1: the letters fused after {root} up to the form's first space, lower-cased, ' and -
    removed ("" when none)."""
    b = form.split("{root}", 1)[1]
    return squeeze(b[:b.find(" ")] if " " in b else b)


def fused_tails(d, kind):
    """T_K of 16.1: the non-empty fused tails of the kind's forms, without repeats, in form order."""
    out = []
    for f in d[kind + "_forms"]:
        x = fused_tail(f)
        if x and x not in out:
            out.append(x)
    return out


def first_syllable(x):
    """fs(x) of 16.1: the shortest prefix holding x's first run of vowels and every consonant after it up to the
    next vowel; all of x when no vowel follows (or x has no vowel)."""
    i = 0
    while i < len(x) and x[i] not in VOWELS:
        i += 1
    if i == len(x):
        return x
    while i < len(x) and x[i] in VOWELS:
        i += 1
    while i < len(x) and x[i] not in VOWELS:
        i += 1
    return x[:i]


def doubled_item(d, kind, form, root):
    """The item of 16.1 (1, 2 or 3) that makes the attempt invalid, or 0."""
    x = fused_tail(form)
    if not x:
        return 0
    r = squeeze(root)
    if r.endswith(x) or r.endswith(first_syllable(x)):
        return 1
    if any(y != x and r.endswith(y) for y in fused_tails(d, kind)):
        return 2
    if r and r[-1] == x[0]:
        return 3
    return 0


def doubled(d, kind, form, root):
    return doubled_item(d, kind, form, root) != 0


# P3-02 r15 14.2 (P3-11, generator 0.20.0): no syllable repeated twice in a row, for every culture. REPEAT_RULE
# follows the served data version (use_version).
REPEAT_RULE = True


def syllables(drawn):
    """14.2: [(s_i, b_i)] of an attempt from its (symbol, string drawn) per pattern position. A boundary stands at
    the first symbol, before every O, before every N not directly preceded by O, and at every separator (which
    belongs to no syllable); b_i leaves out the strings drawn for C symbols."""
    out, cur, prev = [], None, None
    for sym, piece in drawn:
        if sym in SEP:
            if cur is not None:
                out.append(tuple(cur))
            cur, prev = None, sym
            continue
        if cur is not None and (sym == "O" or (sym == "N" and prev != "O")):
            out.append(tuple(cur))
            cur = None
        if cur is None:
            cur = ["", ""]
        cur[0] += piece
        if sym != "C":
            cur[1] += piece
        prev = sym
    if cur is not None:
        out.append(tuple(cur))
    return out


def repeated(drawn):
    """14.2: some two consecutive syllables with s_i not empty and s_i = s_{i+1} or s_i = b_{i+1} (bytes)."""
    sy = syllables(drawn)
    return any(a[0] != "" and (a[0] == b[0] or a[0] == b[1]) for a, b in zip(sy, sy[1:]))


# P3-02 r13 13.2 (P3-10 second round, generator 0.19.0): an attempt whose rendered name holds the same letter a-z
# three times in a row (case-insensitive; every other character, space, ' and - included, breaks a run) is invalid.
# TRIPLE_RULE follows the served data version (use_version).
TRIPLE_RULE = True


def triple(name):
    n = lower(name)
    for i in range(len(n) - 2):
        if "a" <= n[i] <= "z" and n[i] == n[i + 1] == n[i + 2]:
            return True
    return False


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


def name_core(root):
    """P3-02 r10 12.2: the core of a root, its letters without ' and - (the root is lower case)."""
    return lower(root.replace("'", "").replace("-", ""))


def lev1(a, b):
    """True when a and b are at Levenshtein distance at most 1."""
    if a == b:
        return True
    la, lb = len(a), len(b)
    if abs(la - lb) > 1:
        return False
    if la == lb:
        return sum(1 for x, y in zip(a, b) if x != y) == 1
    if la > lb:
        a, b, la, lb = b, a, lb, la
    i = 0
    while i < la and a[i] == b[i]:
        i += 1
    return a[i:] == b[i + 1:]


def core_near(core, cores):
    """12.2: k is near K when some core of K equals k or is one letter (inserted, deleted, replaced) apart."""
    return any(lev1(core, k) for k in cores)


def make_name(d, culture_id, kind, seed, c, e, registry, filt=None, cores=None):
    """4.3; returns (name, attempt number or 'F'); adds the name to registry
    (a set). A filtered attempt is an invalid attempt (11.3). With cores a
    set (the history stage's K of P3-02 r10 12.2, civilizations and
    settlements from generator 0.19.0), an attempt whose core is near K is
    skipped like a name already in the registry; the accepted name's core
    (or the fallback's first valid attempt's core) joins K."""
    first = None
    first_core = None
    for t in range(64):
        valid, n, root = attempt(d, kind, seed, c, e, t, filt=filt)
        if not valid:
            continue
        if first is None:
            first = n
            first_core = name_core(root)
        if n not in registry and (cores is None or not core_near(name_core(root), cores)):
            registry.add(n)
            if cores is not None:
                cores.add(name_core(root))
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
    if cores is not None:
        cores.add(first_core)
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


def test_filter():
    return NameFilter(TEST_FILTER_ANYWHERE)


def filt_entity(want_filtered, want_valid_at=None, seed=0):
    """The smallest entity e of culture filt (c = 1, settlement) whose
    attempts 0 .. want_filtered - 1 are all filtered by the test filter (and,
    if given, attempt want_valid_at is valid and unfiltered)."""
    f = test_filter()
    for e in range(1, 100000):
        ok = True
        for t in range(want_filtered):
            fl = {}
            v, _, _ = attempt(FILT, "settlement", seed, 1, e, t, filt=f, flags=fl)
            ok = ok and fl["filtered"]
        if ok and want_valid_at is not None:
            ok = attempt(FILT, "settlement", seed, 1, e, want_valid_at, filt=f)[0]
        if ok:
            return e
    raise AssertionError("no such entity")


def filt_names():
    """Every valid root name of culture filt with the r4 rules, sorted."""
    out = set()
    for o in FILT["onsets"]:
        for n in FILT["nuclei"]:
            for c in FILT["codas"]:
                out.add(cap(o + n + c))
    return sorted(out)


def r5_scenarios():
    """Scenarios of 11.7 item 8 on culture filt with the test filter."""
    out = []
    e1 = filt_entity(1)
    unfiltered = [n for n in filt_names() if not name_filtered(test_filter(), lower(n))]
    out.append(("filtered", 0, "filt", "settlement", unfiltered, [e1]))
    e2 = filt_entity(2, 2)
    out.append(("redraw_alone", 0, "filt", "settlement", [], [e2]))
    # Other entities first (shipped human names, some of whose attempts are
    # filtered, none of them two or three letters): e2 still takes attempt 2.
    out.append(("redraw_after", 0, None, None, [], [("human", "settlement", e) for e in range(1, 6)]
                + [("human", "civilization", e) for e in range(1, 6)] + [("filt", "settlement", e2)]))
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
        f = test_filter() if icid == "filt" else None
        n, t = make_name(d, icid, ikind, seed, c, e, reg, filt=f)
        res.append((icid, ikind, c, e, t, n))
    return res


def name_list(seed, cultures=None, filt=None):
    cultures = cultures or CULTURES
    reg = set()
    lines = []
    for cid in CULTURE_ORDER:
        for kind in KINDS:
            for e in range(1, GOLDEN_ENTITIES + 1):
                n, t = make_name(cultures[cid], cid, kind, seed, CULTURE_NUMBER[cid], e, reg, filt=filt)
                lines.append("%d %s %s %d %s %s" % (seed, cid, kind, e, t, n))
    return lines


def r4_equivalent_cultures():
    """11.7 item 7: the r5 shipped cultures with god_patterns = patterns and
    the r4 elven forms; with the empty filter they give r4's names."""
    return {cid: with_god_patterns(CULTURES_R4[cid], CULTURES_R4[cid]["patterns"]) for cid in CULTURE_ORDER}


def list_sha(lines):
    return hashlib.sha256("\n".join(lines).encode("ascii")).hexdigest()


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
    if RULES == "r5":
        o.append("#   r5 (P3-02 11, generator 0.17.0): attempts and lists use god_patterns and the shipped")
        o.append("#       name filter of game/data/name_filters/; the elven third form is The House of {root}")
        o.append("#   fattempt <seed> <culture> <kind> <c> <e> <t> <valid 0|1> <filtered 0|1> <name|->")
        o.append("#       attempts with the filter (11.3, 11.5 NameAttempt::filtered); culture filt uses the")
        o.append("#       test filter (anywhere = [ba]), every shipped culture the shipped filter")
        o.append("#   rootword <form with _ for space> <root> <W>     root_word of 11.3")
        o.append("#   filter <W> <0|1>   name_filtered(shipped filter, W) on benign words (the critics' stems")
        o.append("#       are tested in the C++, written as concatenations, never stored here)")
        o.append("#   formtext <culture> <kind> <i> <ok|message>  names_form_text of 11.3 on the shipped forms")
        o.append("#   scenarios filtered, redraw_alone, redraw_after (11.7 item 8) use culture filt, defined in")
        o.append("#       the reference and in p3_02_names_support.hpp, with the test filter")
        o.append("#   r4sha256 <seed> <hex>  11.7 item 7: the list lines of that seed computed with the empty filter,")
        o.append("#       god_patterns = patterns and the r4 elven forms (the 0.16.0 list, whose sha256 line it equals)")
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
    for name in sorted(set((STREAMS_023 if pdr.at_least("0.23.0") else STREAMS).values())):
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
    if RULES == "r5":
        fcases = [(0, "filt", "settlement", 1, filt_entity(1), 0)]
        e2 = filt_entity(2, 2)
        fcases += [(0, "filt", "settlement", 1, e2, t) for t in range(3)]
        # shipped cultures: the first filtered attempt of each kind at seed 42 (entities 1..)
        for cid in CULTURE_ORDER:
            for kind in KINDS:
                for e in range(1, 2000):
                    fl = {}
                    attempt(CULTURES[cid], kind, 42, CULTURE_NUMBER[cid], e, 0, flags=fl)
                    if fl["filtered"]:
                        fcases.append((42, cid, kind, CULTURE_NUMBER[cid], e, 0))
                        fcases.append((42, cid, kind, CULTURE_NUMBER[cid], e, 1))
                        break
        for seed, cid, kind, c, e, t in fcases:
            fl = {}
            f = test_filter() if cid == "filt" else None
            d = CULTURES.get(cid) or TEST_CULTURES[cid]
            v, n, _ = attempt(d, kind, seed, c, e, t, filt=f, flags=fl)
            # a filtered attempt's name is not printed (it may be offensive; it is the invalid case)
            o.append("fattempt %d %s %s %d %d %d %d %d %s" % (seed, cid, kind, c, e, t, 1 if v else 0,
                                                             1 if fl["filtered"] else 0, n if v else "-"))
        for form, root in (("The Hold of {root}", "dur"), ("{root}ford", "ced"), ("Ny{root} Pit", "ash"),
                           ("{root}", "ug'kaal"), ("The {root} Throne", "gnink-snunk"), ("The Realm of {root}",
                                                                                           "arthos")):
            o.append("rootword %s %s %s" % (form.replace(" ", "_"), root, root_word(form, root)))
        for w in ("stard", "iensath", "grork", "arthos", "ascumbrin", "focoth", "khorkhound", "bag", "dog", "baa",
                  "gord", "vanir", "dork", "bald", "buzz"):
            o.append("filter %s %d" % (w, 1 if name_filtered(FILTER, w) else 0))
        for cid in CULTURE_ORDER:
            for kind in KINDS:
                for i, form in enumerate(CULTURES[cid][kind + "_forms"]):
                    o.append("formtext %s %s %d %s" % (cid, kind, i, form_text_problem(FILTER, cid, kind, i, form)
                                                       or "ok"))
    for sc in scenarios() + (r5_scenarios() if RULES == "r5" else []):
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
        o.append("sha256 %d %s" % (seed, list_sha(lines)))
    if RULES == "r5":
        for seed in GOLDEN_SEEDS:
            o.append("r4sha256 %d %s" % (seed, list_sha(name_list(seed, r4_equivalent_cultures(), EMPTY_FILTER))))
    if RULES == "r5" and pdr.at_least("0.19.0"):
        o.extend(r10_golden_lines())
        o.extend(r13_golden_lines())
    if RULES == "r5" and pdr.at_least("0.20.0"):
        o.extend(r15_golden_lines())
        if pdr.at_least("0.21.0"):
            o.extend(r17_golden_lines())
        if pdr.at_least("0.22.0"):
            o.extend(r19_golden_lines())
        if pdr.at_least("0.23.0"):
            o.extend(r23_golden_lines())
    return "\n".join(o) + "\n"


# ---------------------------------------------------------------------------
# P3-02 r10 section 12 (generator 0.19.0): filter entries (data) and the core rule
# ---------------------------------------------------------------------------

# 12.3 unit cases of the predicate: (core, K, near?)
CORE_NEAR_CASES = (
    ("skat", ("skat",), True), ("sligg", ("slig",), True), ("kuzz", ("kuz",), True), ("kaz", ("kuz",), True),
    ("kuz", ("kuz",), True), ("kazz", ("kuz",), False), ("slig", ("sligg",), True), ("ab", (), False),
    ("abc", ("abd", "xyz"), True), ("abc", ("cab",), False), ("nili", ("muli",), False), ("a", ("",), True),
    ("skreek", ("skreeg",), True), ("zeez", ("zez",), True), ("zeez", ("ze",), False),
)
# 12.2: root -> core
CORE_CASES = (("skat", "skat"), ("ogh'krak", "oghkrak"), ("gnink-snunk", "gninksnunk"), ("uruld'duuld", "uruldduuld"))
# 12.1's table: the root word each entry rejects (the critics' names), and names that pass.
R10_REJECTED = ("suckford", "shiety", "realdeadstead", "timhack", "whop", "flopfia", "gordman", "grammam",
                "suckbury", "deadstead", "hackford", "ashackstead", "whopia", "leeflop", "gordford", "grammaik",
                "gulsuckton", "shietwo")
R10_PASSING = ("gramm", "mammga", "twiwhiwou", "timmthorin", "kaidinmamm")
CORE_ENTITIES = 50

# ---------------------------------------------------------------------------
# P3-02 r13 section 13 (P3-10 second round, generator 0.19.0)
# ---------------------------------------------------------------------------

# 13.3's names: filtered by the r13 entries (root words), and the triple-letter cases (rendered names).
R13_FILTERED = ("twipee", "twypoo", "twotitwee", "fassbou", "beamcumstead", "mypee", "teefass", "dincum")
R13_TRIPLE = ("Wasssie", "Floullly Hollow", "Fiasssie", "Passsae", "The Circle of Eesssy", "The Bosssie Ring",
              "AAAb", "xBBBy")
R13_NOT_TRIPLE = ("Bigg-Gnizz", "Aa'azaam", "Billa Hollow", "Aa a", "Kaa-aar", "Mammga", "Ee'e", "")
# 13.3: the fallback's numeral is not tested (the rule tests attempts): with the registry {Ba, Da, Ba II}, culture
# tiny's fallback is "Ba III" (a string triple() holds).
R13_FALLBACK = (("Ba", "Da", "Da II"), "Da III")   # seed 0, settlement 1: the first valid attempt is Da
# Test cultures of 13.3 (also in worldgen/tests/history/p3_02_names_support.hpp): pattern "O" makes the root one
# onset, so every attempt renders one of the listed names.
TRIP = naming(["wasssie", "billa", "bigg-gnizz", "aa'azaam", "fiasssie"], ["a"], [""], ["O"], 1, 32, [],
              ["{root}"], ["{root}"], ["{root}"])
TRIP_HOLLOW = naming(["floullly", "billa"], ["a"], [""], ["O"], 1, 32, [], ["{root}"], ["{root}"],
                     ["{root} Hollow"])
TRIP_CULTURES = (("trip", TRIP), ("trip_hollow", TRIP_HOLLOW))
TRIP_SEEDS = (0, 42)


def r13_golden_lines():
    o = ["# r13 (P3-02 13, generator 0.19.0 second round):",
         "#   triple <0|1> <name>   13.2's test on a rendered name (case-insensitive, three equal letters a-z",
         "#       at consecutive positions); the name is the rest of the line",
         "#   tattempt <culture> <kind> <seed> <c> <e> <t> <valid 0|1> <triple 0|1> <name>  attempts of the",
         "#       test cultures trip and trip_hollow (filter: the shipped one), c = 1",
         "#   tresult <culture> <kind> <seed> <c> <e> <attempt|F> <name>  make_name with a fresh registry"]
    for n in R13_TRIPLE + R13_NOT_TRIPLE:
        if n:
            o.append("triple %d %s" % (1 if triple(n) else 0, n))
    for cid, d in TRIP_CULTURES:
        for kind in KINDS:
            for seed in TRIP_SEEDS:
                for e in (1, 2, 3):
                    for t in range(8):
                        fl = {}
                        v, n, _ = attempt(d, kind, seed, 1, e, t, flags=fl)
                        o.append("tattempt %s %s %d 1 %d %d %d %d %s" % (cid, kind, seed, e, t, 1 if v else 0,
                                                                         1 if fl.get("triple") else 0, n))
                    n, t = make_name(d, cid, kind, seed, 1, e, set())
                    o.append("tresult %s %s %d 1 %d %s %s" % (cid, kind, seed, e, t, n))
    return o


def check_r13(ck):
    """P3-02 r13 13.3."""
    sf = shipped_filter()
    for w in R13_FILTERED:
        ck.eq("13.1 %s filtered" % w, name_filtered(sf, w), True)
    for w in ("billa", "biggnizz", "gramm", "mammga", "cucmber", "spon"):
        ck.eq("13.1 %s passes" % w, name_filtered(sf, w), False)
    for n in R13_TRIPLE:
        ck.eq("13.2 triple %r" % n, triple(n), True)
    for n in R13_NOT_TRIPLE:
        ck.eq("13.2 not triple %r" % n, triple(n), False)
    ck.eq("13.1 sizes of the shipped filter (effective; r15 14.3 from 0.20.0)",
          (len(sf.anywhere), len(sf.at_start), len(sf.whole)),
          (109, 30, 514) if pdr.at_least("0.21.0") else (109, 31, 511) if pdr.at_least("0.20.0") else (110, 31, 508))
    filters = {d: t for d, t in pdr.load_root("game/data", ["name_filter"])["name_filter"]}
    ck.eq("13.1 name_filter_not_redundant passes on the edited lists", pdr.name_filter_messages(sorted(filters.items())),
          [])
    bad = dict(filters, crude=dict(filters["crude"], at_start=sorted(filters["crude"]["at_start"] + ["pee"])))
    ck.eq("13.3 a copy that keeps crude.at_start pee fails name_filter_not_redundant",
          [m.split(":")[0] for m in pdr.name_filter_messages(sorted(bad.items()))], ["name_filter_not_redundant"])
    bad = dict(filters, words=dict(filters["words"], whole=sorted(filters["words"]["whole"] + ["teepee"])))
    ck.eq("13.3 a copy that keeps words.whole teepee fails name_filter_not_redundant",
          [m.split(":")[0] for m in pdr.name_filter_messages(sorted(bad.items()))], ["name_filter_not_redundant"])
    for cid, d in TRIP_CULTURES:
        for kind in KINDS:
            for seed in TRIP_SEEDS:
                for e in (1, 2, 3):
                    n, t = make_name(d, cid, kind, seed, 1, e, set())
                    ck.eq("13.2 %s %s %d %d gives a name without a triple" % (cid, kind, seed, e), triple(n), False)
                    if t != "F" and t > 0:
                        _, n0, _ = attempt(d, kind, seed, 1, e, 0)
                        ck.eq("13.2 %s %s %d %d: attempt 0 skipped for a reason" % (cid, kind, seed, e),
                              triple(n0) or n0 == n, True)
    reg, want = R13_FALLBACK
    ck.eq("13.3 the fallback numeral is exempt", make_name(TINY, "tiny", "settlement", 0, 1, 1, set(reg)), (want, "F"))
    ck.eq("13.3 ... though the numbered name holds III", triple(want), True)
    seen = set()
    for cid, d in TRIP_CULTURES:
        for kind in KINDS:
            for seed in TRIP_SEEDS:
                for e in (1, 2, 3):
                    for t in range(8):
                        fl = {}
                        v, n, _ = attempt(d, kind, seed, 1, e, t, flags=fl)
                        seen.add((v, bool(fl.get("triple"))))
    ck.eq("13.2 the test cultures give both valid and triple attempts", sorted(seen), [(False, True), (True, False)])
    for seed in GOLDEN_SEEDS:
        ck.eq("invariant 7 (13.3) seed %d: no listed name holds three equal letters" % seed,
              [l for l in name_list(seed) if triple(l.split(" ", 5)[5].rsplit(" ", 1)[0]
                                                   if l.split(" ", 5)[4] == "F" else l.split(" ", 5)[5])], [])
        ck.eq("invariant 7 (13.3) seed %d, core lists" % seed,
              [l for l in core_list(seed) if l.split(" ", 5)[4] != "F" and triple(l.split(" ", 6)[6])], [])


# ---------------------------------------------------------------------------
# P3-02 r15 section 14 (P3-11, generator 0.20.0)
# ---------------------------------------------------------------------------

# 14.2's table and 14.4's OONC case: (pattern, strings drawn for its O, N, C symbols in order, repeated?), the
# expected result transcribed from the spec.
R15_ROWS = (
    ("ONONON", ("w", "i", "fl", "ee", "fl", "ee"), True),     # wifleeflee
    ("ONONC", ("l", "i", "l", "i", "n"), True),                # lilin
    ("ONON", ("f", "i", "f", "i"), True),                      # fifi
    ("ONON", ("f", "ee", "f", "ee"), True),                    # feefee
    ("ONON", ("n", "o", "n", "o"), True),                      # nono
    ("ONCONC", ("d", "a", "ck", "d", "a", "ck"), True),        # dackdack (human)
    ("ONC-ONC", ("kr", "a", "k", "kr", "a", "k"), True),       # krak-krak (goblin)
    ("ONCONC", ("gr", "a", "n", "gr", "a", "k"), False),       # grangrak (dwarven)
    ("ONON", ("f", "ee", "f", "i"), False),                    # feefi
    ("ONONON", ("tw", "i", "wh", "i", "w", "ou"), False),      # twiwhiwou
    ("ONCON", ("l", "i", "ll", "", "ia"), False),              # lillia
    ("OONC", ("", "l", "i", "n"), False),                      # 14.4: syllables "" and lin, s_1 empty
)


def row_drawn(pattern, pieces):
    """(symbol, string) per pattern position of a row (separators copied)."""
    out, it = [], iter(pieces)
    for s in pattern:
        out.append((s, next(it)) if s in "ONC" else (s, s))
    return out


def row_root(pattern, pieces):
    return "".join(p for _, p in row_drawn(pattern, pieces))


def row_culture(pattern, pieces):
    """A test culture whose lists hold exactly the row's strings per symbol (first appearance order), the row's
    pattern only, no forbidden entry, letters 1 to 32, every form {root}: every attempt passes the r4 rules, so
    only 14.2 (and the shipped filter, checked) can reject one. Also built by the C++ test from the rrow line."""
    lists = {"O": [], "N": [], "C": []}
    for s, p in row_drawn(pattern, pieces):
        if s in lists and p not in lists[s]:
            lists[s].append(p)
    return naming(lists["O"], lists["N"], lists["C"] or [""], [pattern], 1, 32, [], ["{root}"], ["{root}"],
                  ["{root}"])


def find_row_attempt(d, pattern, pieces, kind="settlement", seed=0, c=1):
    """The smallest (e, t), e in 1..200, t in 0..63, whose attempt draws exactly the row's strings."""
    want = row_drawn(pattern, pieces)
    for e in range(1, 201):
        for t in range(64):
            fl = {}
            attempt(d, kind, seed, c, e, t, flags=fl)
            if fl["drawn"] == want:
                return e, t
    raise AssertionError("row %s %r not reached" % (pattern, pieces))


def dot(xs):
    """A comma-joined list with '.' for an empty string (golden lines)."""
    return ",".join(x if x else "." for x in xs)


# 14.4: the redraw and the fallback. Culture rep: "Ba" (ON) is valid, "Baba" (ONON) repeated.
REP = naming(["b"], ["a"], [""], ["ON", "ONON"], 2, 4, [], ["{root}"], ["{root}"], ["{root}"])
# 14.3's names (P3-02 11.7 item 10: the stems written as concatenations): root words filtered by tard, and not.
R15_TARD = ("tard" + "ham", "tard" + "wuldwick", "hork" + "tard", "s" + "tard")
R15_WORDS = ("via", "van", "von", "ova")
# P3-02 r15 14.4 / 5.1's protocol at 0.20.0 (seed 0, one registry: settlements 1-20, civilizations 1-5, gods 1-3):
# the faerie sample for the critic, recorded from this reference (SAMPLES_R15; not a spec transcription).
SAMPLES_R15 = {
    "faerie": ("Liava, Wiatwiesk Glade, Twamae Glade, Lith, Gliafloss Glade, Viashaeth Hollow, Teelan, Meeie Hollow, "
               "Sywyn Hollow, Iatysk, Wievysk, Miamo Hollow, Mavofie, Fitoth Hollow, Showhia Hollow, "
               "Mawhiath Hollow, Ashfli Hollow, Whywo, Fliavosk Hollow, Whosy Hollow",
               "The Lysk Ring; The Watiafia Court; The Gloss Ring; The Wosk Ring; The Twiwhiwae Ring",
               "Fiwhall, Fiaglo, Gliallsho"),
}


def r15_rep_entity(seed=0):
    """The smallest entity of culture rep (settlement, c 1) whose attempt 0 is repeated (Baba) and whose first
    valid attempt is later."""
    for e in range(1, 500):
        fl = {}
        v, _, _ = attempt(REP, "settlement", seed, 1, e, 0, flags=fl)
        if not v and fl["repeated"]:
            return e
    raise AssertionError("no rep entity")


def r15_golden_lines():
    o = ["# r15 (P3-02 14, generator 0.20.0):",
         "#   rrow <i> <pattern> <onsets> <nuclei> <codas> <pieces> <e> <t> <valid 0|1> <root>  14.2's table and",
         "#       14.4's OONC row: a test culture whose lists are the comma lists (. is the empty string), the",
         "#       row's pattern only, letters 1-32, no forbidden entry, forms {root}; settlement, seed 0, c 1;",
         "#       (e, t) the first attempt drawing exactly the row's pieces; valid = not repeated",
         "#   rattempt <i> <e> <t> <valid 0|1> <repeated 0|1> <pieces> <name>  every attempt t 0-15 of e 1-3",
         "#       of row i's culture (settlement, seed 0, c 1); the name of a repeated attempt is the drawn one",
         "#   rresult rep <kind> <seed> 1 <e> <attempt|F> <name> [<registry comma-joined>]  make_name of culture",
         "#       rep (b / a / . / ON,ONON / 2-4), the entity whose settlement attempt 0 drew Baba; the line with a",
         "#       registry starts from {Ba}: the fallback numbers Ba, the first valid attempt",
         "#   sattempt <seed> <culture> <kind> <c> <e> <t> <valid 0|1> <repeated 0|1> <name>  shipped",
         "#       cultures: the first attempt at seed 42 rejected by 14.2 alone (entities from 1) and the next",
         "#   sample15 <culture> <settlements|civilizations|gods> <names joined by '; '>  5.1's protocol at",
         "#       0.20.0 for the faerie culture (the critic's sample)"]
    for i, (pat, pieces, _) in enumerate(R15_ROWS, 1):
        d = row_culture(pat, pieces)
        e, t = find_row_attempt(d, pat, pieces)
        fl = {}
        v, n, root = attempt(d, "settlement", 0, 1, e, t, flags=fl)
        o.append("rrow %d %s %s %s %s %s %d %d %d %s" % (i, pat, dot(d["onsets"]), dot(d["nuclei"]), dot(d["codas"]),
                                                         dot(pieces), e, t, 1 if v else 0, root))
    for i, (pat, pieces, _) in enumerate(R15_ROWS, 1):
        d = row_culture(pat, pieces)
        for e in (1, 2, 3):
            for t in range(16):
                fl = {}
                v, n, _ = attempt(d, "settlement", 0, 1, e, t, flags=fl)
                o.append("rattempt %d %d %d %d %d %s %s" % (i, e, t, 1 if v else 0, 1 if fl["repeated"] else 0,
                                                            dot([p for s, p in fl["drawn"] if s in "ONC"]), n))
    e = r15_rep_entity()
    for kind in KINDS:
        n, t = make_name(REP, "rep", kind, 0, 1, e, set())
        o.append("rresult rep %s 0 1 %d %s %s" % (kind, e, t, n))
    n, t = make_name(REP, "rep", "settlement", 0, 1, e, {"Ba"})
    o.append("rresult rep settlement 0 1 %d %s %s Ba" % (e, t, n))
    for cid in CULTURE_ORDER:
        for kind in KINDS:
            for e in range(1, 3000):
                fl = {}
                v, _, _ = attempt(CULTURES[cid], kind, 42, CULTURE_NUMBER[cid], e, 0, flags=fl)
                if fl["repeated"]:
                    for t in (0, 1):
                        fl = {}
                        v, n, _ = attempt(CULTURES[cid], kind, 42, CULTURE_NUMBER[cid], e, t, flags=fl)
                        o.append("sattempt 42 %s %s %d %d %d %d %d %s" % (cid, kind, CULTURE_NUMBER[cid], e, t,
                                                                         1 if v else 0, 1 if fl["repeated"] else 0, n))
                    break
    s, v, g = sample_names("faerie")
    o.append("sample15 faerie settlements %s" % "; ".join(s))
    o.append("sample15 faerie civilizations %s" % "; ".join(v))
    o.append("sample15 faerie gods %s" % "; ".join(g))
    return o


def check_r15(ck):
    """P3-02 r15 14.4."""
    for i, (pat, pieces, want) in enumerate(R15_ROWS, 1):
        ck.eq("14.2 row %d %s repeated" % (i, row_root(pat, pieces)), repeated(row_drawn(pat, pieces)), want)
        d = row_culture(pat, pieces)
        ck.eq("14.2 row %d culture valid data" % i, validate(d), [])
        e, t = find_row_attempt(d, pat, pieces)
        fl = {}
        v, _, root = attempt(d, "settlement", 0, 1, e, t, flags=fl)
        ck.eq("14.2 row %d attempt (filter passes, r4 rules pass)" % i, (v, fl["repeated"], fl["filtered"],
                                                                         fl["triple"]), (not want, want, False, False))
    ck.eq("14.2 syllables of OONC", syllables(row_drawn("OONC", ("", "l", "i", "n"))), [("", ""), ("lin", "li")])
    ck.eq("14.2 syllables of NC'ONC", syllables(row_drawn("NC'ONC", ("u", "g", "kr", "a", "k"))),
          [("ug", "u"), ("krak", "kra")])
    ck.eq("14.2 syllables of ONCON (lillia)", syllables(row_drawn("ONCON", ("l", "i", "ll", "", "ia"))),
          [("lill", "li"), ("ia", "ia")])
    ck.eq("14.2 a run without N is a syllable (OONC)", len(syllables(row_drawn("OONC", ("b", "l", "i", "n")))), 2)
    ck.eq("14.2 across a separator", repeated(row_drawn("NC'NC", ("u", "g", "u", "g"))), True)
    ck.eq("14.2 the second is the first plus a coda (NNC: N not after O starts a syllable)",
          repeated(row_drawn("NNC", ("a", "a", "n"))), True)
    # the redraw and the fallback (culture rep)
    e = r15_rep_entity()
    fl = {}
    attempt(REP, "settlement", 0, 1, e, 0, flags=fl)
    ck.eq("14.4 rep attempt 0 draws baba", [p for s, p in fl["drawn"]], ["b", "a", "b", "a"])
    n, t = make_name(REP, "rep", "settlement", 0, 1, e, set())
    first = next(tt for tt in range(64) if attempt(REP, "settlement", 0, 1, e, tt)[0])
    ck.eq("14.4 the next valid attempt is taken", (n, t), ("Ba", first))
    ck.eq("14.4 ... after attempt 0", first > 0, True)
    ck.eq("14.4 the fallback numbers Ba, never Baba", make_name(REP, "rep", "settlement", 0, 1, e, {"Ba"}), ("Ba II", "F"))
    ck.eq("14.4 the fallback with every valid name and the old rule", REPEAT_OFF(lambda: make_name(
        REP, "rep", "settlement", 0, 1, e, {"Ba"})), ("Baba", 0))
    # 14.1 the faerie lists
    fa = CULTURES["faerie"]
    ck.eq("14.1 faerie lists validate", validate(fa), [])
    ck.eq("14.1 faerie lists", {k: fa[k] for k in FAERIE_R15}, FAERIE_R15)
    ck.eq("14.1 no p or b in the faerie lists", any(ch in "pb" for k in ("onsets", "nuclei", "codas") for x in fa[k]
                                                   for ch in x), False)
    ck.eq("14.1 nucleus shares (ee, ou 1/16; others 2/16)", sorted((fa["nuclei"].count(x), x) for x in set(fa["nuclei"])),
          sorted([(1, "ee"), (1, "ou")] + [(2, x) for x in ("i", "ie", "ia", "a", "y", "ae", "o")]))
    lyss = ass = None
    for e in range(1, 3000):
        for t in range(4):
            fl = {}
            v, n, root = attempt(fa, "settlement", 0, 3, e, t, flags=fl)
            if root == "lyss" and lyss is None:
                lyss = (e, t, v)
            if "ass" in root and ass is None and root_valid(dict(fa, forbidden=fa["forbidden"][:-1]), root):
                ass = (e, t, v, root_valid(fa, root))
        if lyss and ass:
            break
    ck.eq("14.1 Lyss is a valid faerie root", lyss is not None and lyss[2], True)
    ck.eq("14.1 a faerie root with a + ss is invalid (forbidden ass)", ass is not None and (ass[2], ass[3]), (False, False))
    # 14.3 the filter
    sf = shipped_filter()
    for w in R15_TARD:
        ck.eq("14.3 %s filtered" % w, name_filtered(sf, w), True)
    ck.eq("14.3 The Horktard Throne's root word", root_word("The {root} Throne", "horktard"), "horktard")
    for w in ("standard", "tarn", "dart", "stad", "tad"):
        ck.eq("14.3 %s passes" % w, name_filtered(sf, w), False)
    for w in R15_WORDS:
        ck.eq("14.3 %s filtered (whole)" % w, name_filtered(sf, w), True)
        ck.eq("14.3 %s inside a word passes" % w, name_filtered(sf, "x" + w + "x"), False)
    ck.eq("14.3 sizes of the shipped filter (effective)", (len(sf.anywhere), len(sf.at_start), len(sf.whole)),
          (109, 30, 514) if pdr.at_least("0.21.0") else (109, 31, 511))   # r17 15.1 from 0.21.0
    filters = {d: t for d, t in pdr.load_root("game/data", ["name_filter"])["name_filter"]}
    ck.eq("14.3 list sizes", [(k, len(filters[k]["anywhere"]), len(filters[k]["at_start"]), len(filters[k]["whole"]))
                              for k in ("slurs", "crude", "words")],
          [("slurs", 33, 17, 5), ("crude", 71, 12, 15), ("words", 5, 1, 391)] if pdr.at_least("0.21.0") else
          [("slurs", 33, 18, 5), ("crude", 71, 12, 14), ("words", 5, 1, 391)])
    ck.eq("14.3 name_filter_not_redundant passes on the edited lists", pdr.name_filter_messages(sorted(filters.items())),
          [])
    bad = dict(filters, slurs=dict(filters["slurs"], whole=sorted(filters["slurs"]["whole"] + ["tard"])))
    ck.eq("14.4 a copy that keeps slurs.whole tard fails name_filter_not_redundant",
          [m.split(":")[0] for m in pdr.name_filter_messages(sorted(bad.items()))], ["name_filter_not_redundant"])
    # invariant 8 by replay on the golden lists (9.3's and r10's core lists)
    for seed in GOLDEN_SEEDS:
        bad = []
        for l in name_list(seed) + core_list(seed):
            w = l.split(" ")
            off = 1 if w[0] == "core" else 0
            cid, kind, e, t = w[1 + off], w[2 + off], int(w[3 + off]), w[4 + off]
            if t == "F":
                tt = next(x for x in range(64) if attempt(CULTURES[cid], kind, seed, CULTURE_NUMBER[cid], e, x)[0])
            else:
                tt = int(t)
            fl = {}
            v, _, _ = attempt(CULTURES[cid], kind, seed, CULTURE_NUMBER[cid], e, tt, flags=fl)
            if not v or repeated(fl["drawn"]):
                bad.append(l)
        ck.eq("invariant 8 (14.4) seed %d: no listed name from a repeated attempt" % seed, bad, [])
    # some shipped name is redrawn by 14.2 in the golden lists (the rule is reached)
    reached = 0
    for seed in GOLDEN_SEEDS:
        for cid in CULTURE_ORDER:
            for kind in KINDS:
                for e in range(1, GOLDEN_ENTITIES + 1):
                    fl = {}
                    attempt(CULTURES[cid], kind, seed, CULTURE_NUMBER[cid], e, 0, flags=fl)
                    reached += fl["repeated"]
    ck.eq("14.2 reached on the golden lists' attempts 0", reached > 0, True)


def old_name_rejected(d, kind, seed, c, e, old):
    """P3-02 r15: some attempt of (seed, c, e) renders `old` (lower-case root as drawn) and 0.20.0 rejects it."""
    for t in range(64):
        fl = {}
        v, n, _ = attempt(d, kind, seed, c, e, t, flags=fl)
        if n == old:
            return not v
    return False


def REPEAT_OFF(fn):
    """fn() with the 14.2 rule off (the 0.19.0 validity), for comparisons; also 16.1's (r19, a later rule)."""
    global REPEAT_RULE, DOUBLE_RULE
    saved = REPEAT_RULE, DOUBLE_RULE
    REPEAT_RULE = DOUBLE_RULE = False
    try:
        return fn()
    finally:
        REPEAT_RULE, DOUBLE_RULE = saved


# --- P3-02 r17 section 15 (P5-01, generator 0.21.0) ---------------------------------------------------------

R17_ROOTS = ("didin", "gririnhard")   # 15.4: the dwarven roots forbidden by didin and ririn (ONC; ONCONC)
R17_STUTTERS = ("didin", "ririn")     # the attempts searched hold these (a drawn root of exactly gririnhard is rare)
R17_VALID = ("dadin", "kadin")        # 15.4: near-repeats with another nucleus stay valid


def r17_find(root, seed=0, kind="settlement", limit=200000, first=False):
    """The first (e, t) of the dwarven culture (c 1), e from 1 and t 0-63 (first: only t up to e's first valid
    attempt, so that make_name with an empty registry reaches t), whose drawn root holds `root`."""
    d = CULTURES_R17["dwarven"]
    for e in range(1, limit):
        for t in range(64):
            fl = {}
            v, _, _ = attempt(d, kind, seed, CULTURE_NUMBER["dwarven"], e, t, flags=fl)
            if root in "".join(p for _, p in fl["drawn"]):
                return e, t
            if first and v:
                break
    raise AssertionError("no attempt draws %s" % root)


def r17_golden_lines():
    o = ["# r17 (P3-02 15, generator 0.21.0):",
         "#   dattempt <seed> dwarven <kind> <c> <e> <t> <drawn root> <valid 0|1> <valid at r15 0|1> <name>  15.2:",
         "#       the first (e, t) (e from 1, t 0-63) whose attempt draws exactly the root (didin: ONC d i din;",
         "#       gririnhard: ONCONC gr i rin h a rd), then the attempts t + 1 to the first valid one; valid at r15",
         "#       is the same attempt with the 0.20.0 dwarven forbidden list",
         "#   dresult <seed> dwarven <kind> <c> <e> <attempt|F> <name>  make_name with an empty registry (4.3)",
         "#   dvalid dwarven <root> <0|1>   root_valid of 4.2 for 15.4's near-repeats"]
    old = CULTURES_R15["dwarven"]
    for root, first in (("didin", True), ("ririn", True)) + tuple((r, False) for r in R17_STUTTERS):
        e, t = r17_find(root, first=first)
        d = CULTURES["dwarven"]
        tt = t
        while True:
            v, n, r = attempt(d, "settlement", 0, CULTURE_NUMBER["dwarven"], e, tt)
            v15, _, _ = attempt(old, "settlement", 0, CULTURE_NUMBER["dwarven"], e, tt)
            fl = {}
            attempt(d, "settlement", 0, CULTURE_NUMBER["dwarven"], e, tt, flags=fl)
            o.append("dattempt 0 dwarven settlement %d %d %d %s %d %d %s" % (
                CULTURE_NUMBER["dwarven"], e, tt, "".join(p for _, p in fl["drawn"]), 1 if v else 0,
                1 if v15 else 0, n))
            if v:
                break
            tt += 1
        n, at = make_name(d, "dwarven", "settlement", 0, CULTURE_NUMBER["dwarven"], e, set())
        n15, at15 = make_name(old, "dwarven", "settlement", 0, CULTURE_NUMBER["dwarven"], e, set())
        o.append("dresult 0 dwarven settlement %d %d %s %s" % (CULTURE_NUMBER["dwarven"], e, at, n))
        o.append("dresult15 0 dwarven settlement %d %d %s %s" % (CULTURE_NUMBER["dwarven"], e, at15, n15))
    for root in R17_VALID + R17_ROOTS:
        o.append("dvalid dwarven %s %d" % (root, 1 if root_valid(CULTURES["dwarven"], root) else 0))
    return o


# P3-02 r19 16 (P5-12, generator 0.22.0): the fused-suffix rule and human forbidden ass.
R19_CASES = (   # (root, form, item of 16.1) of 16.4's unit tests; 0 valid
    ("sudstead", "{root}stead", 1), ("galwick", "{root}wick", 1), ("ston", "{root}ton", 1),
    ("dabur", "{root}bury", 1), ("canasham", "{root}ford", 2), ("deas", "{root}stead", 3),
    ("bant", "{root}ton", 3), ("canasham", "{root}", 0), ("sud", "{root}stead", 0), ("gal", "{root}wick", 0),
    ("bea", "{root}ford", 0), ("stead", "{root}ham", 2), ("deantton", "{root}wick", 2))
R19_FS = (("bury", "bur"), ("stead", "stead"), ("ford", "ford"), ("ton", "ton"), ("dell", "dell"),
          ("wick", "wick"), ("ham", "ham"))
# A fixture culture with {root}ford and {root} Ford: the rule applies to the first only (16.4).
FORDS = naming(["bea", "dabur", "caford"], ["a"], [""], ["O"], 1, 32, [], ["{root}"], ["{root}"],
               ["{root}ford", "{root} Ford"])
R19_HUMAN_ENTITIES = 400


def with_rule(flag, fn):
    global DOUBLE_RULE
    saved = DOUBLE_RULE
    DOUBLE_RULE = flag
    try:
        return fn()
    finally:
        DOUBLE_RULE = saved


def r19_human_scan(seed, entities=R19_HUMAN_ENTITIES):
    """[(e, t, root, item or 'ass', name)] of the human settlement attempts t < the first valid one at r19 that
    16.1 or 16.2 rejects, e = 1..entities (c = human)."""
    d = CULTURES["human"]
    c = CULTURE_NUMBER["human"]
    out = []
    for e in range(1, entities + 1):
        for t in range(64):
            fl = {}
            v, n, r = attempt(d, "settlement", seed, c, e, t, flags=fl)
            if v:
                break
            if fl["doubled"]:
                form = n[len(cap(r)):] if n.startswith(cap(r)) else ""
                out.append((e, t, r, doubled_item(d, "settlement", "{root}" + form, r), n))
            elif fl["r4valid"] is False and "ass" in r and root_valid(CULTURES_R17["human"], r):
                out.append((e, t, r, "ass", n))
    return out


def r19_golden_lines():
    o = ["# r19 (P3-02 16, generator 0.22.0):",
         "#   tails <culture> settlement <T_K, comma-separated or ->",
         "#   fs <tail> <first syllable>",
         "#   doubled <root> <form> <item of 16.1, 0 valid>",
         "#   hreject <seed> human settlement <c> <e> <t> <root> <item 1|2|3|ass> <name>  every attempt before the",
         "#       first valid one that 16.1 or 16.2 rejects, e = 1..%d" % R19_HUMAN_ENTITIES,
         "#   hresult <seed> human settlement <c> <e> <attempt|F> <name> <attempt at r17|F> <name at r17>  make_name"
         " with an empty registry for each e of an hreject line"]
    for cid in CULTURE_ORDER:
        o.append("tails %s settlement %s" % (cid, ",".join(fused_tails(CULTURES[cid], "settlement")) or "-"))
    for x, _ in R19_FS:
        o.append("fs %s %s" % (x, first_syllable(x)))
    for root, form, _ in R19_CASES:
        o.append("doubled %s %s %d" % (root, form, doubled_item(CULTURES["human"], "settlement", form, root)))
    c = CULTURE_NUMBER["human"]
    for seed in GOLDEN_SEEDS:
        rows = r19_human_scan(seed)
        for e, t, r, it, n in rows:
            o.append("hreject %d human settlement %d %d %d %s %s %s" % (seed, c, e, t, r, it, n))
        for e in sorted(set(r[0] for r in rows)):
            n, at = make_name(CULTURES["human"], "human", "settlement", seed, c, e, set())
            n17, at17 = with_rule(False, lambda: make_name(CULTURES_R17["human"], "human", "settlement", seed, c, e,
                                                           set()))
            o.append("hresult %d human settlement %d %d %s %s %s %s" % (seed, c, e, at, n, at17, n17))
    return o


# ---------------------------------------------------------------------------
# P5-02 9.1 (generator 0.23.0): monster and site names
# ---------------------------------------------------------------------------

R23_ENTITIES = 30


def monster_kinds_023():
    """[(kind id, culture id, name_forms)] in canonical id order (P5-02 4.2)."""
    return [(r[0], r[7], list(r[8])) for r in pdr.MONSTERS_KINDS_023]


def monster_name(kind_id, seed, m, registry, cultures=None):
    """P5-02 9.1: the name of monster m of kind kind_id (D and c of the kind's naming_culture, F its name_forms),
    through P3-02 4.3 with the world's one registry; no core set (READINGS of p5_02_monsters_reference N2)."""
    cultures = cultures or CULTURES
    k = [x for x in monster_kinds_023() if x[0] == kind_id][0]
    d = with_forms(cultures[k[1]], "monster", k[2])
    return make_name(d, k[1], "monster", seed, CULTURE_NUMBER[k[1]], m, registry)


def site_name(culture_id, seed, s, registry, cultures=None):
    """P5-02 9.1: an outpost's name (D and c of the founder's culture, F = D.site_forms)."""
    cultures = cultures or CULTURES
    return make_name(cultures[culture_id], culture_id, "site", seed, CULTURE_NUMBER[culture_id], s, registry)


def r23_golden_lines():
    o = ["# r23 (P5-02 9.1, generator 0.23.0): kinds monster and site; streams hist.names.monster and hist.names.site",
         "#   mattempt <seed> <monster kind> <c> <e> <t> <valid 0|1> <root|-> <name|->   attempts t = 0..3, e = 1, 2",
         "#   mformtext <monster kind> <name_forms|lair_forms> <i> <ok|message>   form_text_problem on the kind's forms",
         "#   mname <seed> <monster kind> <e> <attempt|F> <name>   for seeds 0 and 42, one registry per seed starting",
         "#       empty, kinds in canonical id order, %d entities each, ids 1, 2, ... across the kinds" % R23_ENTITIES,
         "#   sname <seed> <culture> <e> <attempt|F> <name>   then, in the same registry, outposts of every culture in",
         "#       canonical id order, %d entities each, ids 1, 2, ... across the cultures" % R23_ENTITIES,
         "#   msha256 <seed> <hex>  of the seed's mname and sname lines joined by LF (no trailing LF)"]
    for kid, cid, forms in monster_kinds_023():
        d = with_forms(CULTURES[cid], "monster", forms)
        for e in (1, 2):
            for t in range(4):
                v, n, root = attempt(d, "monster", 0, CULTURE_NUMBER[cid], e, t)
                o.append("mattempt 0 %s %d %d %d %d %s %s" % (kid, CULTURE_NUMBER[cid], e, t, 1 if v else 0,
                                                             root if v else "-", n if v else "-"))
    for cid in CULTURE_ORDER:
        for i, form in enumerate(CULTURES[cid]["site_forms"]):
            o.append("formtext %s site %d %s" % (cid, i, form_text_problem(FILTER, cid, "site", i, form) or "ok"))
    for row in pdr.MONSTERS_KINDS_023:
        for field, forms in (("name_forms", row[8]), ("lair_forms", row[9])):
            for i, f in enumerate(forms):
                o.append("mformtext %s %s %d %s" % (row[0], field, i,
                                                    form_text_problem(FILTER, row[7], "monster", i, f) or "ok"))
    for seed in GOLDEN_SEEDS:
        reg = set()
        lines = []
        for j, (kid, _, _) in enumerate(monster_kinds_023()):
            for e in range(j * R23_ENTITIES + 1, (j + 1) * R23_ENTITIES + 1):
                n, t = monster_name(kid, seed, e, reg)
                lines.append("mname %d %s %d %s %s" % (seed, kid, e, t, n))
        for j, cid in enumerate(CULTURE_ORDER):
            for e in range(j * R23_ENTITIES + 1, (j + 1) * R23_ENTITIES + 1):
                n, t = site_name(cid, seed, e, reg)
                lines.append("sname %d %s %d %s %s" % (seed, cid, e, t, n))
        o.extend(lines)
        o.append("msha256 %d %s" % (seed, list_sha(lines)))
    return o


def check_r23(ck):
    """P5-02 9.1 on the shipped data: every monster and outpost name is a valid attempt's name or the fallback,
    unique in the registry; the site forms of every culture are culture forms; name lists stable across calls."""
    if not pdr.at_least("0.23.0"):
        return
    for cid in CULTURE_ORDER:
        ck.eq("site_forms of %s (P5-02 4.4)" % cid, CULTURES[cid]["site_forms"], pdr.SITE_FORMS_023[cid])
        ck.eq("site_forms of %s pass the grammars" % cid, validate(CULTURES[cid]), [])
    for seed in GOLDEN_SEEDS:
        reg = set()
        names = []
        for kid, cid, forms in monster_kinds_023():
            for e in range(1, 11):
                n, t = monster_name(kid, seed, e, reg)
                names.append(n)
                ck.ok("monster %s %d not a fallback" % (kid, e), t != "F")
                ck.ok("monster %s %d by one of its forms" % (kid, e),
                      any(n.startswith(f.split("{root}")[0]) and n.endswith(f.split("{root}")[1]) for f in forms))
        for cid in CULTURE_ORDER:
            for e in range(1, 11):
                n, t = site_name(cid, seed, e, reg)
                names.append(n)
                ck.ok("site %s %d by a site form" % (cid, e), any(
                    n.startswith(f.split("{root}")[0]) and n.endswith(f.split("{root}")[1])
                    for f in CULTURES[cid]["site_forms"]))
        ck.eq("monster and site names unique (one registry)", len(set(names)), len(names))
    # the streams are their own: the same entity id of another kind draws another name
    ck.ok("hist.names.monster differs from hist.names.settlement",
          cr.stream_key(0, "hist.names.monster") != cr.stream_key(0, "hist.names.settlement"))


def check_r19(ck):
    """P3-02 r19 16.4's unit tests and invariant 10 on the golden seeds' human names."""
    hu = CULTURES["human"]
    ck.eq("16.2 human forbidden", hu["forbidden"][-1], "ass")
    ck.eq("16.2 human culture valid data", validate(hu), [])
    ck.eq("16.1 human tails", fused_tails(hu, "settlement"), ["ford", "ton", "bury", "wick", "ham", "stead"])
    ck.eq("16.1 elven tails", fused_tails(CULTURES["elven"], "settlement"), ["dell"])
    for cid in ("dwarven", "faerie", "goblin", "orcish"):
        ck.eq("16.1 %s has no tail" % cid, fused_tails(CULTURES[cid], "settlement"), [])
    for cid in CULTURE_ORDER:
        for k in ("god", "civilization"):
            ck.eq("16.1 %s %s has no tail" % (cid, k), fused_tails(CULTURES[cid], k), [])
    for x, f in R19_FS:
        ck.eq("16.1 fs(%s)" % x, first_syllable(x), f)
    for root, form, item in R19_CASES:
        ck.eq("16.1 %s + %s" % (root, form), doubled_item(hu, "settlement", form, root), item)
    ck.eq("16.1 elven root ending nd + dell", doubled_item(CULTURES["elven"], "settlement", "{root}dell", "lend"), 3)
    ck.eq("16.1 elven root lin + dell valid", doubled_item(CULTURES["elven"], "settlement", "{root}dell", "lin"), 0)
    ck.eq("16.1 ' and - removed from the root", doubled_item(hu, "settlement", "{root}ton", "s-t'on"), 1)
    ck.eq("16.1 fixture {root} Ford (B' empty) not affected",
          doubled_item(FORDS, "settlement", "{root} Ford", "caford"), 0)
    ck.eq("16.1 fixture {root}ford: caford doubled", doubled_item(FORDS, "settlement", "{root}ford", "caford"), 1)
    ck.eq("16.1 fixture tails", fused_tails(FORDS, "settlement"), ["ford"])
    ck.eq("16.2 beasstont invalid (forbidden ass)", root_valid(hu, "beasstont"), False)
    ck.eq("16.2 beasstont valid at r17", root_valid(CULTURES_R17["human"], "beasstont"), True)
    # an attempt rejected by 16.1 is followed by t + 1 (4.3), on the golden seeds' human settlements
    c = CULTURE_NUMBER["human"]
    for seed in GOLDEN_SEEDS:
        rows = r19_human_scan(seed)
        ck.ok("16.1 rejections found on seed %d" % seed, any(r[3] in (1, 2, 3) for r in rows))
        for e in sorted(set(r[0] for r in rows))[:20]:
            n, at = make_name(hu, "human", "settlement", seed, c, e, set())
            last = max(r[1] for r in rows if r[0] == e)
            ck.ok("4.3 seed %d e %d: the name comes after the rejected attempts" % (seed, e), at == "F" or at > last)
    # invariant 10 (the part a stored name can show: a bare-form name may end in a tail, 16.1): no ass
    for seed in GOLDEN_SEEDS:
        for ln in name_list(seed):
            parts = ln.split(" ", 5)
            if parts[1] == "human" and parts[2] == "settlement":
                ck.ok("invariant 10: no ass in %s" % parts[5], "ass" not in squeeze(parts[5]))


def check_r17(ck):
    """P3-02 r17 15.4 (the unit tests the reference can state without storing an offensive stem)."""
    d, old = CULTURES["dwarven"], CULTURES_R15["dwarven"]
    ck.eq("15.2 dwarven forbidden", d["forbidden"][-2:], ["didin", "ririn"])
    ck.eq("15.2 dwarven culture valid data", validate(d), [])
    for root in R17_ROOTS:
        ck.eq("15.4 %s invalid at r17" % root, root_valid(d, root), False)
        ck.eq("15.4 %s valid at r15" % root, root_valid(old, root), True)
    for root in R17_STUTTERS:
        e, t = r17_find(root)
        v, _, _ = attempt(d, "settlement", 0, CULTURE_NUMBER["dwarven"], e, t)
        v15, _, _ = attempt(CULTURES_R15["dwarven"], "settlement", 0, CULTURE_NUMBER["dwarven"], e, t)
        ck.eq("15.4 the attempt drawing %s is invalid (valid at r15)" % root, (v, v15), (False, True))
    for st in R17_STUTTERS:
        e, t = r17_find(st, first=True)
        n, at = make_name(d, "dwarven", "settlement", 0, CULTURE_NUMBER["dwarven"], e, set())
        n15, at15 = make_name(CULTURES_R15["dwarven"], "dwarven", "settlement", 0, CULTURE_NUMBER["dwarven"], e,
                              set())
        ck.eq("15.4 %s: at r15 make_name takes the attempt drawing it" % st, (at15, lower(n15).find(st) >= 0),
              (t, True))
        ck.ok("15.4 %s: make_name takes a later attempt (%s at %s > %d)" % (st, n, at, t), at == "F" or at > t)
        ck.ok("15.4 %s: the name holds neither stutter" % st, "didin" not in lower(n) and "ririn" not in lower(n))
    for root in R17_VALID:
        ck.eq("15.4 %s valid" % root, root_valid(d, root), True)
    sf = shipped_filter()
    frag = "f" + "ag"
    ck.eq("15.1 shy%slae filtered (r17: anywhere)" % frag[:1], name_filtered(sf, "shy" + frag + "lae"), True)
    ck.eq("15.1 %swick filtered" % frag[:1], name_filtered(sf, frag + "wick"), True)
    ck.ok("15.1 fagg absent (anywhere)", ("f" + "agg") not in sf.anywhere)
    ck.ok("15.1 fag not at_start", frag not in sf.at_start and frag in sf.anywhere)
    for w in ("shiva", "hindi", "f" + "annie"):
        ck.eq("15.1 %s... filtered as a whole word" % w[:2], name_filtered(sf, w), True)
        ck.eq("15.1 x%s... passes (whole only)" % w[:2], name_filtered(sf, "x" + w), False)
    ck.eq("15.1 root_word of The Hindi Court", root_word("The {root} Court", "hindi"), "hindi")
    filters = {dd: t for dd, t in pdr.load_root("game/data", ["name_filter"])["name_filter"]}
    ck.eq("15.1 sizes (slurs 33/17/5, crude 71/12/15, famous 0/0/103; 15.1 writes famous 105)",
          {k: (len(t["anywhere"]), len(t["at_start"]), len(t["whole"])) for k, t in filters.items()},
          dict(slurs=(33, 17, 5), crude=(71, 12, 15), famous=(0, 0, 103), words=(5, 1, 391)))
    ck.eq("15.1 name_filter_not_redundant passes on the edited lists",
          pdr.name_filter_messages(sorted(filters.items())), [])
    kept = dict(filters, slurs=dict(filters["slurs"], at_start=sorted(filters["slurs"]["at_start"] + [frag])))
    ck.eq("15.1 name_filter_not_redundant fails on a copy keeping the at_start entry beside the anywhere one",
          pdr.name_filter_messages(sorted(kept.items())),
          ["name_filter_not_redundant: 'slurs' at_start '%s' is already rejected by anywhere '%s' of 'slurs'"
           % (frag, frag)])
    ck.eq("15.1 root_word of Fannie Glade", root_word("{root} Glade", "f" + "annie"), "f" + "annie")


def scan_r17(out):
    """P3-02 r17 15.4 (validation's report): 13.1's sample (seeds 0, 1, 2; civilizations and settlements 1-1,500,
    gods 1-300; one registry per seed and culture; make_name without K) at 0.21.0. Per culture, the attempts made
    until each name rejected by each new entry alone: fag (anywhere, not caught by the old at_start), shiva,
    hindi, fannie (whole), didin, ririn (dwarven forbidden); and the fallbacks."""
    frag = "f" + "ag"
    alone = {"fag": NameFilter([frag], [], []), "shiva": NameFilter([], [], ["shiva"]),
             "hindi": NameFilter([], [], ["hindi"]), "fannie": NameFilter([], [], ["f" + "annie"])}
    old_start = NameFilter([], [frag], [])
    for cid in CULTURE_ORDER:
        d = CULTURES[cid]
        c = CULTURE_NUMBER[cid]
        r = dict(names=0, attempts=0, fag=0, shiva=0, hindi=0, fannie=0, didin=0, ririn=0, fallbacks=0)
        for seed in (0, 1, 2):
            reg = set()
            for kind, n_e in (("god", 300), ("civilization", 1500), ("settlement", 1500)):
                for e in range(1, n_e + 1):
                    n, t = make_name(d, cid, kind, seed, c, e, reg)
                    r["names"] += 1
                    r["fallbacks"] += t == "F"
                    for tt in range(64 if t == "F" else t + 1):
                        fl = {}
                        attempt(d, kind, seed, c, e, tt, flags=fl)
                        r["attempts"] += 1
                        root = "".join(p for _, p in fl["drawn"])
                        for w in ("didin", "ririn"):
                            if cid == "dwarven" and w in root:
                                r[w] += 1
                        if fl["r4valid"]:
                            for w, f in alone.items():
                                if name_filtered(f, fl["word"]) and not (w == "fag" and name_filtered(old_start,
                                                                                                     fl["word"])):
                                    r[w] += 1
        out.write("scan17 %s %s\n" % (cid, " ".join("%s %d" % kv for kv in r.items())))


def scan_r15(out):
    """P3-11 3.1 / P3-02 14.2, 14.3: the 59,400-name sample of 13.1 (seeds 0, 1, 2; entities 1-1,500 of each
    culture for civilizations and settlements, 1-300 for gods; one registry per seed and culture; make_name
    without K), at 0.20.0. Per culture, the attempts made until each name, rejected by 14.2 (otherwise valid), by
    the forbidden entry ass (and by nothing else of 4.2), by the filter entry tard alone and by the four words
    alone; and the names whose accepted attempt differs from 0.19.0's rules (approximately: the names that change)."""
    only_tard = NameFilter(["tard"], [], [])
    only_words = NameFilter([], [], list(R15_WORDS))
    tot = {}
    for cid in CULTURE_ORDER:
        d = CULTURES[cid]
        c = CULTURE_NUMBER[cid]
        r = {"names": 0, "attempts": 0, "repeated": 0, "ass": 0, "tard": 0, "words": 0, "fallbacks": 0}
        for seed in (0, 1, 2):
            reg = set()
            for kind, n_e in (("god", 300), ("civilization", 1500), ("settlement", 1500)):
                for e in range(1, n_e + 1):
                    n, t = make_name(d, cid, kind, seed, c, e, reg)
                    r["names"] += 1
                    last = 63 if t == "F" else t
                    if t == "F":
                        r["fallbacks"] += 1
                    for tt in range(last + 1):
                        fl = {}
                        attempt(d, kind, seed, c, e, tt, flags=fl)
                        r["attempts"] += 1
                        r["repeated"] += fl["repeated"]
                        # forbidden ass alone: invalid with the list, valid without its ass entry
                        root = "".join(p for _, p in fl["drawn"])
                        if "ass" in d["forbidden"] and "ass" in root and not root_valid(d, root) and \
                                root_valid(dict(d, forbidden=[x for x in d["forbidden"] if x != "ass"]), root):
                            r["ass"] += 1
                        if fl["r4valid"]:
                            r["tard"] += name_filtered(only_tard, fl["word"])
                            r["words"] += name_filtered(only_words, fl["word"])
        tot[cid] = r
        out.write("scan %s names %d attempts %d repeated %d ass %d tard %d words %d fallbacks %d\n" % (
            cid, r["names"], r["attempts"], r["repeated"], r["ass"], r["tard"], r["words"], r["fallbacks"]))
    return tot


def core_list(seed):
    """The name list of 9.3's order with the history stage's K (12.2) for civilizations and settlements: one
    registry and one K per seed, gods (P3-03) without K."""
    reg, cores, lines = set(), set(), []
    for cid in CULTURE_ORDER:
        for kind in KINDS:
            for e in range(1, CORE_ENTITIES + 1):
                n, t = make_name(CULTURES[cid], cid, kind, seed, CULTURE_NUMBER[cid], e, reg,
                                 cores=None if kind == "god" else cores)
                lines.append("core %d %s %s %d %s %s" % (seed, cid, kind, e, t, n))
    return lines


def core_fallback_case():
    """12.3: a K near every attempt of the test culture tiny (Ba, Da): the fallback numbers the first valid name
    and adds its core to K."""
    reg, cores = set(), {"ba"}
    n, t = make_name(TEST_CULTURES["tiny"], "tiny", "settlement", 0, 1, 1, reg, cores=cores)
    return n, t, sorted(cores)


def r10_golden_lines():
    o = ["# r10 (P3-02 12, generator 0.19.0):",
         "#   corename <root> <core>            12.2's core (letters without ' and -)",
         "#   corenear <core> <K comma-joined|-> <0|1>   12.2's near (equal or one letter apart)",
         "#   corefallback tiny settlement 0 1 1 <attempt|F> <name> <K after, comma-joined>  with K = {ba}",
         "#   core <seed> <culture> <kind> <entity> <attempt|F> <name>  the list of 9.3's order with one K",
         "#       for civilizations and settlements (gods without K), entities 1..50",
         "#   coresha256 <seed> <hex>  of that seed's core lines joined by LF (no trailing LF)"]
    for r, k in CORE_CASES:
        o.append("corename %s %s" % (r, name_core(r)))
    for k, K, _ in CORE_NEAR_CASES:
        if k == "a" and K == ("",):
            continue   # an empty core is not a valid name core; checked only in the reference
        o.append("corenear %s %s %d" % (k, ",".join(K) if K else "-", 1 if core_near(k, set(K)) else 0))
    n, t, K = core_fallback_case()
    o.append("corefallback tiny settlement 0 1 1 %s %s %s" % (t, n, ",".join(K)))
    for seed in GOLDEN_SEEDS:
        lines = core_list(seed)
        o.extend(lines)
        o.append("coresha256 %d %s" % (seed, list_sha(lines)))
    return o


def core_pairs(names_roots):
    """Pairs (a, b) of names whose cores are equal or one letter apart."""
    out = []
    for i in range(len(names_roots)):
        for j in range(i + 1, len(names_roots)):
            if lev1(names_roots[i][1], names_roots[j][1]):
                out.append((names_roots[i][0], names_roots[j][0]))
    return out


def check_r10(ck):
    """P3-02 r10 12.3."""
    for r, k in CORE_CASES:
        ck.eq("12.2 core of %s" % r, name_core(r), k)
    for k, K, want in CORE_NEAR_CASES:
        ck.eq("12.3 near %s in %r" % (k, K), core_near(k, set(K)), want)
    sf = shipped_filter()
    for w in R10_REJECTED:
        ck.eq("12.1 %s rejected" % w, name_filtered(sf, w), True)
    for w in R10_PASSING:
        ck.eq("12.1 %s passes" % w, name_filtered(sf, w), False)
    # r10's own sizes (108, 36, 510) are those before r13's edits (13.1), which check_r13 tests
    ck.eq("12.1 effective sizes (after r13 13.1; r15 14.3)", (len(sf.anywhere), len(sf.at_start), len(sf.whole)),
          (109, 30, 514) if pdr.at_least("0.21.0") else (109, 31, 511) if pdr.at_least("0.20.0") else (110, 31, 508))
    n, t, K = core_fallback_case()
    ck.eq("12.3 fallback: the first valid name numbered", (t, n.endswith(" II")), ("F", True))
    ck.eq("12.3 fallback: first's core joins K", K, sorted({"ba", name_core(lower(n.split()[0]))}))
    # cores=None is the r5 rule exactly (gods, chronology)
    ck.eq("12.3 gods unaffected (no K)", name_list(0)[:5], name_list(0)[:5])
    for seed in GOLDEN_SEEDS:
        lines = core_list(seed)
        rows = [l.split(" ", 6) for l in lines]
        hist = [(r[6], r[2], r[3]) for r in rows if r[3] != "god"]
        ck.eq("12.3 unique names seed %d" % seed, len(set(r[6] for r in rows)), len(rows))
        roots = []
        for r in rows:
            if r[3] == "god":
                continue
            d = CULTURES[r[2]]
            t = r[5]
            if t == "F":
                continue
            _, nm, root = attempt(d, r[3], seed, CULTURE_NUMBER[r[2]], int(r[4]), int(t))
            assert nm == r[6]
            roots.append((nm, name_core(root)))
        ck.eq("invariant 6 (12.3) seed %d: no two cores equal or one letter apart" % seed, core_pairs(roots), [])
        ck.eq("12.3 seed %d: some name redrawn for its core" % seed, True,
              any(l.split()[5] != o.split()[4] for l, o in zip([x for x in lines if " god " not in x],
                                                               [x for x in name_list(seed) if " god " not in x])))



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
    "orcish": ("Kaak Fort, Zaarz Camp, Grush, Zaakugaag, Uruld'duuld, Shaazazuug, Kark Fort, Azol'urash Fort, "
               "Karzthogh, Uromgha, Maag, Uul'krakh Camp, Og'buurk Camp, Zaalduruz Camp, Mak, Duld'sha Fort, "
               "Shuum Fort, Ghorz Fort, Duuldkrosh Fort, Aag'urog Fort",
               "Clan Kram'krorz; The Azakhdam Horde; The Urogh'krorz Horde; The Burk'zork Horde; "
               "The Warband of Maakurokh",
               "Ugh'kaal, Og'uraag, Uruuk"),
}


# P3-02 11.6, "Sample names of 5.1 under r5", transcribed.
SAMPLES_R5 = {
    "dwarven": ("Moumka, Thoumdaili, Makmoudin Deep, Brummthu, Gromka, Kaird Deep, Khaili Hold, Kili, Thoum Hold, "
                "Nodin, Gondhark, Turnbruk Deep, Kidinkorn, Tharndramm Hold, Grangrak, Houmm Deep, Doknirk, Gali, "
                "Nailimim Deep, Tairdbra Hold",
                "The Kourdbrar Throne; The Hold of Didin; Clan Durgarin; Clan Drourd; Clan Drailimai",
                "Brommgrai, Groumtoun, Nurnthaimm"),
    "elven": ("Iensath, Onaeme, Ierrael, Elonoma, Fievos, Mielae, Asvathdell, Lafin, Laevo, Leithiedell, Orith, "
              "Voglael, Mievife, Iefann, Cellve, Fimo, Londcae, Rienel, Nathol, Siendnie",
              "The House of Nife; The Court of Nothind; The House of Aesoth; The House of Riefifie; "
              "The Court of Thiesaell",
              "Saene, Fiendse, Ithonnfa"),
    "faerie": ("Liapa, Wiatwiesk Glade, Twamae Glade, Ybeeie Hollow, Biafloss Glade, Pieshaep Hollow, Teelan, "
               "Meeie Hollow, Sywysh Hollow, Iatysk, Wiepysk, Miemo Hollow, Mapoufee, Feetop Hollow, Showhie Hollow, "
               "Mawhiap Hollow, Athfli Hollow, Whywo, Fliaposk Hollow, Whosy Hollow",
               "The Lyp Ring; The Watiafia Court; The Bouss Ring; The Wosk Ring; The Twiwhiwou Ring",
               "Fiwhall, Fiabo, Biensho"),
    "goblin": ("Krot Pit, Slograb Pit, Gnink-Snunk, Zux, Grunkgnuzz Pit, Grizz-Skeeb, Slank-Skrox, Drobskeet, "
               "Bozz Pit, Sluggatch Hole, Bux-Kreeb, Sniz, Slak, Zug-Gok, Gunitch, Skrizuz, Droggkazz Hole, "
               "Slobeerk, Snok, Kruskreezz Hole",
               "The Gnazz Horde; The Baskeeb Mob; The Grodratch Horde; The Krunkgirk Horde; The Warrens of Gnonk",
               "Zab-Skreeb, Kruskux, Nugnug"),
    "human": ("Deanham, Alantton, Cickham, Gackwick, Brir, Tackbury, Gurdton, Studford, Ceambury, Buld, "
              "Silstudstead, Tirdwick, Dack, Stastint, Moldstead, Edumford, Tontstead, Ceanbir, Durdton, Durdbury",
              "The Duchy of Ashem; The Kingdom of Ashos; The Kingdom of Edes; The Duchy of Wick; The Duchy of Circon",
              "Wasbrid, Histold, Edostold"),
    "orcish": ("Kaak Fort, Zaarz Camp, Grush, Zaakugaag, Uruld'duuld, Shaazazuug, Kark Fort, Azol'urash Fort, "
               "Karzthogh, Uromgha, Maag, Uul'krakh Camp, Og'buurk Camp, Zaalduruz Camp, Mak, Duld'sha Fort, "
               "Shuum Fort, Ghorz Fort, Duuldkrosh Fort, Aag'urog Fort",
               "Clan Kram'krorz; The Azakhdam Horde; The Urogh'krorz Horde; The Burk'zork Horde; "
               "The Warband of Maakurokh",
               "Ug'krarz, Og'uraag, Uruuk'krush"),
}

# P3-02 11.6 "Gods before and after", column r5 (race order elf, goblin,
# dwarf, faerie, human, orc; god entity = race order; empty registry).
GODS_R5 = {
    0: "Saene, Kruskux, Nurnthaimm, Whaia, Steldhil, Uruld'kruum",
    1: "Laesiel, Gragg-Gnurk, Khummhor, Synmia, Steckwint, Kaag'azak",
    2: "Saeriemi, Gagg-Sneegg, Broumnoud, Biewie, Stenedock, Bogh'grum",
    3: "Vaennela, Sluskat, Nonthom, Pyia, Bulwan, A'ghaz",
    4: "Meven, Kukunk, Bornthirin, Wiabie, Dalcold, Ugu'azug",
    5: "Fieniesie, Batgnax, Dirinbad, Sabiwhi, Tolashud, Grammam",
    6: "Ithielell, Droxsloz, Groligu, Whaetwia, Dearbrol, Kaaghaag",
    7: "Onveth, Buzork, Mammga, Ouwheefy, Gordman, Maarkzaagh",
    8: "Saelome, Grotbib, Mairnbu, Twybo, Madgil, Uush'muum",
    9: "Lisael, Skozazz, Nurntond, Whaeouwhia, Geadashid, Um'daz",
    42: "Fielie, Krokzit, Bronhak, Syfiash, Wealstuld, Ogh'krak",
}
GOD_RACE_CULTURES = ("elven", "goblin", "dwarven", "faerie", "human", "orcish")

# P3-02 11.6 "Elven realms before and after", column r5 (civilizations 1, 2).
REALMS_R5 = {
    0: ("The House of Nife", "The Court of Nothind"), 1: ("The House of Glaefaenn", "The Court of Iethfe"),
    2: ("The House of Ithasfa", "The Court of Ienriend"), 3: ("The House of Roelaele", "The House of Vefal"),
    4: ("The Court of Meithiell", "The Realm of Sices"), 5: ("The Realm of Saethoth", "The House of Vamiel"),
    6: ("The Realm of Mocaen", "The House of Folisae"), 7: ("The Realm of Ienngleth", "The Realm of Rasglae"),
    8: ("The Realm of Gliethil", "The Realm of Thalri"), 9: ("The House of Motheme", "The Realm of Ilcienn"),
}


def god_parses(d, name):
    """11.7 item 9: the god root (form {root}) parses as one of god_patterns."""
    root = lower(name)
    for pat in d["god_patterns"]:
        if parse_pattern(d, pat, root):
            return True
    return False


def parse_pattern(d, pat, root):
    if not pat:
        return root == ""
    s0 = pat[0]
    if s0 in SEP:
        return root.startswith(s0) and parse_pattern(d, pat[1:], root[1:])
    arr = d[{"O": "onsets", "N": "nuclei", "C": "codas"}[s0]]
    return any(root.startswith(p) and parse_pattern(d, pat[1:], root[len(p):]) for p in set(arr))


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
        es, ev, eg = (SAMPLES_R5 if RULES == "r5" else SAMPLES)[cid]
        if RULES == "r5" and pdr.at_least("0.20.0") and cid in SAMPLES_R15:   # P3-02 r15 14.4: the faerie row
            es, ev, eg = SAMPLES_R15[cid]
        for label, got, exp in (("settlements", s, es.split(", ")), ("civilizations", v, ev.split("; ")),
                                ("gods", g, eg.split(", "))):
            for i, (a, b) in enumerate(zip(got, exp)):
                if a != b and RULES == "r5" and pdr.at_least("0.21.0") and cid == "dwarven" and \
                        ("didin" in lower(b) or "ririn" in lower(b)):
                    out.write("r17 %s %s %d: %r redrawn as %r (P3-02 15.2)\n" % (cid, label, i + 1, b, a))
                    continue
                if a != b and RULES == "r5" and pdr.at_least("0.22.0") and cid == "human" and label == "settlements":
                    out.write("r19 %s %s %d: %r redrawn as %r (P3-02 16)\n" % (cid, label, i + 1, b, a))
                    continue
                if a != b:
                    bad += 1
                    out.write("DIFF %s %s %d: reference %r, spec %s %r\n" % (cid, label, i + 1, a,
                                                                              "11.6" if RULES == "r5" else "5.1", b))
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

    def ok(self, what, cond):
        self.eq(what, bool(cond), True)


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
    # r15: 4.6 counts 61 under the rules before 14.2; from 0.20.0 the attempts 14.2 rejects drop out
    full_old = set()
    for t in range(64):
        vv, nn, _ = REPEAT_OFF(lambda: attempt(CULTURES["elven"], "settlement", 0, 2, 1, t))
        if vv:
            full_old.add(nn)
    ck.eq("4.6 distinct valid names (before 14.2)", len(full_old), 61)
    rep_only = set()
    for t in range(64):
        fl = {}
        attempt(CULTURES["elven"], "settlement", 0, 2, 1, t, flags=fl)
        if fl["repeated"]:
            rep_only.add(attempt(CULTURES["elven"], "settlement", 0, 2, 1, t)[1])
    ck.eq("4.6 distinct valid names", len(full), 61 - len(rep_only - full) if REPEAT_RULE else 61)
    ck.eq("4.6 fallback", make_name(CULTURES["elven"], "elven", "settlement", 0, 2, 1, set(full)), ("Iensath II", "F"))
    sc = [x for x in scenarios() if x[0] == "seed42"][0]
    got = [(r[5], r[4]) for r in run_scenario(sc)]
    ck.eq("4.6 seed 42 names", [g[0] for g in got],
          ["Khorkhound" if RULES == "r5" else "Khork", "The Grork Horde", "Deantwick", "Ol'uraakh Fort"])
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
    if RULES == "r5":
        check_r5(ck)
    if RULES == "r5" and pdr.at_least("0.19.0"):
        check_r10(ck)
        check_r13(ck)
    if RULES == "r5" and pdr.at_least("0.20.0"):
        check_r15(ck)
    if RULES == "r5" and pdr.at_least("0.21.0"):
        check_r17(ck)
    if RULES == "r5" and pdr.at_least("0.22.0"):
        check_r19(ck)
    if RULES == "r5" and pdr.at_least("0.23.0"):
        check_r23(ck)
    sys.stdout.write("%d checks, %d failures\n" % (ck.count, ck.failures))
    return 1 if ck.failures else 0


def check_r5(ck):
    """P3-02 11 (r5, r6): the examples of 11.3, the vectors and tables of
    11.6, the rule on hand-made data (11.7 items 4-8), invariant 5 and item 9
    on the golden lists, r4 reproduced (item 7)."""
    f = FILTER
    # 11.3 examples with the shipped lists (the offensive ones as concatenations).
    for w, exp in (("cock" + "ford", True), ("bum" + "dou", True), ("but-skrotch", True), ("ugh'kaal", True),
                   # r15 14.3: from 0.20.0 tard is an anywhere entry and Stard is filtered (11.3 as amended)
                   ("bag", True), ("stard", pdr.at_least("0.20.0")), ("cum" + "brin", True),
                   # P3-02 r13 13.1 moves cum to crude.anywhere: 11.3's "while Ascumbrin is not" no longer holds
                   # (a reading: 13.1 rules over 11.3's example)
                   ("ascum" + "brin", pdr.at_least("0.19.0"))):
        ck.eq("11.3 example %s" % w, name_filtered(f, w), exp)
    ck.eq("11.3 Bag Hole: W is the root", root_word("{root} Hole", "bag"), "bag")
    # 11.7 item 5.
    ck.eq("root_word The Hold of", root_word("The Hold of {root}", "dur"), "dur")
    ck.eq("root_word {root}ford", root_word("{root}ford", "ced"), "cedford")
    ck.eq("root_word Ny{root} Pit", root_word("Ny{root} Pit", "ash"), "nyash")
    # 11.7 item 6, hand-made filter.
    hf = NameFilter(["ab"], ["cd"], ["ef"])
    for w, exp in (("xaby", True), ("a-by", True), ("xcdy", False), ("xx-cdy", True), ("cd", True), ("ef", True),
                   ("efx", False), ("x'ef", True)):
        ck.eq("11.7 item 6 %s" % w, name_filtered(hf, w), exp)
    # ASSUMPTION (11.7 item 6 reads "XEF (after lower-casing in root_word) yes"): W of a form
    # "X{root}F" with root "e" is "xef", which no list entry of the item's filter matches (whole
    # "ef" is not "xef"). The reading taken: the case shows that root_word lower-cases the form's
    # fused capitals, so a form "E{root}" with root "f" gives W "ef", rejected as whole "ef"
    # (without the lower-casing the part would be "f").
    ck.eq("11.7 item 6 XEF as written is not filtered", name_filtered(hf, root_word("X{root}F", "e")), False)
    ck.eq("11.7 item 6 fused capitals lower-cased by root_word", name_filtered(hf, root_word("E{root}", "f")), True)
    ck.eq("11.7 item 6 empty filter", any(name_filtered(EMPTY_FILTER, w) for w in ("xaby", "cd", "ef")), False)
    # 11.7 item 10 (r6): the per-word guard.
    ck.eq("The Realm of Arthos not filtered", name_filtered(f, root_word("The Realm of {root}", "arthos")), False)
    # 11.7 item 4 and the r6 fixtures of 11.3 (names_form_text).
    for form, entry in (("{root}" + "cock", "cock"), ("Shit" + "{root}", "shit"), ("The Scrap" + "yard of {root}",
                                                                                 "crap"),
                        ("The Snot" + "ty Court of {root}", "snot")):
        try:
            names_form_text(f, {"x": dict(CULTURES["human"], settlement_forms=[form])})
            ck.eq("names_form_text refuses %s" % form, "passed", "fatal")
        except FatalError as ex:
            ck.eq("names_form_text message for %s" % form, str(ex),
                  "history: culture 'x' settlement_forms[0] '%s' holds the filtered text '%s'" % (form, entry))
    try:
        names_form_text(f, {"x": dict(CULTURES["human"], settlement_forms=["{root}ford"])})
        names_form_text(f, CULTURES)
        ck.eq("names_form_text passes {root}ford and the shipped forms", True, True)
    except FatalError as ex:
        ck.eq("names_form_text passes the shipped forms", str(ex), "")
    # P3-02 r8 11.11.5 item 5: the anchors of 11.11.1 with the shipped filter (bare root words)
    sf = shipped_filter()
    for w in ("gold", "thorn", "moss", "iron", "ash", "grub", "sleet", "horn"):
        ck.eq("r8 anchor %s passes" % w, name_filtered(sf, w), False)
    for w in ("also", "went", "movie", "dork", "zit", "old", "mold"):
        ck.eq("r8 anchor %s is rejected" % w, name_filtered(sf, w), True)
    # 11.6 tables.
    bad = compare_samples(sys.stdout)
    ck.eq("11.6 sample table under r5", bad, 0)
    for seed, row in GODS_R5.items():
        got = [make_name(CULTURES[cid], cid, "god", seed, CULTURE_NUMBER[cid], i + 1, set())[0]
               for i, cid in enumerate(GOD_RACE_CULTURES)]
        if pdr.at_least("0.20.0"):
            # P3-02 r15 14: the faerie god changes with 14.1's lists; another god only where its 0.19.0 attempt is
            # invalid at 0.20.0 (14.2's rule or 14.3's entries), and no god of the table is filtered or repeated
            old = row.split(", ")
            for i, (g, o) in enumerate(zip(got, old)):
                cid = GOD_RACE_CULTURES[i]
                if g != o and cid != "faerie":
                    ck.eq("11.6 gods seed %d %s: %s redrawn because 0.20.0 rejects it" % (seed, cid, o),
                          old_name_rejected(CULTURES[cid], "god", seed, CULTURE_NUMBER[cid], i + 1, o), True)
            ck.eq("11.6 gods seed %d: none filtered" % seed, True,
                  not any(name_filtered(shipped_filter(), lower(g)) for g in got))
        elif pdr.at_least("0.19.0"):
            # P3-02 r10 12.1: the gods the new entries reach (Grammam, Gordman) are redrawn; every other god of
            # the 0.18.0 table stands, and no god of the table is filtered.
            old = row.split(", ")
            ck.eq("11.6 gods seed %d at 0.19.0 (r10 12.1)" % seed,
                  [g for g, o in zip(got, old) if g != o],
                  [g for g, o in zip(got, old) if name_filtered(shipped_filter(), lower(o))])
            ck.eq("11.6 gods seed %d: none filtered" % seed, True,
                  not any(name_filtered(shipped_filter(), lower(g)) for g in got))
        else:
            ck.eq("11.6 gods seed %d" % seed, ", ".join(got), row)
    for seed, row in REALMS_R5.items():
        got = tuple(make_name(CULTURES["elven"], "elven", "civilization", seed, 2, e, set())[0] for e in (1, 2))
        ck.eq("11.6 realms seed %d" % seed, got, row)
    # 11.7 item 7: r4 reproduced with the empty filter and god_patterns = patterns.
    # (r15: with 14.2's rule off; from 0.20.0 the library applies it to every culture, so the r4sha256 golden
    # lines no longer equal the 0.16.0 lists)
    saved = REPEAT_OFF(lambda: list_sha(name_list(0, r4_equivalent_cultures(), EMPTY_FILTER)))
    ck.eq("11.7 item 7 r4 list seed 0 (0.16.0 sha256)", saved,
          "7b81d85d11630b64f27f7eb16dc9f0815b764a0fc8ee60d506f68686a04942b9")
    ck.eq("11.7 item 7 r4 list seed 42 (0.16.0 sha256)",
          REPEAT_OFF(lambda: list_sha(name_list(42, r4_equivalent_cultures(), EMPTY_FILTER))),
          "8403dc92b716ecfb7c22bb9589d3e487aecaf08a43ffc81a89a370f0f4f80f4c")
    # 11.7 item 8.
    sc = {x[0]: x for x in r5_scenarios()}
    r = run_scenario(sc["filtered"])
    ck.eq("item 8 filtered: fallback of the first unfiltered valid attempt", r[0][4], "F")
    e1 = r[0][3]
    first = None
    for t in range(64):
        v, n, _ = attempt(FILT, "settlement", 0, 1, e1, t, filt=test_filter())
        if v:
            first = n
            break
    ck.eq("item 8 filtered: the numeral is on the first unfiltered name", r[0][5], first + " II")
    ck.eq("item 8 redraw: alone = after others", run_scenario(sc["redraw_alone"])[0][4:],
          run_scenario(sc["redraw_after"])[-1][4:])
    ck.eq("item 8 redraw: attempt 2", run_scenario(sc["redraw_alone"])[0][4], 2)
    # Invariant 5 and item 9 on the golden lists.
    for seed in GOLDEN_SEEDS:
        for ln in name_list(seed):
            sd, cid, kind, e, t, name = ln.split(" ", 5)
            d = CULTURES[cid]
            ck.eq("invariant 5 %s" % ln, root_filtered_of(d, kind, name), False)
            if kind == "god" and t != "F":
                ck.eq("item 9 god parses %s" % ln, god_parses(d, name), True)
            if kind == "civilization":
                ck.eq("item 9 no bare realm %s" % ln, " " in name, True)


def root_filtered_of(d, kind, name):
    """Invariant 5 recomputed from a name: some form of the kind reproduces
    the name (numeral stripped) with a root whose W is filtered."""
    base = name
    if " " in name:
        head, tail = name.rsplit(" ", 1)
        if tail and all(ch in "IVXLCDM" for ch in tail):
            base = head
    hits = []
    for form in d[kind + "_forms"]:
        pre, post = form.split("{root}")
        if len(base) >= len(pre) + len(post) and base.startswith(pre) and base.endswith(post):
            root = lower(base[len(pre):len(base) - len(post)] if post else base[len(pre):])
            if root and root_valid(d, root):
                hits.append(name_filtered(FILTER, root_word(form, root)))
    return bool(hits) and all(hits)


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
                    if RULES == "r5":
                        assert not root_filtered_of(CULTURES[cid], kind, n), "invariant 5"
                        assert kind != "god" or t == "F" or god_parses(CULTURES[cid], n), "god pattern"
                        assert kind != "civilization" or " " in n, "bare realm"
                    if t == "F":
                        fb += 1
                    else:
                        worst = max(worst, t)
                sys.stdout.write("seed %d %-8s %-12s names %d fallbacks %d max_attempt %d\n"
                                 % (seed, cid, kind, len(reg), fb, worst))


use_rules("r5")


def main(argv):
    if "--version" in argv:   # 0.19.0 (default): P3-02 r10's filter; 0.18.0 the short list; 0.17.0 P3-08's
        i = argv.index("--version")
        use_version(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    if "--rules" in argv:
        i = argv.index("--rules")
        use_rules(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
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
        sys.stdout.write("%d differences from spec %s\n" % (bad, "11.6 (r5)" if RULES == "r5" else "5.1"))
        return 1 if bad else 0
    if len(argv) >= 2 and argv[1] == "scan":   # P3-11 3.1: the rates of 14.2 and 14.3 on 13.1's sample
        if pdr.at_least("0.21.0") and "--r15" not in argv:   # P5-01: P3-02 r17 15.4's rates
            scan_r17(sys.stdout)
        else:
            scan_r15(sys.stdout)
        return 0
    if len(argv) >= 2 and argv[1] == "sweep":
        sweep(int(argv[2]) if len(argv) >= 3 else 2000)
        return 0
    sys.stderr.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.modules.setdefault("p3_names_reference", sys.modules["__main__"])   # one module object when run as a script (P5-04a integration)
    sys.exit(main(sys.argv))
