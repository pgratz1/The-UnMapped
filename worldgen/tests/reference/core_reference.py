#!/usr/bin/env python3
"""Independent reference implementation of the P0-01 deterministic core.

Written from docs/specs/p0-01-deterministic-core.md only, as the second
reading of the spec that section 13.8 asks for. Python 3 standard library,
arbitrary-precision integers, no floating point anywhere.

Usage (run from anywhere; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/core_reference.py check
        Run every vector of spec section 13 and the checkable prose claims,
        plus self-tests (SHA-256 against hashlib, isqrt against math.isqrt).
        Exit status 1 if anything disagrees.

    python3 -B worldgen/tests/reference/core_reference.py generate [PATH]
        Write the golden vector file (default:
        worldgen/tests/golden/core_vectors.txt next to this directory).

    python3 -B worldgen/tests/reference/core_reference.py generate -
        Write the golden vector file to standard output.

Values are modelled as Python ints: an Fx is its raw int64, a uint64 is an
int in [0, 2^64). FatalError stands for the project fatal error (spec 4.1);
ParseError for the error value returned by parse_decimal and phrase checks.
"""

import sys
import os

# ---------------------------------------------------------------------------
# Basics
# ---------------------------------------------------------------------------

M64 = (1 << 64) - 1
M32 = (1 << 32) - 1
INT64_MIN = -(1 << 63)
INT64_MAX = (1 << 63) - 1

RAW_MAX = (1 << 63) - 1          # Fx MAX raw
RAW_MIN = -RAW_MAX               # Fx MIN raw (INT64_MIN is not valid)
ONE = 1 << 32
HALF = 1 << 31
PI_RAW = 13493037705


class FatalError(Exception):
    """The project fatal error (spec 4.1)."""


class ParseError(Exception):
    """An error value (parse_decimal, phrase checks): user-data errors."""


def fatal(msg):
    raise FatalError(msg)


def u64(x):
    return x & M64


def as_i64(x):
    """Two's-complement reinterpretation of a uint64 bit pattern."""
    x &= M64
    return x - (1 << 64) if x >> 63 else x


def rha(n, d):
    """Round half away from zero of the exact rational n/d (spec 5.2)."""
    if d == 0:
        fatal("division by zero")
    an, ad = abs(n), abs(d)
    q, r = divmod(an, ad)
    if 2 * r >= ad:
        q += 1
    return -q if (n < 0) != (d < 0) else q


# ---------------------------------------------------------------------------
# Plain int64 helpers (spec 4, 5.4)
# ---------------------------------------------------------------------------

def check_i64(v):
    if v < INT64_MIN or v > INT64_MAX:
        fatal("int64 overflow")
    return v


def checked_add(a, b):
    return check_i64(a + b)


def checked_sub(a, b):
    return check_i64(a - b)


def checked_mul(a, b):
    return check_i64(a * b)


def floor_div(a, b):
    if b == 0:
        fatal("division by zero")
    return check_i64(a // b)        # Python // is floor division


def floor_mod(a, b):
    if b == 0:
        fatal("division by zero")
    return a % b                    # Python % has the sign of the divisor


def mul_div(a, b, c):
    """RHA(a*b/c); fatal if c = 0 or result outside int64 or = INT64_MIN."""
    if c == 0:
        fatal("mul_div by zero")
    r = rha(a * b, c)
    if r < -INT64_MAX or r > INT64_MAX:
        fatal("mul_div overflow")
    return r


# ---------------------------------------------------------------------------
# U128 (spec 5.3)
# ---------------------------------------------------------------------------

def mul_u64_wide(a, b):
    """Reference method: four 32x32 partial products with carries."""
    a0, a1 = a & M32, a >> 32
    b0, b1 = b & M32, b >> 32
    p00 = a0 * b0
    p01 = a0 * b1
    p10 = a1 * b0
    p11 = a1 * b1
    mid = (p00 >> 32) + (p01 & M32) + (p10 & M32)
    lo = ((mid & M32) << 32) | (p00 & M32)
    hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32)
    assert hi <= M64 and (hi << 64 | lo) == a * b
    return hi, lo


def divmod_u128_u64(hi, lo, d):
    """Restoring shift-subtract long division over 128 bits."""
    if d == 0:
        fatal("divmod by zero")
    n = (hi << 64) | lo
    q = 0
    r = 0
    for i in range(127, -1, -1):
        r = (r << 1) | ((n >> i) & 1)
        q <<= 1
        if r >= d:
            r -= d
            q |= 1
    assert (q, r) == divmod(n, d)
    return q >> 64, q & M64, r


# ---------------------------------------------------------------------------
# Fx (spec 5)
# ---------------------------------------------------------------------------

def fx_check(raw):
    if raw < RAW_MIN or raw > RAW_MAX:
        fatal("Fx overflow")
    return raw


def fx_from_raw(r):
    if r == INT64_MIN:
        fatal("from_raw(INT64_MIN)")
    if r < INT64_MIN or r > INT64_MAX:
        fatal("not an int64")
    return r


def fx_from_int(n):
    if n < -(2**31 - 1) or n > 2**31 - 1:
        fatal("from_int out of range")
    return n << 32


def fx_from_ratio(num, den):
    if den == 0:
        fatal("from_ratio den 0")
    return fx_check(rha(num * ONE, den))


def fx_floor_int(a):
    return a >> 32


def fx_ceil_int(a):
    return -((-a) >> 32)


def fx_round_int(a):
    return rha(a, ONE)


def fx_trunc_int(a):
    s = -1 if a < 0 else 1
    return s * (abs(a) >> 32)


def fx_add(a, b):
    return fx_check(a + b)


def fx_sub(a, b):
    return fx_check(a - b)


def fx_neg(a):
    return -a


def fx_abs(a):
    return abs(a)


def fx_sat_add(a, b):
    return max(RAW_MIN, min(RAW_MAX, a + b))


def fx_sat_sub(a, b):
    return max(RAW_MIN, min(RAW_MAX, a - b))


def fx_mul(a, b):
    """Reference method of 5.4, via mul_u64_wide."""
    neg = (a < 0) != (b < 0)
    hi, lo = mul_u64_wide(abs(a), abs(b))
    p = (hi << 64) | lo
    q = p >> 32
    r = p & M32
    if r >= HALF:
        q += 1
    if q > RAW_MAX:
        fatal("mul overflow")
    res = -q if neg else q
    assert res == fx_check(rha(a * b, ONE))
    return res


def fx_div(a, b):
    """Reference method of 5.4, via divmod_u128_u64."""
    if b == 0:
        fatal("div by zero")
    neg = (a < 0) != (b < 0)
    n = abs(a) << 32
    qh, ql, r = divmod_u128_u64(n >> 64, n & M64, abs(b))
    q = (qh << 64) | ql
    if 2 * r >= abs(b):
        q += 1
    if q > RAW_MAX:
        fatal("div overflow")
    res = -q if neg else q
    assert res == fx_check(rha(a * ONE, b))
    return res


def fx_mul_int(a, n):
    return fx_check(a * n)


def fx_div_int(a, n):
    if n == 0:
        fatal("div_int by zero")
    return fx_check(rha(a, n))


def fx_min(a, b):
    return a if a <= b else b


def fx_max(a, b):
    return a if a >= b else b


def fx_clamp(x, lo, hi):
    if lo > hi:
        fatal("clamp lo > hi")
    return lo if x < lo else hi if x > hi else x


def fx_lerp(a, b, t):
    d = fx_sub(b, a)
    return fx_add(a, fx_mul(d, t))


def fade(t):
    if t < 0 or t > ONE:
        fatal("fade out of range")
    t2 = fx_mul(t, t)
    t3 = fx_mul(t2, t)
    a = fx_from_raw(6 * t - 15 * ONE)
    b = fx_add(fx_mul(t, a), fx_from_raw(10 * ONE))
    f = fx_mul(t3, b)
    return fx_clamp(f, 0, ONE)


# ---------------------------------------------------------------------------
# Decimal parse, canonical text, format (spec 5.6, 5.7)
# ---------------------------------------------------------------------------

DIGITS = "0123456789"


def parse_decimal(text):
    """Returns raw, or raises ParseError."""
    if not isinstance(text, str):
        text = text.decode("latin-1")
    s = text
    neg = False
    if s[:1] in ("-", "+"):
        neg = s[0] == "-"
        s = s[1:]
    if "." in s:
        ip, fp = s.split(".", 1)
        if fp == "":
            raise ParseError("trailing .")
    else:
        ip, fp = s, None
    if ip == "":
        raise ParseError("empty int part")
    if any(c not in DIGITS for c in ip):
        raise ParseError("bad int part")
    if ip[0] == "0" and len(ip) > 1:
        raise ParseError("leading zero")
    if len(ip) > 10:
        raise ParseError("too many int digits")
    f = 0
    if fp is not None:
        if any(c not in DIGITS for c in fp):
            raise ParseError("bad frac part")
        if len(fp) > 18:
            raise ParseError("too many frac digits")
        f = len(fp)
    n = int(ip + (fp or ""))
    mag = rha(n * ONE, 10 ** f)
    if mag > RAW_MAX:
        raise ParseError("out of range")
    return -mag if neg else mag


def canonical_decimal(text):
    """Canonical text of a valid decimal (5.6). Raises ParseError if invalid."""
    parse_decimal(text)
    s = text
    if s.startswith("+"):
        s = s[1:]
    if "." in s:
        ip, fp = s.split(".", 1)
        fp = fp.rstrip("0")
        s = ip + ("." + fp if fp else "")
    if s == "-0":
        s = "0"
    return s


def to_decimal(raw, d):
    if d < 0 or d > 18:
        fatal("to_decimal digits out of range")
    scaled = rha(raw * 10 ** d, ONE)
    out = "-" if scaled < 0 else ""
    m = abs(scaled)
    out += str(m // 10 ** d)
    if d > 0:
        out += "." + str(m % 10 ** d).rjust(d, "0")
    return out


# ---------------------------------------------------------------------------
# Square roots (spec 9.1)
# ---------------------------------------------------------------------------

def _isqrt_bits(n, bits):
    """Bitwise digit-by-digit (restoring) square root of n < 2^bits."""
    res = 0
    bit = 1 << (bits - 2)
    while bit > n:
        bit >>= 2
    while bit:
        if n >= res + bit:
            n -= res + bit
            res = (res >> 1) + bit
        else:
            res >>= 1
        bit >>= 2
    return res


def isqrt_u64(n):
    assert 0 <= n <= M64
    return _isqrt_bits(n, 64)


def isqrt_u128(hi, lo):
    n = (hi << 64) | lo
    return _isqrt_bits(n, 128)


def fx_sqrt(x):
    if x < 0:
        fatal("sqrt of negative")
    n = x << 32
    r = isqrt_u128(n >> 64, n & M64)
    if n - r * r > r:
        r += 1
    return r


def fx_hypot(a, b):
    n = a * a + b * b
    r = isqrt_u128(n >> 64, n & M64)
    if n - r * r > r:
        r += 1
    if r > RAW_MAX:
        fatal("hypot overflow")
    return r


# ---------------------------------------------------------------------------
# SplitMix64 and Rng (spec 6)
# ---------------------------------------------------------------------------

GOLDEN = 0x9E3779B97F4A7C15


def splitmix64_next(x):
    """Returns (new x, output)."""
    x = u64(x + GOLDEN)
    z = x
    z = u64((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9)
    z = u64((z ^ (z >> 27)) * 0x94D049BB133111EB)
    return x, z ^ (z >> 31)


def splitmix64_seq(x, k):
    out = []
    for _ in range(k):
        x, z = splitmix64_next(x)
        out.append(z)
    return out


def rotl(x, k):
    assert 0 < k < 64
    return u64((x << k) | (x >> (64 - k)))


class Rng:
    def __init__(self, key=None, state=None):
        if state is not None:
            if all(w == 0 for w in state):
                fatal("all-zero state")
            self.s = [u64(w) for w in state]
        else:
            self.s = splitmix64_seq(u64(key), 4)
        self.draws = 0

    @classmethod
    def from_state(cls, s0, s1, s2, s3):
        return cls(state=(s0, s1, s2, s3))

    def state(self):
        return tuple(self.s)

    def next(self):
        s0, s1, s2, s3 = self.s
        result = u64(rotl(u64(s1 * 5), 7) * 9)
        t = u64(s1 << 17)
        s2 ^= s0
        s3 ^= s1
        s1 ^= s2
        s0 ^= s3
        s2 ^= t
        s3 = rotl(s3, 45)
        self.s = [s0, s1, s2, s3]
        self.draws += 1
        return result

    def next_u32(self):
        return self.next() >> 32

    def uniform_u64(self, s):
        if s == 0:
            fatal("uniform_u64(0)")
        x = self.next()
        hi, lo = mul_u64_wide(x, s)
        if lo < s:
            t = u64(0 - s) % s
            while lo < t:
                x = self.next()
                hi, lo = mul_u64_wide(x, s)
        return hi

    def uniform_i64(self, lo, hi):
        if lo > hi:
            fatal("uniform_i64 lo > hi")
        span = u64(u64(hi) - u64(lo))
        if span == M64:
            return as_i64(self.next())
        return as_i64(u64(lo) + self.uniform_u64(span + 1))

    def uniform_fx01(self):
        return self.next() >> 32

    def uniform_fx(self, lo, hi):
        if lo >= hi:
            fatal("uniform_fx lo >= hi")
        return lo + self.uniform_u64(u64(hi - lo))

    def chance(self, p):
        if p < 0 or p > ONE:
            fatal("chance p out of range")
        return self.uniform_fx01() < p

    def chance_ratio(self, num, den):
        if den == 0 or num > den:
            fatal("chance_ratio bad args")
        return self.uniform_u64(den) < num

    def weighted_index(self, w):
        if len(w) == 0:
            fatal("weighted_index empty")
        total = 0
        for x in w:
            if x < 0:
                fatal("negative weight")
            total += x
            if total > M64:
                fatal("weight sum overflow")
        if total == 0:
            fatal("weighted_index total 0")
        r = self.uniform_u64(total)
        acc = 0
        for i, x in enumerate(w):
            acc += x
            if r < acc:
                return i
        raise AssertionError("unreachable")

    def shuffle(self, a):
        n = len(a)
        for i in range(n - 1, 0, -1):
            j = self.uniform_u64(i + 1)
            a[i], a[j] = a[j], a[i]
        return a


# ---------------------------------------------------------------------------
# Hashing and streams (spec 7)
# ---------------------------------------------------------------------------

def mix64(z):
    z = u64((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9)
    z = u64((z ^ (z >> 27)) * 0x94D049BB133111EB)
    return z ^ (z >> 31)


def absorb(h, w):
    return mix64(u64((h ^ w) + GOLDEN))


def hash_words(tag, words):
    h = tag
    for w in words:
        h = absorb(h, u64(w))
    return absorb(h, len(words))


def pack_bytes(b):
    """Byte-string encoding of 7.2: length word, then 8 bytes per word LE."""
    if isinstance(b, str):
        b = b.encode("ascii")
    words = [len(b)]
    for i in range(0, len(b), 8):
        chunk = b[i:i + 8]
        w = 0
        for j, c in enumerate(chunk):
            w |= c << (8 * j)
        words.append(w)
    return words


def tag_value(ascii8):
    b = ascii8.encode("ascii")
    assert len(b) == 8
    return sum(c << (8 * i) for i, c in enumerate(b))


TAG_STREAM = 0x4D5254533A4D4E55
TAG_SUB = 0x534255533A4D4E55
TAG_CELL2 = 0x324C45433A4D4E55
TAG_CELL3 = 0x334C45433A4D4E55
TAG_PHRASE = 0x535248503A4D4E55

TAGS = [
    ("TAG_STREAM", "UNM:STRM", TAG_STREAM),
    ("TAG_SUB", "UNM:SUBS", TAG_SUB),
    ("TAG_CELL2", "UNM:CEL2", TAG_CELL2),
    ("TAG_CELL3", "UNM:CEL3", TAG_CELL3),
    ("TAG_PHRASE", "UNM:PHRS", TAG_PHRASE),
]


def stream_name_valid(name):
    if not (1 <= len(name) <= 64):
        return False
    for part in name.split("."):
        if part == "" or not ("a" <= part[0] <= "z"):
            return False
        for c in part:
            if not ("a" <= c <= "z" or "0" <= c <= "9" or c == "_"):
                return False
    return True


def stream_key(seed, name):
    # 7.3: [seed, len(name), packed name words...]; pack_bytes supplies the
    # length word, so there is exactly one length word.
    return hash_words(TAG_STREAM, [seed] + pack_bytes(name))


def substream(key, index):
    return hash_words(TAG_SUB, [key, index])


def hash_at(key, x, y):
    return hash_words(TAG_CELL2, [key, u64(x), u64(y)])


def hash_at3(key, x, y, z):
    return hash_words(TAG_CELL3, [key, u64(x), u64(y), u64(z)])


def phrase_seed(b):
    """phrase_seed on exact bytes (no trimming or validation)."""
    return hash_words(TAG_PHRASE, pack_bytes(b))


PHRASE_WS = b" \t\r\n"


def phrase_seed_text(b):
    """7.3 'Text phrases': validate UTF-8, trim ASCII whitespace, 1..256."""
    if isinstance(b, str):
        b = b.encode("utf-8")
    try:
        b.decode("utf-8", "strict")
    except UnicodeDecodeError:
        raise ParseError("invalid UTF-8")
    t = b.strip(PHRASE_WS)
    if not (1 <= len(t) <= 256):
        raise ParseError("phrase length")
    return phrase_seed(t)


# ---------------------------------------------------------------------------
# Noise (spec 8)
# ---------------------------------------------------------------------------

def grad_dot(g, u, v):
    return [u + v, -u + v, u - v, -u - v, u, -u, v, -v][g]


def noise2(key, x, y):
    xi = x >> 32
    yi = y >> 32
    fx = x & 0xFFFFFFFF
    fy = y & 0xFFFFFFFF
    d = {}
    for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
        u = fx - dx * ONE
        v = fy - dy * ONE
        g = hash_at(key, xi + dx, yi + dy) >> 61
        d[(dx, dy)] = grad_dot(g, u, v)
    su = fade(fx_from_raw(fx))
    sv = fade(fx_from_raw(fy))
    nx0 = fx_lerp(d[(0, 0)], d[(1, 0)], su)
    nx1 = fx_lerp(d[(0, 1)], d[(1, 1)], su)
    n = fx_lerp(nx0, nx1, sv)
    return fx_clamp(n, -ONE, ONE)


def fbm2(key, x, y, octaves, base_freq, lacunarity, gain):
    if not (1 <= octaves <= 16):
        fatal("octaves")
    if base_freq <= 0 or lacunarity <= 0 or gain <= 0 or gain > ONE:
        fatal("fbm2 params")
    s = 0
    norm = 0
    amp = ONE
    f = base_freq
    for o in range(octaves):
        ko = substream(key, o)
        n = noise2(ko, fx_mul(x, f), fx_mul(y, f))
        s = fx_add(s, fx_mul(amp, n))
        norm = fx_add(norm, amp)
        amp = fx_mul(amp, gain)
        f = fx_mul(f, lacunarity)
    return fx_clamp(fx_div(s, norm), -ONE, ONE)


# ---------------------------------------------------------------------------
# SHA-256 (FIPS 180-4), written out, not hashlib
# ---------------------------------------------------------------------------

_K = [
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
]
_H0 = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]


def _rotr32(x, n):
    return ((x >> n) | (x << (32 - n))) & M32


class Sha256:
    def __init__(self):
        self.h = list(_H0)
        self.buf = b""
        self.length = 0

    def _block(self, blk):
        w = [int.from_bytes(blk[4 * i:4 * i + 4], "big") for i in range(16)]
        for i in range(16, 64):
            s0 = _rotr32(w[i - 15], 7) ^ _rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3)
            s1 = _rotr32(w[i - 2], 17) ^ _rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10)
            w.append((w[i - 16] + s0 + w[i - 7] + s1) & M32)
        a, b, c, d, e, f, g, h = self.h
        for i in range(64):
            S1 = _rotr32(e, 6) ^ _rotr32(e, 11) ^ _rotr32(e, 25)
            ch = (e & f) ^ (~e & g)
            t1 = (h + S1 + ch + _K[i] + w[i]) & M32
            S0 = _rotr32(a, 2) ^ _rotr32(a, 13) ^ _rotr32(a, 22)
            maj = (a & b) ^ (a & c) ^ (b & c)
            t2 = (S0 + maj) & M32
            h, g, f, e, d, c, b, a = g, f, e, (d + t1) & M32, c, b, a, (t1 + t2) & M32
        self.h = [(x + y) & M32 for x, y in zip(self.h, [a, b, c, d, e, f, g, h])]

    def update(self, data):
        self.length += len(data)
        self.buf += bytes(data)
        while len(self.buf) >= 64:
            self._block(self.buf[:64])
            self.buf = self.buf[64:]

    def final(self):
        bitlen = self.length * 8
        pad = b"\x80" + b"\x00" * ((55 - self.length) % 64) + bitlen.to_bytes(8, "big")
        self.update(pad)
        assert self.buf == b""
        return b"".join(x.to_bytes(4, "big") for x in self.h)


def sha256_hex(data):
    s = Sha256()
    s.update(data)
    return s.final().hex()


# ---------------------------------------------------------------------------
# Spec checks (section 13 and prose claims)
# ---------------------------------------------------------------------------

def outcome(fn, *args):
    """Run fn; return its value, 'FATAL' or 'ERROR'."""
    try:
        return fn(*args)
    except FatalError:
        return "FATAL"
    except ParseError:
        return "ERROR"


class Checker:
    def __init__(self):
        self.fails = 0
        self.count = 0

    def eq(self, label, got, want):
        self.count += 1
        ok = got == want
        if not ok:
            self.fails += 1
        print(("PASS " if ok else "FAIL ") + label
              + ("" if ok else f"  got={got!r} want={want!r}"))


def run_checks():
    c = Checker()
    F = lambda fn, *a: outcome(fn, *a)

    print("== 13.1 Fx")
    c.eq("from_int(1)", fx_from_int(1), 4294967296)
    c.eq("from_int(-3)", fx_from_int(-3), -12884901888)
    c.eq("from_int(2147483647) ok", F(fx_from_int, 2147483647) != "FATAL", True)
    c.eq("from_int(2147483648) fatal", F(fx_from_int, 2147483648), "FATAL")
    c.eq("from_int(-2147483648) fatal", F(fx_from_int, -2147483648), "FATAL")
    c.eq("from_raw(INT64_MIN) fatal", F(fx_from_raw, INT64_MIN), "FATAL")
    c.eq("mul(1.5,2.5)", fx_mul(6442450944, 10737418240), 16106127360)
    c.eq("mul(raw1,raw1)", fx_mul(1, 1), 0)
    c.eq("mul(raw1,HALF)", fx_mul(1, HALF), 1)
    c.eq("mul(raw-1,HALF)", fx_mul(-1, HALF), -1)
    c.eq("mul(raw3,HALF)", fx_mul(3, HALF), 2)
    c.eq("mul(raw-3,HALF)", fx_mul(-3, HALF), -2)
    c.eq("mul(65536,32767)", fx_mul(fx_from_int(65536), fx_from_int(32767)), fx_from_int(2147418112))
    c.eq("mul(65536,32768) fatal", F(fx_mul, fx_from_int(65536), fx_from_int(32768)), "FATAL")
    c.eq("div(ONE,3)", fx_div(ONE, fx_from_int(3)), 1431655765)
    c.eq("div(2,3)", fx_div(fx_from_int(2), fx_from_int(3)), 2863311531)
    c.eq("div(-2,3)", fx_div(fx_from_int(-2), fx_from_int(3)), -2863311531)
    c.eq("div(raw1,2)", fx_div(1, fx_from_int(2)), 1)
    c.eq("div(ONE,raw1) fatal", F(fx_div, ONE, 1), "FATAL")
    c.eq("div(x,0) fatal", F(fx_div, ONE, 0), "FATAL")
    c.eq("mul_div(7,3,2)", mul_div(7, 3, 2), 11)
    c.eq("mul_div(-7,3,2)", mul_div(-7, 3, 2), -11)
    c.eq("from_ratio(1,3)", fx_from_ratio(1, 3), 1431655765)
    r = -6442450944
    c.eq("floor/ceil/round/trunc -1.5",
         (fx_floor_int(r), fx_ceil_int(r), fx_round_int(r), fx_trunc_int(r)), (-2, -1, -2, -1))
    r = 6442450944
    c.eq("floor/ceil/round/trunc 1.5",
         (fx_floor_int(r), fx_ceil_int(r), fx_round_int(r), fx_trunc_int(r)), (1, 2, 2, 1))
    c.eq("fade(0),fade(HALF),fade(ONE)", (fade(0), fade(HALF), fade(ONE)), (0, 2147483648, 4294967296))
    c.eq("fade(raw -1) fatal", F(fade, -1), "FATAL")
    c.eq("fade(ONE+1) fatal", F(fade, ONE + 1), "FATAL")
    c.eq("lerp(2,4,HALF)", fx_lerp(fx_from_int(2), fx_from_int(4), HALF), fx_from_int(3))
    c.eq("MAX + raw1 fatal", F(fx_add, RAW_MAX, 1), "FATAL")
    c.eq("sat_add(MAX, raw1)", fx_sat_add(RAW_MAX, 1), RAW_MAX)
    c.eq("PI = round(pi*2^32) (Machin, exact ints)", _pi_raw(), PI_RAW)
    c.eq("PI hex", hex(PI_RAW), "0x3243f6a89")

    print("== 13.2 decimal")
    for t, w in [("0", 0), ("-0", 0), ("+0.000", 0), ("1", 4294967296), ("0.5", 2147483648),
                 ("-1.25", -5368709120), ("0.1", 429496730), ("-0.1", -429496730),
                 ("0.35", 1503238554), ("2147483647.9999999", 9223372036854775379),
                 ("2147483647.99999999999999999", "ERROR"),
                 ("0.000000000000000001", 0), ("0.0000000000000000001", "ERROR"),
                 ("12345678901", "ERROR")]:
        c.eq(f"parse {t!r}", F(parse_decimal, t), w)
    for t in [".5", "5.", "05", "1e3", "1_000", " 1", "1 ", "", "-", "0x10", "1.2.3"]:
        c.eq(f"parse {t!r} error", F(parse_decimal, t), "ERROR")
    for t, w in [("+0.500", "0.5"), ("-0", "0"), ("3.000", "3"), ("10", "10"), ("-2.50", "-2.5")]:
        c.eq(f"canonical {t!r}", canonical_decimal(t), w)
    c.eq("to_decimal(HALF,3)", to_decimal(HALF, 3), "0.500")
    c.eq("to_decimal(-5368709120,1)", to_decimal(-5368709120, 1), "-1.3")
    c.eq("to_decimal(raw1,9)", to_decimal(1, 9), "0.000000000")
    c.eq("to_decimal(raw-1,2)", to_decimal(-1, 2), "0.00")
    c.eq("to_decimal(from_int(-7),0)", to_decimal(fx_from_int(-7), 0), "-7")
    # Property: parse(to_decimal(x, 18)) == x
    rng = Rng(stream_key(0, "test.decimal"))
    vals = [RAW_MIN, RAW_MAX, 1, -1, 0]
    for _ in range(100000):
        vals.append(fx_from_raw_or_skip(as_i64(rng.next())))
    for _ in range(20000):
        vals.append(as_i64(rng.next()) >> (rng.next() % 63))
    bad = [v for v in vals if v != INT64_MIN and parse_decimal(to_decimal(v, 18)) != v]
    c.eq("round-trip parse(to_decimal(x,18)) over 120005 values", bad, [])
    # Exhaustive check of the argument near extremes and with canonical text
    bad = [v for v in vals[:5000] if v != INT64_MIN
           and parse_decimal(canonical_decimal(to_decimal(v, 18))) != v]
    c.eq("canonical text parses to same raw (5000 values)", bad, [])

    print("== 13.3 wide and sqrt")
    c.eq("mul_u64_wide(max,max)", mul_u64_wide(M64, M64), (0xFFFFFFFFFFFFFFFE, 1))
    c.eq("mul_u64_wide(2^32,2^32)", mul_u64_wide(1 << 32, 1 << 32), (1, 0))
    c.eq("divmod(2^64,3)", divmod_u128_u64(1, 0, 3), (0, 6148914691236517205, 1))
    c.eq("0x5555555555555555", 6148914691236517205, 0x5555555555555555)
    for n, w in [(0, 0), (1, 1), (2, 1), (3, 1), (4, 2), (15, 3), (16, 4), (M64, 4294967295)]:
        c.eq(f"isqrt_u64({n})", isqrt_u64(n), w)
    c.eq("sqrt(0)", fx_sqrt(0), 0)
    c.eq("sqrt(raw1)", fx_sqrt(1), 65536)
    c.eq("sqrt(1/4)", fx_sqrt(fx_from_ratio(1, 4)), 2147483648)
    c.eq("sqrt(4)", fx_sqrt(fx_from_int(4)), 8589934592)
    c.eq("sqrt(2)", fx_sqrt(fx_from_int(2)), 6074001000)
    c.eq("sqrt(3)", fx_sqrt(fx_from_int(3)), 7439101574)
    c.eq("sqrt(5)", fx_sqrt(fx_from_int(5)), 9603838835)
    c.eq("sqrt(raw-1) fatal", F(fx_sqrt, -1), "FATAL")
    c.eq("hypot(3,4)", fx_hypot(fx_from_int(3), fx_from_int(4)), fx_from_int(5))
    # Cross-check of the SHA-256 claim: H0[0], H0[1], H0[3] are frac(sqrt 2,3,5).
    for k, h in [(2, 0x6a09e667), (3, 0xbb67ae85), (5, 0x3c6ef372)]:
        c.eq(f"floor(frac(sqrt {k})*2^32) == SHA-256 initial word {h:#010x}",
             _isqrt_bits(k << 64, 128) & M32, h)
    rng = Rng(stream_key(0, "test.sqrt"))
    bad = 0
    for _ in range(100000):
        x = rng.next() >> (1 + rng.next() % 63)
        r = fx_sqrt(x)
        n = x << 32
        # The spec's form (2r-1)^2 <= 4N fails for x = 0 (r = 0 gives 1 <= 0),
        # so the lower bound is checked only for x > 0; see README.
        if (x > 0 and (2 * r - 1) ** 2 > 4 * n) or 4 * n >= (2 * r + 1) ** 2 or abs(r * r - n) > r:
            bad += 1
    c.eq("sqrt round-to-nearest property, 1e5 values (lower bound for x > 0)", bad, 0)
    c.eq("spec 13.3 property (2r-1)^2 <= 4N is false at x = 0", (2 * fx_sqrt(0) - 1) ** 2 <= 0, False)

    print("== 13.4 Rng")
    c.eq("splitmix64(0) x5", splitmix64_seq(0, 5),
         [0xE220A8397B1DCDAF, 0x6E789E6AA1B965F4, 0x06C45D188009454F,
          0xF88BB8A8724C81EC, 0x1B39896A51A8749B])
    c.eq("splitmix64(1234567) x5", splitmix64_seq(1234567, 5),
         [6457827717110365317, 3203168211198807973, 9817491932198370423,
          4593380528125082431, 16408922859458223821])
    g = Rng.from_state(1, 2, 3, 4)
    o1 = g.next()
    st = g.state()
    o2 = g.next()
    o3 = g.next()
    c.eq("from_state(1,2,3,4) next x3", (o1, o2, o3), (11520, 0, 1509978240))
    c.eq("state after first", st, (7, 0, 262146, 211106232532992))
    g = Rng(0)
    c.eq("Rng(0) state", g.state(), tuple(splitmix64_seq(0, 4)))
    c.eq("Rng(0) first", g.next(), 0x99EC5F36CB75F2B4)
    c.eq("from_state(0,0,0,0) fatal", F(Rng.from_state, 0, 0, 0, 0), "FATAL")
    g = Rng.from_state(1, 2, 3, 4)
    a = g.uniform_u64(10)
    d1 = g.draws
    b = g.uniform_u64(3)
    c.eq("uniform_u64(10), then (3); draws", (a, d1, b, g.draws - d1), (0, 1, 0, 2))
    c.eq("uniform_u64(2^63)", Rng.from_state(1, 2, 3, 4).uniform_u64(1 << 63), 5760)
    c.eq("shuffle [a,b,c]", Rng.from_state(1, 2, 3, 4).shuffle(["a", "b", "c"]), ["b", "c", "a"])
    c.eq("weighted_index([0,5,3])", Rng.from_state(1, 2, 3, 4).weighted_index([0, 5, 3]), 1)
    g = Rng(5)
    c.eq("uniform_u64(1) = 0, 1 draw", (g.uniform_u64(1), g.draws), (0, 1))
    c.eq("uniform_u64(0) fatal", F(Rng(5).uniform_u64, 0), "FATAL")
    g, h = Rng(9), Rng(9)
    c.eq("uniform_i64(full) = int64(next), 1 draw",
         (g.uniform_i64(INT64_MIN, INT64_MAX), g.draws), (as_i64(h.next()), 1))
    g = Rng(11)
    ok = all(g.chance(0) is False for _ in range(10000)) and g.draws == 10000
    ok = ok and all(g.chance(ONE) is True for _ in range(10000)) and g.draws == 20000
    c.eq("chance(0)/chance(ONE) 1e4 each, one draw each", ok, True)
    c.eq("weighted_index([]) fatal", F(Rng(1).weighted_index, []), "FATAL")
    c.eq("weighted_index([0,0]) fatal", F(Rng(1).weighted_index, [0, 0]), "FATAL")
    c.eq("weighted_index sum > 2^64-1 fatal", F(Rng(1).weighted_index, [M64, 1]), "FATAL")
    # property tests (quick versions)
    g = Rng(stream_key(0, "test.rng"))
    ok = True
    for s in [1, 2, 3, 7, 10, 1000, 2**32 + 1, 2**63 + 1, M64]:
        for _ in range(2000):
            if g.uniform_u64(s) >= s:
                ok = False
    c.eq("uniform_u64(s) < s", ok, True)
    ok = all(sorted(Rng(n).shuffle(list(range(n)))) == list(range(n)) for n in range(65))
    c.eq("shuffle is a permutation n=0..64", ok, True)
    g = Rng(stream_key(0, "test.rng"))
    cnt = [0] * 5
    N = 200000
    for _ in range(N):
        cnt[g.weighted_index([1, 2, 3, 0, 4])] += 1
    ok = cnt[3] == 0 and all(abs(cnt[i] * 10 - w * N) * 100 <= N * 10
                             for i, w in enumerate([1, 2, 3, 0, 4]))
    c.eq(f"weighted_index frequencies within 1% ({N} draws) {cnt}", ok, True)

    print("== 13.5 hashing")
    c.eq("mix64(0)", mix64(0), 0)
    c.eq("absorb(0,0)", absorb(0, 0), 0xE220A8397B1DCDAF)
    c.eq("absorb(0,GOLDEN)", absorb(0, GOLDEN), 0x6E789E6AA1B965F4)
    c.eq("pack test.noise", pack_bytes("test.noise"), [10, 0x696F6E2E74736574, 0x0000000000006573])
    c.eq("pack The Unmapped", pack_bytes("The Unmapped"), [12, 0x616D6E5520656854, 0x0000000064657070])
    c.eq("pack empty", pack_bytes(""), [0])
    for name, asc, val in TAGS:
        c.eq(f"{name} = ASCII {asc}", tag_value(asc), val)
    c.eq("phrase trims", phrase_seed_text(b"  The Unmapped\n"), phrase_seed_text(b"The Unmapped"))
    c.eq("phrase case matters", phrase_seed_text(b"the unmapped") != phrase_seed_text(b"The Unmapped"), True)
    c.eq("phrase invalid utf-8 error", F(phrase_seed_text, b"\xff"), "ERROR")
    c.eq("phrase whitespace-only error", F(phrase_seed_text, b" \t\r\n"), "ERROR")
    c.eq("stream_key(s,a) != stream_key(s,a.b)", stream_key(7, "a") != stream_key(7, "a.b"), True)
    for n in ["test.hash", "test.noise", "test.wide"]:
        c.eq(f"stream name {n} valid", stream_name_valid(n), True)

    print("== 13.6 noise")
    k0 = stream_key(0, "test.noise")
    keys = [substream(k0, i) for i in range(10)]
    ok = True
    for k in keys:
        for i in range(-50, 50, 7):
            for j in range(-50, 50, 9):
                if noise2(k, fx_from_int(i), fx_from_int(j)) != 0:
                    ok = False
    c.eq("noise2 = 0 at lattice points", ok, True)
    ok = True
    for k in keys:
        for i in range(-20, 20, 3):
            for j in range(-20, 20, 5):
                ds = []
                for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                    g = hash_at(k, i + dx, j + dy) >> 61
                    ds.append(grad_dot(g, HALF - dx * ONE, HALF - dy * ONE))
                want = sum(ds)
                assert want % 4 == 0
                if noise2(k, fx_from_int(i) + HALF, fx_from_int(j) + HALF) != want // 4:
                    ok = False
    c.eq("noise2 cell centre = (d00+d10+d01+d11)/4 exactly", ok, True)
    c.eq("fbm2 octaves=1 base=ONE == noise2(substream(k,0))",
         all(fbm2(k0, x, y, 1, ONE, 2 * ONE, HALF) == noise2(substream(k0, 0), x, y)
             for x, y in [(123456789, -987654321), (-5 * ONE - 7, 3 * ONE + 99)]), True)
    c.eq("fbm2 octaves 0 fatal", F(fbm2, k0, 0, 0, 0, ONE, 2 * ONE, HALF), "FATAL")
    c.eq("fbm2 octaves 17 fatal", F(fbm2, k0, 0, 0, 17, ONE, 2 * ONE, HALF), "FATAL")
    c.eq("fbm2 gain 0 fatal", F(fbm2, k0, 0, 0, 1, ONE, 2 * ONE, 0), "FATAL")
    c.eq("fbm2 base_freq 0 fatal", F(fbm2, k0, 0, 0, 1, 0, 2 * ONE, HALF), "FATAL")
    # Continuity spot check: 2^-12 steps, |delta| <= 2^-9
    worst = 0
    step = 1 << 20
    for k in keys[:3]:
        for i in range(400):
            x = (i * 0x9E3779B9) % (40 * ONE) - 20 * ONE
            y = (i * 0x7F4A7C15) % (40 * ONE) - 20 * ONE
            a0 = noise2(k, x, y)
            worst = max(worst, abs(noise2(k, x + step, y) - a0), abs(noise2(k, x, y + step) - a0))
    c.eq(f"continuity |dnoise| <= 2^-9 over 2400 steps (worst raw {worst})", worst <= (1 << 23), True)
    # Unclamped range over all 8^4 gradient configurations at a 1/16 grid
    worst = _noise_range_exhaustive(16)
    c.eq(f"unclamped noise over all gradient configs within [-ONE, ONE] (max |n| raw {worst})",
         worst <= ONE, True)

    print("== 13.7 SHA-256")
    for msg, w in [(b"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
                   (b"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
                   (b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"),
                   (b"a" * 1000000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")]:
        c.eq(f"sha256 len {len(msg)}", sha256_hex(msg), w)
    msg = bytes((i * 37 + 11) & 0xFF for i in range(130))
    one = sha256_hex(msg)
    ok = True
    for off in range(65):
        s = Sha256()
        s.update(msg[:off])
        s.update(msg[off:])
        ok = ok and s.final().hex() == one
    c.eq("incremental split at 0..64", ok, True)

    print("== self-tests")
    import hashlib
    import math
    data = bytes(range(256)) * 4
    ok = all(sha256_hex(data[:n]) == hashlib.sha256(data[:n]).hexdigest() for n in range(1025))
    c.eq("sha256 == hashlib for lengths 0..1024", ok, True)
    g = Rng(stream_key(0, "test.self"))
    ok = True
    for _ in range(20000):
        n = (g.next() << 64 | g.next()) >> (g.next() % 128)
        if isqrt_u128(n >> 64, n & M64) != math.isqrt(n):
            ok = False
        m = n & M64
        if isqrt_u64(m) != math.isqrt(m):
            ok = False
    c.eq("isqrt == math.isqrt", ok, True)

    print(f"\n{c.count} checks, {c.fails} failures")
    return c.fails == 0


def fx_from_raw_or_skip(r):
    return 1 if r == INT64_MIN else r


def _pi_raw():
    """round(pi * 2^32) via Machin's formula in integers with guard bits."""
    prec = 200
    scale = 1 << prec

    def arctan_inv(x):
        total = 0
        term = scale // x
        k = 0
        x2 = x * x
        while term:
            total += term // (2 * k + 1) if k % 2 == 0 else -(term // (2 * k + 1))
            term //= x2
            k += 1
        return total
    pi = 4 * (4 * arctan_inv(5) - arctan_inv(239))
    return rha(pi, 1 << (prec - 32))


def _noise_range_exhaustive(steps):
    """Max |unclamped noise| over all 8^4 corner-gradient choices and a grid
    of (steps+1)^2 points in one cell, using the spec's arithmetic."""
    pts = [i * ONE // steps for i in range(steps + 1)]
    fades = {p: fade(min(p, ONE)) for p in pts}
    worst = 0
    for p in pts:
        for q in pts:
            if p == ONE or q == ONE:
                continue
            su, sv = fades[p], fades[q]
            dots = {}
            for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                u, v = p - dx * ONE, q - dy * ONE
                dots[(dx, dy)] = sorted({grad_dot(g, u, v) for g in range(8)})
            # Noise is linear in each corner's dot with a non-negative weight,
            # so the extremes come from the extreme dots at each corner.
            for pick in (0, -1):
                d00 = dots[(0, 0)][pick]
                d10 = dots[(1, 0)][pick]
                d01 = dots[(0, 1)][pick]
                d11 = dots[(1, 1)][pick]
                n = fx_lerp(fx_lerp(d00, d10, su), fx_lerp(d01, d11, su), sv)
                worst = max(worst, abs(n))
    return worst


# ---------------------------------------------------------------------------
# Golden vector file
# ---------------------------------------------------------------------------

HEADER = """\
# The Unmapped: golden vectors for the P0-01 deterministic core.
# Generated by worldgen/tests/reference/core_reference.py (independent
# Python reference written from docs/specs/p0-01-deterministic-core.md).
# Do not edit by hand; regenerate with
#   python3 -B worldgen/tests/reference/core_reference.py generate
#
# Format
#   ASCII, LF line endings. Lines starting with '#' are comments; there are
#   no blank lines. Every other line is one vector:
#       <kind> <arg> <arg> ... = <result> <result> ...
#   Tokens are separated by exactly one space. The token '=' separates
#   arguments from results.
#   Token forms:
#     0x...      unsigned 64-bit integer in lowercase hex (uint64 values:
#                hashes, keys, words, RNG state, uniform_u64 bounds, U128
#                halves, isqrt_u64 arguments).
#     -?[0-9]+   decimal signed integer (Fx raw values, int64 values, small
#                counts such as draw counts, octaves, digit counts).
#     true/false booleans.
#     [a,b,c]    a list of integer tokens, comma separated, no spaces; [] is
#                empty.
#     "..."      a byte string. Bytes 0x21..0x7e other than '"' and '\\' appear
#                as themselves; every other byte (including space) is written
#                \\xHH (two lowercase hex digits), so a string token never
#                contains a space.
#     FATAL      the operation raises worldgen::FatalError (spec 4.1).
#     ERROR      the operation returns its error value (parse_decimal,
#                phrase checks).
#   S0 S1 S2 S3 below means four 0x tokens: an Rng made with
#   Rng::from_state(S0, S1, S2, S3). 'draws' is the number of next() calls
#   the sampling call consumed.
#
# Kinds (arguments = results). Fx values are raw int64 in decimal.
#   fx.from_int n = raw|FATAL          fx.from_raw r = raw|FATAL
#   fx.from_ratio num den = raw|FATAL
#   fx.floor_int a = n   fx.ceil_int a = n   fx.round_int a = n   fx.trunc_int a = n
#   fx.add a b = raw|FATAL   fx.sub a b = raw|FATAL
#   fx.sat_add a b = raw     fx.sat_sub a b = raw
#   fx.neg a = raw   fx.abs a = raw
#   fx.mul a b = raw|FATAL   fx.div a b = raw|FATAL
#   fx.mul_int a n = raw|FATAL   fx.div_int a n = raw|FATAL
#   fx.clamp x lo hi = raw|FATAL
#   fx.lerp a b t = raw|FATAL   fx.fade t = raw|FATAL
#   fx.sqrt a = raw|FATAL       fx.hypot a b = raw|FATAL
#   fx.parse "text" = raw|ERROR
#   fx.canonical "text" = "text"        (inputs are all valid decimals)
#   fx.to_decimal a d = "text"|FATAL
#   int.mul_div a b c = n|FATAL
#   int.checked_add a b = n|FATAL   int.checked_sub a b = n|FATAL
#   int.checked_mul a b = n|FATAL
#   int.floor_div a b = n|FATAL     int.floor_mod a b = n|FATAL
#   wide.mul a b = hi lo                       (all 0x)
#   wide.divmod hi lo d = qhi qlo rem|FATAL    (all 0x)
#   isqrt.u64 n = r                            (0x)
#   isqrt.u128 hi lo = r                       (0x)
#   splitmix64 x k = out1 ... outk             (x and outputs 0x, k decimal)
#   rng.seed key = S0 S1 S2 S3                 (state of Rng(key))
#   rng.next S0 S1 S2 S3 k = out1 ... outk
#   rng.state_after S0 S1 S2 S3 k = T0 T1 T2 T3   (state after k next() calls)
#   rng.stream seed "name" k = out1 ... outk   (first k next() of Rng(stream_key(seed, name)))
#   rng.next_u32 S0 S1 S2 S3 = r draws         (r decimal)
#   rng.uniform_u64 S0 S1 S2 S3 s = r draws | FATAL   (s, r 0x)
#   rng.uniform_i64 S0 S1 S2 S3 lo hi = r draws | FATAL   (decimal)
#   rng.uniform_fx01 S0 S1 S2 S3 = raw draws
#   rng.uniform_fx S0 S1 S2 S3 lo hi = raw draws | FATAL
#   rng.chance S0 S1 S2 S3 p = bool draws | FATAL
#   rng.chance_ratio S0 S1 S2 S3 num den = bool draws | FATAL   (num, den 0x)
#   rng.weighted_index S0 S1 S2 S3 [w,...] = index draws | FATAL   (weights 0x)
#   rng.shuffle S0 S1 S2 S3 n = [perm] draws   (shuffle of [0,1,...,n-1])
#   hash.tag NAME = value                      (NAME is TAG_STREAM etc.)
#   hash.mix64 z = h   hash.absorb h w = h'
#   hash.words tag [w,...] = h                 (hash_words)
#   hash.pack "bytes" = [len,w1,...]           (byte-string words, 0x)
#   hash.stream_key seed "name" = key
#   hash.substream key index = key
#   hash.at key x y = h                        (hash_at; x, y decimal int64)
#   hash.at3 key x y z = h                     (hash_at3; x, y, z decimal)
#   hash.phrase_seed "bytes" = seed            (exact bytes, no trimming)
#   hash.phrase_text "bytes" = seed|ERROR      (UTF-8 check, trim, 1..256)
#   noise.noise2 key x y = raw                 (x, y raw decimal)
#   noise.fbm2 key x y octaves base_freq lacunarity gain = raw|FATAL
#   sha256 "bytes" = hex                       (hex digest, lowercase, no 0x)
#   sha256.repeat "bytes" count = hex          (digest of bytes repeated count times)
"""


def tok_u(x):
    return "0x%x" % x


def tok_i(x):
    return str(x)


def tok_s(b):
    if isinstance(b, str):
        b = b.encode("latin-1")
    out = []
    for c in b:
        if 0x21 <= c <= 0x7E and c not in (0x22, 0x5C):
            out.append(chr(c))
        else:
            out.append("\\x%02x" % c)
    return '"' + "".join(out) + '"'


def tok_list(xs, f=tok_i):
    return "[" + ",".join(f(x) for x in xs) + "]"


def tok_b(v):
    return "true" if v else "false"


def tok_res(v, f=tok_i):
    if v == "FATAL" or v == "ERROR":
        return v
    return f(v)


class Out:
    def __init__(self):
        self.lines = [HEADER.rstrip("\n")]
        self.counts = {}

    def comment(self, text):
        self.lines.append("# " + text)

    def vec(self, kind, args, results):
        self.lines.append(" ".join([kind] + list(args) + ["="] + list(results)))
        self.counts[kind] = self.counts.get(kind, 0) + 1

    def text(self):
        return "\n".join(self.lines) + "\n"


def S(st):
    return [tok_u(w) for w in st]


def vec_rng_call(o, kind, st, extra_args, call, fmt):
    g = Rng.from_state(*st)
    try:
        r = call(g)
        o.vec(kind, S(st) + extra_args, [fmt(r), tok_i(g.draws)])
    except FatalError:
        o.vec(kind, S(st) + extra_args, ["FATAL"])


def gen_vectors():
    o = Out()
    vr = Rng(stream_key(0, "test.vectors"))   # input picker; inputs are printed

    def rand_raw():
        x = as_i64(vr.next())
        sh = vr.next() % 64
        x >>= sh
        return 1 if x == INT64_MIN else x

    # ---------------- Fx conversions
    o.comment("--- Fx conversions (spec 5.2)")
    for n in [0, 1, -1, -3, 7, 2147483647, -2147483647, 2147483648, -2147483648, 4294967296, INT64_MAX]:
        o.vec("fx.from_int", [tok_i(n)], [tok_res(outcome(fx_from_int, n))])
    for r in [0, 1, -1, INT64_MAX, -INT64_MAX, INT64_MIN]:
        o.vec("fx.from_raw", [tok_i(r)], [tok_res(outcome(fx_from_raw, r))])
    for num, den in [(1, 3), (-1, 3), (2, 3), (-2, 3), (1, 2), (1, 4), (355, 113), (-355, 113),
                     (1, 7), (22, 7), (1, 0), (0, 5), (1, -3), (-1, -3), (2147483647, 1),
                     (2147483648, 1), (1, 4294967296), (1, 8589934592), (3, 8589934592),
                     (-1, 8589934592), (-3, 8589934592), (1, 8589934593), (INT64_MAX, INT64_MAX),
                     (INT64_MIN, INT64_MAX), (INT64_MAX, 4294967296), (INT64_MIN, -4294967296),
                     (INT64_MIN, 4294967296), (1, INT64_MIN), (123456789, 1000)]:
        o.vec("fx.from_ratio", [tok_i(num), tok_i(den)], [tok_res(outcome(fx_from_ratio, num, den))])
    rounding = [0, 1, -1, HALF - 1, HALF, HALF + 1, -HALF + 1, -HALF, -HALF - 1, ONE, -ONE,
                6442450944, -6442450944, ONE + HALF - 1, -ONE - HALF + 1, 10737418240,
                -10737418240, RAW_MAX, RAW_MIN, 3 * ONE + HALF, -3 * ONE - HALF]
    for a in rounding:
        o.vec("fx.floor_int", [tok_i(a)], [tok_i(fx_floor_int(a))])
        o.vec("fx.ceil_int", [tok_i(a)], [tok_i(fx_ceil_int(a))])
        o.vec("fx.round_int", [tok_i(a)], [tok_i(fx_round_int(a))])
        o.vec("fx.trunc_int", [tok_i(a)], [tok_i(fx_trunc_int(a))])

    # ---------------- add / sub / sat / neg / abs
    o.comment("--- Fx add, sub, saturating, neg, abs (spec 5.4)")
    pairs = [(0, 0), (1, 2), (RAW_MAX, 1), (RAW_MAX, 0), (RAW_MIN, -1), (RAW_MIN, 0),
             (RAW_MAX, RAW_MIN), (RAW_MAX, RAW_MAX), (RAW_MIN, RAW_MIN), (ONE, -ONE),
             (-HALF, -HALF), (RAW_MAX - 5, 5), (RAW_MAX - 5, 6), (RAW_MIN + 5, -5), (RAW_MIN + 5, -6)]
    for a, b in pairs:
        o.vec("fx.add", [tok_i(a), tok_i(b)], [tok_res(outcome(fx_add, a, b))])
        o.vec("fx.sub", [tok_i(a), tok_i(b)], [tok_res(outcome(fx_sub, a, b))])
        o.vec("fx.sat_add", [tok_i(a), tok_i(b)], [tok_i(fx_sat_add(a, b))])
        o.vec("fx.sat_sub", [tok_i(a), tok_i(b)], [tok_i(fx_sat_sub(a, b))])
    for a in [0, 1, -1, RAW_MAX, RAW_MIN, ONE, -HALF]:
        o.vec("fx.neg", [tok_i(a)], [tok_i(fx_neg(a))])
        o.vec("fx.abs", [tok_i(a)], [tok_i(fx_abs(a))])

    # ---------------- mul / div
    o.comment("--- Fx mul and div (spec 5.4), hand-picked then pseudo-random")
    mul_cases = [(6442450944, 10737418240), (1, 1), (1, HALF), (-1, HALF), (3, HALF), (-3, HALF),
                 (5, HALF), (-5, HALF), (HALF, HALF), (ONE, ONE), (-ONE, ONE),
                 (fx_from_int(65536), fx_from_int(32767)), (fx_from_int(65536), fx_from_int(32768)),
                 (fx_from_int(-65536), fx_from_int(32768)), (RAW_MAX, ONE), (RAW_MIN, ONE),
                 (RAW_MAX, -ONE), (RAW_MAX, HALF), (RAW_MAX, ONE + 1), (RAW_MIN, ONE + 1),
                 (RAW_MAX, RAW_MAX), (0, RAW_MAX), (1431655765, 3 * ONE),
                 (fx_from_int(46340), fx_from_int(46340)), (fx_from_int(46341), fx_from_int(46341)),
                 (fx_from_int(-46341), fx_from_int(46341)), (3037000499 * 65536, 3037000499 * 65536),
                 (4294967295, 4294967295), (-4294967295, 4294967295), (2147483649, 1), (2147483647, 1),
                 (-2147483649, 1), (-2147483647, 1), (6074001000, 6074001000), (PI_RAW, PI_RAW)]
    for _ in range(64):
        mul_cases.append((rand_raw(), rand_raw()))
    for a, b in mul_cases:
        o.vec("fx.mul", [tok_i(a), tok_i(b)], [tok_res(outcome(fx_mul, a, b))])
    div_cases = [(ONE, fx_from_int(3)), (fx_from_int(2), fx_from_int(3)), (fx_from_int(-2), fx_from_int(3)),
                 (fx_from_int(2), fx_from_int(-3)), (fx_from_int(-2), fx_from_int(-3)),
                 (1, fx_from_int(2)), (-1, fx_from_int(2)), (1, fx_from_int(3)), (3, fx_from_int(2)),
                 (ONE, 1), (ONE, 0), (0, 0), (0, 5), (HALF, 1), (HALF - 1, 1), (-HALF + 1, 1),
                 (HALF, 2), (RAW_MAX, ONE), (RAW_MIN, ONE), (RAW_MAX, -ONE), (RAW_MAX, ONE - 1),
                 (RAW_MAX, RAW_MAX), (RAW_MIN, RAW_MAX), (1, RAW_MAX), (ONE, RAW_MAX),
                 (fx_from_int(1), PI_RAW), (fx_from_int(22), fx_from_int(7)),
                 (fx_from_int(-1000), 3), (ONE, 3), (ONE, -3)]
    for _ in range(64):
        a, b = rand_raw(), rand_raw()
        div_cases.append((a, b))
    for a, b in div_cases:
        o.vec("fx.div", [tok_i(a), tok_i(b)], [tok_res(outcome(fx_div, a, b))])
    o.comment("--- Fx mul_int, div_int (spec 5.4)")
    for a, n in [(ONE, 3), (HALF, -3), (RAW_MAX, 1), (RAW_MAX, 2), (RAW_MAX, -1), (RAW_MIN, -1),
                 (fx_from_int(1073741823), 2), (fx_from_int(1073741824), 2), (-1, INT64_MAX),
                 (1, INT64_MIN), (0, INT64_MIN), (12345, -6789)]:
        o.vec("fx.mul_int", [tok_i(a), tok_i(n)], [tok_res(outcome(fx_mul_int, a, n))])
    for a, n in [(ONE, 3), (-ONE, 3), (ONE, 0), (5, 2), (-5, 2), (5, -2), (7, 2), (3, 2), (1, 2),
                 (-1, 2), (1, 3), (RAW_MAX, 2), (RAW_MAX, -1), (RAW_MIN, -1), (RAW_MAX, INT64_MIN),
                 (-(1 << 62), INT64_MIN), ((1 << 62) - 1, INT64_MIN), (fx_from_int(10), 4)]:
        o.vec("fx.div_int", [tok_i(a), tok_i(n)], [tok_res(outcome(fx_div_int, a, n))])
    o.comment("--- int mul_div (spec 5.4)")
    for a, b, c in [(7, 3, 2), (-7, 3, 2), (7, -3, 2), (7, 3, -2), (-7, -3, -2), (1, 1, 3), (2, 1, 3),
                    (1, 1, 0), (INT64_MAX, INT64_MAX, INT64_MAX), (INT64_MAX, INT64_MAX, 1),
                    (INT64_MAX, 2, 2), (INT64_MAX, 3, 2), (-INT64_MAX, 1, 1), (INT64_MAX, -1, 1),
                    (1 << 62, 2, -1), (1 << 62, 2, 1), (1 << 62, -2, 1), (INT64_MAX, INT64_MAX, INT64_MAX - 1),
                    (1000000007, 998244353, 1 << 32), (-1000000007, 998244353, 1 << 32),
                    (5, 1, 10), (-5, 1, 10), (15, 1, 10), (25, 1, -10), (0, 0, 7)]:
        o.vec("int.mul_div", [tok_i(a), tok_i(b), tok_i(c)], [tok_res(outcome(mul_div, a, b, c))])
    o.comment("int.mul_div with an INT64_MIN argument (spec 5.4 does not exclude it; see README)")
    for a, b, c in [(INT64_MIN, 1, 2), (INT64_MIN, 1, -1), (INT64_MIN, -1, 2), (1, INT64_MIN, INT64_MIN),
                    (INT64_MIN, INT64_MIN, INT64_MIN), (3, 5, INT64_MIN)]:
        o.vec("int.mul_div", [tok_i(a), tok_i(b), tok_i(c)], [tok_res(outcome(mul_div, a, b, c))])
    o.comment("--- int checked helpers and floor_div / floor_mod (spec 5.4)")
    for a, b in [(1, 2), (INT64_MAX, 1), (INT64_MAX, 0), (INT64_MIN, -1), (INT64_MIN, 0),
                 (-(1 << 62), -(1 << 62)), (1 << 62, 1 << 62), (INT64_MAX, INT64_MIN), (-1, INT64_MIN)]:
        o.vec("int.checked_add", [tok_i(a), tok_i(b)], [tok_res(outcome(checked_add, a, b))])
        o.vec("int.checked_sub", [tok_i(a), tok_i(b)], [tok_res(outcome(checked_sub, a, b))])
    for a, b in [(3, 4), (-3, 4), (1 << 31, 1 << 31), (1 << 32, 1 << 31), (1 << 32, -(1 << 31)),
                 (1 << 32, 1 << 32), (INT64_MAX, -1), (INT64_MIN, 1), (INT64_MIN, -1),
                 (3037000499, 3037000499), (3037000500, 3037000500), (-3037000500, 3037000500)]:
        o.vec("int.checked_mul", [tok_i(a), tok_i(b)], [tok_res(outcome(checked_mul, a, b))])
    for a, b in [(7, 2), (-7, 2), (7, -2), (-7, -2), (6, 3), (-6, 3), (0, 5), (5, 0), (INT64_MIN, -1),
                 (INT64_MIN, 1), (INT64_MIN, 2), (INT64_MAX, -2), (-1, INT64_MAX), (-1, INT64_MIN),
                 (1, INT64_MIN), (-2048, 64), (-2049, 64)]:
        o.vec("int.floor_div", [tok_i(a), tok_i(b)], [tok_res(outcome(floor_div, a, b))])
        o.vec("int.floor_mod", [tok_i(a), tok_i(b)], [tok_res(outcome(floor_mod, a, b))])

    # ---------------- clamp, lerp, fade
    o.comment("--- Fx clamp, lerp, fade (spec 5.4, 5.5)")
    for x, lo, hi in [(5, 0, 10), (-5, 0, 10), (15, 0, 10), (0, 0, 0), (1, 2, 1), (RAW_MIN, -ONE, ONE)]:
        o.vec("fx.clamp", [tok_i(x), tok_i(lo), tok_i(hi)], [tok_res(outcome(fx_clamp, x, lo, hi))])
    lerps = [(fx_from_int(2), fx_from_int(4), HALF), (0, ONE, HALF), (0, 1, HALF), (0, -1, HALF),
             (0, 3, HALF), (ONE, -ONE, HALF), (0, ONE, 0), (0, ONE, ONE), (fx_from_int(-7), fx_from_int(13), 1431655765),
             (0, ONE, 2 * ONE), (0, ONE, -ONE), (RAW_MIN, RAW_MAX, 0), (-ONE, RAW_MAX, 0),
             (RAW_MAX, 0, ONE), (0, RAW_MAX, ONE), (1, RAW_MAX, ONE), (5, 2, 1),
             # mul(b - a, t) overflows although b - a and the exact a + (b-a)t fit:
             (-(1 << 62), (1 << 62) - 1, ONE + (1 << 20))]
    for _ in range(16):
        a = as_i64(vr.next()) >> 30
        b = as_i64(vr.next()) >> 30
        t = vr.next() >> 32
        lerps.append((a, b, t))
    for a, b, t in lerps:
        o.vec("fx.lerp", [tok_i(a), tok_i(b), tok_i(t)], [tok_res(outcome(fx_lerp, a, b, t))])
    fades = [0, 1, 2, HALF - 1, HALF, HALF + 1, ONE // 4, 3 * ONE // 4, ONE // 3, ONE - 1, ONE,
             -1, ONE + 1, 12345, ONE - 12345, 1 << 20, 1 << 26]
    for _ in range(16):
        fades.append(vr.next() >> 32)
    for t in fades:
        o.vec("fx.fade", [tok_i(t)], [tok_res(outcome(fade, t))])

    # ---------------- sqrt / hypot / isqrt / wide
    o.comment("--- isqrt, sqrt, hypot (spec 9.1)")
    for n in [0, 1, 2, 3, 4, 8, 9, 15, 16, 24, 25, 99, 100, 2**32 - 1, 2**32, 2**32 + 1,
              4294967295 ** 2, 4294967295 ** 2 - 1, 2**62, 2**63, M64 - 1, M64]:
        o.vec("isqrt.u64", [tok_u(n)], [tok_u(isqrt_u64(n))])
    u128s = [0, 1, 2, 3, 1 << 64, (1 << 64) - 1, (1 << 64) + 1, M64 ** 2, M64 ** 2 - 1, M64 ** 2 + 2 * M64,
             (1 << 128) - 1, 1 << 127, (1 << 126) + 12345, 3 << 64, 5 << 64, 2 << 64]
    for _ in range(12):
        hi_ = vr.next() >> (vr.next() % 64)
        lo_ = vr.next()
        u128s.append((hi_ << 64) | lo_)
    for n in u128s:
        o.vec("isqrt.u128", [tok_u(n >> 64), tok_u(n & M64)], [tok_u(isqrt_u128(n >> 64, n & M64))])
    sq = [0, 1, 2, 3, 4, -1, RAW_MIN, fx_from_ratio(1, 4), fx_from_int(2), fx_from_int(3), fx_from_int(4),
          fx_from_int(5), fx_from_int(10), fx_from_int(2147483647), RAW_MAX, ONE, ONE - 1, ONE + 1, HALF,
          fx_from_int(2048), fx_from_int(2048 * 2048), 65536, 65535, 65537, PI_RAW]
    for _ in range(32):
        sq.append(vr.next() >> (1 + vr.next() % 63))
    for x in sq:
        o.vec("fx.sqrt", [tok_i(x)], [tok_res(outcome(fx_sqrt, x))])
    hy = [(fx_from_int(3), fx_from_int(4)), (fx_from_int(-3), fx_from_int(4)), (fx_from_int(5), fx_from_int(-12)),
          (0, 0), (1, 1), (ONE, ONE), (ONE, 0), (0, -ONE), (RAW_MAX, 0), (RAW_MIN, 0), (RAW_MAX, 1),
          (RAW_MAX, 1 << 32), (RAW_MAX, 1 << 33), (RAW_MAX, RAW_MAX), (1 << 62, 1 << 62),
          (3 << 61, 3 << 61), (fx_from_int(2047), fx_from_int(2047)), (HALF, HALF)]
    for _ in range(24):
        hy.append((rand_raw(), rand_raw()))
    for a, b in hy:
        o.vec("fx.hypot", [tok_i(a), tok_i(b)], [tok_res(outcome(fx_hypot, a, b))])
    o.comment("--- U128 helpers (spec 5.3)")
    wm = [(M64, M64), (1 << 32, 1 << 32), (0, M64), (1, M64), (M64, 2), (0xFFFFFFFF, 0xFFFFFFFF),
          (0x100000001, 0xFFFFFFFF), (GOLDEN, GOLDEN), (0x8000000000000000, 2)]
    for _ in range(16):
        a_ = vr.next()
        b_ = vr.next() >> (vr.next() % 64)
        wm.append((a_, b_))
    for a, b in wm:
        h_, l_ = mul_u64_wide(a, b)
        o.vec("wide.mul", [tok_u(a), tok_u(b)], [tok_u(h_), tok_u(l_)])
    wd = [(1, 0, 3), (0, 0, 1), (0, 7, 2), (M64, M64, M64), (M64, M64, 1), (M64, M64, 2), (1, 0, 0),
          (0x7FFFFFFFFFFFFFFF, M64, 0x8000000000000000), (0xFFFFFFFF, 0, 0xFFFFFFFF00000001),
          (0x123456789, 0xABCDEF0123456789, 10 ** 18)]
    for _ in range(16):
        hi_ = vr.next() >> (vr.next() % 64)
        lo_ = vr.next()
        d_ = vr.next() >> (vr.next() % 64)
        wd.append((hi_, lo_, d_ | 1))
    for hi_, lo_, d_ in wd:
        r_ = outcome(divmod_u128_u64, hi_, lo_, d_)
        if r_ == "FATAL":
            o.vec("wide.divmod", [tok_u(hi_), tok_u(lo_), tok_u(d_)], ["FATAL"])
        else:
            o.vec("wide.divmod", [tok_u(hi_), tok_u(lo_), tok_u(d_)], [tok_u(x) for x in r_])

    # ---------------- decimal
    o.comment("--- decimal parse, canonical text, format (spec 5.6, 5.7)")
    parses = ["0", "-0", "+0", "+0.000", "-0.000", "1", "-1", "+1", "0.5", "-1.25", "0.1", "-0.1", "0.35",
              "2147483647", "-2147483647", "2147483648", "-2147483648", "2147483647.9999999",
              "-2147483647.9999999", "2147483647.99999999999999999", "2147483647.999999999767169356",
              "2147483647.999999999883584678", "2147483647.999999999883584677", "2147483647.999999999883584679", "0.000000000116415321",
              "0.000000000116415322", "0.000000000232830643", "-0.000000000116415322",
              "0.000000000000000001", "0.0000000000000000001", "0.999999999999999999", "9999999999",
              "1234567890.123456789012345678", "12345678901", "3.14159265358979", "0.0", "10.10",
              "1.000000000000000000", "-0.00000000011641532", ".5", "5.", "05", "00", "-05.0", "1e3",
              "1_000", " 1", "1 ", "", "-", "+", "+-1", "--1", "0x10", "1.2.3", "1..2", "1.-2",
              "\xd9\xa1", "\xef\xbc\x91", "1\x00", "\t1", "1,5", "-.5", "+.5", "0.5\n"]
    for t in parses:
        o.vec("fx.parse", [tok_s(t)], [tok_res(outcome(parse_decimal, t))])
    for t in ["+0.500", "-0", "3.000", "10", "-2.50", "0", "+0", "-0.000", "0.0", "-0.10", "1.000000000000000001",
              "2147483647.9999999", "+2147483647", "100.00100", "-1", "-0.5"]:
        o.vec("fx.canonical", [tok_s(t)], [tok_s(canonical_decimal(t))])
    fmts = [(HALF, 3), (-5368709120, 1), (1, 9), (-1, 2), (fx_from_int(-7), 0), (0, 0), (0, 18),
            (1, 18), (-1, 18), (RAW_MAX, 18), (RAW_MIN, 18), (RAW_MAX, 0), (RAW_MIN, 0), (RAW_MAX, 9),
            (HALF, 0), (-HALF, 0), (HALF - 1, 0), (-HALF + 1, 0), (ONE + HALF, 0), (-ONE - HALF, 0),
            (PI_RAW, 10), (-PI_RAW, 10), (429496730, 1), (429496730, 18), (1503238554, 2),
            (fx_from_ratio(1, 3), 5), (fx_from_ratio(2, 3), 5), (fx_from_ratio(-2, 3), 5),
            (ONE, -1), (ONE, 19), (ONE - 1, 9), (ONE - 1, 10), (2147483647 << 32, 3), (-2, 9)]
    for _ in range(24):
        fmts.append((rand_raw(), vr.next() % 19))
    for a, d in fmts:
        r_ = outcome(to_decimal, a, d)
        o.vec("fx.to_decimal", [tok_i(a), tok_i(d)], [tok_res(r_, tok_s)])

    # ---------------- splitmix / rng
    o.comment("--- SplitMix64 (spec 6.2)")
    for x in [0, 1234567, 1, M64, GOLDEN, 0xDEADBEEF]:
        o.vec("splitmix64", [tok_u(x), "8"], [tok_u(v) for v in splitmix64_seq(x, 8)])
    o.comment("--- xoshiro256** seeding and sequences (spec 6.1, 6.2)")
    seed_keys = [0, 1, 2, 1234567, M64, GOLDEN, stream_key(0, "test.noise"), stream_key(0, "test.hash")]
    for k in seed_keys:
        o.vec("rng.seed", [tok_u(k)], S(Rng(k).state()))
    states = [(1, 2, 3, 4), (0, 0, 0, 1), (1, 0, 0, 0), (M64, M64, M64, M64)] + [Rng(k).state() for k in seed_keys]
    for st in states:
        g = Rng.from_state(*st)
        o.vec("rng.next", S(st) + ["16"], [tok_u(g.next()) for _ in range(16)])
    for st in states[:6]:
        for k in [1, 2, 100]:
            g = Rng.from_state(*st)
            for _ in range(k):
                g.next()
            o.vec("rng.state_after", S(st) + [tok_i(k)], S(g.state()))
    for seed, name in [(0, "test.noise"), (0, "test.hash"), (0, "test.wide"), (1, "test.noise"),
                       (M64, "geo.elevation"), (42, "hist.awakening")]:
        g = Rng(stream_key(seed, name))
        o.vec("rng.stream", [tok_u(seed), tok_s(name), "8"], [tok_u(g.next()) for _ in range(8)])

    o.comment("--- sampling functions with draw counts (spec 6.3)")
    sst = [(1, 2, 3, 4)] + [Rng(substream(stream_key(0, "test.vectors"), i)).state() for i in range(6)]
    for st in sst[:4]:
        vec_rng_call(o, "rng.next_u32", st, [], lambda g: g.next_u32(), tok_i)
        vec_rng_call(o, "rng.uniform_fx01", st, [], lambda g: g.uniform_fx01(), tok_i)
    # uniform_u64: fixed bounds on every state, plus states found to reject
    bounds = [1, 2, 3, 7, 10, 1000, 2**32 + 1, 2**63, 2**63 + 1, M64, 0]
    for st in sst:
        for s in bounds:
            vec_rng_call(o, "rng.uniform_u64", st, [tok_u(s)], lambda g, s=s: g.uniform_u64(s), tok_u)
    # the spec's two-call case: uniform_u64(3) after one draw from (1,2,3,4)
    g = Rng.from_state(1, 2, 3, 4)
    g.next()
    vec_rng_call(o, "rng.uniform_u64", g.state(), [tok_u(3)], lambda g: g.uniform_u64(3), tok_u)
    o.comment("states chosen so that uniform_u64 rejects at least once (draws > 1)")
    found = 0
    i = 0
    rej_bound = [(1 << 63) + 1, (1 << 63) + (1 << 62), M64 // 3 * 2 + 1]
    while found < 9:
        st = Rng(substream(stream_key(0, "test.reject"), i)).state()
        s = rej_bound[found % 3]
        g = Rng.from_state(*st)
        g.uniform_u64(s)
        if g.draws > 1:
            vec_rng_call(o, "rng.uniform_u64", st, [tok_u(s)], lambda g, s=s: g.uniform_u64(s), tok_u)
            found += 1
        i += 1
    rej_states = [Rng(substream(stream_key(0, "test.reject"), j)).state() for j in range(i)]
    i64s = [(INT64_MIN, INT64_MAX), (0, 0), (-5, 5), (-10, -1), (INT64_MIN, INT64_MAX - 1),
            (INT64_MIN + 1, INT64_MAX), (INT64_MIN, -1), (0, INT64_MAX), (-1, 0), (1, 0),
            (INT64_MIN, INT64_MIN), (-2048, 2047)]
    for st in sst[:3]:
        for lo, hi in i64s:
            vec_rng_call(o, "rng.uniform_i64", st, [tok_i(lo), tok_i(hi)],
                         lambda g, lo=lo, hi=hi: g.uniform_i64(lo, hi), tok_i)
    for st in rej_states[:4]:
        vec_rng_call(o, "rng.uniform_i64", st, [tok_i(INT64_MIN), tok_i(1 << 62)],
                     lambda g: g.uniform_i64(INT64_MIN, 1 << 62), tok_i)
    fxr = [(0, ONE), (-ONE, ONE), (RAW_MIN, RAW_MAX), (5, 6), (ONE, ONE), (ONE, 0),
           (fx_from_int(-100), fx_from_int(100)), (HALF, ONE + HALF)]
    for st in sst[:3]:
        for lo, hi in fxr:
            vec_rng_call(o, "rng.uniform_fx", st, [tok_i(lo), tok_i(hi)],
                         lambda g, lo=lo, hi=hi: g.uniform_fx(lo, hi), tok_i)
    for st in sst[:4]:
        for p in [0, ONE, HALF, 1, ONE - 1, -1, ONE + 1, 1431655765]:
            vec_rng_call(o, "rng.chance", st, [tok_i(p)], lambda g, p=p: g.chance(p), tok_b)
    for st in sst[:3]:
        for num, den in [(0, 1), (1, 1), (1, 2), (1, 3), (2, 3), (0, 0), (2, 1), (1, M64), (M64 - 1, M64),
                         (1 << 63, (1 << 63) + 1)]:
            vec_rng_call(o, "rng.chance_ratio", st, [tok_u(num), tok_u(den)],
                         lambda g, num=num, den=den: g.chance_ratio(num, den), tok_b)
    wsets = [[0, 5, 3], [1], [0, 0, 1], [1, 2, 3, 0, 4], [], [0, 0], [M64, 1], [M64], [1 << 63, 1 << 63],
             [(1 << 63) + 1, 0, (1 << 62)], [3, 0, 0, 0, 0, 0, 0, 7], [1] * 20]
    for st in sst[:4]:
        for w in wsets:
            vec_rng_call(o, "rng.weighted_index", st, [tok_list(w, tok_u)],
                         lambda g, w=w: g.weighted_index(w), tok_i)
    for st in sst[:3]:
        for n in [0, 1, 2, 3, 5, 10, 32]:
            vec_rng_call(o, "rng.shuffle", st, [tok_i(n)], lambda g, n=n: g.shuffle(list(range(n))),
                         lambda v: tok_list(v))

    # ---------------- hashing
    o.comment("--- domain tags, mixing, packing, streams (spec 7)")
    for name, asc, val in TAGS:
        o.vec("hash.tag", [name], [tok_u(val)])
    for z in [0, 1, 2, GOLDEN, M64, 0x8000000000000000, 0x0123456789ABCDEF]:
        o.vec("hash.mix64", [tok_u(z)], [tok_u(mix64(z))])
    for h, w in [(0, 0), (0, GOLDEN), (0, 1), (1, 0), (M64, M64), (TAG_STREAM, 0), (TAG_SUB, 12345)]:
        o.vec("hash.absorb", [tok_u(h), tok_u(w)], [tok_u(absorb(h, w))])
    for tag, ws in [(0, []), (0, [0]), (TAG_STREAM, []), (TAG_STREAM, [0]), (TAG_CELL2, [1, 2, 3]),
                    (TAG_SUB, [M64, M64]), (TAG_PHRASE, [0])]:
        o.vec("hash.words", [tok_u(tag), tok_list(ws, tok_u)], [tok_u(hash_words(tag, ws))])
    for s in [b"", b"a", b"test.noise", b"The Unmapped", b"abcdefgh", b"abcdefghi", b"geo.elevation",
              b"\xff\x00\x80", "Ünmapped".encode("utf-8")]:
        o.vec("hash.pack", [tok_s(s)], [tok_list(pack_bytes(s), tok_u)])
    o.comment("the six hash values named in spec 13.5")
    o.vec("hash.stream_key", [tok_u(0), tok_s("test.noise")], [tok_u(stream_key(0, "test.noise"))])
    o.vec("hash.stream_key", [tok_u(M64), tok_s("geo.elevation")], [tok_u(stream_key(M64, "geo.elevation"))])
    o.vec("hash.substream", [tok_u(1), tok_u(0)], [tok_u(substream(1, 0))])
    o.vec("hash.at", [tok_u(1), "-1", "2"], [tok_u(hash_at(1, -1, 2))])
    o.vec("hash.at3", [tok_u(1), "0", "0", "0"], [tok_u(hash_at3(1, 0, 0, 0))])
    o.vec("hash.phrase_seed", [tok_s("The Unmapped")], [tok_u(phrase_seed(b"The Unmapped"))])
    o.comment("further hash and stream values")
    for seed, name in [(0, "test.hash"), (0, "test.wide"), (0, "test.vectors"), (1, "test.noise"),
                       (0, "a"), (0, "a.b"), (12345, "geo.elevation.continent"), (42, "hist.awakening"),
                       (0, "a" * 64)]:
        o.vec("hash.stream_key", [tok_u(seed), tok_s(name)], [tok_u(stream_key(seed, name))])
    for k, i in [(0, 0), (1, 1), (M64, M64), (stream_key(0, "test.noise"), 0), (stream_key(0, "test.noise"), 4)]:
        o.vec("hash.substream", [tok_u(k), tok_u(i)], [tok_u(substream(k, i))])
    for k, x, y in [(0, 0, 0), (1, 2, -1), (1, -1, -1), (M64, INT64_MIN, INT64_MAX), (12345, 2047, 2047),
                    (stream_key(0, "test.noise"), -3, 7)]:
        o.vec("hash.at", [tok_u(k), tok_i(x), tok_i(y)], [tok_u(hash_at(k, x, y))])
    for k, x, y, z in [(0, 0, 0, 0), (1, -1, 2, 3), (M64, INT64_MIN, 0, INT64_MAX), (7, 100, 200, -300)]:
        o.vec("hash.at3", [tok_u(k), tok_i(x), tok_i(y), tok_i(z)], [tok_u(hash_at3(k, x, y, z))])
    for b in [b"", b"x", b"the unmapped", b"abcdefgh", "Ünmapped".encode("utf-8")]:
        o.vec("hash.phrase_seed", [tok_s(b)], [tok_u(phrase_seed(b))])
    for b in [b"The Unmapped", b"  The Unmapped\n", b"\t\r\nThe Unmapped \t", b"the unmapped", b"",
              b" \t\r\n", b"\xff", b"\xc0\xaf", b"\xed\xa0\x80", "Ünmapped".encode("utf-8"),
              b"\x0bThe Unmapped", b"a" * 256, b"a" * 257, b" " + b"a" * 256 + b" ", b"The  Unmapped"]:
        o.vec("hash.phrase_text", [tok_s(b)], [tok_res(outcome(phrase_seed_text, b), tok_u)])

    # ---------------- noise
    o.comment("--- noise2 (spec 8.1); keys: stream_key(0,test.noise) and substream(that, 3)")
    kn = stream_key(0, "test.noise")
    pts = [(0, 0), (ONE, 0), (HALF, HALF), (-HALF, HALF), (fx_from_int(3) + HALF, fx_from_int(-5) + HALF),
           (parse_decimal("0.25"), parse_decimal("0.75")), (parse_decimal("1.75"), parse_decimal("-3.125")),
           (parse_decimal("123.456"), parse_decimal("-78.9")), (parse_decimal("-1000.001"), parse_decimal("999.999")),
           (fx_from_ratio(1, 3), fx_from_ratio(2, 3)), (fx_from_ratio(-1, 7), fx_from_ratio(-22, 7)),
           (1, -1), (-1, 1), (parse_decimal("2047.9"), parse_decimal("0.0001")),
           (parse_decimal("-2147483.5"), parse_decimal("2147483.25")), (fx_from_int(2147483646) + ONE - 1, -(fx_from_int(2147483646) + ONE - 1)),
           (parse_decimal("0.1"), parse_decimal("0.2")), (parse_decimal("-0.9"), parse_decimal("-0.1")),
           (parse_decimal("17.5"), parse_decimal("42.5")), (parse_decimal("-17.5"), parse_decimal("-42.5"))]
    for k in [kn, substream(kn, 3)]:
        for x, y in pts:
            o.vec("noise.noise2", [tok_u(k), tok_i(x), tok_i(y)], [tok_i(noise2(k, x, y))])
    o.comment("--- fbm2 (spec 8.2); first block: P0 test pattern params (5, 1/32, 2, 1/2)")
    bf, lac, gain = 134217728, 2 * ONE, HALF
    fpts = [(0, 0), (HALF, HALF), (fx_from_int(1), fx_from_int(1)), (fx_from_int(16), fx_from_int(16)),
            (fx_from_int(100), fx_from_int(200)), (fx_from_int(2047), fx_from_int(2047)),
            (fx_from_int(-1), fx_from_int(-1)), (fx_from_int(-37) + HALF, fx_from_int(511) + HALF),
            (parse_decimal("123.456"), parse_decimal("-78.9")), (parse_decimal("-1000.001"), parse_decimal("999.999")),
            (fx_from_ratio(1, 3), fx_from_ratio(2, 3)), (fx_from_ratio(-1, 7), fx_from_ratio(-22, 7)),
            (parse_decimal("1024.25"), parse_decimal("512.75")), (parse_decimal("-2048.5"), parse_decimal("2048.5")),
            (fx_from_int(33), fx_from_int(-65)), (parse_decimal("7.7"), parse_decimal("1999.1"))]
    for x, y in fpts:
        o.vec("noise.fbm2", [tok_u(kn), tok_i(x), tok_i(y), "5", tok_i(bf), tok_i(lac), tok_i(gain)],
              [tok_res(outcome(fbm2, kn, x, y, 5, bf, lac, gain))])
    o.comment("other fbm2 parameters, including argument errors")
    others = [(1, ONE, 2 * ONE, HALF), (16, 1 << 20, 2 * ONE, HALF), (8, ONE // 10, parse_decimal("1.9"), parse_decimal("0.6")),
              (3, ONE, 3 * ONE, ONE), (4, fx_from_int(7), HALF, parse_decimal("0.25")),
              (0, ONE, 2 * ONE, HALF), (17, ONE, 2 * ONE, HALF), (1, ONE, 2 * ONE, 0),
              (1, 0, 2 * ONE, HALF), (1, ONE, 0, HALF), (1, ONE, 2 * ONE, ONE + 1), (1, -ONE, 2 * ONE, HALF)]
    for oc, b_, l_, g_ in others:
        for x, y in [(parse_decimal("3.3"), parse_decimal("-4.4")), (fx_from_int(-100), fx_from_int(250))]:
            o.vec("noise.fbm2", [tok_u(kn), tok_i(x), tok_i(y), tok_i(oc), tok_i(b_), tok_i(l_), tok_i(g_)],
                  [tok_res(outcome(fbm2, kn, x, y, oc, b_, l_, g_))])
    o.comment("the f update after the last octave overflows (f is never used again); literal 8.2 reading: fatal")
    o.vec("noise.fbm2", [tok_u(kn), "0", "0", "1", tok_i(fx_from_int(1 << 20)), tok_i(fx_from_int(1 << 11)), tok_i(HALF)],
          [tok_res(outcome(fbm2, kn, 0, 0, 1, fx_from_int(1 << 20), fx_from_int(1 << 11), HALF))])
    o.comment("overflow of mul(x, f) is fatal")
    o.vec("noise.fbm2", [tok_u(kn), tok_i(fx_from_int(2000000000)), "0", "2", tok_i(ONE), tok_i(2 * ONE), tok_i(HALF)],
          [tok_res(outcome(fbm2, kn, fx_from_int(2000000000), 0, 2, ONE, 2 * ONE, HALF))])

    # ---------------- sha256
    o.comment("--- SHA-256 (spec 10, 13.7)")
    for m in [b"", b"abc", b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
              b"The Unmapped", b"\x00", bytes(range(1, 128)),
              b"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"]:
        o.vec("sha256", [tok_s(m)], [sha256_hex(m)])
    for m, n in [(b"a", 1000000), (b"a", 55), (b"a", 56), (b"a", 63), (b"a", 64), (b"a", 65),
                 (b"a", 119), (b"a", 120), (b"\x00", 1000), (b"0123456789", 13)]:
        o.vec("sha256.repeat", [tok_s(m), tok_i(n)], [sha256_hex(m * n)])
    return o


def main(argv):
    if len(argv) < 2 or argv[1] not in ("check", "generate"):
        print(__doc__)
        return 2
    if argv[1] == "check":
        return 0 if run_checks() else 1
    o = gen_vectors()
    text = o.text()
    text.encode("ascii")
    dest = argv[2] if len(argv) > 2 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "golden", "core_vectors.txt")
    if dest == "-":
        sys.stdout.write(text)
    else:
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="ascii", newline="\n") as f:
            f.write(text)
        total = sum(o.counts.values())
        print(f"wrote {dest}: {total} vectors", file=sys.stderr)
        for k in sorted(o.counts):
            print(f"  {k:24s} {o.counts[k]}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
