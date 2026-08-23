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

Three further specs are drafted but NOT started, each to be picked up in its
own session:

- `specs/021-disk-manager` — graphical disk manager, live editing of the mounted
  disk, inspection tools, and CLI parity. Depends on 020.
- `specs/022-disk-image-formats` — 2MG, nibble, compressed/archived images, extra
  filesystems, larger media. Depends on 020/021; large-media parts gated on
  GH #101 / #93.
- `specs/023-ca65-dialect` — ca65's absolute subset, split out of 019. Builds on
  019's dialect mechanism, which has shipped; full compatibility needs a linker
  (GH #58).

**Sequencing.** 021 needs 020's filesystem layer and 022 needs 020/021. 023's
gate was 019's dialect mechanism (023 SC-006 requires that adding ca65 change
nothing in that mechanism), and that shipped in 1.18.0 — 023 can start any
time.

**019 and 020 both rewrote the command line, and 019 landed first.** Master was
merged into 020 and the reconciliation was decided in 019's favor on the point
that matters: **the grammar is table-driven**, a dialect's flags are data that
the parser walks and the help is generated from, and **assembling names its
dialect** — `CassoCli as65 <source>`, not a bare source file. 020's command-line
work rides on top of that: the `disk` subcommand and its grammar, as65's exit
codes, and the rejoining of a command line PowerShell cut in half. Both branches
had independently withdrawn `--cpu` in favor of as65's `-x`, so the collision
predicted here never materialized.

**020 is partially delivered.** Its User Story 1 (assembler binary output) is
done: `WriteRaw`, `WriteDosBinary` and `WriteFlatImage` all live in
`CassoCore/OutputFormats`, with tests, and those writers plus the
`OutputFormat` enum are on master. **What is branch-only is which one is the
default.** Making the unpadded span the default is commit `3a99fc31`, on
020 and not on master, so master still writes the padded 64 KB image when no
format flag is given. A session picking up 020 should implement US2 onward and
treat US1 as complete.

**The unpadded span has no flag of its own.** `--raw` named it for one revision
and was retired by owner decision: a flag whose only effect is to select the
default earns a line in the help and buys no capability. Several places in the
tree still claimed that the padded 64 KB image is what AS65 writes and that the
span was our own modern addition. That is backwards. AS65's own manual says its
binary "begins at the lowest used address, and continues up to the highest used
address", and its `testincl.bin` is 21 bytes, so the unpadded span **is** AS65's
behavior and the padded image is the departure from it. The claims in
`docs/Assembler.md`, `CommandLineOptions.h` and `CommandLineParser.cpp` were
corrected; historical `CHANGELOG` entries were left as written. Check
https://github.com/Ludoclt/as65_142 rather than this tree's prose before
changing an as65 default again.

**Two 020 findings that outlive the feature.** `NibblizationLayer::Denibblize`
stops at the first sector it cannot decode on a track and leaves that sector and
every later one on the track as zeros, while returning `S_OK` — and
`DiskImage::Serialize` puts it on the emulator's flush path, so a guest that
leaves a track partly written can lose the rest of it on eject today. Separately,
`ProDosReader` and `ProDosFileWriter` already exist but are declared inside
`ProDosSkeleton.h`, so a survey by filename misses them; DOS 3.3 has no reader at
all. Both are written up in `specs/020-disk-file-access/research.md`.

Recent specs live under `specs/` (015 printer support, 016 Apple //c, 017
blank-disk creation, and 019 assembler dialects + Merlin are all complete and
shipped).
<!-- SPECKIT END -->
