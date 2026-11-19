# P3-02 Names

Status: **draft, spec writer, 2026-11-19, awaiting approval.** Phase P3 History core, spec 2 (P3-00 3.1). Design doc sections 5 ("the seed varies the dates, the places and the gods' names"; civilizations, settlements), 6 ("the database stores facts and names"), 14 (every reference a model writes must resolve to a real entity: names must be unique). Depends on P0-01 (hashing, streams), P0-03 (string grammars, named checks), P3-00 (4.6 keyed draws) and P3-01 (the `culture` kind carries this spec's record). It introduces the domain tag `TAG_HIST` and the keyed-draw helpers that every later history spec uses.

## 1. Purpose

A deterministic name generator: given a culture's naming data, a name kind (god, civilization, settlement), the entity's id and the world seed, produce a pronounceable name in that culture's sound, unique in the world. Names are placeholders of the generator, stored in the history tables (P3-03, P3-04); the enrichment pass of design 14 may tell stories about them but never renames them in the world file.

## 2. What this spec touches (for the engineer)

| Area | Files |
| --- | --- |
| Domain tag | `worldgen/include/worldgen/core/hash.hpp` (or wherever the P0-01 tags live): `TAG_HIST`; P0-01's tag table gains a line (P0-01 amendment, section 8) |
| Keyed draws | new `worldgen/include/worldgen/history/draw.hpp`, `worldgen/src/history/draw.cpp` |
| Names | new `worldgen/include/worldgen/history/names.hpp`, `worldgen/src/history/names.cpp`; the record reader `culture_naming_from(const data::Record&)` |
| Streams | `worldgen/include/worldgen/core/streams.hpp`: `hist.names.god`, `hist.names.civilization`, `hist.names.settlement` |
| Build | `worldgen/CMakeLists.txt` (the new sources; the first files of `src/history/`) |
| Tests | `worldgen/tests/history/` (new), reference `worldgen/tests/reference/p3_names_reference.py`, golden `worldgen/tests/golden/p3-02-names.txt`; a CTest group `worldgen-tests-history` (CLAUDE.md "Adding code") |

The `culture_naming` record **type** (field list and grammars of section 3) is registered with the `culture` kind by P3-01, and the shipped `naming` blocks (section 5) are committed in P3-01's culture files: the definitions live here, the files there. **Merge order: P3-01, then P3-02.** P3-02 changes no world byte (no stage calls it before P3-03), so it needs no version bump.

**Which spec introduces `TAG_HIST`** (P3-00 4.6 left it open): **P3-02**, because it merges before P3-03 and P3-04 and is the first user of keyed draws. P3-03 and P3-04 use `draw`, `pick` and the other helpers of section 4 as defined here.

## 3. The naming data: record `culture_naming`

Fields, all required, in this declaration order (canonical encoding, P0-03 8):

| Field | Type | Meaning |
| --- | --- | --- |
| `onsets` | array(string(8, syllable), 1, 64) | consonant starts of a syllable; `""` allowed (no onset) |
| `nuclei` | array(string(8, syllable), 1, 64) | vowel cores; `""` not allowed (check) |
| `codas` | array(string(8, syllable), 1, 64) | syllable ends; `""` allowed |
| `patterns` | array(string(16, name_pattern), 1, 16) | shapes of a root word, picked uniformly; repeat an entry to weight it |
| `min_letters`, `max_letters` | int(1, 32) | bounds on the letters of a root (separators not counted) |
| `forbidden` | array(string(8, syllable), 0, 32) | substrings a root must not contain |
| `god_forms`, `civilization_forms`, `settlement_forms` | array(string(48, name_form), 1, 8) | templates for each kind, picked uniformly; repeat an entry to weight it |

String grammars (kind-local `StringGrammar`s, defined here and registered by P3-01 with the record; descriptions as printed in error messages):

| Grammar | Rule | Description |
| --- | --- | --- |
| `syllable` | zero or more ASCII lower-case letters `a`–`z` | `a syllable (lower-case letters a-z, possibly empty)` |
| `name_pattern` | one or more of the symbols `O`, `N`, `C`, `'`, `-`; at least one `N`; not starting or ending with `'` or `-`; no two of `'`, `-` adjacent | `a name pattern (O, N, C, ' and -, with at least one N)` |
| `name_form` | ASCII letters, space, `'` and `-`, plus exactly one `{root}`; no leading or trailing space, no two spaces in a row | `a name form (letters, spaces, ' and -, with exactly one {root})` |

Named checks (per definition of kind `culture`, reported `… failed for 'culture.<id>': <message>`), in this order:

| Check | Condition | Message |
| --- | --- | --- |
| `naming_nuclei_nonempty` | no nucleus is `""` | `naming.nuclei[<i>] must not be empty` |
| `naming_letters_ordered` | `min_letters` ≤ `max_letters` | `naming.min_letters must not be above naming.max_letters` |
| `naming_forbidden_nonempty` | no forbidden entry is `""` | `naming.forbidden[<i>] must not be empty` |

**Character set: ASCII only.** Names consist of ASCII letters, spaces, apostrophes and hyphens, and from the fallback of 4.4 Roman numerals. So the report, `hash` and every reader handle names without escaping or Unicode normalization, and the cross-platform text contract of `report --history` (P3-00 11) holds trivially. Diacritics and other scripts are not supported (section 10, question 2).

## 4. Algorithm

### 4.1 Keyed draws (introduced here, for every history spec)

```
TAG_HIST                     = 0x545349483A4D4E55          // "UNM:HIST", P0-01 7.2's packing
draw(name, w0, a, b, k)      = hash_words(TAG_HIST, [stream_key(seed, name), w0, a, b, k])     // uint64
pick(u, n)                   = ((u >> 32) × n) >> 32       // 1 ≤ n ≤ 2^32; an index in [0, n)
chance(u, p)                 = (u >> 32) < p.raw           // Fx p in [0, ONE]: true with probability p
weighted(u, w[0..m-1])       = the smallest i with (u >> 32) × W < 2^32 × (w[0] + … + w[i]), W = Σ w   // w ≥ 0, W ≥ 1, in uint128
```

`w0` is the year in the simulation's draws (P3-00 4.6) and the attempt number in this spec's; a and b are entity ids or other integers (0 if unused); k counts the draws of one decision. All words are `uint64` (signed values as two's complement). `stream_key(seed, name)` may be computed once per stream and kept. Seams: `history::draw(key, w0, a, b, k)` taking the stream key, `history::pick`, `history::chance`, `history::weighted`.

### 4.2 One attempt

Input: the culture's naming data D, the name kind K ∈ {god, civilization, settlement} with its forms F = D.`<K>_forms`, the culture number c (its `culture.id` in the world, 1 … in canonical id order, P3-03), the entity id e, the attempt number t, and the stream key S_K = stream_key(seed, `hist.names.<K>`). With u(j) = draw(S_K, t, c, e, j):

```
form    = F[pick(u(0), |F|)]
pattern = D.patterns[pick(u(1), |D.patterns|)]
root = ""; j = 2
for each symbol s of pattern, left to right:
    s = O: root += D.onsets[pick(u(j), |D.onsets|)];  j += 1
    s = N: root += D.nuclei[pick(u(j), |D.nuclei|)];  j += 1
    s = C: root += D.codas [pick(u(j), |D.codas|)];   j += 1
    s = ' or -: root += s                                 // no draw
valid = D.min_letters ≤ (letters a–z in root) ≤ D.max_letters
        and root contains no D.forbidden entry as a substring
        and root does not start or end with ' or -, and has no two of ' and - adjacent
name  = form with "{root}" replaced by Cap(root)
valid = valid and |name| ≤ 48 bytes
```

Cap(root) upper-cases the first character of the root if it is a letter, and every letter that directly follows a `-`; a letter after `'` stays lower case (`Uruld'duuld`, `Gnink-Snunk`). (An empty onset or coda can put two separators side by side or a separator at an end; such an attempt is invalid.)

### 4.3 A name

```
name(K, c, e, registry):
    first = none
    for t = 0, 1, …, 63:
        (valid, n) = attempt(t)
        if not valid: continue
        if first is none: first = n
        if n is not in registry: add n to registry; return n
    if first is none: fatal "names: culture '<id>' produced no valid <K> name in 64 attempts"
    k = 2; while first + " " + roman(k) is in registry: k += 1
    n = first + " " + roman(k); add n to registry; return n
```

`roman(k)` is P1-06 RG11's (subtractive form, thousands as repeated M). The registry is a set of names compared as exact bytes.

### 4.4 Uniqueness, bounds and order

- **The registry is per world** and holds every name the world has: the caller (P3-03) fills it first with the region names of table `region`, in id order, and then adds every name it generates. Gods, civilizations and settlements share it, so no two named entities of a world share a name, whatever their kind. Regions keep their placeholder names (P1-06), which have a different form (`East Wood II`), so collisions with them are rare but are still avoided.
- **Bounds**: at most 64 attempts per name; a name is at most 48 bytes, or 48 + 1 + the numeral after the fallback (a numeral of a world with fewer than 4000 names is at most 15 bytes; 64 bytes is the hard limit, fatal above `names: a name longer than 64 bytes`, which cannot happen with fewer than 2^16 names).
- **Order**: a name depends on the registry at the moment it is drawn, so callers name entities in their creation order (H2): the result is defined because creation order is. A name does not depend on how many draws other decisions made (H4): its draws are keyed by (kind, attempt, culture, entity).
- **Cost**: at most 64 × (2 + 16) draws per name; worlds have a few hundred named entities.

### 4.5 Name kinds and streams

| Kind | Stream | Entity id e | Used by |
| --- | --- | --- | --- |
| `god` | `hist.names.god` | `god.id` | P3-03 |
| `civilization` | `hist.names.civilization` | `civilization.id` | P3-04, P3-05 |
| `settlement` | `hist.names.settlement` | `settlement.id` | P3-05 |

Reserved for later specs (not registered now): `person` (P5 figures), `faction` (P5), `region` (cultural region names, P5 or P6), `site` (P6). A later kind adds its forms field to `culture_naming` (a data change) and its stream.

### 4.6 Worked example

Seed 0, kind `settlement`, culture `elven` (c = 2: canonical culture order is dwarven 1, elven 2, faerie 3, goblin 4, human 5, orcish 6), entity e = 1, empty registry. S = stream_key(0, "hist.names.settlement") = 0x969C418DDF3C5EDC.

| j | u(j) (t = 0) | pick | Result |
| --- | --- | --- | --- |
| 0 | 0xAB9A949FA699954C | 2 of 4 | form `{root}` |
| 1 | 0xF77E9ED830C1A5E0 | 4 of 5 | pattern `NCONC` |
| 2 | 0xAF071A40E08E8E51 | 4 of 6 | N `ie` |
| 3 | 0x5EAF3E5C2245C62D | 3 of 10 | C `n` |
| 4 | 0x42F8D74C06F27F04 | 3 of 13 | O `s` |
| 5 | 0x109285B9E64CA453 | 0 of 6 | N `a` |
| 6 | 0x7347EA42EAED2BB2 | 4 of 10 | C `th` |

Root `iensath` (7 letters, within 4–11, no forbidden substring), name **`Iensath`**. If the registry already holds `Iensath`, attempt 1 gives **`Focoth`**. If it holds every valid name of the 64 attempts (61 distinct ones), the result is **`Iensath II`**.

Further vectors (seed 42, one registry, in this order): god, dwarven (c 1), e 1 → `Khork` (attempt 0); civilization, goblin (c 4), e 3 → `The Grork Horde` (attempt 1, attempt 0 being invalid); settlement, human (c 5), e 17 → `Deantwick`; settlement, orcish (c 6), e 200 → `Ol'uraakh Fort`.

## 5. Shipped naming data

The six `naming` blocks, committed by P3-01 in `game/data/cultures/<id>.toml` (and the fixture copies) after the `race` and `name` keys of the culture (P3-01 4). Design intent: elven flowing and vowel-rich, goblin short, harsh and hyphenated, dwarven heavy consonant clusters with `Hold` and `Deep`, faerie light and diminutive with glades and hollows, human English-sounding with English place suffixes, orcish guttural with apostrophes.

```toml
[culture.dwarven.naming]
onsets = ["d", "b", "th", "k", "g", "br", "dr", "gr", "t", "h", "kh", "n", "m", "thr"]
nuclei = ["a", "u", "o", "i", "ai", "ou"]
codas = ["r", "n", "m", "k", "d", "rn", "rd", "rk", "mm", "nd", "rin", "li", "din"]
patterns = ["ONC", "ONCONC", "ONCONC", "ONCON"]
min_letters = 4
max_letters = 10
forbidden = ["mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk"]
god_forms = ["{root}"]
civilization_forms = ["The Hold of {root}", "Clan {root}", "The {root} Throne"]
settlement_forms = ["{root}", "{root} Deep", "{root} Hold", "{root}"]
```

```toml
[culture.elven.naming]
onsets = ["", "l", "th", "s", "f", "n", "m", "r", "v", "gl", "c", "el", "ith"]
nuclei = ["a", "e", "i", "ae", "ie", "o"]
codas = ["", "", "l", "n", "th", "s", "r", "nd", "ll", "nn"]
patterns = ["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"]
min_letters = 4
max_letters = 11
forbidden = ["aea", "iei", "iae", "eae", "aa", "ii", "ee", "thth", "lll", "nnn", "ea", "ia"]
god_forms = ["{root}"]
civilization_forms = ["The Court of {root}", "The Realm of {root}", "{root}"]
settlement_forms = ["{root}", "{root}", "{root}", "{root}dell"]
```

```toml
[culture.faerie.naming]
onsets = ["", "p", "f", "w", "l", "s", "t", "b", "sh", "wh", "fl", "tw", "m"]
nuclei = ["i", "ee", "ie", "ia", "a", "y", "ae", "ou", "o"]
codas = ["", "", "ll", "n", "sh", "th", "p", "sk", "ss"]
patterns = ["ONON", "ONONON", "ONC", "ONONC", "ONCON"]
min_letters = 3
max_letters = 10
forbidden = ["eee", "iee", "yy", "ouou", "shsh"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Ring", "The Circle of {root}", "The {root} Court"]
settlement_forms = ["{root}", "{root} Glade", "{root} Hollow", "{root}"]
```

```toml
[culture.goblin.naming]
onsets = ["g", "gr", "sn", "k", "kr", "z", "sk", "b", "n", "gn", "dr", "sl", "skr"]
nuclei = ["a", "i", "u", "o", "ee"]
codas = ["k", "g", "b", "z", "t", "x", "nk", "rk", "gg", "zz", "tch"]
patterns = ["ONC", "ONC", "ONCONC", "ONC-ONC", "ONONC"]
min_letters = 3
max_letters = 10
forbidden = ["eee", "ggg", "zzz", "xg", "xk"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Horde", "The Warrens of {root}", "The {root} Mob"]
settlement_forms = ["{root}", "{root}", "{root} Pit", "{root} Hole"]
```

```toml
[culture.human.naming]
onsets = ["b", "c", "d", "g", "h", "m", "r", "s", "t", "w", "st", "br", "al", "ed", "ash"]
nuclei = ["a", "e", "i", "o", "u", "ea"]
codas = ["n", "d", "r", "s", "l", "rd", "ck", "ld", "m", "nt"]
patterns = ["ONC", "ONC", "ONCONC", "ONC"]
min_letters = 4
max_letters = 9
forbidden = ["eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm"]
god_forms = ["{root}"]
civilization_forms = ["The Kingdom of {root}", "The {root} League", "The Duchy of {root}"]
settlement_forms = ["{root}", "{root}ford", "{root}ton", "{root}bury", "{root}wick", "{root}ham", "{root}stead"]
```

```toml
[culture.orcish.naming]
onsets = ["gh", "gr", "ug", "m", "k", "z", "sh", "b", "d", "th", "kr", "ur", "az"]
nuclei = ["a", "u", "o", "aa", "uu"]
codas = ["g", "sh", "k", "rz", "z", "gh", "rk", "l", "m", "", "kh", "ld"]
patterns = ["ONC", "ONC", "ONC'ONC", "ONCONC", "NC'ONC"]
min_letters = 3
max_letters = 10
forbidden = ["aaa", "uuu", "ghgh", "shsh"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Horde", "Clan {root}", "The Warband of {root}"]
settlement_forms = ["{root}", "{root} Camp", "{root}", "{root} Fort"]
```

### 5.1 Sample names

Computed by the spec writer's reference (`~/.cache/the-unmapped/out/p3-02-spec/names_ref.py`, data `cultures.py`): seed 0, one registry per culture, settlements for entities 1–20, then civilizations 1–5, then gods 1–3. For the lead and Paul to judge the sound; these are first values.

| Culture (c) | Settlements 1–20 | Civilizations 1–5 | Gods 1–3 |
| --- | --- | --- | --- |
| `dwarven` (1) | Moumka, Thoumdaili, Makmoudin Deep, Brummthu, Gromka, Kaird Deep, Khaili Hold, Kili, Thoum Hold, Nodin, Gondhark, Turnbruk Deep, Kidinkorn, Tharndramm Hold, Grangrak, Houmm Deep, Doknirk, Gali, Nailimim Deep, Tairdbra Hold | The Kourdbrar Throne; The Hold of Didin; Clan Durgarin; Clan Drourd; Clan Drailimai | Brommgrai, Groumtoun, Nurnthaimm |
| `elven` (2) | Iensath, Onaeme, Ierrael, Elonoma, Fievos, Mielae, Asvathdell, Lafin, Laevo, Leithiedell, Orith, Voglael, Mievife, Iefann, Cellve, Fimo, Londcae, Rienel, Nathol, Siendnie | Nife; The Court of Nothind; Aesoth; Riefifie; The Court of Thiesaell | Saene, Fiendse, Ithonnfa |
| `faerie` (3) | Liapa, Wiatwiesk Glade, Twamae Glade, Lip, Biafloss Glade, Pieshaep Hollow, Teelan, Meeie Hollow, Sywysh Hollow, Iatysk, Wiepysk, Miemo Hollow, Mapoufee, Feetop Hollow, Showhie Hollow, Mawhiap Hollow, Athfli Hollow, Whywo, Fliaposk Hollow, Whosy Hollow | The Lyp Ring; The Watiafia Court; The Bouss Ring; The Wosk Ring; The Twiwhiwou Ring | Fiwhall, Fiabo, Biensho |
| `goblin` (4) | Krot Pit, Slograb Pit, Gnink-Snunk, Zux, Grunkgnuzz Pit, Grizz-Skeeb, Slank-Skrox, Drobskeet, Bag Hole, Sluggatch Hole, Bux-Kreeb, Sniz, Slak, Zug-Gok, Gunitch, Slagreeg Hole, Droggkazz Hole, Slobeerk, Snotgab, Kruskreezz Hole | The Gnazz Horde; The Warrens of Bonk-Gitch; The Grodratch Horde; The Krunkgirk Horde; The Warrens of Gnonk | Zabskreeb, Kruskux, Bazz |
| `human` (5) | Deanham, Alantton, Cickham, Gackwick, Brir, Tackbury, Gurdton, Studford, Ceambury, Buld, Silstudstead, Tirdwick, Dack, Stastint, Moldstead, Edumford, Tontstead, Ceanbir, Durdton, Durdbury | The Duchy of Ashem; The Kingdom of Ashos; The Duchy of Hick; The Duchy of Wick; The Duchy of Circon | Simbear, Histold, Edostold |
| `orcish` (6) | Kaak Fort, Zaarz Camp, Grush, Zaakugaag, Uruld'duuld, Uu'mold Fort, Kark Fort, Azol'urash Fort, Karzthogh, Uromgha, Maag, Uul'krakh Camp, Og'buurk Camp, Zaalduruz Camp, Mak, Duld'sha Fort, Shuum Fort, Ghorz Fort, Duuldkrosh Fort, Aag'urog Fort | Clan Kram'krorz; The Azakhdam Horde; The Urogh'krorz Horde; The Burk'zork Horde; The Warband of Maakurokh | Ugh'kaal, Og'uraag, Uruuk |

Weak spots visible in the samples, left for tuning with Paul's taste: some human roots are clipped (`Brir`, `Buld`, `Stastint`); elven names lean on `ie`; faerie settlement forms use `Hollow` often (two of four forms carry a suffix). Each is a data change in one block.

## 6. Invariants

1. Every generated name matches: ASCII letters, spaces, `'`, `-`; at most 64 bytes; the form's fixed text plus a root whose letter count is within the culture's bounds, optionally followed by a space and a Roman numeral ≥ II.
2. No name is generated twice into one registry.
3. The name of (seed, kind, culture, entity) with a given registry content is a pure function; it does not depend on any other draw, on thread count or on the order in which names of other cultures were generated, except through the registry content.
4. With an empty registry, the result is the first valid attempt.

## 7. Determinism notes

Integer arithmetic only (`uint64` hashes, a `uint128` product in `weighted`); string operations on ASCII bytes; the registry is an ordered set or a hash set used only for membership (never iterated, so H3 holds); no locale-dependent case mapping (upper-casing is `a`–`z` to `A`–`Z` by arithmetic).

## 8. Amendments requested

- **P0-01**: the tag table gains `TAG_HIST`, `UNM:HIST`, 0x545349483A4D4E55; the stream-name list gains the three `hist.names.*` names (P0-01 7.3; names registered in `streams.hpp`).
- **P3-00 4.6**: done on this branch: P3-02 introduces `TAG_HIST` and the helpers.

## 9. Tests required

For the independent test writer; seams (interim; the engineer records the final signatures after implementation):

```cpp
namespace worldgen::history {
std::uint64_t draw(std::uint64_t stream_key, std::uint64_t w0, std::uint64_t a, std::uint64_t b, std::uint64_t k);
std::uint64_t pick(std::uint64_t u, std::uint64_t n);                 // fatal if n = 0 or n > 2^32
bool chance(std::uint64_t u, Fx p);                                    // fatal if p outside [0, ONE]
std::size_t weighted(std::uint64_t u, std::span<const std::int64_t> w); // fatal if a weight < 0 or the sum is 0
struct CultureNaming { /* the fields of section 3 */ };
CultureNaming culture_naming_from(const data::Record& naming);
enum class NameKind { god, civilization, settlement };
struct NameAttempt { bool valid; std::string name; };
NameAttempt name_attempt(const CultureNaming&, NameKind, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity, std::uint64_t attempt);
class NameRegistry { public: bool contains(std::string_view) const; void add(std::string); /* … */ };
std::string make_name(const CultureNaming&, std::string_view culture_id, NameKind, std::uint64_t seed,
                      std::uint64_t culture, std::uint64_t entity, NameRegistry&);
std::string roman(std::uint64_t k);   // may reuse the regions stage's
}
```

### 9.1 Unit tests

1. `draw` against the reference for 16 tuples; `TAG_HIST` equals `tag_value("UNM:HIST")`.
2. `pick`: n = 1 → 0; u = 0 → 0; u = 2^64 − 1, n = 6 → 5; `chance` at p = 0 (never), ONE (always), HALF at u with high word 2^31 − 1 (true) and 2^31 (false); `weighted` with weights (0, 3, 1): the boundary values of u.
3. The worked example of 4.6 exactly, attempt by attempt (u(j), the picks, the root), and the collision and fallback cases.
4. Validity rules: a root below `min_letters`, above `max_letters`, with a forbidden substring, starting with `'`, with `'-` inside, and a name over 48 bytes are each invalid (hand-made naming data).
5. Capitalization: `gnink-snunk` → `Gnink-Snunk`, `uruld'duuld` → `Uruld'duuld`, `iensath` → `Iensath`.
6. Fatal: naming data that can never produce a valid root (e.g. `min_letters` 30 with one-letter syllables) is fatal with the message of 4.3.
7. Loader checks of section 3: each grammar and named check fails once with its message (through P3-01's kind registration).

### 9.2 Property tests

8. For each shipped culture and kind, 2,000 names into one registry (entities 1 … 2,000, seed 0 and 1): all unique, all satisfy invariant 1; the share needing the Roman fallback is reported (the reference measured 0 fallbacks in 1,000 settlement names per shipped culture, seed 0).
9. Order independence (invariant 3): naming entities of culture A interleaved with culture B gives the same A names as naming A alone, when no B name collides with an A name (checked by the test).

### 9.3 Golden vectors

10. `worldgen/tests/golden/p3-02-names.txt`, written by the test writer's **independent reference** `worldgen/tests/reference/p3_names_reference.py` (from this spec, using `core_reference` for hashing; not derived from the C++): for seeds 0 and 42, for each shipped culture and each kind, entities 1–50 named in the order (culture, kind, entity) into one registry per seed: one line `<seed> <culture> <kind> <entity> <attempt or F> <name>`; plus the SHA-256 of the newline-joined list per seed. The C++ must reproduce every line. The vectors of 4.6 must appear unchanged in it (seed 0 elven settlement 1 is `Iensath` with an empty registry: the file starts each seed with an empty registry, and its order is fixed, so the test writer states where 4.6's tuples fall).

## 10. Open questions

1. **The sound of the six cultures** (5.1). Recommendation: approve as first values; Paul and the critic judge on real histories, and changes are data.
2. **ASCII only.** Recommendation: approve; diacritics (`Elrond` vs `Eärendil`) would bring Unicode normalization into the report contract, the registry comparison and every reader. Revisit if Paul wants them, with NFC as the rule.
3. **Names unique across kinds** (a civilization and a settlement never share a name). Recommendation: approve; it keeps every reference in the log and in enrichment output unambiguous. Real histories reuse names (a city and its realm); history can express that with a form such as `The Kingdom of {root}` around a settlement's root in a later revision.
