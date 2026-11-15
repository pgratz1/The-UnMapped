#!/usr/bin/env python3
"""Banned-construct lint for the world generator's output path.

Scans C++ sources for constructs that spec P0-01 section 4
(docs/specs/p0-01-deterministic-core.md) bans in code whose results can reach
world.db, and that can be found mechanically with few false positives.
Comments, string literals and character literals are ignored.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B tools/lint/determinism_lint.py              # scan the output path
    python3 -B tools/lint/determinism_lint.py PATH ...     # scan given files/dirs
    python3 -B tools/lint/determinism_lint.py --self-test  # check the lint itself
    python3 -B tools/lint/determinism_lint.py --list-rules

Exit status: 0 clean, 1 findings (or self-test failure), 2 usage error.

Suppression (see README.md): a comment of the form

    // determinism-lint: allow(rule-a, rule-b) -- reason why this is safe

on the offending line, or alone on the line(s) directly above it. The reason
is mandatory. A suppression that names an unknown rule, has no reason, or
suppresses nothing is itself reported.

Python 3 standard library only.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.dont_write_bytecode = True

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Output-path roots, relative to the repository root, and what is excluded.
DEFAULT_ROOTS = ("worldgen",)
EXCLUDED_DIRS = ("worldgen/tests",)  # test code is not output path
CXX_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".ipp", ".inl", ".tpp"}

# Files allowed to use compiler-specific 128-bit types and intrinsics. Spec
# P0-01 5.3 permits a fast path only inside mul_u64_wide and divmod_u128_u64,
# which live in this file. Paths are repository-relative, '/' separated.
WIDE_TYPE_ALLOWLIST = {"worldgen/src/core/wide.cpp"}

MIN_REASON_LEN = 10  # characters; "safe" alone is not a reason

SUPPRESS_RE = re.compile(r"determinism-lint\s*:\s*allow\s*\(([^)]*)\)\s*(?:--\s*(.*))?$")
SUPPRESS_MARK_RE = re.compile(r"determinism-lint\s*:")


@dataclass(frozen=True)
class Rule:
    name: str
    summary: str
    pattern: re.Pattern
    message: str
    applies: object = None  # optional predicate(relpath) -> bool


def _rx(p: str) -> re.Pattern:
    return re.compile(p)


CMATH_FUNCS = (
    "sin|cos|tan|asin|acos|atan|atan2|sinh|cosh|tanh|asinh|acosh|atanh|"
    "exp|exp2|expm1|log|log2|log10|log1p|logb|ilogb|pow|sqrt|cbrt|hypot|"
    "fma|fmod|remainder|remquo|floor|ceil|round|lround|llround|trunc|"
    "nearbyint|rint|lrint|llrint|erf|erfc|tgamma|lgamma|frexp|ldexp|"
    "modf|scalbn|scalbln|copysign|nextafter|nexttoward|fdim|fmax|fmin|"
    "isnan|isinf|isfinite|signbit|fpclassify|lerp"  # not midpoint: <numeric>, exact on integers
)

RULES: tuple[Rule, ...] = (
    Rule(
        "float-type",
        "float, double, long double (and other floating types)",
        _rx(r"\b(?:float|double|__float128|__float80|_Float(?:16|32|64|128)x?|std\s*::\s*float(?:16|32|64|128)_t|std\s*::\s*bfloat16_t)\b"),
        "floating-point type in the output path (P0-01 4: integer and fixed-point only)",
    ),
    Rule(
        "banned-include",
        "#include of <cmath>, <math.h>, <random>, <chrono>, <ctime>, <time.h>, <unordered_map>, <unordered_set>, <locale>, <clocale>",
        _rx(r"^[ \t]*#[ \t]*(?:include|include_next|import)[ \t]*[<\"](cmath|math\.h|random|chrono|ctime|time\.h|unordered_map|unordered_set|locale|clocale)[>\"]"),
        "banned header in the output path (P0-01 4)",
    ),
    Rule(
        "std-math",
        "std:: transcendental and rounding functions from <cmath>",
        _rx(r"\bstd\s*::\s*(?:" + CMATH_FUNCS + r")\b(?=\s*[(<])"),
        "standard-library math function in the output path; use the project's own (P0-01 4, 9)",
    ),
    Rule(
        "std-random",
        "<random> engines and distributions, std::shuffle, std::sample, std::random_shuffle",
        _rx(
            r"\bstd\s*::\s*(?:ranges\s*::\s*)?(?:shuffle|sample|random_shuffle)\b"
            r"|\bstd\s*::\s*(?:\w+_distribution|mt19937(?:_64)?|random_device|minstd_rand0?|default_random_engine"
            r"|ranlux\w*|knuth_b|linear_congruential_engine|mersenne_twister_engine|subtract_with_carry_engine"
            r"|discard_block_engine|independent_bits_engine|shuffle_order_engine|seed_seq|generate_canonical)\b"
        ),
        "standard-library randomness in the output path; use worldgen::Rng (P0-01 4, 6)",
    ),
    Rule(
        "c-rand",
        "rand(), srand()",
        _rx(r"\bs?rand\s*\("),  # free calls only, see _is_free_call
        "C library rand/srand in the output path; use worldgen::Rng (P0-01 4)",
    ),
    Rule(
        "wall-clock",
        "time(), clock(), std::chrono, timespec_get, gettimeofday",
        _rx(r"\b(?:time|clock|timespec_get|gettimeofday|clock_gettime)\s*\(|\bstd\s*::\s*chrono\b"),
        "wall-clock or timing in the output path (P0-01 4)",
    ),
    Rule(
        "std-hash",
        "std::hash",
        _rx(r"\bstd\s*::\s*hash\b"),
        "std::hash differs between standard libraries; use the project's hashing (P0-01 4, 7)",
    ),
    Rule(
        "unordered-container",
        "std::unordered_map/set/multimap/multiset",
        _rx(r"\bstd\s*::\s*(?:pmr\s*::\s*)?unordered_(?:map|set|multimap|multiset)\b"),
        "unordered container in the output path; iteration order differs between libraries (P0-01 4)",
    ),
    Rule(
        "locale",
        "setlocale, std::locale, imbue",
        _rx(r"\bsetlocale\s*\(|\bstd\s*::\s*locale\b|(?:\.|->)\s*imbue\s*\("),
        "locale-dependent behaviour in the output path (P0-01 4)",
    ),
    Rule(
        "long-type",
        "long, unsigned long, long int (not long long, not long double)",
        _rx(r"\blong\b"),  # refined by _plain_long below
        "'long' is 32-bit on Windows and 64-bit on Linux; use std::int64_t / std::int32_t (P0-01 4)",
    ),
    Rule(
        "wchar-type",
        "wchar_t",
        _rx(r"\bwchar_t\b"),
        "wchar_t is 16-bit on Windows and 32-bit on Linux (P0-01 4)",
    ),
    Rule(
        "wide-int-type",
        "__int128 and 128-bit multiply/divide intrinsics outside the allow-listed file",
        _rx(r"\b(?:__int128(?:_t)?|__uint128_t|_umul128|_mul128|_udiv128|_div128|__umulh|__mulh)\b"),
        "compiler-specific 128-bit arithmetic outside the allow-listed fast path (P0-01 5.3)",
        applies=lambda rel: rel not in WIDE_TYPE_ALLOWLIST,
    ),
    Rule(
        "unstable-sort",
        "std::sort, partial_sort, partial_sort_copy, nth_element, partition (and std::ranges:: forms) — review and suppress with a reason",
        _rx(r"\bstd\s*::\s*(?:ranges\s*::\s*)?(?:sort|partial_sort|partial_sort_copy|nth_element|partition)\b"),
        "unstable algorithm: equal elements are ordered differently by different standard libraries; "
        "use std::stable_sort or a strict total-order comparator, then suppress with the reason (P0-01 4)",
    ),
)

RULE_NAMES = {r.name for r in RULES}
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
class Lexed:
    code: str  # same length/lines as input; comments and literal bodies are spaces
    comments: list[Comment] = field(default_factory=list)


def _is_ident(c: str) -> bool:
    return c.isalnum() or c == "_"


def lex(src: str) -> Lexed:
    """Returns the source with comments and the contents of string and
    character literals replaced by spaces (newlines kept), plus the comments.
    The text of #include header names is kept, so includes can be checked.
    Handles raw strings, encoding prefixes and digit separators (1'000)."""
    out = list(src)
    comments: list[Comment] = []
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
            m = re.match(r"#[ \t]*(?:include|include_next|import)\b", src[i:])
            if m:
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
                m = re.compile(r'"([^()\\ \t\n]{0,16})\(').match(src, j)
                if m:
                    delim = m.group(1)
                    close = src.find(")" + delim + '"', m.end())
                    body_end = n if close < 0 else close
                    end = n if close < 0 else close + len(delim) + 2
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
            blank(i + 1, min(j, n))
            i = j + 1 if j < n and src[j] == c else j
            continue
        i += 1
    return Lexed("".join(out), comments)


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
    rules: set[str]
    used: set[str] = field(default_factory=set)


def _parse_suppressions(lx: Lexed, code_lines: list[str], path: str) -> tuple[list[Suppression], list[Finding]]:
    sups: list[Suppression] = []
    problems: list[Finding] = []

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


_PREV_WORD = re.compile(r"(\w+)\s*$")
_NEXT_WORD = re.compile(r"\s*(\w+)")


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


_FREE_FUNCS = {"rand", "srand", "time", "clock", "timespec_get", "gettimeofday", "clock_gettime", "setlocale"}
_CALL_KEYWORDS = {"return", "throw", "case", "co_return", "co_yield", "else", "do", "and", "or", "not"}
_LAST_WORD = re.compile(r"(\w+)$")


def _is_free_call(code: str, m: re.Match) -> bool:
    """For the C library functions in _FREE_FUNCS: True if the match is a
    call of the global or std:: function, False if it is a member access
    (x.time(), p->clock()), another namespace's function (other::time()), or
    a declaration (int time(); Foo* clock();). Heuristic: a name preceded by
    an identifier other than a keyword like 'return', or by '*', '&' or '>',
    is taken to be a declaration."""
    word = re.match(r"\w+", code[m.start():])
    if not word or word.group(0) not in _FREE_FUNCS:
        return True  # std::chrono, std::locale, .imbue( are not filtered
    b = code[max(0, m.start() - 200):m.start()].rstrip()
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


def scan_text(src: str, relpath: str) -> list[Finding]:
    src = src.replace("\r\n", "\n").replace("\r", "\n")
    lx = lex(src)
    code = lx.code
    code_lines = code.split("\n")
    line_starts = [0]
    for k, ch in enumerate(code):
        if ch == "\n":
            line_starts.append(k + 1)

    def pos_to_line_col(pos: int) -> tuple[int, int]:
        lo, hi = 0, len(line_starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if line_starts[mid] <= pos:
                lo = mid
            else:
                hi = mid - 1
        return lo + 1, pos - line_starts[lo] + 1

    raw: list[Finding] = []
    for rule in RULES:
        if rule.applies is not None and not rule.applies(relpath):
            continue
        flags_text = code
        for m in rule.pattern.finditer(flags_text) if rule.name != "banned-include" else ():
            if rule.name == "long-type" and not _plain_long(flags_text, m):
                continue
            if rule.name in ("c-rand", "wall-clock", "locale") and not _is_free_call(flags_text, m):
                continue
            ln, col = pos_to_line_col(m.start())
            raw.append(Finding(relpath, ln, col, rule.name, rule.message))
        if rule.name == "banned-include":
            for idx, text in enumerate(code_lines):
                m = rule.pattern.search(text)
                if m:
                    raw.append(Finding(relpath, idx + 1, m.start() + 1, rule.name,
                                       f"{rule.message}: <{m.group(1)}>"))

    sups, problems = _parse_suppressions(lx, code_lines, relpath)
    kept: list[Finding] = []
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


def _rel(path: Path, repo: Path) -> str:
    try:
        return path.resolve().relative_to(repo.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def collect(paths: list[Path], repo: Path, apply_excludes: bool) -> list[Path]:
    files: list[Path] = []
    for p in paths:
        if p.is_file():
            files.append(p)
        elif p.is_dir():
            for f in sorted(p.rglob("*")):
                if f.is_file() and f.suffix in CXX_SUFFIXES:
                    files.append(f)
    if apply_excludes:
        files = [f for f in files
                 if not any(_rel(f, repo) == d or _rel(f, repo).startswith(d + "/") for d in EXCLUDED_DIRS)]
    return sorted(set(files), key=lambda f: _rel(f, repo))


def scan_files(files: list[Path], repo: Path) -> list[Finding]:
    findings: list[Finding] = []
    for f in files:
        text = f.read_bytes().decode("utf-8", errors="replace")
        findings.extend(scan_text(text, _rel(f, repo)))
    return findings


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

EXPECT_RE = re.compile(r"expect\s*:\s*([\w\-, ]+)")


def self_test(here: Path, repo: Path) -> int:
    """Each file in selftest/ marks the findings it must produce with a
    trailing comment 'expect: rule[, rule]' on the offending line. A file
    with no markers must produce no findings. The lint must report exactly
    the marked (line, rule) pairs, no more and no fewer."""
    tdir = here / "selftest"
    files = sorted(f for f in tdir.iterdir() if f.is_file() and f.suffix in CXX_SUFFIXES)
    if not files:
        print("self-test: no snippet files found", file=sys.stderr)
        return 1
    failures = 0
    checked = 0
    rules_seen: set[str] = set()
    for f in files:
        text = f.read_text(encoding="utf-8")
        expected: set[tuple[int, str]] = set()
        for idx, ln in enumerate(text.split("\n"), start=1):
            m = EXPECT_RE.search(ln)
            if m and "//" in ln[: m.start()]:
                for r in m.group(1).split(","):
                    if r.strip():
                        expected.add((idx, r.strip()))
        # Files named *allowlisted* are scanned under the allow-listed path.
        rel = "worldgen/src/core/wide.cpp" if "allowlisted" in f.name else _rel(f, repo)
        got = {(x.line, x.rule) for x in scan_text(text, rel)}
        rules_seen |= {r for _, r in got}
        missing = expected - got
        extra = got - expected
        checked += 1
        for ln, r in sorted(missing):
            print(f"self-test FAIL {f.name}:{ln}: expected {r}, not reported")
            failures += 1
        for ln, r in sorted(extra):
            print(f"self-test FAIL {f.name}:{ln}: unexpected {r}")
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


def main(argv: list[str]) -> int:
    here = Path(__file__).resolve().parent
    repo = here.parent.parent
    ap = argparse.ArgumentParser(description="Banned-construct lint for worldgen output-path code (P0-01 4).")
    ap.add_argument("paths", nargs="*", type=Path,
                    help="files or directories to scan (default: worldgen/, excluding worldgen/tests/)")
    ap.add_argument("--repo", type=Path, default=repo, help="repository root (default: inferred from script location)")
    ap.add_argument("--self-test", action="store_true", help="check the lint against tools/lint/selftest/")
    ap.add_argument("--list-rules", action="store_true", help="print the rules and exit")
    args = ap.parse_args(argv)

    if args.list_rules:
        for r in RULES:
            print(f"{r.name:20} {r.summary}")
        for r in sorted(META_RULES):
            print(f"{r:20} (suppression hygiene)")
        return 0
    if args.self_test:
        return self_test(here, args.repo)

    if args.paths:
        targets = args.paths
        excludes = False
    else:
        targets = [args.repo / r for r in DEFAULT_ROOTS]
        excludes = True
    for t in targets:
        if not t.exists():
            print(f"determinism-lint: no such path: {t}", file=sys.stderr)
            return 2
    files = collect(targets, args.repo, excludes)
    findings = scan_files(files, args.repo)
    for f in findings:
        print(f.format())
    print(f"determinism-lint: {len(files)} file(s) scanned, {len(findings)} finding(s)",
          file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
