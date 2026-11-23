# P4-03 territory view: world critic review

Date 2026-11-23. Branch origin/p4/03-viewer (0b93b9e), generator 0.21.0. Seeds 42 (viewer screenshots), 0, 1, 7, 15, 59, 91 at 1024, years 2500, 4500, 5999 (seed 1 at 4500 only looked at closely). Images: `~/.cache/the-unmapped/out/p4-03-critic/t<seed>-<year>.png` and `~/.cache/the-unmapped/out/p4-03-viewer/territory-1024-*.png`.

## Verdict

Pass with notes. The view reads as a political map at a glance, teal is a clear success, and relief survives; the weak spots are same-race neighbours (goblin above all), pale shades that lose their relief and inner borders, and a legend that mixes two meanings.

## Findings, most serious first

1. **Same-race neighbours are the weak pair; goblins worst.** The goblin entries 0, 2, 3 (230,195,20), (170,125,0), (205,160,60) are all gold/ochre and sit side by side. Seed 15, year 5999, NW quarter: gold, ochre and tan realms touch and only the black line separates them; seed 59, year 5999: the big tan realm (205,160,60) and the olive-ochre one (170,125,0) are told apart only by a darker value, and the inner borders inside both are almost invisible. Seed 42, year 6000: Zak-Neez Mob (ochre) beside Grork Horde (yellow) is fine, Slig Mob (tan) beside Zak-Neez is marginal. Suggest: goblin entry 3 to a more distinct hue, e.g. (225,150,0) is no better; use a greenish olive (150,150,30) or a pale straw only if it stays 80+ from steppe. Also keep the rule that a neighbour never takes the same entry (see 6).
2. **Pale shades (entry 1 of every race) lose their relief and sit near free land.** Goblin 1 (250,235,110) is a flat wash: seed 0 north (year 5999), seed 59 west, seed 15 centre. It is also the closest colour to free steppe (224,210,150) and to alpine white at snow lines (seed 59 west edge: pale yellow beside white peaks). Elf 1 (150,230,150) and human 1 (125,235,230) are also flat; human 1 (seed 0 west, seed 91 east coast) is the nearest thing to ocean shallows glow, though still clearly cyan against blue. Suggest: lower the lightness of entry 1 about 15 percent (goblin 1 to (235,215,90), elf 1 to (130,215,130), human 1 to (100,215,215)); that keeps hillshade range and distance from steppe/alpine.
3. **Black borders inside free land compete with realm outlines.** Free regions keep full black borders (seed 91 north, seed 1 east, seed 42 north), so the free land is a dense web of black lines as strong as the lines between realms. A realm's outline does not stand out from the web. Suggest: draw a border with both sides free at about 50 percent black (shade_colour at 8/16 of the underlying cell), leaving black for borders where at least one side is held.
4. **Inner borders (12/16 of the realm colour) are visible on saturated and dark realms, nearly invisible on pale ones.** Visible: orc red (seed 0 south-east), elf green (seed 7), violet. Nearly gone: goblin 1 (seeds 0, 15, 59), human 1, elf 1. They are not noise: they are thin, sparse and do not break the shape. Making them stronger would risk noise, so only the pale shades need help: suggest 10/16 for the dim factor when the colour's luma is above 190 (or use 12/16 for all and 10/16 for entry 1 of each race).
5. **Legend.** (a) The "border within a realm" row has no swatch, so it looks like a heading and gives no idea what to look for. Suggest a swatch of the dimmed first realm colour, or the label "border within a realm (darker shade of the realm's colour)". (b) "free land" lists every water row (ocean ramp) under that heading although the ocean is not free land; suggest a heading `water` before the water rows, or leave them out of the territory legend. (c) The heading rows for races, and civilization rows, are good; the long legend is cut at the ocean row in the 1600x1000 screenshot, so the water rows are not reachable without scrolling (acceptable).
6. **Reused colours when a race has more than four realms.** Not seen in these seeds (no race had more than four realms alive; seed 15 humans had four civilizations founded). The rule k mod 4 is fine; verify the neighbour case when P5 adds races.

## Answers to the questions

1. Neighbouring realms of different races: all told apart at a glance in the seeds seen. Confusable pairs: seed 15 yr 5999 goblin 0/3/2 chain (NW); seed 59 yr 5999 goblin 3 and 2 (centre, only value differs); seed 42 yr 6000 goblin 3 vs goblin 2 (W); seed 0 yr 5999 goblin 1 vs steppe/alpine beside it is unambiguous only because of the border. Orange dwarf 0 (220,105,25) beside red orc 0 (seed 91 yr 5999, seed 0 centre) differ well. Elf 0 vs elf 2 (seed 7 yr 5999 centre) differ in value, OK.
2. Teal reads as land. Against rivers (thin dark blue) and lakes (flat mid blue 64,128,204) it is separate by hue; against the sea, seed 91 east and seed 42 north, cyan realms touch the coast and the coast remains clear because of the shaded halo. Human 1 is pale ice-blue and the least land-like, see finding 2.
3. Colours are loud but about right: the saturation makes realms read as politics, and the hillshade is still clear on entries 0, 2, 3 (ridges in seed 0 orc and dwarf realms, seed 7 violet). Relief is lost on entry 1 shades (finding 2). Free land stays quiet, which is the intended contrast.
4. See finding 4: useful on strong colours, too faint on pale ones.
5. See finding 5.

## Images most worth showing Paul

- `/home/pgratz/.cache/the-unmapped/out/p4-03-viewer/territory-1024-6000.png` (the viewer with legend and year bar)
- `/home/pgratz/.cache/the-unmapped/out/p4-03-critic/t15-5999.png` (goblin chain, the confusable case)
- `/home/pgratz/.cache/the-unmapped/out/p4-03-critic/t59-5999.png` (one goblin power, faint inner borders, pale yellow beside alpine)
- `/home/pgratz/.cache/the-unmapped/out/p4-03-critic/t0-5999.png` (teal, orange, red, violet at their best; pale entries and free-land border web)
