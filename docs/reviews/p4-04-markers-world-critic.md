# P4-04 settlement and place markers: world critic review

Date 2026-11-23. Seeds: 42 (viewer screenshots, CLI x1/x2), 15 and 91 (generated here at 1024; map composite, elevation, territory year 100). Images are in `~/.cache/the-unmapped/out/p4-04-viewer/`, `p4-04-impl/` and `p4-04-critic/`. Limits: I did not render a busy war year for seeds 0 and 59, did not test hover myself (only the supplied screenshot), and judged ring colours from the 5978 screenshot and the key list only.

## Verdict

Pass with notes. The markers carry the history onto the map and work over every view. The problem is legibility at the fitted zoom: several kinds and states are hard to tell apart there.

## What works

- Fortress (diamond) and hamlet/village (circles) against town/city/capital (squares) are told apart at once by shape (`s42-1024.territory-6000.markers.x2.png`). Seed 15's map composite shows the fortress diamonds sitting in the dwarf mountains, which reads well.
- Marker density is right. About 85 standing settlements on seed 42 do not clutter the map, and the settlement clusters make the realms readable even where the fills are the same.
- Over the plain map and elevation (`s15-map.png`, `s91-elev.png`) the white halo and black ink keep every marker visible on green, brown, white peaks and blue water. Markers work on every non-territory view (Q5).
- Rings are rare. `territory-5978-markers-rings.png` shows two rings in a year with five events. They do not hide any settlement, because the settlement shows through the middle.
- The sack hover text in `territory-6000-zoom-ruin-hover.png` is the best thing in the stage. A ruin on the coast tells its story in one line.

## Findings, most serious first

**F1. At the fitted zoom the declining and capital states cannot be read (Q1, Q4). Should block the merge, but only if Paul wants those states usable without zooming.**
In `territory-6000-markers.png` (z about 0.84, glyph cell 1 screen pixel, so the viewer is about 1/2 the size of the CLI x2 image) a hamlet is about 7 px and a village about 9 px. A "declining" village (hollow interior) looks the same as a standing one, and a capital looks the same as a city. In the key list the rows `city`, `capital` and `town` are near-identical squares. Only at CLI x2 are the capital's thick outline and the hollow ring visible.
Suggestion: in the viewer, size the glyph cell by zoom as 2 when z is below 1.5 (1 only when z is below 0.5, i.e. a 2048 map). That makes a hamlet 14 px, a village 18 px, a city 22 px and a capital 26 px at the fitted view of a 1024 map. Also let a capital carry a distinct shape cue (for example a darker ink fill instead of only a thicker outline), and give declining a 2 cell interior at minimum.

**F2. City and town are hard to separate (Q1).**
A city differs from a town only by a 3x3 ink square in the centre. At x2 this reads as a dot in a square, but at the fitted zoom it is a black speck. Suggestion: make the city one cell larger than the spec's S(3) glyph or make its centre a diamond or cross of ink. Alternatively accept it, since the hover text names the kind.

**F3. Ruins are too faint (Q1).**
The ruin X is 7 px with a thin ink line and a white halo. On seed 42, `territory-6000-markers.png`, the ruins (for example west of the Finen realm) are nearly invisible. These are the places the player wants to find. The faded ruin (grey X, no halo, `s42-1024.territory-6000.markers-faded.x2.png`) is acceptable only as a hint, and is lost on a light fill. Suggestion: draw ruins at glyph radius 4 (9x9) with a 2 cell ink stroke, and keep the faded ruin at radius 3 with a halo, since a grey X without a halo vanishes on yellow and light cyan realms.

**F4. The owner's fill barely shows inside its own realm (Q2).**
This confirms the worry. In the cyan Brel realm in the north-east and in the Grork yellow in the middle, a town is a pale square with a black outline that reads only because of the ink and halo, not the fill. It is acceptable since the outline holds. Where it fails is on small markers: a hamlet in its own realm is a black dot. On free land and snow (seed 15 map composite) the fills read fine; against dark forest the white halo does the work. Suggestion: keep the halo, and in the viewer make the key list swatch show the owner colour on a mid-grey backdrop so the fill is understood. Where the realm colour and the ground colour are the same, no further change.

**F5. Siege and plague rings look alike in the key list (Q3).**
Both the siege and the plague rows show a yellow/gold ring (the plague a little greener). Sack or capture (purple/magenta) and battle (red) are clear. Suggestion: move plague to a clearly different hue (a sickly green or white-green against yellow siege). I could not confirm the difference on the map itself, since the supplied rings are one battle-type and one sack-type ring.

**F6. Hover result is shown far from the cursor and ruins have a tiny hit area (Q6).**
The text appears in the bottom label, not near the mouse. At zoom 6 the ruin X is about 14 px, which is small to hit. Suggestion: hit-test with a one-cell margin around small markers. The text itself is clear. Two small points: "ruined in 4289: The Grork Horde sacked..." is good; but "population 831 recorded in <since>" in spec 8.5 should render a year; and a ruin whose event is 0 gives no cause, so say "cause not recorded" instead of nothing.

**F7. Key list rows.**
`pass` is a 5 px chevron, nearly unreadable in the key, and the faded-ruin row appears only when its option is on (good). The "other event" ring is white and is not explained. Suggestion: name it "other event (founding, truce ...)" or drop it from the key when no such ring is on screen.

## Patterns across seeds

- Seed 42, 15 and 91 all show the same sizes problem (F1, F3): capitals and declining marks are unreadable at the fitted zoom, in 3 of 3.
- Fortress diamonds cluster in mountains in 3 of 3, which tells the dwarf story well.
- Ruin density is plausible (10 to 15 per world on the three seeds at the end year) and they sit in or near the realm that lost them.

## Suggestions (rules and parameters)

1. Viewer glyph cell g: 2 for z below 1.5 on maps of 1024 or less (F1).
2. Ruin glyph radius 4 and a thicker stroke; faded ruin with a halo (F3).
3. Plague ring colour distinct from siege (F5).
4. Hit margin of one cell for small markers; hover text for a ruin with no event and the `<since>` year (F6).
5. Capital and city cue stronger than outline thickness (F1, F2).

## Merge blocking

F1 is the only finding I would have fixed first, and it is a viewer constant, not a library change. Nothing in the CLI overlay blocks the merge. F2 to F7 can follow in a later round.

## Images to show Paul

1. `/home/pgratz/.cache/the-unmapped/out/p4-04-impl/s42-1024.territory-6000.markers.x2.png` (kinds, capitals, ruins readable)
2. `/home/pgratz/.cache/the-unmapped/out/p4-04-viewer/territory-6000-markers.png` (the fitted-zoom size problem)
3. `/home/pgratz/.cache/the-unmapped/out/p4-04-viewer/territory-6000-zoom-ruin-hover.png` (the hover text)
