# Casso: Claude Code Instructions

Project guidelines, code style, EHM patterns, build rules, and current feature context are in:

**`.github/copilot-instructions.md`**

Read that file at the start of every session.

<!-- SPECKIT START -->
**018-3d-desk-scene SHIPPED in 1.21.0** ("The one where the skeuomorphic theme
goes to 11"), merged to master 2026-08-30. The 3D desk scene replaced the
skeuomorphic theme's 2D chrome: four CAD-built devices at true dimensions,
per-pixel lighting, cast and contact shadows, and the picture on spherical-sag
glass with input inverse-projected back through the curvature. Artifacts:
`specs/018-3d-desk-scene/` (spec.md, plan.md, research.md, data-model.md,
contracts/, quickstart.md). Scripts need
`$env:SPECIFY_FEATURE = "018-3d-desk-scene"` since the branch name is
unnumbered.

Still open on 018: **movable drives** (the last unstarted piece), Disk II
realism tweaks, and //e monitor (A2M2010) refinement. Groundwork for movable
drives is already in place: `DeskSceneLayout::MakeDeviceWorld` builds each
device's world matrix and `driveTx[0]`/`driveTx[1]` place them side by side,
`DeskSceneHitTester` resolves per-drive rays with occlusion, `EmulatorShell`'s
bezel-tilt and compass drags are the gesture pattern to model a drag on, and
`GlobalUserPrefs::monitorTilt` shows how a per-device scene property persists.

**Parked:** GH #131, steady-state GPU cost that scales with window area rather
than picture area. Its first step is a measurement, not a design, and it
overlaps #100 -- plan them together rather than building invalidation twice.

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

**`specs/020-disk-file-access` SHIPPED in 1.20.0** (merged to master;
1.20.1 followed) -- disk file access for the build loop: assembler binary
output, DOS 3.3 / ProDOS file read+write, a `disk` subcommand -- create, init,
list, get, put, delete, boot, sectorread and sectorwrite -- and boot
configuration. See
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
  the wider Broderbund/Gebelli catalog). Tracked by GH #94. The spec is on
  master; branch `025-game-compat-patcher` carries no unique commits, so start
  fresh from master. Next step is `/speckit-clarify` or `/speckit-plan`.
- `specs/026-assembler-to-disk` — the assembler writes its object into a disk
  image rather than only to host files. Drafted on branch
  `026-assembler-to-disk`; next step is `/speckit-clarify`.
- `specs/027-nibble-images` — mount and write back `.nib` images, split out of
  022 the way 023 was split out of 019. Research notes are committed on branch
  `027-nibble-images`; the spec itself is not written yet. **Writing is the hard
  part, not reading**: the mount path is a write-back path
  (`FlushEntry` -> `DiskImage::Serialize` on eject, power cycle and reset), so
  load-only is impossible, and `NibblizationLayer` converts sectors rather than
  nibble bytes, so the loader is a new seam and not an adapter. Nothing in Casso
  reads `.nib` today; the extension filter now answers from the loader's own
  routing table, so adding the format there makes every surface offer it without
  a second list. Do not oversell it: `.nib` records whole bytes and loses
  self-sync information, which is what copy protection inspects, so the reason
  to do this is compatibility with existing `.nib` collections, not fidelity.

**Why 026 exists, since the analysis is not obvious from the code.** Merlin's
`DSK`, `TYP` and `SAV` all assume the assembler writes onto a ProDOS volume.
Casso's `ArtifactWriter` writes host files only and no assembler path touches a
disk image, so `TYP` — which sets a ProDOS file type — has nowhere to land, and
`DSK` is redirected to a host file instead. 1.20's disk file access looked like
it unblocked `TYP`, and did not: it shipped as a separate `disk` command, not as
an assembler output target. Two decisions in the spec are ones a naive
implementation gets wrong. **Only the object goes into the image** — listing,
symbols and debug info stay on the host, because that is where host tools and
any future in-emulator debugger read them while the program under test runs from
the image. And **a file type with no counterpart is refused by name, not
approximated**: ProDOS `SYS` has no DOS 3.3 equivalent, because the ProDOS
kernel boots by scanning the volume directory for a `SYS`-typed entry and
DOS 3.3 has no system-program concept at all. The load address also stops being
retyped — the assembler knows the origin, so it writes the aux type itself,
where `disk put --load` today lets the two disagree silently.

**026 closes three of the six Merlin refusals** (`TYP`, `SAV`, and `DSK`'s real
meaning), leaving `REL`/`ENT`/`EXT` on the linker (GH #112) and the second `XC`
on a 65816 core. `SAV` is NOT a linker problem and does not wait on #112: a
linker combines several partial objects into one output, where `SAV` writes
several complete independent ones. Three open questions are recorded in the
spec rather than assumed away — whether the image must already exist, what
`SAV` means with no image target, and whether the volume's startup program can
be set.

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

**Merge master into a long-lived branch EARLY, and expect renames.** Master
took two sweeping accessor renames in August 2026 -- VerbNoun across 278 files
(`HasHitBound`->`HitBound`, `JsonEqual`->`AreJsonEqual`, our `ARRAYSIZE`->
`std::size`) and Dxui getters across 156 (`DpiScaler::Px`->`ToPx`,
`Bounds`->`GetBounds`, `Visible`->`IsVisible`, `DxuiColor` helpers->`Compute*`).
The convention is codified in `docs/coding-standards-backlog.md`: getters take
`Get`/`Is`, theme colors are exempt. **Renames merge cleanly and then fail to
compile**, so textual conflicts badly understate the work -- taking these into
018 cost 13 conflicts and roughly 100 stale call sites the compiler had to
find one round at a time. `026-assembler-to-disk` and `027-nibble-images` are
both hundreds of commits behind and have not taken either sweep yet.

**019 and 020 both rewrote the command line, and 019 landed first.** Master was
merged into 020 and the reconciliation was decided in 019's favor on the point
that matters: **the grammar is table-driven**, a dialect's flags are data that
the parser walks and the help is generated from, and **assembling names its
dialect**, `CassoCli as65 <source>`, not a bare source file. 020's command-line
work rides on top of that: the `disk` subcommand and its grammar, as65's exit
codes, and the rejoining of a command line PowerShell cut in half. Both branches
had independently withdrawn `--cpu` in favor of as65's `-x`, so the collision
predicted here never materialized.

**020 shipped whole.** All of it: the assembler's output
formats, DOS 3.3 and ProDOS read and write, the `disk` subcommand with nine
commands, boot configuration, and a command line that matches as65's. The
runner was split into `DiskCommandResult`,
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
