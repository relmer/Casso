# Casso — Claude Code Instructions

Project guidelines, code style, EHM patterns, build rules, and current feature context are in:

**`.github/copilot-instructions.md`**

Read that file at the start of every session.

<!-- SPECKIT START -->
**Active spec: `specs/020-disk-file-access`** (Planned) — disk file access for the
build loop: assembler binary output, DOS 3.3 / ProDOS file read+write, a
`disk` subcommand, and boot configuration. Clarified and planned; see
[`specs/020-disk-file-access/plan.md`](specs/020-disk-file-access/plan.md).
Next step is `/speckit-tasks`.

Four further specs are drafted but NOT started, each to be picked up in its own
session:

- `specs/019-assembler-dialects` — the dialect mechanism plus Merlin. Seeded by
  GH #92. Independent of 020; see the CLI note below.
- `specs/021-disk-manager` — graphical disk manager, live editing of the mounted
  disk, inspection tools, and CLI parity. Depends on 020.
- `specs/022-disk-image-formats` — 2MG, nibble, compressed/archived images, extra
  filesystems, larger media. Depends on 020/021; large-media parts gated on
  GH #101 / #93.
- `specs/023-ca65-dialect` — ca65's absolute subset, split out of 019. Depends on
  019's dialect mechanism; full compatibility needs a linker (GH #58).

**Sequencing.** 019 and 020 ran in parallel and **both rewrote the command-line
surface**, so they no longer merge cleanly. The landing order is decided:

1. **019 merges to master first.**
2. **Then master merges into 020**, and the conflicts get resolved here.
3. **Then 020 merges back to master** as its own release.

**Expect real work at step 2, concentrated in the usage and command-line code.**
The two branches step on each other across `CommandLineParser`, `CommandLine`,
and the help text; `CommandLineHelp` exists only on 020. One collision is known
by name: 020 withdrew `--cpu` in favor of as65's `-x`, while 019 pins `--cpu` in
as65 mode with its own tests and refuses it **by name** on the Merlin path to
point the user at Merlin's `XC` directive. Those tests need retargeting at `-x`
and that refusal needs rewording, or its guidance degrades to a bare "unknown
option". 020 also carries a fix 019 will want and a bug 019 already fixed — the
"0 lines assembled" miscount, corrected on 019 by `11f43ff6`.

The earlier claim that moving the command-line surface into `CassoCore` would let
both extend it "without a blind merge" was optimistic: it made the code testable,
which is worth having, but 020 then restructured what it found there.

**The merge is surveyed.** `specs/020-disk-file-access/merge-019-prep.md` maps
what 019 did to the command line — it dismantled `CommandLine.cpp` into per-mode
modules, which 020 grew instead — and lists the five real collisions with a
suggested order of work. Read it before starting step 2.

The others are gated: 021 needs 020's filesystem layer, 022 needs 020/021, and
**023 needs 019's dialect mechanism** (023 SC-006 requires that adding ca65
change nothing in that mechanism), so 023 must not start before 019 lands.

**Versions are pre-assigned so `Version.h` is not a merge conflict.**
`handoff/disk-write-integrity` holds 1.17.0, 019 takes 1.18, and 020 is 1.19.0
with its release already cut in the changelog — merging 020 to master IS that
release.

**020 is partially delivered.** Its User Story 1 (assembler binary output) is
already done and on master: the unpadded span and `--dos-bin` live in
`CassoCore/OutputFormats` alongside `WriteFlatImage`, with tests. The unpadded
span is the default and has no flag of its own — `--raw` named it for one
revision and was retired by owner decision. A session picking up 020 should
implement US2 onward and treat US1 as complete.

**Two 020 findings that outlive the feature.** `NibblizationLayer::Denibblize`
stops at the first sector it cannot decode on a track and leaves that sector and
every later one on the track as zeros, while returning `S_OK` — and
`DiskImage::Serialize` puts it on the emulator's flush path, so a guest that
leaves a track partly written can lose the rest of it on eject today. Separately,
`ProDosReader` and `ProDosFileWriter` already exist but are declared inside
`ProDosSkeleton.h`, so a survey by filename misses them; DOS 3.3 has no reader at
all. Both are written up in `specs/020-disk-file-access/research.md`.

Recent specs live under `specs/` (015 printer support, 016 Apple //c, and 017
blank-disk creation are all complete and shipped).
<!-- SPECKIT END -->
