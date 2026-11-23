#!/usr/bin/env python3
"""Banned-construct lint for the world generator and the command line.

Scans C++ sources for constructs that can make output differ by platform,
compiler, standard library or thread count: the bans of spec P0-01 section 4
(docs/specs/p0-01-deterministic-core.md, 4.2 as amended by section 15) and
P1-00 section 8 (docs/specs/p1-00-geography-overview.md, rules D1 to D10),
where they can be found mechanically with few false positives. Comments,
string literals and character literals are ignored by the code rules; a
separate pass reads the SQL text in the string literals of the database and
inspection code. It also checks the project's CMake files for floating-point
flags and the pinned toml++ configuration.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B tools/lint/determinism_lint.py              # default scope
    python3 -B tools/lint/determinism_lint.py PATH ...     # scan given files/dirs
    python3 -B tools/lint/determinism_lint.py --self-test  # check the lint itself
    python3 -B tools/lint/determinism_lint.py --list-rules

The default scope is worldgen/ without worldgen/tests/, tools/worldgen-cli/
without tools/worldgen-cli/tests/, the project's CMake files, and the toml++
configuration check. Exit status: 0 clean, 1 findings (or self-test failure),
2 usage error.

Suppression (see README.md): a comment of the form

    // determinism-lint: allow(rule-a, rule-b) -- reason why this is safe

on the offending line, or alone on the line(s) directly above it. The reason
is mandatory. A suppression that names an unknown rule, has no reason, or
suppresses nothing is itself reported.

Python 3 standard library only.
"""

from __future__ import annotations

import argparse
import bisect
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

sys.dont_write_bytecode = True

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Default scope, relative to the repository root, and what is excluded.
DEFAULT_ROOTS = ("worldgen", "tools/worldgen-cli")
EXCLUDED_DIRS = ("worldgen/tests", "tools/worldgen-cli/tests")  # test code writes no world
CXX_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".ipp", ".inl", ".tpp"}

# CMake files of the project (vendored trees under extern/*/ are not ours).
CMAKE_FILES = ("CMakeLists.txt", "CMakePresets.json", "extern/CMakeLists.txt")
CMAKE_DIRS = ("cmake", "worldgen", "tools", "gdext")
CMAKE_EXCLUDED_DIRS = ("tools/lint/selftest",)

# The toml++ configuration: the guard in this source must be matched by the
# pinned INTERFACE definitions of this CMake target.
TOML_GUARD_FILE = "worldgen/src/data/toml_document.cpp"
TOML_CMAKE_FILE = "extern/CMakeLists.txt"
TOML_CMAKE_TARGET = "unmapped_tomlplusplus"

# Areas. A file under worldgen/ is library code, a file under
# tools/worldgen-cli/ is command-line code; any other path (explicit paths,
# self-test snippets) gets every rule of both.
LIB, CLI, OTHER = "lib", "cli", "other"
ALL_AREAS = frozenset({LIB, CLI, OTHER})
LIB_AREAS = frozenset({LIB, OTHER})
CLI_AREAS = frozenset({CLI, OTHER})


def area_of(rel: str) -> str:
    if rel.startswith("worldgen/"):
        return LIB
    if rel.startswith("tools/worldgen-cli/"):
        return CLI
    return OTHER


# Files allowed to use compiler-specific 128-bit types and intrinsics. Spec
# P0-01 5.3 permits a fast path only inside mul_u64_wide and divmod_u128_u64,
# which live in this file. Paths are repository-relative, '/' separated.
WIDE_TYPE_ALLOWLIST = frozenset({"worldgen/src/core/wide.cpp"})

# The one threading facility (P0-01 4.2, P1-00 D10): the only file that may
# start threads or share state between them. Its header needs neither.
PARALLEL_ALLOWLIST = frozenset({"worldgen/src/core/parallel.cpp"})

# SQL text is read in the database layer and the inspection code.
SQL_DIRS = ("worldgen/src/db/", "worldgen/src/inspect/",
            "worldgen/include/worldgen/db/", "worldgen/include/worldgen/inspect/")

# History code (P3-00 4.5, 4.6): the history library and the two history
# stages, chronology (P3-03) and history (P3-04 to P3-06). Prefixes, so that
# worldgen/src/stages/history_*.cpp is covered too.
HISTORY_DIRS = ("worldgen/src/history/", "worldgen/include/worldgen/history/",
                "worldgen/src/stages/chronology", "worldgen/src/stages/history",
                "worldgen/include/worldgen/stages/chronology", "worldgen/include/worldgen/stages/history")

MIN_REASON_LEN = 10  # characters; "safe" alone is not a reason

SUPPRESS_RE = re.compile(r"determinism-lint\s*:\s*allow\s*\(([^)]*)\)\s*(?:--\s*(.*))?$")
SUPPRESS_MARK_RE = re.compile(r"determinism-lint\s*:")

# Kinds of rule.
CODE, SQL, LITERAL, CMAKE, REPO = "code", "sql", "literal", "cmake", "repo"


@dataclass(frozen=True)
class Rule:
    name: str
    summary: str
    message: str
    pattern: Optional[re.Pattern] = None  # on the code with comments and literals blanked
    includes: tuple = ()  # header names reported by this rule
    areas: frozenset = ALL_AREAS
    exempt: frozenset = frozenset()  # files where the rule does not apply
    only_dirs: tuple = ()  # if set, applies only under these prefixes (and to OTHER)
    filter: Optional[Callable] = None  # filter(code, match) -> bool: keep the match
    kind: str = CODE
    review: bool = False  # each use needs a suppression with a reason
    where: str = "library and CLI"

    def applies(self, rel: str) -> bool:
        a = area_of(rel)
        if a not in self.areas or rel in self.exempt:
            return False
        if self.only_dirs and a != OTHER and not rel.startswith(self.only_dirs):
            return False
        return True


def _rx(p: str, flags: int = 0) -> re.Pattern:
    return re.compile(p, flags)


# --- filters ---------------------------------------------------------------

_PREV_WORD = re.compile(r"(\w+)\s*$")
_NEXT_WORD = re.compile(r"\s*(\w+)")
_LAST_WORD = re.compile(r"(\w+)$")
_CALL_KEYWORDS = {"return", "throw", "case", "co_return", "co_yield", "else", "do", "and", "or", "not"}


def _plain_long(code: str, m: re.Match) -> bool:
    """True if this 'long' token is not part of 'long long' or 'long double'
    (any whitespace, including newlines, between the words)."""
    prev = _PREV_WORD.search(code, max(0, m.start() - 64), m.start())
    nxt = _NEXT_WORD.match(code, m.end())
    if prev and prev.group(1) == "long":
        return False
    if nxt and nxt.group(1) in ("long", "double"):
        return False
    return True


def _free_call_at(code: str, start: int) -> bool:
    """True if the function name starting at `start` is a call of the global
    or std:: function, False if it is a member access (x.time(), p->clock()),
    another namespace's function (other::time()), or a declaration
    (int time(); Foo* clock();). Heuristic: a name preceded by an identifier
    other than a keyword like 'return', or by '*', '&' or '>', is taken to be
    a declaration."""
    b = code[max(0, start - 200):start].rstrip()
    if b.endswith(".") or b.endswith("->"):
        return False
    if b.endswith("::"):
        q = _LAST_WORD.search(b[:-2].rstrip())
        return q is None or q.group(1) == "std"  # ::time( and std::time( are calls
    w = _LAST_WORD.search(b)
    if w:
        return w.group(1) in _CALL_KEYWORDS
    if b.endswith(("*", ">")) or (b.endswith("&") and not b.endswith("&&")):
        return False
    return True


def _free_call(code: str, m: re.Match) -> bool:
    """For rules whose pattern has a group 'fn': the free-call test on it.
    Matches without that group (std::chrono, .imbue() are kept."""
    if "fn" not in m.re.groupindex or m.start("fn") < 0:
        return True
    return _free_call_at(code, m.start("fn"))


def _int_abs_filter(code: str, m: re.Match) -> bool:
    """For int-abs: keep std::abs( / ::abs( and free calls of labs( and the
    other C-only names; drop other namespaces' functions (worldgen::abs on Fx)
    and members. Unqualified abs( is not reported: it is the project's Fx abs
    found by argument-dependent lookup."""
    if m.start("fn") >= 0:
        return _free_call_at(code, m.start("fn"))
    q = m.group("q")
    if q is None:
        before = code[max(0, m.start() - 64):m.start()].rstrip()
        return not (before.endswith(".") or before.endswith("->"))
    return q.strip() == "std"


def _std_thread_filter(code: str, m: re.Match) -> bool:
    """std::thread::hardware_concurrency and std::thread::id belong to their
    own rules; everything else that the thread rule matches is kept."""
    if m.group(0).replace(" ", "").endswith("std::thread"):
        if re.match(r"\s*::\s*(?:hardware_concurrency|id)\b", code[m.end():m.end() + 64]):
            return False
    return _free_call(code, m)


def _matching_paren(code: str, open_pos: int) -> int:
    """Index of the ')' matching the '(' at open_pos, or -1."""
    depth = 0
    for k in range(open_pos, len(code)):
        ch = code[k]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return k
    return -1


def _matching_brace(code: str, open_pos: int) -> int:
    """Index of the '}' matching the '{' at open_pos, or -1."""
    depth = 0
    for k in range(open_pos, len(code)):
        ch = code[k]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return k
    return -1


def _range_for_colon(header: str) -> int:
    """Offset of the ':' of a range-for header (top level, not '::'), or -1
    for a classic for (a top-level ';') or no colon."""
    depth = 0
    k = 0
    while k < len(header):
        ch = header[k]
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == ";" and depth == 0:
            return -1
        elif ch == ":":
            if header[k + 1:k + 2] == ":":
                k += 2
                continue
            if depth == 0:
                return k
        k += 1
    return -1


# The live entity containers of history::State (P3-04 6.1) and the calls
# that change them: found_* appends to the civilization or settlement
# vector (a reallocation invalidates every iterator, span and reference
# into it), open_relation / close_relation insert into and erase from the
# relation map (an erase invalidates the iterator of the erased key).
_LIVE_RANGE = re.compile(r"(?:(?:\.|->)\s*(?:civs|settlements|relations)\s*\(\s*\)|\b(?:civs_|settlements_|relations_))\s*$")
_LIVE_MUTATOR = re.compile(r"\b(?:found_civilization|found_settlement|open_relation|close_relation)\s*\(")


# Founding by proxy (added by the P3-09 conflict balance audit). A function
# defined in the same file whose body calls found_civilization( or
# found_settlement( (directly, or through another such function of the
# file) founds too: P3-09's flight founds a settlement in flee(), which
# siege() calls, which battle() calls, which fight() calls inside a loop.
# The two history rules treat a call of such a function as a founding.
# Same file only: a helper defined elsewhere is still not seen.
_DEF_NAME = re.compile(r"\b(?P<name>[A-Za-z_]\w*)\s*\(")
_NOT_FUNCTIONS = frozenset({
    "if", "for", "while", "switch", "catch", "return", "sizeof", "alignof", "decltype", "static_assert",
    "noexcept", "requires", "defined", "throw", "new", "delete", "co_return", "co_await",
})
_DEF_TAIL = re.compile(r"\s*(?:(?:const|noexcept|override|final|volatile|&&?)\b\s*|&\s*)*(?:->[^;{}()]*)?\{")
_CALL_NAME = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
_proxy_cache: dict = {}


def _function_bodies(code: str) -> list:
    """(name, body start, body end) of every function definition in the
    lexed code: a name, its parameter list, optional qualifiers, then '{'."""
    out: list = []
    for m in _DEF_NAME.finditer(code):
        name = m.group("name")
        if name in _NOT_FUNCTIONS:
            continue
        close = _matching_paren(code, m.end() - 1)
        if close < 0:
            continue
        t = _DEF_TAIL.match(code, close + 1)
        if not t:
            continue
        brace = t.end() - 1
        end = _matching_brace(code, brace)
        out.append((name, brace, end if end >= 0 else len(code)))
    return out


def _founding_proxies(code: str) -> frozenset:
    """Names of the functions of this file that found, transitively."""
    hit = _proxy_cache.get(code)
    if hit is not None:
        return hit
    bodies = _function_bodies(code)
    calls = {}
    founders = set()
    for name, start, end in bodies:
        body = code[start:end]
        calls.setdefault(name, set()).update(c for c in _CALL_NAME.findall(body) if c != name)
        if _FOUNDING.search(body):
            founders.add(name)
    changed = True
    while changed:
        changed = False
        for name, called in calls.items():
            if name not in founders and called & founders:
                founders.add(name)
                changed = True
    result = frozenset(founders - {"found_civilization", "found_settlement"})
    _proxy_cache[code] = result
    return result


def _founding_call(code: str) -> re.Pattern:
    """found_civilization( / found_settlement( or a call of a founding proxy."""
    names = sorted(_founding_proxies(code))
    if not names:
        return _FOUNDING
    return re.compile(r"\bfound_(?:civilization|settlement)\s*\(|\b(?:" + "|".join(map(re.escape, names)) + r")\s*\(")


def _live_iteration(code: str, m: re.Match) -> bool:
    """For history-live-iteration: True if this 'for (' is a range-for over
    State::civs(), settlements() or relations() (or the members civs_,
    settlements_, relations_) whose body calls found_civilization,
    found_settlement, open_relation or close_relation."""
    open_pos = m.end() - 1
    close = _matching_paren(code, open_pos)
    if close < 0:
        return False
    header = code[open_pos + 1:close]
    colon = _range_for_colon(header)
    if colon < 0 or not _LIVE_RANGE.search(header[colon + 1:].rstrip()):
        return False
    k = close + 1
    while k < len(code) and code[k].isspace():
        k += 1
    if k < len(code) and code[k] == "{":
        end = _matching_brace(code, k)
        body = code[k:end if end >= 0 else len(code)]
    else:
        end = code.find(";", k)
        body = code[k:end if end >= 0 else len(code)]
    if _LIVE_MUTATOR.search(body):
        return True
    # A founding proxy matters for the vectors only (civs, settlements).
    if re.search(r"relations", header[colon + 1:]):
        return False
    return bool(_founding_call(code).search(body))


_FOUNDING = re.compile(r"\bfound_(?:civilization|settlement)\s*\(")


def _block_end(code: str, pos: int) -> int:
    """Offset of the '}' that closes the block containing pos (or the end)."""
    depth = 0
    for k in range(pos, len(code)):
        ch = code[k]
        if ch == "{":
            depth += 1
        elif ch == "}":
            if depth == 0:
                return k
            depth -= 1
    return len(code)


def _held_reference(code: str, m: re.Match) -> bool:
    """For history-held-reference: True if the reference, pointer or span
    declared by this match is used after a found_civilization( or
    found_settlement( call that follows it in the same block (the use is
    after the call's closing parenthesis, so the arguments do not count)."""
    name = m.group("ref") or m.group("span") or m.group("auto")
    end = _block_end(code, m.end())
    use = re.compile(r"\b" + re.escape(name) + r"\b")
    for f in _founding_call(code).finditer(code, m.end(), end):
        close = _matching_paren(code, f.end() - 1)
        if close < 0 or close >= end:
            continue
        if use.search(code, close + 1, end):
            return True
    return False


# Subsystem-owned entity vectors (added by the P5-02 beasts and monsters
# audit). MonstersSubsystem keeps its monsters and sites in the members
# monsters_ and sites_ and hands out Monster& / Site& through monster(id)
# and site(id); an append to the vector may reallocate it, exactly like a
# founding for State's vectors. Type name -> the member it points into.
_MEMBER_VECTORS = {"Monster": "monsters_", "Site": "sites_"}
_MEMBER_ACCESSORS = {"monster": "monsters_", "site": "sites_"}
_MEMBER_GROW = r"\s*(?:\.|->)\s*(?:push_back|emplace_back|emplace|insert|resize|assign|clear|erase|reserve)\s*\("
_member_proxy_cache: dict = {}


def _member_growers(code: str, member: str) -> frozenset:
    """Names of the functions of this file whose body grows `member`
    (directly or through another such function of the file)."""
    key = (member, code)
    hit = _member_proxy_cache.get(key)
    if hit is not None:
        return hit
    grow = re.compile(r"\b" + re.escape(member) + _MEMBER_GROW)
    calls = {}
    growers = set()
    for name, start, end in _function_bodies(code):
        body = code[start:end]
        calls.setdefault(name, set()).update(c for c in _CALL_NAME.findall(body) if c != name)
        if grow.search(body):
            growers.add(name)
    changed = True
    while changed:
        changed = False
        for name, called in calls.items():
            if name not in growers and called & growers:
                growers.add(name)
                changed = True
    result = frozenset(growers)
    _member_proxy_cache[key] = result
    return result


def _member_held_reference(code: str, m: re.Match) -> bool:
    """For history-member-held-reference: True if the Monster& / Site&
    (or auto& from monster( / site() declared by this match is used after a
    later call that grows the vector it points into, in the same block."""
    name = m.group("ref") or m.group("auto")
    member = _MEMBER_VECTORS.get(m.group("type") or "") or _MEMBER_ACCESSORS.get(m.group("acc") or "")
    if not name or not member:
        return False
    names = sorted(_member_growers(code, member))
    alt = r"\b" + re.escape(member) + _MEMBER_GROW
    if names:
        alt += r"|\b(?:" + "|".join(map(re.escape, names)) + r")\s*\("
    grow = re.compile(alt)
    end = _block_end(code, m.end())
    use = re.compile(r"\b" + re.escape(name) + r"\b")
    for g in grow.finditer(code, m.end(), end):
        close = _matching_paren(code, g.end() - 1)
        if close < 0 or close >= end:
            continue
        if use.search(code, close + 1, end):
            return True
    return False


_STMT_START_CHARS = (";", "{", "}", ")", ":")
_VOID_CAST_TAIL = re.compile(r"static_cast\s*<\s*void\s*>\s*\(\s*$")


def _discarded_call(code: str, m: re.Match) -> bool:
    """For unchecked-write: True if the call is an expression statement whose
    value is thrown away: `fwrite(...);`, `(void)fwrite(...);`,
    `if (x) fputs(...);`, `static_cast<void>(fputs(...));`."""
    start = m.start("fn")
    if not _free_call_at(code, start):
        return False
    close = _matching_paren(code, m.end() - 1)
    if close < 0:
        return False
    after = code[close + 1:close + 64].lstrip()
    before = code[max(0, start - 200):start].rstrip()
    if before.endswith("::"):  # std::fwrite, ::fwrite
        before = before[:-2].rstrip()
        if before.endswith("std"):
            before = before[:-3].rstrip()
    if _VOID_CAST_TAIL.search(before):
        return after.startswith(")")
    if not after.startswith(";"):
        return False
    if before == "" or before.endswith(_STMT_START_CHARS):
        return not before.endswith("::")
    w = _LAST_WORD.search(before)
    return bool(w and w.group(1) in ("else", "do"))


# --- the code rules ----------------------------------------------------------

CMATH_FUNCS = (
    "sin|cos|tan|asin|acos|atan|atan2|sinh|cosh|tanh|asinh|acosh|atanh|"
    "exp|exp2|expm1|log|log2|log10|log1p|logb|ilogb|pow|sqrt|cbrt|hypot|"
    "fma|fmod|remainder|remquo|floor|ceil|round|lround|llround|trunc|"
    "nearbyint|rint|lrint|llrint|erf|erfc|tgamma|lgamma|frexp|ldexp|"
    "modf|scalbn|scalbln|copysign|nextafter|nexttoward|fdim|fmax|fmin|"
    "isnan|isinf|isfinite|signbit|fpclassify|lerp"  # not midpoint: <numeric>, exact on integers
)

STD = r"\bstd\s*::\s*"
STD_R = r"\bstd\s*::\s*(?:ranges\s*::\s*)?"

NUM_TEXT_C_FUNCS = (
    "strtod|strtof|strtold|strtol|strtoll|strtoul|strtoull|strtoimax|strtoumax|"
    "wcstod|wcstof|wcstold|wcstol|wcstoll|wcstoul|wcstoull|"
    "atof|atoi|atol|atoll|_wtof|_wtoi|_wtol|_wtoll|_atoi64|_wtoi64|"
    "printf|fprintf|sprintf|snprintf|vprintf|vfprintf|vsprintf|vsnprintf|"
    "wprintf|fwprintf|swprintf|vwprintf|vfwprintf|vswprintf|"
    "printf_s|fprintf_s|sprintf_s|snprintf_s|_snprintf|_snprintf_s|_scprintf|"
    "scanf|fscanf|sscanf|vscanf|vfscanf|vsscanf|wscanf|fwscanf|swscanf|"
    "scanf_s|fscanf_s|sscanf_s|"
    "ecvt|fcvt|gcvt|_ecvt|_fcvt|_gcvt|strfromd|strfromf|strfroml"
)

RULES: tuple = (
    # P0-01 section 4: the original rules.
    Rule(
        "float-type",
        "float, double, long double (and other floating types)",
        "floating-point type in the output path (P0-01 4: integer and fixed-point only)",
        _rx(r"\b(?:float|double|__float128|__float80|_Float(?:16|32|64|128)x?|std\s*::\s*float(?:16|32|64|128)_t|std\s*::\s*bfloat16_t)\b"),
    ),
    Rule(
        "banned-include",
        "#include of <cmath>, <math.h>, <random>, <chrono>, <ctime>, <time.h>, <unordered_map>, <unordered_set>, <locale>, <clocale>",
        "banned header in the output path (P0-01 4)",
        includes=("cmath", "math.h", "random", "chrono", "ctime", "time.h",
                  "unordered_map", "unordered_set", "locale", "clocale"),
    ),
    Rule(
        "std-math",
        "std:: transcendental and rounding functions from <cmath>",
        "standard-library math function in the output path; use the project's own (P0-01 4, 9)",
        _rx(STD + r"(?:" + CMATH_FUNCS + r")\b(?=\s*[(<])"),
    ),
    Rule(
        "std-random",
        "<random> engines and distributions, std::shuffle, std::sample, std::random_shuffle",
        "standard-library randomness in the output path; use worldgen::Rng (P0-01 4, 6)",
        _rx(
            STD_R + r"(?:shuffle|sample|random_shuffle)\b"
            r"|" + STD + r"(?:\w+_distribution|mt19937(?:_64)?|random_device|minstd_rand0?|default_random_engine"
            r"|ranlux\w*|knuth_b|linear_congruential_engine|mersenne_twister_engine|subtract_with_carry_engine"
            r"|discard_block_engine|independent_bits_engine|shuffle_order_engine|seed_seq|generate_canonical)\b"
        ),
    ),
    Rule(
        "c-rand",
        "rand(), srand()",
        "C library rand/srand in the output path; use worldgen::Rng (P0-01 4)",
        _rx(r"\b(?P<fn>s?rand)\s*\("),
        filter=_free_call,
    ),
    Rule(
        "wall-clock",
        "time(), clock(), std::chrono, timespec_get, gettimeofday",
        "wall-clock or timing in the output path (P0-01 4)",
        _rx(r"\b(?P<fn>time|clock|timespec_get|gettimeofday|clock_gettime)\s*\(|\bstd\s*::\s*chrono\b"),
        filter=_free_call,
    ),
    Rule(
        "std-hash",
        "std::hash",
        "std::hash differs between standard libraries; use the project's hashing (P0-01 4, 7)",
        _rx(STD + r"hash\b"),
    ),
    Rule(
        "unordered-container",
        "std::unordered_map/set/multimap/multiset",
        "unordered container in the output path; iteration order differs between libraries (P0-01 4)",
        _rx(STD + r"(?:pmr\s*::\s*)?unordered_(?:map|set|multimap|multiset)\b"),
    ),
    Rule(
        "locale",
        "setlocale, std::locale, imbue",
        "locale-dependent behaviour in the output path (P0-01 4)",
        _rx(r"\b(?P<fn>setlocale)\s*\(|\bstd\s*::\s*locale\b|(?:\.|->)\s*imbue\s*\("),
        filter=_free_call,
    ),
    # Character classification and case mapping (added by the P3-08 audit):
    # the <cctype> and <cwctype> functions follow the C locale (LC_CTYPE), and
    # passing a negative char (a byte >= 0x80 where char is signed: GCC and
    # Clang on x86, MSVC) is undefined behaviour, so lower-casing a name with
    # them could differ between platforms or crash. The project classifies
    # ASCII by explicit ranges (data/text.hpp, history/names.cpp).
    Rule(
        "ctype",
        "#include of <cctype>, <ctype.h>, <cwctype>, <wctype.h>; calls of isalpha(, isdigit(, ... tolower(, "
        "toupper( and the wide forms (also std:: and ::)",
        "locale-dependent character classification or case mapping in the output path, undefined for a "
        "negative char; compare explicit ASCII ranges such as c >= 'A' && c <= 'Z' (P0-01 4)",
        _rx(r"\b(?P<fn>isw?(?:alnum|alpha|blank|cntrl|digit|graph|lower|print|punct|space|upper|xdigit)"
            r"|tow?(?:lower|upper)|iswctype|towctrans|wctype|wctrans)\s*\("),
        filter=_free_call,
        includes=("cctype", "ctype.h", "cwctype", "wctype.h"),
    ),
    Rule(
        "long-type",
        "long, unsigned long, long int (not long long, not long double)",
        "'long' is 32-bit on Windows and 64-bit on Linux; use std::int64_t / std::int32_t (P0-01 4)",
        _rx(r"\blong\b"),
        filter=_plain_long,
    ),
    Rule(
        "wchar-type",
        "wchar_t",
        "wchar_t is 16-bit on Windows and 32-bit on Linux (P0-01 4)",
        _rx(r"\bwchar_t\b"),
    ),
    Rule(
        "wide-int-type",
        "__int128 and 128-bit multiply/divide intrinsics outside the allow-listed file",
        "compiler-specific 128-bit arithmetic outside the allow-listed fast path (P0-01 5.3)",
        _rx(r"\b(?:__int128(?:_t)?|__uint128_t|_umul128|_mul128|_udiv128|_div128|__umulh|__mulh)\b"),
        exempt=WIDE_TYPE_ALLOWLIST,
        where="library and CLI, except worldgen/src/core/wide.cpp",
    ),
    Rule(
        "unstable-sort",
        "std::sort, partial_sort, partial_sort_copy, nth_element, partition (and std::ranges:: forms)",
        "unstable algorithm: equal elements are ordered differently by different standard libraries; "
        "use std::stable_sort or a strict total-order comparator, then suppress with the reason (P0-01 4)",
        _rx(STD_R + r"(?:sort|partial_sort|partial_sort_copy|nth_element|partition)\b"),
        review=True,
    ),
    Rule(
        "int-abs",
        "std::abs, std::labs, std::llabs, std::imaxabs, ::abs (and the other ::-qualified forms); "
        "calls of labs(, llabs(, imaxabs(",
        "library integer abs: abs of the most negative value is undefined, and which integer overloads "
        "the global namespace holds differs between standard libraries, so an int64 can bind to "
        "int abs(int) and be truncated on one platform only; write v < 0 ? -v : v with a stated bound, "
        "or the project's Fx abs (P0-01 4, 5)",
        _rx(r"(?P<q>\b\w+\s*)?::\s*(?:abs|labs|llabs|imaxabs)\s*\(|\b(?P<fn>labs|llabs|imaxabs)\s*\("),
        filter=_int_abs_filter,
    ),
    # Threads and shared state (P0-01 4.2 as amended, P1-00 D10).
    Rule(
        "packed-bool",
        "std::vector<bool> (also std::pmr::), std::bitset; #include <bitset>",
        "bit-packed container: writing different elements from different threads (run_blocks, "
        "for_each_row_block) is a data race on the shared word, so a result could depend on scheduling; "
        "use std::vector<std::uint8_t> (P0-01 4.2, P1-00 D1, D2, D8)",
        _rx(STD + r"(?:pmr\s*::\s*)?vector\s*<\s*bool\s*>|" + STD + r"bitset\b"),
        includes=("bitset",),
    ),
    # Pointer order (P0-01 4: no dependence on pointer values). Added by the
    # P1-06 regions audit.
    Rule(
        "pointer-order",
        "std::map/set/multimap/multiset (also std::pmr::) whose key type is a pointer; std::less, greater, "
        "less_equal, greater_equal, compare_three_way of a pointer type; std::uintptr_t, std::intptr_t",
        "ordering or keying by pointer value: addresses differ between runs, allocators and platforms, so "
        "an iteration order or a comparison result would too; key by an index, an id or the value itself "
        "(P0-01 4)",
        _rx(STD + r"(?:pmr\s*::\s*)?(?:map|set|multimap|multiset)\s*<\s*(?:const\s+|volatile\s+)*"
            r"[\w:]+(?:\s*<[^<>;]*>)?\s*(?:const\s*|volatile\s*)*\*"
            r"|" + STD + r"(?:less|greater|less_equal|greater_equal|compare_three_way)\s*<[^<>;]*\*\s*>"
            r"|\b(?:std\s*::\s*)?u?intptr_t\b"),
    ),
    Rule(
        "thread",
        "std::thread, std::jthread, std::async, std::packaged_task, std::promise, std::future, std::execution::, "
        "#pragma omp, pthread_create, CreateThread, _beginthread(ex); #include <thread>, <future>, <execution>, "
        "<stop_token>, <omp.h>, <pthread.h> - outside the one parallel facility",
        "thread or parallel execution outside worldgen/src/core/parallel.cpp; use run_blocks / map_blocks "
        "(P0-01 4.2, P1-00 D10)",
        _rx(STD + r"(?:thread|jthread|async|packaged_task|promise|future|shared_future|launch|stop_token|stop_source)\b"
            r"|" + STD + r"execution\b"
            r"|^[ \t]*#[ \t]*pragma[ \t]+omp\b"
            r"|\b(?P<fn>pthread_create|CreateThread|_beginthread|_beginthreadex)\s*\(", re.M),
        includes=("thread", "future", "execution", "stop_token", "omp.h", "pthread.h"),
        exempt=PARALLEL_ALLOWLIST,
        filter=_std_thread_filter,
        where="library and CLI, except worldgen/src/core/parallel.cpp",
    ),
    Rule(
        "shared-state",
        "std::atomic, std::atomic_ref, std::atomic_flag, std::memory_order, std::mutex and the other mutexes, "
        "locks, condition variables, semaphores, latches, barriers, std::call_once, _Atomic, __atomic_/__sync_ "
        "builtins, _Interlocked*; #include <atomic>, <mutex>, <shared_mutex>, <condition_variable>, "
        "<semaphore>, <latch>, <barrier>, <stdatomic.h> - outside the one parallel facility",
        "state shared between threads outside worldgen/src/core/parallel.cpp: results would depend on "
        "scheduling (P0-01 4.2, P1-00 D8, D10)",
        _rx(STD + r"(?:atomic\w*|memory_order\w*|kill_dependency|mutex|recursive_mutex|timed_mutex|recursive_timed_mutex"
            r"|shared_mutex|shared_timed_mutex|lock_guard|unique_lock|scoped_lock|shared_lock|condition_variable(?:_any)?"
            r"|counting_semaphore|binary_semaphore|latch|barrier|call_once|once_flag|notify_all_at_thread_exit)\b"
            r"|\b_Atomic\b|\b__(?:atomic|sync)_\w+\s*\(|\b_Interlocked\w*\s*\("),
        includes=("atomic", "mutex", "shared_mutex", "condition_variable", "semaphore", "latch", "barrier",
                  "stdatomic.h"),
        exempt=PARALLEL_ALLOWLIST,
        where="library and CLI, except worldgen/src/core/parallel.cpp",
    ),
    Rule(
        "hardware-concurrency",
        "hardware_concurrency, sysconf(_SC_NPROCESSORS_*), get_nprocs, GetSystemInfo, GetActiveProcessorCount",
        "the machine's thread count must never reach a result: the library takes the thread count from its "
        "caller; the CLI may use it only as the --threads default, with a suppression (P0-01 4, P1-00 D10)",
        _rx(r"\bhardware_concurrency\b|\b_SC_NPROCESSORS_\w+|\b(?P<fn>get_nprocs(?:_conf)?|GetSystemInfo"
            r"|GetNativeSystemInfo|GetActiveProcessorCount|GetMaximumProcessorCount)\s*\("),
        filter=_free_call,
        review=True,
        where="library and CLI (CLI only with a suppression)",
    ),
    Rule(
        "thread-identity",
        "std::this_thread::, std::thread::id, .get_id(), thread_local, _Thread_local, __thread, "
        "__declspec(thread), pthread_self, GetCurrentThreadId, gettid",
        "thread identity or thread-local state: a result could depend on which thread ran it (P0-01 4.2, "
        "P1-00 D10)",
        _rx(STD + r"this_thread\b|" + STD + r"thread\s*::\s*id\b|(?:\.|->)\s*get_id\s*\(|\bthread_local\b"
            r"|\b_Thread_local\b|\b__thread\b|\b__declspec\s*\(\s*thread\s*\)"
            r"|\b(?P<fn>pthread_self|GetCurrentThreadId|gettid)\s*\("),
        filter=_free_call,
        where="library and CLI, parallel.cpp included",
    ),
    Rule(
        "unordered-reduction",
        "std::reduce, std::transform_reduce, std::inclusive_scan, std::exclusive_scan, "
        "std::transform_inclusive_scan, std::transform_exclusive_scan",
        "reduction in an unspecified order: non-associative results differ by library; use std::accumulate "
        "or combine in ascending block order (P0-01 4.2 rule 3, P1-00 D8)",
        _rx(STD + r"(?:reduce|transform_reduce|inclusive_scan|exclusive_scan|transform_inclusive_scan"
            r"|transform_exclusive_scan)\b"),
    ),
    Rule(
        "heap-order",
        "std::priority_queue, std::make_heap, std::push_heap, std::pop_heap, std::sort_heap (and std::ranges:: forms)",
        "heap order among equal keys differs between standard libraries; the comparator must be a strict "
        "total order ending in a push sequence number (P1-00 D6); suppress with the reason",
        _rx(STD + r"priority_queue\b|" + STD_R + r"(?:make_heap|push_heap|pop_heap|sort_heap)\b"),
        review=True,
    ),
    Rule(
        "environment",
        "getenv, std::getenv, secure_getenv, _wgetenv, getenv_s, _dupenv_s, GetEnvironmentVariable*, "
        "GetEnvironmentStrings*, environ",
        "the library reads no environment variable: the caller passes the value as an argument "
        "(P0-03 7.6, P1-00 6.3)",
        _rx(r"\b(?P<fn>getenv|secure_getenv|_wgetenv|getenv_s|_wgetenv_s|_dupenv_s|_wdupenv_s)\s*\("
            r"|\bGetEnvironment(?:Variable|Strings)\w*\b|\b_?w?environ\b"),
        filter=_free_call,
        areas=LIB_AREAS,
        where="library only (the CLI reads the environment and passes it on)",
    ),
    Rule(
        "directory-iteration",
        "std::filesystem::directory_iterator, recursive_directory_iterator, opendir, readdir, FindFirstFile*, "
        "FindNextFile*",
        "directory entries come in file-system order, which differs between platforms; sort before use and "
        "suppress with the reason",
        _rx(r"\b(?:recursive_)?directory_iterator\b|\b(?P<fn>opendir|readdir|readdir_r|scandir|_findfirst\w*"
            r"|_findnext\w*|FindFirstFile\w*|FindNextFile\w*)\s*\("),
        filter=_free_call,
        review=True,
    ),
    Rule(
        "number-text",
        "strto*/wcsto*, ato*, std::sto*, std::to_string, std::to_wstring, the printf and scanf families, ecvt/fcvt/gcvt",
        "text-to-number or number-to-text through the C library or locale: formats and accepted input differ "
        "by platform and locale; use std::to_chars / std::from_chars on integers or the project's formatting "
        "(P0-01 4)",
        _rx(r"\b(?P<fn>" + NUM_TEXT_C_FUNCS + r")\s*\(|" + STD + r"(?:to_string|to_wstring|stoi|stol|stoll|stoul"
            r"|stoull|stof|stod|stold)\b"),
        filter=_free_call,
    ),
    Rule(
        "stream-format",
        "std::cout/cerr/clog/cin (and w forms), string streams, iomanip manipulators; "
        "#include <iostream>, <sstream>, <iomanip>, <strstream>, <spanstream>, <syncstream>",
        "formatted stream I/O: number formatting follows the stream's locale; write text with the project's "
        "formatting (P0-01 4)",
        _rx(STD + r"w?(?:cout|cerr|clog|cin)\b|" + STD + r"(?:basic_)?w?(?:i|o)?(?:string|str|span)stream\b|"
            + STD + r"(?:basic_)?w?osyncstream\b|"
            + STD + r"(?:setprecision|setw|setfill|setbase|fixed|scientific|hexfloat|defaultfloat|showpoint"
            r"|put_time|get_time|put_money|get_money)\b"),
        includes=("iostream", "sstream", "iomanip", "strstream", "spanstream", "syncstream"),
    ),
    Rule(
        "sqlite-value-type",
        "sqlite3_bind/column/result/value_double, sqlite3_bind/column/result/value_int (not the _int64 forms)",
        "SQLite call that stores or reads a float or a 32-bit integer: worlds hold INTEGER (int64), TEXT and "
        "BLOB only (P0-02 5.3)",
        _rx(r"\bsqlite3_(?:bind|column|result|value)_(?:double|int)\b"),
    ),
    Rule(
        "fp-pragma",
        "#pragma float_control / fp_contract / fenv_access / STDC FP_CONTRACT|FENV_ACCESS|FENV_ROUND|"
        "CX_LIMITED_RANGE / clang fp / GCC optimize / optimize, _Pragma(, __pragma(, optimize attributes",
        "pragma or attribute that changes floating-point or optimisation semantics per file (P0-01 4: "
        "compilation)",
        _rx(r"^[ \t]*#[ \t]*pragma[ \t]+(?:float_control|fp_contract|fenv_access|STDC[ \t]+(?:FP_CONTRACT|FENV_ACCESS"
            r"|FENV_ROUND|FENV_DEC_ROUND|CX_LIMITED_RANGE)|clang[ \t]+fp|GCC[ \t]+optimize|optimize)\b"
            r"|\b_Pragma\s*\(|\b__pragma\s*\(|__attribute__\s*\(\s*\(\s*optimize\b|\[\[\s*gnu\s*::\s*optimize\b", re.M),
    ),
    Rule(
        "unchecked-write",
        "a call of fwrite, fputs, fputc, putc, puts, putchar, fflush, fclose, fprintf, printf, ... whose "
        "result is discarded (including a cast to void)",
        "result of a C stream write discarded: a failed write must change the exit status (P0-04 4.6)",
        _rx(r"\b(?P<fn>fwrite|fputs|fputc|putc|puts|putchar|fputws|fputwc|putwc|fflush|fclose|fprintf|printf"
            r"|vfprintf|vprintf|fwprintf|wprintf)\s*\("),
        filter=_discarded_call,
    ),
    Rule(
        "two-draws",
        "two random draws (.next(, .next_u32(, .uniform_*(, .chance(, .chance_ratio(, .weighted_index(, "
        ".shuffle(, splitmix64_next() in one full expression",
        "two random draws in one full expression: the order of evaluation is unspecified and differs between "
        "compilers; make each draw its own statement (P0-01 4, P1-00 D9)",
    ),
    # History (P3-00 4.5 rule H4, 4.6): every random decision is a keyed draw.
    Rule(
        "history-sequential-rng",
        "the sequential generator in history code: Rng, splitmix64_next(, #include \"worldgen/core/rng.hpp\"",
        "sequential generator in history code: a draw taken from a shared stream shifts every later decision "
        "when one is added or removed; use a keyed draw, history::draw(stream_key, w0, a, b, k) (P3-00 H4, 4.6)",
        _rx(r"\bRng\b|\bsplitmix64_next\s*\("),
        includes=("worldgen/core/rng.hpp",),
        areas=frozenset({LIB}),
        only_dirs=HISTORY_DIRS,
        where="library: " + ", ".join(HISTORY_DIRS) + " (prefixes)",
    ),
    # History state (P3-04 6.1, 6.2). Added by the P3-04 engine audit.
    Rule(
        "history-live-iteration",
        "a range-for over State::civs(), settlements() or relations() (or civs_, settlements_, relations_) "
        "whose body calls found_civilization(, found_settlement(, open_relation( or close_relation(",
        "the loop changes the container it iterates: founding appends to the civilization or settlement vector "
        "(a reallocation invalidates the loop's iterators, and when it happens depends on the standard "
        "library's growth factor), close_relation erases from the relation map; iterate over ids up to a count "
        "taken before the loop, or collect the changes and apply them after it (P3-04 6.1, 6.2)",
        _rx(r"\bfor\s*\("),
        filter=_live_iteration,
        areas=frozenset({LIB}),
        only_dirs=HISTORY_DIRS,
        where="library: " + ", ".join(HISTORY_DIRS) + " (prefixes)",
    ),
    # Added by the P3-05 peoples audit: the other form of P3-04 audit A1.
    Rule(
        "history-held-reference",
        "a local Civilization& / Settlement& / Civilization* / Settlement*, a std::span<const Civilization> or "
        "std::span<const Settlement>, or an auto& / auto* / auto whose initializer is a call of .civ(, .settlement(, "
        ".civs() or .settlements() (nothing after it), that is used after a later found_civilization( or found_settlement( call in the same block",
        "a reference, pointer or span into State's civilization or settlement vector is used after a founding: "
        "the founding may reallocate the vector, and whether it does depends on the standard library's growth "
        "factor, so the use reads freed memory on one platform and not on another; re-fetch by id after the "
        "founding, or copy the fields you need before it (P3-04 6.9, P3-05 13.1 item 1)",
        _rx(r"\b(?:Civilization|Settlement)\s*[&*]\s*(?P<ref>\w+)\s*="
            r"|\bstd\s*::\s*span\s*<\s*const\s+(?:Civilization|Settlement)\s*>\s*(?P<span>\w+)\s*[={(]"
            r"|\bauto\b\s*[&*]?\s*(?P<auto>\w+)\s*=\s*(?:\w+\s*(?:\(\s*\))?\s*(?:\.|->)\s*)+"
            r"(?:civ|settlement|civs|settlements)\s*\([^()]*\)\s*;"),
        filter=_held_reference,
        areas=frozenset({LIB}),
        only_dirs=HISTORY_DIRS,
        where="library: " + ", ".join(HISTORY_DIRS) + " (prefixes)",
    ),
    # Added by the P5-02 beasts and monsters audit: the same failure for the
    # vectors a subsystem owns (MonstersSubsystem::monsters_ and sites_).
    Rule(
        "history-member-held-reference",
        "a local Monster& / Site& / Monster* / Site*, or an auto& / auto* whose initializer is a call of "
        "monster( or site( (nothing after it), that is used after a later call that grows the vector it "
        "points into (monsters_ or sites_ .push_back(, .emplace_back(, .insert(, .resize(, ..., or a function "
        "of the same file that does so) in the same block",
        "a reference or pointer into a subsystem's monster or site vector is used after the vector grows: the "
        "append may reallocate it, and whether it does depends on the standard library's growth factor, so the "
        "use reads freed memory on one platform and not on another; re-fetch by id after the append, or copy "
        "the fields you need before it (P3-04 6.9, P5-02 6.7)",
        _rx(r"\b(?P<type>Monster|Site)\s*[&*]\s*(?P<ref>\w+)\s*="
            r"|\bauto\b\s*[&*]\s*(?P<auto>\w+)\s*=\s*(?:\w+\s*(?:\(\s*\))?\s*(?:\.|->)\s*)*"
            r"(?P<acc>monster|site)\s*\([^()]*\)\s*;"),
        filter=_member_held_reference,
        areas=frozenset({LIB}),
        only_dirs=HISTORY_DIRS,
        where="library: " + ", ".join(HISTORY_DIRS) + " (prefixes)",
    ),
    # SQL text in string literals.
    Rule(
        "sql-autoincrement",
        "AUTOINCREMENT in SQL text",
        "AUTOINCREMENT keeps a hidden sqlite_sequence table; use INTEGER PRIMARY KEY with explicit ids (P0-02)",
        kind=SQL, only_dirs=SQL_DIRS, areas=LIB_AREAS,
    ),
    Rule(
        "sql-maintenance",
        "ANALYZE, PRAGMA optimize, PRAGMA incremental_vacuum, VACUUM not followed by INTO, in SQL text",
        "statistics and maintenance statements make the file depend on history; never run them on the "
        "working database (P0-02, worldgen/src/db/runtime_settings.hpp)",
        kind=SQL, only_dirs=SQL_DIRS, areas=LIB_AREAS,
    ),
    Rule(
        "sql-setting",
        "journal_mode = WAL, auto_vacuum set to anything but NONE/0, in SQL text",
        "SQLite setting that differs from the pinned configuration (P0-02, runtime_settings.hpp)",
        kind=SQL, only_dirs=SQL_DIRS, areas=LIB_AREAS,
    ),
    Rule(
        "sql-real",
        "REAL, FLOAT, DOUBLE, NUMERIC, DECIMAL as a type in SQL text",
        "floating-point (or numeric-affinity) SQL type: worlds hold INTEGER, TEXT and BLOB only (P0-02 5.3)",
        kind=SQL, only_dirs=SQL_DIRS, areas=LIB_AREAS,
    ),
    Rule(
        "sql-unordered-select",
        "SELECT with no ORDER BY in the same SQL text",
        "SELECT without ORDER BY: row order is unspecified; add ORDER BY on a unique key, or suppress with the "
        "reason the order cannot matter (one row, an aggregate, ORDER BY appended later)",
        kind=SQL, only_dirs=SQL_DIRS, areas=LIB_AREAS, review=True,
    ),
    # Source encoding (added by the P4-02 audit): a byte of 0x80 or more
    # written directly in a literal is turned into execution bytes through
    # the compiler's source and execution character sets (/utf-8 on MSVC,
    # the UTF-8 default of GCC and Clang, overridable by -finput-charset or
    # an editor saving in another encoding). Escapes fix the bytes.
    Rule(
        "non-ascii-literal",
        "a character of U+0080 or above (or an invalid UTF-8 byte) inside a string or character literal, raw "
        "strings and prefixed literals included; comments are not checked",
        "non-ASCII character in a literal: its bytes depend on the source encoding; write the UTF-8 bytes as "
        "hex escapes, splitting the literal after the last one (\"\\xC2\\xB0\" \"C\", P4-02 8)",
        kind=LITERAL,
    ),
    # Build configuration.
    Rule(
        "cmake-fp-flag",
        "-ffast-math, -Ofast, -funsafe-math-optimizations, -ffinite-math-only, -fassociative-math, "
        "-freciprocal-math, -fno-signed-zeros, -fno-trapping-math, -ffp-contract=fast|on, -ffp-model=fast, "
        "/fp:fast, /fp:contract in the project's CMake files and presets",
        "floating-point flag that changes arithmetic (P0-01 4: compilation; cmake/UnmappedCompilerFlags.cmake)",
        kind=CMAKE, where="CMake files of the project (not extern/*/)",
    ),
    Rule(
        "toml-config",
        "every TOML_* macro that worldgen/src/data/toml_document.cpp guards with #error is pinned, with the same "
        "value, in extern/CMakeLists.txt",
        "toml++ configuration guard and pinned definitions disagree (P0-03)",
        kind=REPO, where="extern/CMakeLists.txt against worldgen/src/data/toml_document.cpp",
    ),
)

RULE_NAMES = {r.name for r in RULES}
RULES_BY_NAME = {r.name: r for r in RULES}
META_RULES = {"bad-suppression", "unused-suppression"}

# ---------------------------------------------------------------------------
# Lexer: blank out comments and literals, keep line structure
# ---------------------------------------------------------------------------

_STR_PREFIXES = {"", "u8", "u", "U", "L"}
_RAW_PREFIXES = {"R", "u8R", "uR", "UR", "LR"}


@dataclass
class Comment:
    line: int
    text: str
    standalone: bool  # no code before it on its line


@dataclass
class StringLit:
    start: int  # offset of the opening quote (raw: of the prefix's quote)
    end: int  # offset just past the closing quote
    body: str  # the text between the quotes, escapes not interpreted
    raw: bool


@dataclass
class Lexed:
    code: str  # same length/lines as input; comments and literal bodies are spaces
    comments: list = field(default_factory=list)
    strings: list = field(default_factory=list)  # StringLit, string literals only (not char)
    chars: list = field(default_factory=list)  # StringLit, character literals


def _is_ident(c: str) -> bool:
    return c.isalnum() or c == "_"


_RAW_OPEN = re.compile(r'"([^()\\ \t\n]{0,16})\(')
_INCLUDE_DIRECTIVE = re.compile(r"#[ \t]*(?:include|include_next|import)\b")


def lex(src: str) -> Lexed:
    """Returns the source with comments and the contents of string and
    character literals replaced by spaces (newlines kept), plus the comments
    and string literals. The text of #include header names is kept, so
    includes can be checked. Handles raw strings, encoding prefixes and digit
    separators (1'000)."""
    out = list(src)
    comments: list = []
    strings: list = []
    chars: list = []
    n = len(src)
    i = 0
    line = 1
    code_on_line = False
    in_include = False  # inside an #include directive line
    at_line_start = True

    def blank(a: int, b: int) -> None:
        for k in range(a, b):
            if out[k] != "\n":
                out[k] = " "

    while i < n:
        c = src[i]
        if c == "\n":
            line += 1
            code_on_line = False
            in_include = False
            at_line_start = True
            i += 1
            continue
        if c in " \t\r\f\v":
            i += 1
            continue
        if c == "\\" and i + 1 < n and src[i + 1] == "\n":  # line splice
            i += 1
            continue
        # Comments
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            j = i + 2
            while j < n and not (src[j] == "\n" and src[j - 1] != "\\"):
                j += 1
            comments.append(Comment(line, src[i + 2 : j], not code_on_line))
            blank(i, j)
            line += src.count("\n", i, j)  # spliced line comment
            i = j
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            comments.append(Comment(line, src[i + 2 : max(i + 2, j - 2)], not code_on_line))
            blank(i, j)
            line += src.count("\n", i, j)
            i = j
            continue
        # Preprocessor include: keep the header name verbatim.
        if at_line_start and c == "#":
            if _INCLUDE_DIRECTIVE.match(src, i):
                in_include = True
            at_line_start = False
            code_on_line = True
            i += 1
            continue
        at_line_start = False
        code_on_line = True
        if in_include and c in "<\"":
            close = ">" if c == "<" else "\""
            j = src.find(close, i + 1)
            if j < 0 or "\n" in src[i:j]:
                i += 1
                continue
            i = j + 1
            continue
        # Numbers (pp-number), so that digit separators are not char literals.
        if c.isdigit() or (c == "." and i + 1 < n and src[i + 1].isdigit()):
            if i > 0 and _is_ident(src[i - 1]):
                i += 1
                continue
            j = i + 1
            while j < n:
                d = src[j]
                if d in "eEpP" and j + 1 < n and src[j + 1] in "+-":
                    j += 2
                elif _is_ident(d) or d == ".":
                    j += 1
                elif d == "'" and j + 1 < n and _is_ident(src[j + 1]):
                    j += 2
                else:
                    break
            i = j
            continue
        # Identifiers (possibly a literal prefix)
        if c.isalpha() or c == "_":
            j = i + 1
            while j < n and _is_ident(src[j]):
                j += 1
            ident = src[i:j]
            if j < n and src[j] == '"' and ident in _RAW_PREFIXES:
                m = _RAW_OPEN.match(src, j)
                if m:
                    delim = m.group(1)
                    close = src.find(")" + delim + '"', m.end())
                    body_end = n if close < 0 else close
                    end = n if close < 0 else close + len(delim) + 2
                    strings.append(StringLit(j, end, src[m.end():body_end], True))
                    blank(m.end(), body_end)
                    line += src.count("\n", j, end)
                    i = end
                    continue
            if j < n and src[j] in "\"'" and ident in _STR_PREFIXES:
                i = j  # fall through to the literal on the next iteration
                continue
            i = j
            continue
        # String and character literals
        if c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                if src[j] == "\\" and j + 1 < n:
                    j += 2
                    continue
                if src[j] == "\n":  # unterminated; stop at end of line
                    break
                j += 1
            end = j + 1 if j < n and src[j] == c else j
            if c == '"':
                strings.append(StringLit(i, end, src[i + 1:min(j, n)], False))
            else:
                chars.append(StringLit(i, end, src[i + 1:min(j, n)], False))
            blank(i + 1, min(j, n))
            i = end
            continue
        i += 1
    return Lexed("".join(out), comments, strings, chars)


# ---------------------------------------------------------------------------
# Scanning
# ---------------------------------------------------------------------------


@dataclass(frozen=True, order=True)
class Finding:
    path: str
    line: int
    col: int
    rule: str
    message: str

    def format(self) -> str:
        return f"{self.path}:{self.line}:{self.col}: {self.rule}: {self.message}"


@dataclass
class Suppression:
    comment_line: int
    target_line: int
    rules: set
    used: set = field(default_factory=set)


def _parse_suppressions(lx: Lexed, code_lines: list, path: str) -> tuple:
    sups: list = []
    problems: list = []

    def next_code_line(after: int) -> int:
        k = after  # 1-based line 'after'; code_lines is 0-based
        while k < len(code_lines):
            if code_lines[k].strip():
                return k + 1
            k += 1
        return after + 1

    for cm in lx.comments:
        text = cm.text.strip()
        if text.endswith("*/"):
            text = text[:-2].rstrip()
        if not SUPPRESS_MARK_RE.search(text):
            continue
        m = SUPPRESS_RE.search(text)
        if not m:
            problems.append(Finding(path, cm.line, 1, "bad-suppression",
                                    "malformed suppression; expected 'determinism-lint: allow(rule, ...) -- reason'"))
            continue
        rules = {r.strip() for r in m.group(1).split(",") if r.strip()}
        reason = (m.group(2) or "").strip()
        unknown = sorted(r for r in rules if r not in RULE_NAMES)
        if not rules or unknown:
            problems.append(Finding(path, cm.line, 1, "bad-suppression",
                                    "unknown or missing rule name(s): " + (", ".join(unknown) or "(none)")))
            continue
        if len(reason) < MIN_REASON_LEN:
            problems.append(Finding(path, cm.line, 1, "bad-suppression",
                                    f"suppression needs a reason after '--' (at least {MIN_REASON_LEN} characters)"))
            continue
        target = next_code_line(cm.line) if cm.standalone else cm.line
        sups.append(Suppression(cm.line, target, rules))
    return sups, problems


_INCLUDE_LINE = re.compile(r"^[ \t]*#[ \t]*(?:include|include_next|import)[ \t]*[<\"]([^>\"\n]+)[>\"]", re.M)

# --- two random draws in one full expression --------------------------------

_DRAW = re.compile(r"(?:\.|->)\s*(?:next|next_u32|uniform_\w+|chance|chance_ratio|weighted_index|shuffle)\s*\("
                   r"|\bsplitmix64_next\s*\(")
_CONTROL_HEAD = re.compile(r"\b(?:if(?:\s+constexpr)?|while|for|switch)\s*\(")
_PP_LINE = re.compile(r"^[ \t]*#.*$", re.M)
_CLOSE_TO_OPEN = {")": "(", "]": "["}


def _full_expression_boundaries(code: str) -> list:
    """Offsets between which two evaluations are sequenced: ';', '{', '}',
    the parentheses of if/while/for/switch heads, preprocessor lines, the
    '?' and ':' of a conditional (the condition is sequenced before the one
    branch that is evaluated), and a comma that is not inside parentheses or
    brackets (between declarators, braced-init-list elements and comma
    operands, all evaluated left to right)."""
    marks = []
    stack: list = []
    n = len(code)
    for k, ch in enumerate(code):
        if ch in ";{}":
            marks.append(k)
            if ch == "{":
                stack.append("{")
            elif ch == "}":
                while stack and stack.pop() != "{":
                    pass
        elif ch in "([":
            stack.append(ch)
        elif ch in ")]":
            if stack and stack[-1] == _CLOSE_TO_OPEN[ch]:
                stack.pop()
        elif ch == "?":
            marks.append(k)
        elif ch == ":":
            if not ((k + 1 < n and code[k + 1] == ":") or (k > 0 and code[k - 1] == ":")):
                marks.append(k)
        elif ch == ",":
            if not stack or stack[-1] == "{":
                marks.append(k)
    for m in _CONTROL_HEAD.finditer(code):
        open_pos = m.end() - 1
        close = _matching_paren(code, open_pos)
        marks.append(open_pos)
        if close >= 0:
            marks.append(close)
    for m in _PP_LINE.finditer(code):
        marks.append(m.start())
        marks.append(m.end())
    return sorted(set(marks))


def _receiver(code: str, m: re.Match) -> str:
    """The object a draw is made on, as normalised text: 'rng' for
    rng.next(), 'streams.rng()' for streams.rng().next(), the argument for
    splitmix64_next(x). Draws on different receivers use different state."""
    if code[m.start()] not in ".-":
        close = _matching_paren(code, m.end() - 1)
        return "splitmix64_next:" + re.sub(r"\s+", "", code[m.end():close if close >= 0 else m.end()])
    k = m.start() - 1
    while k >= 0:
        while k >= 0 and code[k].isspace():
            k -= 1
        if k >= 0 and code[k] in ")]":
            depth = 0
            while k >= 0:
                if code[k] in ")]":
                    depth += 1
                elif code[k] in "([":
                    depth -= 1
                    if depth == 0:
                        break
                k -= 1
            k -= 1
            continue
        j = k
        while j >= 0 and _is_ident(code[j]):
            j -= 1
        if j == k:
            break
        k = j
        t = k
        while t >= 0 and code[t].isspace():
            t -= 1
        if t >= 0 and code[t] == ".":
            k = t - 1
        elif t >= 1 and code[t - 1:t + 1] in ("->", "::"):
            k = t - 2
        else:
            break
    return re.sub(r"\s+", "", code[k + 1:m.start()])


def _two_draws(code: str) -> list:
    """Positions of the first draw of every full expression that makes two or
    more draws on the same receiver."""
    draws = list(_DRAW.finditer(code))
    if len(draws) < 2:
        return []
    bounds = _full_expression_boundaries(code)
    groups: dict = {}
    for m in draws:
        key = (bisect.bisect_left(bounds, m.start()), _receiver(code, m))
        groups.setdefault(key, []).append(m.start())
    return sorted(ps[0] for ps in groups.values() if len(ps) >= 2)


# --- SQL text ------------------------------------------------------------------

# SQL is recognised by upper-case keywords (the project's convention), so
# prose in messages ("select a layer") is not read as SQL. Inside SQL text the
# checks below ignore case.
_SQL_START = re.compile(r"^\s*(?:SELECT|INSERT|REPLACE|UPDATE|DELETE|CREATE|ALTER|DROP|PRAGMA|VACUUM|ANALYZE|REINDEX"
                        r"|ATTACH|DETACH|BEGIN|COMMIT|END|ROLLBACK|SAVEPOINT|RELEASE|WITH|EXPLAIN|VALUES)\b")
# Upper-case clause keywords mark a fragment of SQL (" FROM ", " ORDER BY ").
_SQL_FRAGMENT = re.compile(r"\b(?:SELECT|FROM|WHERE|ORDER BY|GROUP BY|VALUES|INSERT INTO|JOIN|PRIMARY KEY|NOT NULL"
                           r"|AUTOINCREMENT|COLLATE|STRICT|WITHOUT ROWID|PRAGMA|ANALYZE|VACUUM)\b")
_SQL_QUOTED = re.compile(r"'(?:[^']|'')*'|\"(?:[^\"]|\"\")*\"|`[^`]*`|\[[^\]]*\]|--[^\n]*|/\*.*?\*/", re.S)
_SQL_CHECKS = (
    ("sql-autoincrement", re.compile(r"\bAUTOINCREMENT\b", re.I), "AUTOINCREMENT"),
    ("sql-maintenance", re.compile(r"\bANALYZE\b", re.I), "ANALYZE"),
    ("sql-maintenance", re.compile(r"\bPRAGMA\s+(?:\w+\s*\.\s*)?(?:optimize|incremental_vacuum)\b", re.I),
     "PRAGMA optimize / incremental_vacuum"),
    ("sql-maintenance", re.compile(r"\bVACUUM\b(?!\s+(?:\w+\s+)?INTO\b)", re.I), "VACUUM without INTO"),
    ("sql-setting", re.compile(r"\bjournal_mode\s*=\s*\(?\s*WAL\b", re.I), "journal_mode = WAL"),
    ("sql-setting", re.compile(r"\bauto_vacuum\s*=\s*\(?\s*(?!NONE\b|0\b)\w+", re.I), "auto_vacuum not NONE"),
    ("sql-real", re.compile(r"\b(?:REAL|FLOAT|DOUBLE|NUMERIC|DECIMAL)\b", re.I), "floating-point SQL type"),
)
_SQL_SELECT = re.compile(r"\bSELECT\b", re.I)
_SQL_ORDER_BY = re.compile(r"\bORDER\s+BY\b", re.I)


def _sql_groups(lx: Lexed) -> list:
    """Adjacent string literals ("a" "b") joined: list of (start offset, text)."""
    groups: list = []
    code = lx.code
    for s in lx.strings:
        text = s.body if s.raw else s.body.replace("\\\"", "\"").replace("\\n", "\n").replace("\\t", " ")
        if groups and not code[groups[-1][2]:s.start].strip(" \t\n\r\f\vLuU8R"):
            groups[-1][1] += text
            groups[-1][2] = s.end
        else:
            groups.append([s.start, text, s.end])
    return [(g[0], g[1]) for g in groups]


def _sql_findings(lx: Lexed) -> list:
    """(offset, rule, detail) for each SQL problem in the string literals."""
    out: list = []
    for start, text in _sql_groups(lx):
        if not (_SQL_START.search(text) or _SQL_FRAGMENT.search(text)):
            continue
        sql = _SQL_QUOTED.sub(" ", text)
        for rule, rx, what in _SQL_CHECKS:
            if rx.search(sql):
                out.append((start, rule, what))
        if _SQL_SELECT.search(sql) and not _SQL_ORDER_BY.search(sql):
            out.append((start, "sql-unordered-select", "SELECT without ORDER BY"))
    return out


def scan_text(src: str, relpath: str) -> list:
    src = src.replace("\r\n", "\n").replace("\r", "\n")
    lx = lex(src)
    code = lx.code
    code_lines = code.split("\n")
    line_starts = [0]
    for k, ch in enumerate(code):
        if ch == "\n":
            line_starts.append(k + 1)

    def pos_to_line_col(pos: int) -> tuple:
        lo = bisect.bisect_right(line_starts, pos) - 1
        return lo + 1, pos - line_starts[lo] + 1

    active = [r for r in RULES if r.kind in (CODE, SQL, LITERAL) and r.applies(relpath)]
    active_names = {r.name for r in active}
    raw: list = []
    includes = [(m.group(1).strip(), m.start(1) - 1) for m in _INCLUDE_LINE.finditer(code)]
    for rule in active:
        if rule.kind != CODE:
            continue
        if rule.pattern is not None:
            for m in rule.pattern.finditer(code):
                if rule.filter is not None and not rule.filter(code, m):
                    continue
                ln, col = pos_to_line_col(m.start())
                raw.append(Finding(relpath, ln, col, rule.name, rule.message))
        for header, pos in includes:
            if header in rule.includes:
                ln, col = pos_to_line_col(pos)
                raw.append(Finding(relpath, ln, col, rule.name, f"{rule.message}: <{header}>"))
    if "two-draws" in active_names:
        msg = RULES_BY_NAME["two-draws"].message
        for p in _two_draws(code):
            ln, col = pos_to_line_col(p)
            raw.append(Finding(relpath, ln, col, "two-draws", msg))
    if any(r.kind == SQL for r in active):
        for pos, rule, what in _sql_findings(lx):
            if rule in active_names:
                ln, col = pos_to_line_col(pos)
                raw.append(Finding(relpath, ln, col, rule, f"{RULES_BY_NAME[rule].message} [{what}]"))

    if "non-ascii-literal" in active_names:
        msg = RULES_BY_NAME["non-ascii-literal"].message
        for lit in sorted(lx.strings + lx.chars, key=lambda x: x.start):
            body_start = src.find("(", lit.start) + 1 if lit.raw else lit.start + 1
            for k, ch in enumerate(lit.body):
                if ord(ch) >= 0x80:
                    ln, col = pos_to_line_col(body_start + k)
                    raw.append(Finding(relpath, ln, col, "non-ascii-literal", msg))
                    break

    sups, problems = _parse_suppressions(lx, code_lines, relpath)
    kept: list = []
    for f in raw:
        hit = None
        for s in sups:
            if s.target_line == f.line and f.rule in s.rules:
                hit = s
                break
        if hit is None:
            kept.append(f)
        else:
            hit.used.add(f.rule)
    for s in sups:
        for r in sorted(s.rules - s.used):
            problems.append(Finding(relpath, s.comment_line, 1, "unused-suppression",
                                    f"suppression of '{r}' matches nothing on line {s.target_line}; remove it"))
    return sorted(set(kept + problems))


# --- CMake files -----------------------------------------------------------------

_CMAKE_FP_FLAG = re.compile(
    r"(?<![\w-])(?:-ffast-math|-Ofast|-funsafe-math-optimizations|-ffinite-math-only|-fassociative-math"
    r"|-freciprocal-math|-fno-signed-zeros|-fno-trapping-math|-fcx-limited-range|-ffp-contract=(?:fast|on)"
    r"|-ffp-model=(?:fast|aggressive)|-fp-model[= ]fast)(?![\w-])"
    r"|(?<!\w)[/-]fp:(?:fast|contract)\b")


def _strip_cmake_comments(text: str) -> str:
    """Blanks '#' comments (line and bracket) outside double-quoted strings,
    keeping line structure. JSON (presets) has no '#' outside strings."""
    out = list(text)
    i, n = 0, len(text)
    in_str = False
    while i < n:
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
            i += 1
            continue
        if c == '"':
            in_str = True
            i += 1
            continue
        if c == "#":
            m = re.match(r"#\[(=*)\[", text[i:])
            if m:
                end = text.find("]" + m.group(1) + "]", i)
                end = n if end < 0 else end + len(m.group(1)) + 2
            else:
                end = text.find("\n", i)
                end = n if end < 0 else end
            for k in range(i, end):
                if out[k] != "\n":
                    out[k] = " "
            i = end
            continue
        i += 1
    return "".join(out)


def scan_cmake_text(text: str, relpath: str) -> list:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    code = _strip_cmake_comments(text) if not relpath.endswith(".json") else text
    rule = RULES_BY_NAME["cmake-fp-flag"]
    out: list = []
    for idx, ln in enumerate(code.split("\n"), start=1):
        for m in _CMAKE_FP_FLAG.finditer(ln):
            out.append(Finding(relpath, idx, m.start() + 1, rule.name, f"{rule.message}: {m.group(0)}"))
    return out


def is_cmake_file(p: Path) -> bool:
    return p.name in ("CMakeLists.txt", "CMakePresets.json", "CMakeUserPresets.json") or p.suffix == ".cmake"


# --- toml++ configuration -----------------------------------------------------

_TOML_GUARD_PAIR = re.compile(r"!\s*defined\s*\(\s*(TOML_\w+)\s*\)\s*\|\|\s*\1\s*!=\s*(\w+)")
_PP_IF = re.compile(r"^[ \t]*#[ \t]*if\b((?:[^\n]*\\\n)*[^\n]*)\n((?:[ \t]*\n)*)[ \t]*#[ \t]*error\b", re.M)


def toml_guard_pins(cpp_text: str) -> dict:
    """The TOML_* macros and values that an '#if ... #error' guard requires."""
    code = lex(cpp_text.replace("\r\n", "\n")).code
    pins: dict = {}
    for m in _PP_IF.finditer(code):
        cond = m.group(1).replace("\\\n", " ")
        for name, value in _TOML_GUARD_PAIR.findall(cond):
            pins[name] = value
    return pins


def check_toml_config(cpp_text: str, cpp_rel: str, cmake_text: str, cmake_rel: str,
                      target: str = TOML_CMAKE_TARGET) -> list:
    rule = "toml-config"
    msg = RULES_BY_NAME[rule].message
    pins = toml_guard_pins(cpp_text)
    if not pins:
        return [Finding(cpp_rel, 1, 1, rule, f"{msg}: no '#if !defined(TOML_X) || TOML_X != v ... #error' guard found")]
    cmake = _strip_cmake_comments(cmake_text.replace("\r\n", "\n"))
    m = re.search(r"\btarget_compile_definitions\s*\(\s*" + re.escape(target) + r"\s+INTERFACE\b([^)]*)\)", cmake)
    if not m:
        return [Finding(cmake_rel, 1, 1, rule, f"{msg}: no target_compile_definitions({target} INTERFACE ...)")]
    head_line = cmake.count("\n", 0, m.start()) + 1
    defined: dict = {}
    for d in re.finditer(r"(?<![\w-])(TOML_\w+)(?:=(\S+))?", m.group(1)):
        defined[d.group(1)] = (d.group(2) if d.group(2) is not None else "1",
                               cmake.count("\n", 0, m.start(1) + d.start()) + 1)
    out: list = []
    for name in sorted(pins):
        want = pins[name]
        if name not in defined:
            out.append(Finding(cmake_rel, head_line, 1, rule,
                               f"{msg}: {name} is guarded in {cpp_rel} ({name}={want}) but not pinned here"))
        elif defined[name][0] != want:
            out.append(Finding(cmake_rel, defined[name][1], 1, rule,
                               f"{msg}: {name}={defined[name][0]} here, but {cpp_rel} requires {name}={want}"))
    return out


# --- files --------------------------------------------------------------------


def _rel(path: Path, repo: Path) -> str:
    try:
        return path.resolve().relative_to(repo.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def _excluded(rel: str, dirs: tuple) -> bool:
    return any(rel == d or rel.startswith(d + "/") for d in dirs)


def collect(paths: list, repo: Path, apply_excludes: bool) -> tuple:
    """(C++ files, CMake files) under the given paths."""
    cxx: list = []
    cmake: list = []
    for p in paths:
        if p.is_file():
            (cmake if is_cmake_file(p) else cxx).append(p)
        elif p.is_dir():
            for f in p.rglob("*"):
                if f.is_file() and f.suffix in CXX_SUFFIXES:
                    cxx.append(f)
                elif f.is_file() and is_cmake_file(f) and not apply_excludes:
                    cmake.append(f)
    if apply_excludes:
        cxx = [f for f in cxx if not _excluded(_rel(f, repo), EXCLUDED_DIRS)]
    key = lambda f: _rel(f, repo)  # noqa: E731
    return sorted(set(cxx), key=key), sorted(set(cmake), key=key)


def default_cmake_files(repo: Path) -> list:
    files = [repo / f for f in CMAKE_FILES if (repo / f).is_file()]
    for d in CMAKE_DIRS:
        root = repo / d
        if root.is_dir():
            files.extend(f for f in root.rglob("*") if f.is_file() and is_cmake_file(f)
                         and not _excluded(_rel(f, repo), CMAKE_EXCLUDED_DIRS))
    return sorted(set(files), key=lambda f: _rel(f, repo))


def _read(f: Path) -> str:
    return f.read_bytes().decode("utf-8", errors="replace")


def scan_files(cxx: list, cmake: list, repo: Path) -> list:
    findings: list = []
    for f in cxx:
        findings.extend(scan_text(_read(f), _rel(f, repo)))
    for f in cmake:
        findings.extend(scan_cmake_text(_read(f), _rel(f, repo)))
    return findings


def repo_checks(repo: Path) -> list:
    cpp, cm = repo / TOML_GUARD_FILE, repo / TOML_CMAKE_FILE
    rule = "toml-config"
    for p, rel in ((cpp, TOML_GUARD_FILE), (cm, TOML_CMAKE_FILE)):
        if not p.is_file():
            return [Finding(rel, 1, 1, rule, f"{RULES_BY_NAME[rule].message}: file missing")]
    return check_toml_config(_read(cpp), TOML_GUARD_FILE, _read(cm), TOML_CMAKE_FILE)


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

EXPECT_RE = re.compile(r"expect\s*:\s*([\w\-, ]+)")
SELFTEST_PATH_RE = re.compile(r"lint-selftest-path\s*:\s*(\S+)")


def _expected(text: str, marker: str) -> set:
    expected: set = set()
    for idx, ln in enumerate(text.split("\n"), start=1):
        m = EXPECT_RE.search(ln)
        if m and marker in ln[: m.start()]:
            for r in m.group(1).split(","):
                if r.strip():
                    expected.add((idx, r.strip().split()[0]))
    return expected


def self_test(here: Path, repo: Path) -> int:
    """Each file in selftest/ marks the findings it must produce with a
    trailing comment 'expect: rule[, rule]' on the offending line ('//' in
    C++ snippets, '#' in .cmake snippets). A file with no markers must
    produce no findings. The lint must report exactly the marked (line, rule)
    pairs, no more and no fewer. A C++ snippet whose first lines contain
    'lint-selftest-path: <path>' is scanned as if it were that repository
    path (allow-lists, library or CLI area, SQL directories). The pair in
    selftest/toml_config/ is checked by the toml++ configuration check, with
    markers in its CMakeLists.txt."""
    tdir = here / "selftest"
    files = sorted(f for f in tdir.iterdir() if f.is_file() and (f.suffix in CXX_SUFFIXES or f.suffix == ".cmake"))
    if not files:
        print("self-test: no snippet files found", file=sys.stderr)
        return 1
    failures = 0
    checked = 0
    rules_seen: set = set()

    def compare(name: str, expected: set, got: set) -> int:
        bad = 0
        for ln, r in sorted(expected - got):
            print(f"self-test FAIL {name}:{ln}: expected {r}, not reported")
            bad += 1
        for ln, r in sorted(got - expected):
            print(f"self-test FAIL {name}:{ln}: unexpected {r}")
            bad += 1
        return bad

    for f in files:
        text = f.read_text(encoding="utf-8")
        if f.suffix == ".cmake":
            got = {(x.line, x.rule) for x in scan_cmake_text(text, f.name)}
            expected = _expected(text, "#")
        else:
            head = "\n".join(text.split("\n")[:5])
            pm = SELFTEST_PATH_RE.search(head)
            rel = pm.group(1) if pm else _rel(f, repo)
            got = {(x.line, x.rule) for x in scan_text(text, rel)}
            expected = _expected(text, "//")
        rules_seen |= {r for _, r in got}
        checked += 1
        failures += compare(f.name, expected, got)

    pair = tdir / "toml_config"
    cpp, cm = pair / "toml_document.cpp", pair / "CMakeLists.txt"
    if cpp.is_file() and cm.is_file():
        cm_text = cm.read_text(encoding="utf-8")
        got = {(x.line, x.rule) for x in check_toml_config(cpp.read_text(encoding="utf-8"), "toml_document.cpp",
                                                           cm_text, "CMakeLists.txt")}
        rules_seen |= {r for _, r in got}
        checked += 1
        failures += compare("toml_config/CMakeLists.txt", _expected(cm_text, "#"), got)
        # The guard is required: without one, the check fails.
        if not check_toml_config("int x;\n", "none.cpp", cm_text, "CMakeLists.txt"):
            print("self-test FAIL toml_config: a source with no guard was accepted")
            failures += 1
    else:
        print("self-test FAIL: selftest/toml_config/ is missing")
        failures += 1

    untested = sorted((RULE_NAMES | META_RULES) - rules_seen)
    for r in untested:
        print(f"self-test FAIL: rule {r} is not exercised by any bad snippet")
        failures += 1
    status = "FAILED" if failures else "passed"
    print(f"self-test {status}: {checked} snippet files, {failures} failure(s)")
    return 1 if failures else 0


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main(argv: list) -> int:
    here = Path(__file__).resolve().parent
    repo = here.parent.parent
    ap = argparse.ArgumentParser(description="Banned-construct lint for worldgen and worldgen-cli (P0-01 4, P1-00 8).")
    ap.add_argument("paths", nargs="*", type=Path,
                    help="files or directories to scan, with no exclusions (default: worldgen/ and "
                         "tools/worldgen-cli/ without their tests/, the project's CMake files, the toml++ check)")
    ap.add_argument("--repo", type=Path, default=repo, help="repository root (default: inferred from script location)")
    ap.add_argument("--self-test", action="store_true", help="check the lint against tools/lint/selftest/")
    ap.add_argument("--list-rules", action="store_true", help="print the rules and exit")
    args = ap.parse_args(argv)

    if args.list_rules:
        for r in RULES:
            tag = " [review: suppress each use with a reason]" if r.review else ""
            print(f"{r.name:22} {r.summary}\n{'':22} applies to: {r.where}{tag}")
        for r in sorted(META_RULES):
            print(f"{r:22} (suppression hygiene)")
        return 0
    if args.self_test:
        return self_test(here, args.repo)

    if args.paths:
        for t in args.paths:
            if not t.exists():
                print(f"determinism-lint: no such path: {t}", file=sys.stderr)
                return 2
        cxx, cmake = collect(args.paths, args.repo, apply_excludes=False)
        extra: list = []
    else:
        targets = [args.repo / r for r in DEFAULT_ROOTS]
        for t in targets:
            if not t.exists():
                print(f"determinism-lint: no such path: {t}", file=sys.stderr)
                return 2
        cxx, _ = collect(targets, args.repo, apply_excludes=True)
        cmake = default_cmake_files(args.repo)
        extra = repo_checks(args.repo)
    findings = sorted(set(scan_files(cxx, cmake, args.repo) + extra))
    for f in findings:
        print(f.format())
    print(f"determinism-lint: {len(cxx)} C++ file(s), {len(cmake)} CMake file(s) scanned, "
          f"{len(findings)} finding(s)", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
