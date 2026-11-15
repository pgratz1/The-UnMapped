# Vendored dependencies

Third-party code used by the build, pinned and committed. The build never downloads anything. Each library is wrapped in a CMake target in `extern/CMakeLists.txt`, built with the project's floating-point flags but with warnings suppressed, and exposed through `SYSTEM` include directories.

Adding or updating a dependency needs a reason and a license check (see `.claude/agents/build-engineer.md`). Record it here with its checksums. Updating SQLite changes the generator's output contract: it needs a proposals entry and new golden hashes.

| Name | Version | Target | License | Source |
| --- | --- | --- | --- | --- |
| SQLite amalgamation | 3.54.1 (`SQLITE_SOURCE_ID` 2026-10-16 14:45:20 555e07ef33f69a0957d24d8e555b702464a131d6f1607f4c3c1e4551e8e6ca76) | `unmapped::sqlite3` | Public domain | https://sqlite.org/2026/sqlite-amalgamation-3540100.zip |
| doctest | 2.5.3 | `unmapped::doctest` | MIT | https://github.com/doctest/doctest/releases/download/v2.5.3/doctest-v2.5.3.tar.gz |
| toml++ | 3.4.0 | `unmapped::tomlplusplus` | MIT | https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz (tag commit 30172438cee64926dc41fdd9c11fb3ba5b2ba9de) |
| stb_image_write | 1.18, stb commit 9600522de53973336fa033a289858a4354cbb04c | `unmapped::stb_image_write` | Public domain or MIT, at the user's choice | https://github.com/nothings/stb/archive/9600522de53973336fa033a289858a4354cbb04c.tar.gz |

## Checksums

Downloaded archives (SHA-256 unless noted):

| Archive | Checksum | Verified against |
| --- | --- | --- |
| `sqlite-amalgamation-3540100.zip` | SHA3-256 `92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30`; SHA-256 `0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9` | SHA3-256 published on https://sqlite.org/download.html: match |
| `doctest-v2.5.3.tar.gz` | `eb4a39729d091e43d4d593888bf4d145ee9a0f0e13f7404484b6f2fb60d5d50f` | SHA-256 digest of the GitHub release asset: match |
| `tomlplusplus-v3.4.0.tar.gz` (GitHub tag archive) | `8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155` | No published checksum; recorded at download |
| `stb-9600522de53973336fa033a289858a4354cbb04c.tar.gz` (GitHub commit archive) | `8546b1281cf61e4b5229e9eb05055d765f1b076f6ba03aab377df09a20fb613b` | No published checksum; pinned by commit SHA |

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

## Notes

- **SQLite.** Only `sqlite3.c`, `sqlite3.h` and `sqlite3ext.h` from the amalgamation are kept (the `shell.c` command-line shell is not). SQLite carries no license file; its public-domain dedication is at the top of `sqlite3.c`. Compile options are set in exactly one place, the block marked `SQLITE CONFIGURATION` in `extern/CMakeLists.txt`. Runtime settings such as page size and pragmas belong in `worldgen/src/db/`.
- **toml++.** The single-header `toml.hpp` from the release tag. 3.4.0 is the latest release. TOML floats are parsed to `double`; data that reaches the database should use integers (design doc section 4).
- **stb_image_write.** The implementation is compiled once in `stb/stb_image_write_impl.c`, a two-line project file. Include `<stb_image_write.h>` elsewhere without defining `STB_IMAGE_WRITE_IMPLEMENTATION`. Used for map image export, not in the `world.db` path.
- **doctest.** The single header `doctest/doctest.h`, included as `<doctest/doctest.h>`.
