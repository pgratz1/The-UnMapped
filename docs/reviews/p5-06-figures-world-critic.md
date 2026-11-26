# World critic review: P5-06 figures (generator 0.27.0, history report v8)

Sample: default seeds 0-11 and high-monster-pressure seeds 0-3, map 1024. Counts below come from `full.txt` event lines and the summary blocks. Marker images read: default-s0 plain, high-s1 faded (the other pairs were not opened; see F6).

## Verdict

Acceptable to merge. Nothing is broken or absurd. The figures give the history people to follow, and the causes read. Several things repeat and should be tuned in a follow-up (F1 to F6).

## Findings

**F1. Every tower is open at the end, and towers pile up at one spot. Severity: should fix in follow-up.**
- Every one of the 16 worlds has all of its towers open: 6/6, 4/4, 9/9, and so on. No tower is ever lost, left to ruin or shown as old. Every other site kind ends.
- Towers are founded again on the same spot or one cell away while the earlier tower is still open. Examples:
  - default-s2: si4 at 853,533 (3216), si15 at 850,532 (4210), si33 at 855,534 (5284), all in The Holt of Adders (rg24) and all of c1.
  - default-s2: si24, si34 and si40 at 487,438 / 487,437 / 486,439 (rg13, c5, years 4865, 5520, 5726).
  - high-s1: si1, si15, si30 at 334-336,372 (rg15, c1, years 112, 3464, 4455).
- This is the pile the viewer engineer saw. It is a generator fact, not a drawing fault. Three towers founded 1,000 years apart within three cells, all standing, do not read as a place with a past.
- Remedy: close a tower when its founder's line dies out or its realm falls. A new tower in a region with an open tower should either reuse the old one (a new master takes the tower) or sit at least N cells away.

**F2. Epithets repeat inside a world, but the 100-world bound is the limit rather than the problem. Severity: note.**
- Epithets are a small set: Founder, Old, Learned, Exile, Last, Unbeaten, Faithless, Hammer, Gravewarden, Conqueror, Accursed.
- In the `[figures]` blocks, per world:
  - `the Old` appears 7 times in default-s4 and high-s1, 5 times in default-s1 and default-s8.
  - `the Faithless` appears 6 times in high-s1.
  - `the Founder` appears 4 times in default-s4.
- Epithets repeat across realms, so a reader meets "X the Old" in one realm after another. Founder and Exile are rule-bound and fine. Old and Faithless are the ones that grate.
- Remedy: widen the pool for age-based and betrayal-based epithets, for example Grey, Long-Lived, Oathbreaker, Forsworn. Keep a per-world cap of about 3 for any non-rule epithet.

**F3. Heroes rise with no outcome, and some worlds have none. Severity: note.**
- Heroes: 0 in default-s1, s4 and s6 (3 of 12 default worlds). Heroes rise 10 times in high-s2, with 4 monster-slain-by-hero events and no hero_slain.
- Heroes rise and leave no result. Gnit of the Snoskrag Mob "took up the hunt" in 2077 (high-s2 e78) and is then slain, per the `[figures]` block, by a different brood. The hunt after the rise often ends elsewhere.
- Heroes also repeat. Saerigle Wyrmbane (default-s0 f3) kills three monsters (2108 e59, 2177 e73, and one more), which reads well, like a legend.
- Remedy: none required. Consider a short follow-up line, "the hunt failed", when a hero's hunt lapses.

**F4. Validation D1 (hero rises after the provoking monster was already dealt with). Severity: note.**
- I found no same-year case in the 16 worlds.
- I did find a hero rising long after the monster was last driven off, with no stated reason: high-s2 e620 (4999, m12, driven off 4765 e581), e791 (5558, m13, driven off 4733 e536), and high-s3 e280 (3869, m5, driven off 3433 e212).
- These may be monsters that woke again. The hero's text "After the raids of X" does not say so. A reader sees a gap of 234 to 825 years.
- Remedy: cite the raid that actually provoked the hero. The cause already exists (`[causes e619]`), so the text could name the year. Otherwise leave as it is.

**F5. Realm summaries read well but the `[figures]` and `[monsters]` summary blocks are thin. Severity: should fix in follow-up.**
- default-s8 `[figures]`: "figures: 86 ... and 80 more", with 6 generals listed and no ruler, hero or caster shown. A player learns nothing about heroes or casters from the summary.
- `[monsters]` lists 4 of 6, then "and 2 more". The living ones come last in some worlds (m2 awake, m6 asleep in default-s8), so the interesting ones are first only by luck.
- Remedy: list the living monsters first, then the slain. In `[figures]` show at least one of each kind: the ruler at the end, the longest-lived caster, any hero.
- Summary length is fine, 552 to 740 lines (median about 590). Realm blocks stay short. Milestones print as a count plus one line, and the `quiet_stretch` line with "47 new generations of rulers" reads well. I saw no "old ruin" wording in any summary or full file (`grep` finds none), so I could not judge item 4's old-ruin label. If it is meant to show, it does not.

**F6. Marker images. Severity: note.**
- default-s0 and high-s1 are readable. Settlements, mines (the "x" glyphs) and towers are distinguishable at full size.
- The towers show as small purple dots. At this scale three towers on adjacent cells are indistinguishable from one. This follows from F1.
- Libraries beside settlements are hard to pick out from the plain town glyphs; they are tiny.
- I did not open default-s1, high-s0 and the faded copies of the other pairs.

**F7. Living monsters and lairs on former settlements. Severity: note.**
- Default worlds with a living monster (awake or asleep) at the end: 10 of 12. Awake only: default-s0, s6, s8 and s10 (4 of 12). Dead or absent: default-s4 (4 monsters, all slain) and default-s1 (one asleep).
- High pressure: living monsters in 4 of 4.
- The best places are in the sites blocks, for example default-s8 m6 "The Restless of Cickstead", asleep in The Barrows of Cickstead (si24), raids 6, killed 1,090, over a ruined town. That reads as a place to find.
- The weak side is undead naming. high-s1 has ten undead hosts named "Dead/Restless/Host of X" over a lair "Barrows/Howes of X". They read the same.
- Remedy: add a few more undead name forms.

**F8. Rulers on the record are few, and numerals are rare. Severity: note.**
- "rulers: 268 reigns, 10 recorded" (default-s8 c2). The log names about 4% of reigns, which is the design, but the numeral and "reign N" counter on goblin hordes (reign 100 in default-s5) next to a 30-year ruler looks odd.
- Humans and dwarves get numerals ("Seald II", "Drunnair II" at the end, default-s8 c4), as designed. Goblins never do, which is also as designed.
- Remedy: none. Consider hiding "reign N" above, say, 50 in the report.

## What reads well

- Succession disputes with exiles have clear causes (default-s0 e55, e56, e57: crisis, dispute, exile, each citing the one before).
- Generals appear and fall in the right place. `Glieyflie the Hammer`, 11 battles led and 5 won, and `Skatch`, who fell in battle in 3020 (default-s2 e165), read as people with a record.
- Casters, towers and books form a chain: Gloelill raises the Star-Tower (2162 e67), writes books there (2218 e79, 2243 e81 citing e67), and the blight in default-s2 (3323 e238) cites the tower's founding (e219).
- Heroes who slay several monsters (default-s0 f3) give the early ages a legend.
- Ruin lists are long and every ruin has a cause, in the dark tone: "abandoned for fear of The Restless of Cickstead", "overrun by", "sacked by".
- Renamed regions ("The Fallen Downs", "Ashiswick's Pyre") carry the loss into the map.
- No world is empty of figures: 7 to 10 rulers' accessions in every world, and all six races have generals or casters in the larger worlds.
