# Vendored dependencies

Third-party code used by the build, pinned and committed. The build never downloads anything. Each library is wrapped in a CMake target in `extern/CMakeLists.txt`, built with the project's floating-point flags but with warnings suppressed, and exposed through `SYSTEM` include directories.

Adding or updating a dependency needs a reason and a license check (see `.claude/agents/build-engineer.md`). Record it here with its checksums. Updating SQLite changes the generator's output contract: it needs a proposals entry and new golden hashes.

| Name | Version | Target | License | Source |
| --- | --- | --- | --- | --- |
| SQLite amalgamation | 3.54.1 (`SQLITE_SOURCE_ID` 2026-10-16 14:45:20 555e07ef33f69a0957d24d8e555b702464a131d6f1607f4c3c1e4551e8e6ca76) | `unmapped::sqlite3` | Public domain | https://sqlite.org/2026/sqlite-amalgamation-3540100.zip |
| doctest | 2.5.3 | `unmapped::doctest` | MIT | https://github.com/doctest/doctest/releases/download/v2.5.3/doctest-v2.5.3.tar.gz |
| toml++ | 3.4.0 | `unmapped::tomlplusplus` | MIT | https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz (tag commit 30172438cee64926dc41fdd9c11fb3ba5b2ba9de) |
| stb_image_write | 1.18, stb commit 9600522de53973336fa033a289858a4354cbb04c | `unmapped::stb_image_write` | Public domain or MIT, at the user's choice | https://github.com/nothings/stb/archive/9600522de53973336fa033a289858a4354cbb04c.tar.gz |
| godot-cpp | 10.0.0-stable (tag commit 507ed9d840c01a3c5b2a39af8bb4000bfac30bf5), API 4.7 | `unmapped::godot-cpp`, only with `-DUNMAPPED_BUILD_GDEXT=ON` | MIT | https://github.com/godotengine/godot-cpp/archive/refs/tags/10.0.0-stable.tar.gz |
| Terrain3D | v1.1.0-stable (tag object f1034f45a4fe2435404d2c8ad5c3c8e5e087ecb8, commit 629bbd65526bbdc3d1aa371840c7028055c30b02) | `unmapped::terrain3d`, only with `-DUNMAPPED_BUILD_TERRAIN3D=ON` (not yet built, see Notes) | MIT | https://github.com/TokisanGames/Terrain3D/archive/refs/tags/v1.1.0-stable.tar.gz |

## Checksums

Downloaded archives (SHA-256 unless noted):

| Archive | Checksum | Verified against |
| --- | --- | --- |
| `sqlite-amalgamation-3540100.zip` | SHA3-256 `92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30`; SHA-256 `0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9` | SHA3-256 published on https://sqlite.org/download.html: match |
| `doctest-v2.5.3.tar.gz` | `eb4a39729d091e43d4d593888bf4d145ee9a0f0e13f7404484b6f2fb60d5d50f` | SHA-256 digest of the GitHub release asset: match |
| `tomlplusplus-v3.4.0.tar.gz` (GitHub tag archive) | `8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155` | No published checksum; recorded at download |
| `stb-9600522de53973336fa033a289858a4354cbb04c.tar.gz` (GitHub commit archive) | `8546b1281cf61e4b5229e9eb05055d765f1b076f6ba03aab377df09a20fb613b` | No published checksum; pinned by commit SHA |
| `godot-cpp-10.0.0-stable.tar.gz` (GitHub tag archive) | `ebbf64f0f8f8b8b66bc4bc4d3797db97f95b19cf0dfb5b3f8cf56683d9df7db4` | No published checksum; recorded at download. `gdextension/extension_api-4-7.json` checked against the pinned Godot 4.7.2 editor, see Notes |
| `Terrain3D-1.1.0-stable.tar.gz` (GitHub tag archive, `v1.1.0-stable.tar.gz`) | `1aa251b80b072613173e964a2353fe7a585a49510161a4251db3aeb4ceba5527` | No published checksum; recorded at download (2026-11-27) |

Vendored files (SHA-256), to check that nothing here was edited:

```
d6c84557261893439b341d2e6c35187ae3ea24fbfd843ad94616f72d89242b8c  sqlite/sqlite3.h
0c657666d819f261cc4007526ad57e1669de9949df7a8ea847aa284a2bf0ee52  sqlite/sqlite3.c
48963abfefb41ac329ef9d706acb5eadba952a99a748285c9ded6b5e0942721b  sqlite/sqlite3ext.h
cfd518a3ef90f67e1f3ba514df23fb3627437de1a2feeba78cf5062a40021421  doctest/doctest/doctest.h
0fe0b331fa1513dcce8604ff1fa925f32d1cea17d8aeb1c2471fad40d291adc5  doctest/LICENSE.txt
6b5172ad4dd6519aec67b919181fa7a38a2234131e5b2afa232dfe444819783e  tomlplusplus/toml.hpp
529bc3900a9571e49db285b0df432397e70b881cc3bf48de6667ae74ff4b06d8  tomlplusplus/LICENSE
a8e7f77f0ccfdc9bac09a71268b131931ff81aafca82c393aeb6aaba10d9e7ca  stb/stb_image_write.h
eb36e45c704dd0b59730115523e089b051852625b4db0a6fe76c991900af20e7  stb/LICENSE
```

Check with `cd extern && sha256sum -c` on the block above.

godot-cpp has 125 files; they are checked as one tree hash and by their key files:

```
26a5b210d90760156ce886267ce9df235787dcc6cebfd7ecd95ef5b6fdcc95bf  godot-cpp/LICENSE.md
53d37f85be32b6d10fb2266ca51f6ef0c3a55728acdb7c8301b1458a93c00943  godot-cpp/gdextension/extension_api-4-7.json
7d8c0a039d9743eb8ebf88681ae0c641d8d3aa5ffca11081745a84da803e09a1  godot-cpp/gdextension/gdextension_interface.json
```

Tree hash `ddfddce5c5c9ec4b2f9d13cc40a2e8c1b7ccdb0de3579682c25cd0c1d1896688`, from `cd extern && find godot-cpp -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum | sha256sum`. Every vendored file is byte-identical to the same path in the tag archive.

Terrain3D has 52 files; they are checked as one tree hash and by their key files:

```
f7ecc09ce65c97828130728072c5a6ed3cc185d3bf0436e51a15828ad9ceca7c  terrain3d/LICENSE.txt
8db6fb43bd28aa266a1632c5cfc91f076f01b02435eec22d0accf30301a847bd  terrain3d/src/terrain_3d.h
b5f881ef19220b07650161a0b7411175d46347bc81261ea5f6d62988818ac34a  terrain3d/src/register_types.cpp
d76ff1849c48ecb58bf271442b6833530c4e872996653176e32383ca37261ab4  terrain3d/project/addons/terrain_3d/terrain.gdextension
```

Tree hash `3559acd5f1b8ef30ad5a61c621cb8d450011005d9a2021342d249a9c6e53ac10`, from `cd extern && find terrain3d -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum | sha256sum`. Every vendored file is byte-identical to the same path in the tag archive.

## Notes

- **SQLite.** Only `sqlite3.c`, `sqlite3.h` and `sqlite3ext.h` from the amalgamation are kept (the `shell.c` command-line shell is not). SQLite carries no license file; its public-domain dedication is at the top of `sqlite3.c`. Compile options are set in exactly one place, the block marked `SQLITE CONFIGURATION` in `extern/CMakeLists.txt`. Runtime settings such as page size and pragmas belong in `worldgen/src/db/`.
- **toml++.** The single-header `toml.hpp` from the release tag. 3.4.0 is the latest release. TOML floats are parsed to `double`; data that reaches the database should use integers (design doc section 4).
- **stb_image_write.** The implementation is compiled once in `stb/stb_image_write_impl.c`, a two-line project file. Include `<stb_image_write.h>` elsewhere without defining `STB_IMAGE_WRITE_IMPLEMENTATION`. Used for map image export, not in the `world.db` path.
- **doctest.** The single header `doctest/doctest.h`, included as `<doctest/doctest.h>`.
- **godot-cpp.** C++ bindings for Godot's GDExtension API, used only by the extension in `gdext/` and built only with `-DUNMAPPED_BUILD_GDEXT=ON` (worldgen and worldgen-cli never see it). From version 10, godot-cpp is versioned separately from Godot and targets Godot 4.3 or later through an API version; the project builds it for API 4.7, the API of the pinned engine (Godot 4.7.2, see `gdext/README.md`). Kept from the tag archive: `CMakeLists.txt`, `cmake/`, the three Python generator scripts (`binding_generator.py`, `make_interface_header.py`, `build_profile.py`) and `doc_source_generator.py`, `include/`, `src/`, `natvis/`, `LICENSE.md`, `README.md`, and in `gdextension/` only `extension_api-4-7.json`, `gdextension_interface.json` and `README.md`. Removed: the API files for 4.3 to 4.6 (26 MB), `test/`, `misc/`, `tools/` and `SConstruct` (the SCons build), and the repository dotfiles. Nothing was edited. `extension_api-4-7.json` was compared with `Godot_v4.7.2-stable_linux.x86_64 --headless --dump-extension-api`: every section is identical except `header` (version_patch 0 against 2), and `gdextension_interface.json` is byte-identical to the editor's `--dump-gdextension-interface-json`. The bindings (about 2,100 files) are generated at configure and build time by Python 3, into the build tree, from these files; the build needs Python 3.8 or newer and never the network. Configuration is set in one place, the block marked `GODOT-CPP CONFIGURATION` in `extern/CMakeLists.txt`. Size in the repository: 8.7 MB, of which 7.0 MB is `extension_api-4-7.json`.
- **Terrain3D.** The terrain plug-in of the 3D terrain preview (spec `docs/specs/v3d-01-terrain-preview.md`, section 3), a GDExtension of its own. Kept from the tag archive: `src/` (C++ sources and headers, and the shader sources in `src/shaders/`, which the C++ includes as raw string literals), `LICENSE.txt`, `README.md`, and `project/addons/terrain_3d/terrain.gdextension` (reference only: entry symbol `terrain_3d_init`). The C++ loads one optional file by a `res://` path, the ocean material `res://addons/terrain_3d/extras/shaders/M_ocean.tres`, and only when its ocean is enabled, which the preview never does; it is not vendored. Removed: `SConstruct`, the Visual Studio project files, `doc/` (the class reference that SCons embeds in debug builds), `.github/`, the demo project, the editor plug-in (`plugin.cfg` and every `.gd` file, hard rule 3), tools, icons, extras, and the empty `godot-cpp` submodule folder (upstream pins a godot-cpp master commit of 2026-03 there). Nothing was edited. It is to be compiled against the project's godot-cpp 10.0.0-stable (API 4.7). **It does not compile unchanged against godot-cpp 10.0.0** (checked 2026-11-27 with GCC, `linux-gcc-release`): 16 errors in 9 files, all from godot-cpp API changes (`Math_PI` and `Math_TAU` macros gone, `is_power_of_2` and `closest_power_of_2` now only in `godot::Math` and overloaded, `Vector2i` to `String` conversion now explicit). How to proceed is Paul's choice (spec Q-V3D-01-1), so no build target exists yet. Size in the repository: 784 kB.
