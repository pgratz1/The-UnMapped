# P0-01 Deterministic core

Status: approved by the lead, 2026-11-15; amended the same day after the first implementation and the independent reference (edge cases in 5.4, 5.6, 6.2, 7.3, 8.1, 8.2, 13.3, 13.5, 13.8). Seed phrases are scrubbed (7.3, 13.5) from generator version 0.2.0, per the design doc. Phase P0 Foundation. Design doc sections 4 (determinism rules), 10, 13.

## 1. Purpose

Every later stage computes with the primitives in this spec and nothing else: one fixed-point type, one random generator, one way to derive seed streams and location-keyed values, one integer gradient noise, a small set of math functions, and SHA-256. Each primitive is defined as an exact integer function, so any correct implementation gives the same bits on GCC, Clang and MSVC, Linux and Windows. Where a result is defined mathematically ("the exact product rounded half away from zero"), any implementation that produces that result conforms; the reference methods given here are one way to get it.

Code location (interim, for the generator engineer): `worldgen/include/worldgen/core/` and `worldgen/src/core/`. Namespace `worldgen`.

## 2. Inputs

None. These are pure functions and value types. The only external input is the 64-bit world seed, which callers pass in.

## 3. Outputs

Nothing is written to the database by this spec. All other P0 specs and every later stage depend on it.

## 4. Platform assumptions and general rules

These apply to all code in `worldgen/` whose results can reach the database (the "output path").

| Rule | Detail |
| --- | --- |
| Targets | 64-bit only (x86-64 now; arm64 must also work). C++20, which guarantees two's complement and arithmetic right shift of negative signed values. |
| Integer types | Use `std::int64_t`, `std::uint64_t`, `std::int32_t`, `std::uint32_t`, `std::uint8_t` and so on. Never `long`, `unsigned long`, `wchar_t` or plain `char` arithmetic in the output path (`long` is 32-bit on Windows, 64-bit on Linux). Convert `std::size_t` to `std::uint64_t` explicitly before hashing or serializing it. Treat bytes as `std::uint8_t`. |
| Signed overflow | Undefined behaviour in C++20, so never allowed. Fixed-point operations below are checked. Plain integer code must either have a documented bound that rules out overflow or use the checked helpers (`checked_add`, `checked_sub`, `checked_mul` on `int64_t`, which raise the fatal error on overflow). |
| Shifts | Shift counts are always in `[0, width-1]`. Left shifts of negative signed values are done on the unsigned reinterpretation. |
| Byte order | Every multi-byte value that is hashed, stored in a blob or compared across platforms is serialized little-endian by arithmetic (`b[i] = (v >> (8*i)) & 0xFF`), never by `memcpy` of a struct or integer. |
| Evaluation order | Function arguments and the operands of `+`, `*`, `==` etc. are evaluated in an unspecified order, and compilers differ. **Never make two random draws, or two calls with side effects on the same state, within one full expression.** `f(rng.next(), rng.next())` and `a = rng.next() + rng.next()` are banned; each draw is its own statement. |
| Sorting | `std::sort`, `std::partial_sort`, `std::nth_element` and `std::partition` are not stable and order equal elements differently between standard libraries. In the output path, sort only with a comparator that is a strict total order on the elements (ties broken by a unique key), or use `std::stable_sort`. |
| Containers | No iteration over `std::unordered_*` containers. No ordering or hashing by pointer value. No `std::hash`. |
| Banned in the output path | `float`, `double`, `long double`; `<cmath>`; `<random>` (engines and distributions); `std::shuffle`, `std::sample`; `std::chrono`, `time`, `clock`; `std::thread::hardware_concurrency` as a parameter of results; locale-dependent formatting or parsing (`printf("%f")`, iostream number formatting under a locale, `std::tolower`/`isalpha` with non-ASCII input, `setlocale`). Use `std::to_chars`/`std::from_chars` for integers only, or project code. |
| Compilation | No fast-math flags anywhere (`-ffast-math`, `/fp:fast`). The build engineer also sets `-ffp-contract=off` on GCC/Clang as a backstop for non-output code. |
| Static state | No mutable global or static state in the output path (no hidden global RNG, no caches whose contents depend on call order). |

### 4.1 Fatal error

Every contract violation in this spec (overflow, division by zero, square root of a negative number, empty weight set, invalid argument) **raises the project fatal error**: an exception type `worldgen::FatalError` (derived from `std::exception`, carrying a message) that is never caught inside `worldgen/`. The operation never returns a wrapped, saturated or otherwise substituted value. `worldgen-cli` catches it at top level, deletes any partial output and exits with code 1 (see P0-04). Because the inputs are deterministic, a world that overflows fails on every platform.

Saturating variants exist only where named (`sat_add`, `sat_sub`), for code that wants them explicitly.

### 4.2 Parallel work

P0 code is single-threaded. When a later stage runs in parallel, it must follow this rule, and its spec must say how:

1. Split work into chunks whose boundaries depend only on the data dimensions and a constant chunk size written in the spec (for example rows in blocks of 64), never on the thread count.
2. Inside a chunk, randomness comes only from location-keyed hashes (section 7.3) or from a stream derived from the chunk index (`substream(key, chunk_index)`). Never share one `Rng` between threads.
3. Combine chunk results in ascending chunk index. Exact integer sums may be combined in any order, since they are associative; anything else (fixed-point products, concatenated lists, min/max with ties, first-found) is combined strictly in chunk order.
4. Running with 1 thread and with N threads gives identical output. Every parallel stage has a test that checks this.

## 5. Fixed-point type `Fx` (Q32.32)

### 5.1 Format

| Property | Value |
| --- | --- |
| Storage | One `std::int64_t` "raw" value. |
| Meaning | value = raw / 2^32. 32 integer bits including sign, 32 fractional bits. |
| `ONE` | raw 2^32 = 4294967296 |
| `HALF` | raw 2^31 = 2147483648 |
| Resolution | 2^-32, about 2.33e-10 |
| Valid raw range | [-(2^63 - 1), 2^63 - 1], that is about ±2147483648.0. **raw = INT64_MIN (-2^63) is not a valid value**; any operation that would produce it is overflow. This makes negation and absolute value always defined. |
| `MAX` / `MIN` | raw 2^63 - 1 / raw -(2^63 - 1) |
| `PI` | raw 13493037705 (0x3243F6A89), = round(pi × 2^32). Provided for P1; no P0 function uses it. |

Why Q32.32: geography needs range (elevation in metres, distances in cells up to 2048, squared distances, sums over neighbourhoods) and precision (noise accumulated over octaves, erosion increments). Q16.16 in 32 bits overflows at 32768 and squares overflow beyond 181; Q32.32 in 64 bits removes most of that pain at the cost of 128-bit intermediates for multiply and divide (section 5.3). Layers stored in the database are quantized to narrower integer types by each stage's spec; `Fx` itself is an in-memory computation type and, where a column holds one, it is stored as its raw `int64`.

`Fx` is a value type with no implicit conversions from or to any arithmetic type. Construction is only by the named functions below, plus a default constructor that gives raw 0 so that `Fx` can be held in arrays and vectors. Comparison (`==`, `<`, etc.) compares raw values.

### 5.2 Conversions

| Function | Definition | Fatal when |
| --- | --- | --- |
| `Fx::from_raw(int64 r)` | raw = r | r = INT64_MIN |
| `Fx::from_int(int64 n)` | raw = n × 2^32 | n < -(2^31 - 1) or n > 2^31 - 1 |
| `Fx::from_ratio(int64 num, int64 den)` | raw = RHA(num × 2^32 / den), exact | den = 0, or result out of range |
| `parse_decimal(text)` | section 5.6 | grammar or range error (returned as an error value, not fatal, because the input is user data) |
| `raw()` | returns raw | never |
| `floor_int()` | raw >> 32 (arithmetic shift = floor) | never |
| `ceil_int()` | -((-raw) >> 32) | never |
| `round_int()` | RHA(raw / 2^32) | never |
| `trunc_int()` | toward zero: sign(raw) × (\|raw\| >> 32) | never |
| `to_decimal(int d)` | section 5.7, 0 ≤ d ≤ 18. Provided both as a member, `x.to_decimal(d)`, and as a free function, `to_decimal(x, d)`. | d out of range |

**RHA** (round half away from zero) of an exact rational n/d: q = floor(\|n\| / \|d\|), r = \|n\| - q·\|d\|; if 2r ≥ \|d\| then q = q + 1; result = q with the sign of n/d. This is the only rounding mode used for multiply, divide, `mul_div`, `from_ratio` and decimal conversion. It makes every operation odd-symmetric: `mul(-a, b) == -mul(a, b)`.

There is no conversion to or from `float`/`double` in `worldgen/`. Code outside the output path that wants one (a debug print, the Godot viewer) does it itself from `raw()`.

### 5.3 128-bit intermediates

Multiply and divide need 128-bit intermediates. MSVC has no `__int128`, so the core provides its own:

- `U128` — a struct of two `uint64_t` (`hi`, `lo`), value hi·2^64 + lo.
- `mul_u64_wide(uint64 a, uint64 b) → U128` — the exact product.
- `divmod_u128_u64(U128 n, uint64 d) → (quotient U128, remainder uint64)` — exact floor division; d ≠ 0.
- Comparison, addition, subtraction and shifts on `U128` as needed.

**Reference method (portable, required):** `mul_u64_wide` splits each operand into 32-bit halves and sums the four 32×32→64 partial products with carries. `divmod_u128_u64` uses restoring shift-subtract long division over the 128 bits (or two steps of 128/64 using 32-bit digits); any exact method is acceptable.

**Fast paths (optional):** the exact product and the floor quotient are unique, so an implementation may use `unsigned __int128` on GCC/Clang or `_umul128`/`_udiv128` on MSVC x64 inside these two functions only, provided the portable reference path is also compiled on every compiler and a unit test compares the two over the vector set in section 13.3 and 10^6 pairs from a fixed `Rng` stream. Nothing outside these functions uses compiler-specific wide types.

### 5.4 Arithmetic

All operations are on raw values, exact before rounding, checked after.

| Operation | Definition | Rounding | Fatal when |
| --- | --- | --- | --- |
| `a + b`, `a - b` | raw sum / difference | exact | result outside the valid range |
| `-a` | -raw | exact | never (INT64_MIN excluded) |
| `abs(a)` | \|raw\| | exact | never |
| `mul(a, b)` (also `a * b`) | RHA(a.raw × b.raw / 2^32) | RHA | result outside the valid range |
| `div(a, b)` (also `a / b`) | RHA(a.raw × 2^32 / b.raw) | RHA | b = 0; result outside the valid range |
| `mul_int(a, int64 n)` | raw × n | exact | overflow |
| `div_int(a, int64 n)` | RHA(raw / n) | RHA | n = 0 |
| `mul_div(int64 a, int64 b, int64 c)` | RHA(a × b / c) with a 128-bit product; a plain integer helper used by `Fx` and by stages | RHA | c = 0; result outside int64 or = INT64_MIN |
| `min`, `max`, `clamp(x, lo, hi)` | on raw; clamp requires lo ≤ hi | exact | lo > hi |
| `lerp(a, b, t)` | a + mul(b - a, t), evaluated exactly in that order | one RHA in mul | b - a, the product or the sum overflows, even when the exact final value would fit. `t` is not range-checked. |
| `sat_add`, `sat_sub` | as `+`/`-`, but clamp to MIN/MAX instead of failing | exact | never |

Implementation of `mul` (reference): sign = sign(a) × sign(b); P = mul_u64_wide(\|a.raw\|, \|b.raw\|); q = P >> 32; r = P mod 2^32; if r ≥ 2^31 then q = q + 1; fail if q > 2^63 - 1; result = sign × q.

Implementation of `div` (reference): sign as above; N = \|a.raw\| × 2^32 as U128; (q, r) = divmod(N, \|b.raw\|); if 2r ≥ \|b.raw\| then q = q + 1 (compute 2r in uint64: r < \|b\| ≤ 2^63 so 2r < 2^64); fail if q > 2^63 - 1; result = sign × q.

Plain-integer equivalents in the same file: `checked_add/sub/mul(int64, int64)`, `floor_div(int64, int64)`, `floor_mod(int64, int64)` (result has the sign of the divisor), defined exactly as their names say. Edge cases: the `checked_*` helpers may return INT64_MIN (only `Fx` excludes it). `floor_div(a, 0)` and `floor_mod(a, 0)` are fatal. `floor_div(INT64_MIN, -1)` is fatal (overflow) and `floor_mod(INT64_MIN, -1)` is 0. `mul_div` accepts INT64_MIN as an argument (magnitudes are taken in uint64); only its result is range-checked.

### 5.5 `fade` (quintic smoothstep)

Used by noise and available to stages. Input t with 0 ≤ t.raw ≤ ONE; fatal otherwise.

```
t2 = mul(t, t)
t3 = mul(t2, t)
a  = Fx::from_raw(6 * t.raw - 15 * ONE)     // exact; |6t| ≤ 6·2^32
b  = mul(t, a) + Fx::from_raw(10 * ONE)
f  = mul(t3, b)
return clamp(f, 0, ONE)
```

This evaluates 6t^5 - 15t^4 + 10t^3 in exactly this order, with RHA at each `mul`. fade(0) = 0, fade(HALF) = HALF exactly, fade(ONE) = ONE.

### 5.6 Decimal parsing `parse_decimal(text) → Fx or error`

This is how fractional numbers from TOML (P0-03) and the command line reach the generator, with no platform float parsing.

Grammar (ASCII, whole string, no surrounding whitespace):

```
decimal  = [ "-" | "+" ] int-part [ "." frac-part ]
int-part = "0" | nonzero-digit { digit }        ; at most 10 digits
frac-part= digit { digit }                      ; 1 to 18 digits
```

No exponent, no underscores, no leading `.`, no trailing `.`, no leading zeros in the integer part except a lone `0`. More than 18 fractional digits is an error, not a silent truncation.

Value: let N be the integer formed by all digits of int-part and frac-part concatenated (N < 10^28 < 2^94) and f the number of fractional digits. raw magnitude = RHA(N × 2^32 / 10^f), computed exactly in U128 (N × 2^32 < 2^126; 10^f ≤ 10^18 < 2^60). Apply the sign after rounding. Error if the magnitude exceeds 2^63 - 1. "-0" and "-0.000" give raw 0.

Note: with f ≤ 18 a tie (exact .5 remainder) is impossible, because a tie would need 2^33 to divide the denominator, which takes f ≥ 33. The rounding rule is still stated so that the definition is complete.

**Canonical text** (used when a value is stored in `meta`, P0-02): remove a leading `+`; if a fraction is present, remove trailing zeros from it and then remove the `.` if the fraction is empty; "-0" becomes "0". Canonical text parses to the same raw value as the original. It is a normalization of the text of a valid decimal, not a unique name for a value: "0.000000000000000001" and "0" both parse to raw 0 and each is canonical, and "-0.000000000000000001" keeps its sign. Only text whose digits are all zero loses its sign ("-0", "-0.000" and "+0" give "0"). Text that fails the grammar has no canonical form; the function returns an error value.

### 5.7 Decimal formatting `to_decimal(d)`

For reports. scaled = RHA(raw × 10^d / 2^32) (U128 product; raw × 10^18 < 2^123). Output: "-" if scaled < 0 (never "-0"), then \|scaled\| / 10^d in decimal without leading zeros (at least one digit), then, if d > 0, "." followed by \|scaled\| mod 10^d zero-padded to d digits.

## 6. Random generator `Rng` (xoshiro256**)

### 6.1 Algorithm

One algorithm for the whole project: **xoshiro256\*\*** (Blackman and Vigna, 2018), 256-bit state, 64-bit output.

State: four `uint64_t` words s0, s1, s2, s3, never all zero. All arithmetic is modulo 2^64. `rotl(x, k) = (x << k) | (x >> (64 - k))` for 0 < k < 64.

```
next():
    result = rotl(s1 * 5, 7) * 9
    t  = s1 << 17
    s2 = s2 ^ s0
    s3 = s3 ^ s1
    s1 = s1 ^ s2
    s0 = s0 ^ s3
    s2 = s2 ^ t
    s3 = rotl(s3, 45)
    return result
```

### 6.2 Seeding

`Rng(uint64 key)`: run SplitMix64 from x = key and take its first four outputs as s0, s1, s2, s3 in that order.

```
splitmix64_next(x):            // x is updated in place
    x = x + 0x9E3779B97F4A7C15
    z = x
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
    z = (z ^ (z >> 27)) * 0x94D049BB133111EB
    return z ^ (z >> 31)
```

SplitMix64 outputs are a bijection of distinct counter values, so at most one of the four words can be zero and the state is never all zero.

`Rng::from_state(s0, s1, s2, s3)` sets the state directly (fatal if all four are zero). It exists for tests and test vectors.

`Rng` is movable and **not copyable** (copy constructor and copy assignment deleted), because copying a generator silently duplicates a stream. Two generators with the same stream are made by constructing both from the same key. Moving an `Rng` transfers the stream: the move constructor and move assignment set the source's four state words to zero, and `next()` (and so every sampling function) raises the fatal error when the state is all zero. A moved-from `Rng` therefore cannot silently repeat the stream it gave away; it may be assigned a new generator and used again.

Keys come from section 7. A stage never seeds an `Rng` with the raw world seed.

### 6.3 Sampling functions

These replace every standard distribution. Each one's draw count is part of its definition, because later draws depend on it.

| Function | Definition | Draws |
| --- | --- | --- |
| `next() → uint64` | section 6.1 | 1 |
| `next_u32() → uint32` | next() >> 32 | 1 |
| `uniform_u64(uint64 s) → [0, s)` | Lemire's method, below. Fatal if s = 0. | ≥ 1 |
| `uniform_i64(int64 lo, int64 hi) → [lo, hi]` | below. Fatal if lo > hi. | ≥ 1 |
| `uniform_fx01() → Fx in [0, 1)` | raw = next() >> 32 | 1 |
| `uniform_fx(Fx lo, Fx hi) → Fx in [lo, hi)` | raw = lo.raw + uniform_u64(hi.raw - lo.raw) with the difference taken in uint64. Fatal if lo ≥ hi. | ≥ 1 |
| `chance(Fx p) → bool` | uniform_fx01().raw < p.raw. Fatal unless 0 ≤ p ≤ ONE. p = 0 is always false, p = ONE always true. | 1 |
| `chance_ratio(uint64 num, uint64 den) → bool` | uniform_u64(den) < num. Fatal if den = 0 or num > den. | ≥ 1 |
| `weighted_index(span<const uint64> w) → index` | below | ≥ 1 |
| `shuffle(span<T> a)` | below | n - 1 calls of uniform_u64 |

**`uniform_u64(s)`** (Lemire 2019, "Fast random integer generation in an interval"), all in uint64 except the product:

```
x = next(); m = mul_u64_wide(x, s); l = low64(m)
if l < s:
    t = (0 - s) mod s              // = (2^64 - s) mod s, computed in uint64
    while l < t:
        x = next(); m = mul_u64_wide(x, s); l = low64(m)
return high64(m)
```

Result is unbiased. It always consumes at least one draw, including for s = 1 (which returns 0).

**`uniform_i64(lo, hi)`**: span = uint64(hi) - uint64(lo) (mod 2^64). If span = 2^64 - 1, return int64(next()) (two's-complement reinterpretation). Otherwise return int64(uint64(lo) + uniform_u64(span + 1)) (mod 2^64).

**`weighted_index(w)`**: weights are non-negative integers in index order (callers with `Fx` weights pass raw values, which must be ≥ 0). total = checked sum (fatal on uint64 overflow); fatal if w is empty or total = 0. r = uniform_u64(total). Walk i = 0, 1, …, keeping acc = w0 + … + wi; return the first i with r < acc. A zero weight is never chosen.

**`shuffle(a)`** (Fisher–Yates, descending): for i = n - 1 down to 1: j = uniform_u64(i + 1); swap a[i] and a[j]. For n ≤ 1, no draws.

Deferred: approximately normal draws (for example an Irwin–Hall sum of k uniforms) and other shapes are added by the spec that first needs them, in this file, with exact definitions.

## 7. Seed streams and hashing

### 7.1 Mixing primitives

```
GOLDEN = 0x9E3779B97F4A7C15
mix64(z):                      // SplitMix64 finalizer; a bijection on uint64
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
    z = (z ^ (z >> 27)) * 0x94D049BB133111EB
    return z ^ (z >> 31)
absorb(h, w) = mix64((h ^ w) + GOLDEN)                 // mod 2^64
hash_words(tag, w[0..n-1]):
    h = tag
    for i in 0..n-1: h = absorb(h, w[i])
    return absorb(h, n)
```

For fixed h, `absorb(h, ·)` is a bijection, so two word sequences of the same length that differ only in their last word never collide.

### 7.2 Encoding rules

| Value | Encoded as |
| --- | --- |
| uint64 | itself |
| int64 (coordinates, indices) | its two's-complement bit pattern as uint64 |
| smaller integers | widened to 64 bits (sign-extended for signed types), then as above |
| byte string (stage names, phrases) | first a word holding the length in bytes, then the bytes packed 8 per word little-endian (byte i of the string goes to bits 8·(i mod 8) … of word i / 8), the last word zero-padded. The empty string is one word, 0. |

Domain tags (8 ASCII bytes packed little-endian, so they read as text in a hex dump):

| Tag | ASCII | uint64 |
| --- | --- | --- |
| `TAG_STREAM` | `UNM:STRM` | 0x4D5254533A4D4E55 |
| `TAG_SUB` | `UNM:SUBS` | 0x534255533A4D4E55 |
| `TAG_CELL2` | `UNM:CEL2` | 0x324C45433A4D4E55 |
| `TAG_CELL3` | `UNM:CEL3` | 0x334C45433A4D4E55 |
| `TAG_PHRASE` | `UNM:PHRS` | 0x535248503A4D4E55 |

### 7.3 Derivations

| Function | Definition | Use |
| --- | --- | --- |
| `stream_key(uint64 seed, name)` | hash_words(TAG_STREAM, [seed, encoded name…]), where the encoded name is the byte-string encoding of 7.2: its length word followed by its packed words. The length appears once. A name that fails the stream-name grammar below is fatal. | One per stage or purpose, e.g. `stream_key(seed, "geo.elevation")`. `Rng(stream_key(...))` is the stage's stream. |
| `substream(uint64 key, uint64 index)` | hash_words(TAG_SUB, [key, index]) | Per octave, per chunk, per entity, per iteration. |
| `hash_at(uint64 key, int64 x, int64 y)` | hash_words(TAG_CELL2, [key, x, y]) | Location-keyed values: independent of evaluation order. |
| `hash_at3(uint64 key, int64 x, int64 y, int64 z)` | hash_words(TAG_CELL3, [key, x, y, z]) | Location plus a third index (layer, year, octave). |
| `phrase_seed(bytes)` | hash_words(TAG_PHRASE, [len, packed words…]). A pure hash of exactly the bytes given: no validation and no trimming. | The hashing step of a text-phrase seed. |
| `seed_from_phrase(text)` | Scrubs the text by the text-phrase rules below, then `phrase_seed` of the scrubbed bytes. Returns the scrubbed phrase and the seed, or an error value (the phrase scrubs to nothing; the scrubbed phrase is longer than 256 bytes). Not fatal, because the phrase is user data. | Text-phrase world seed → 64 bits. This is what the settings loader and the command line call. |

**Stream names.** Grammar `[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)*`, 1 to 64 bytes, ASCII. Convention: `<area>.<stage>[.<purpose>]`, e.g. `geo.elevation`, `geo.elevation.continent`, `hist.awakening`. Every name used anywhere is listed as a constant in one source file (`worldgen/include/worldgen/core/streams.hpp`, interim), and a test checks that the list has no duplicates. P0 names: `test.hash`, `test.noise` (used by the test pattern in P0-04).

**Text phrases.** (Design doc section 4; this replaces the earlier trim-only rule.) A phrase is **scrubbed** before it is hashed, byte by byte, with no knowledge of any character encoding:

1. Each byte `A`–`Z` (0x41–0x5A) becomes the corresponding lower-case byte `a`–`z` (add 0x20).
2. Each byte that is then not `a`–`z` (0x61–0x7A) and not `0`–`9` (0x30–0x39) becomes a space (0x20). This includes every byte of a multi-byte UTF-8 character, every control character, NUL, punctuation and whitespace.
3. Each run of spaces becomes one space.
4. A leading space and a trailing space are removed.

The result contains only `a`–`z`, `0`–`9` and single interior spaces. If it is empty the phrase is rejected (error value: empty). If it is longer than 256 bytes it is rejected (error value: too long); the length of the text before scrubbing is not limited. The input need not be valid UTF-8, since every non-ASCII byte becomes a space. seed = phrase_seed(scrubbed bytes); `seed_from_phrase` does all of this in one call. So "Ashen Marches!", "ashen-marches" and "ASHEN  MARCHES" all scrub to `ashen marches` and give the same seed, and "Ünmapped Wörld" scrubs to `nmapped w rld`. A phrase made only of other characters, such as one written entirely in a non-Latin script, scrubs to nothing and is rejected. Scrubbing is idempotent. The input is a sized byte string: a NUL byte is a separator like any other, never a terminator. No Unicode normalization or accent folding is applied, so "Café" with a precomposed é scrubs to `caf` while "Cafe" followed by a combining accent scrubs to `cafe`: two phrases that look alike can be different worlds. A phrase in a non-Latin script keeps only any ASCII letters and digits it contains ("東京 2024" scrubs to `2024`) and is rejected only when none remain. The scrub is also available on its own as `scrub_phrase(bytes)`, which returns the scrubbed text with no emptiness or length check. `seed_from_phrase` has exactly two error values, empty and too long. The scrubbed phrase and the derived numeric seed are both recorded in `meta` (P0-02); the text as typed is not stored. A phrase is never interpreted as a number: the phrase `42` and the numeric seed 42 are different worlds.

## 8. Integer gradient noise

### 8.1 `noise2(uint64 key, Fx x, Fx y) → Fx in [-ONE, ONE]`

2D gradient (Perlin-style) noise on the integer lattice, evaluated entirely in Q32.32.

```
xi = x.raw >> 32;      yi = y.raw >> 32                 // floor, int64
fx = x.raw & 0xFFFFFFFF;  fy = y.raw & 0xFFFFFFFF       // fraction, raw in [0, ONE)
for (dx, dy) in (0,0), (1,0), (0,1), (1,1):
    u = fx - dx*ONE;  v = fy - dy*ONE                   // raw, exact, in (-ONE, ONE)
    g = hash_at(key, xi + dx, yi + dy) >> 61            // top 3 bits, 0..7
    d[dx][dy] = dot(g, u, v)                            // exact integer adds
su = fade(Fx::from_raw(fx));  sv = fade(Fx::from_raw(fy))
nx0 = lerp(d[0][0], d[1][0], su)
nx1 = lerp(d[0][1], d[1][1], su)
n   = lerp(nx0, nx1, sv)
return clamp(n, -ONE, ONE)
```

Gradient table (the improved-Perlin 2D set):

| g | gradient | dot(g, u, v) |
| --- | --- | --- |
| 0 | (1, 1) | u + v |
| 1 | (-1, 1) | -u + v |
| 2 | (1, -1) | u - v |
| 3 | (-1, -1) | -u - v |
| 4 | (1, 0) | u |
| 5 | (-1, 0) | -u |
| 6 | (0, 1) | v |
| 7 | (0, -1) | -v |

Properties: noise2 is exactly 0 at every integer lattice point. At a cell centre (xi + ½, yi + ½), su = sv = HALF exactly and every product is exact, so the result is exactly (d00 + d10 + d01 + d11) / 4, each d being one of {-1, -½, 0, ½, 1}. The mathematical range is [-1, 1] (reached at cell centres); the clamp guards against rounding.

Range of inputs: every `Fx` value of x and y is valid. xi + 1 is computed in int64 and cannot overflow, so `noise2` makes no argument check.

### 8.2 `fbm2(key, x, y, octaves, base_freq, lacunarity, gain) → Fx in [-ONE, ONE]`

```
sum = 0; norm = 0; amp = ONE; f = base_freq
for o in 0 .. octaves-1:
    ko = substream(key, o)
    n  = noise2(ko, mul(x, f), mul(y, f))
    sum  = sum + mul(amp, n)
    norm = norm + amp
    amp  = mul(amp, gain)
    f    = mul(f, lacunarity)
return clamp(div(sum, norm), -ONE, ONE)
```

Requirements (fatal otherwise): 1 ≤ octaves ≤ 16; base_freq > 0; lacunarity > 0; 0 < gain ≤ ONE. Each octave uses its own lattice hash (`substream(key, o)`), which removes the shared-origin artefact of octaves that share one lattice. Overflow of `mul(x, f)` is fatal, so a stage must choose frequencies that keep sample coordinates below 2^31. The loop is followed literally: `amp` and `f` are also updated after the last octave, so an overflow in that final update is fatal even though its result is unused.

Domain warping, ridged noise and other variants are composed from `noise2` by the P1 elevation spec.

## 9. Math functions

### 9.1 Provided in P0

| Function | Definition |
| --- | --- |
| `isqrt_u64(uint64 n)` | floor(√n), exact. Reference: bitwise digit-by-digit (restoring) square root. |
| `isqrt_u128(U128 n)` | floor(√n), exact, result < 2^64. Same method over 128 bits. |
| `sqrt(Fx x)` | x ≥ 0 (fatal otherwise). N = x.raw × 2^32 (U128); r = isqrt_u128(N); if N - r² > r then r = r + 1; result raw = r. This is round-to-nearest; ties cannot occur because (r + ½)² is never an integer. |
| `hypot(Fx a, Fx b)` | √(a² + b²) rounded to nearest, computed without intermediate rounding: N = a.raw² + b.raw² (U128, < 2^127); r = isqrt_u128(N); if N - r² > r then r = r + 1; fatal if r > 2^63 - 1; result raw = r. |
| `fade`, `lerp`, `clamp`, `min`, `max`, `abs`, `mul_div`, `floor/ceil/round/trunc` | sections 5.2–5.5 |

### 9.2 Deferred to P1

`sin`, `cos`, `atan2`, `exp`, `log` and fractional `pow` are **not** specified in P0. Reasons: no P0 code needs them; the precision and range each needs depends on how P1 formulates climate (latitude, wind direction) and erosion (evaporation, slope), and several of those can be formulated without transcendentals (temperature linear in latitude, winds on the 8-neighbour grid, evaporation as a repeated multiply); a function specified before its use is likely to be specified wrongly. When P1 needs one, its spec adds it to this file with: an exact integer algorithm (table plus fixed-point linear interpolation, or a polynomial with integer coefficients and a stated evaluation order); any table committed to the source as integer literals generated once by a script with arbitrary-precision arithmetic, with the script committed; and a test of the maximum error against a committed high-precision reference table. The function itself never calls `<cmath>`.

## 10. SHA-256

The core carries a SHA-256 implementation (FIPS 180-4) used by `worldgen-cli hash` (P0-04), the content hash of loaded data (P0-03) and logical table hashes. It is a project implementation in `worldgen/src/core/`, not a new dependency: about 150 lines, with standard test vectors, and its results can be checked against `sha256sum`. Interface: incremental (`update(bytes)`, `final() → 32 bytes`) plus a one-shot helper. Hex output is lowercase.

## 11. Invariants

- Every function in this spec is a pure function of its arguments (and, for `Rng`, its state). No global state.
- `mul(a, b) == mul(b, a)`; `mul(-a, b) == -mul(a, b)`; `mul(a, ONE) == a`; `div(a, ONE) == a`; `div(mul_int(a, n), from_int(n)) == a` when no overflow.
- `uniform_u64(s) < s`; `uniform_i64(lo, hi)` ∈ [lo, hi]; `uniform_fx(lo, hi)` ∈ [lo, hi); `uniform_fx01()` ∈ [0, ONE).
- `weighted_index` never returns an index with weight 0.
- `shuffle` produces a permutation of its input.
- `noise2` ∈ [-ONE, ONE]; `noise2` = 0 at integer lattice points; `fbm2` ∈ [-ONE, ONE].
- `sqrt` rounds to nearest: with r = sqrt(x).raw and N = x.raw·2^32, \|r² - N\| ≤ r.
- No result depends on thread count, call order across different keys, or platform.

## 12. Determinism notes

| Risk | How it is avoided |
| --- | --- |
| No `__int128` on MSVC | `U128` and portable reference functions; any fast path is limited to two functions and tested against the reference. |
| Signed overflow is UB and optimizers exploit it differently | Checked operations; fatal error on overflow; UBSan in CI (requested from the build engineer, see report). |
| Rounding differences | One rounding rule (RHA), stated for every operation; no floating point. |
| Unspecified argument evaluation order | Rule in section 4: one draw per statement. |
| Unstable sorts | Rule in section 4. |
| `long`/`char` width and signedness | Fixed-width types only. |
| Endianness | Arithmetic little-endian serialization. |
| Standard-library RNG and distributions differ between libraries | Project `Rng` and sampling only. |

## 13. Tests required

The test writer implements these from this spec alone. "tbc" marks a vector that must be confirmed by the validation specialist before it becomes a golden value (see section 13.8).

### 13.1 Unit tests: `Fx`

Raw values in decimal unless prefixed 0x.

| Case | Expected |
| --- | --- |
| `from_int(1).raw()` | 4294967296 |
| `from_int(-3).raw()` | -12884901888 |
| `from_int(2147483647)` | ok; `from_int(2147483648)` and `from_int(-2147483648)` fatal |
| `from_raw(INT64_MIN)` | fatal |
| `mul(1.5, 2.5)`: raws 6442450944 × 10737418240 | 16106127360 (3.75) |
| `mul(raw 1, raw 1)` | 0 |
| `mul(raw 1, HALF)` (exact 0.5 raw) | 1 (half away from zero) |
| `mul(raw -1, HALF)` | -1 |
| `mul(raw 3, HALF)` / `mul(raw -3, HALF)` | 2 / -2 |
| `mul(from_int(65536), from_int(32767))` | from_int(2147418112) |
| `mul(from_int(65536), from_int(32768))` | fatal (2^63) |
| `div(ONE, from_int(3))` | 1431655765 |
| `div(from_int(2), from_int(3))` / `div(from_int(-2), from_int(3))` | 2863311531 / -2863311531 |
| `div(raw 1, from_int(2))` | 1 |
| `div(ONE, raw 1)` | fatal (2^64) |
| `div(x, 0)` | fatal |
| `mul_div(7, 3, 2)` / `mul_div(-7, 3, 2)` | 11 / -11 (10.5 rounds away) |
| `from_ratio(1, 3)` | 1431655765 |
| `floor_int`, `ceil_int`, `round_int`, `trunc_int` of raw -6442450944 (-1.5) | -2, -1, -2, -1 |
| same of raw 6442450944 (1.5) | 1, 2, 2, 1 |
| `fade(0)`, `fade(HALF)`, `fade(ONE)` | 0, 2147483648, 4294967296 |
| `fade(raw -1)`, `fade(ONE + 1)` | fatal |
| `lerp(from_int(2), from_int(4), HALF)` | from_int(3) |
| `a + b` with a = MAX, b = raw 1 | fatal; `sat_add` gives MAX |

### 13.2 Unit tests: decimal parse and format

| Input | Expected raw |
| --- | --- |
| "0" / "-0" / "+0.000" | 0 |
| "1" | 4294967296 |
| "0.5" | 2147483648 |
| "-1.25" | -5368709120 |
| "0.1" / "-0.1" | 429496730 / -429496730 |
| "0.35" | 1503238554 |
| "2147483647.9999999" | 9223372036854775379 |
| "2147483647.99999999999999999" | error (rounds to 2^63) |
| "0.000000000000000001" (18 digits) | 0 |
| "0.0000000000000000001" (19 digits) | error |
| "12345678901" (11 integer digits) | error |
| ".5", "5.", "05", "1e3", "1_000", " 1", "1 ", "", "-", "0x10", "1.2.3" | error |

Canonical text: "+0.500" → "0.5"; "-0" → "0"; "3.000" → "3"; "10" → "10"; "-2.50" → "-2.5".

Format: `to_decimal(HALF, 3)` = "0.500"; `to_decimal(raw -5368709120, 1)` = "-1.3"; `to_decimal(raw 1, 9)` = "0.000000000"; `to_decimal(raw -1, 2)` = "0.00"; `to_decimal(from_int(-7), 0)` = "-7".

Property: for 10^5 raw values from a fixed stream (plus MIN, MAX, ±1, 0), `parse_decimal(to_decimal(x, 18)) == x` exactly. Reason: rounding to 18 decimal places moves the value by at most 5·10^-19, which is less than 2.2·10^-9 raw units, so rounding back to Q32.32 recovers x.

### 13.3 Unit tests: wide arithmetic and square roots

- `mul_u64_wide(2^64-1, 2^64-1)` = hi 0xFFFFFFFFFFFFFFFE, lo 1.
- `mul_u64_wide(0x100000000, 0x100000000)` = hi 1, lo 0.
- `divmod_u128_u64(hi 1, lo 0; 3)` = quotient 6148914691236517205 (0x5555555555555555), remainder 1.
- If a fast path exists: reference path equals fast path on the vectors above and on 10^6 pairs from `Rng(stream_key(0, "test.wide"))` (a test-only stream name, listed in the registry).
- `isqrt_u64`: 0→0, 1→1, 2→1, 3→1, 4→2, 15→3, 16→4, 2^64-1 → 4294967295.
- `sqrt(Fx)`: sqrt(0) = 0; sqrt(raw 1) = 65536; sqrt(from_ratio(1,4)) = 2147483648; sqrt(from_int(4)) = 8589934592; sqrt(from_int(2)) = 6074001000; sqrt(from_int(3)) = 7439101574; sqrt(from_int(5)) = 9603838835; sqrt(raw -1) fatal. (The √2, √3, √5 values follow from the SHA-256 initial hash words, which are the first 32 fractional bits of √2, √3, √5, rounded with the next hex digit; checked by hand, still listed for confirmation.)
- `hypot(from_int(3), from_int(4))` = from_int(5).
- Property: for 10^5 random x > 0, r = sqrt(x).raw satisfies (r - ½)² ≤ x.raw·2^32 < (r + ½)², checked in U128 as (2r-1)² ≤ 4N < (2r+1)². (x = 0 is excluded: sqrt(0) = 0 and the lower bound does not hold for r = 0.)

### 13.4 Unit tests: `Rng` and sampling

- SplitMix64 from x = 0, first five outputs: 0xE220A8397B1DCDAF, 0x6E789E6AA1B965F4, 0x06C45D188009454F, 0xF88BB8A8724C81EC, 0x1B39896A51A8749B (published reference values; tbc).
- SplitMix64 from x = 1234567, first five outputs (decimal): 6457827717110365317, 3203168211198807973, 9817491932198370423, 4593380528125082431, 16408922859458223821 (published reference values; tbc).
- `Rng::from_state(1, 2, 3, 4)`: next() = 11520, then 0, then 1509978240 (derived by hand; tbc). State after the first call: (7, 0, 262146, 211106232532992).
- `Rng(0)`: state = the first four SplitMix64(0) outputs above; first next() = 0x99EC5F36CB75F2B4 (derived by hand from the SplitMix64 values; tbc).
- `from_state(0,0,0,0)` fatal.
- From `from_state(1,2,3,4)`: `uniform_u64(10)` = 0 using one draw; then `uniform_u64(3)` = 0 using two draws (the draw 0 is rejected because t = 1). Check draw counts by comparing the next `next()` against a second generator advanced the expected number of times.
- From `from_state(1,2,3,4)`: `uniform_u64(2^63)` = 5760.
- From `from_state(1,2,3,4)`: `shuffle([a, b, c])` = [b, c, a].
- From `from_state(1,2,3,4)`: `weighted_index([0, 5, 3])` = 1.
- `uniform_u64(1)` = 0 and consumes one draw. `uniform_u64(0)` fatal. `uniform_i64(INT64_MIN, INT64_MAX)` consumes exactly one draw and equals int64(next()) of a twin generator.
- `chance(0)` false and `chance(ONE)` true for 10^4 draws each; each consumes one draw.
- `weighted_index([])`, `weighted_index([0, 0])`, weights summing past 2^64 - 1: fatal.
- Compile-time: `std::is_copy_constructible_v<Rng>` is false.
- Move: after `Rng b = std::move(a)`, `b` continues the stream `a` had (same outputs as a twin), `a.next()` is fatal, and after `a = Rng(key)` it works again. The same holds for move assignment.

Property tests (fixed streams, so they are deterministic):
- `uniform_u64(s)` for s in {1, 2, 3, 7, 10, 1000, 2^32+1, 2^63+1, 2^64-1}: every result < s.
- Chi-squared over 10^6 draws of `uniform_u64(10)` and `uniform_u64(7)`: statistic below the 0.999 critical value. Because the stream is fixed the result is deterministic; the test guards against a broken implementation, not chance.
- `shuffle` of 0..n-1 is a permutation for n in 0..64.
- `weighted_index` frequencies over 10^6 draws with weights [1, 2, 3, 0, 4] are within 1% absolute of the expected shares, and index 3 never occurs.

### 13.5 Unit tests: hashing and streams

- `mix64(0)` = 0. `absorb(0, 0)` = 0xE220A8397B1DCDAF (equals SplitMix64(0) first output; tbc with that vector). `absorb(0, GOLDEN)` = 0x6E789E6AA1B965F4 (second SplitMix64(0) output; tbc).
- Byte packing: "test.noise" → words [10, 0x696F6E2E74736574, 0x0000000000006573]. "The Unmapped" → [12, 0x616D6E5520656854, 0x0000000064657070]. "" → [0].
- `stream_key(seed, name)` equals `hash_words(TAG_STREAM, [seed, len(name), packed name words…])` computed with the primitives (structural test). `stream_key` with a name outside the grammar is fatal.
- `stream_key(0, "test.noise")`, `stream_key(0xFFFFFFFFFFFFFFFF, "geo.elevation")`, `substream(1, 0)`, `hash_at(1, -1, 2)`, `hash_at3(1, 0, 0, 0)`, `phrase_seed("The Unmapped")`: values to be generated by the validation specialist with an independent reference implementation (see 13.6) and recorded as golden vectors.
- `seed_from_phrase` scrubs: "Ashen Marches!", "ashen-marches", "ASHEN  MARCHES", "  ashen\tmarches\n" and "ashen marches" all return the phrase `ashen marches` and the same seed, equal to `phrase_seed("ashen marches")`; "The Unmapped" returns `the unmapped`; "Ünmapped Wörld" returns `nmapped w rld`; "a1B2" returns `a1b2`; bytes 0x00, 0x7F, 0xFF and an invalid UTF-8 sequence inside a phrase each act as a space. Errors: the empty string, all-whitespace, all-punctuation, a phrase of only non-ASCII bytes, and a phrase whose scrubbed form is 257 bytes; a scrubbed form of exactly 256 bytes is accepted, and so is a much longer input that scrubs to 256 bytes or fewer. Scrubbing a scrubbed phrase returns it unchanged (property test over random byte strings). `phrase_seed` itself hashes exactly the bytes it is given, so `phrase_seed(" x")` differs from `phrase_seed("x")` and `phrase_seed("X")` differs from `phrase_seed("x")`.
- Sensitivity: over 10^4 keys, `hash_at(k, x, y)` for (x, y) and (y, x) with x ≠ y never coincide; `stream_key(s, "a")` ≠ `stream_key(s, "a.b")`.
- Registry: all stream names match the grammar and are unique.

### 13.6 Unit tests: noise

- `noise2(k, from_int(i), from_int(j))` = 0 for 1000 integer points including negatives, for 10 keys.
- Centre value: for 1000 cells (i, j) and 10 keys, `noise2(k, i+½, j+½)` equals (d00 + d10 + d01 + d11)/4 where each d is computed by the test from `hash_at(k, i+dx, j+dy) >> 61` and the gradient table with u, v = ±½.
- Range: over a 512 × 512 grid at offsets of 1/7 cell and 10 keys, every `noise2` and `fbm2` value lies in [-ONE, ONE].
- Continuity: for adjacent samples 2^-12 apart, \|Δnoise\| ≤ 2^-9 (a loose bound that catches lattice or fade errors).
- `fbm2` with octaves = 1 and base_freq = ONE equals `noise2(substream(k, 0), x, y)`.
- Argument checks: octaves 0 or 17, gain 0, base_freq 0 are fatal.

### 13.7 Unit tests: SHA-256

FIPS 180-4 vectors: "" → e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855; "abc" → ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad; "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" → 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1; one million "a" → cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0. Incremental updates split at every offset 0..64 of a 130-byte message give the same digest as one-shot.

### 13.8 Golden vectors and cross-platform check

- The validation specialist computes the tbc vectors above, plus a fixed list of hash, stream and noise outputs (at least: the six hash values in 13.5, the first 8 outputs of `Rng(stream_key(0, "test.noise"))`, `noise2` and `fbm2` at 16 fixed points), with an **independent reference implementation** written from this spec (a short Python script with arbitrary-precision integers is recommended) and commits them under `worldgen/tests/golden/core_vectors.*`. The C++ tests compare against that file on every preset, which turns each vector into a cross-platform check.
- **The golden file exists:** `worldgen/tests/golden/core_vectors.txt`, generated by `worldgen/tests/reference/core_reference.py` (see the README beside it; the file's header documents every vector kind). A C++ test parses the file and checks every line against the implementation; a kind the test does not recognise is a test failure, so no vector is skipped silently. Kind `hash.phrase_seed` maps to `phrase_seed` and `hash.phrase_text` to `seed_from_phrase` (its result is the seed; a kind `hash.phrase_scrub` gives the scrubbed text).
- A golden-seed test for the core is the P0-04 test-pattern world, whose layers exercise hashing and noise end to end.

## 14. Open questions

1. **Fixed-point format.** Q32.32 is chosen for range and precision; cost is 128-bit intermediates. If P1 profiling shows multiply is a bottleneck, the fast path in 5.3 is the remedy, not a narrower format. Approve Q32.32.
2. **Rounding rule.** Round half away from zero everywhere (symmetric). The alternative, add-half-then-shift (round half up), is one instruction cheaper but not odd-symmetric. Approve RHA.
3. **Overflow policy.** Fatal error, never saturate silently. Approve.
4. **RNG.** xoshiro256** seeded by SplitMix64. Approve.
5. **Phrase normalization.** Decided in the design doc (section 4, 2026-11-15): phrases are scrubbed as in 7.3. This changed the seed of every phrase containing a capital letter or punctuation, so the generator version was bumped to 0.2.0.
6. **Noise.** 2D gradient noise with 8 gradients and quintic fade. Simplex or value noise were the alternatives. P1 may need 3D noise (for example time-varying fields); it would be added here.

## 15. Amendment for phase P1 (2026-11-15)

P1-00 appendix A1 is adopted as part of this spec: the constants `SQRT2` and `INV_SQRT2`, the integer helper `bit_length(uint64)` with its tests, the two additions to the parallel-work rule 4.2 (the thread count is supplied by the caller and never affects a result; work may be split into parts defined by the data), and the note that ridged noise and domain warping are composed from `noise2` and `fbm2` in P1-01 section 4.1. The first-pass geography stages need none of the functions deferred in 9.2.
