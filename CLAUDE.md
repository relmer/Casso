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

**`028-shared-disk-images` is COMPLETE on its branch and not merged**
(2026-09-01): an image changed outside the emulator is picked up by a running
one. All 116 tasks walked, including both real-build scenarios. Another session
owns it. Coordinate before touching `DiskImageStore`, `CommitPlan`,
`ChangePrompt` or `EmulatorShell`.

One sub-case is still owed and is worth knowing about: forcing the copy to fail
by denying new-file creation on the folder never reached the refusal dialog at
all. **Unverified hypothesis:** `IsHeldByAnotherProcess` opens the file to
answer, and an access-denied may read as "somebody else is writing", which is a
silent indefinite defer. Instrument that call before believing it.

027 shipped in 1.22.0 (merge `322de943`), which released the seven disk files it
had been holding.

**"Did it ship" is not "is the branch an ancestor of master".** A feature branch
usually gains commits AFTER its merge, so `git merge-base --is-ancestor
origin/<branch> origin/master` answers no for work that shipped. Ask whether the
merge commit or the code is on master instead.

**Open specs**, each to be picked up in its own session. Read the spec itself
for detail; this is an index, not a status report.

| Spec | Blocked by |
|---|---|
| 021 disk manager | nothing; 020 shipped |
| 022 disk image formats | 021; large media on GH #101 / #93 |
| 023 ca65 dialect | nothing; full compatibility needs a linker (GH #58) |
| 025 game compat patcher | nothing; builds on unmerged `game-patch-table` |
| 028 shared disk images | complete, awaiting merge; see above |
| per-slot card config (GH #124) | not yet written |

<!-- SPECKIT START -->
<!-- SPECKIT END -->
