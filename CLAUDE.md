# Casso: Claude Code Instructions

Project guidelines, code style, EHM patterns, build rules, and current feature context are in:

**`.github/copilot-instructions.md`**

Read that file at the start of every session.

<!-- SPECKIT START -->
**Active spec: `specs/024-mockingboard-speech`** (IMPLEMENTED, GH #123) — the
Mockingboard's SSI-263 voice chip shipped on branch `024-mockingboard-speech`:
clean-room `Ssi263` core + formant synthesis, the A/C variant split
(`mockingboard` / `mockingboard-c`), C as the default for ][+, //e, //e
Enhanced (deployed via the embedded-config version bump — editing
Resources/*.json alone ships NOTHING), the VIA CA1/CB1 seam, the speech demo
disk, and Hardware-tab product naming. First light achieved audibly.

**024 SHIPPED in 1.19.0** (merged to master; GH #123 and #125 closed). The
phoneme ROM was read off the visual6502 die shot and **fully decoded**: six
significance-interleaved 4-bit fields (F1/F2/F3 filter codes, VA, FA, nasal)
plus closure/class/fricative/voiced flags, cross-validated against the
SC-01A decap (22/46 identical formant-code triplets, closure 46/46), whose
measured capacitor network now supplies the code-to-Hz mapping. Data,
method, plate, and comparison: `specs/024-mockingboard-speech/rom-extraction/`.
Audio clicks were fixed by the dedicated event-driven WASAPI render pump
(#125). Still open on the spec: T040/T060 (title regression + acceptance
sets — need acquired period software, local gates).

**Also active: `specs/020-disk-file-access`** (IMPLEMENTED, unmerged) --
disk file access for the
build loop: assembler binary output, DOS 3.3 / ProDOS file read+write, a
`disk` subcommand -- create, init, list, get, put, delete, boot, sectorread
and sectorwrite -- and boot configuration. Delivered on branch
`020-disk-file-access` and cut as **1.20.0** there, since master took 1.19.0
for the Mockingboard first. See
[`specs/020-disk-file-access/plan.md`](specs/020-disk-file-access/plan.md).

Four further specs are drafted but NOT started, each to be picked up in its
own session:

- `specs/021-disk-manager`: graphical disk manager, live editing of the mounted
  disk, inspection tools, and CLI parity. Depends on 020.
- `specs/022-disk-image-formats`: 2MG, nibble, compressed/archived images, extra
  filesystems, larger media. Depends on 020/021; large-media parts gated on
  GH #101 / #93.
- `specs/023-ca65-dialect`: ca65's absolute subset, split out of 019. Builds on
  019's dialect mechanism, which has shipped; full compatibility needs a linker
  (GH #58).
- `specs/025-game-compat-patcher` — runtime patch table over live guest RAM,
  defusing title protection checks that fail by design on later machines
  (Choplifter on Enhanced //e and //c, the Karateka //c VBL spin, and a path to
  the wider Broderbund/Gebelli catalog). Tracked by GH #94. Work continues on
  branch `025-game-compat-patcher`; next step is `/speckit-clarify` or
  `/speckit-plan`.

One more is not yet written: **per-slot card configuration**
(GH #124) — a Hardware-tab dropdown selecting any supported card for any slot,
with default slot assignments modeling period-typical install locations. It is
where users will pick between the Mockingboard A and C, but 024 does not depend
on it (024 selects its variant by machine configuration).

**Sequencing.** 021 needs 020's filesystem layer and 022 needs 020/021. 023's
gate was 019's dialect mechanism (023 SC-006 requires that adding ca65 change
nothing in that mechanism), and that shipped in 1.18.0; 023 can start any
time. 025 is independent of both the disk and assembler tracks and can start
any time; it builds on the unmerged `game-patch-table` proof-of-concept (see
its `research/parked-branch.md` for the gap analysis, including an unresolved
scan-cost question).

**019 and 020 both rewrote the command line, and 019 landed first.** Master was
merged into 020 and the reconciliation was decided in 019's favor on the point
that matters: **the grammar is table-driven**, a dialect's flags are data that
the parser walks and the help is generated from, and **assembling names its
dialect**, `CassoCli as65 <source>`, not a bare source file. 020's command-line
work rides on top of that: the `disk` subcommand and its grammar, as65's exit
codes, and the rejoining of a command line PowerShell cut in half. Both branches
had independently withdrawn `--cpu` in favor of as65's `-x`, so the collision
predicted here never materialized.

**020 is delivered and awaiting merge.** All of it: the assembler's output
formats, DOS 3.3 and ProDOS read and write, the `disk` subcommand with nine
commands, boot configuration, and a command line that matches as65's. Cut as
**1.20.0** on the branch. The runner was split into `DiskCommandResult`,
`DiskImageSession`, `DiskHelpPage` and the command runner itself, every
function leaves through one exit, and `Casso.exe` now parses its own command
line through the shared table-driven grammar in `CassoCore`.

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
every later one on the track as zeros, while returning `S_OK`, and
`DiskImage::Serialize` puts it on the emulator's flush path, so a guest that
leaves a track partly written can lose the rest of it on eject today. Separately,
`ProDosReader` and `ProDosFileWriter` already exist but are declared inside
`ProDosSkeleton.h`, so a survey by filename misses them; DOS 3.3 has no reader at
all. Both are written up in `specs/020-disk-file-access/research.md`.

Recent specs live under `specs/` (015 printer support, 016 Apple //c, 017
blank-disk creation, and 019 assembler dialects + Merlin are all complete and
shipped).
<!-- SPECKIT END -->
