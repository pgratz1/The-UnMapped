# P5-08 Pantheon, divine interventions and cults

Status: **r1 (2026-11-27)**, for approval. Phase P5 History depth, stage 08 (`docs/specs/p5-00-history-depth-overview.md` 3.1, the P5-08 paragraph and its amendment: "P5-08 also takes cults (a god's followers and their sanctums), in place of P5-10's cults"; Paul's scope decision Q-P5-00-1 of 2026-11-24). **Base: P5-07 as built** (spec r2 and `docs/reviews/p5-07-impl-notes.md`): generator 0.29.0, schema 13, history report format v10, event kinds 1 to 99, 107 streams, browser groups 0 to 8. This stage: generator **0.30.0**; schema **14** (13); history report format **v11**; event kinds **100 to 110**; **120** streams; browser group 9.

Design: section 5 "Gods and the order of the races" ("A pantheon of gods in three camps (good, neutral, evil) creates the races and keeps acting on the world"; "During active phases gods can bless or curse a people, raise a champion, send a monster, or mark a site as sacred or profane. Each act is an event with causes and consequences like any other"), "Flare-ups" ("a god acts"), the world-creation setting *Divine activity* (distant, normal, meddling), "Entities" (factions: "noble houses, cults, bandit bands and orders"; sites: "shrines"), the P5 decisions ("Sites begin in phase P5 … marked shrines"; "The gods come late, after monsters and figures, so that all five kinds of intervention arrive together"; "The divine script stays refused through phase P5"; "Exit check. Every race, god camp and monster kind is a participant in at least one event"), section 6 (`god`: id, name, alignment, domain; `faction`: id, name, kind, civ_id), section 9 (dungeon origin "cult sanctum"; spellbooks in "cult sanctums"), section 14 "The divine script (experimental)". Builds on P3-03 (table `god`), P3-04 6.8 (the intervention source), P5-02 (monsters, sites, tension), P5-04 (alliances and trade, the site seam), P5-06 (figures, towers) and P5-07 (the deep).

## 1. Purpose

The pantheon grows from six creator gods to fifteen: nine more gods, three in each camp, each with a **domain** (war, death, harvest, craft, wild, oaths, lore, ruin) and a title, named per world, written by `chronology`. In history each god keeps a **regard** for each realm, built from the realm's deeds that the god's domain cares about (a sack of the god's people angers its creator; a broken alliance angers the Keeper of Oaths; a mine opened pleases the Hammer Under the Mountain). When a regard is strong enough, in a tick year, the god may **act**: bless or curse the realm, raise a champion for it, send a monster against it, or mark a place as sacred or profane. Every act is an event that cites the deeds that moved the god, and every act changes something other subsystems read: growth, strength in war, deaths, a monster with a lair, a figure who fights as hero and general, a shrine that makes land shunned or a settlement thrive. A god's regard building in a quiet age can bring a flare-up.

An act can give rise to a **cult**: a god's following among a realm, founded by a recorded figure where one fits (the champion, the realm's caster, its ruler) or by a prophet, with a temple at a settlement. Cults spread along trade and alliance links, may be taken up as a realm's faith (pleasing their god, angering the gods of the opposed camp) or suppressed by rulers (angering their god), and end when no realm keeps them, or go into hiding in a sanctum in the wild that lasts to the present day. Cults are rows of a new table `faction` (kind 1), so the later factions stage adds its guilds to the same table.

The stage also carries the follow-ups of P5-07 and P5-04a (section 12): the deep in summaries, the deep's wakings spread over the ages, aid that does not read as a cause of a fall, deepening lines with more variety, the flight plan computed once, a cap on written alliance renewals, one tower per region with masters who take it up, and towers that end when their line of masters lapses.

## 2. Items

| # | Item | In short | Section | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | The pantheon | data kind `god` (nine shipped); `god` gains `domain`, `title`, `race`; names by P3-02 | 4.1, 6.1, 9 | yes (table `god`) |
| 2 | Regard | per (god, realm), from deeds in the log (data), fading; causes kept | 4.2, 6.2 | yes |
| 3 | Interventions | at most one act a tick year (T5), five kinds, effects through seams; `--divine-activity` | 6.3 to 6.5, 7 | yes |
| 4 | The gods' flare-up | a strong regard in a quiet step can trigger a flare-up in which the god acts | 6.6 | yes |
| 5 | Cults | founding, temples, spread, faith, suppression, hiding, endings | 6.7 to 6.10 | yes |
| 6 | `--divine-script` | stays refused, new message; the source/applier split is the seam a script will use | 6.11 | CLI text only |
| 7 | Follow-ups | P5-07 critic F1 to F4, audit A1, A2; P5-04a critic F1, F5; tower lapse | 12 | yes |
| 8 | Report v11, timeline, browser, markers, viewer | `[pantheon]`, `[cults]`, realm lines; group 9; glyphs 21 to 23 | 10, 11 | report and viewer |

## 3. Inputs, streams, draws

As P5-02 3, P5-04 3, P5-06 3. **Read**: the log from the subsystem's cursor (P3-04 23.3); civilizations (race, culture, capital, home region, ended), settlements (owner, region, cell, population, status, `owner_event`, `ruin_event`), regions (owner, borders); open `alliance` and `trade` relations through `AllianceView` (7.4); figures through `FigureView` (7.5); monster kinds, living counts, lairs and free cells through `MonstersSubsystem` (7.3); the `god` table's rows as written by chronology (read back through `HistoryInputs`); data `god` (4.1) and tuning `history.pantheon` (4.2); `param.divine_activity` (A, 4.2). `param.event_intensity` does not scale this stage.

**Streams**, appended to `core/streams.hpp` after `hist.deep.warren_kind` (the 107th), in this order (**120** streams):

| # | Stream | Key (a, b, k) | Drawn |
| --- | --- | --- | --- |
| 108 | `hist.pantheon.act` | god, civilization, 0 | once per tick year at most, for the year's one candidate pair (6.3) |
| 109 | `hist.pantheon.kind` | god, civilization, 0 | once per act (6.3) |
| 110 | `hist.pantheon.flare` | god, civilization, 0 | once per Q4 at most (6.6) |
| 111 | `hist.pantheon.monster_kind` | god, civilization, 0 | once per `divine_monster` (6.4) |
| 112 | `hist.pantheon.site_name` | site, 0, 0 | once per shrine, temple or sanctum (6.4, 6.7, 6.9) |
| 113 | `hist.pantheon.cult_found` | the act's event, 0, 0 | once per act at most (6.7) |
| 114 | `hist.pantheon.cult_name` | faction, 0, 0 | once per cult (6.7) |
| 115 | `hist.pantheon.cult_spread` | faction, civilization, 0 | once per (cult, candidate realm) and step at most (6.8) |
| 116 | `hist.pantheon.adopt` | faction, civilization, 0 | once per (cult, realm) and step at most (6.9) |
| 117 | `hist.pantheon.suppress` | faction, civilization, 0 | once per (cult, realm) and step at most (6.9) |
| 118 | `hist.pantheon.hide` | faction, 0, 0 | once per cult at most (6.9) |
| 119 | `hist.deep.stir` | site, 0, 0 | once per mine and Q4 at most (12.2) |
| 120 | `hist.deep.warren_stir` | settlement, 0, 0 | once per warren and Q4 at most (12.2) |

All through `ctx.draw` (the step's year is part of the draw; P3-02 4.1). `chance_pm`, `chance_ppm`, `pick`, `weighted` as P5-02 3 and P3-02 4.1. Figures created by this stage use the existing streams with their existing keys: `hist.names.person` (figure id), `hist.figures.life` (figure id, `D_at` the entry year). Monsters sent by a god use `hist.names.monster` (monster id) and `hist.monsters.lair_name` (site id). God names use `hist.names.god` (god id, P3-02 4.5). All arithmetic int64 with checked multiplication; `div` is floor division of non-negative values. Y is the step's year (a quiet step's start), L its length.

# Rules

## 4. Data

### 4.1 Data kind `god` (new, required; `game/data/gods/`, one file per god)

A required kind whose directory may hold no definition (then the pantheon is the creators alone). Fields, all required:

| Field | Type | Meaning |
| --- | --- | --- |
| `name_culture` | culture id | the culture whose `god_patterns` name it (P3-02 4.5); check `god_culture`: `name_culture <x> is not a culture` |
| `camp` | enum(`good`, `neutral`, `evil`) | its camp |
| `domain` | enum(`war`, `death`, `harvest`, `craft`, `wild`, `oaths`, `lore`, `ruin`) | its domain (`creation` belongs to the creators and is refused here: grammar) |
| `title` | string(40, `god_title`) | grammar `god_title`: `the ` followed by ASCII letters, single spaces, `'` and `-`; e.g. `the Keeper of Oaths` |
| `favour` | record {`bless`, `champion`, `mark`} int(0, 1000) | weights of the kinds of a favourable act (6.3) |
| `wrath` | record {`curse`, `monster`, `mark`} int(0, 1000) | weights of the kinds of a wrathful act |
| `bless` | enum(`plenty`, `valour`) | what its blessing does (6.4) |
| `curse` | enum(`pestilence`, `withering`, `defeat`) | what its curse does |
| `monsters` | array(monster id, 0, 8) | the kinds it may send; check `god_monsters`: each a `monster` definition with `wake` `wild` or `dead` (`monsters[<i>] <k> is not a wild or dead monster`); check `god_monster_weight`: `wrath.monster` > 0 needs at least one (`wrath.monster needs monsters`) |
| `deeds` | array(deed, 0, 32) | what moves its regard (4.3) |

Checks also: `god_favour_weights` (the three `favour` weights are not all 0: `favour weights must not all be 0`), `god_wrath_weights` likewise.

**Ids and order.** Pantheon gods take god ids n + 1, n + 2, … (n = the number of race definitions; the creators keep ids 1 … n, P3-03 C5) in the canonical id order of the `god` definitions (byte order of ids, P0-03 7.4). The shipped ids are the domain words so that the order reads naturally.

**Shipped** (taste; starting values for calibration, 15.2). With six races the ids are 7 to 15 in this order:

| god id | def id | name culture | camp | domain | title | favour b / c / m | wrath c / m / m | bless | curse | monsters |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 7 | `conquest` | orcish | evil | war | the Red Banner | 400 / 500 / 100 | 700 / 0 / 300 | valour | defeat | — |
| 8 | `craft` | dwarven | neutral | craft | the Hammer Under the Mountain | 600 / 200 / 200 | 700 / 0 / 300 | valour | defeat | — |
| 9 | `death` | goblin | evil | death | the Hollow Keeper | 400 / 0 / 600 | 300 / 500 / 200 | valour | pestilence | `undead` |
| 10 | `harvest` | faerie | good | harvest | the Giver of Harvests | 700 / 0 / 300 | 700 / 0 / 300 | plenty | withering | — |
| 11 | `lore` | elven | good | lore | the Lamp of the Wise | 500 / 200 / 300 | 600 / 0 / 400 | plenty | withering | — |
| 12 | `oaths` | elven | good | oaths | the Keeper of Oaths | 400 / 400 / 200 | 700 / 0 / 300 | valour | defeat | — |
| 13 | `ruin` | goblin | evil | ruin | the Burner of Halls | 500 / 300 / 200 | 400 / 400 / 200 | valour | pestilence | `dragon`, `giant` |
| 14 | `war` | human | neutral | war | the Shield of Battles | 400 / 500 / 100 | 700 / 0 / 300 | valour | defeat | — |
| 15 | `wild` | human | neutral | wild | the Hunter in the Green | 500 / 0 / 500 | 300 / 500 / 200 | plenty | withering | `troll`, `ogre`, `wyvern` |

Both fixture roots (`geo`, `geo-small`) gain the nine files. Fixture content **`pantheon-small`** is in memory (as P5-07's `depths-small`): monsters-small with `act_threshold` 30, `act_permille` 1000, `god_rest_years` 5, `pair_rest_years` 10, `cult_found_permille` 1000, `cult_spread_permille` 300, `adopt_permille` 300, `suppress_permille` 200, `hide_permille` 500, `flare_permille` 1000, so every path of 6 occurs in a 512 world.

### 4.2 Tuning `history.pantheon` (new fixed id of kind `history`; `game/data/history/pantheon.toml` and both fixture roots)

All required, in this order.

| Field | Type | Start | Meaning |
| --- | --- | --- | --- |
| `activity_permille` | record {`distant`, `normal`, `meddling`} int(0, 10000) | 300, 1000, 2500 | A, by `param.divine_activity` (P5-00 5) |
| `regard_max` | int(1, 100000) | 400 | regard is clamped to [−max, max] |
| `regard_fade_per_century` | int(0, 100000) | 20 | regard moves toward 0 by this per 100 years (6.2) |
| `regard_memory_years` | int(1, 100000) | 300 | a contribution older than this is forgotten (not cited) |
| `contributions_max` | int(1, 64) | 8 | contributions kept per pair |
| `act_threshold` | int(1, 100000) | 100 | the regard at which a god may act |
| `act_permille` | int(0, 1000) | 150 | yearly chance of an act, before A; 0: no god ever acts |
| `god_rest_years` | int(0, 10000) | 30 | a god acts at most once in this many years |
| `pair_rest_years` | int(0, 10000) | 100 | a god acts on one realm at most once in this many years |
| `flare_regard` | int(1, 100000) | 200 | the regard at which a god may stir a quiet age (6.6) |
| `flare_permille` | int(0, 1000) | 100 | chance per Q4, before A |
| `bless_years`, `curse_years` | int(1, 1000) | 50, 50 | how long a blessing's or a curse's effect lasts |
| `plenty_growth_permille` | int(0, 1000) | 300 | added to the growth of a blessed realm's settlements (7.1) |
| `withering_growth_permille` | int(0, 1000) | 500 | taken from it under a withering |
| `valour_permille` | int(0, 500) | 50 | added to a blessed realm's force in war (7.1) |
| `defeat_permille` | int(0, 500) | 60 | taken from it under a curse of defeat |
| `pestilence_toll_permille` | int(0, 1000) | 120 | the share of each settlement of the capital region that a pestilence kills |
| `send_hops` | int(0, 8) | 2 | a sent monster lairs within this many borders of the realm's capital region |
| `sacred_growth_permille` | int(0, 1000) | 200 | added to the growth of a settlement under a sacred shrine |
| `profane_years` | int(0, 10000) | 300 | a profane shrine's region is shunned for founders this long |
| `champion_epithet` | string(24, `god_form`) | `Chosen of {god}` | a champion's epithet; `{god}` exactly once |
| `cult_found_permille` | int(0, 1000) | 250 | chance that a favourable act founds a cult |
| `cult_fear_permille` | int(0, 1000) | 500 | factor on it after a wrathful act (a cult of appeasement) |
| `cult_spread_permille` | int(0, 1000) | 20 | yearly chance per candidate realm, before affinity |
| `affinity_permille` | record {`same`, `near`, `opposed`} int(0, 1000) | 1000, 500, 100 | factor by the god's camp against the realm's race's camp (6.8) |
| `adopt_regard` | int(0, 100000) | 50 | a realm takes a cult as its faith only when its god's regard is at least this, or the cult was founded in it |
| `adopt_permille` | int(0, 1000) | 15 | yearly chance, before temper |
| `suppress_permille` | int(0, 1000) | 10 | yearly chance, before temper |
| `temper_adopt_permille` | record {`steady`, `warlike`, `peaceable`, `grasping`, `just`} int(0, 4000) | 1000, 500, 1500, 700, 1000 | factor by the reign's temper (P5-06 6.3) |
| `temper_suppress_permille` | record, int(0, 4000) | 1000, 1500, 500, 1000, 1300 | |
| `faith_points` | int(0, 100000) | 60 | regard of the god toward a realm that takes its cult as faith |
| `rival_faith_points` | int(0, 100000) | 40 | regard lost by each god of the opposed camp, and by the realm's creator when the god's camp differs from the creator's |
| `suppress_points` | int(0, 100000) | 120 | regard lost by a cult's god toward the realm that suppresses it |
| `hide_permille` | int(0, 1000) | 400 | chance that a cult suppressed in its last realm goes into hiding |
| `sanctum_hops` | int(0, 8) | 3 | a sanctum lies within this many borders of the seat's region |
| `creator` | record {`favour`, `wrath`, `bless`, `curse`, `monsters`, `deeds`} | 4.3 | the fields of 4.1 for every creator god (kin: its race) |

Checks: `pantheon_flare_above_act` (`flare_regard <f> is below act_threshold <a>`); `pantheon_act_below_max` (`act_threshold <a> is above regard_max <m>`); the `god_*` checks of 4.1 on `creator`. **Code defaults** (a tuning built in code): `act_permille` 0, `cult_found_permille` 0, `flare_permille` 0, the others their starts (inert). The **`zero`** fixture tuning: shipped values with `act_permille` 0 and `flare_permille` 0 (I0, 14).

`history.chronology` gains `creator_title`, string(40, `god_title` with `{plural}` once), start **`the Maker of {plural}`**: a creator's title (6.1). Code default the start.

### 4.3 Deeds

A deed is a record {`kind`: event kind key, `role`: role key, `points`: int(−1000, 1000), `who`: enum(`kin`, `any`, `foe`), `against`: role key or empty}, read by 6.2. Checks (`pantheon_deed_*`, each `deeds[<i>]: …`): `kind` is a kind of the compiled catalogue (`kind <k> is unknown`); `role` and a non-empty `against` are roles of it with entity type civilization (`role <r> of <k> is not a civilization role`); `against` ≠ `role`; `points` ≠ 0; `who` = `foe` needs `points` < 0 (`a deed for foes must be negative`).

**Kin and foe.** For a creator god g of race r_g: kin(g, c) when c's race is r_g; foe(g, c) when c's race's camp is the opposed one (good against evil; neutral has none). For a pantheon god of camp κ: kin(g, c) when c's race's camp is κ; foe(g, c) when it is κ's opposite. `any`: every realm.

**Shipped deeds** (written `kind.role ±points who [against role]`; taste, calibrated in 15.2):

| god | deeds |
| --- | --- |
| creators (`creator`) | `sacked.attacker −60 any against defender`; `settlement_captured.attacker −25 any against defender`; `civ_conquered.conqueror −120 any against civilization`; `war_won.winner +30 kin`; `monster_slain.slayer +25 kin`; `monster_slain_by_hero.slayer +35 kin`; `became_city.civilization +20 kin`; `deep_sealed.civilization +40 kin`; `refuge_held.defender +30 kin` |
| `conquest` | `civ_conquered.conqueror +100 kin`; `war_won.winner +30 kin`; `sacked.attacker +15 kin`; `peace.loser −30 kin`; `tribute.payer −20 kin`; `truce.parties −20 kin` |
| `craft` | `mine_opened.civilization +25 kin`; `delving_deepened.civilization +15 kin`; `arms_sold.seller +10 any`; `deep_sealed.civilization +30 any`; `deep_woken.civilization −40 any` |
| `death` | `sacked.attacker +20 any`; `blight.civilization +30 any`; `battle_led.winner +5 any`; `monster_slain.slayer −30 any`; `monster_slain_by_hero.slayer −40 any` |
| `harvest` | `became_town.civilization +10 kin`; `became_city.civilization +15 kin`; `trade_opened.partner_a +10 kin`; `trade_opened.partner_b +10 kin`; `blight.civilization −80 any`; `sacked.attacker −30 any` |
| `lore` | `tower_founded.civilization +30 any`; `book_written.civilization +20 any`; `blight.civilization −60 any` |
| `oaths` | `alliance_formed.ally_a +15 any`; `alliance_formed.ally_b +15 any`; `spoils_shared.principal +25 any`; `ally_joined_war.ally +20 any`; `alliance_betrayed.betrayed +40 any`; `alliance_betrayed.betrayer −150 any`; `spoils_withheld.principal −50 any`; `ally_refused.ally −25 any` |
| `ruin` | `sacked.attacker +40 kin`; `settlement_captured.attacker +15 kin`; `truce.parties −20 kin`; `trade_opened.partner_a −10 kin`; `trade_opened.partner_b −10 kin`; `became_city.civilization −15 foe` |
| `war` | `battle.winner +10 any`; `battle_led.winner +15 any`; `allied_battle.winner +10 any`; `war_won.winner +25 any`; `ally_refused.ally −30 any`; `ally_withdrew.ally −30 any` |
| `wild` | `colony_founded.founder −8 any`; `became_city.civilization −20 any`; `mine_opened.civilization −15 any`; `monster_slain.slayer −25 any`; `settlement_abandoned.civilization +15 any`; `outpost_abandoned.civilization +10 any` |

Creator `creator` record: favour 500 / 300 / 200, wrath 700 / 0 / 300, bless `plenty`, curse `pestilence`, monsters none.

Event rules stay code with their numbers in TOML (P5-00 4.6); the deed table is data in the manner of P5-12's renaming rules, so a later stage adds its kinds to a god without touching this subsystem.

### 4.4 Culture fields (amends P3-02 3)

Appended after `regnal_permille` (P5-06 4.2), all required (shipped and both fixture roots), each array(string(48, `god_form`), 1, 8); grammar `god_form`: `name_form`'s with the slot `{god}` exactly once in place of `{root}`:

| culture | `cult_forms` | `temple_forms` | `sanctum_forms` | `sacred_forms` | `profane_forms` |
| --- | --- | --- | --- | --- | --- |
| dwarven | `The Hammer of {god}`, `The Oathsworn of {god}` | `The Hall of {god}`, `The Deep Shrine of {god}` | `The Sealed Hall of {god}` | `The Hallowed Stone of {god}` | `The Shunned Delving of {god}` |
| elven | `The Children of {god}`, `The Starward Circle of {god}` | `The House of {god}`, `The White Fane of {god}` | `The Hidden Fane of {god}` | `The Blessed Grove of {god}` | `The Withered Grove of {god}` |
| faerie | `The Dreamers of {god}`, `The Hidden Ring of {god}` | `The Bower of {god}`, `The Moon-Glade of {god}` | `The Lost Bower of {god}` | `The Shining Ring of {god}` | `The Black Ring of {god}` |
| goblin | `The Teeth of {god}`, `The Grovellers of {god}` | `The Pit of {god}`, `The Bone-House of {god}` | `The Secret Pit of {god}` | `The Fat Stone of {god}` | `The Stinking Ground of {god}` |
| human | `The Brotherhood of {god}`, `The Order of {god}` | `The Temple of {god}`, `The Sanctuary of {god}` | `The Hidden Sanctum of {god}` | `The Holy Ground of {god}` | `The Cursed Ground of {god}` |
| orcish | `The Blood of {god}`, `The Howlers of {god}` | `The Blood-Hall of {god}`, `The War-Fane of {god}` | `The Black Cave of {god}` | `The Red Stone of {god}` | `The Ashen Field of {god}` |

Every word is checked against the name filter by the engineer; a failing word is a data defect. **Naming** (cults, temples, sanctums, shrines): F = the culture's forms, start = pick(D(stream; id, 0, 0), |F|); the first form in cyclic order from start, with `{god}` replaced by the god's name, that is valid (P5-12 5.2 items 1 to 4) and not in the registry; else the start form's name with ` ` and roman(k), the smallest k ≥ 2 free (P5-02 9.2's fallback); added to the registry. The culture is the realm's (the people who name the place).

## 5. State

`PantheonSubsystem` (new: `worldgen/src/history/pantheon.cpp`, `pantheon_cults.cpp`; header `worldgen/history/pantheon.hpp`; tuning id `history.pantheon`) keeps, in its own members, ids only:

- per god (index god id − 1): camp, domain, kin rule, the compiled deeds; `last_act_year` (−∞ none), `acts` (count);
- per pair (g, c), a `std::map` keyed (g, c): regard R, its contributions (event id, year, points) in append order, `last_act_year`;
- deeds by kind: a vector indexed by event kind id of (god, deed index) lists, built once in `prepare`, ascending god then deed index;
- effects: per civilization, the active effects {kind (plenty, withering, valour, defeat), until year, event} in append order;
- shrines: per open shrine (site id ascending) its region, settlement, profane flag, origin year;
- `pending_acts` (act event ids not yet tested for a cult, ascending);
- cults: ids 1, 2, … (H2; at most 100,000, fatal `history: bound: factions exceed 100000`): name, god, founding realm, founder, seat site, founded year and event, status (1 present, 2 hidden, 3 ended), ended year, ending event, ending code, its open temples (site ids), its sanctum (site id or 0); per god its live cult (0 none);
- `pending_flare` (an event id or 0) and `flared` (god, civilization, deed event) set with it (6.6);
- the log cursor.

`MonstersSubsystem` keeps, per site of kind 3 and per warren, a `stirring` flag (12.2).

## 6. Rules

### 6.1 The pantheon (chronology; amends P3-03 C5, 5.2, 8.1)

**C5 (creators)** unchanged in id, camp and name; each creator row also gets `domain` 1 (`creation`), `title` = `history.chronology.creator_title` with `{plural}` replaced by the race's `plural` (`the Maker of Elves`) and `race` = the race id.

**C5a (the rest of the pantheon)**, after C5, for each `god` definition d in canonical id order, i = 1, 2, …: god `id` = n + i, `camp` = d's camp (0 good, 1 neutral, 2 evil), `domain` = d's domain code (9), `title` = d's title, `race` = 0, `name` = `make_name(naming of d.name_culture, that culture's id, NameKind::god, seed, c = that culture's id, e = id, registry)`.

**8.1 order of naming** gains item 4: the pantheon's names, in god id order, after the creators. So no region, age or creator name moves; the history stage rebuilds the registry from every `god.name` as before (8.2).

`report`'s `[chronology]` key `gods` counts every row (15 with the shipped data); no new key.

### 6.2 Regard (T4, Q4: the log, then the fade)

At its T4 and Q4 the subsystem reads every event from its cursor in ascending id (P5-06 6.2's manner) and, for an event e of kind κ and year y, applies each deed d of `deeds_by_kind[κ]` (ascending god, then deed index):

```
for each participant of e in role d.role, ascending seq: civilization r (skip 0)
    skip unless who(d.who, g, r)                      // kin(g, r), true, or foe(g, r)
    skip if d.against ≠ "" and no participant of e in role d.against is a kin of g
    skip if d.points > 0 and foe(g, r)                // a god never favours its foes
    skip if r has ended before y
    add(g, r, d.points, e)

add(g, c, p, e):  R(g,c) = clamp(R(g,c) + p, −regard_max, regard_max)
                  append (e, y, p) to the pair's contributions
                  drop every contribution with year < y − regard_memory_years
                  while more than contributions_max: drop the first (oldest)
```

The subsystem's own events add regard through 6.9's rules (faith, suppression), by the same `add`. Then the **fade**, once per step after the log: amount = F(Y + L) − F(Y) with F(t) = `regard_fade_per_century` × t div 100; for every pair in key order, R moves toward 0 by min(|R|, amount). (F is cumulative, so the fade over a span does not depend on how steps cut it.) A pair whose realm has ended is dropped from the map at the step after its ending event is read.

### 6.3 The act (engine step T5; amends P3-04 6.8)

**The source** (`PantheonSubsystem::interventions`, pure). In each tick year, the candidates are the pairs (g, c) with |R| ≥ `act_threshold`, c alive and visible, `Y − last_act_year(g) ≥ god_rest_years`, `Y − last_act_year(g, c) ≥ pair_rest_years`, at least one contribution whose sign is R's, and: when R > 0, not foe(g, c); when R < 0, c's faith is not g's cult (6.9). The **candidate** is the one with the largest |R|, then the smaller g, then the smaller c. None: no intervention.

```
p = min(1000, act_permille × A div 1000)
certain = (6.6's flared pair is (g, c) and this is the first tick year of the phase whose trigger is its deed)
if not certain and not chance_pm(D(hist.pantheon.act; g, c, 0), p): no intervention
W = favour weights (bless, champion, mark) when R > 0, else wrath weights (curse, monster, mark), of g (a creator: `creator`)
    with 0 for a kind not eligible (below)
all W = 0: no intervention
kind = [the kind of index weighted(D(hist.pantheon.kind; g, c, 0), W)]
return { Intervention{ year Y, god g, kind, target civilization c, region ρ, scripted false } }
```

At most one intervention a year. ρ is 6.4's region of the kind. **Eligibility** (pure reads):

| kind | eligible when |
| --- | --- |
| bless | always |
| curse | c has a standing visible capital (pestilence reads its region) |
| champion | c has no living champion (role 6), the figures seam is set, and c has work for a champion in year Y: c is a side or an open joined ally of an open war, or some awake, unsealed monster remembers a raid on c within `hunt_memory_years` and c's hunters against it (P5-02 6.4.2, before any hero) are above 0 |
| monster | g has a `monsters` kind with living count < `max_alive` and 6.4's lair search finds a region |
| mark | 6.4's place exists |

**Causes of an act**: the contributions of the pair whose sign is R's, ranked by (|points| descending, id descending), the first three, listed ascending. The **lead deed** e* is the first of that ranking.

**The engine** (P3-04 6.8 amended, 7.6): validates each intervention as before with the supported kinds `bless`, `curse`, `champion`, `monster`, `mark` and target type civilization only (alive, visible), then calls the applier (`PantheonSubsystem::apply`) for each valid one in source order. The seeded source returns only feasible acts, so `apply` finding an act impossible is fatal (`history: pantheon: <kind> of god <g> on civilization <c> could not be applied`).

**Apply**, in this order: decide every draw and value (6.4); append the event (8.1; causes as above; `telling` = (c + acts(g)) mod 3); make the effect (6.4); then **spend**: R(g, c) = 0, the pair's contributions cleared, `last_act_year(g)` = `last_act_year(g, c)` = Y, `acts(g)` += 1; append the event id to `pending_acts`.

### 6.4 The five kinds

| kind | region ρ | what it does | event |
| --- | --- | --- | --- |
| **bless** | c's capital's region (capital 0: home region) | an effect for `bless_years` from Y: `plenty` (growth + `plenty_growth_permille`) or `valour` (force + `valour_permille`), by g's `bless` (7.1) | `divine_bless` (`effect` 1 plenty, 2 valour; `years`) |
| **curse** | c's capital's region | `pestilence`: for each standing visible settlement u of c in ρ ascending, dead_u = P_u × `pestilence_toll_permille` div 1000; `set_population(u, P_u − dead_u)` and `note_reduction(u, event)` for dead_u > 0 (P3-05 13); `withering`: growth − `withering_growth_permille` for `curse_years`; `defeat`: force − `defeat_permille` for `curse_years` | `divine_curse` (`effect` 1 pestilence, 2 withering, 3 defeat; `dead` = Σ dead_u, 0 unless pestilence; `years`, 0 for pestilence) |
| **champion** | c's capital's region | a figure f = `FiguresSubsystem::next_figure_id()`, role 6 champion, race and culture of c, civilization c, entry Y, ages by P5-06 6.9 with `hero_age_*`, `skill` = `skill_max`, epithet = `champion_epithet` with `{god}` = g's name; name by P5-06 6.11 | `divine_champion`, then `FiguresSubsystem::raise_divine(ctx, f, 6, c, event, epithet)` (7.5) |
| **monster** | the lair's region | kind k = the eligible kinds of g's `monsters` in data order, `weighted(D(hist.pantheon.monster_kind; g, c, 0), [wake_rate_k])`; the lair region: BFS from c's capital region over region borders (P5-02 7.1's distances), regions within `send_hops` with no open lair and a free wild cell (P5-02 6.6.1), by (hops ascending, id ascending), the first | `divine_monster`; the monster and its lair by `MonstersSubsystem::prepare_sent` / `create_sent` (7.3) |
| **mark** | the place's region | the **place**: if e* has a settlement u that has never had a shrine: u's cell, `settlement` u (u may be ruined); else the region R' = e*'s region, else c's capital region, when R' has no open shrine and a free height cell (`height_cell(ctx, R', {})`, P5-04a 21.4): that cell, `settlement` 0; else not eligible. Sacred when R > 0, profane when R < 0 | `divine_mark` (`profane` 0/1); the shrine s = `next_site_id()`, named by 4.4 (`sacred_forms` or `profane_forms`, stream `hist.pantheon.site_name`), `open_site` kind 5 with `founder_civ` c, `monster` 0, origin Y and the event |

**Effects in force.** An effect of an act in year Y is in force in every year y with Y ≤ y < Y + `years`. Several effects of one realm add; `growth_permille` is clamped to [−900, 1000] and `force_permille` to [−500, 500]. A **sacred** shrine at a standing settlement u adds `sacred_growth_permille` to u's growth while it is open (shrines never end, 9). A **profane** shrine makes its region **shunned** for founders in the years origin ≤ y < origin + `profane_years`.

**Sent monsters** stand outside the world cap L_world of P5-02 6.3 and the one-lair-per-region test of a wild appearance (as P5-07's deep kinds, L-4), and are limited by their kind's `max_alive`. A sent monster is born awake; its years are P5-02 6.4's (raids within `range_hops`, sleep, waking, hunts, heroes). Its lair counts for every rule that reads lairs.

### 6.5 What other subsystems see

| Subsystem | Reads (7) | Effect |
| --- | --- | --- |
| Peoples (P3-05) | `DivineView::growth_permille`, `shunned` | the positive growth term × (1000 + growth) div 1000; a shunned region is skipped by site and colony searches as P5-02 7.2's shunned regions |
| Conflict (P3-06) | `DivineView::force_permille` | force × (1000 + force) div 1000, after P5-04 7.5's and P5-07 6.3's factors |
| Monsters (P5-02) | `prepare_sent`, `create_sent` | a monster and a lair; `born_event` the `divine_monster` |
| Figures (P5-06) | `raise_divine`; `champion_of(c)` | the champion is the realm's hunter ahead of its hero (`hero_of`), is appointed general with no draw at the realm's next declaration (6.6 of P5-06, first choice), and at its raising takes command of an open war of its realm without a general, or waits for that war's general to die (7.5); 6.10's epithets skip champions and prophets; adds `cult_adopted` and `cult_suppressed` (role `civilization`) to the deed list of 6.4 rule 3 (the ruler is named) |
| Relations, alliances | — | no seam: a god's acts move no disposition and no alliance |
| Renaming (P5-12) | — | no rule in this stage (a later data change may add one) |

### 6.6 The gods' flare-up (Q4; tension source)

In Q4, after 6.2: `pending_flare` = 0 and `flared` cleared; then the pair (g, c) with the largest |R| ≥ `flare_regard` that would be a candidate of 6.3 at year Y (same tests), ties as 6.3: if `chance_pm(D(hist.pantheon.flare; g, c, 0), min(1000, flare_permille × A div 1000))`: `pending_flare` = its lead deed e*, `flared` = (g, c, e*). At most one per Q4.

`PantheonSubsystem::tension(ctx)` returns {T_d, `pending_flare`} when `pending_flare` ≠ 0, else {0, 0} (T_d the world's `flare_threshold`; P5-02 6.5's form; pure). Tension sources in order: conflict, monsters, pantheon; the engine keeps the first source on a tie, so the gods flare a quiet age only when neither war nor a monster would have. In the first tick year of a flare-up whose `phase_trigger_event` is e*, the flared pair acts with no `hist.pantheon.act` draw (6.3 `certain`) if it is still a candidate; a god's flare-up forces no war (P5-02's rule for monster flare-ups).

### 6.7 Cults: founding (T4 and Q4, after 6.2)

For each act a in `pending_acts`, ascending (each tested once; in Q4 it waits while `quiet_events_left(c)` = 0; removed after its test):

```
c = a's civilization; g = a's god
skip a (removed) unless c alive and visible and g has no live cult (status 1 or 2)
p = cult_found_permille × (1000 when a is bless, champion or a sacred mark; cult_fear_permille when curse, monster or a profane mark) div 1000
if not chance_pm(D(hist.pantheon.cult_found; a, 0, 0), p): removed
seat t = the settlement of a's shrine when a is a mark at a settlement that is standing, visible and c's;
         else c's capital; t must have no open temple (kind 6); else c's standing visible settlement without an open temple
         with the largest population, then the smaller id; none: removed (no founding)
founder f = a's champion when a is a champion and it lives; else c's living caster; else c's reigning recorded ruler (FigureView);
            else a prophet: f = next_figure_id(), role 7, race, culture and civilization of c, entry Y, ages by caster ages (P5-06 6.9)
k = the next faction id; its name by 4.4 (`cult_forms`, stream hist.pantheon.cult_name, key k); the temple s = next_site_id(), its name (`temple_forms`, key s)
append cult_founded (region of t, settlement t; participants k faction, g god, c civilization, f founder, s site; value telling = k mod 3; cause a)
a prophet: FiguresSubsystem::raise_divine(ctx, f, 7, c, cult_founded, "")
open_site(kind 6, name, region of t, t's cell, settlement t, founder_civ c, monster 0, origin Y, cult_founded)
the faction row (kind 1, civ c, god g, founder f, seat s, founded Y and the event, status 1); g's live cult = k
open relation cult_in (a = k type 7, b = c type 3) with cult_founded
```

One live cult per god (L-6). A cult of a god g may be founded among any people, an evil god's among a good people included (Q-P5-08-4).

### 6.8 Cults: spread (T4 and Q4)

For each cult k with status 1, ascending: the **candidates** are the realms a, ascending id, alive and visible, with no open `cult_in`(k, a), that hold an open `alliance` or `trade` relation with some realm c having an open `cult_in`(k, c) (read through `AllianceView::partners`, 7.4); the **link** of a is the smallest such c and its relation (alliance before trade, then the smaller `since`).

```
affinity = same when camp(g) = camp(r_a) (camps are compared, never races: a creator's camp is its race's, so its cult
           is `same` in every realm of a race of that camp), opposed when good against evil, else near
p = cult_spread_permille × affinity div 1000                   // T4
p = min(1000, p × L)                                           // Q4; quiet_events_left(a) > 0, else next a (no draw)
for each candidate a in order, until one succeeds: if chance_pm(D(hist.pantheon.cult_spread; k, a, 0), p):
    append cult_spread (region a's capital region; participants k faction, g god, a civilization, c from;
                        causes k's cult_founded and the link's opening event when it is not 0, ascending)
    open cult_in(k, a) with it
```

The comparison is of camps (integration E2, lead ruling): a creator's cult spreads to a realm of another race of its camp at the `same` chance, and across camps at the lower chances, which keeps cross-camp cults rare (Paul's decision: a tenth of the rate). A cult founded in this step is no candidate cult (6.12 item 4): it spreads from the next step. At most one spread per cult per step. A cult spreads only along links (design 9's "a guild's reach follows trade and alliance links").

### 6.9 Cults: faith, suppression, hiding (T4 and Q4, after 6.8)

The realms with an open `cult_in`, in (k, c) order, each at most one of these per step; a cult founded in this step takes no part until the next step (6.12 item 4):

**Faith.** Eligible: c has no open `faith_of`, c alive and visible, and R(g, c) ≥ `adopt_regard` or k's founding realm is c. p = `adopt_permille` × `temper_adopt_permille`[τ_c] div 1000 (τ_c the current reign's temper, `FigureView::temper(c)`); Q4: min(1000, p × L) with the budget. `chance_pm(D(hist.pantheon.adopt; k, c, 0), p)`: a temple s at c's capital t when t is standing and has no open temple (named as 6.7); append **`cult_adopted`** (region of t, settlement t; participants k faction, g god, c civilization, s site 0–1; cause c's `cult_founded` or `cult_spread` of k); `open_site` when s; open **`faith_of`**(k, c) with it; `add(g, c, faith_points, e)`; for every god g' whose camp is opposed to g's, ascending, and for c's creator when its camp differs from g's: `add(g', c, −rival_faith_points, e)`. A realm keeps its faith until the cult or the realm ends.

**Suppression.** Eligible (and not c's faith): camp(g) is opposed to camp(r_c), or c's faith is a cult of a god whose camp is opposed to g's. p = `suppress_permille` × `temper_suppress_permille`[τ_c] div 1000; Q4 as above. `chance_pm(D(hist.pantheon.suppress; k, c, 0), p)`:

1. Close `cult_in`(k, c); every open temple of k whose settlement is c's ends with code 8 (suppressed) and this event (end order: site id ascending).
2. If k has no other open `cult_in`: `chance_pm(D(hist.pantheon.hide; k, 0, 0), hide_permille)` and a **sanctum place**: the regions within `sanctum_hops` of k's seat's region by (hops, id) with a free height cell, the first. Hidden: a sanctum site s' (kind 7, settlement 0, `founder_civ` c, named by `sanctum_forms`), status 2. Otherwise k **ends**: status 3, ending 1 (suppressed), ended year Y, ending event this event; every open temple of k ends with code 7 and this event.
3. Append **`cult_suppressed`** before 1 and 2 take effect in the rows (region c's capital region; participants k faction, g god, c civilization, s' sanctum 0–1; value `hidden` 0/1; cause c's `cult_founded`, `cult_spread` or `cult_adopted` of k, the latest); `add(g, c, −suppress_points, e)`.

So suppression angers the god, and its wrath may follow by 6.3 with the suppression among its causes. **A hidden cult** stays at its sanctum to E: it spreads no more, is never suppressed again, and is a place for P6 (20). It does not re-emerge in this stage (Q-P5-08-3).

### 6.10 Cults: endings and temples (T4 and Q4, first after the log)

- **A realm's end** (`civ_ended` or `civ_conquered` of c read in the log): `cult_in`(k, c) and `faith_of`(k, c) close with that event (keys copied first, P3-04 6.9 rule 1). A cult with status 1 left with no open `cult_in` ends: status 3, ending 2 (its realms fell), ended year and event those of the realm's ending event; its open temples end with code 7 and that event.
- **Temples** (each open site of kind 6 ascending, of cult k, at settlement t, founded by c): t not standing → ending 4 with t's `ruin_event`; t's owner o ≠ c and o has no open `cult_in`(k, o) → ending 4 with t's `owner_event`. An ending event older than the temple waits (P5-04a 21.4 r3's reading). The ended year is the ending event's year.
- **At the end of the history** (integration E3): a realm whose `civ_ended` or `civ_conquered` falls after the pantheon's last T4 (in T5 or T6 of the last year, E − 1) has no step left to read it. So in T6 of year E − 1, after every other subsystem (the order of 6.12 item 3, the pantheon last, as figures' present of P5-06 6.12), the pantheon reads the ending events of the log after its T4 cursor, in event order, and applies the two rules above exactly as its next step would have: first the realm's end (each `cult_in`(k, c) and `faith_of`(k, c) closes with the realm's ending event; a cult with status 1 left with no open `cult_in` ends with status 3, ending 2, the ended year and event those of the realm's ending event, and its open temples end with code 7), then the temple rule (ending 4 with the ruin or owner event, the wait of an older event kept). The ending year is the ending event's year, which is E − 1. No event is written (neither rule writes one), no draw is made, and the regard deeds of these events are not read (no act follows). A cult cannot hide here: hiding is a suppression's (6.9).
- **Shrines** (kind 5) and **sanctums** (kind 7) never end.
- A god never ends. A god with no cult and no follower is unchanged: its regard and acts go on.

### 6.11 `--divine-script` (amends P3-01, P5-00 5)

Stays refused (design 5: "The divine script stays refused through phase P5"; design 14: an experiment after the prototype). The refusal text becomes **`divine scripts are not supported yet`** (was `divine scripts need phase P5`; P5-00 5). What this stage fixes for a later script source: the five kind keys, the target type (civilization), the engine's validation (6.3), and the source/applier split (7.6): a script source returns `Intervention`s with `scripted` true, and `apply` writes them with `divine_script` 1 and the causes it can find (none is allowed: the kinds are not `caused`, 8.1). Q-P5-08-5.

### 6.12 P3-04 6.9, visibility, quiet steps

1. `PantheonSubsystem` founds no civilization or settlement. It opens and closes `cult_in` and `faith_of` only outside loops over `relations()`, with keys copied first; it creates figures, monsters and sites only through the seams (7), never inside a loop over another subsystem's lists; its own loops run over ids up to a count taken before the loop; members hold ids.
2. `tension()` and `interventions()` are pure.
3. Order: `subsystems` = peoples, conflict, renaming, monsters, relations, alliances, figures, **pantheon**; `tension` = conflict, monsters, **pantheon**; the engine's `interventions` = the pantheon, `applier` = the pantheon.
4. Visibility: pantheon's T4 runs last in T4 and sees every change of the step; entities created in the step are invisible (P5-02 6.7): a cult founded in the step is not a candidate for spread (6.8), faith or suppression (6.9) until the next step. T5 sees the state after T4.
5. The pantheon also runs in T6, only in the history's last year E − 1, for the finish of 6.10 (integration E3); in every other T6 it does nothing.
5. Keys: god, civilization, faction, site, settlement and event ids; never list positions.
6. **Quiet steps**: `cult_founded`, `cult_spread`, `cult_adopted`, `cult_suppressed` and (12.6) `tower_taken_up`, `tower_abandoned` may occur, each charged to its first civilization participant through `quiet_events_left`. Acts (100 to 104) occur in tick years only. No world-level pantheon event.

## 7. Seams

### 7.1 `DivineView` (new, `worldgen/history/pantheon.hpp`; implemented by `PantheonSubsystem`, set on peoples and conflict by the stage)

```cpp
class DivineView {
public:
    virtual ~DivineView() = default;
    virtual std::int64_t growth_permille(std::int64_t civ, std::int64_t settlement, std::int64_t year) const = 0;  // [-900, 1000]
    virtual std::int64_t force_permille(std::int64_t civ, std::int64_t year) const = 0;                            // [-500, 500]
    virtual bool shunned(std::int64_t region, std::int64_t year) const = 0;
};
```

Unset (tests, the I0 build): 0, 0, false. **Peoples** (amends P3-05's growth): the positive growth term of a settlement u of owner c in T1, and Q1's over the step, is multiplied by (1000 + `growth_permille(c, u, Y)`) div 1000; a zero or negative term is unchanged. The site and colony searches (P3-05 6.2) skip a region with `shunned(region, Y)`. **Conflict** (amends P3-06 7.3's force, after P5-07 6.3): f = f × (1000 + `force_permille(c, Y)`) div 1000.

### 7.3 Monsters (amends P5-02 6.3, 11.4)

```cpp
struct SentMonster { std::int64_t monster, lair, kind, region; SiteCell cell; std::string name, lair_name; };
SentMonster MonstersSubsystem::prepare_sent(const StepContext&, std::int64_t kind, std::int64_t region) const;  // names, cell; no state change
void MonstersSubsystem::create_sent(StepContext&, const SentMonster&, std::int64_t event);                     // monster awake, born Y; lair kind 1
std::int64_t MonstersSubsystem::lair_region_for_sent(const StepContext&, std::int64_t from_region, std::int64_t hops) const;  // 6.4's search, 0 none
```

The lair search and a wild appearance share P5-02 6.6.1's free-cell rule. `living()` (P5-07 7.2) counts sent monsters (they are wild or dead kinds); L_world's test skips monsters whose `born_event` is a `divine_monster`.

### 7.4 `AllianceView::partners` (amends P5-04 7.1)

`virtual std::vector<std::pair<std::int64_t, std::int64_t>> partners(std::int64_t c) const = 0;`: every civilization with an open `alliance` or `trade` relation with c, ascending id, each with its opening event (the alliance's when both; 0 for an origin alliance).

### 7.5 Figures (amends P5-06 6.6, 6.7, 6.10, 7.1, 7.7)

`std::int64_t next_figure_id() const`; `void raise_divine(StepContext&, std::int64_t figure, std::int64_t role, std::int64_t civ, std::int64_t event, std::string_view epithet)` (creates the figure with `start_event` = event, name, life by 6.9; fatal if `figure` is not the next id); `FigureView::champion_of(c)` (0 none), `caster_of(c)`, `ruler_figure(c)` (the reigning recorded ruler, 0 none), `temper(c)` (the current reign's temper). `hero_of(c)` returns c's living champion, else its hero (lead ruling PN5: the champion hunts first; the hunt's chance and reach are unchanged). **Appointment** (6.6): a side whose realm has a living champion not serving another war appoints it, with no `hist.figures.appoint` draw; otherwise as before. **A champion raised in war** (lead ruling PN5): when `raise_divine` makes a champion of c and c is the attacker or defender of open wars, the champion is appointed at once (`general_appointed`, causes the declaration and the `divine_champion`, no draw) to the open war of smallest declaration id whose side has no general; when every such side has a living general, the champion waits on each and takes the post, with no draw, in the next T3 vacancies pass after that general dies of age (a fall already leaves a vacancy that appoints the champion first); a joined ally has no general (P5-06 6.6) and gets no post. A champion or prophet dies as a hero or caster does (6.9; `figure_died` with the existing `how` of non-rulers), or falls in battle or to a monster.

### 7.6 The engine (amends P3-04 6.4, 6.8)

```cpp
class InterventionApplier {
public:
    virtual ~InterventionApplier() = default;
    virtual void apply(StepContext&, const Intervention&) = 0;
};
// EngineConfig gains: InterventionApplier* applier;   // null only with EmptyInterventionSource (fatal otherwise)
```

In T5 of each tick year: `list = interventions->interventions(ctx)`; for each in order, validated as P3-04 6.8 with the supported kinds of 6.3, `applier->apply(ctx, i)`; invalid ones are dropped without an event.

### 7.7 P3-04 6.9

As 6.12. Lint rule `history-member-held-reference` covers the new file (the auditor decides on any new rule).

## 8. Events

### 8.1 Catalogue (appended after 99; ids final if no other stage merges first)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values |
| --- | --- | --- | --- | --- | --- | --- |
| 100 | `divine_bless` | 0 | 1 | 0 | 1 `actor` god; 2 `civilization` civ | `effect` (1–2), `years`, `telling` (0–2), `divine_script` (0–1) |
| 101 | `divine_curse` | 0 | 1 | 0 | 1 `actor` god; 2 `civilization` civ | `effect` (1–3), `dead` (≥ 0), `years`, `telling`, `divine_script` |
| 102 | `divine_champion` | 0 | 1 | 0 | 1 `actor` god; 2 `figure` figure; 3 `civilization` civ | `telling`, `divine_script` |
| 103 | `divine_monster` | 0 | 1 | 0 | 1 `actor` god; 2 `monster` monster; 3 `civilization` civ; 4 `lair` site | `telling`, `divine_script` |
| 104 | `divine_mark` | 0 | 1 | 2 | 1 `actor` god; 2 `civilization` civ; 3 `site` site | `profane` (0–1), `telling`, `divine_script` |
| 105 | `cult_founded` | 1 | 1 | 1 | 1 `faction` faction; 2 `god` god; 3 `civilization` civ; 4 `founder` figure; 5 `site` site | `telling` (0–2) |
| 106 | `cult_spread` | 1 | 1 | 0 | 1 `faction` faction; 2 `god` god; 3 `civilization` civ; 4 `from` civ | — |
| 107 | `cult_adopted` | 1 | 1 | 1 | 1 `faction` faction; 2 `god` god; 3 `civilization` civ; 4 `site` site 0–1 | — |
| 108 | `cult_suppressed` | 1 | 1 | 0 | 1 `faction` faction; 2 `god` god; 3 `civilization` civ; 4 `sanctum` site 0–1 | `hidden` (0–1) |
| 109 | `tower_taken_up` | 1 | 1 | 2 | 1 `figure` figure; 2 `civilization` civ; 3 `site` site | — |
| 110 | `tower_abandoned` | 1 | 1 | 2 | 1 `site` site; 2 `civilization` civ; 3 `figure` figure | — |

(god = entity type 2, civ 3, figure 6, faction **7** (first use), monster 8, site 9; unlisted counts 1–1.) Kinds 100 to 104 are not `caused` so that a scripted act (6.11) may have none; every seeded act has one to three causes (invariant 2). The writer checks faction ids as P5-06 8.5 checks figure ids. Kinds 109 and 110 are the figures subsystem's (12.6).

### 8.2 Templates and salience (data, `history.report`)

**Cause words** (one list, used by kinds 100 to 104 through `{@causes:…}`, P3-04 r12 20.2 and P5-04a 6.2's comma form): `sacked=the sack of a city`, `settlement_captured=a city taken`, `civ_conquered=a realm conquered`, `war_won=a war won`, `battle=a battle won`, `battle_led=a battle won`, `allied_battle=a battle won`, `monster_slain=a monster slain`, `monster_slain_by_hero=a monster slain`, `became_city=a city grown great`, `became_town=a town grown`, `deep_sealed=the deep sealed`, `deep_woken=greed in the deep`, `refuge_held=a refuge held`, `peace=a peace made`, `tribute=tribute paid`, `truce=a truce made`, `mine_opened=a mine opened`, `delving_deepened=deep delving`, `arms_sold=arms sold`, `blight=a blighted land`, `trade_opened=trade opened`, `tower_founded=a tower raised`, `book_written=a book written`, `alliance_formed=an oath sworn`, `spoils_shared=spoils shared`, `ally_joined_war=an ally's faith kept`, `alliance_betrayed=an oath broken`, `spoils_withheld=spoils withheld`, `ally_refused=a call to arms refused`, `ally_withdrew=an ally's flight`, `colony_founded=the felling of the wild`, `settlement_abandoned=land given back to the wild`, `outpost_abandoned=land given back to the wild`, `cult_adopted=a faith taken up`, `cult_suppressed=its faithful put down` (written below as `{@causes:WORDS|…}`). A later stage that adds a deed adds its words here.

New role form **`{g:titled}`** for a god role: the name, a space and the title (`Saene the Maker of Elves`, `Ormala the Keeper of Oaths`). (Amends P3-04 8.4.)

| key | sal. | template | summary? |
| --- | --- | --- | --- |
| `divine_bless` | 55 | `{@causes:WORDS|Pleased by |, }{actor:titled}{telling? blessed| smiled upon| set its hand over}{effect?| the fields of| the arms of} {civilization}{years?| for a year| for # years}.` | yes |
| `divine_curse` | 60 | `{@causes:WORDS|Angered by |, }{actor:titled}{effect?| sent a pestilence upon| withered the fields of| turned the fortunes of war against} {civilization}{dead?|; one died|; # died}.` | yes |
| `divine_champion` | 55 | `{@causes:WORDS|Pleased by |, }{actor:titled}{telling? raised up| chose| set its mark upon} {figure} as the champion of {civilization}.` | yes |
| `divine_monster` | 60 | `{@causes:WORDS|Angered by |, }{actor:titled}{telling? sent| loosed| woke} {monster}, {monster:kind}, against {civilization}.` | yes |
| `divine_mark` | 45 | `{@causes:WORDS|After |, }{actor:titled}{profane? hallowed| cursed} the ground of {site}.` | yes |
| `cult_founded` | 50 | `{telling?In the name of |Calling upon |Sworn to }{god:titled}, {founder} founded {faction} at {site} in {@settlement}.` | yes |
| `cult_spread` | 25 | `{faction} spread from {from} into {civilization}{@causes:trade_opened=the roads of trade,alliance_formed=the bonds of alliance| along |}.` | no |
| `cult_adopted` | 45 | `{civilization}{civilization:under} took {faction} as the faith of the realm.` | yes |
| `cult_suppressed` | 50 | `{civilization}{civilization:under} put down {faction}{hidden?|, and its faithful fled into hiding}.` | yes |
| `tower_taken_up` | 20 | `{@causes:figure_died=the death of its last master|After |, }{figure} of {civilization} became master of {site}.` | no |
| `tower_abandoned` | 25 | `{@causes:figure_died=the death of its last master|After |, }{site} of {civilization} stood empty.` | no |

Samples (illustrative names): `Angered by the sack of a city, Saene the Maker of Elves sent a pestilence upon the Gub Horde; 1,206 died.` · `Pleased by an oath sworn and an ally's faith kept, Ormala the Keeper of Oaths blessed the arms of the Kingdom of Harl for 50 years.` · `Pleased by a war won, Thorvald the Shield of Battles raised up Edric as the champion of the Kingdom of Harl.` · `Angered by the felling of the wild, Hethra the Hunter in the Green sent Grumm, a troll of the hills, against the Kingdom of Harl.` · `After the sack of a city, Kruskux the Maker of Goblins cursed the ground of the Stinking Ground of Kruskux.` · `Sworn to Ormala the Keeper of Oaths, Edric founded the Order of Ormala at the Temple of Ormala in Dunmere.` · `The Order of Ormala spread from the Kingdom of Harl into the Hold of Durn along the roads of trade.` · `The Horde of Krog under Zig put down the Children of Saene, and its faithful fled into hiding.` · `After the death of its last master, the Star-Tower of Femaeth of the House of Glaefaenn stood empty.`

`{@causes:…}` renders the words of each listed cause kind once, joined as the renderer does today. **P5-06's deed list** gains `cult_adopted` and `cult_suppressed` (role `civilization`), so `{civilization:under}` names a ruler there.

### 8.3 Causes (W6) and saliences

An act: its pair's strongest contributions (6.3). A founding: its act. A spread: the cult's founding and the link's opening. A faith or a suppression: the cult's latest event in that realm. A tower taken up or abandoned: the last master's death event. Saliences: a curse or a sent monster reads like a waking of the deep (60); a blessing or a champion just below (55); a cult's founding and its suppression like a sealing (50); a faith like an aid at the gate (45); a spread is bookkeeping (25). Estimated summary candidates a default world: 10 to 25 acts, 2 to 6 foundings, 1 to 4 faiths and suppressions, inside P5-02 12.9's budget (the threshold rises to make room; T18, 15.1).

## 9. Schema 14 (hard rule 7)

**DDL** (statements replaced in place or appended; the order of `DDL_STATEMENTS` is otherwise unchanged):

```
CREATE TABLE god (id INTEGER PRIMARY KEY, name TEXT NOT NULL, camp INTEGER NOT NULL, domain INTEGER NOT NULL, title TEXT NOT NULL, race INTEGER NOT NULL) STRICT
CREATE TABLE faction (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, civ INTEGER NOT NULL, god INTEGER NOT NULL, founder INTEGER NOT NULL, seat INTEGER NOT NULL, founded_year INTEGER NOT NULL, founding_event INTEGER NOT NULL, ended_year INTEGER, ending_event INTEGER NOT NULL, ending INTEGER NOT NULL, status INTEGER NOT NULL) STRICT
```

- `god` (replaced in place, ninth to thirteenth as before): `domain` 1 creation, 2 war, 3 death, 4 harvest, 5 craft, 6 wild, 7 oaths, 8 lore, 9 ruin; `title` (6.1); `race` the race it made, 0 for the rest of the pantheon. Rows: n creators, then the pantheon in id order (6.1).
- `faction` (new, appended after `book`; 37 tables): written by the history stage in ascending id. `kind` 1 cult (the factions stage appends its codes); `civ` the founding realm (design 6's `civ_id`); `god`; `founder` a figure; `seat` the founding temple (a site of kind 6); `founded_year`, `founding_event` its `cult_founded`; `ended_year` NULL and `ending_event` 0 unless ended; `ending` 0 not ended, 1 suppressed, 2 its realms fell; `status` at E 1 present, 2 hidden, 3 ended.
- Code lists: `site.kind` gains **5 shrine** (`settlement` the marked settlement or 0, `founder_civ` the realm the mark concerns, `origin_event` its `divine_mark`; never ends), **6 temple** (`settlement` its settlement, `founder_civ` the realm that raised it, `origin_event` a `cult_founded` or `cult_adopted`; `ending` 0, 4, 7 or 8), **7 sanctum** (settlement 0, `founder_civ` the realm that suppressed the cult, `origin_event` a `cult_suppressed`; never ends). `site.ending` gains **7 abandoned** (its keepers are gone: a cult ended, a tower's master long dead, 12.6) and **8 suppressed** (closed by a ruler). Kind 4 `tower` may end with 7. `figure.role` gains **6 champion** and **7 prophet**. Entity type **7 `faction`** comes into use.
- Relation kinds (text keys): `cult_in` and `faith_of`, both asymmetric, a = faction (type 7), b = civilization (type 3); `cult_in` opened by `cult_founded` or `cult_spread`, closed by `cult_suppressed` or the realm's ending event; `faith_of` opened by `cult_adopted`, closed by the realm's ending event or the cult's end (its ending event). At most one `faith_of` in force per civilization.
- `SCHEMA_VERSION`, `PRAGMA user_version`, `meta.schema_version` **14**; readers refuse 13 (`world schema version 13, this build reads 14`). The empty world gains one root page (`faction`): 48 pages, 196608 bytes (to be measured by the engineer with the vendored SQLite).
- `event_kind` gains 100 to 110; kinds 93 and 98's `telling` range widens to 0–8 (12.3; a reader range, not DDL).

**Reader checks** (both readers, one function as P5-02 11.4; messages `corrupt table '<t>': row <k>: <reason>`):

- `god`: ids 1 … m dense; rows 1 … n (n = rows of `race`) have `race` = id, `domain` 1, `camp` = the race's camp; rows above n have `race` 0 and `domain` 2 to 9 (`domain <d> not 2 to 9 for a pantheon god`); `camp` 0 to 2.
- `faction`: ids dense; `kind` 1; `god` a row; `civ` a civilization; `founder` a figure; `seat` a site of kind 6 whose `origin_event` is `founding_event`; `founding_event` a `cult_founded` naming the faction; `status` 1 to 3, with `ending` 0 and `ended_year` NULL exactly when `status` < 3; `ending_event` of year `ended_year`; status 2 exactly when a `cult_suppressed` naming it has `hidden` 1 (`faction <k> hidden without a sanctum`).
- `site`: kinds 1 to 7 (`kind <k> not 1 to 7`); a kind 5's `origin_event` is a `divine_mark` naming it; a kind 6's a `cult_founded` or `cult_adopted` naming it; a kind 7's a `cult_suppressed` naming it as `sanctum`; kinds 5 and 7 have `ending` 0; kind 6 `ending` 0, 4, 7 or 8, kind 4 0, 4, 5 or 7 (`ending <k> is not … for a <kind>`).
- `monster`: `born_event` may be a `divine_monster` naming it, for a kind with `wake` 1 or 2.
- `figure`: `role` 1 to 7; a role 6's `start_event` is a `divine_champion` naming it; a role 7's a `cult_founded` naming it as `founder`.
- Events: the values of 100 to 110 in 8.1's ranges; `divine_script` 0 on every event of a generated world with an empty `param.divine_script`; every `cult_spread`, `cult_adopted`, `cult_suppressed` names a faction founded before it; a `tower_abandoned`'s site is of kind 4 and ends with code 7 and that event.

## 10. History report v11 (amends P3-07; new section 34)

1. Header **`# worldgen history v11`** at both levels; `report --help` prints it. The growth rule holds.
2. **`[pantheon]`** (new block after `[figures]`): `gods: <n>; acts: <a> (bless <b>, curse <c>, champion <h>, monster <m>, mark <k>); flares: <f>` (f = the flare-up phases (kind 3) whose `trigger_event` is a cause of an act of that phase's first year), then per god `  g<id> <name> <title> (<camp>, <domain>): acts <n>[ (<kind> <k>, …)][; cult fa<id> <name>]` (kind counts in the order of 8.1, zeros left out). Full: every god in id order. Summary: gods with an act or a cult, by (acts descending, id ascending), at most `SUMMARY_GODS_MAX` (8), then `  and <n> more`.
3. **`[cults]`** (new block after `[pantheon]`): `cults: <n> (present <p>, hidden <h>, ended <e>)`, then per cult `  fa<id> <name>, of g<id> <name>: founded <year> e<id> by f<id> <name> at si<id> <temple name> in s<id> <name>; in c<id> <name>[, …]; faith of c<id> <name> since <year>[, …]; <end part>` with end part `present`, `hidden at si<id> <name> since <year> e<id>` or `ended <year> e<id> suppressed|its realms fell`; "in" lists the realms with an open `cult_in` at E (or at its end). Full: every cult in id order. Summary: present first, then hidden, then ended, then id; at most `SUMMARY_CULTS_MAX` (6), then `  and <n> more`.
4. **`[civilizations]`** (both levels; not at summary when the world has more than `SUMMARY_LINES_CIVS` civilizations): `  gods: blessed by g<id> <name> e<id>[, …]; cursed by …; champion f<id> <name> e<id>; sent m<id> <name> e<id>; marked si<id> <name> e<id>` (acts naming the realm, parts only when non-empty, at most 4 entries a part then ` and <n> more`); `  faith: fa<id> <name> since <year> e<id>` when it had one. Both are block lines with entity segments.
5. **The deep in summaries** (12.1).
6. `[sites]`: shrines, temples and sanctums print as lairs and towers do, with `sacred`/`profane` after a shrine's name, the cult after a temple's or sanctum's, and the ending words `abandoned` (7) and `suppressed` (8).
7. `[figures]`: roles `champion` and `prophet` in the role counts and lists.
8. `[stats]` (full; appended in this order): `divine_acts`, `divine_bless`, `divine_curse`, `divine_champion`, `divine_monster`, `divine_mark`, `divine_good`, `divine_neutral`, `divine_evil` (acts by the actor's camp), `divine_creators` (acts by creators), `divine_flares`, `divine_dead` (Σ `dead` of kind 101), `cults_founded`, `cults_spread`, `cults_adopted`, `cults_suppressed`, `cults_hidden`, `cults_present`, `cult_founders_prophets`, `shrines_sacred`, `shrines_profane`, `temples`, `towers_taken_up`, `towers_abandoned`, `renewals_capped` (12.5), `deep_stirred` (12.2).
9. **Length**: about +16 block lines at summary (`[pantheon]` ≤ 10, `[cults]` ≤ 8), +1 to 2 lines per realm touched by a god, and the pinned deep events (12.1). Bound: T18 (15.1).

## 11. Timeline, browser, markers (library) and the viewer

### 11.1 Library

- **Timeline** (amends P4-01; r9): `std::int64_t faith_at(std::int64_t civ, std::int64_t year) const` (the faction of the `faith_of` row in force, 0 none); `std::vector<std::int64_t> cults_in_at(std::int64_t civ, std::int64_t year) const` (ascending); `std::int64_t cult_status_at(std::int64_t faction, std::int64_t year) const` (0 not founded, 1 present, 2 hidden, 3 ended); `std::int64_t divine_effect_at(std::int64_t civ, std::int64_t year) const` (bit 1 plenty, 2 withering, 4 valour, 8 defeat; read from kinds 100 and 101 with their `years`). `sites_existing` includes kinds 5 to 7.
- **Browser** (amends P4-05 4.4): tenth kind group **`gods` = 9** for kinds 100 to 108 (`ALL_GROUPS` 0x3FF); 109 and 110 in `figures`. Entities of type 2 (god) and 7 (faction) are selectable; `block` returns the `[pantheon]` or `[cults]` line and the realm `gods:`/`faith:` lines link them.
- **Markers** (amends P4-04 3.6): glyph **21 `shrine`** (sacred and profane told apart by the hover text), **22 `temple`**, **23 `sanctum`**; `MarkerOptions::shrines` and `temples` (sanctums with temples); `event_symbol` for 100 to 110 plain. `std::string_view shrine_word(bool profane)`: `holy to`, `cursed by`.

### 11.2 Viewer work (viewer engineer)

1. Markers group boxes `Shrines` and `Temples`; glyphs 21 to 23.
2. Hover: shrine `<name> (<shrine_word> <god name>), since <year>`; temple `<name> (temple of <cult name>), since <year>`; sanctum `<name> (hidden sanctum of <cult name>), since <year>`; a realm's territory hover gains `, faith: <cult name>` when `faith_at` ≠ 0 at the shown year.
3. Browser group `Gods`; god and cult blocks.
4. Headless checks **M13** (on a `pantheon-small` world at 512: a shrine and a temple hover at E and absent before their origin years) and **B15** (the `gods` group lists the world's kinds 100 to 108; selecting a god shows its block with links).

## 12. Follow-ups carried (this stage is the next change to world bytes)

### 12.1 P5-07 critic F1: the deep in summaries (report only)

1. **Pinned in the summary set** (amends P3-07 24.3 item 2): F gains every event of kinds `deep_woken`, `deep_sealed`, `warren_sealed` and `hold_lost`, whatever its salience, with its unit; the `ally_aided_hold` events citing such an outcome (12.4) are companions in its unit. F counts toward its age's floor as quiet stretches do.
2. **Realm block line** (both levels; at summary under the rule of 10 item 4): `  deep: woke m<id> <name> e<id>[, …]; sealed m<id> <name> in si<id> <name> e<id>[, …]; lost s<id> <name> e<id>[, …]; aided c<id> <name> e<id>[, …]; aided by c<id> <name> e<id>[, …]` (each part only when non-empty, at most 4 entries then ` and <n> more`): woke = kind 94 with this realm as `civilization`; sealed = kinds 95 and 99 (the lair, so the sealed place can be found); lost = kind 96; aided = kind 97 with the realm as `ally` (its `partner` listed); aided by = kind 97 with the realm as `partner` (its `ally` listed).
3. `[monsters]` at summary: sealed monsters come straight after the awake ones (then the other asleep, then the dead).

### 12.2 P5-07 critic F2: the deep stirs in quiet ages (L-5)

*Measured* (critic): about 25 of 31 wakings fall at year 5500 or later. Wakings are tick-only, and the last active phase holds most ticks of the deep's millennia. **Rule** (amends P5-07 6.1, 6.4, 6.10): in monsters' Q4, after the deepening and warren delving, at most one **stir** per step: the mine candidates of P5-07 6.4 (open, not woken, not sealed, depth ≥ 1, founder alive, town standing), then the warren candidates of 6.10, ascending id, while none has stirred and `pending_flare` = 0:

```
p = min(10^6, deep_wake_ppm[d − 1] × M div 1000 × L × deep_stir_permille div 1000)       // warrens: × deep_warren_wake_permille div 1000
if chance_ppm(D(hist.deep.stir; s, 0, 0), p)  (warrens: hist.deep.warren_stir; t):  stirring(s) = true; pending_flare = s's last delving_deepened (t's last warren_delved)
```

In the first tick year of a flare-up whose trigger is that deepening, the stirring candidate wakes in T4 with no `hist.deep.wake` (`warren_wake`) draw, if still a candidate; the kind, gate and take are as P5-07. `stirring` clears at the next Q4. New field `history.monsters.deep_stir_permille` int(0, 1000), start **1000**, code default 0. The flare forces no war (P5-02). **Target FU1**: wakings in years < E − 500 at least 35% of all wakings pooled per sample, with DP2 (25 to 55 worlds) kept; calibration lowers `deep_wake_ppm[2]` if DP2 rises above its bound.

### 12.3 P5-07 critic F4: deepening and lost-hold lines (data, and two value ranges)

- `delving_deepened` (93): `telling` = (s + 4 × d) mod 9 (was (s + d + 1) mod 3); `warren_delved` (98): (t + 4 × d) mod 9. Templates:
  - 93: `{telling?Following a rich seam, |Year upon year, |By hammer and lamp, |Chasing the silver down, |Shaft by shaft, |Heedless of the old warnings, |With new picks from the forge, |Through a hard generation, |Singing the old delving songs, }{civilization} {depth?worked|sank the shafts of|drove the halls of|broke the floor of} {site} {depth?deeper|below the reach of daylight|down to the mountain's roots|into the oldest dark beneath the world}{telling?, and struck a vein of silver|, and broke into a cavern of crystal|, and felt a cold breath rise from below|, and found the bones of an older people|, and heard hammering where none worked|, and struck black iron|, and found a lake no light had touched|, and lost a crew to a sudden fall|, and found gold in the dark}.` (no verb `delved`, so no `delved the Delvings of`)
  - 98: `{telling?Ever greedy for the dark, |Squabbling and scraping, |Grub by grub and stone by stone, |Whipped on by their chiefs, |Stealing each other's picks, |Gnawing like rats, |Chasing a rumour of gold, |Fleeing the daylight, |In fits of greed and spite, }{civilization} dug the warrens of {@settlement} {depth?deeper|below the reach of daylight|down among the mountain's roots|into the oldest dark beneath the world}{telling?, and ate what they found there|, and found a nest of blind worms|, and lost a dozen in a fall|, and fought over a lump of silver|, and found old bones to gnaw|, and heard something breathing|, and struck a cold black water|, and found a passage no goblin had cut|, and came up shrieking}.`
- `hold_lost` (96): `{@causes:monster_raid=the raids|After |, }{monster}{telling? came up out of the deep into| rose from below into| broke out of the deeps into| boiled up out of the burrows into| came up the goblins' own tunnels into| rose under the warrens of} {@settlement}{telling?, and tore it from|, and wrested it from|, and took it from|, and took it from|, and took it from|, and tore it from} {civilization}; {dead?no one died|one died|# died}{survivors?, and none escaped|, and one escaped|, and # fled}.` (no `<settlement> of <realm>`, so no `Thark Hold of the Hold of Khairthain`).
- Reader checks: kinds 93 and 98 `telling` 0 to 8.

### 12.4 P5-07 critic F3: aid after the outcome (amends P5-07 6.4 item 6, 6.5, 6.6, 6.11, 8.3, invariant 3)

At a waking the order of events becomes: `deep_woken`; the outcome (`deep_sealed`, `warren_sealed`, or `hold_lost` with its flight's or ruin's events); then one `ally_aided_hold` per aid, ascending ally id, each followed by its `credit`. The outcome cites the `deep_woken` only; each aid cites the `deep_woken` and the outcome. The aid is still decided before the gate and still adds to S (P5-07 6.5): only the record's order and causes change. Template of 97: `{telling?Answering its ally's call, |Bound by old oaths, |Unbidden, }{ally}{ally:under}{telling? sent warriors from | marched out of | hurried its people from }{origin} to stand with {partner} at the gate of {@settlement} against {monster}{@causes:deep_sealed=and the gate held,warren_sealed=and the tunnels were closed|, |}{@causes:hold_lost=but the hold fell all the same|, |}.` Reader check of 97 widens: it cites a `deep_woken` and an outcome of that waking, both of its year. P5-07 invariant 3 reads "… followed in the same step by exactly one outcome of its monster, then its aid events (at most `deep_aid_max`), with no event of another monster before the outcome".

### 12.5 P5-04a critic F1: a cap on written renewals (amends P5-04a 6.2)

A notable renewal is written only while its alliance row has fewer than `history.alliances.renew_written_max` written renewals (int(0, 100), start **3**, 0 no cap, code default 0); otherwise it is made quietly (P5-07 7.5's `credit_units`, no event, R unchanged). The `[alliances]` line's `renewed <k>` keeps counting every renewal; `[stats]` `renewals_capped` counts the quiet ones made by the cap. **Target FU2**: `alliance_renewed` events a default world median ≤ 8.

### 12.6 P5-04a critic F5 and the tower lapse (amends P5-06 6.8, P5-04a 21.4; lead ruling L-1 of P5-04a)

1. **A new master** (the rise of 6.8): when the new caster's seat t (c's capital) holds an open kind 4 site of c (a library at t), or t's region holds one (a tower), the caster takes up the one with the smallest id instead of founding: append **`tower_taken_up`** (region of the site; settlement t for a library, else 0; participants f figure, c civilization, s site; cause the death event of the site's last master); the site's master = f. No new site, no name draw (the `hist.figures.tower_name` draw is not made). So a realm has at most one open tower per region.
2. **A fresh tower's cause**: P5-06 6.8's cause plus the death event of c's last caster when there is one, ascending. `tower_founded`'s template gains `{@causes:figure_died=the death of the last master|After |, }` at its front.
3. **Lapse**: in figures' tower endings (P5-04a 21.4: T3, T6, Q4), after the tests for 5 and 4: an open kind 4 site whose master died in year d ends when Y ≥ d + `history.figures.tower_lapse_years` (int(0, 10000), start **200**, 0 never, code default 0): append **`tower_abandoned`** (region of the site; settlement for a library; participants s site, c civilization, the master figure; cause the master's death event); `end_site_seam(s, Y, that event, 7)`. Q4: needs `quiet_events_left(c)`, else it waits. A caster whose site has ended writes no book and causes no blight (unchanged).
4. Books and blights (6.8) use the caster's mastered site.
5. **Target FU3**: towers and libraries ended at E ≥ 25% of kind 4 sites pooled per sample; FG7 (P5-06 15.1) kept.

### 12.7 P5-07 audit A1 and A2 (no world change expected)

- **A1**: `deep_take(ctx, m, t, cause, std::optional<DeepFlightPlan> plan)`: at a waking it uses the plan computed at the gate (P5-07 6.12 item 1) and does not recompute it; a later take computes its own. The FatalError "found no flight" remains only for a later take whose plan is empty after the take was decided (unreachable by construction; kept as a guard).
- **A2**: a later take (P5-07 6.7) also requires `ctx.visible(waking realm)`.
- **A3** (coverage): `world-hashes.sh` gains the case **`p5-divine-meddling-1024`** (seed 9, `--divine-activity meddling`; seventeen cases), and validation reports which cases hold a warren waking or a deep flight.

### 12.8 Left

| Item | Why |
| --- | --- |
| P5-04a critic F7 (thaw and hate flip-flop) | the critic asks for no change |
| A second deep line form for warrens in `hold_lost` (critic answer 4: "a warren hold is also a hold_lost") | wording only; 12.3's goblin tellings carry it |
| Cults that re-emerge from hiding | Q-P5-08-3 |

## 13. Invariants (every seed; property tests and `check-world`)

1. `god`: rows 1 … n creators as P3-03 C5 with domain 1; rows n + 1 … the pantheon in canonical id order with their data's camp, domain and title; names distinct from every region and age name and from each other.
2. Every act (100 to 104) is in a tick year; `divine_script` 0; one to three causes, each an event of a deed of its god (4.3) whose receiver was its civilization, all of one sign (positive for bless, champion and a sacred mark, negative for curse, monster and a profane mark), within `regard_memory_years`; at most one act per year; a god's acts at least `god_rest_years` apart, a pair's at least `pair_rest_years`.
3. No act favours a foe of its god; no wrathful act targets a realm whose faith is its god's cult at the act.
4. A pestilence's `dead` equals the population lost by c's settlements of the capital region in that event; a `divine_monster`'s monster is born of it, awake, of a kind in its god's `monsters`, its lair within `send_hops` of c's capital region; a `divine_champion`'s figure has role 6, civilization c and `start_event` the act, and in the act's year c was at war (an open war as a side or open joined ally) or an awake monster c could hunt under P5-02 6.4.2 existed (6.3); a `divine_mark`'s site is of kind 5 with `origin_event` the act, and no settlement has two shrines.
5. Every cult: founded by a `cult_founded` citing an act of its god on its founding realm, one year or more after the act (a later step); its founder a figure alive at the founding (a role 7 founder created by it); its seat a temple at a settlement of the founding realm; at most one live cult per god at every year's end.
6. `cult_in`(k, a) opens only by `cult_founded` or `cult_spread`; a `cult_spread` cites k's founding and its `from` realm had `cult_in` and an open alliance or trade with a at the event; at most one spread per cult per step.
7. At most one `faith_of` per realm in force; `faith_of`(k, c) only while `cult_in`(k, c) is open; no `cult_suppressed` of a realm's own faith; a suppression's realm satisfies 6.9's hostility rule at the event.
8. A cult with status 2 has exactly one sanctum (kind 7) and no open `cult_in` at E; status 3 has `ending` 1 or 2 and every temple of it ended by its ending year.
8a. At E no `cult_in` or `faith_of` is open on a realm that has ended (P3-06 invariant 4), including a realm ended in the last year (6.10's finish); no `cult_spread`, `cult_adopted` or `cult_suppressed` of a cult falls in the step of its `cult_founded` (6.12 item 4).
9. Every open temple at E stands on a standing settlement whose owner has `cult_in` of its cult or raised it.
10. The force of a realm in every battle equals 0.29.0's formula times (1000 + `force_permille`) div 1000 (replayed from the acts' effects); growth likewise (replayed by the reference on fixtures).
11. 12.2: every waking in a tick year that follows a stir has the stirring candidate's last deepening as the phase trigger; no waking happens in a quiet step.
12. 12.4: every `ally_aided_hold` follows its outcome and cites it; every outcome at a waking cites only the waking.
13. 12.6: no realm has two open kind 4 sites in one region at any year's end opened after 0.30.0's rule (a library at its city excepted); every `tower_abandoned` is at least `tower_lapse_years` after its master's death and ends its site with 7.
14. Every earlier invariant holds (P5-02 to P5-07), with sent monsters counted as monsters, champions and prophets as figures, and kinds 5 to 7 as sites.

## 14. Determinism notes (for the auditor)

- Integer arithmetic, int64, checked multiplication. Largest products: `act_permille` × A ≤ 1000 × 10^4; P_u × 1000; the force factor (1000 + 1000 + 3000) × f × (1000 + 500) as P5-07's with one more factor; `deep_wake_ppm` × M × L × `deep_stir_permille` × `deep_warren_wake_permille` ≤ 10^6 × 10^4 × 25 × 10^3 × 10^3 < 2^63 (checked; the engineer may reorder the divisions to keep it below 2^53, but the order of operations is the spec's).
- Thirteen keyed streams (3), keys of ids, never positions. Every draw sits behind a fixed order of tests: the candidate first (pure), then the act draw, then the kind; a founding's draw before its seat and founder; a spread's candidates ascending with one draw each until one succeeds.
- Ordered containers only: the pair map keyed (g, c); vectors by id. Deeds by kind compiled once in `prepare` in (god, deed index) order.
- The fade is cumulative (F(Y + L) − F(Y)), so it does not depend on how steps cut a span; resume (P3-04 16.3) rebuilds every member by the replay.
- `telling` values are functions of ids and counts the reference can replay.
- No floating point, no unordered container.
- **I0** (proof obligation): with `act_permille` 0 and `flare_permille` 0, no act and no cult happen and every `DivineView` read is neutral, so the history tables are byte-identical to those of the same build with the pantheon subsystem not installed, apart from the catalogue rows of kinds 100 to 110. (The `god` table and the 12.x follow-ups are the same in both builds.)

## 15. Targets and calibration

### 15.1 Targets (1024 defaults; A = seeds 0–99, B = 100–199; settings samples on seeds 0–49)

| Target | Definition | Bound |
| --- | --- | --- |
| **PN1** | acts a world | median 8 to 25 on each sample; max ≤ 60 |
| **PN2** | by activity (seeds 0–49) | `meddling` median ≥ 1.8 × normal's; `distant` median ≤ 0.6 × normal's, with at least one act in ≥ 30 of the 50 worlds |
| **PN3** | every camp acts (design's exit check, strengthened) | a god of each camp acts in ≥ 80 worlds of each sample |
| **PN4** | kinds | each of the five ≥ 5% of acts pooled; none > 45% |
| **PN5** | champions | worlds with a champion ≥ 30; ≥ 50% of champions fight (a `battle_led` naming them, a hunt, a slaying) |
| **PN6** | sent monsters | ≤ 20% of the monsters alive at E; M4a holds |
| **PN7** | pestilence | `divine_dead` median ≤ 3% of the world's population at E |
| **PN8** | gods' flare-ups | reported; median ≤ 3 |
| **CU1** | cults founded | median 2 to 6; ≥ 1 cult in ≥ 85 worlds |
| **CU2** | spread | ≥ 40% of cults are in two realms or more at some year |
| **CU3** | faith | worlds with a faith ≥ 50 |
| **CU4** | suppression | worlds with a suppression 20 to 70 |
| **CU5** | hidden at E | 10 to 50 worlds |
| **CU6** | founders | prophets ≤ 60% of founders pooled |
| **FU1** | the deep spread (12.2) | wakings before E − 500 ≥ 35% pooled per sample; DP2 kept |
| **FU2** | renewals (12.5) | `alliance_renewed` median ≤ 8 |
| **FU3** | towers end (12.6) | ≥ 25% of kind 4 sites ended at E; FG7 kept |
| **C1** (guard) | wars median | 22 to 28 |
| **G2** (guard) | standing settlements; population at E (medians) | ≥ 81; ≥ 205,000 |
| **M4a** (guard, Paul) | a living monster at E | ≥ 85 worlds on each sample |
| **M4b** (guard, Paul) | a lair on a former settlement open at E | average of A and B ≥ 65, neither below 60 |
| **T16** (guard) | repeated battles | ≤ 2 per thousand |
| **T18** (guard) | summary lines | default median ≤ **700**; every world ≤ L(C); `civs4` median ≤ 1,050 |
| **DP, GW, AD** (guards) | P5-07 15.1 | as there (DP2 read with FU1) |
| **GW4** (guard) | goblin regions at E | median ≤ 12 on each sample; mean ≤ 0.29.0's + 0.3 (evil blessings must not feed the strongest race) |
| **AL1, AL5, FG5** (guards) | as P5-04a 15.1 and P5-06 15.1 | as there |
| **timing** | `generate` seed 0 1024 release | ≤ +1.5 s over 0.29.0 |

### 15.2 Calibration (data only, the engineer, in this order; history replayed on cached geography, checked equal to `generate`)

1. PN1 by `act_threshold`, then `act_permille`, then `regard_fade_per_century`.
2. PN3, PN4 by the god definitions' weights and deed points (creators' points first: they act most).
3. PN7, G2 by `pestilence_toll_permille`; C1, T16, GW4 by `valour_permille` and `defeat_permille` (lower valour first if GW4 moves).
4. PN6, M4a by the `monster` weights.
5. CU1 by `cult_found_permille`, `cult_fear_permille`; CU2 by `cult_spread_permille`; CU3 by `adopt_permille`, `adopt_regard`; CU4, CU5 by `suppress_permille`, `hide_permille`.
6. PN2 checked on the settings samples; PN8 by `flare_regard`, `flare_permille`.
7. FU1 by `deep_stir_permille`, then `deep_wake_ppm[2]` for DP2; FU2 by `renew_written_max`; FU3 by `tower_lapse_years`.
8. After every step: C1, G2, M4a, M4b, T16, T18, GW4; a step that breaks one is undone and reported to the lead. At most 8 sweeps; a miss is reported, not looped on.

## 16. Tests required

C++ (new shards `worldgen-tests-pantheon` for `*pantheon_*` and `-slow-pantheon`; the chronology, figures, monsters, depths, alliances and history-report shards for 6.1, 7.5, 7.3, 12.2 to 12.6 and 10):

1. Data: the `god` kind and its checks (each `god_*`, grammar `god_title`, `god_form`); `history.pantheon` bounds, absence and its two checks; the culture fields; `creator_title`; deeds' checks with unknown kinds and roles.
2. Chronology: ids, camps, domains, titles and names of the fifteen gods on seeds 0, 1, 42 and `--races elf,orc` (absent races keep their creator; the pantheon is the same); creator and age names unchanged from 0.29.0 (`p3-03-chronology.txt`'s names); a world with no `god` definitions.
3. Regard: each `who` and `against`; a positive deed to a foe skipped; several participants in one role (`truce.parties`); clamp; memory and `contributions_max`; the fade over steps of different lengths gives the same R.
4. The act: candidate order and ties; each rest; eligibility of each kind (curse without a capital, champion with a living one, monster without room or region, mark without a place); the causes (three, one sign, ascending); the spend; at most one a year; `certain` after a gods' flare-up and not after another source's.
5. Effects: plenty, withering, valour, defeat in force for exactly `years`; sums and clamps; pestilence's deaths and `note_reduction`; sent monster outside L_world, inside `max_alive`; champion as hero and as general (no appointment draw); sacred growth; profane shunning and its end.
6. Cults: founding (each founder fallback, the seat rules, no seat → no founding, one live cult per god, Q4 wait); spread (link order, one per step, affinity, Q4 budget); faith (regard or founding realm; temple at the capital; rival gods' and creator's regard); suppression (hostility rules, temples ending 8, hiding with and without a place, ending 1); a realm's end (rows closed, ending 2, temples 7); temples ending 4 by ruin and by owner.
7. Engine: the applier is called once per valid intervention in order; an invalid one (wrong year, unknown kind, ended target) is dropped with no event; a null applier with a non-empty source is fatal.
8. Seams: `DivineView` unset is neutral; `partners`; `prepare_sent` is pure; `raise_divine` refuses a wrong id.
9. Rendering: each new template at each `telling`, `effect`, `profane`, `hidden`, `years` and `dead` form; `{g:titled}` for a creator and a pantheon god; the cause words; the changed templates of 12.3 to 12.6.
10. Reader: every check and message of 9 on corrupt fixtures; a schema 13 world refused with the version message.
11. Report v11 on fixture **H20** (`h20-v11.toml`, from H19): every act kind, a flare by a god, a champion who leads a battle, a sent monster, both marks, a cult founded by a prophet and one by a ruler, a spread, a faith with rival wrath, a suppression into hiding and one that ends a cult, a realm's end that ends a cult, a tower taken up and one abandoned, a capped renewal, a sealing and an aid in a realm block (12.1), both levels.
12. Follow-ups: 12.2's stir (Q4 draw, the flare trigger, the waking with no wake draw, the stir cleared); 12.3's `telling` mod 9; 12.4's order and causes; 12.5's cap at 3 and 0; 12.6's take-up, lapse and Q4 wait; 12.7 A1 (the take uses the gate's plan; a test seam that makes the second plan differ no longer fails).
13. Timeline, browser, markers: `faith_at`, `cults_in_at`, `cult_status_at`, `divine_effect_at`; group 9; glyphs 21 to 23; `shrine_word`.
14. Invariants of 13: default seeds 0–9 at 512 with `pantheon-small` and shipped; slow: 0–19 at 1024 shipped, `--divine-activity meddling` 0–4 and `distant` 0–4.
15. I0 (14) and resume (P3-04 16.3) with an act, a cult and a flare before the resume point.
16. CLI: `--divine-script` refused with the new message.

Reference: **`p5_08_pantheon_reference.py`** (test writer): replays 6.2 to 6.10 on the fixtures and `check-world` for 13; golden **`worldgen/tests/golden/p5-08-pantheon.txt`** (`pantheon-small` worlds at 512 seeds 0–9 and shipped seed 42 at 1024: hashes of `god`, `faction`, `site`, `figure`, `event`, `event_value`, and the stat lines of 10 item 8). Carried test-side items: `p3_chronology_reference.py` writes the pantheon rows; `p5_07_depths_reference.py` knows 12.2 and 12.4; `p5_04_alliances_reference.py` the renewal cap; `p5_06_figures_reference.py` the take-up, lapse and champions; `p4_browser_reference.py` group 9.

## 17. Golden values and CI

Every world file and every history golden is re-recorded once, after the cross-preset comparison (generator 0.30.0, schema 14). `p3-03-chronology.txt` moves (the `god` table), but its creator and age names do not. Must not move: `core_vectors.txt`, `p1-0*`, `p3-02-names.txt`. `world-hashes.sh` gains `p5-divine-meddling-1024` (seventeen cases). New CTest entries `worldgen-tests-pantheon` and `-slow-pantheon`.

## 18. Work split (for cost)

| Part | Kind | Agent | Sections |
| --- | --- | --- | --- |
| A | data kind `god`, culture fields, `history.pantheon`, chronology's C5a and the `god` DDL | generator engineer | 4, 6.1, 9 (god) |
| B | `PantheonSubsystem`: regard, source and applier, effects, flare, cults | the same engineer | 5, 6.2 to 6.10, 6.12 |
| C | seams and the engine: `DivineView` in peoples and conflict, monsters' sent monsters, figures' champions, prophets, appointment and `hero_of`, `AllianceView::partners`, `InterventionApplier`, the CLI message | the same, or a second engineer on disjoint files | 6.11, 7 |
| D | follow-ups in monsters, alliances and figures | a second engineer | 12.2 to 12.7 |
| E | catalogue, readers, report v11 and 12.1, digest, timeline, browser, markers library | one engineer for `history_report.cpp`, `digest*`, `timeline.cpp` | 8, 9, 10, 11.1, 12.1 |
| F | calibration sweeps, data only | a separate agent after A to E | 15.2 |
| G | tests and references | test writer | 16 |
| H | goldens | validation, once | 17 |
| I | viewer | viewer engineer | 11.2 |

## 19. Amendments

| Spec | Section | What |
| --- | --- | --- |
| P3-01 | the `--divine-script` refusal text | 6.11 |
| P3-02 | 3 (culture fields), 4.5 (god names of the pantheon) | 4.4, 6.1 |
| P3-03 | C5, C5a, 5.2, 8.1 | 6.1, 9 |
| P3-04 | 6.4 (`EngineConfig::applier`), 6.8 (the source/applier split, supported kinds), 8.4 (`{g:titled}`) | 6.3, 7.6, 8.2 |
| P3-05 | growth, site and colony searches | 7.1 |
| P3-06 | 7.3 force | 7.1 |
| P3-07 | 24.3 item 2 (F), new 34 (v11) | 10, 12.1 |
| P5-02 | 6.3 (caps for sent monsters), 6.5 (third tension source), 11.4 (`born_event`), 13 | 6.4, 6.6, 7.3, 9 |
| P5-04 | 7.1 (`partners`) | 7.4 |
| P5-04a | 6.2 (renewal cap), 21.4 (lapse, take-up) | 12.5, 12.6 |
| P5-06 | 6.4 rule 3 (deeds), 6.6, 6.7, 6.8, 6.10, 7.1, 7.7, 9.1 (roles 6, 7) | 6.5, 7.5, 12.6 |
| P5-07 | 6.1, 6.4, 6.5, 6.6, 6.7, 6.10, 6.11, 6.12 item 1, 8.1 (`telling` 93, 98), 8.2, invariant 3 | 12.2 to 12.4, 12.7 |
| P4-01, P4-04, P4-05 | timeline reads, glyphs, group 9 | 11.1 |
| `schema.md` | the DDL of 9, code lists, relation kinds, changelog row 14, catalogue 100 to 110 | 9, 22 |

## 20. For P6 (P5-00 10.2)

P6 reads, per shrine: its cell, its god, sacred or profane, the deed it marks (the act's causes); per temple: its settlement, cult and god, its end; per sanctum: its cell, the cult hidden there and its god (a dungeon of origin "cult sanctum", design 9; a place for spellbooks); per champion: its god and deeds. Quest hooks: a hidden cult at E, a profane ground, a champion's tomb (its death place). Whether gods act during play stays design 15's open question (history only, as assumed).

## 21. Open questions

**For Paul** (each also needs a row in `docs/questions-for-paul.md`, the lead's):

| Id | Question | Recommendation | What the build does meanwhile |
| --- | --- | --- | --- |
| Q-P5-08-1 | The pantheon's shape: besides the six creators, nine gods, three per camp (good: harvest, oaths, lore; neutral: craft, wild, war; evil: war, death, ruin), each with a title such as "the Keeper of Oaths" and a name drawn per world. Is this the pantheon you want, or more or fewer gods, or other domains? | Take it: fifteen gods is enough for each realm to meet three or four of them in a history, and every domain answers deeds the history already records. More gods would mostly sit idle | ships the nine |
| Q-P5-08-2 | How often should gods act? The start is about 10 to 25 acts in a default world (one every few centuries of simulated time), 2.5 times as many with "meddling" and under a third with "distant". | Take it: often enough that each age has a few acts, rare enough that each one is read as an event | PN1 and PN2 |
| Q-P5-08-3 | A cult put down in its last realm may go into hiding at a sanctum in the wild and last to the present day, never returning in history. Should hidden cults stay hidden (a place for the player to find), or may they re-emerge in history later? | Stay hidden in this stage: the sanctum is a strong dungeon and quest place, and re-emergence is a later rule if the critic finds hidden cults too static | stay hidden |
| Q-P5-08-4 | May a people take up the cult of a god of the opposed camp (an evil god's cult among elves or men, a good god's among goblins)? | Yes, rarely: spread across the camps runs at a tenth of the rate within one, and such cults are the usual ones rulers put down, which angers their god. It suits the dark tone | as recommended |
| Q-P5-08-5 | The divine script: keep it refused (as the design says for P5), or build a minimal form now? | Keep it refused; this stage fixes the intervention kinds, the validation and the seam a script source will plug into, so the experiment of design 14 can come after the prototype | refused, with the message "divine scripts are not supported yet" |

**For the lead:**

| Id | Question | Recommendation (interim) |
| --- | --- | --- |
| L-1 | Schema 14 as in 9 (`god` gains `domain`, `title`, `race`; table `faction`; site kinds 5 to 7; endings 7, 8; roles 6, 7; entity type 7) | Yes, one bump for all of it. Proposals text in 22 |
| L-2 | Cults as `faction` rows of kind 1 now, rather than a `cult` table | Yes: design 6 names `faction`, the factions stage appends its kinds and membership, and a guild's chapters can use the same relation pattern as `cult_in` |
| L-3 | The engine's source/applier split (7.6) | Yes: it keeps `interventions()` pure and lets a script source replace the seeded one without touching the effects |
| L-4 | Sent monsters outside the world cap and the one-lair-per-region test of a wild appearance | Yes, as P5-07's deep kinds: an act is caused, not a random pressure; `max_alive` still binds |
| L-5 | 12.2's stir in quiet steps (critic F2) | Take: about 30 lines in `monsters_deep.cpp`, uses the existing flare mechanism, and spreads the deep over the ages; leave it if FU1 cannot be met without breaking DP2 |
| L-6 | One live cult per god | Yes: a cult reads as "the cult of Ormala"; a second following of one god would need a schism rule |
| L-7 | The tower lapse with a new kind `tower_abandoned` and ending 7, and a new master by `tower_taken_up` (12.6) | Take: the event keeps the ended year honest (no back-dating to the master's death), and the take-up removes the pile-up without cutting casters |
| L-8 | 12.4: aid events after the outcome | Yes: the record then reads in the order a reader understands, and the aid's sentence carries the outcome |
| L-9 | `renew_written_max` 3 | Yes; the block line keeps the full count |
| L-10 | Kind ids 100 to 110 | Final if no other stage merges first |
| L-11 | A prophet (figure role 7) when no recorded figure fits a founding | Yes: every cult then has a founder (design 9: "a founder who is a figure in the chronicle"), and CU6 keeps prophets from being the rule |
| L-12 | Pestilence on the capital's region and acts without disposition effects | Yes for this stage: it keeps the acts local and visible and leaves relations and alliances unmoved (their targets are fresh) |

## 22. Text for the lead (`schema.md` and `proposals.md`)

**`docs/specs/schema.md`**: section 1 at version 14 with the two statements of 9 (`god` replaced, `faction` appended after `book`); the `god` paragraph (version 6) gains "from version 14 (generator 0.30.0, P5-08): one row per race (its creator, `race` = id, `domain` 1) and then one per `god` definition (`race` 0, `domain` 2 to 9); `title`"; a paragraph for `faction` (version 14, 9); `site.kind` 5 to 7, `site.ending` 7 and 8, kind 4 ending 7; `figure.role` 6 and 7; entity type 7 in use; relation kinds `cult_in`, `faith_of`; the catalogue rows 100 to 110 and the `telling` ranges of 93 and 98; changelog row: `14 | 0.30.0 | P5-08 | god gains domain, title, race; table faction (cults, kind 1); site.kind 5 shrine, 6 temple, 7 sanctum; site.ending 7 abandoned, 8 suppressed; figure.role 6 champion, 7 prophet; entity type 7 faction in use; event kinds 100 to 110; readers refuse 13`.

**`docs/design/proposals.md`**: entries "Schema 14: the pantheon and cults (P5-08)" and "History: P5-08 pantheon, interventions and cults (readings)" (appended by the spec writer, r1).

## Revision notes

- r1 (2026-11-27): first draft, for approval.
- r1a (2026-11-27, integration E1 to E5, lead rulings): 6.8 compares camps for a creator (E2, the engine's reading); 6.10 gains the finish at the end of the history and 6.12 item 5 its T6 run (E3); 6.8, 6.9 and 6.12 item 4 state that a cult founded in a step waits for the next (E4); invariant 8a. E1 (the growth factor on the term, then the cap, 6.5 and 7.1) and E5 (one function for both readers, 9) were engine defects against the text as it stood.
