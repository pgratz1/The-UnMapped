# World critic review: P3-09 conflict balance (generator 0.18.0)

Date 2026-11-20. Material:
- the 13 worlds in `~/.cache/the-unmapped/out/p3-09-validation/critic/`: seeds 42, 0, 1, 2, 3, 7, 11 and 23 at 1024; seed 42 `turbulent`; seed 42 with four civilizations per race; seeds 42 and 5 at 512; seed 42 at 2048;
- the same worlds at 0.17.0 (`p3-08-validation/critic/`);
- read-only queries over the 100 default 1024 sweep worlds (`sweep/default/`), and for flights also over the 512, span 12000, dense, turbulent and civs 4 sets.

Event ids and years are those of the 0.18.0 worlds. My scripts (war storylines per world, ending checks, sweep queries) are in my session scratchpad and not in the repository. Validation's D1 (2048 seed 3) and the P3-10 wording items are left out, as asked.

## 1. Verdict

**Pass with notes.** P3-09 fixes what Paul saw:
- No side sues for peace straight after winning its last battle.
- The orcs win more wars than they lose.
- Faeries no longer kill thousands for a handful lost.
- Wars now cost the loser something.

Nothing I found should block the merge. The most serious new problem is narrow, but it is the mirror of Paul's complaint. When a **third party** takes the war goal, the attacker "prevails" and collects tribute or land, even if it lost the fighting (F1: 42 endings in 100 worlds, 7 of them blatant). Below that, the problems are about how the history reads rather than about broken rules:
- the faerie search repeats too often and is always in the summary;
- a loophole lets one border wood be fought over 11 times;
- the orcs are a threat on average but absent in a quarter of worlds;
- goblins start most wars.

## 2. What improved

- **War endings read right.** Seed 42 at 4994–5003 is exactly Paul's case reversed. The Hold of Dulimin kills 991, 971 and 932 goblins in three battles (e318, e319, e326), takes Skragg Pit (e321), then "The Hold of Dulimin prevailed over the Grork Horde after 9 years of war" (e327), and the Grork Horde cedes Larkfell (e328). "Sued for peace" now goes only to a side that has given up, for example "The House of Finen sued the Grork Horde for peace after 21 years of war" (3287 e126) after three lost battles and the sack of Laesiend. Across my 13 worlds, no `peace` or `war_won` names as loser the side that won the last battle, except endings by aim (F1).
- **Truces mean "no one won".** Seed 42 3826 e161: the Hold won the first battle and the goblins won the next two, but the Hold stood. The text is "…made peace after 9 years of war, neither the victor". Seed 42 5601 e473 is similar: the dwarves took Snugg, but the goblins won the last battle, so the war ended in a truce.
- **Raids read as raids.** In seed 42, 5781 e592–e596 is one battle, the sack of Leithonn, "prevailed … after 1 year of war", then "251 were carried off". The tribute line gives a short war a cost and a grudge.
- **Revenge makes chains you can follow.** Seed 1:
  - 4990 e327: the House of Glaefaenn cedes Douredge Woods to the Warrens of Zeez;
  - 5645 e471: the elves declare war over Douredge Woods (causes: e326);
  - 5663 e489: they win it back, after sacking Gnog Pit and Guzz;
  - 5954 e717: they go on into North Wood.

  The oldest people now strikes back. Elves declare 36 wars in the default sweep, against 0 at 0.17.0.
- **The last refuge becomes a story.** Seed 23, 5166 e526–e529: the Warrens of Kaggdrark sack Brerstadford, the Kingdom of Durd's last town. Then "The survivors of Brerstadford fled into hiding and founded Welbrarwick in Southwest Downs", and the capital moves there. The humans survive to the present with 9 settlements. Seed 7 5816 e616: "The survivors of Ghaldgruz Camp fled into hiding and founded Al'dol in Dwalehanger." Al'dol is still the Krom Horde's only town at 6000: a hidden orc camp for a player to find. No race goes extinct in any of the 375 sweep worlds.
- **The orcs can be a scourge.** Seed 42: the Ghuughzuz Horde (awakened 5596) wins against the Duchy of Dant twice (5712 e562 with a cession; 5833 e638 with a tribute). It then sacks Hudrorstead, and "The Duchy of Dant fell to the Ghuughzuz Horde" (5916 e744). That is the design's "most recent age of conflict".
- **The 2048 map has a history.** Seed 42 at 2048 has 24 civilizations, 37 wars and 1,411 events, against a near peace at 0.17.0. Three orc clans and four human kingdoms fight in the last 400 years, two orc clans are conquered by human duchies (e1023, e1115), and the summary at threshold 46 still follows it (F7).

## 3. Findings, most serious first

### F1. A war "won" because someone else took the goal (rule; P3-06 7.7 item 2 with 20.3)

An ending by aim achieved (the defender no longer holds the goal) is exempt from the last-battle rule. The attacker is then treated as the winner and takes 20.3's cost, even when a **third civilization** took the goal and the attacker lost the fighting.

- **Seed 23, 5883–5906.** The Warrens of Kaggdrark attack the Hold of Muli over Illcairn Barrows. Muli wins three of the four battles (e871, e876, e887; about 2,300 goblin dead against 300), and takes Gneenk (e878). Meanwhile the orcs of the Urarz Horde take Nogg and Krox in Illcairn Barrows (e883, e890). Muli sues the orcs for peace and pays them 1,308 people (e891, e892). The next year comes "The Warrens of Kaggdrark prevailed over the Hold of Muli after 23 years of war" (5906 e893), and Muli pays the goblins too: "1175 were carried off" (e894). The goblins took nothing and lost every battle but one.
- **Seed 3, 5859–5888.** The Kingdom of Tead wins 4 of 7 battles against the Sneegg Horde, including the last (e709, 757 goblins dead). The Asherdtos League takes Northeast Downs in another war (e707, 5887). Then "The Sneegg Horde prevailed over the Kingdom of Tead after 29 years of war" (e710), with tribute 1,880.
- **Seed 42 with civs 4, 3969–3970.** The Grunk Horde splits by secession (e841). The Warrens of Dribnark "prevailed … after 1 year of war" (e845) without a battle, and receive Witchgate Barrows (e846).

**Sweep, 100 default worlds:**
- 42 endings in which the attacker wins while the goal is held by neither side; all 42 carry a tribute or cession;
- 19 in which the attacker took nothing;
- 7 in which it also lost more battles than it won (s22 e690, s23 e893, s3 e710, s58 e83 and e295, s61 e468, s93 e489);
- 2 with no battle at all.

There are 23 cases of one payer paying two tributes within five years, and several of them come from this rule.

**Proposal (rule, smallest).** In 7.7 item 2, the aim counts as achieved for the attacker only when the attacker (or a civilization it conquered in this war) holds G. When G passed to anyone else (a third party, a secession, abandonment), the war "lost its cause". End it by the score with the last-battle rule of 20.15 item 3, and when the score is 0 or the loser won the last battle, write a truce. That makes all seven blatant cases truces or defender wins. Validation can check it as invariant (8) without the aim exception.

### F2. The faerie search: right idea, repeated too often, and always in the summary (rule and salience; question 2)

The first search reads well. Seed 42 4418–4423: "The Grork Horde searched the Tangle of Yews for the Teeshti Court and found no one; the war ended after 5 years." It is eerie and faerie-like, and the faerie courts survive: 0 of 100 worlds end with one faerie settlement, against 23 at 0.17.0. But:

- **Repetition.** Seed 1 has 11 `search_abandoned` in 29 wars (4973–5985). Pyrehurst is searched three times (Skreegg 5648, Alid 5781, Skreegg 5985), as is the Weald of Floods (4973, 5041, 5758). The faeries' whole visible history from 4178 to 6000 is their foundings and eleven times "found no one"; they take part in one battle of their own (5841 e584). Seed 42 has 7: Badgerhorn Wood is searched by three different hordes (Zak-Neez e428, Grork e522, Slig e683). In the sweep:
  - 437 unfound wars (median 4 per world, max 12);
  - goblins make 323 of them, orcs 73;
  - the most unfound wars over one (defender, region) is 3 or 4 in every world (median 3).

  The weariness rule allows up to `weary_wars` = 3 per region, and a faerie court holds 2–6 regions, so 10 or more fruitless wars per world are within the rules.
- **Always in the summary.** Salience 40 is above every default summary threshold (20 / 31 / 36), so each fruitless war takes two summary lines (the declaration at 70 and the search at 40). P3-06 20.17 item 2 meant it to fall "below the summary's usual threshold". In seed 1 that is 22 of 217 summary lines, 10%.
- **"Found no one" after finding them.** In 67 of the 437 unfound wars the searcher had already fought a battle in the very region it then "searched and found no one"; in 18 it had sacked or taken a settlement. Seed 42: the Grork Horde sacks Biatwian in the Tangle of Yews (4985 e310; 402 dead), then "searched the Tangle of Yews … and found no one" (4988 e312). Seed 42 5925 e747: the Teeshti Court defeats the horde in the Gnarled Forest, then 5933 e750: "searched the Gnarled Forest … and found no one". Seed 1: two battles in the Grove of Boars (5841, 5844), then e599.
- **Faeries never act.** Faeries declare 5 wars in 100 worlds. Being hard to find is now all they do in history.

**Proposals.**
- (a) **Rule:** a `search_abandoned` makes its defender weary of that region at once, for every claimant: set 20.4's count to `weary_wars`, reset as now by a change of owner. The wood gets a reputation. That keeps the first search per region and drops the repeats: at most one per faerie region, about halving the 437.
- (b) **Data:** `search_abandoned` salience 40 → 30, so that only the first per world, or none, reaches a default summary. Alternatively, the summary shows the first per (searcher race, hidden civilization) and folds the rest.
- (c) **Template:** a second form when the war had fought or sacked in the region, chosen by a value `found_before` (0/1): "{searcher} lost the trail of {hidden} in {@region}; the war ended after {years} years."

### F3. One border wood fought over eleven times: weariness reset by re-colonisation (rule; question 4)

Seed 42, the Wood of Stags (region 35), held by the House of Finen:
- the Grork Horde declares war over it 7 times: 2524 e56, 3266 e114, 3835 e166, 4274 e206, 4968 e288, 5569 e423 and 5872 e681;
- the Warrens of Dribnark (5580 e438), the Hold of Dulimin (5594 e455) and the Ghuughzuz Horde (5679 e540, 5780 e590) add four more;
- that makes 11 wars, each sacking a new elven village: Glathenddell, Laesiend, Elalcie, Lienond, Nienann, Ithiglae, Lesol, Ithiefar, Leithonn, Sogla;
- the House of Finen pays tribute to goblins or orcs six times.

`region_control` shows why weariness never applies. Each sack empties the region for 3 to 66 years, then the elves resettle it, and that counts as a "change of owner" that resets the count. There are ten control rows, all with civ 1: 1475–2527, 2530–3287, … 5849–5873. The same pattern appears in seed 42 at 512 (Boughcombe Wood, 8 wars) and in civs 4 (the Wood of Stags, 8). Across the sweep `wars_same_goal_max` reaches 11, while `wars_same_goal_unchanged_max` stays ≤ 4.

The repetition has a dark charm ("ten elven villages lie in ash under the Wood of Stags"). But it is the same sentence pair seven times with the same outcome, and it is the world Paul reads.

**Proposal (rule).** In 20.4, "a change of owner" means a different civilization: a gap with no owner followed by the same owner does not reset the count. Optionally (P3-05, taste), a civilization does not re-colonise for `resettle_wait` years a region where its settlement was sacked, so the ruin stays a ruin for a while.

### F4. Orcs: a threat on average, absent in a quarter of worlds (tuning; question 5)

The target is met in aggregate: orcs win 115 wars and lose 68. Per world it is uneven:
- in 24 of 100 default worlds orcs declare no war in their age at all;
- in 44 they win no war;
- in 59 they never sack or take a settlement;
- 25 worlds have orc wins and losses at 0 / 0, and 14 at 0 / 1.

Orcs are also the race that most often loses its last town and flees: 16 of the 46 flights are orcs. They are the only race whose worlds with one standing settlement went up (7, from 2).

From the chronicles:
- seed 3: the Warband of Uld'uguum (awakened 5602) fights nobody in 398 years;
- seed 7: the Krom Horde (5637) loses all three of its wars, flees in 5816 and ends with one camp;
- seed 42: the orcs are the scourge of the age (above).

The likely cause is `odds_min` 700 against kingdoms ten times the horde's size in the orcs' roughly 400 years. That is P3-09 decision 8.5's trade-off, now visible per world.

**Proposal (tuning, for measurement).** Give orcs a per-race odds minimum (race field `odds_min_permille`, orc 500, others the 700 of `history.conflict`). The prototype measured 36% of orc-age declarations at `odds_min` 500 globally. A per-race value avoids making the weak peoples reckless. Target: orcs declare at least one war in ≥ 90% of worlds, with T5 still met.

### F5. Goblins start most wars: monotony in a quarter of worlds, not a plausibility problem (question 6)

Goblins declare 1,922 of 2,523 default wars (76%). The per-world median is 785‰, and in 24 worlds goblins declare ≥ 900‰ of the wars (seed 42 civs 4: 91 of 100). Their targets:

| Elves | Faeries | Goblins | Dwarves | Humans | Orcs |
| --- | --- | --- | --- | --- | --- |
| 796 | 383 | 322 | 245 | 175 | 1 |

Goblins are the largest people in 58 worlds and hold ≥ 700‰ in 21. Humans are the largest in 37 worlds (≥ 700‰ in 6), including 6 of my 13, among them seed 42 and the 2048 map.

This fits the design: goblins are numerous, made in opposition to the elves, and have 3,900 years against the humans' 1,000. Reading seed 42 or seed 1, the goblin wars against elves are the spine of the history, not noise. Where it hurts is the share: in a quarter of worlds every other people is only ever a victim. Elves start 0.4 wars per world, dwarves 1, humans 1.5.

**Smallest proposals, in order:**
- (i) F2 (a) alone removes most repeat goblin searches of faerie woods. Searches are 323 of the 1,922 goblin wars; perhaps 150–200 of them are repeats, which would bring goblins to about 72%.
- (ii) If more is wanted: human `aggression` 400 → 500 (rate 160 → 250 per 1,000). Humans are "ambitious" in the design and start few wars; this gives the last thousand years a second aggressor without touching goblins.
- I would not lower goblin aggression; the elf–goblin feud is the design's backbone.
- Target for validation: goblins ≥ 900‰ of declarations in ≤ 10 of 100 worlds.

### F6. A hidden refuge does not behave as hidden (rule, minor; question 3)

Flights happen in 37 of 100 default worlds (46 flights), and they read well (section 2). What follows is sometimes not hiding. The refuge stays hidden, but its colonies are not (P3-05: "its colonies are not hidden"), so the people is back in contact almost at once:
- seed 23: Welbrarwick founds Geantford in Woodcutter's Landing 38 years after "fled into hiding" (5204 e570), and three more colonies by 5279;
- seed 7: Al'dol founds Duuk'thaak (5921), and the Krom Horde, still hidden, declares war on the elves (5924 e676) and on Clan Nirgurn (5975 e724), losing Duuk'thaak, Zol Fort and Thukh'zom.

In the sweep, 7 of 46 default flights are followed by a war declaration by the fled people within its 300 hidden years, and 4 by the loss of a further town.

**Proposal (rule).** While a civilization's capital is hidden, the civilization makes no claims or declarations, and its colonies in the refuge's region are hidden until the same year. It lies low for its hidden years, which is what the sentence promises.

### F7. 2048: busy but legible; the summary hides the fighting (report; question 7)

Seed 42 at 2048 has 24 civilizations (4 elf, 4 goblin, 4 dwarf, 3 faerie, 5 human, 4 orc). With 1,411 events the summary threshold rises to 46, which leaves 156 lines, 102 of them in 5500–6000. Battles (30) and sieges (25) disappear, so a summary war is declaration, sack, peace. The human states are large: the Duchy of Breld has 215,126 people in 14 regions, and humans hold 677‰ of the people. It is not noise: each line names who and where, and conquests (e1115, e1338) close stories. But the four Houses and Realms of elves and four Duchies and Kingdoms are hard to tell apart in a 500-year block.

**Proposal (P3-07, report).** At 2048 the summary threshold should be computed per civilization, or the summary grouped per war, rather than one global cut. Leave the rules alone.

### F8. Names (filter; question 8)

The 393-word list does what Paul decided. The examples validation lists (Wild, Nix Hole, Snag, Drab, Notch, Gnut-Snag) now pass and read acceptably for goblins. Two kinds of name still stand out:

- **Comic or crude by compound**, not touched by this change, since `crude.toml` is unchanged:
  - Suckgirstead (seed 0, settlement 50, human);
  - Merdford (seed 0, 81, human);
  - Fannie (seed 1, 65, elven);
  - Bonkhair Hold (seed 7, 71, dwarven; "bonk" is only on the whole-word list);
  - Kakdird (seed 42 civs 4, 100, dwarven; "kak" is whole-word only);
  - Lopee (seed 11, 56, faerie).

  Proposal (data): move `suck`, `bonk` and `kak` from whole to `at_start`, and add `merd` at start and `fanni` and `fanny` whole.
- **English words now allowed**, which break a culture's sound when they land on it: Till (seed 42, 82, a faerie village in the Tangle of Yews among Piaba Glade and Twiatwo), Herd (seed 42, 94, human) and Mam (seed 11, 92, orcish). Ashes (seed 3, 115, human) and Wild (seed 0, 64) suit the tone. This is Paul's decision and I would keep it. If anything, a per-culture opt-out: faerie and elven names should not take an English whole word.

## 4. Patterns across the sweep (100 default worlds unless stated)

- Endings: `peace` 7, `war_won` 5, truce 5 and `search_abandoned` 4 per world (medians); conquest 1.
- Attacker wins where the goal ended with a third party or no one: 42; with nothing taken: 19 (F1).
- Unfound wars: 437 in 92 worlds; 67 of them after a battle in the searched region (F2).
- The most wars over one (defender, region): up to 11 (seed 42), made possible by resets through re-colonisation (F3).
- Orcs: no declaration in 24 worlds, no win in 44, no settlement taken in 59; 16 of the 46 flights (F4).
- Goblins: ≥ 900‰ of declarations in 24 worlds; largest people in 58 (F5).
- Flights: 46 in 37 worlds; 7 followed by a declaration while hidden (F6).
- Faeries with one settlement: 0 (0.17.0: 23). Elves with one settlement: 2 (0.17.0: 8). No race extinct in 375 worlds.

## 5. Questions for Paul

1. **F1.** When a third party takes the war goal, should the attacker's war end as a truce ("lost its cause") or be decided by the score? I recommend the score with the last-battle rule, falling back to a truce. Either way, no tribute for a war the attacker did not win.
2. **F2.** Should one failed search be enough to make a faerie wood weary for everyone, so that faeries gain a reputation and the search appears once per wood? And may `search_abandoned` drop to salience 30?
3. **F6.** Should a people that fled into hiding lie low (no claims or declarations, colonies hidden too) for its hidden years?
4. **F3.** Should the same owner resettling a sacked region reset war weariness? I recommend not.
5. **F4.** Is "orcs win more than they lose, summed over worlds" enough, or should the orcs be a visible threat in almost every world (per-race `odds_min`)?
6. **F5.** Is a goblin-led history in a quarter of worlds acceptable as the design's "numerous" goblins? Or should humans become a second aggressor (aggression 500)?
