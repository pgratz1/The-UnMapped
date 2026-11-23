# P3-02 Names

Status: **approved by the lead 2026-11-19; revised 2026-11-19 (text only, after implementation); amended 2026-11-19 for P3-03 (r2); revised 2026-11-19 after the P3-03 implementation (r3: 7 registry container, 4.3 `roman` location, 8 P0-01 request applied); amendment r5 drafted 2026-11-20 by the spec writer (section 11: name filter, god patterns, realm forms); **r6 (2026-11-20): r5 ships with the P3-08 change, generator 0.17.0 (`docs/specs/p3-08-followups.md`), with the filter applied to every word of the rendered name and the redraw stated (11.3, 11.6)**; **r13 (2026-11-21, P3-10 second round, generator 0.19.0): crude fragments inside a word and no three equal letters (section 13)**; **r15 (2026-11-21, P3-11, generator 0.20.0): faerie syllables, no repeated syllable, `tard` (section 14)** **r17 (2026-11-22, P5-01, generator 0.21.0): `fag` inside a word, `shiva`, `hindi`, `fannie`, the dwarven stutters `didin` and `ririn` (section 15)**; **r18 (2026-11-22): text after the P5-01 integration**; the seven questions of 11.10 are open with Paul, their interim choices stand; **r10 (2026-11-20, P3-10, generator 0.19.0): section 12, filter entries and near-identical names**: see the revision notes at the end. Phase P3 History core, spec 2 (P3-00 3.1). Design doc sections 5 ("the seed varies the dates, the places and the gods' names"; civilizations, settlements), 6 ("the database stores facts and names"), 14 (every reference a model writes must resolve to a real entity: names must be unique). Depends on P0-01 (hashing, streams), P0-03 (string grammars, named checks), P3-00 (4.6 keyed draws) and P3-01 (the `culture` kind carries this spec's record). It introduces the domain tag `TAG_HIST` and the keyed-draw helpers that every later history spec uses.

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

Fields, all required, in this declaration order (canonical encoding, P0-03 8). *r5, not yet shipped: `god_patterns` after `patterns`, and the check `naming_realm_forms` (11.4).*

| Field | Type | Meaning |
| --- | --- | --- |
| `onsets` | array(string(8, syllable), 1, 64) | consonant starts of a syllable; `""` allowed (no onset) |
| `nuclei` | array(string(8, syllable), 1, 64) | vowel cores; `""` not allowed (check) |
| `codas` | array(string(8, syllable), 1, 64) | syllable ends; `""` allowed |
| `patterns` | array(string(16, name_pattern), 1, 16) | shapes of a root word, picked uniformly; repeat an entry to weight it |
| `min_letters`, `max_letters` | int(1, 32) | bounds on the letters of a root (separators not counted) |
| `forbidden` | array(string(8, syllable), 0, 32) | substrings a root must not contain (phonotactics; the shared name filter of r5 is separate, 11.2) |
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
weighted(u, w[0..m-1])       = the smallest i with (u >> 32) × W < 2^32 × (w[0] + … + w[i]), W = Σ w   // m ≥ 1, w ≥ 0, 1 ≤ W ≤ 2^63 − 1, products in uint128
```

`w0` is the year in the simulation's draws (P3-00 4.6) and the attempt number in this spec's; a and b are entity ids or other integers (0 if unused); k counts the draws of one decision. All words are `uint64` (signed values as two's complement). `stream_key(seed, name)` may be computed once per stream and kept.

**`weighted` in detail.** The weights are `int64`; the sum W is formed with overflow checking, so a list whose sum exceeds 2^63 − 1 is fatal (P3-00 H6), as are an empty list, a negative weight and W = 0. This is the one wording of the rule: P3-00 4.6's "the first index whose prefix sum exceeds (u >> 32) × Σ >> 32" is the same rule (for integers P and x, P > ⌊x / 2^32⌋ exactly when 2^32 × P > x); the wording here is the authoritative one. A zero weight is never chosen. A positive weight w[i] is chosen for at least one value of the high word u >> 32 when w[i] / W ≥ 2^-32, and for at most one value (possibly none) when w[i] / W < 2^-32: so a weight whose share is below 2^-32 is in general unreachable, though it may still be chosen for a single high word (a positive w[0] is chosen for the high word 0 whatever its share). Callers keep every weight they mean to be reachable at a share of at least 2^-32.

Seams: `history::draw(key, w0, a, b, k)` taking the stream key, `history::pick`, `history::chance`, `history::weighted`.

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

*r5, not yet shipped: gods take their pattern from `god_patterns`, and `valid` also requires that the root word passes the name filter (11.3, 11.4).*

**An invalid attempt** has `valid` false; its `name` (and `root`, section 9) are unspecified and must not be relied on by callers or tests. Only `valid` is defined for it.

**Malformed naming data.** A form without exactly one `{root}`, a pattern symbol other than `O`, `N`, `C`, `'` and `-`, and an empty `onsets`, `nuclei`, `codas`, `patterns` or `<K>_forms` list are ruled out by the loader (the grammars and array bounds of section 3), so loaded data never has them. The library does not rely on that: `name_attempt` (and so `make_name`) is fatal when it meets one, with `names: a name form must hold exactly one {root}`, `names: a name pattern holds a symbol other than O, N, C, ' and -` and `names: a naming list is empty` respectively. A list is checked when the attempt picks from it (an empty `codas` list is not met by a pattern without `C`).

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

`roman(k)` is P1-06 RG11's (subtractive form, thousands as repeated M), and like P1-06's it is fatal for k outside [1, 100000] (`roman: k outside [1, 100000]`). From P3-03 on there is one implementation, `worldgen::roman_numeral` in `worldgen/core/roman.hpp`; `history::roman` and `stages::roman` forward to it. The registry is a set of names compared as exact bytes; adding a name it already holds changes nothing.

### 4.4 Uniqueness, bounds and order

- **The registry is per world** and holds every name the world has: the caller (P3-03) fills it first with the region names of table `region`, in id order, and then adds every name it generates (r2: P3-03 8 adds the age names, then the god names; the `history` stage rebuilds it as the set of names in `region`, `age` and `god`, P3-03 8.2). Gods, civilizations and settlements share it, so no two named entities of a world share a name, whatever their kind. Regions keep their placeholder names (P1-06), which have a different form (`East Wood II`), so collisions with them are rare but are still avoided.
- **Bounds**: at most 64 attempts per name; a name is at most 48 bytes, or 48 + 1 + the numeral after the fallback. **64 bytes is the hard limit**: a fallback name of exactly 64 bytes is accepted, one of 65 bytes or more is fatal with `names: a name longer than 64 bytes` (checked after the numeral is chosen, before the name is added). Every numeral below 4888 has at most 15 bytes (3888, `MMMDCCCLXXXVIII`, is the longest below 4000), and 4888, `MMMMDCCCLXXXVIII`, is the first with 16; so the limit can be reached only by a 48-byte first name whose numbered forms II … MMMMDCCCLXXXVII are all taken, which needs at least 4,887 names in the registry sharing that first name.
- **Order**: a name depends on the registry at the moment it is drawn, so callers name entities in their creation order (H2): the result is defined because creation order is. A name does not depend on how many draws other decisions made (H4): its draws are keyed by (kind, attempt, culture, entity).
- **Cost**: at most 64 × (2 + 16) draws per name; worlds have a few hundred named entities.

### 4.5 Name kinds and streams

| Kind | Stream | Entity id e | Used by |
| --- | --- | --- | --- |
| `god` | `hist.names.god` | `god.id` (= the race id of the god's race, one god per race, present or not: P3-03 5.2, r2) | P3-03 |
| `civilization` | `hist.names.civilization` | `civilization.id` | P3-04 (stub, S2), P3-05 |
| `settlement` | `hist.names.settlement` | `settlement.id` | P3-04 (stub, S2), P3-05 |

The **kind order** is the order of this table, god, civilization, settlement, which is also the order of the enumerators of `NameKind` (section 9) and the order of kinds in the golden list (9.3).

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

Further vectors (seed 42, one registry starting empty, separate from any other, in this order): god, dwarven (c 1), e 1 → `Khork` (attempt 0); civilization, goblin (c 4), e 3 → `The Grork Horde` (attempt 1, attempt 0 being invalid); settlement, human (c 5), e 17 → `Deantwick`; settlement, orcish (c 6), e 200 → `Ol'uraakh Fort`.

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

The table follows this protocol: seed 0; **one registry per culture**, starting empty; in each, settlements for entities 1–20, then civilizations 1–5, then gods 1–3, named with `make_name` and the culture's number c. The test writer's independent reference reproduces the table exactly (`python3 -B worldgen/tests/reference/p3_names_reference.py samples`, which compares its names with a transcription of this table). For the lead and Paul to judge the sound; these are first values.

| Culture (c) | Settlements 1–20 | Civilizations 1–5 | Gods 1–3 |
| --- | --- | --- | --- |
| `dwarven` (1) | Moumka, Thoumdaili, Makmoudin Deep, Brummthu, Gromka, Kaird Deep, Khaili Hold, Kili, Thoum Hold, Nodin, Gondhark, Turnbruk Deep, Kidinkorn, Tharndramm Hold, Grangrak, Houmm Deep, Doknirk, Gali, Nailimim Deep, Tairdbra Hold | The Kourdbrar Throne; The Khandthra Throne; Clan Durgarin; Clan Drourd; Clan Drailimai | Brommgrai, Groumtoun, Nurnthaimm |
| `elven` (2) | Iensath, Onaeme, Ierrael, Elonoma, Fievos, Mielae, Asvathdell, Lafin, Laevo, Leithiedell, Orith, Voglael, Mievife, Iefann, Cellve, Fimo, Londcae, Rienel, Nathol, Siendnie | Nife; The Court of Nothind; Aesoth; Riefifie; The Court of Thiesaell | Saene, Fiendse, Ithonnfa |
| `faerie` (3) | Liapa, Wiatwiesk Glade, Twamae Glade, Lip, Biafloss Glade, Pieshaep Hollow, Teelan, Meeie Hollow, Sywysh Hollow, Iatysk, Wiepysk, Miemo Hollow, Mapoufee, Feetop Hollow, Showhie Hollow, Mawhiap Hollow, Athfli Hollow, Whywo, Fliaposk Hollow, Whosy Hollow | The Lyp Ring; The Watiafia Court; The Bouss Ring; The Wosk Ring; The Twiwhiwou Ring | Fiwhall, Fiabo, Biensho |
| `goblin` (4) | Krot Pit, Slograb Pit, Gnink-Snunk, Zux, Grunkgnuzz Pit, Grizz-Skeeb, Slank-Skrox, Drobskeet, Bag Hole, Sluggatch Hole, Bux-Kreeb, Sniz, Slak, Zug-Gok, Gunitch, Slagreeg Hole, Droggkazz Hole, Slobeerk, Snotgab, Kruskreezz Hole | The Gnazz Horde; The Warrens of Bonk-Gitch; The Grodratch Horde; The Krunkgirk Horde; The Warrens of Gnonk | Zabskreeb, Kruskux, Bazz |
| `human` (5) | Deanham, Alantton, Cickham, Gackwick, Brir, Tackbury, Gurdton, Studford, Ceambury, Buld, Silstudstead, Tirdwick, Dack, Stastint, Moldstead, Edumford, Tontstead, Ceanbir, Durdton, Durdbury | The Duchy of Ashem; The Kingdom of Ashos; The Duchy of Hick; The Duchy of Wick; The Duchy of Circon | Simbear, Histold, Edostold |
| `orcish` (6) | Kaak Fort, Zaarz Camp, Grush, Zaakugaag, Uruld'duuld, Uu'mold Fort, Kark Fort, Azol'urash Fort, Karzthogh, Uromgha, Maag, Uul'krakh Camp, Og'buurk Camp, Zaalduruz Camp, Mak, Duld'sha Fort, Shuum Fort, Ghorz Fort, Duuldkrosh Fort, Aag'urog Fort | Clan Kram'krorz; The Azakhdam Horde; The Urogh'krorz Horde; The Burk'zork Horde; The Warband of Maakurokh | Ugh'kaal, Og'uraag, Uruuk |

*r18: from generator 0.21.0 the dwarven civilization 2 is "The Khandthra Throne" (before, "The Hold of Didin", which 15.2's `didin` now forbids).*

*r5, not yet shipped: each block gains `god_patterns`, the elven `civilization_forms` lose the bare `{root}` (11.4); the r5 sample table is in 11.6.*

Weak spots visible in the samples, left for tuning with Paul's taste: some human roots are clipped (`Brir`, `Buld`, `Stastint`); elven names lean on `ie`; faerie settlement forms use `Hollow` often (two of four forms carry a suffix). Each is a data change in one block.

## 6. Invariants

1. Every generated name matches: ASCII letters, spaces, `'`, `-`; at most 64 bytes; the form's fixed text plus a root whose letter count is within the culture's bounds, optionally followed by a space and a Roman numeral ≥ II.
2. No name is generated twice into one registry.
3. The name of (seed, kind, culture, entity) with a given registry content is a pure function; it does not depend on any other draw, on thread count or on the order in which names of other cultures were generated, except through the registry content.
4. With an empty registry, the result is the first valid attempt.

## 7. Determinism notes

Integer arithmetic only (`uint64` hashes, a `uint128` product in `weighted`); string operations on ASCII bytes; the registry is an ordered set used only for membership (never iterated, so H3 holds; unordered containers are banned in `worldgen`, P0-01 4 and the determinism lint rule `unordered-container`); no locale-dependent case mapping (upper-casing is `a`–`z` to `A`–`Z` by arithmetic).

## 8. Amendments requested

- **P0-01**: the tag table gains `TAG_HIST`, `UNM:HIST`, 0x545349483A4D4E55; the stream-name list gains the three `hist.names.*` names (P0-01 7.3; names registered in `streams.hpp`, appended to `all_stream_names` after `geo.regions.cost` in the order god, civilization, settlement). `all_stream_names` grows from 30 to 33 entries; a test that asserts the exact count changes with it (today `worldgen/tests/stages_climate_offset_test.cpp`, 30 → 33). Registering a stream name changes no world byte. (Applied as P0-01 section 16, 2026-11-19, with P3-03's `hist.chronology.awakening`: 34 names.)
- **P3-00 4.6**: done on this branch: P3-02 introduces `TAG_HIST` and the helpers. P3-00 4.6's one-line description of `weighted` is equivalent to 4.1's; 4.1 is the authoritative wording, with the bounds stated there.

## 9. Tests required

For the independent test writer. Seams, as implemented (`worldgen/include/worldgen/history/draw.hpp`, `worldgen/include/worldgen/history/names.hpp`; `TAG_HIST` beside the other P0-01 tags in `worldgen/include/worldgen/core/hash.hpp`). Every contract violation raises `FatalError`.

```cpp
namespace worldgen::history {
// draw.hpp
std::uint64_t draw(std::uint64_t stream_key, std::uint64_t w0, std::uint64_t a, std::uint64_t b, std::uint64_t k) noexcept;
std::uint64_t pick(std::uint64_t u, std::uint64_t n);                  // fatal if n = 0 or n > 2^32
bool chance(std::uint64_t u, Fx p);                                     // fatal if p outside [0, ONE]
std::size_t weighted(std::uint64_t u, std::span<const std::int64_t> w); // fatal if w is empty, a weight < 0, the sum is 0
                                                                        // or the sum exceeds 2^63 - 1 (4.1)
// names.hpp
struct CultureNaming {                       // the fields of section 3, in declaration order
    std::vector<std::string> onsets, nuclei, codas, patterns;
    std::int64_t min_letters, max_letters;
    std::vector<std::string> forbidden, god_forms, civilization_forms, settlement_forms;
};
CultureNaming culture_naming_from(const data::Record& naming);   // the value of a culture's `naming` field
enum class NameKind : std::uint8_t { god, civilization, settlement };   // the order of 4.5
std::string_view name_kind_text(NameKind);                        // "god", "civilization", "settlement"
std::string_view name_stream(NameKind);                           // "hist.names.<kind>" (4.5)
const std::vector<std::string>& name_forms(const CultureNaming&, NameKind);   // naming.<kind>_forms
inline constexpr std::size_t NAME_ATTEMPT_MAX_BYTES = 48;          // 4.2
inline constexpr std::size_t NAME_MAX_BYTES = 64;                  // 4.4
inline constexpr std::uint64_t NAME_ATTEMPTS = 64;                 // 4.3
struct NameAttempt { bool valid; std::string name; std::string root; };   // root before Cap; name and root
                                                                          // unspecified when !valid (4.2)
std::string capitalize_root(std::string_view root);               // Cap of 4.2
NameAttempt name_attempt(const CultureNaming&, NameKind, std::uint64_t seed, std::uint64_t culture,
                         std::uint64_t entity, std::uint64_t attempt);    // fatal on malformed data (4.2)
class NameRegistry {
public:
    bool contains(std::string_view) const;
    void add(std::string);               // adding a name already present is a no-op
    std::size_t size() const noexcept;
};
std::string make_name(const CultureNaming&, std::string_view culture_id, NameKind, std::uint64_t seed,
                      std::uint64_t culture, std::uint64_t entity, NameRegistry&);   // 4.3; adds the name
struct NameChoice { std::string name; std::uint64_t attempt; bool fallback; };
NameChoice choose_name(const CultureNaming&, std::string_view culture_id, NameKind, std::uint64_t seed,
                       std::uint64_t culture, std::uint64_t entity, NameRegistry&);
std::string roman(std::uint64_t k);      // P1-06 RG11; fatal outside [1, 100000]
}
```

`choose_name` is `make_name` with the attempt that produced the name, which the golden format of 9.3 needs: `name` is `make_name`'s result (and is added to the registry the same way); when `fallback` is false, `attempt` is the number t of the accepted attempt; when `fallback` is true (the numeral rule of 4.3 was used), `attempt` is the number of the first valid attempt, whose name took the numeral. `make_name(...)` equals `choose_name(...).name`.

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

10. `worldgen/tests/golden/p3-02-names.txt`, written by the test writer's **independent reference** `worldgen/tests/reference/p3_names_reference.py` (from this spec, using `core_reference` for hashing; not derived from the C++). **The name list**: for seeds 0 and 42, one registry per seed starting empty; cultures in canonical id order (c = 1 … 6), within a culture the kinds in the order of 4.5 (god, civilization, settlement), within a kind entities 1–50; one line per name, `<seed> <culture> <kind> <entity> <attempt or F> <name>`, in naming order (900 lines per seed). `<attempt>` is the decimal number of the accepted attempt, `F` marks a name that took the numeral fallback (`choose_name`'s `fallback`). **The checksum**: per seed, one line `sha256 <seed> <hex>`, the SHA-256 in lower-case hex of that seed's 900 name lines (each the full line as above) joined by LF, with no trailing LF. The C++ must reproduce every line.

**Where 4.6's vectors appear.** They are not lines of the name list as such: in the list, the registry is not empty when a given tuple is named, and the seed-42 vectors of 4.6 come from their own registry. The file therefore carries them as separate items, and the test writer's header states where each falls:
- seed 0, elven settlement 1 with an empty registry (`Iensath`, attempt 0): the scenario `empty`; the collision case (`Focoth`, attempt 1): scenario `collide`; the fallback case (`Iensath II`): scenario `full` (61 preloaded names);
- in the name list, the same tuple is the line `0 elven settlement 1 0 Iensath`: the registry then holds the 250 earlier names of seed 0 (dwarven 150, elven god and civilization 100), none of them `Iensath`, so the result agrees with 4.6;
- the seed-42 vectors of 4.6 (one registry of their own, in the order given there): scenario `seed42`.

## 10. Open questions

1. **The sound of the six cultures** (5.1). Recommendation: approve as first values; Paul and the critic judge on real histories, and changes are data.
2. **ASCII only.** Recommendation: approve; diacritics (`Elrond` vs `Eärendil`) would bring Unicode normalization into the report contract, the registry comparison and every reader. Revisit if Paul wants them, with NFC as the rule.
3. **Names unique across kinds** (a civilization and a settlement never share a name). Recommendation: approve; it keeps every reference in the log and in enrichment output unambiguous. Real histories reuse names (a city and its realm); history can express that with a form such as `The Kingdom of {root}` around a settlement's root in a later revision.

## 11. Amendment r5: name filter, god patterns, realm forms (ships with P3-08, generator 0.17.0; r6; r8: the short word list, 11.11)

**Ships with P3-08** (r6; `docs/specs/p3-08-followups.md`, generator 0.17.0). This section is normative for that change, which also carries the region names (P1-06 r3) and the faerie weights (P3-05 12.3). Until it is implemented sections 1 to 10 describe the shipped behaviour (generator 0.15.0 and 0.16.0) and the C++, the reference and every golden follow them. The data files of 11.2 are committed at their final paths now, because a loader that does not know a kind never reads its directory (P0-03 7.2): they change no world until kind `name_filter` is registered. The culture changes of 11.4 cannot be committed now (the loader refuses an unknown key `god_patterns`), so their text is given here, ready to apply.

Why: the world critic's review of P3-05 (`docs/reviews/p3-05-peoples-world-critic.md` F5) found, in eight worlds, a goblin settlement name whose first syllables form a racial slur plus two near misses, human towns starting with a crude word for a body part, gods named with ordinary English words ("Bald", "Buzz", "Gord"), gods with village-short names ("Krok", "Sten", "Alis"), and 7 of 14 elven realms with a bare one-word name. Design-level record: `docs/design/proposals.md`, "Names: generated names are filtered; gods have their own style; every realm name has a realm form".

### 11.1 What changes

| Area | r4 (shipped) | r5 |
| --- | --- | --- |
| Validity of an attempt (4.2) | letters, culture `forbidden`, separators, 48 bytes | the same, **and the root word passes the name filter** (11.3) |
| Data | kind `culture` | new required kind **`name_filter`** (11.2), shared by every culture; `culture_naming` gains **`god_patterns`** (11.4) |
| God roots | shapes from `patterns` | shapes from **`god_patterns`** (shipped: the culture's shapes with two or more nuclei) |
| Elven civilization forms | `The Court of {root}`, `The Realm of {root}`, `{root}` | `The Court of {root}`, `The Realm of {root}`, **`The House of {root}`** |
| Loader checks | three `naming_*` checks | plus **`naming_realm_forms`** (11.4), the `name_filter` checks (11.2) and the stage check `names_form_text` (11.3) |
| Schema | 7 | **unchanged** (no table or column changes; only values) |

### 11.2 Data kind `name_filter`

A new kind (P0-03 7.1): name **`name_filter`**, directory **`name_filters`**, **required**, open ids. Kinds are processed in ascending name order, so it comes after `history` and before `placename` and `race`. Shipped files (normative, committed with this revision): `game/data/name_filters/slurs.toml` (id `slurs`), `crude.toml` (`crude`), `words.toml` (`words`), `famous.toml` (`famous`). The fixture roots `worldgen/tests/data/geo/` and `worldgen/tests/data/geo-small/` get identical copies with the implementation (every data root that loads holds every required kind).

**Fields**, all required, in this declaration order (the canonical encoding, P0-03 8):

| Field | Type | Meaning (11.3) |
| --- | --- | --- |
| `anywhere` | array(string(16, filter_word), 0, 4096) | fragments rejected anywhere in the root word, across `'` and `-` |
| `at_start` | array(string(16, filter_word), 0, 4096) | fragments rejected at the start of a part of the root word |
| `whole` | array(string(16, filter_word), 0, 4096) | words rejected when a part of the root word is exactly the word |

Grammar `filter_word` (kind-local): two to sixteen ASCII lower-case letters `[a-z]{2,16}`; description `lower-case letters a-z, 2 to 16 of them`.

**Named checks**, per definition (`… failed for 'name_filter.<id>': <message>`):

| Check | Condition | Message |
| --- | --- | --- |
| `name_filter_unique` | no entry twice within one list | `<field>[<j>] '<value>' repeats <field>[<i>]` (first list in declaration order, first repeat j, its first occurrence i) |

**Kind checks** (all definitions, ascending id), in this order:

| Check | Condition | Message |
| --- | --- | --- |
| `name_filter_distinct` | no entry is in the same list of two definitions | `<field> '<value>' is in '<id1>' and '<id2>'` (first such entry by definition id, then list, then position; ids ascending) |
| `name_filter_not_redundant` | no `at_start` or `whole` entry contains an `anywhere` entry as a substring, and no `whole` entry starts with an `at_start` entry (such an entry can never change a result) | `'<id>' <field> '<value>' is already rejected by <field2> '<value2>' of '<id2>'` (first offending entry by definition id, list `at_start` before `whole`, position; for it, the first covering entry by definition id, list, position) |

**Where the checks report** (r7, as implemented): `name_filter_unique` at the definition it fails for; `name_filter_distinct` and `name_filter_not_redundant` at the later of the two definitions involved (the one with the larger id), and each kind check reports only its first failure.

**The effective lists** are the unions over all definitions. The rule of 11.3 tests membership only, so the order of entries and the split into definitions and files never change a name; they do change `content_hash` (a moved entry changes the canonical encoding), as for any data edit. A list may be empty; with all three empty the filter rejects nothing.

**Sizes of the shipped lists** (entries; r8, Paul's short list, 11.11): `slurs` 33 anywhere, 19 at_start, 6 whole; `crude` 68, 16, 14; `words` 0, 0, **393**; `famous` 0, 0, 101. Effective: 101 anywhere, 35 at_start, **514** whole. The slur and crude entries are kept to the shortest fragment that matches (`nig`, not the full words), so that the file is not a list of offensive text. **`words` (r8) holds the English words that make a name read as a joke or an error: crude, comic or modern-sounding words, and function words and inflected forms** (11.11 gives the rule, the classes and every word). Ordinary English words that fit the setting are not on it and stay as name roots (`The Kingdom of Gold`, `Thorn Deep`, `Moss Hollow`). (r5 to r7, generator 0.17.0: `words` held 647 entries, every lower-case word of the system English word lists (`/usr/share/dict/american-english` and `british-english`) of three or more letters that a shipped culture reaches as a root part with probability at least 10^-5 per attempt (11.6), plus 14 comic words added by hand (`gord`, `pypy`, `bap`, `dud`, ...); the r8 list is a subset of it.) Two-letter words are left out on purpose: orcish apostrophe parts (`Am'gash`, `Um'daz`) read as orcish, not English. `famous` holds gods of real religions, well-known names of fiction and games, famous places and peoples, and common modern first names that the shipped syllables reach (`thor`, `isis`, `vanir`, `durin`, `elrond`, `greek`, `stan`), after Paul's rule for region names (only famous names are excluded).

**Content hash.** `name_filter` is a new required kind, so `content_hash` changes for every world, including the P0 empty worlds (P0-03 8).

### 11.3 The filter rule

For an attempt of 4.2 with form F and root R (R before Cap), split F at `{root}` into A and B. Then:

```
A' = the part of A after its last space (all of A if it has no space)
B' = the part of B before its first space (all of B if it has no space)
W  = lower(A') + R + lower(B')             // the root word; lower maps A-Z to a-z, nothing else
S  = W with every ' and - removed
parts = the maximal non-empty runs of letters a-z in W   (W split at ' and -)
filtered(W) = some e in anywhere is a substring of S
           or some p in parts starts with some e in at_start
           or some p in parts equals some e in whole
```

4.2's validity becomes `valid = (r4 validity) and not filtered(W)`.

**The whole rendered name is covered** (r6, after the world critic's review of P3-06, `docs/reviews/p3-06-conflict-world-critic.md` F10: "a substring blocklist over the whole name, not only at word starts"). Every word of the rendered name (the name split at spaces) is tested, in two parts:

- **The root word** W, by `filtered(W)` above, at every attempt. W holds every letter of the drawn root and every letter of the form fused to it (`{root}ford`, `Ny{root} Pit`), so a fragment is caught anywhere inside the word, across the root's syllables, its `'` and `-` and the fused form letters, not only at a word start (`Cockford`: W `cockford`; a goblin `…-Sn*gg`: W squeezed `…sn*gg`).
- **Every other word**, the form's fixed words (`The`, `Hold`, `of`, `Hollow`), once per form by the stage check `names_form_text` below, for `anywhere` and `at_start`. They are the same in every name of the form, so a form that passed the check can never make a name rejected or offensive through them. `whole` is not applied to fixed words: they are English by design, and `words` holds `hold`, `stead`, `ton` and `wick`.

The test is per word and not across spaces: the squeezed whole name (all spaces removed) was measured on every distinct civilization, settlement and god name of the 375 worlds of the P3-06 validation sweep (18,427 names, generator 0.16.0): it adds exactly one rejection that the rule above does not make, and it is a false positive across a space (`The Realm of Arthos`: `…ofarthos` holds an `anywhere` entry spanning `of` and `Arthos`). Two-word crude names are caught through their root word, since the crude part is always the root (`Bag Hole`: W `bag`; a `{root} Hole` with a crude root: its W).

The Roman numeral of 4.3's fallback is not part of an attempt (the fallback numbers the first valid attempt's name, which passed the filter).

**How a rejected name is redrawn** (r6, stated for the engineer). A filtered attempt is an invalid attempt, and 4.3 takes the next attempt t + 1: its draws are u(j) = draw(S_K, t + 1, c, e, j) (4.2), keyed by the stream of the name kind, the attempt number, the culture number and the entity id. So the name an entity gets depends only on the seed, its kind, its culture, its id, the data and the registry at the moment it is named (4.4), never on how many attempts other names needed or on which of them were filtered. If all 64 attempts are invalid (filtered or not) the fatal message of 4.3 applies; if valid attempts exist but all are in the registry, the fallback numbers the first valid one, which passed the filter. Examples with the shipped lists: `Cockford` (W `cockford`; anywhere `cock`), `Bumdou` (at_start `bum`), `But-Skrotch` (parts `but`, `skrotch`: whole `but`, and anywhere `krotch`), `Ugh'kaal` (part `ugh`, whole), `Bag Hole` (W `bag`, whole); `Stard` passes (whole `tard` does not equal `stard`; r15: from 0.20.0 `tard` is an `anywhere` entry and `Stard` is filtered, 14.3), `Bumdou` is filtered (at_start `bum`) while `Abumdou` is not. (r14: this sentence gave `Cumbrin` filtered by at_start `cum` and `Ascumbrin` passing; 13.1 makes `cum` an `anywhere` entry, so from r13 both are filtered, and the at_start example is `bum`.)

A filtered attempt is an invalid attempt for every purpose of 4.3 (it is skipped; its name is never the fallback's first name), and the fatal message of 4.3 is unchanged. Invariant 6 gains: **5. No generated name's root word is filtered.**

**Stage check `names_form_text`** (the history stage, with the culture and filter data, before any name is drawn; P3-03's input checks gain it): for every culture (ascending id), kind (god, civilization, settlement) and form i, with A', B' as above and S_A, S_B their lower-cased letters without `'` and `-`: no `anywhere` entry is a substring of S_A or of S_B, and S_A (if not empty) does not start with an `at_start` entry; and (r6) for every **fixed word** of the form (each word of A before A' and of B after B', split at spaces, lower-cased, `'` and `-` removed): no `anywhere` entry is a substring of it and it does not start with an `at_start` entry. Otherwise fatal: `history: culture '<id>' <kind>_forms[<i>] '<form>' holds the filtered text '<entry>'`. Such a form would reject every attempt, or reject them for its own fixed letters. The shipped forms pass (their fixed letters inside the root word are `dell`, `ford`, `ton`, `bury`, `wick`, `ham`, `stead`; their fixed words, checked by the spec writer against the shipped lists on 2026-11-20, are `the`, `of`, `kingdom`, `league`, `duchy`, `hold`, `clan`, `throne`, `deep`, `court`, `realm`, `house`, `horde`, `warrens`, `mob`, `pit`, `hole`, `ring`, `circle`, `glade`, `hollow`, `camp`, `fort`, `warband`). A test with a fixture form `The Scrapyard of {root}` (the `anywhere` entry `crap` inside a fixed word) and one with `The Snotty Court of {root}` (the `at_start` entry `snot`) is fatal with the message above (r6).

**Determinism.** Byte comparisons of ASCII strings, no locale, membership tests only (the lists are ordered `std::vector`s or ordered sets, never unordered containers); the result does not depend on list order.

### 11.4 Culture naming: `god_patterns` and realm forms

**New field of `culture_naming`** (section 3), declared directly after `patterns` (so the field order becomes `onsets`, `nuclei`, `codas`, `patterns`, `god_patterns`, `min_letters`, ...):

| Field | Type | Meaning |
| --- | --- | --- |
| `god_patterns` | array(string(16, name_pattern), 1, 16) | shapes of a god's root, picked uniformly; repeat an entry to weight it |

4.2 changes in one line: `pattern = P[pick(u(1), |P|)]` with P = D.`god_patterns` for K = god and D.`patterns` otherwise. Every other draw is as before. (With `god_patterns` equal to `patterns` and empty filter lists, r5 gives r4's names exactly, which the tests use.) The library treats an empty `god_patterns` like the other lists of 4.2 (`names: a naming list is empty`).

**New named check of kind `culture`**, after the three of section 3:

| Check | Condition | Message |
| --- | --- | --- |
| `naming_realm_forms` | every `civilization_forms` entry contains a space (a word besides the root's) | `naming.civilization_forms[<i>] must hold a word besides {root}` |

**Shipped data** (applied to `game/data/cultures/<id>.toml` and both fixture copies with the implementation; a line `god_patterns = …` after `patterns` in each culture, and one changed line for the elves):

```toml
# dwarven
god_patterns = ["ONCONC", "ONCONC", "ONCON"]
# elven
god_patterns = ["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"]
civilization_forms = ["The Court of {root}", "The Realm of {root}", "The House of {root}"]
# faerie
god_patterns = ["ONON", "ONONON", "ONONC", "ONCON"]
# goblin
god_patterns = ["ONCONC", "ONC-ONC", "ONONC"]
# human
god_patterns = ["ONCONC"]
# orcish
god_patterns = ["ONC'ONC", "ONCONC", "NC'ONC"]
```

Each culture's `god_patterns` is its `patterns` without the one-nucleus shapes (`ONC`, and the elves have none), keeping the repeats. This is the smallest rule that works: no god root is one syllable, it is data (Paul can give gods longer or stranger shapes per culture), and it costs one field. The elven third form keeps its index, so an elven realm that had a form keeps its name and only the bare ones change (`Nife` → `The House of Nife`).

### 11.5 Seams (interim; the engineer records the final signatures)

```cpp
namespace worldgen::history {
struct NameFilter { std::vector<std::string> anywhere, at_start, whole; };   // effective lists (11.2)
NameFilter name_filter_from(const data::ContentSet&);                      // union in ascending definition id
std::string root_word(std::string_view form, std::string_view root);       // W of 11.3
bool name_filtered(const NameFilter&, std::string_view root_word);         // filtered(W)
// CultureNaming gains std::vector<std::string> god_patterns, after patterns.
// NameAttempt gains bool filtered: true when the r4 rules pass and the filter rejects (valid is then false).
NameAttempt name_attempt(const CultureNaming&, const NameFilter&, NameKind, std::uint64_t seed,
                         std::uint64_t culture, std::uint64_t entity, std::uint64_t attempt);
std::string make_name(const CultureNaming&, const NameFilter&, std::string_view culture_id, NameKind, ...);
NameChoice choose_name(const CultureNaming&, const NameFilter&, std::string_view culture_id, NameKind, ...);
}
```

The three callers (`stages/chronology.cpp` for gods, `history/stubs.cpp` and `history/peoples.cpp` for civilizations and settlements) pass the world's filter, built once per world from the loaded data.

**As implemented** (generator engineer, P3-08, 2026-11-20; `worldgen/include/worldgen/history/names.hpp`): the seams above, with `NameAttempt::filtered`; `name_attempt`, `make_name` and `choose_name` take the filter as their second parameter, and the overloads without it (an empty filter) remain, so r4 callers and tests compile; `std::optional<std::string> names_form_text_problem(const data::ContentSet&, const NameFilter&)` and `check_names_form_text(...)` (the FatalError) are the stage check, run by `chronology_stage` after `check_history_data` and by `history_stage` after `check_engine_inputs`, both with the message of 11.3 unchanged (prefix `history:` also in the chronology stage).

### 11.6 Measurements

(r8) The figures of this section are for the full `words` list of r5 to r7 (647 words, generator 0.17.0) and stay as its record. The short list of r8 (393 words; measured in 11.11.3 with 392, `mold` released) is measured in 11.11.3: exact rates, the 50-world comparison and samples.

Prototype `~/.cache/the-unmapped/out/p3-02-names-filter/` (`nfilter.py` the rule of 11.3, `r5.py` the r5 attempt on the test writer's reference `p3_names_reference.py`, `space.py` an exact enumeration of every root a culture can draw with its probability, `measure.py`, `sample.py`, `residual.py`, `gods.py`, `realms.py`, `placescan.py`, `edits.py`). Rates are the share of attempts that pass the r4 rules and that the filter rejects.

**Exact rejection rates** (enumeration of the whole root space; god rows with `god_patterns`):

| Culture | God | Civilization | Settlement | Main entries |
| --- | --- | --- | --- | --- |
| dwarven | 0.53% | 2.89% | 2.89% | words (2.45%), `bum`, `turd`, `dik`, `durin` |
| elven | 1.58% | 1.58% | 1.46% | `lol` (0.88%), words, `nig`, `fag` |
| faerie | 2.19% | 5.17% | 5.17% | words (3.2%), `pee` (0.87%), `shit`, `twat`, `tit` |
| goblin | 11.89% | 12.96% | 12.96% | words (9.4%), `nig` (1.4%), `slut`, `neeg`, `slag`, `krotch`, `snot` |
| human | 3.49% | 19.12% | 5.14% | words (16.3% of realms: bare English roots), `lol`, `turd`, `dick`, `cunt`, `sick`, `cock` |
| orcish | 7.57% | 6.49% | 6.49% | words (6.0%: `ugh`, `old`, `ash`, `ark` parts), `shag`, `bum`, `baal` |

Without the `words` and `famous` lists (slurs and crude only) the rates are dwarven 0.47%, elven 1.09%, faerie 1.95%, goblin 3.88%, human 2.01% (settlements 1.92%), orcish 0.62%.

**Keyed-draw sample** (`sample.py`: per culture and kind, seeds 0–399 × entities 1–500 = 200,000 names, one registry per seed, culture and kind; so registries of 500 names, more than any real world): agrees with the exact rates within 0.05 points.

| Culture | Kind | Attempts | Invalid (r4) | Filtered | Filtered share | Registry repeats | Most attempts for one name | Fallbacks |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| dwarven | god | 273,438 | 72,214 | 1,103 | 0.55% | 121 | 9 | 0 |
| dwarven | civilization | 270,298 | 62,953 | 6,013 | 2.90% | 1,332 | 9 | 0 |
| dwarven | settlement | 270,627 | 63,057 | 6,008 | 2.89% | 1,562 | 10 | 0 |
| elven | god | 227,512 | 23,903 | 3,136 | 1.54% | 473 | 6 | 0 |
| elven | civilization | 227,513 | 24,170 | 3,219 | 1.58% | 124 | 8 | 0 |
| elven | settlement | 227,023 | 23,822 | 2,894 | 1.42% | 307 | 6 | 0 |
| faerie | god | 209,320 | 4,297 | 4,446 | 2.17% | 577 | 5 | 0 |
| faerie | civilization | 219,083 | 7,314 | 10,996 | 5.19% | 773 | 6 | 0 |
| faerie | settlement | 219,275 | 7,403 | 10,965 | 5.18% | 907 | 5 | 0 |
| goblin | god | 253,324 | 26,078 | 27,024 | 11.89% | 222 | 8 | 0 |
| goblin | civilization | 250,996 | 15,553 | 30,569 | 12.98% | 4,874 | 7 | 0 |
| goblin | settlement | 251,518 | 15,402 | 30,632 | 12.97% | 5,484 | 8 | 0 |
| human | god | 241,430 | 33,988 | 7,362 | 3.55% | 80 | 8 | 0 |
| human | civilization | 375,051 | 107,109 | 51,232 | 19.12% | 16,710 | 16 | 0 |
| human | settlement | 304,007 | 86,706 | 11,081 | 5.10% | 6,220 | 12 | 0 |
| orcish | god | 234,036 | 17,474 | 16,404 | 7.57% | 158 | 6 | 0 |
| orcish | civilization | 230,348 | 12,154 | 14,171 | 6.49% | 4,023 | 7 | 0 |
| orcish | settlement | 230,927 | 12,034 | 14,288 | 6.53% | 4,605 | 7 | 0 |

(Measured before the last four hand-added `crude` words, `pantie`, `panty`, `sissy`, `weenie`, whose reach is below 10^-5 per attempt in total.)

**The 64-attempt bound is never neared.** The worst case is human civilizations: an attempt succeeds with probability 0.7147 × (1 − 0.1912) = 0.578, so all 64 fail with probability 0.422^64 ≈ 10^-24 on an empty registry; the most attempts any of the 3.6 million sampled names needed was 16, and no name took the numeral fallback.

**Which syllables cause the hits.** No single syllable causes most of a culture's offensive hits (slurs and crude lists); the largest shares are goblin nucleus `i` (`nig`, with onsets `n`, `sn`, `gn` and a following `g`), human coda `ck` (`cock`, `dick`, `sick`, `cack`), faerie onset `p` (`pee`, `piss`), dwarven nucleus `u` (`bum`, `turd`, `kunt`, `dung`) and orcish onset `sh` (`shag`). Removing one syllable (exact, settlements, offensive share before → after): goblin coda `tch` 3.88% → 3.25%, goblin onset `n` → 3.20%; human coda `ck` 1.92% → 1.21%; faerie onset `p` 1.95% → 1.10%; dwarven nucleus `u` 0.47% → 0.13%; orcish onset `sh` 0.62% → 0.32%. **Recommendation: no syllable edits.** Each edit removes at most half of a culture's hits, changes its sound and every name, and the filter is needed anyway (the goblin slur comes from three onsets and two codas around one nucleus). If Paul wants fewer redraws in one culture, dwarven `u` is the cheapest (it is the culture's only rounded vowel besides `o` and `ou`, so the sound would change most there; not recommended for that reason).

**Residual dictionary collisions** after filtering: the 50 most likely root parts per culture that are still words of the system English lists (probability per attempt, over the three kinds equally; most below 10^-5 each). Paul can judge whether any should join `words`:

| Culture | Residual share of names | Words | The 50 most likely, most likely first |
| --- | --- | --- | --- |
| `dwarven` | 0.012% | 59 | burkha, dunno, dingo, burka, bongo, bingo, karma, kinda, gamma, gumbo, gonna, grandma, tango, tundra, hairdo, nimbi, manga, mango, manna, mambo, mamma, momma, bargain, barman, barmaid, bantam, bunkum, bonbon, bombard, bodkin, bourbon, gunman, gourmand, drunkard, grandad, grammar, tartar, tartan, tankard, tannin, turban, turbid, turgid, harbor, harbour, hangar, handgun, handmaid, humdrum, margin |
| `elven` | 0.223% | 451 | allies, inner, lain, lair, lathes, lather, lases, laser, lanes, lamas, lames, lamer, laces, lacier, leis, lethal, lemon, level, lever, levies, lies, lion, lilies, lither, lifer, linen, lines, liner, limes, limier, limos, liras, liven, lives, liver, liefer, loan, loath, loin, loon, loses, loser, loner, loves, lover, local, their, theses, thesis, themes |
| `faerie` | 0.022% | 164 | into, ally, ani, any, anti, only, onto, pinto, pithy, piano, pansy, pony, poppa, poppy, filly, foully, folly, wino, wanly, lassie, lasso, silly, sally, sappy, sassy, soppy, tiny, tipsy, teeny, teensy, tally, tansy, tony, billy, ballsy, bani, banshee, bassi, basso, bounty, bony, bossy, shiny, shanty, whiny, whisky, wholly, flinty, flippy, floppy |
| `goblin` | 0.002% | 4 | kabob, bigot, nabob, zigzag |
| `human` | 0.040% | 628 | banded, banged, bantam, banter, badder, badger, badges, barbed, barber, barged, barges, barman, barrack, barren, barred, barrel, barter, bashed, bashes, basses, bassos, basted, bastes, balded, balsas, balsam, backward, bender, bedded, bedder, bedrock, bedstead, bested, bestir, belted, binder, binged, binges, bidden, bidder, birded, bilges, bimbos, bonbon, bonded, bonged, bongos, border, boredom, bossed |
| `orcish` | 1.750% | 20 | am, um, oz, tho, ma, mu, mo, do, magma, mammal, mamma, mambo, madam, momma, modal, muumuu, bozo, dado, dogma, dodo |

Fused words (root plus form letters, such as `{root}ford`) that are dictionary words: one, `Badminton` (human, probability below 10^-8).

**Gods before and after** (race ids in awakening order: elf, goblin, dwarf, faerie, human, orc; empty registry). Rule A is this amendment (`god_patterns` + filter); alternative B keeps `patterns` and raises gods' `min_letters` to 6 (with the filter):

| Seed | r4 (shipped 0.15.0) | r5 (god patterns + filter) | alternative B (min 6 letters + filter) |
| --- | --- | --- | --- |
| 0 | Saene, Kruskux, Nurnthaimm, Whaia, Steldhil, Bald | Saene, Kruskux, Nurnthaimm, Whaia, Steldhil, Uruld'kruum | Elaethienie, Kruskux, Nurnthaimm, Twaetwiss, Steldhil, Zaa'thash |
| 1 | Laesiel, Gragg-Gnurk, Khummhor, Synmia, Steck, Kaag | Laesiel, Gragg-Gnurk, Khummhor, Synmia, Steckwint, Kaag'azak | Laesiel, Gragg-Gnurk, Khummhor, Synmia, Dentgerd, Dold'azogh |
| 2 | Saeriemi, Gagg, Broumnoud, Biewie, Sten, Bogh | Saeriemi, Gagg-Sneegg, Broumnoud, Biewie, Stenedock, Bogh'grum | Saeriemi, Skigg-Grug, Broumnoud, Biewie, Daldbreas, Muugazag |
| 3 | Vaennela, Buzz, Nonthom, Pyia, Alis, Uguuggrush | Vaennela, Sluskat, Nonthom, Pyia, Bulwan, A'ghaz | Vaennela, Sluskat, Nonthom, Wafielou, Tentceas, Uguuggrush |
| 4 | Meven, Kukunk, Bornthirin, Wiabie, Hint, Ugu | Meven, Kukunk, Bornthirin, Wiabie, Dalcold, Ugu'azug | Aesaer, Kukunk, Bornthirin, Wiabie, Asheald, Ugaark |
| 5 | Fieniesie, Bat, Dirinbad, Sabiwhi, Tolashud, Grammam | Fieniesie, Batgnax, Dirinbad, Sabiwhi, Tolashud, Grammam | Fieniesie, Grigneetch, Dirinbad, Sabiwhi, Tolashud, Grammam |
| 6 | Ithielell, Drox, Groligu, Whaetwia, Dearbrol, Kaa'ghaag | Ithielell, Droxsloz, Groligu, Whaetwia, Dearbrol, Kaaghaag | Ithielell, Kakgog, Groligu, Whaetwia, Dearbrol, Kaa'ghaag |
| 7 | Vanir, Grux, Mammga, Ouwheefy, Gord, Maark'zaagh | Onveth, Buzork, Mammga, Ouwheefy, Gordman, Maarkzaagh | Onveth, Bogsnagg, Mammga, Ouwheefy, Gordbick, Maark'zaagh |
| 8 | Saelome, Bunkgnog, Mairnbud, Twybo, Breck, Krakkraa | Saelome, Grotbib, Mairnbu, Twybo, Madgil, Uush'muum | Saelome, Bunkgnog, Mairnbud, Wountie, Edimbron, Krakkraa |
| 9 | Lisael, Grink, Nurntond, Whaeouwhia, Gead, Kaalugogh | Lisael, Skozazz, Nurntond, Whaeouwhia, Geadashid, Um'daz | Lisael, Kreekrat, Nurntond, Whaeouwhia, Brirdbead, Kaalugogh |
| 42 | Fielie, Krok, Bronhak, Syfiash, Wealstuld, Kruu | Fielie, Krokzit, Bronhak, Syfiash, Wealstuld, Ogh'krak | Fielie, Gragdreex, Bronhak, Syfiash, Wealstuld, Ogh'krak |

| Culture | r4 mean letters | r4 share <= 4 letters | r5 mean | r5 share <= 4 | r5 min..max |
| --- | --- | --- | --- | --- | --- |
| elven | 6.70 | 5.6% | 6.72 | 5.3% | 4..11 |
| goblin | 6.53 | 23.7% | 8.06 | 0.0% | 5..10 |
| dwarven | 7.24 | 11.1% | 8.16 | 0.0% | 5..10 |
| faerie | 6.40 | 15.5% | 6.89 | 5.9% | 3..10 |
| human | 5.41 | 44.3% | 7.63 | 0.0% | 6..9 |
| orcish | 6.61 | 21.1% | 8.15 | 0.5% | 3..10 |

Rule A keeps every god that already had two syllables and lengthens the short ones from the same draws (`Krok` → `Krokzit`, `Steck` → `Steckwint`, `Bald` → `Uruld'kruum`, `Vanir` → `Onveth`); B replaces them with longer, often worse roots (`Brirdbead`, `Tentceas`). A leaves some English-looking human compounds (`Gordman`, `Dalcold`, `Stenedock`), since the human syllables are English by design.

**Elven realms before and after** (seeds 0–9, civilizations 1 and 2, empty registry): 8 of 20 were bare; after, none.

| Seed | Civ | r4 | r5 |
| --- | --- | --- | --- |
| 0 | 1 | Nife | The House of Nife |
| 0 | 2 | The Court of Nothind | The Court of Nothind |
| 1 | 1 | Glaefaenn | The House of Glaefaenn |
| 1 | 2 | The Court of Iethfe | The Court of Iethfe |
| 2 | 1 | Ithasfa | The House of Ithasfa |
| 2 | 2 | The Court of Ienriend | The Court of Ienriend |
| 3 | 1 | Roelaele | The House of Roelaele |
| 3 | 2 | Vefal | The House of Vefal |
| 4 | 1 | The Court of Meithiell | The Court of Meithiell |
| 4 | 2 | The Realm of Sices | The Realm of Sices |
| 5 | 1 | The Realm of Saethoth | The Realm of Saethoth |
| 5 | 2 | Vamiel | The House of Vamiel |
| 6 | 1 | The Realm of Mocaen | The Realm of Mocaen |
| 6 | 2 | Folisae | The House of Folisae |
| 7 | 1 | The Realm of Ienngleth | The Realm of Ienngleth |
| 7 | 2 | The Realm of Rasglae | The Realm of Rasglae |
| 8 | 1 | The Realm of Gliethil | The Realm of Gliethil |
| 8 | 2 | The Realm of Thalri | The Realm of Thalri |
| 9 | 1 | Motheme | The House of Motheme |
| 9 | 2 | The Realm of Ilcienn | The Realm of Ilcienn |

**Sample names of 5.1 under r5** (same protocol). The reference transcribes this table in place of 5.1's when r5 ships:

| Culture (c) | Settlements 1–20 | Civilizations 1–5 | Gods 1–3 |
| --- | --- | --- | --- |
| `dwarven` (1) | Moumka, Thoumdaili, Makmoudin Deep, Brummthu, Gromka, Kaird Deep, Khaili Hold, Kili, Thoum Hold, Nodin, Gondhark, Turnbruk Deep, Kidinkorn, Tharndramm Hold, Grangrak, Houmm Deep, Doknirk, Gali, Nailimim Deep, Tairdbra Hold | The Kourdbrar Throne; The Khandthra Throne; Clan Durgarin; Clan Drourd; Clan Drailimai | Brommgrai, Groumtoun, Nurnthaimm |
| `elven` (2) | Iensath, Onaeme, Ierrael, Elonoma, Fievos, Mielae, Asvathdell, Lafin, Laevo, Leithiedell, Orith, Voglael, Mievife, Iefann, Cellve, Fimo, Londcae, Rienel, Nathol, Siendnie | The House of Nife; The Court of Nothind; The House of Aesoth; The House of Riefifie; The Court of Thiesaell | Saene, Fiendse, Ithonnfa |
| `faerie` (3) | Liapa, Wiatwiesk Glade, Twamae Glade, Ybeeie Hollow, Biafloss Glade, Pieshaep Hollow, Teelan, Meeie Hollow, Sywysh Hollow, Iatysk, Wiepysk, Miemo Hollow, Mapoufee, Feetop Hollow, Showhie Hollow, Mawhiap Hollow, Athfli Hollow, Whywo, Fliaposk Hollow, Whosy Hollow | The Lyp Ring; The Watiafia Court; The Bouss Ring; The Wosk Ring; The Twiwhiwou Ring | Fiwhall, Fiabo, Biensho |
| `goblin` (4) | Krot Pit, Slograb Pit, Gnink-Snunk, Zux, Grunkgnuzz Pit, Grizz-Skeeb, Slank-Skrox, Drobskeet, Bozz Pit, Sluggatch Hole, Bux-Kreeb, Sniz, Slak, Zug-Gok, Gunitch, Skrizuz, Droggkazz Hole, Slobeerk, Snok, Kruskreezz Hole | The Gnazz Horde; The Baskeeb Mob; The Grodratch Horde; The Krunkgirk Horde; The Warrens of Gnonk | Zab-Skreeb, Kruskux, Nugnug |
| `human` (5) | Deanham, Alantton, Cickham, Gackwick, Brir, Tackbury, Gurdton, Studford, Ceambury, Buld, Silstudstead, Tirdwick, Dack, Stastint, Moldstead, Edumford, Tontstead, Ceanbir, Durdton, Durdbury | The Duchy of Ashem; The Kingdom of Ashos; The Kingdom of Edes; The Duchy of Ceam; The Duchy of Circon | Wasbrid, Histold, Edostold |
| `orcish` (6) | Kaak Fort, Zaarz Camp, Grush, Zaakugaag, Uruld'duuld, Shaazazuug, Kark Fort, Azol'urash Fort, Karzthogh, Uromgha, Maag, Uul'krakh Camp, Og'buurk Camp, Zaalduruz Camp, Mak, Duld'sha Fort, Shuum Fort, Ghorz Fort, Duuldkrosh Fort, Aag'urog Fort | Clan Kram'krorz; The Azakhdam Horde; The Urogh'krorz Horde; The Burk'zork Horde; The Warband of Maakurokh | Ug'krarz, Og'uraag, Uruuk'krush |

*r18: dwarven civilization 2 is "The Khandthra Throne" from 0.21.0 (15.2; before, "The Hold of Didin").*

**Vectors of 4.6 under r5**: seed 0 elven settlement 1 is unchanged (`Iensath`, collision `Focoth`); seed 42: god, dwarven (c 1), e 1 → **`Khorkhound`** (attempt 0; r4 `Khork`); `The Grork Horde`, `Deantwick` and `Ol'uraakh Fort` are unchanged.

**Region names (P1-06 r3).** The r5 lists were run over every word of the 15 `placename` files on `origin/p1/06-region-names` (837 distinct words) and every one-word compound `{prefix}{suffix}` and `{prefix}{feature}` over the union of all definitions (26,507, a superset of the reachable ones). `famous`: no hits. `slurs` + `crude`: 12 words and 542 compounds, almost all false positives on English: `night`, `knight`, `benighted` (`nig`), `sickle`, `sexton`, `tithe`, `pool`, `slag`, `gravedigger`, and compounds such as `Pinegrove` (`negr`), `Frostwater` (`twat`), `Bitterdale` (`terd`), `Boarseam` (`arse`). The compounds that do read badly are `Wanknoll`, `Wanknott`, `Wanknowe` (already rejected by P1-06's own `forbidden` entry `wank`), `Bonereach`, `Boneridge`, `Bonerigg`, `Bonerock`, `Bonereed` (`boner`) and `Dungate`, `Dungap`, `Dungrass`, `Dungrove`, `Dungill` (`Dun` + g…). `words` rejects 57 single words by design (region names are English). Every entry of P1-06's `forbidden` is already rejected by the r5 lists. **Recommendation: the region namer does not share this data**: its words are hand-written English, the history fragments are tuned for invented syllables and reject ordinary words, and P1-06 V4 and V5 already guard compounds. Instead P1-06's `common.forbidden` gains `boner` and `dung` in the same shipped change (a data edit of P1-06 r3; neither occurs in a single list word, so only the compounds above are affected).

**The names of the 0.16.0 sweep** (r6). Every distinct civilization, settlement and god name in the 375 worlds of the P3-06 validation sweep (`~/.cache/the-unmapped/out/p3-06-validation/sweep/`, 18,427 names; scan `~/.cache/the-unmapped/out/p3-08-spec/names/scan.py`), split by its culture's forms into root word and fixed text: the r5 rule rejects 1,001 (5.4%): dwarven 33 of 1,988, elven 37 of 3,629, faerie 73 of 2,278, goblin 564 of 5,550 (10.2%), human 193 of 2,935 (6.6%), orcish 101 of 2,047. Under r5 each of them is redrawn, and every later name of its world can move with it (the registry). **Every name the two world critics cite is rejected** (`~/.cache/the-unmapped/out/p3-08-spec/names/critic.py`, looked up in the critic and sweep worlds of P3-05 and P3-06): the goblin names written masked in the reviews, `N*gsnib Pit`, `Krob-Sn*gg`, `Drin*gg`, `Guz-Gn*gg`, `Nik-Gn*gg`, `Gn*gsneet Pit` (all by the `slurs` fragment of three letters), `Kreets*ut`, `S*utkeeb` (a `crude` fragment) and `Gn*zitch Hole` (a `crude` fragment); the crude and comic names `Dork`, `Turdstead`, `Cockford`, `Cockton`, `Cackford`, `Cickalolwick`, `Wardsickbury`, `Bumdou`, `Snotgab`, `But-Skrotch`, `Dog Fort`, `Baa Fort`, `Bag Hole`, `Pypy`, `Bap Glade`; the gods `Bald`, `Buzz`, `Gord`, `Vanir`. One cited name passes, `Sneezskrub Hole` (comic, not offensive; left to Paul's lists). Residual forms found by a scan of the names r5 keeps for other stems: goblin names ending in `snot` (`Geesnot Pit`, `Grunksnot`; `snot` is `at_start` only, so a word ending in it passes) and the faerie `Twypoo` (`poo` is `at_start` only). They are comic rather than offensive and stay as data for Paul to judge (11.10 question 1).

### 11.7 Tests required (for the test writer)

Data and loader:
1. The shipped `game/data/name_filters/` loads; the fixture copies are byte-identical; every list is sorted (a convention test, as P1-06's); a data root without `name_filters/` is a required-kind error.
2. Each check of 11.2 fails once with its message: a grammar failure (`Ni`, `a`, a 17-letter entry, `ab1`), `name_filter_unique`, `name_filter_distinct`, both branches of `name_filter_not_redundant`.
3. `naming_realm_forms` fails for `civilization_forms = ["{root}"]` and for `["{root}ia"]`; `god_patterns` missing, empty, or with a bad pattern fails as the other pattern fields do.
4. `names_form_text`: a culture with settlement form `{root}cock` and one with `Shit{root}` are fatal with the message of 11.3; `{root}ford` passes.

Rule (hand-made filter and naming data):
5. `root_word`: `The Hold of {root}` → the root; `{root}ford` → root + `ford`; `Ny{root} Pit` → `ny` + root.
6. `name_filtered` with anywhere `[ab]`, at_start `[cd]`, whole `[ef]`: `xaby` yes, `a-by` yes (across `-`), `xcdy` no, `xx-cdy` yes, `cd` yes, `ef` yes, `efx` no, `x'ef` yes, lower-casing in `root_word` (r7): form `E{root}` with root `f` gives the word `ef`, rejected as a `whole` word (r6 wrote `XEF`, which as `X{root}F` with root `e` is the word `xef` and matches no entry of the item); an empty filter rejects nothing.
7. With empty lists and `god_patterns` = `patterns`, every name of `p3-02-names.txt` (r4) is reproduced: r5 is r4 there.
8. An attempt rejected only by the filter has `valid` false and `filtered` true; it is skipped by `make_name`, and the fallback numbers the first unfiltered valid name (hand-made data where attempt 0 is filtered and every later valid name is in the registry). (r6) Redraw: with hand-made data where attempts 0 and 1 of entity 5 are filtered, the name is attempt 2's, and it equals what attempt 2 gives when entity 5 is named alone with an empty registry, whatever other entities were named before (the redraw depends on (kind, attempt, culture, entity) only).

Shipped data:
9. Property, per culture and kind, 2,000 names (seeds 0 and 1, as 9.2 item 8): no root word is filtered (invariant 5, recomputed by the test's own code of 11.3), every god root parses as one of its culture's `god_patterns` (so it has at least two nuclei with the shipped data), no civilization name is a bare root.
10. The critic's examples are rejected as root words of their forms: `cockford`, `cockton`, `cackford`, `bumdou`, `but-skrotch`, `snotgab`, `bag`, `dog`, `dork`, `baa`, `bald`, `buzz`, `gord`, `vanir`, the goblin slur example (`n` + `igg` + `snib`, written in the test as a concatenation) and its near misses `snig`, `drinigg`; and `stard`, `iensath`, `grork` pass. (r6) Also, each written in the test as a concatenation of its parts so that the source holds no offensive word: `turd` + `stead`; the goblin forms of P3-06's review `krob-` + `sn` + `igg`, `guz-` + `gn` + `igg`, `nik-` + `gn` + `igg`, `gn` + `ig` + `sneet`, `kreets` + `lut`, `s` + `lut` + `keeb`, `gn` + `azi` + `tch` (the review's spellings with the masked letter restored in the test, never in this text; each is rejected by an `anywhere` entry); and the false-positive guard: `The Realm of Arthos` is **not** filtered (the rule is per word).

Goldens:
11. `p3-02-names.txt` is regenerated by the updated reference (`p3_names_reference.py` gains 11.3 and 11.4, written from this text), with the scenarios of 9.3 plus `filtered` (item 8's case) and the r5 vectors of 11.6; the reference's `samples` command compares with the r5 table of 11.6.

### 11.8 Consequences

- **Generator version**: one bump for the combined follow-up change (region names, faerie weights, this amendment): 0.17.0 if P3-06 merges as 0.16.0.
- **Every world file changes**: `content_hash` (new required kind; new culture field), so `p0-worlds.txt`, `p1-worlds.txt` and `p3-worlds.txt` are re-recorded after the cross-preset comparison, and `p3-01-data.txt` (content hashes of the three data roots) is re-recorded; P3-01's spec listings of the culture files gain `god_patterns` and the elven form, and the listings test (P3-01 8.3 test 9) gains the `name_filter` files.
- **Names change**: every god whose r4 pattern had one nucleus, every name whose r4 root word is filtered, every bare elven realm, and every later name whose attempt or registry content moves with them. So `p3-02-names.txt`, `p3-03-chronology.txt` (god names), `p3-04-engine.txt` (sentences with god, civilization and settlement names), `p3-05-peoples.txt`, and every P3-06 or P3-07 golden that stores names or rendered text are regenerated by their references. The P1 stage goldens (`p1-01` to `p1-06`) hash layers and tables that do not change.
- **No schema change** (schema 7): tables and columns are unchanged; `docs/specs/schema.md` needs no edit.
- **P3-03**: its input checks gain `names_form_text` (11.3); its god naming call passes the filter. **P3-04, P3-05**: their `make_name` calls pass the filter. **P0-03**: the kind list gains `name_filter` and the grammar `filter_word` (kind-local). **P3-01**: the culture record gains `god_patterns` and the check `naming_realm_forms`.

### 11.9 Not addressed here

The critic's other name findings stay open for a later data or rule change: 43 of 176 elven settlements end in `-dell` and half of goblin settlements end in ` Pit` or ` Hole` (a per-world cap on a repeated suffix); same-core pairs in one world (`Skreek Pit` and `Skreek Hole`; the registry compares whole names); goblin and orcish stems overlap (`Grork`).

### 11.10 Open questions (for Paul; each with a recommendation and the interim choice)

(r8) All seven are decided (Paul, 2026-11-20): question 1 as below, questions 2 to 7 as recommended and shipped with P3-08.

1. **How wide is the English-word rule?** **Closed 2026-11-20 by Paul: the short list** ("the short reject list is correct, the happy accidents should be preserved"; `docs/design/proposals.md`, "Paul's decisions of 2026-11-20 on the name filter questions"). Specified in 11.11 (r8). The question as it was put: r5 rejects every ordinary English word the syllables reach with probability at least 10^-5 (647 words), in every kind, so roots are always invented words and English comes only from the forms (`Hold`, `Glade`). The cost: evocative bare roots go too (`The Kingdom of Gold`, `Thorn Deep`, `Moss Hollow`), and human realms are redrawn 16% of the time. The alternative is a hand-curated list of only the comic words (about 150). Recommendation and interim: the full list, as shipped in `words.toml`; Paul can delete any word he likes.
2. **Region names**: share the filter data or not? Recommendation and interim: not shared; P1-06's `common.forbidden` gains `boner` and `dung` (11.6).
3. **Syllable edits.** Recommendation and interim: none (11.6); the filter does the work.
4. **God style.** Recommendation and interim: rule A, `god_patterns` per culture, shipped as the culture's multi-syllable shapes. B (a minimum of six letters) is the alternative; a domain epithet ("Krok the Gnawer") waits for P5's pantheon.
5. **The elven third realm form.** Recommendation and interim: `The House of {root}`. Alternatives: `The Kingdom of {root}` (human already uses it), `The Hidden Realm of {root}`, `{root} under the Leaves`.
6. **Orcish two-letter apostrophe parts** (`Am'gash`, `Um'daz`, `Do'grak`) are allowed; 1.75% of orcish names have one. Recommendation and interim: keep them.
7. **The lists are public text** in a public repository. Recommendation and interim: plain short fragments, as shipped. Storing only hashes of the fragments would need every substring of every root word hashed at match time and would make the lists impossible to review or edit by hand.

### 11.11 Amendment r8: the short English-word list (Paul's decision; ships after P3-08)

**Decision.** Paul, 2026-11-20, on 11.10 question 1: "the short reject list is correct, the happy accidents should be preserved" (`docs/design/proposals.md`, "Paul's decisions of 2026-11-20 on the name filter questions"). This revision replaces the content of `game/data/name_filters/words.toml` (and its byte-identical fixture copies `worldgen/tests/data/geo/name_filters/words.toml` and `worldgen/tests/data/geo-small/name_filters/words.toml`). Nothing else changes: the rule of 11.3, the fields and checks of 11.2, the lists `slurs`, `crude` and `famous`, the cultures, the code and the schema stay as shipped with P3-08 (generator 0.17.0).

#### 11.11.1 The rule for `whole` entries of `words`

A word is on the `words` list only when a god, realm or settlement named with it (as the bare root, or as a part of the root word split at `'` and `-`) reads as one of:

| Reason | What it covers | Examples of names it prevents |
| --- | --- | --- |
| C crude or bodily | sexual double meanings, bodily functions, body parts | `The Realm of Nose`, `Gut Pit`, `Zit` |
| J comic, childish or slang | interjections and noises, nicknames and insults, baby talk, slang, comic food and household things | `Dork`, `Bag Hole`, `Buzz`, `The Sock League`, `Whoa` |
| M modern or anachronistic | technology, money and trade, modern slang and abbreviations, real-world titles and dress, modern first names, famous names | `The House of Movie`, `The Rent League`, `Clan Khan`, `The Bob Horde` |
| F function words, inflected forms, plain slips | pronouns, conjunctions, prepositions, determiners, numbers, modal and past forms, `-ly` adverbs, and plain verbs and adjectives with no reading as a place | `The Kingdom of Went`, `The Duchy of Told`, `Old'sho` |

In short: **crude, comic or modern-sounding words, plus the words whose names read as an error**. Every other English word is a permitted root, in particular materials, plants, animals, terrain, weather, colours, weapons, grim words, archaic and rural words (`The Kingdom of Gold`, `Thorn Deep`, `Moss Hollow`, `Clan Horn`, `Maim Hold`, `Grub`, `Sleet Pit`, `Wild`), and **a neutral word in doubt is permitted** (that is the decision: happy accidents are preserved). Class F is part of "comic": a realm called `The Kingdom of And` reads as a generator error, not as a name, so function words and inflected forms stay rejected; plain adjectives that read as a description of a place (`bold`, `grand`, `lone`, `tall`, `thin`, `wild`) are released, and `old` is kept (11.11.6).

The candidate words are the 647 of r5 (every lower-case word of the system English lists of three or more letters that a shipped culture reaches as a root part with probability at least 10^-5 per attempt, plus 14 comic words added by hand). Each was judged by the rule above; a word the cultures cannot reach does no harm and needs no entry. After a syllable edit, the reach measurement of 11.6 gives the new candidates, and each is judged by the same rule.

**The shipped list** (normative): 393 entries, all from the 647; 254 released (`mold` restored to the list by the lead's interim choice, 2026-11-20, after the measurements of 11.11.3). Sizes of 11.2 become `words` 0, 0, 393; effective `whole` 514. Every entry and its reason, and every released word, are in 11.11.7. No entry is itself offensive (offensive fragments live in `slurs` and `crude`), so the spec writes every word plainly; none needed masking.

**Loader checks.** The new list passes `name_filter_unique`, `name_filter_distinct` and `name_filter_not_redundant` (it is a subset of a list that passed them; removing entries cannot create a repeat or a covering entry). It stays sorted, so the convention test of 11.7 item 1 holds.

#### 11.11.2 How the list was made

Spec writer, 2026-11-20 (`~/.cache/the-unmapped/out/p3-10-words/`: `decide.py` the decisions and reasons, `classes.py` the classes of released words, `writetoml.py` the file, `reach.py` the reach of each word per culture and kind by exact enumeration, `rates.py` the exact redraw rates, `gen.sh` and `worlds.py` the world comparison, `sample.py` the keyed-draw sample, `review.py` the review file `word-decisions.md`). Each of the 647 words was judged once by the rule; then the released words were read in the names of the 50 worlds of 11.11.3 and of the keyed-draw sample, and 31 words whose names read badly there were put back (`The Duchy of Stick`, `Drink Pit`, `Bit Hole`, `Grab Hole`, `The Duchy of Bent`, `The Realm of Loci`, `The House of Mile`, `The Tick League`, `Beard`, `Old'zarz Camp`, `The Kingdom of Curd`, `The House of Lien`, ...; the 31 are listed in 11.11.7). The figures below are for the final list: the sample and the exact rates were computed with it; the 50 worlds were generated with the list before its last three put-backs (`curd`, `bead`, `lien`, found in the keyed-draw sample), and no name in those worlds has one of the three as a part of its words, so the final list gives the same names (only `content_hash` differs).

#### 11.11.3 Measurements

**Exact redraw rates** (share of the attempts that pass the r4 rules and that the filter rejects, by enumeration of each culture's root space as in 11.6; gods with `god_patterns`). "full" is r5 to r7 (647 words), "short" is r8 as measured (392 words, `mold` released; the shipped 393 differ by `mold` only), "none" is the filter with no `words` list at all (slurs, crude and famous only):

| Culture | God full / short / none | Civilization full / short / none | Settlement full / short / none |
| --- | --- | --- | --- |
| dwarven | 0.53% / **0.53%** / 0.53% | 2.89% / **1.73%** / 0.86% | 2.89% / **1.73%** / 0.86% |
| elven | 1.58% / **1.41%** / 1.19% | 1.58% / **1.41%** / 1.19% | 1.46% / **1.33%** / 1.17% |
| faerie | 2.19% / **2.16%** / 2.03% | 5.17% / **3.89%** / 2.08% | 5.17% / **3.89%** / 2.08% |
| goblin | 11.89% / **10.16%** / 4.93% | 12.96% / **10.78%** / 4.38% | 12.96% / **10.78%** / 4.38% |
| human | 3.49% / **3.49%** / 3.49% | 19.12% / **12.94%** / 3.85% | 5.13% / **4.25%** / 2.95% |
| orcish | 7.57% / **4.97%** / 0.96% | 6.49% / **4.38%** / 0.89% | 6.49% / **4.38%** / 0.89% |

The full-list figures reproduce 11.6's table to within 0.01 points (the same enumeration; `rates.py`, short-list rows of the last three cultures by `rates_short.py`, the same computation for the short list only; a culture whose civilization and settlement forms put no letters beside the root has equal rates for the two kinds). Human realms, redrawn 19.12% of the time with the full list (16.3 points of it from `words`), are redrawn 12.94% of the time with the short list.

**Worlds** (seeds 0–49, map size 1024, default settings, `worldgen-cli` 0.17.0 built with `linux-gcc-release` from this branch; each seed generated with the shipped data and with the data root whose `words.toml` is the short list; `gen.sh`, `worlds.py`): 5761 named entities (gods, civilizations and settlements); **79 names differ (1.37%)**; the event count is equal in all 50 worlds (names do not steer the simulation). A name is counted as redrawn when its first attempt that passes the r4 rules is rejected by the filter (attempts recomputed with the reference, `p3_names_reference.py`, which gives the world's own name for 5755 of the 5761 entities; the others are registry repeats): 427 (7.41%) with the full list, **363 (6.30%)** with the short list.

| Culture | Kind | Names | Changed | Redrawn, full | Redrawn, short |
| --- | --- | --- | --- | --- | --- |
| dwarven | god | 50 | 0 | 0.00% | 0.00% |
| dwarven | civilization | 74 | 1 | 4.05% | 2.70% |
| dwarven | settlement | 366 | 4 | 3.55% | 2.46% |
| elven | god | 50 | 0 | 2.00% | 2.00% |
| elven | civilization | 83 | 0 | 1.20% | 1.20% |
| elven | settlement | 1048 | 0 | 1.62% | 1.62% |
| faerie | god | 50 | 0 | 2.00% | 2.00% |
| faerie | civilization | 65 | 0 | 3.08% | 3.08% |
| faerie | settlement | 448 | 1 | 2.68% | 2.68% |
| goblin | god | 50 | 1 | 8.00% | 8.00% |
| goblin | civilization | 153 | 2 | 13.07% | 11.76% |
| goblin | settlement | 2034 | 56 | 13.77% | 11.50% |
| human | god | 50 | 0 | 2.00% | 2.00% |
| human | civilization | 76 | 3 | 15.79% | 14.47% |
| human | settlement | 754 | 5 | 5.17% | 4.64% |
| orcish | god | 50 | 1 | 8.00% | 6.00% |
| orcish | civilization | 54 | 2 | 5.56% | 1.85% |
| orcish | settlement | 306 | 3 | 4.58% | 3.59% |

The redraw figures of the worlds are shares of names, the exact rates above shares of attempts; human civilizations: 15.79% of the 76 realms needed a redraw with the full list, 14.47% with the short one.

**Every name of the 50 worlds that the short list lets through** (79; the full list's name in brackets), by culture:

- dwarven (5): Brim [Gronkhai]; Hind Deep [Thrain]; Maim Hold [Durgrili Deep]; Clan Horn [The Boundgri Throne]; Brim Deep [Thrurndra].
- faerie (1): Wash Glade [Amee Glade].
- goblin (59): Nix Hole [Slaskrig Pit]; Geerk-Drub Pit [Grak Hole]; Bark-Nux Pit [Graz Pit]; Skrizz-Slat [Slitch Hole]; Slink Hole [Dritch Pit]; Bark [Greekgragg Pit]; The Skiz-Sleet Horde [The Warrens of Girkkat]; Grit Pit [Skizkrirk]; Grub [Koggnotch Pit]; Slink [Snazz Hole]; Slug [Slazzskit Hole]; Grub Hole [Slabgnob]; The Slink Horde [The Krax Horde]; Bog Hole [Greetch Pit]; Slat Hole [Neezz Pit]; Drab-Slitch [Sneezkrox Hole]; Krub-Drub [Sluzz Pit]; Beez-Grit Hole [Gnoskrog Hole]; Slat [Sleeg]; Grub [Gneesnuz]; Bog [Bak-Gnunk Hole]; Drab Pit [Nozz Pit]; Bark-Grirk [Drib-Skib]; Skreex-Slog [Gruzsnut]; Slub-Nix [Slinug Hole]; Slog-Gutch [Bax Hole]; Kab-Slog Pit [Sleezkreex Pit]; Notch [Sneegreeg]; Sligg-Slab [Gox]; Gnat-Gix Pit [Kadreegg Hole]; Bat-Kagg [Krurk Pit]; Slat Hole [Dreex-Grik]; Nub Pit [Snugg Hole]; Slink [Keetchgnat Hole]; Gnut-Snag [Kazank]; Kit Pit [Skutch-Gib]; Slab Pit [Gnaz]; Drab-Skroz Pit [Nuggzutch Hole]; Kit [Kig]; Grub-Gnag [Krixdreeg]; Druk-Slog [Slazz-Drurk Hole]; Gigg-Nix Hole [Skutch]; Kit [Neezkrark Hole]; Gugg-Slog Pit [Druzzsnib]; Bat Pit [Skraslirk]; Grurk-Drab Pit [Gneex-Beenk Hole]; Sleek Pit [Skeekax]; Sleet Pit [Gix-Slaz]; Grub [Kazz]; Kit [Drux-Zeek Pit]; Sleet [Gnabbogg]; Slab-Slirk Pit [Skidrot Hole]; Slank-Bog Hole [Snib Hole]; Notch-Gank Hole [Skrubskrab Pit]; Grub [Skrax Pit]; Grub [Skrork Hole]; Bog-Krirk Hole [Dritzark Pit]; Slab-Kob [Slinab]; Sleet-Krag Pit [Gnub-Neenk Hole].
- human (8): Wild [Aleck]; The Rack League [The Kingdom of Hald]; Beck [Asheadford]; The Duchy of Mold [The Duchy of Breld]; Seam [Borbeasford]; The Mold League [The Kingdom of Aluck]; Ward [Stant]; Herd [Hordcintton].
- orcish (6): Ark'groz [Uguukh'mosh]; The Uush'mold Horde [The Shuugh Horde]; Ork'murk Camp [Azald]; Kruu'mold [Ogh'krak]; Daa'dam Fort [Maak'ghuu]; Clan Uukh'shark [Clan Uurz'daald].

**Keyed-draw sample** (`sample.py`: per culture, seeds 0–199, entities 1–60, the three kinds, empty registry, 36,000 names per culture): the share of names whose first valid attempt the full list rejected and the short list keeps, the most frequent released words, and the first 14 such names with distinct words:

- dwarven: 285 of 36000 names released (0.79%); top words hind 11, gird 11, bound 11, hound 10, maim 10, murk 10, maid 10, dark 9. The Hold of Barn (civ); Clan Thrum (civ); Dark (set); Hound (set); Torn Deep (set); Hark Deep (set); Grin Deep (set); The Hold of Braid (civ); Clan Maim (civ); Clan Mound (civ); Dour (set); Murk (set); Bird Hold (set); Mourn (set).
- elven: 48 of 36000 names released (0.13%); top words rose 3, mesa 2, lava 2, aerie 2, sera 2, lithe 2, cave 2, lace 2. Mesa (god); Fife (god); Lone (god); The Realm of Lava (civ); The Court of Lathe (civ); Aerie (god); Sera (set); Lithe (set); The Court of Rose (civ); Rise (god); Cave (god); Lace (set); Mace (god); Vine (god).
- faerie: 345 of 36000 names released (0.96%); top words flee 17, boll 14, ash 12, mien 11, teeth 11, wall 10, lath 9, lash 9. Boll (set); The Wish Court (civ); Bola (set); Wash Glade (set); Man Glade (set); Sin (set); The Circle of Lath (civ); Twin Hollow (set); Mask Hollow (set); Ship Hollow (set); The Circle of Fish (civ); Tin Glade (set); The Bee Court (civ); The Wall Ring (civ).
- goblin: 807 of 36000 names released (2.24%); top words drab 51, gnat 49, kit 48, bark 47, snag 47, nub 46, slug 46, sleet 44. Nub Pit (set); Nix Hole (set); The Warrens of Kit-Slirk (civ); Geerk-Drub Pit (set); Bark-Nux Pit (set); The Slug-Bugg Mob (civ); The Warrens of Gnat (civ); Slak-Grub (set); Skrizz-Slat (set); Slink Hole (set); Zok-Slog (god); The Warrens of Snag-Drirk (civ); The Slab Horde (civ); Zig-Sleet Pit (set).
- human: 955 of 36000 names released (2.65%); top words dead 29, bear 27, mead 26, bold 25, alum 24, cold 23, steam 23, gild 23. The Duchy of Wick (civ); The Ward League (civ); The Kingdom of Brim (civ); The Kingdom of Stead (civ); The Herd League (civ); The Kingdom of Cold (civ); The Mead League (civ); The Kingdom of Bard (civ); The Kingdom of Rock (civ); The Rack League (civ); The Kingdom of Weal (civ); Sear (set); The Duchy of Gird (civ); The Kingdom of Beam (civ).
- orcish: 855 of 36000 names released (2.38%); top words ark 165, ash 129, murk 66, mold 60, bold 57, bark 55, dark 55, dash 51. Uu'mold Fort (set); The Warband of Murk (civ); The Dam Horde (civ); The Ash'uguuk Horde (civ); The Warband of Mogh'bold (civ); Shaash'bark (god); Bush'gruush (set); The Warband of Dash'ugaagh (civ); The Ak'dark Horde (civ); Mak'bog (god); Ark'graa (god); Shark'ghuuz (god); Bash'gro (set).

(The samples above were drawn with `mold` released; with the shipped list `Kruu'mold`, `The Uush'mold Horde` and the other `mold` names are redrawn.)

**Names that read badly** in the first reading of these samples put their word back on the list (11.11.2, 31 words). Of the names above, none reads as crude or comic to the spec writer; the weakest are `The Mold League` and `The Duchy of Mold` (human; `mold` is kept released, 11.11.6), `Man Glade`, `Boll` and `The Kingdom of Beam`.

**The sample table of 11.6** (5.1's names under r5) changes in one place with the shipped list of r8 (393 words), as the test writer's reference golden on `origin/p3/09-int` (`worldgen/tests/golden/p3-02-names.txt`) records it: human civilization 4 `The Duchy of Ceam` becomes **`The Duchy of Wick`**. Orcish settlement 6 stays `Shaazazuug` (with `mold` released, while 11.11.3 was measured, it had become `Uu'mold Fort`), and the seed 42 orcish god stays `Ogh'krak` at attempt 1. Every other name and the vectors of 4.6 are unchanged.

#### 11.11.4 Consequences

- **Data change only.** `words.toml` and its two fixture copies change; no C++ code, no reference rule, no loader check, no culture file changes. **No schema change** (schema 7; `docs/specs/schema.md` unchanged).
- **Every world hash changes**: the data root's `content_hash` covers the file (P0-03 8), so every world file changes, the P0 empty worlds included: `p0-worlds.txt`, `p1-worlds.txt` and `p3-worlds.txt` are re-recorded after the cross-preset comparison, and `p3-01-data.txt` (content hashes of the three data roots: shipped, `geo`, `geo-small`) is re-recorded by `p3_data_reference.py`.
- **Names change** where a root word's only rejecting entry was a released word, and every later name of the world that moves with it through the registry (4.4); the simulation does not read names, so no event changes (11.11.3: event counts equal in all 50 worlds). So the name and text goldens are regenerated by their references: `p3-02-names.txt` (`p3_names_reference.py golden`: the r5 sample table of 11.6 and its scenarios), `p3-03-chronology.txt` (god names), `p3-04-engine.txt`, `p3-05-peoples.txt` and `p3-06-conflict.txt` (rendered sentences and name columns), and any P3-07 golden that stores names. The P1 stage goldens (`p1-01` to `p1-06`) hash layers and tables that do not change.
- **Generator version**: the change alters world bytes, so it needs a bump in the change that carries it. It ships with the next generator change after P3-08: with **P3-09 "conflict balance" as 0.18.0** (one bump, one re-recording of the goldens), or, **if P3-09 is delayed, alone as 0.17.1** (a data-only bump; the same golden re-recording). The lead chooses; the data file of this branch is ready for either.
- **11.10 question 1** is closed (above); 11.6's r5 figures stay as the record of the full list; the r8 figures are in 11.11.3.

#### 11.11.5 Tests to update (for the test writer)

Tests that encode the list's size or members:

1. `worldgen/tests/history/history_names_data_test.cpp`, the shipped-filter test (11.2 sizes per definition): `{0, 0, 647}` for `words` becomes `{0, 0, 393}`.
2. `worldgen/tests/reference/p3_data_reference.py` (the `name_filter` sizes check, `words=(0, 0, 647)`): becomes `(0, 0, 393)`; then `p3-01-data.txt` is re-recorded from it.
3. `worldgen/tests/golden/p3-02-names.txt`: regenerated by `p3_names_reference.py golden` (it reads the shipped `name_filters/`); its `filter` lines are unchanged in value (the critic stems `bag`, `dog`, `baa`, `gord`, `vanir`, `dork`, `bald`, `buzz` are all still rejected, and the benign words still pass), so 11.7 item 10's C++ list in `history_names_test.cpp` needs no edit.
4. 11.7 item 1 (sorted lists, byte-identical fixtures) and item 9 (no root word filtered, recomputed by the test's own rule from the shipped files) need no edit: they read the files.
5. New: a check of 11.11.1's anchors with the shipped filter (C++ and the reference): the bare root words `gold`, `thorn`, `moss`, `iron`, `ash`, `grub`, `sleet`, `horn` are **not** filtered; `also`, `went`, `movie`, `dork`, `zit`, `old` **are**. The words are written plainly; none is offensive. (No test should pin the whole list: it is hand data that Paul edits.)
6. The world goldens and stage goldens of 11.11.4, re-recorded by the validation specialist after the cross-preset comparison, never to make a test pass.

The engineer's part: none in code. The change is the data file (this branch), the generator version bump and the version comment in `worldgen/include/worldgen/version.hpp`, in the change that ships it.

#### 11.11.6 Choices made in doubt

Paul may move any of these; each is one line of `words.toml`.

- `old`: kept (reject). Plain adjective; `Old Fort` and `The Old Horde` read well, but the orcish compounds the syllables make most often (`Old'sho`, `Old'zarz Camp`; `old` is 0.6% of orcish god attempts) read as an English word glued to an orcish one.
- `man`: released. A realm `The Kingdom of Man` would be a happy accident; `Man Glade` (faerie, in the sample) is weaker.
- `mine`: released. Reads as the dwarven noun (`Mine Deep`), not the pronoun.
- `mold`: **kept** (restored to the list by the lead's interim choice, 2026-11-20; it was released when 11.11.3 was measured). `The Mold League` and `The Duchy of Mold` read as household mould.
- `teeth`: released. A grim word that fits goblins and orcs (`Teeth Pit`); the other body parts are kept.
- `beard`: kept (put back). A human town called `Beard` reads as a joke.
- `lay`: kept. The ballad sense fits, but the crude modern sense is the common one.
- `bent`: kept (put back). `The Duchy of Bent` reads as an error, and the word is slang.
- `muck`: kept (put back). `The Kingdom of Muck` is comic; goblin `Muck Pit` would have fitted.
- `runt`: released. Goblins as runts is a fitting accident (`The Runt Horde`).
- `bash`: released. Orcish `The Bash Horde` reads as a blow, not a party.
- `kit`: released. Goblin `Kit`, `Kit Pit`; the modern sense (a kit) is weak in a bare name.
- `sheep`: released. Rural (`Sheep Hollow`).
- `grog`: kept. A stock troll name and pirate slang; comic.
- `khan, emir, amir, pasha, lama, sari`: kept. Real-world titles and dress make a realm read as a real place.
- `ales, afar, alum, ashy, iris, life, rise, pity, wish`: released, in doubt: each has a reading that fits the setting.

#### 11.11.7 Appendix: every word, kept and released

The review file `~/.cache/the-unmapped/out/p3-10-words/word-decisions.md` holds the same lists. To move a word, edit `words.toml` (sorted) and say so in a revision note.

##### Kept on the reject list (393)

**Crude or bodily (29)**: sexual double meanings, bodily functions, and body parts, whose bare names read as a joke ("The Realm of Nose", "Gut Pit").

`bap`, `beard`, `brain`, `bras`, `burp`, `come`, `face`, `gob`, `gut`, `hair`, `hand`, `head`, `kink`, `lay`, `lice`, `lip`, `mouth`, `nose`, `oral`, `pap`, `rear`, `shin`, `slit`, `sore`, `stud`, `suck`, `tibia`, `wee`, `zit`.

**Comic, childish or slang (126)**: interjections and noises, nicknames and insults, baby talk, slang, and comic food and household things ("Dork", "Bag Hole", "The Sock League", "Whoa"; `mold`, restored by the lead's interim choice: "The Mold League" reads as household mould).

`aces`, `alas`, `baa`, `baby`, `bag`, `bald`, `ball`, `bead`, `bean`, `beefy`, `beep`, `beet`, `bib`, `big`, `bloke`, `bob`, `boo`, `bop`, `bosh`, `botch`, `box`, `boy`, `bro`, `bug`, `bunk`, `butch`, `buzz`, `curd`, `darn`, `dog`, `dork`, `drain`, `drink`, `drunk`, `duck`, `dud`, `dude`, `dweeb`, `fatso`, `fie`, `fishy`, `flan`, `flap`, `flashy`, `flip`, `flop`, `fly`, `foo`, `fop`, `gab`, `gag`, `geek`, `geez`, `gent`, `gonk`, `goof`, `gord`, `grab`, `grog`, `gunk`, `heck`, `hick`, `lame`, `lard`, `lousy`, `love`, `mama`, `mash`, `mime`, `mold`, `mom`, `moo`, `mop`, `mousy`, `muck`, `mug`, `mum`, `mush`, `nab`, `nag`, `nark`, `nerd`, `nit`, `nut`, `ogle`, `ogles`, `papa`, `pie`, `pop`, `posh`, `poss`, `pypy`, `rant`, `sass`, `sham`, `shoo`, `showy`, `shush`, `skunk`, `slob`, `snark`, `snit`, `snitch`, `snob`, `snub`, `snug`, `sock`, `soup`, `soupy`, `stick`, `stir`, `teas`, `thud`, `tick`, `toss`, `toupee`, `toy`, `twee`, `ugh`, `vase`, `wack`, `weepy`, `whisk`, `whoa`, `win`, `woo`.

**Modern or anachronistic (116)**: technology, money and trade, modern slang and abbreviations, real-world titles, modern first names, famous names ("The House of Movie", "The Rent League", "Clan Khan", "The Bob Horde").

`alien`, `altho`, `amen`, `amir`, `anon`, `bank`, `batch`, `bath`, `bill`, `bin`, `bit`, `boss`, `bot`, `brad`, `bran`, `bunt`, `card`, `case`, `cent`, `cola`, `coma`, `cont`, `deal`, `dean`, `deck`, `doz`, `drag`, `drug`, `emir`, `emos`, `fan`, `flash`, `floss`, `flyby`, `foci`, `fora`, `gear`, `gig`, `grad`, `gram`, `grid`, `grok`, `hack`, `hock`, `info`, `khan`, `lama`, `lase`, `lien`, `limo`, `lira`, `lire`, `loci`, `loco`, `male`, `mall`, `map`, `mark`, `mass`, `math`, `meld`, `meme`, `memo`, `min`, `mini`, `moll`, `mono`, `movie`, `nib`, `nova`, `novae`, `oval`, `pan`, `pasha`, `pay`, `pill`, `pita`, `poi`, `poll`, `polo`, `rave`, `ream`, `rent`, `rick`, `role`, `sale`, `sari`, `semi`, `shill`, `shop`, `silo`, `sine`, `skeet`, `skit`, `slot`, `sofa`, `soli`, `solo`, `soy`, `stack`, `stent`, `stock`, `stunt`, `team`, `tee`, `teen`, `teepee`, `theme`, `thug`, `tie`, `tipi`, `tour`, `typo`, `visa`, `viva`, `weld`.

**Function words, inflected forms and plain slips (122)**: pronouns, conjunctions, prepositions, numbers, modal and past forms, and plain verbs and adjectives with no reading as a place ("The Kingdom of Went", "The Duchy of Told", "Old'sho").

`all`, `also`, `amend`, `ashed`, `ask`, `aver`, `back`, `been`, `bent`, `born`, `both`, `bred`, `brunt`, `but`, `came`, `care`, `dear`, `dint`, `drank`, `dug`, `either`, `else`, `emend`, `even`, `ever`, `fill`, `fine`, `five`, `fore`, `gain`, `got`, `greet`, `ground`, `hard`, `heal`, `hear`, `heard`, `held`, `hint`, `hold`, `hour`, `ices`, `icier`, `kind`, `lie`, `lii`, `line`, `lop`, `lose`, `main`, `may`, `mean`, `meant`, `mild`, `mile`, `mind`, `miss`, `more`, `move`, `name`, `nice`, `niece`, `nine`, `none`, `not`, `noun`, `old`, `once`, `ones`, `other`, `over`, `rare`, `read`, `real`, `rife`, `rile`, `safe`, `same`, `sane`, `save`, `say`, `see`, `seen`, `sent`, `shall`, `shy`, `shyly`, `sip`, `slunk`, `sold`, `some`, `son`, `south`, `steal`, `stint`, `stuck`, `stun`, `than`, `there`, `these`, `thieve`, `thine`, `third`, `those`, `told`, `ton`, `too`, `top`, `two`, `want`, `way`, `wean`, `wear`, `went`, `who`, `why`, `will`, `with`, `won`, `wont`, `word`, `you`.

##### Released: allowed as name roots (254)

**Materials, minerals and metals (15)**: `alum`, `ash`, `ashen`, `ashes`, `ashy`, `brick`, `gold`, `grit`, `iron`, `lava`, `lime`, `mica`, `ores`, `slab`, `tin`.

**Plants and growing things (16)**: `bark`, `cane`, `gourd`, `grain`, `iris`, `ivies`, `lily`, `mint`, `moss`, `osier`, `posy`, `rose`, `sap`, `stem`, `thorn`, `vine`.

**Animals (22)**: `bat`, `bear`, `bee`, `bird`, `boa`, `buck`, `fish`, `gnat`, `grub`, `herd`, `hind`, `horn`, `hound`, `mane`, `mare`, `mice`, `mole`, `moth`, `shark`, `sheep`, `slug`, `vole`.

**Terrain, water and places (26)**: `bay`, `beck`, `bog`, `cave`, `cove`, `dam`, `dock`, `isle`, `lane`, `lee`, `mere`, `mesa`, `mill`, `mire`, `mound`, `nave`, `pass`, `path`, `seam`, `seas`, `ship`, `stead`, `tent`, `vale`, `wall`, `wick`.

**Weather, fire, light and colour (18)**: `burn`, `cold`, `dark`, `drab`, `eves`, `fire`, `glare`, `morn`, `murk`, `rime`, `sheen`, `sleet`, `star`, `steam`, `tan`, `teal`, `tint`, `wan`.

**Weapons, war and the hunt (17)**: `bind`, `bola`, `brand`, `dirk`, `drum`, `flay`, `gird`, `hunt`, `lash`, `mace`, `maim`, `oath`, `rack`, `sack`, `toll`, `ward`, `whip`.

**Epithets of place (plain adjectives that read as a description) (6)**: `bold`, `grand`, `lone`, `tall`, `thin`, `wild`.

**Grim and dark words (15)**: `dead`, `dour`, `evil`, `grim`, `loss`, `mourn`, `omen`, `pall`, `sere`, `sin`, `tear`, `torn`, `vice`, `vile`, `weep`.

**Myth, lore and archaic words (29)**: `aeon`, `aerie`, `aeries`, `afar`, `ales`, `ark`, `bard`, `dram`, `eras`, `ether`, `ethos`, `fife`, `geld`, `gild`, `hark`, `lithe`, `lore`, `loth`, `mead`, `mien`, `myth`, `piety`, `rove`, `sire`, `sleek`, `slink`, `thrum`, `weal`, `wily`.

**Rural life and craft (25)**: `barn`, `beam`, `braid`, `bread`, `cone`, `cord`, `flask`, `glove`, `lace`, `lad`, `lass`, `lath`, `lathe`, `maid`, `man`, `meal`, `oven`, `pin`, `rice`, `sash`, `sieve`, `tack`, `tap`, `tip`, `twill`.

**Plain words let through in doubt (65)**: neutral nouns, verbs and adjectives with a fitting or harmless reading as a name, and unusual forms: `angle`, `ban`, `band`, `bash`, `bask`, `bass`, `boll`, `bond`, `bound`, `brim`, `bush`, `cant`, `core`, `dash`, `dent`, `drub`, `fall`, `fame`, `fare`, `fee`, `file`, `fin`, `flee`, `grin`, `grind`, `icon`, `ill`, `kit`, `lap`, `life`, `limy`, `live`, `mask`, `mine`, `mock`, `nix`, `notch`, `nub`, `pip`, `pith`, `pity`, `race`, `rise`, `rock`, `runt`, `seal`, `sear`, `seep`, `sera`, `sill`, `slat`, `slog`, `snag`, `sole`, `sop`, `task`, `teeth`, `till`, `tuck`, `turn`, `twin`, `vane`, `vise`, `wash`, `wish`.

##### Put back after reading the sample (31)

Released at first, then returned to the reject list because a name in the measurement sample (11.11.3) read badly: `bead`, `beard`, `bent`, `bit`, `bunt`, `curd`, `deck`, `drain`, `drink`, `flap`, `foci`, `fora`, `gent`, `grab`, `hard`, `hock`, `lien`, `loci`, `mass`, `mile`, `muck`, `nib`, `novae`, `old`, `sham`, `soli`, `stick`, `stir`, `stock`, `tick`, `weld`. They are counted above.

## 12. Amendment r10: comic names and near-identical names (P3-10, generator 0.19.0)

Normative for the change P3-10 (`docs/specs/p3-10-followups.md`), from generator **0.19.0**, not before; written after the world critic's review of P3-08 (`docs/reviews/p3-08-followups-world-critic.md` F6). Two parts: entries added to the filter lists (data) and a uniqueness rule on a name's core in the history stage (rule). No schema change.

### 12.1 Filter entries (data; `game/data/name_filters/` and both fixture copies)

The review lists names that read as comic or English by accident. Each was checked against 59,400 names drawn with the shipped 0.18.0 data and lists (the names reference `worldgen/tests/reference/p3_names_reference.py`, `make_name` for entities 1 to 1,500 of each culture and kind, 1 to 300 for gods, seeds 0, 1 and 2; script `~/.cache/the-unmapped/out/p3-10-spec/an/frag.py`): a general fragment is added where every name it would reject reads as the critic's example does, and a one-off is left where the fragment is a culture's own syllable.

| Name (world, 0.17.0) | Entry | List | Names of the 59,400 it rejects (examples) |
| --- | --- | --- | --- |
| `Suckford` (seed 1) | `suck` | `crude.anywhere` (and `words.whole` loses `suck`) | 12 (`Suckbury`, `The Duchy of Suckdud`, `Gulsuckton`) |
| `Shiety Glade` (seed 3) | `shiet` | `crude.anywhere` | 16 (`Shietwo Hollow`, `The Circle of Shietae`) |
| `Realdeadstead` (seed 42 at 512) | `dead` | `words.anywhere` | 40 (`Deadstead`, `Curedeadstead`, `The Kingdom of Dead`) |
| "The Timhack League" (seed 1) | `hack` | `words.anywhere` (and `words.whole` loses `hack`) | 26 (`Hackford`, `Ashackstead`) |
| `Whop` (seed 42) | `whop` | `words.anywhere` | 13 (`The Whop Court`, `Whopia Hollow`) |
| god `Flopfia` (seed 11) | `flop` | `words.anywhere` (and `words.whole` loses `flop`) | 11 (`The Circle of Flopfy`, `Leeflop Glade`) |
| god `Gordman` (seed 7) | `gord` | `words.at_start` (and `words.whole` loses `gord`) | 13 (`Gordford`, `Clan Gordgou`) |
| god `Grammam` (seed 5) | `gramma` | `words.anywhere` | the `gramma…` names (`Grammaik Hold`); `Gramm` and `The Gramm Throne` pass |

Left, with the reason: "The Twiwhiwou Ring" (`twiw` is a faerie syllable pair; 12 names, all faerie, read as faerie); god `Mammga` (`mamm` is a dwarven syllable, 18 names such as `Kaidinmamm Hold`; `Mammga` alone reads as "mamma", a one-off not worth a rule); `tim` (66 names, most of them sound: `Timmthorin`; the critic's "Timhack" goes by `hack`). The whole entries removed are made redundant by the new entries (11.2's check `name_filter_not_redundant` would refuse them). Sizes after the edit: `crude` 70 anywhere, 16 at_start, 14 whole; `words` 5 anywhere, 1 at_start, 389 whole. The edit rejects about 0.24% of the drawn names of the sample (141 of 59,400, before overlaps); a rejected name is redrawn as 11.3 says.

### 12.2 No near-identical names in one world (amends 4.3 and 4.4)

Same-core names in one world read as the same place twice (`Skat` and `Skat Pit`, seed 11; `Skreek Pit` and `Skreek Hole`, seed 2; `The Slig Mob` and `Sligg Pit`, seed 42; the critic counted 49 pairs one letter apart in 8 worlds at 0.17.0).

**The core** of a valid attempt is its root R (4.2) with every `'` and `-` removed (letters a–z only; R is lower case before Cap). The history stage keeps, beside the registry, the set K of the cores of the civilization and settlement names it has drawn (gods and regions are not in K: gods are drawn by the chronology stage, and region names are English words). A core k is **near** K when some core of K equals k or is at Levenshtein distance 1 from it (one letter inserted, deleted or replaced). 4.3 becomes, for kinds `civilization` and `settlement` in the history stage (r11, as implemented: the rule belongs to the history stage's registry, the one the stage builds and names through; `make_name` called with a plain registry, as the chronology stage and the unit tests of 4.3 do, is unchanged):

```
    for t = 0, 1, …, 63:
        (valid, n, core) = attempt(t)
        if not valid: continue
        if first is none: first = (n, core)
        if n is not in registry and core is not near K: add n to registry; add core to K; return n
    … fallback as before, numbering first's name; add first's core to K
```

So a near attempt is skipped like a name already in the registry: it can still be the fallback's first name (the fallback stays exact, and its numeral keeps the name unique), and the fatal message of 4.3 is reached only when no attempt is valid, as before. Gods (P3-03) keep 4.3 unchanged. The rule applies at every core length: a variant that spared cores of three letters or fewer (where one letter apart is the goblin and orcish syllables' nature, `Kuz`, `Kaz`) was measured and costs as much while leaving pairs (below). Comparing a core with every core of K costs |K| comparisons of short strings per attempt: at most a few thousand per name in the largest worlds (2048, four civilizations per race).

**Measured** with the conflict reference at 0.18.0 (`worldgen/tests/reference/p3_conflict_reference.py`, which reproduces the 0.18.0 worlds row for row; checked on seed 42 at 1024) and the rule switched on by a harness (`~/.cache/the-unmapped/out/p3-10-spec/h10.py`; names do not change the simulation), 1024 default, seeds 0 to 19 (2,222 civilization and settlement names at 0.18.0; each rule was run together with one of P3-05 22.5's faerie site variants, which changes the settlements slightly, 2,218 to 2,222 names, and no name rule):

| Rule | pairs with equal cores | pairs one letter apart | names redrawn (attempt > 0) | extra attempts | fallbacks |
| --- | --- | --- | --- | --- | --- |
| none (0.18.0) | 14 | 123 | 383 | — | 0 |
| equal cores only | 0 | 121 | 396 | +19 | 0 |
| distance 1 from 4 letters, equal below | 0 | 9 | 465 | +144 | 0 |
| **distance 1 at every length** (the rule) | **0** | **0** | **471** | **+155** | **0** |

So the rule redraws 88 more names of about 2,220 (4%), about 0.07 more attempts per name, and no world needed the fallback. With every P3-10 change on (P3-05 22.5 as shipped): 2,189 names, 0 pairs equal or one letter apart, 464 redrawn, no fallback. ("Redrawn" counts names whose accepted attempt is not attempt 0, for any reason: a filtered root, a name in the registry, a near core.)

Rejected: equality of cores alone (it leaves `The Slig Mob` beside `Sligg Pit`); an edit distance of 2 (`Nili` and `Muli`), not measured, since at three- and four-letter cores it would forbid most of a syllable set.

### 12.3 Invariants and tests

- Invariant (adds to 6): **6. No two civilization or settlement names of a world have cores that are equal or one letter apart**, except a fallback name (numbered) and the names its first valid attempt was near.
- Unit tests: a registry holding the core `skat` rejects an attempt with root `skat` (`Skat Pit`) and accepts the next attempt; with `slig` in K, root `sligg` is skipped (distance 1); with `kuz` in K, roots `kuzz`, `kaz` and `kuz` are skipped and `kazz` passes (distance 2); a world whose every attempt is near K falls back to the first valid name with a numeral; gods are not affected.
- Filter data: the eight entries of 12.1 reject the names in the table; `Gramm` passes; `name_filter_not_redundant` passes on the shipped lists.
- Golden: `p3-02-names.txt` is re-recorded by the reference (the filter entries change names; the core rule is a history-stage rule and is checked through `p3-05-peoples.txt` and `p3-06-conflict.txt`, whose names change).

## 13. Amendment r13: crude fragments inside a word, and no three equal letters (P3-10 second round, generator 0.19.0)

Normative for the change P3-10 (`docs/specs/p3-10-followups.md` section 12, item 22), from generator **0.19.0** (not yet merged; the version stays 0.19.0); after the world critic's F4 (`docs/reviews/p3-10-followups-world-critic.md`). Data and one validity rule; no schema change.

### 13.1 Filter entries (data; `game/data/name_filters/` and both fixture copies)

The critic's names: `pee` in Twipee (also a god), Mypee, Lopee, Shaepee, Wypee Glade, Tapeefla, Miepeewhie Hollow; Twypoo; The Circle of Twotitwee; Fassbou Hollow (validation F5: Teefass); Beamcumstead and Dincum (human). `pee`, `poo`, `tit` and `cum` are already `crude.at_start` entries, which catch them only at the start of a part of the root word, and these names hold them inside. So the four move from `at_start` to **`anywhere`**, and `fass` is added there (11.3: a substring of the squeezed root word S):

| Edit | List | Why |
| --- | --- | --- |
| add `cum`, `fass`, `pee`, `poo`, `tit` | `crude.anywhere` | the critic's names |
| remove `cum`, `pee`, `poo`, `tit` | `crude.at_start` | covered by the new `anywhere` entries; 11.2's `name_filter_not_redundant` would refuse them |
| remove `poop`, `tits`, `titt` | `crude.anywhere` | contain a new `anywhere` entry, so they can never change a result |
| remove `poof` | `slurs.at_start` | contains `poo`; refused by `name_filter_not_redundant` otherwise |
| remove `teepee`, `toupee` | `words.whole` | contain `pee`; refused by `name_filter_not_redundant` otherwise |

Sizes after the edit: `crude` 72 anywhere, 12 at_start, 14 whole; `slurs` 33, 18, 6; `words` 5, 1, 387; `famous` unchanged.

**`cum` for every culture, not for humans only.** The kind has no per-culture list (11.2), and no culture needs one. **Checked**: with `p3_names_reference.py` (`make_name`, the shipped 0.19.0 r16 data), seeds 0, 1 and 2, entities 1 to 1,500 of every culture for kinds civilization and settlement and 1 to 300 for gods, 59,400 names (9,900 per culture), each root word tested for the five fragments inside it (`'` and `-` removed, fixed form words excluded). The new matches are:
- `cum`: 2 names, both human (`Alircumstead`, `Bilcumstead`), none of another culture;
- `pee`: 89 names, all faerie (`Beepee Hollow`, `The Circle of Flaepee`, god `Fynpee`);
- `fass`: 4, all faerie (`Eefass`, `Fassy`);
- `tit`: 3, all faerie (`Pytitwie`, `The Ititi Ring`);
- `poo`: 1, faerie (`The Poupoo Court`).

**No dwarven, elven, goblin or orcish name is touched**, and none of the matches is a name worth keeping. The shipped forms' fixed words and fused letters (11.3's `names_form_text` list: `the`, `of`, `kingdom`, … , `dell`, `ford`, `stead`) hold none of the five, so the stage check still passes. The common English words that hold them (`cucumber`, `title`, `speech`, `spoon`) are not reached by the shipped syllables.

### 13.2 No three equal letters in a row (amends 11.3's validity)

An attempt of 4.2 with rendered name n is **invalid** when lower(n) (A–Z mapped to a–z) holds the same letter a–z at three consecutive positions. Space, `'` and `-` are characters too, so they break a run: `Bigg-Gnizz` and `Aa'azaam` are valid, `Wasssie` and `Floullly Hollow` are not. 4.2's validity becomes `valid = (r4 validity) and not filtered(W) and not triple(n)`. A triple-letter attempt is an invalid attempt for every purpose of 4.3, as a filtered one is (11.3): it is skipped, it is never the fallback's first name, and the next attempt t + 1 is drawn with its own key. The rule applies to every name `make_name` draws: gods (chronology), civilizations and settlements (history). Regions are named by P1-06 from English place words and are not covered. **Checked**: in the 18,507 region names of the P3-10 validation sweep none holds three equal letters, so no P1-06 change is made.

**Measured** on the same 59,400 names: 52 have three equal letters, all faerie (`Passsae`, `Baellli`, `The Circle of Eesssy`, the critic's `The Bosssie Ring`); none of another culture. With 13.1, 152 of the 59,400 names (0.26%) are redrawn: 150 faerie (1.5% of faerie names) and 2 human. Each is redrawn by 4.3; no culture comes near the fatal message (64 invalid attempts).

### 13.3 Invariants and tests

- Invariant 6 gains: **7. No generated name (god, civilization, settlement) holds three equal letters in a row** (case-insensitive, consecutive characters), the Roman numeral of 4.3's fallback aside (`III`, `VIII`: the rule tests attempts, and the fallback numbers a valid attempt's name).
- Unit tests: roots giving `Twipee`, `Twypoo`, `Twotitwee`, `Fassbou Hollow`, `Beamcumstead` are filtered; `Wasssie`, `Floullly Hollow` and a god `Fiasssie` are invalid, and the next attempt is taken; `Bigg-Gnizz`, `Aa'azaam` and `Billa Hollow` are valid; `name_filter_not_redundant` passes on the edited lists and fails on a copy that keeps `crude.at_start` `pee` or `words.whole` `teepee`.
- Golden: `p3-02-names.txt`, and through gods `p3-03-chronology.txt`, re-recorded by the reference; the history goldens follow through names.

## 14. Amendment r15: faerie syllables, no repeated syllable, `tard` (P3-11, generator 0.20.0)

Normative for the change P3-11 (`docs/specs/p3-11-followups.md`), from generator **0.20.0**, not before. Sources: Paul's decision of 2026-11-21 on faerie names (`docs/design/proposals.md`, "Paul's decision of 2026-11-21: faerie names keep their character, without the baby-talk sounds": "no `p` and `b` onsets, fewer `ee` and `ou` nuclei, and no syllable repeated twice in a row"), and the world critic's N3 of the P3-10 second round (`docs/reviews/p3-10-followups-round2-world-critic.md`: `tard`, and faerie names ending in `ass`). Data and one validity rule; no schema change; no new stream or draw.

### 14.1 Faerie syllables (data; `game/data/cultures/faerie.toml` and both fixture copies)

The faerie block of section 5 (as amended by 11.4) becomes, with every other line unchanged (`patterns`, `god_patterns`, `min_letters` 3, `max_letters` 10, the three form lists):

```toml
[culture.faerie.naming]
onsets = ["", "v", "f", "w", "l", "s", "t", "gl", "sh", "wh", "fl", "tw", "m"]
nuclei = ["i", "i", "ee", "ie", "ie", "ia", "ia", "a", "a", "y", "y", "ae", "ae", "ou", "o", "o"]
codas = ["", "", "ll", "n", "sh", "th", "sk", "ss"]
forbidden = ["eee", "iee", "yy", "ouou", "shsh", "ass"]
```

| Edit | Why |
| --- | --- |
| onsets: `p` → `v`, `b` → `gl` (13 entries, as before) | Paul's "no `p` and `b` onsets". The replacements keep the soft, light sound (`Vael`, `Vyll Glade`, `Glia`, `The Glaeli Court`). Both are also elven onsets, but the faeries already share five onsets with the elves (`""`, `f`, `l`, `s`, `m`), and their character is carried by what the elves do not have: the nuclei `y`, `ee`, `ou`, `ia`, the codas `sk`, `ss`, `sh`, the onsets `w`, `wh`, `tw`, `fl`, and the glade and ring forms (`Vyskia Glade` against the elven `Voglael`). So the names do not drift toward the elven ones, the alternative Paul turned down. They reach few English words: as one-syllable roots `v` gives `via`, `vie`, `van`, `von`, `vee`, and `gl` gives `glee`, `gloss` (14.3 takes `via`, `van`, `von` and the two-syllable `ova` into `words`; `vie`, `vee`, `glee`, `gloss` are neutral words in doubt, permitted by 11.11.1). Rejected: `n` (`nan`, `non`, `nosh`), `r` (`ran`, `rash`, `risk`, `roll`), `th` (`thin`, `than`, `thou`, Thoth), `sw`, the one soft cluster no other culture uses (`swill`, `swish`, `Swiss`, `swan`, `swath`), and `z` (`zee` only, but a buzzing sound that the goblins' `z` already owns) |
| nuclei: 16 entries, `ee` and `ou` once, the seven others twice | "fewer `ee` and `ou`": each falls from 1/9 (111‰) to 1/16 (62.5‰) of the nucleus draws, the others rise from 111‰ to 125‰. Nuclei are picked uniformly (4.2), so a repeated entry is a weight, as `codas` already repeats `""`; section 3 has no uniqueness check on syllable lists |
| codas: `p` removed (8 entries) | A coda followed by an empty onset is an onset in all but spelling (`ONCON` with onset `""`: `Li-p-a`, `Ma-p-ou-fee`), so the `p` coda would keep the `p` sound Paul removed. Not replaced: every replacement tried reaches more English words (`l`: `feel`, `foul`, `wheel`; `s`: `lies`, `ties`, `flees`, `was`; `nn`: `Flynn`, `Lynn`), and the share of `""` rises only from 2/9 to 2/8 |
| forbidden: `ass` added | A faerie root ends in `ass` only through the nucleus `a` or `ia` followed by the coda `ss` (no other pair of the lists yields `ass`), and the critic found 11 such names in 960 faerie names (`Fiesass`, `Liass`, `Wobass Glade`, `The Circle of Twaepass`), in no other culture. The culture's own `forbidden` list (substrings of the root, 4.2) is enough, so no filter rule and no new filter list is needed. It also rejects `ass` inside a faerie root (`Lassia`): with 14.1's shares about one root in 50 (3 of 5 patterns hold one `C`, drawn `ss` with 1/8, after `a` or `ia` with 4/16); `ss` after every other nucleus stays (`Lyss`, `Fiess`, `Glouss`). The shared filter keeps `ass` as an `at_start` entry for every culture |

Names Paul and the critic named as the faerie character stay reachable (`Lyfae`, `Faeli`, `Atwia`); the baby-talk examples of the decision do not (`Weebaema`: `b`; `Eepou`: `p`; `Wifleeflee`: 14.2). `The Twiwhiwou Ring` (left by Paul's decision of 2026-11-20, 12.1) is still reachable: its syllables `twi`, `whi`, `wou` are not repeated in the sense of 14.2.

### 14.2 No syllable repeated twice in a row (amends 4.2's validity; every culture)

**Syllables of an attempt.** Read the attempt's pattern left to right. A **syllable boundary** stands at the pattern's first symbol, before every `O`, before every `N` not directly preceded by `O`, and at every separator `'` or `-` (the separator itself belongs to no syllable). The **syllables** are the runs of symbols between boundaries, separators left out. For syllable i let s_i be the concatenation of the strings drawn for its symbols (onset, nucleus and coda text as drawn, lower case, before Cap), and b_i the same concatenation without the strings drawn for its `C` symbols. With the shipped patterns every syllable is `ON`, `ONC`, `N` or `NC`: `ONONC` has the syllables `ON`, `ONC`; `ONCON` has `ONC`, `ON`; `NCONC` has `NC`, `ONC`; `ONC'ONC` and `ONC-ONC` have `ONC`, `ONC`. The rule is defined for any pattern the grammar allows (a run without `N`, such as the first `O` of `OONC`, is a syllable of its own).

**Repeated.** An attempt has a **repeated syllable** when for some two consecutive syllables i and i + 1 (consecutive in the pattern, also across a separator), s_i is not empty and **s_i = s_{i+1} or s_i = b_{i+1}**: the second syllable is the first again, or the first again followed by a coda. Comparison is of bytes. Text is compared, not list indices, so a repeated list entry (`"i"` twice in the faerie nuclei, `""` twice in the codas) changes nothing.

4.2's validity becomes `valid = (r4 validity) and not filtered(W) and not triple(n) and not repeated`. A repeated attempt is an invalid attempt for every purpose of 4.3, as a filtered or a triple-letter one is (11.3, 13.2): it is skipped, it is never the fallback's first name, and the next attempt t + 1 is drawn with its own keys u(j) = draw(S_K, t + 1, c, e, j) (stream `hist.names.<K>`, as every attempt). No draw is added or reordered: the test reads the strings the attempt has already drawn.

| Root (pattern, drawn strings) | s_i, s_{i+1}, b_{i+1} | Result |
| --- | --- | --- |
| `wifleeflee` (`ONONON`: `w` `i` `fl` `ee` `fl` `ee`) | `flee`, `flee` | repeated |
| `lilin` (`ONONC`: `l` `i` `l` `i` `n`) | `li`, `lin`, `li` | repeated |
| `fifi`, `feefee`, `nono` (`ONON`) | equal | repeated |
| `dackdack` (human `ONCONC`) | `dack`, `dack` | repeated |
| `krak-krak` (goblin `ONC-ONC`) | `krak`, `krak` | repeated (a separator does not break it) |
| `grangrak` (dwarven `ONCONC`: `gr` `a` `n` `gr` `a` `k`) | `gran`, `grak`, `gra` | valid (a closed first syllable followed by another coda reads as a compound) |
| `feefi` (`ONON`) | `fee`, `fi` | valid |
| `twiwhiwou` (`ONONON`) | `twi`, `whi`, `wou` | valid (no two consecutive equal) |
| `lillia` (`ONCON`: `l` `i` `ll` `""` `ia`) | `lill`, `ia`, `ia` | valid |

**A general rule, not a per-culture switch** (decision). No shipped culture's naming wants a syllable said twice in a row: the critic's comic names include a human god `Codcod` and faerie `Feefi`-like doubles, a human `Dackdack` or a goblin `Krak-Krak` reads as a nursery word or a cartoon chant, and the triple-letter rule (13.2), the closest precedent, is general. A switch would add a required field to every culture file and the loader for a choice no culture makes; one can be added later (a data and loader change) if a culture is meant to chant. The cost is small: a repeat needs the same onset and nucleus twice, and none for one-syllable shapes; no culture comes near 4.3's fatal limit. (r16: r15 estimated "about one attempt in 60 to 150" for two-syllable shapes. **Measured** at 0.20.0 (`docs/reviews/p3-11-followups-validation.md` 5), the rule rejects one otherwise valid attempt in **137** (elven), **175** (faerie), **220** (goblin), **961** (orcish), **1,902** (dwarven) and **2,968** (human); 14.1's faerie `ass` rejects one root in 54 and 14.3's `tard` one human name in 469, as estimated.)

### 14.3 Filter entries (data; `game/data/name_filters/` and both fixture copies)

| Edit | List | Why |
| --- | --- | --- |
| add `tard`; remove `retard` (now redundant) | `slurs.anywhere` | The critic's `Tardham`, `Tardwuldwick` (human) and `The Horktard Throne` (dwarven) read as the slur fragment. `whole` caught none of them, since `tard` is never a whole part of a root word; `at_start` would catch `Tardham` and `The Hold of Tardnu` but not `Horktard`, where the fragment is a stressed syllable inside the word. So `anywhere`, as the critic suggests. `slurs` is the right file: it is the fragment of a disability slur, as `retard` was |
| remove `tard` | `slurs.whole` | covered by the new `anywhere` entry; `name_filter_not_redundant` (11.2) would refuse it |
| remove `bastard` | `crude.anywhere` | contains `tard`, so it can never change a result (as 13.1 removed `poop`, `tits`, `titt`) |
| add `ova`, `van`, `via`, `von` | `words.whole` | English words the new faerie onsets reach as a whole root (14.1; at least 10^-5 per attempt by the lists' shares): function words (`via`, `von`) and comic or modern words (`van`, `ova`), the classes of 11.11.1 |

**Which names `tard` rejects.** Only human and dwarven roots reach it: a `t` or `st` onset, the nucleus `a`, the coda `rd` (human `Tardbury`, `Stard`, `Custard`; dwarven `Horktard`). By the lists' shares a human root syllable is `tard` or `stard` with probability about 2/15 × 1/6 × 1/10 ≈ 1/450, so about 0.3% of human names and less of dwarven names are redrawn; the harmless losses (`Stard`, `Stardton`, `Mustard`, `Dastard`) are the critic's accepted ones. No other culture can draw `t` + `a` + `rd` (elven, goblin, orcish and faerie lists have no `rd` coda after a `t`). *To be confirmed by the test writer's scan of the 13.1 sample (the names `tard` rejects, per culture).* The shipped forms' fixed words and fused letters (11.3's `names_form_text` list) hold no `tard`.

Sizes after the edit: `slurs` 33 anywhere, 18 at_start, 5 whole; `crude` 71, 12, 14; `words` 5, 1, 391; `famous` unchanged.

### 14.4 Invariants, tests and goldens

- Invariant 6 gains: **8. No generated name (god, civilization, settlement) comes from an attempt with a repeated syllable** (14.2). It is checked by replaying attempts (the reference), not by scanning stored names: a name alone does not say how its root was split into syllables.
- Unit tests: the nine rows of 14.2's table, each with a hand-made pattern and drawn strings; `OONC` with onsets `"", "l"` and the rest `"i"`, `"n"` (syllables `""` and `lin`; s_1 empty, so not repeated); an attempt rejected only by 14.2 is followed by attempt t + 1 and is never the fallback's first name; the faerie lists of 14.1 load and pass the checks of section 3; a faerie root with `a` + `ss` is invalid (forbidden `ass`) and `Lyss` is valid; `Tardham`, `Tardwuldwick`, `The Horktard Throne`, `Stard` are filtered, `Standard` (no `tard`) is not; `name_filter_not_redundant` passes on the edited lists and fails on a copy that keeps `slurs.whole` `tard`.
- Data: `p3_data_reference.py` (the content hashes of the three data roots) and the loader tests.
- Golden: `p3-02-names.txt` re-recorded by the reference (every culture through 14.2, faeries through 14.1, human and dwarven through `tard`); `p3-03-chronology.txt` through the gods; the history goldens through names. Sections 5.1 and 11.6's sample tables stay the record of the data they were drawn with (r4, r5); the reference's transcriptions of them are unchanged, and the test writer adds the faerie sample of 5.1's protocol at 0.20.0 to the reference (a new `SAMPLES_R15` row) for the critic to judge.
- Validation reports: per culture, attempts rejected by 14.2, by `forbidden` `ass`, by `tard` and by the four new words; fallbacks (must be 0); the faerie names of the default sweep with `ee`, `ou`, `p` or `b` (no `p` or `b` may start a faerie syllable; letters `p` and `b` cannot occur in a faerie name at all with 14.1's lists).

## 15. Amendment r17: `fag` inside a word, `shiva`, two dwarven stutters (P5-01, generator 0.21.0)

Normative for the change P5-01 (`docs/specs/p5-01-followups.md`, item 5), from generator **0.21.0**, not before. Sources: the P3-11 world critic's F3 and F4 (`docs/reviews/p3-11-followups-world-critic.md`) and the lead's ruling of 2026-11-22 (`docs/design/proposals.md`, "History: the P3-11 orc targets, the lead's ruling", left for the next change). Data only: no rule, no field, no schema change, no stream or draw.

### 15.1 Filter entries (data; `game/data/name_filters/` and both fixture copies)

| Edit | List | Why |
| --- | --- | --- |
| move `fag` from `at_start` to `anywhere`; remove `fagg` | `slurs` | The faerie `Shyfaglae` (default seed 87) reads as "Shy-fag-lae": the fragment stands as a stressed syllable inside the word, which `at_start` cannot see (as `tard` in 14.3). `fagg` (`anywhere`) contains `fag` and can no longer change a result. No shipped English word the cultures reach holds `fag` (a `f` onset, the nucleus `a` and a `g` onset or coda: faerie `f` + `a` + `gl`, human `f` + `a` + `g`) |
| add `shiva` | `famous.whole` | The seat of the Circle of Glae in default seed 77 is `Shiva`, the name of a living religion's god, reached by the faerie `v` onset (14.1); `siva` is already listed |
| add `hindi` | `famous.whole` | a language's name (dwarven, `civs4` seed 17) |
| add `fannie` | `crude.whole` | British slang (elven, `civs4` seed 1, twice) |

`name_filter_not_redundant` (11.2) passes on the edited lists: no `at_start` or `whole` entry contains `fag`, and `shiva`, `hindi` and `fannie` contain no `anywhere` entry and start with no `at_start` entry. Sizes after the edit: `slurs` 33 anywhere (`fag` in, `fagg` out), 17 at_start, 5 whole; `crude` 71, 12, 15; `famous` 0, 0, 103 (r18; r17 said 105); `words` unchanged.

### 15.2 Two dwarven stutters (data; `game/data/cultures/dwarven.toml` and both fixture copies)

The dwarven codas `din` and `rin` spell an onset and a nucleus inside one coda, so 14.2's syllable test cannot see the stutter they make after a matching onset and the nucleus `i`: `d` + `i` + `din` gives `Didin`, and `br`, `dr`, `gr` or `thr` + `i` + `rin` gives `Bririn`, `Dririn`, `Gririnhard`. The dwarven `forbidden` list gains **`didin`** and **`ririn`** (appended, in that order):

```toml
forbidden = ["mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk", "didin", "ririn"]
```

A root holding either is invalid (4.2), and the next attempt t + 1 is drawn with its own keys, as for every forbidden substring. The codas stay: `Thorin`-like and `Balin`-like names (`Gromrin`, `Kadin`, `Hodin`) are the dwarven sound; only the doubled `di-din` and `ri-rin` go. Near-repeats with another nucleus (`Dadin`, `Brarin`) stay, as Paul left the faerie near-repeats (`docs/design/proposals.md`, "Paul's decisions of 2026-11-22", decision 4).

### 15.3 What this changes (counted by the spec writer in the 375 worlds of the P3-11 validation sweep, 41,137 names)

| Entry | Stored names it rejects at 0.20.0 |
| --- | --- |
| `fag` anywhere | 1: `Shyfaglae` |
| `shiva` | 1: `Shiva` |
| `hindi`, `fannie` | 1: `Hindi`; 2: `Fannie` |
| `didin` | 8 (7 distinct): `Clan Didin`, `Didin`, `Didin Deep`, `Didingrud`, `Didinmour Deep`, `Didinthrak Deep`, `Houmdidin` |
| `ririn` | 19 (9 distinct): `Bririn`, `Bririntai`, `Dririn Deep`, `Griringou`, `Gririnhard`, `Gririnhard Deep`, `Gririnmo`, `Gromdririn`, `Grumbririn Hold` |

A rejected name is replaced by the entity's next valid attempt (4.3); the new name can change later names only through the registry's uniqueness (7). No history rule reads a name.

### 15.4 Tests and goldens

- Unit tests: `Shyfaglae` and `Fagwick` are filtered, `Fagwick` also before r17 (the old `at_start`), `Shyfaglae` only from r17; `Shiva`, `The Hindi Court`, `Fannie Glade` are filtered; a dwarven root `didin` (`ONC`: `d` `i` `din`) and `gririnhard` (`ONCONC`: `gr` `i` `rin` `h` `a` `rd`) are invalid by `forbidden` and followed by attempt t + 1; `dadin` and `kadin` are valid; `name_filter_not_redundant` passes on the edited lists and fails on a copy that keeps `fag` in `at_start` beside the new `anywhere` entry; `fagg` is absent from the shipped file (the check compares `at_start` and `whole` entries with `anywhere` ones, not two `anywhere` entries, so a data test asserts it).
- Data: `p3_data_reference.py` (content hashes of the three data roots) and the loader tests.
- Golden: `p3-02-names.txt` re-recorded by the reference (dwarven names through 15.2; faerie, elven and dwarven names through 15.1); `p3-03-chronology.txt` through god names those entries reach; the history goldens through names.
- Validation reports: per culture, attempts rejected by each new entry (the scan of 13.1's sample, `p3_names_reference.py scan`); in the sweep's stored names, none holding `fag`, `didin` or `ririn`, and none equal to `shiva`, `hindi` or `fannie` as a word; fallbacks 0.

## Revision note (2026-11-19, text only, after implementation)

The implementation (`p3/02-names`) and the independent tests (`p3/02-names-tests`) agreed; this revision makes the text say what they do. No behaviour, golden value or world byte changes.

1. 4.5, 9.3: the kind order (god, civilization, settlement) is stated, for `NameKind` and the golden list.
2. 9.3: the per-seed SHA-256 is over that seed's full name lines joined by LF, no trailing LF.
3. 9.3: where each 4.6 vector appears in the golden file (scenarios `empty`, `collide`, `full`, `seed42`; the list line of `Iensath` with 250 earlier names), replacing "must appear unchanged".
4. 4.2, 9: an invalid attempt's `name` (and `root`) is unspecified.
5. 4.3, 4.4: `roman` is fatal outside [1, 100000]; 64 bytes passes, 65 is fatal; the old claim "cannot happen with fewer than 2^16 names" was wrong and is replaced by the exact condition (4,887 names sharing a 48-byte first name).
6. 4.1: `weighted`'s bounds (sum at most 2^63 − 1, fatal above), the reachability of small weights stated exactly, and the equivalence with P3-00 4.6's wording.
7. 9: seams as implemented: `choose_name` and `NameChoice`, `NameAttempt::root`, `name_kind_text`, `name_stream`, `name_forms`, `capitalize_root`, the three constants, `NameRegistry::size`; `add` of a present name is a no-op.
8. 4.2: malformed naming data is ruled out by the loader and fatal in the library, with the messages.
9. 8: `all_stream_names` grows from 30 to 33; the count test changes with it.
10. 5.1: the sample protocol is stated; the independent reference reproduces the table.

## Revision note r2 (2026-11-19, amendments for P3-03)

Text only. 4.4: the registry also holds the age names (fixed text, numeral fallback) and is rebuilt by the `history` stage from tables `region`, `age` and `god` (P3-03 8). 4.5: `god.id` equals the race id, every race has a god (P3-03 5.2).

## Revision note r3 (2026-11-19, after the P3-03 implementation)

Text only; no behaviour, golden value or world byte changes. 7: the registry is an ordered set (the words allowing a hash set are removed: unordered containers are banned in `worldgen`, and the code uses an ordered set). 4.3: `roman` now lives in `worldgen/core/roman.hpp` as `roman_numeral`, with `history::roman` and `stages::roman` forwarding to it. 8: the P0-01 request is applied (P0-01 section 16).

## Revision note r4 (2026-11-19, amendments for P3-04)

Requested by P3-04 18.2. 4.5: civilization and settlement names are drawn by the `history` stage from P3-04 on (its stub placement, P3-04 7.2, in creation order after the registry is rebuilt from `region`, `age` and `god`), and by P3-05 when it replaces the stub. No rule of this spec changes.

## Revision note r5 (2026-11-20, amendment, not yet shipped)

After the world critic's review of P3-05 (F5: a slur-like goblin name, crude human town names, comic English god names, short gods, bare elven realms). New section 11, normative for the follow-up change after P3-06 and not before: (1) a shared name filter, new required data kind `name_filter` (`game/data/name_filters/`, four files committed now, ignored by the current loader), rejecting an attempt whose root word contains an `anywhere` fragment, has a part starting with an `at_start` fragment, or has a part equal to a `whole` word; (2) `culture_naming.god_patterns`, shipped as each culture's multi-syllable shapes; (3) the elven bare realm form becomes `The House of {root}`, and a loader check requires every civilization form to hold a word besides the root. Measurements, samples before and after, the region-name scan, tests, consequences (generator bump, every world hash and the name and sentence goldens; no schema change) and seven questions for Paul are in 11.6 to 11.10. Pointers in sections 3, 4.2 and 5 mark what r5 changes; nothing else in sections 1 to 10 changes until r5 ships.

## Revision note r6 (2026-11-20, r5 ships with P3-08)

r5 (section 11) ships with the P3-08 change (`docs/specs/p3-08-followups.md`, generator 0.17.0); the seven questions of 11.10 are open with Paul and their interim choices stand. Two additions after the world critic's review of P3-06 (`docs/reviews/p3-06-conflict-world-critic.md` F10): 11.3 states that the whole rendered name is covered, the root word by `filtered(W)` at every attempt and the form's fixed words by the stage check `names_form_text`, which now also checks every fixed word for `anywhere` and `at_start` (`whole` is not applied to them); the test is per word, not across spaces, since the squeezed whole name added only a false positive on 18,427 sweep names. 11.3 states how a rejected name is redrawn (the next keyed attempt; nothing depends on other names' rejections). 11.6 gains the scan of the 0.16.0 sweep names (5.4% of them rejected; every name either critic cites is rejected but one comic name). 11.7 items 8 and 10 gain cases. No list entry changes: the shipped lists already catch every stem the reviews found.

## Revision note r7 (2026-11-20, corrections after the P3-08 implementation)

Text only. 11.7 item 6: the lower-casing case is form `E{root}`, root `f`, word `ef`, rejected as a whole word (r6's `XEF` matched no entry). 11.2: where the kind checks report (the later definition, the first failure only). 11.3: `names_form_text`'s message is used verbatim by the chronology stage, with no stage prefix (P3-03 r3, r4).

## Revision note r8 (2026-11-20, amendment: the short English-word list)

Paul's decision on 11.10 question 1 (the short list; questions 2 to 7 accepted as shipped). New section 11.11: the rule for `words` entries (crude, comic or modern-sounding words, and function words, inflected forms and plain slips whose names read as an error; a neutral word in doubt is permitted), the shipped list (392 of the 647 r5 words; 255 released), how it was made, measurements (exact rates, 50 worlds at 1024, samples), consequences (a data change only: no code, no schema change; every world hash and the name and text goldens change; generator bump with the change that ships it, P3-09 as 0.18.0 or alone as 0.17.1), the tests to update, the choices made in doubt and every word with its reason. 11.2's sizes and description of `words`, 11.6 (a pointer) and 11.10 (closed) are updated. `game/data/name_filters/words.toml` and its two fixture copies hold the new list on branch `p3/10-word-list`.

## Revision note r9 (2026-11-20, `mold` back on the list; with P3-09)

Text and data corrections in 11.11 after the lead's interim choice to restore `mold` to the reject list. The list is 393 words (254 released) and the effective `whole` list 514. `mold` moves from the released plants to the kept comic words in 11.11.7, and 11.11.6 is updated. The 11.6 sample table changes in one place only (`The Duchy of Wick`); orcish settlement 6 stays `Shaazazuug` and the seed 42 orcish god `Ogh'krak`, as the test writer's reference golden on `origin/p3/09-int` records. The measurements of 11.11.3 were taken with `mold` released and are kept as they were.

## Revision note r10 (2026-11-20, comic and near-identical names; for P3-10, generator 0.19.0)

New section 12 (`docs/specs/p3-10-followups.md`): eight filter entries for the names the P3-08 world critic listed, with four `whole` entries removed as redundant (`suck`, `hack`, `flop`, `gord`; 12.1, data; r12: this note said three); no two civilization or settlement names of a world with cores equal or one letter apart (12.2, amends 4.3 and 4.4 for the history stage); invariant 6, tests (12.3). No earlier section is renumbered.

## Revision note r11 (2026-11-21, after the P3-10 implementation)

12.2: the core rule applies to the history stage's registry; `make_name` with a plain registry is unchanged (the implementation's reading, confirmed).

## Revision note r12 (2026-11-21, text after the P3-10 integration)

The r10 note is corrected: 12.1 removes four `whole` entries (`suck`, `hack`, `flop`, `gord`), as its table and list sizes say, not three. No rule or data change.

## Revision note r13 (2026-11-21, P3-10 second round; generator stays 0.19.0)

New section 13: `cum`, `fass`, `pee`, `poo` and `tit` as `crude.anywhere` entries, with the entries they make redundant removed (`crude.at_start` `cum`, `pee`, `poo`, `tit`; `crude.anywhere` `poop`, `tits`, `titt`; `slurs.at_start` `poof`; `words.whole` `teepee`, `toupee`); `cum` for every culture, since only human names reach it (13.1); a name with three equal letters in a row is an invalid attempt (13.2); invariant 7 and tests (13.3). No earlier section is renumbered.

## Revision note r14 (2026-11-21, text after the P3-10 second-round integration)

11.3, "How a rejected name is redrawn": the example of an at_start entry that does not match inside a word was `Cumbrin` filtered while `Ascumbrin` is not, which 13.1 made wrong (`cum` is an `anywhere` entry, so `Ascumbrin` is filtered). It is now `Bumdou` filtered (at_start `bum`) while `Abumdou` is not, checked against the shipped lists. No rule or data change.

## Revision note r15 (2026-11-21, P3-11; generator 0.20.0)

New section 14 (`docs/specs/p3-11-followups.md`): the faerie syllable lists after Paul's decision of 2026-11-21 (onsets without `p` and `b`, replaced by `v` and `gl`; `ee` and `ou` at half their share; the coda `p` removed; `ass` in the faerie `forbidden` list, 14.1); no syllable repeated twice in a row, a validity rule for every culture, with syllables defined from the pattern (14.2); `tard` as a `slurs.anywhere` entry, with `retard`, `bastard` and `slurs.whole` `tard` removed as redundant, and four English words the new onsets reach added to `words.whole` (14.3); invariant 8 and tests (14.4). 11.3's example "`Stard` passes" holds only before 0.20.0: from 14.3 `Stard` is filtered. No earlier section is renumbered.

## Revision note r16 (2026-11-22, text after the P3-11 validation)

14.2: the rejection rates of the repeated-syllable rule as measured at 0.20.0 replace r15's estimate (1 in 137 elven, 175 faerie, 220 goblin, 961 orcish, 1,902 dwarven, 2,968 human). No rule or data change.

## Revision note r17 (2026-11-22, P5-01; generator 0.21.0)

New section 15 (`docs/specs/p5-01-followups.md`, item 5): `fag` moves from `slurs.at_start` to `slurs.anywhere` and `fagg` is removed; `shiva` and `hindi` join `famous.whole`, `fannie` `crude.whole` (15.1); the dwarven `forbidden` list gains `didin` and `ririn` (15.2). Data only. No earlier section is renumbered.

## Revision note r18 (2026-11-22, text after the P5-01 integration)

Text only; no rule or data change. 15.1: the `famous` list holds 103 `whole` entries after the edit (101 and `shiva`, `hindi`), not 105. 5.1 and 11.6's sample tables: the dwarven civilization 2, "The Hold of Didin", is "The Khandthra Throne" from 0.21.0, since 15.2 forbids `didin`; both tables give the 0.21.0 name, with a note.
