# Design proposals

`design.md` in this folder is read-only: it is exported from a working document and overwritten on each update. Record proposed changes, open questions and discovered problems here. Paul carries them back to the design conversation, and accepted items appear in the next export of `design.md`.

## How to add an entry

Add entries at the bottom, newest last, using this shape:

```
### <short title>

- Date:
- Raised by: <agent role or person>
- Design doc section:
- Problem: what is wrong, unclear or missing
- Proposal: the suggested change
- Interim choice: what was done in the meantime, if anything
- Status: open
```

Status values: `open`, `accepted`, `declined`, `superseded`.

## Entries

### License decided: MIT

- Date: 2026-11-15
- Raised by: Paul, with the lead session
- Design doc section: 10 (tech stack table, "License: To be decided") and 12 (open question "License and distribution")
- Problem: the license was undecided, and the repository is now public.
- Proposal: record the project as open source under the MIT license, with no plan to sell it. Mark the open question as decided. Note for milestone 9: the newer Piper and espeak-ng are GPL-3. Linking them is allowed, and it makes any distributed build that includes them GPL-3 as a whole while the project's own source stays MIT. Running the voice engine as a separate process avoids that.
- Interim choice: `LICENSE` (MIT) added at the repository root.
- Status: open

### Build output outside the source tree

- Date: 2026-11-15
- Raised by: Paul, with the lead session
- Design doc section: 10 (build and CI)
- Problem: the working checkout lives in a Dropbox folder, so build trees and generated worlds written into the repository would be synced.
- Proposal: state in section 10 that all generated files go to a build root outside the tree (`$HOME/.cache/the-unmapped/` by default, overridable with `UNMAPPED_BUILD_ROOT`), and that in-source builds are refused.
- Interim choice: the rule is recorded in the "Build and test" section of `CLAUDE.md`, with a `.gitignore` as a backstop.
- Status: open
