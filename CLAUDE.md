# Casso: Claude Code Instructions

Project guidelines, code style, EHM patterns, build rules and testing are in:

**`.github/copilot-instructions.md`**

Read that file at the start of every session.

This file carries only what the tree cannot tell you: hazards that cost a
session when hit cold, corrections to conclusions a careful reading would
otherwise reach, and who is working where right now. Shipped-feature history
belongs in `CHANGELOG.md` and `git log`; design rationale belongs in the spec it
came from. If something here is derivable from the code, delete it.

Everything below `<!-- SPECKIT START -->` belongs to Spec Kit's `agent-context`
extension. Do not hand-write inside those markers; add to this section instead.

## Hazards

**Merge master into a long-lived branch EARLY, and expect renames.** Master has
taken four sweeping accessor renames. August 2026: VerbNoun across 278 files and
Dxui getters across 156. 2026-08-30: 279 functions across `CassoEmuCore`,
`CassoCore`, `Casso` and `CassoCli`, then 249 across Dxui and its consumers,
every noun-first name taking a verb (`ExtensionFor` -> `GetPrimaryExtension`,
`Visible` -> `IsVisible`). **Renames merge cleanly and then fail to compile**, so
textual conflicts badly understate the work: taking two of them into 018 cost 13
conflicts and roughly 100 stale call sites the compiler surfaced one round at a
time. The rule, the derivation method, and the traps that cost real time --
Windows SDK macro collisions, continuation lines left silently crooked -- are in
`docs/coding-standards-backlog.md` items 6 and 7.

**Speckit scripts need `$env:SPECIFY_FEATURE` when the branch name is
unnumbered.** `.specify/scripts/powershell/check-prerequisites.ps1` rejects any
branch that does not start with three digits.

**`ProDosReader` and `ProDosFileWriter` are declared inside `ProDosSkeleton.h`,**
so a survey by filename misses them. DOS 3.3 has no reader at all.

**The pre-push hook cannot catch an orphaned `////` banner, so run
`scripts/CheckStyle.ps1 -Mode Tree` before every master merge.** The hook calls
CheckStyle diff-scoped -- `-Against <base> -Revision <sha>`, not `-Mode Tree` --
so it reports only on lines added in the pushed range. A banner is orphaned by
editing its *surroundings*, and the signature it belongs to is never one of
those lines -- so the violation is invisible to the hook and fails
the tree sweep that CI runs on every master push. Both ways this has happened
were ordinary edits: a new function inserted immediately after an existing one
took over its banner, and a function that shared a banner with its neighbour
was separated from it when the neighbour's body grew. 2026-09-05, merge
`63642607`, two CS0014s in `CommandToolbar.cpp`; master was red for everyone
until `d9a9d6f1`, and the session that merged on top of it inherited a failing
style job. The companion rule for the insertion case -- splice ahead of the
`////` banner, never ahead of the signature -- is real but only covers the
author who knows they are splicing.

## Corrections

**The unpadded span is AS65's behavior; the padded 64 KB image is the
departure.** Several places in the tree claimed the reverse. AS65's manual
describes its binary as beginning at the lowest used address and continuing to
the highest, and its `testincl.bin` is 21 bytes. `docs/Assembler.md`,
`CommandLineOptions.h` and `CommandLineParser.cpp` were corrected; historical
CHANGELOG entries were left as written. Check
https://github.com/Ludoclt/as65_142 rather than this tree's prose before
changing an as65 default. There is no flag for the span: `--raw` existed for one
revision and was retired, because a flag whose only effect is to select the
default buys no capability.

**1.20's disk file access did not unblock Merlin's `TYP`.** It shipped as a
separate `disk` command, not as an assembler output target, so no assembler path
touched a disk image. 026 closed that gap (merge `0afa7359`).

## Who is where

**Concurrency only: which files another session is holding right now.** Not what
a spec contains, not how far along it is, and not whether it merged --
`specs/<name>/tasks.md` carries the first two and `git log` carries the third.
An index of specs lived here and went stale three times in one session: it still
called 027 unmerged after 1.22.0 had shipped it, which sent a session off to
merge work already on master. A list kept true by hand is a list that will lie.

**"Did it ship" is not "is the branch an ancestor of master".** A feature branch
usually gains commits AFTER its merge, so `git merge-base --is-ancestor
origin/<branch> origin/master` answers no for work that shipped. Ask whether the
merge commit or the code is on master instead.

<!-- SPECKIT START -->
## Active feature

**030-screenshot-capture** -- screenshot capture modes, file output, and metadata.
Closes GH #132.

- Plan: `specs/030-screenshot-capture/plan.md`
- Spec: `specs/030-screenshot-capture/spec.md`
- Research: `specs/030-screenshot-capture/research.md`
- Data model: `specs/030-screenshot-capture/data-model.md`
- Metadata contract: `specs/030-screenshot-capture/contracts/screenshot-metadata.md`
- Validation: `specs/030-screenshot-capture/quickstart.md`
<!-- SPECKIT END -->
