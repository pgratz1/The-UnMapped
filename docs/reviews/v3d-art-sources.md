# V3D art sources: terrain textures and vegetation models

Research of 2026-11-28. Question: which sources of ground textures and vegetation models may be committed to a public MIT repository, modified, and later shipped in a game.

How to read this file: a sentence in quotation marks was read on the page at the given URL on 2026-11-28 through an automated fetch (the fetch tool returns extracted text, so a quote is the page's wording as extracted; check it by eye before relying on it in a licence file). Anything marked **[unverified]** comes from a search-result snippet or from general knowledge and was not read on a primary page. Nexus Mods refused every automated fetch (HTTP 403), so every statement about a Skyrim mod is **[unverified]**.

## 1. Verdict

Recommended, in this order:

1. **Poly Haven** (CC0). One source covers nearly every ground type and has scanned conifers, shrubs, dead wood and boulders. Offers the OpenGL normal map directly, PNG at 1K and 2K, and stable per-file URLs listed by a public API.
2. **ambientCG** (CC0). Second source for ground textures, strongest on snow, grass and plain soil. The licence page names inclusion of raw files in a video game explicitly.
3. **Quaternius** (CC0 on the two packs checked) and **Kenney Nature Kit** (CC0) for low-poly, stylized trees, rocks and bushes: placeholders or a stylized look, not a match for scanned ground textures.
4. **cgbookcase** (CC0) as a third texture source for rock, gravel and pebbles.

Avoid, and why:

- **ShareTextures.** Its licence is a "Custom CC0 ... with specific restrictions", among them "No asset redistribution on other websites, in plugins, or as part of collections without our written permission." A public repository holding the raw files is redistribution on another website.
- **All six Skyrim mods** as things stand. None could be read on its own page. From what search results quote: one is an AI upscale of the game's own textures (derived from Bethesda's work, which no mod author can license), two are compilations of other authors' assets, one requires release on Nexus and bars paid use, and one requires the reusing "mod" to have open permissions. Only "WOODLAND - Flora Overhaul" is reported to grant use in "any project you like"; it is the one worth a manual check and a written confirmation from its author. Nexus permission fields are written for use in other mods, not as copyright licences for use in another game.
- **OpenGameArt** was not examined (fetch budget). Its items carry individual licences; only items marked CC0 on their own page qualify, each checked singly.

Gaps in the CC0 sources: no reeds were found in the lists read; heath and moor ground has weak coverage (moss and grass-and-rock scans only); Poly Haven's broadleaf trees are few.

## 2. CC0 libraries

| Source | Licence sentence (quoted) and URL | Attribution | Formats and maps | Direct URLs |
| --- | --- | --- | --- | --- |
| ambientCG | "All ambientCG assets are provided under the Creative Commons CC0 1.0 Universal License." "You can copy, modify, distribute and perform the assets, even for commercial purposes, all without asking permission." The page also says one can "include the raw files in your project, for example a video game." https://docs.ambientcg.com/license/ | Not required: "You don't need to give credit but I would of course appreciate it, if you did it anyways." | Zip archives per asset in 1K, 2K, 4K, 8K, each as JPG or PNG (seen for Snow015 in the site's API). Maps listed by the API: color, displacement, normal, roughness, ambient-occlusion. That each zip holds both `NormalGL` and `NormalDX` files is **[unverified]** (general knowledge; the API response showed only `NormalDX` in preview names). | `https://ambientcg.com/get?file=Snow015_1K-JPG.zip` (pattern `<AssetId>_<res>-<fmt>.zip`, seen in the API). Asset page: `https://ambientcg.com/a/<id>`. No stability promise found. |
| Poly Haven | "Our assets are all licensed as CC0, which is effectively Public Domain". "You can redistribute them, share them around, include them when sharing your own work, or even in a product you sell." https://polyhaven.com/license | Not required: "You do not need to give credit". | Textures (seen for `forrest_ground_01` at `https://api.polyhaven.com/files/forrest_ground_01`): maps Diffuse, nor_gl (OpenGL), nor_dx (DirectX), Rough, Displacement, AO, plus packed `arm` (AO, roughness, metal); each at 1k, 2k, 4k, 8k as jpg, png and exr. Models: glTF, FBX and Blender files with 1k to 8k textures **[unverified]**. | `https://dl.polyhaven.org/file/ph-assets/Textures/png/2k/forrest_ground_01/forrest_ground_01_diff_2k.png` and `..._nor_gl_2k.png` (from the files API). The API is the supported way to get URLs. Note the terms forbid "Web scraping or data mining without express permission" of the website; a handful of downloads through the API is not that, a bulk mirror might be. |
| Quaternius | FAQ: "each kit uses either CC0 or the QAL License." "QAL does not allow direct reselling." https://quaternius.com/faq.html . Stylized Nature MegaKit: licence shown as CC0; "Free to use in personal, educational and commercial projects." https://quaternius.com/packs/stylizednaturemegakit.html . Ultimate Nature Pack: licence shown as CC0. https://quaternius.com/packs/ultimatenature.html | "No, attribution is not necessary. However, credit is always appreciated." | Stylized Nature MegaKit: "116 Unique nature models", "40 Tree models, 35 plants and flowers, 27 rocks, and many more"; FBX, OBJ, glTF; but "A portion of the kit is completely free", the rest is paid (Pro and Source tiers). Ultimate Nature Pack: "150 models with everything you need!", FBX, OBJ, Blend, untextured (vertex or material colours). | Downloads go through the pack page (Google Drive or itch.io) **[unverified]**; no stable file URL. Check the licence `.txt` in each download, since a kit may be QAL, not CC0. |
| Kenney | "Yes, all game assets on the asset pages are public domain licensed (CC0)." "You're free to use them, even in commercial projects." https://kenney.nl/support . Nature Kit page shows "Creative Commons CC0", 330 files, tags nature, tree, rock, foliage. https://kenney.nl/assets/nature-kit | "Attribution is not required." | Low-poly models; formats not stated on the page (FBX, OBJ and glTF in the zip **[unverified]**). No PBR textures. | One zip from the asset page; no stable URL found. |
| cgbookcase | "The textures are published under the CC0 1.0 license, which means means you can use them for free without giving credit." https://www.cgbookcase.com/textures (the separate `/license` page returned 404) | Not required (same sentence). | Resolution filters 1K to 8K; listings seen state 4K, 6K or 8K. Maps per texture not shown on the listing: base colour, normal, roughness, height, AO as PNG **[unverified]**; normal convention **[unverified]**. | Not checked. |
| ShareTextures | "licensed under the Custom CC0 (Creative Commons Zero) license with specific restrictions"; "No asset redistribution on other websites, in plugins, or as part of collections without our written permission."; "CC0 only applies to assets downloaded directly from our site." https://www.sharetextures.com/p/license | "No attribution required (but appreciated!)." | Not checked. | "No automated downloads, hotlinking, or embedding direct downloads in third-party apps". **Not usable for a public repository.** |
| OpenGameArt | Not examined. | | | |

Working rule for the repository: keep, beside each imported asset, a line with source, asset id, URL, licence and download date. CC0 asks for none of it, but it is the only proof of origin later.

## 3. Skyrim Special Edition mods (Nexus Mods)

Every Nexus page fetched returned HTTP 403 (`www.nexusmods.com` and `nexusmods.com`). The table reports what search results quote from the pages; none of it was read in place, none of the Nexus "Permissions and credits" fields were seen, so no contradiction between description and fields could be checked. All rows are **[unverified]**.

| Mod (Nexus id, URL) | What search results quote about permissions | Sources of the assets, as reported | Verdict |
| --- | --- | --- | --- |
| Enhanced Vanilla Landscapes (144135, by ICFur) https://www.nexusmods.com/skyrimspecialedition/mods/144135 | No permission text found. | Tagged "AI Assisted"; the page is reported to say the game's textures were upscaled with generative AI and touched up by hand. | **Not usable.** The textures are derived from the game's own, which belong to Bethesda; the mod author cannot grant rights in them whatever the page's fields say. |
| Seasonal Landscapes (66903, by Mefariah) https://www.nexusmods.com/skyrimspecialedition/mods/66903 | Reported to say it uses only open-permission assets and to ask that the original authors be credited when its models or textures are reused. | A compilation: credits reported for Tamu75, Mindflux, Elinen, joshezzel, TateTaylorUSA, Jelidity, ossumpossum, wSkeever, DrJacopo, ArtByMari, Rudy, Skurkbro, T4gtr34um3r. | **Not usable** as a source. Rights sit with a dozen other authors, each with their own terms, granted for Skyrim mods; each would have to be traced to its original page. |
| Seasonal Landscapes - Unfrozen (71967, by Mefariah) https://www.nexusmods.com/skyrimspecialedition/mods/71967 | Permission section not found. A related patcher by the same author is reported as "Asset use is allowed as long as you credit me" with conversion to other games prohibited. | Builds on Seasonal Landscapes and powerofthree's Seasons framework; texture authorship not found. | **Unverified; treat as not usable.** Same compilation problem as the main mod. |
| WOODLAND - Flora Overhaul (48926) https://www.nexusmods.com/skyrimspecialedition/mods/48926 | Reported: the author calls the permissions completely open and says the tree and shrub models can be used "in any project you like", asking to be told. | Reported as the author's own tree and shrub models; the mountain flower model is credited to JonnyWang13. Whether bark and leaf textures are original, photo-sourced or from elsewhere was not found. | **Unverified; usable with conditions if confirmed.** Conditions: read the page and its permission fields by hand; get the author's written statement that use in a non-Skyrim game, modification and redistribution in a public repository are allowed and under what licence; exclude anything credited to others; confirm no texture is derived from the game's. |
| Trees of Iduna (92303) https://www.nexusmods.com/skyrimspecialedition/mods/92303 | Reported: anyone may use the assets with credit to the author and contributors, the result must be released on Nexus, no use in paid mods, textures not to be used in mods that earn donation points. | "a collection of many talented mod authors tree meshes"; bark textures made by the author "from photographing the trees of my local area". | **Not usable.** Release on Nexus and no paid use are incompatible with a public repository and a game that may one day be sold; the meshes belong to several other authors. |
| Cathedral Landscapes (21954, by JonnyWang13) https://www.nexusmods.com/skyrimspecialedition/mods/21954 | Reported from the author's pages: "All assets created by JonnyWang are free to use, so long as you share your mod with open permissions as well." Assets by other authors need their permission. | Mixed: the author's own assets and others'. Which landscape textures are whose, and their photo or scan sources, was not found. | **Unverified; usable with conditions at best.** The grant is worded for mods and has a share-alike condition with no defined licence; it needs the author's written confirmation for a game and a list of which files are his. |

Formats and conversion (general knowledge, **[unverified]** against a primary page):

- Textures are DDS (BC1, BC3 or BC7), which Godot imports directly. A Skyrim landscape set is a diffuse map and a normal map with specular in its alpha channel; there is no roughness map and, outside parallax add-ons, no height map. Both would have to be authored, and the normal map's green channel checked against Godot's OpenGL convention.
- Meshes are NIF (Gamebryo/NetImmerse). Conversion goes through Blender with a NIF add-on (PyNifly or the NifTools add-on) and export to glTF; materials are rebuilt by hand, and tree LOD billboards, collision and wind data do not carry over.
- Bethesda's EULA and Creation Kit terms on game-derived content were not read for this review.

## 4. GitHub: Git LFS and ordinary files

| Fact | Statement and URL |
| --- | --- |
| Included LFS quota, GitHub Free (personal) | 10 GiB storage and 10 GiB bandwidth per month (table on the pages; one page writes GiB, the other GB). "Each GitHub account includes a quota of free bandwidth and storage for Git Large File Storage (Git LFS)." https://docs.github.com/en/billing/concepts/product-billing/git-lfs and https://docs.github.com/en/billing/reference/product-usage-included |
| What counts against bandwidth | "When you download a Git LFS file, the bandwidth you use is included in the repository owner's bandwidth usage." Downloads by GitHub Actions count too (same page). So every clone or fetch by anyone that pulls LFS objects counts against the owner. Uploads were not stated as counted. |
| CI checkouts | `actions/checkout` input `lfs`: "Whether to download Git-LFS files", default "false". https://github.com/actions/checkout/blob/main/README.md . A checkout without `lfs: true` fetches pointer files only and uses no LFS bandwidth (inference from the default); with `lfs: true` every job downloads the objects unless they are cached. |
| Over the quota | With a $0 budget: "Git LFS usage is blocked for the rest of the calendar month." Without a payment method: over storage, clones get pointer files only and pushes of new files fail; over bandwidth, LFS is disabled on the account until the next month. Usage resets monthly. (Billing page above.) |
| Price beyond the quota | "Bandwidth is billed for each GiB of data downloaded"; "Storage is billed by calculating an hourly usage rate." The docs pages give no figure and point to the pricing calculator. $0.07 per GiB-month of storage and $0.0875 per GiB of bandwidth are reported by search results from https://github.com/pricing/calculator and GitHub's changelog of 2024-06-03 **[unverified: not read on the page]**. |
| Ordinary files | "larger than 50 MiB, you will receive a warning from Git"; "GitHub blocks files larger than 100 MiB."; browser uploads "can be no larger than 25 MiB." Repository size: under 1 GB ideal, "less than 5 GB is strongly recommended." https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github |

Consequence for this project: a 2K PNG is roughly 5 to 15 MB **[estimate]**, so a terrain set of 12 to 16 materials packed into two textures each is a few hundred MB as PNG, far under the per-file limits. In plain Git every clone and every one of the nine or so CI jobs per run downloads it, and history keeps every replaced version. In LFS, CI stays free of it by default (`lfs` false), but 10 GiB of monthly bandwidth is only a few dozen full clones of a few-hundred-MB asset set, by anyone.

## 5. Godot 4.x import and Terrain3D packing

| Fact | Statement and URL |
| --- | --- |
| Image formats | Godot imports BMP, DDS, KTX, EXR, HDR, JPEG, PNG, TGA, WebP and SVG. https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_images.html |
| Normal-map convention | "Godot requires the normal map to use the X+, Y+ and Z+ coordinates, which is known as an OpenGL-style normal map." A DirectX-style map needs the import option Normal Map Invert Y. (Same page.) |
| Compression on import | A texture used in 3D is detected (Detect 3D), switched to VRAM Compressed with mipmaps and reimported. VRAM Compressed is "Stored as S3TC, BPTC or ETC2 depending on platform."; with High Quality it "uses BPTC compression on desktop platforms and ASTC compression on mobile platforms." A texture used as a normal map is imported as RGTC (two channels). Mipmaps "increase memory usage by roughly 33%." (Same page.) |
| `.import` files and cache | "Make sure to commit these files to your version control system, as these files contain important metadata." The imported results live in the hidden `.godot/imported/` folder; "committing the `.godot/` folder to the version control system is not recommended." Deleted results are reimported; a source file is reimported when its MD5 checksum changes. https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/import_process.html |
| Terrain3D packing | Albedo texture: "RGB: Albedo, A: Height". Normal texture: RGB the OpenGL normal map, "A: Roughness" (AO, when used, is folded into the RGB side by the packer). https://terrain3d.readthedocs.io/en/stable/docs/texture_prep.html (the `stable` docs; that they are those of v1.1 was not checked) |
| Terrain3D formats | PNG, imported with "Mode: VRAM Compressed", "Normal Map: Disabled", mipmaps generated, optionally High Quality (gives DXT5, or BPTC with High Quality); or DDS "BC3 / DXT5, linear (intel plugin), Color + alpha, mipmaps generated", used as is. Note `Normal Map: Disabled`: RGTC would drop the roughness in alpha. |
| Terrain3D sizes | "All albedo textures must be the same size, and all normal textures must be the same size." "All albedo textures must be the same format, and all normal textures must be the same format." Power-of-two sizes are recommended. "Terrain3D supports up to 32 texture sets". |
| Packer | The menu entry "Pack Textures..." takes albedo, height and optional normal, roughness and AO files, can convert a DirectX normal map and a smoothness map, and writes PNG. |

Each source's colour, displacement, OpenGL normal and roughness maps at one common resolution map directly onto this packing. The run copy of the project (`<build>/game/`) already keeps `.godot/` out of the repository.

## 6. Candidate assets

Poly Haven ids were read from `https://api.polyhaven.com/assets?t=textures&c=terrain` and `...?t=models&c=nature`; the page is `https://polyhaven.com/a/<id>`. ambientCG ids were read from its API (`https://ambientcg.com/api/v2/full_json`); the page is `https://ambientcg.com/a/<id>`. cgbookcase pages are `https://www.cgbookcase.com/textures/<slug>`. Names and tags were read; no texture was looked at, so suitability is a guess from the tags.

| Ground type | Candidates |
| --- | --- |
| Temperate forest | Poly Haven `forrest_ground_01` (grass, twigs, leaves), `forest_leaves_02` (moss, leaf litter), `leaves_forest_ground`, `forest_ground_04`; ambientCG `Ground037` (damp, earth, forest, grass), `ScatteredLeaves009`; cgbookcase `forest-ground-01`, `forest-floor-01` |
| Boreal forest | Poly Haven `forrest_ground_03` (pine needles), `forrest_sand_01` (sand, pine needles), `forest_ground_05`; ambientCG `Ground106` (earth, forest) |
| Grassland | ambientCG `Grass001` (dense, fresh), `Grass004` (dense); cgbookcase `grass-01`; Poly Haven `rocky_terrain_02` (grass, aerial) |
| Steppe | Poly Haven `grass_ground` (dry grass, dirt, turf), `dry_ground_rocks`, `gravelly_sand`; ambientCG `Ground109` (beige, dry) |
| Heath and moor | Poly Haven `aerial_grass_rock` (moss, grass), `brown_mud_leaves_01` (grass, mud, moss); cgbookcase `moss-01`. Weak coverage: no heather or peat scan found. |
| Wetland | Poly Haven `brown_mud_03` (wet soil, wet patches), `brown_mud_02`, `aerial_mud_1`; ambientCG `Ground054` (beach, mud, sand) |
| Desert | Poly Haven `dry_ground_01` (cracked, desert), `mud_cracked_dry_03`, `sand_01`, `sandstone_cracks` |
| Tundra | Poly Haven `snow_03`, `snow_04` (snow with mud), `lichen_rock`, `rocks_ground_05` |
| Alpine rock | Poly Haven `rock_face`, `rock_face_03`, `rock_08`, `gray_rocks`; cgbookcase `rock-face-01`, `grey-rock-01` |
| Snow | Poly Haven `snow_02` (smooth, powdery), `snow_01`, `snow_field_aerial`; ambientCG `Snow006`, `Snow015`; cgbookcase `snow-covered-path-01` |
| Beach | Poly Haven `aerial_beach_01`, `coast_sand_01`, `coast_sand_02`, `aerial_sand`; cgbookcase `beach-gravel-01` |
| River and lake beds | Poly Haven `ganges_river_pebbles`, `river_small_rocks`, `dry_riverbed_rock`, `mud_cracked_dry_riverbed_002`; cgbookcase `pebbles-01`, `pebbles-02` |

| Vegetation kind | Candidates |
| --- | --- |
| Broadleaf trees | Poly Haven `tree_small_02` (category Broadleaf Trees), `jacaranda_tree`, `island_tree_01` to `island_tree_03` (kind not checked); Quaternius Stylized Nature MegaKit (40 trees, free part only); Kenney Nature Kit |
| Conifers | Poly Haven `fir_tree_01`, `pine_tree_01`, `fir_sapling`, `fir_sapling_medium`, `pine_sapling_medium`, `pine_sapling_small` |
| Dead trees and wood | Poly Haven `dead_tree_trunk`, `dead_tree_trunk_02`, `tree_stump_01`, `tree_stump_02`, `root_cluster_01`, `dry_branches_medium_01` |
| Shrubs and ground cover | Poly Haven `shrub_01` to `shrub_04`, `shrub_sorrel_01`, `searsia_lucida`, `fern_02`, `grass_medium_01`, `grass_medium_02`, `moss_01`, `nettle_plant` |
| Reeds | None found in the lists read. |
| Boulders and rock | Poly Haven `boulder_01`, `stone_01`, `rock_moss_set_01`, `rock_moss_set_02`, `rock_07`, `rock_09`, `rock_face_01`, `rock_face_02` (models), `namaqualand_boulder_02` to `_06` (arid) |

Poly Haven's models are film-grade scans: polygon counts, missing LODs and the cost of alpha-tested foliage were not checked, and each will need decimation and LODs before it is game-ready **[unverified]**.

## 7. Not verified

- Every Nexus statement in section 3: all six mod pages refused the fetch. Not seen at all: the "Permissions and credits" fields of any mod, and so any contradiction with the description; the permission text of Enhanced Vanilla Landscapes and of Seasonal Landscapes - Unfrozen; the origin of the textures in WOODLAND and Cathedral Landscapes. That mod 144135 is the "Enhanced Vanilla Landscapes" the owner meant is an assumption (a LOD and grass mod, 167903, has a similar name).
- Bethesda's terms on game-derived assets, and the DDS and NIF conversion notes (general knowledge).
- The Git LFS overage prices ($0.07 and $0.0875 per GiB): from search results, not read on GitHub's calculator. Whether LFS uploads, forks or release archives count against bandwidth.
- ambientCG: the file names inside a zip (both normal conventions), and any promise that `get?file=` URLs stay stable. Poly Haven: model formats and polygon counts. Kenney: file formats in the zip. cgbookcase: its licence page (404; the sentence comes from the texture listing), maps, formats and normal convention. Quaternius: which other kits are CC0 and which QAL, the contents of the free part of the Stylized Nature MegaKit, and the tree kinds in either pack.
- ambientCG ids for rock, moss, sand and gravel were not listed (the API response was cut off); only the ids named in section 6 were seen.
- OpenGameArt was not examined.
- Whether `terrain3d.readthedocs.io/en/stable` describes v1.1 exactly.
- No texture or model was downloaded or viewed; the candidates are chosen by name and tag only.
