# Changelog

All notable changes to Casso are documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/).
Versioned entries use `MAJOR.MINOR.PATCH` from [Version.h](CassoCore/Version.h).
Entries before versioning was introduced use dates only.

## [Unreleased]

### Added
- **The assembler writes its object into a disk image.** `--disk <image>` sends
  the object onto a volume instead of to a host file, with `--as` naming it
  there, `--type` giving it a filesystem type and `--startup` making it the
  program the volume runs at boot. The documented build loop drops from six
  commands to three.
- **The load address comes from the source's origin.** There is no `--load` on
  the assembler and there must not be: placing an assembled file used to mean
  restating the origin the source already declared, with nothing checking the
  two agreed, so a source whose origin moved produced a file the guest loaded
  at the wrong address.
- **Merlin's `TYP` is implemented.** It sets the filesystem type the output
  takes, stated as a ProDOS type byte. The accepted set is Merlin's own —
  `$00`, `$06`, `$F0` through `$F7` and `$FF` — plus `$04` text and `$FC`
  Applesoft, which Merlin lists in neither direction. Text, binary and Applesoft
  map to both filesystems; the rest are refused on DOS 3.3 by name rather than
  approximated, because DOS 3.3 has five types and none of them means a system
  program, a command file, or no type at all. A byte outside the set is refused
  naming the byte, as Merlin's own `ILLEGAL FILE TYPE` does.
- **Merlin's `SAV` is implemented, and one assembly can produce several
  files.** It writes the span accumulated since the previous save and carries
  on, with the accumulation emptied, so no byte appears in two outputs and each
  records the address its own first byte assembles to. It works with or without
  a disk: without one it writes host files.
- **A second `DSK` closes the file the first opened and begins another**, so a
  source carrying two produces two files with no `SAV` anywhere. It used to
  keep only the last name, which is indistinguishable from Merlin for one
  occurrence and wrong for two.

  The Merlin subset boundary falls from six refused constructs to four. The
  remaining ones are `REL`, `ENT` and `EXT`, which need the relocating linker,
  and a second `XC`, which needs a 65816 core.
- **An assembly producing several outputs produces a listing for each**, named
  after the object it describes, holding that object's code and the equates
  above it. One listing spanning every output made a reader looking for one
  program walk past the others.

### Fixed
- **Two Casso instances no longer write into each other's temporary file.**
  The commit temporary was derived from the image path alone, so two emulators
  sharing an image could each rename the other's bytes over the target. Data
  loss.
- **Casso no longer writes an image back over a change it never saw.** It
  recorded nothing about a mounted file and re-checked nothing before writing,
  so an external edit was overwritten silently. It now keeps the displaced
  version.

### Changed
- **`CassoCli run` now requires `--as65` or `--merlin` for a source file.** It
  used to assume as65. Binaries are unaffected.
- **Merlin's `-l` takes no filename and writes files rather than standard
  output.** One name cannot serve a source that saves itself twice, so listings
  are named after the objects instead. A filename supplied anyway is refused by
  name. `as65 -l` is unchanged: it keeps its filename and its standard-output
  default, and an as65 source has no way to produce a second output.
- **`as65 -x` now performs AS65's `JMP`-to-`BRA` optimization, and `NOOPT` /
  `-n` switch it off.** It was the one optimization Casso did not do, so a
  backward, in-range `JMP` now assembles to two bytes instead of three and
  every label below it moves. `OPT`, `NOOPT` and `-n` are now implemented
  rather than ignored.

## [1.22.0]: The one with nibble support

### Added
- **Nibble disk images (`.nib`, `.nb2`) are supported.**

### Changed
- **Faster disk decoding**: ~2x on formatted tracks and ~100x on unformatted
  ones, for every image format.

## [1.21.0]: The one where the skeuomorphic theme goes to 11

### Added

- Tilt the Monitor II by the marks on its bezel. Remembered per monitor.
- Drag the scene to rotate it.
- Corner compass: arrows rotate (hold to repeat), drag to turn, orb resets.
  Shift+two-finger drag on touchpad. Ctrl-0 to reset.
- The //c comes up green, from the Monitor //c it shipped with. Still
  overridable per machine.
- Restore window placement on launch, maximized included.
- Settings > Theme: scene antialiasing (off / 2x / 4x). Preview shows the
  real scene.
- Drive 2 on any machine with a Disk ][ interface; drive count comes from
  the card's ports.
- A real-time 3D desk scene replaces the skeuomorphic theme's 2D chrome.
  The picture maps onto spherical-sag glass, and input is inverse-projected
  through the curvature so a click lands on the right emulated pixel.
- Four new device models (Monitor II, Monitor //c, Disk II, Disk IIc),
  CAD-built from photographs at true dimensions. Openings are boolean cuts
  and edges are real fillets.
- Each machine gets the hardware it shipped with: a Monitor II over Disk IIs
  for the //e and ][+, a Monitor //c over Disk IIcs for the //c. Switching
  machines swaps the stack.
- Case marks are modeled, not painted: embossed bezel icons, inlaid
  cassowary, drive numbers, IN USE, the disk ][ logotype.
- Drive doors animate on mount and eject. The Disk II swings on its
  cantilever; the Disk IIc slides back and lifts.
- Everything the 2D drive band did carries over: activity lights, write-
  protect padlock and tooltips, drag-and-drop, slot click ejects and
  browses, body click browses.
- Alt+Enter fills the monitor with the picture alone; the drives live in a
  slide-up strip revealed by pointer dwell or Ctrl+D.
- Settings > Theme can drop the CRT monitor and put the picture back on a
  flat rect. The 3D drives stay either way.
- Compact themes (DarkModern, RetroTerminal) keep their 2D widgets.
- Per-pixel shading from two lights plus a specular highlight, with the
  power lamp as a real light in the same pass.
- Shadows are cast across the desk, and each device has a contact shadow
  under it.
- Lamps are lit lenses in a recess rather than painted marks.

- **The Harte vectors now check instruction timing, not just results.** The
  packed fixtures had discarded the per-cycle trace, so no depth of vectors
  could catch a timing error. It found three timing bugs on its first run.
- **The Harte vectors now cover the undocumented opcodes.** 77 of the 79
  illegal NMOS opcodes had no vectors at all, so their tests loaded nothing
  and passed. All 79 now run at full depth and came out clean.
- **An offline cycle reference at `docs/cycle-reference.md`.** Every opcode's
  mnemonic, addressing mode, length, and base cycle count for the 6502 and
  65C02, generated from the emulator's own instruction tables.

### Changed

- Monitor II rear rebuilt as one dark molding, plus contrast wheel, power
  notch and rounded edges.
- Monitor //c rear panel modeled control for control; tube runs through the
  bezel opening.
- The //c ships a Disk IIc, not a rebadged Disk II.
- Fullscreen no longer resizes the window; its toolbar can be summoned.
- internal: builds and checks run at BelowNormal (`-NormalPriority` opts
  out), with MSBuild node reuse off so the priority reaches the compiler.

- **Rebuilt the casso-rocks demo** with separate sets of DHGR and HGR images,
  dithered specifically for mono and for color displays, radically improving
  clarity on mono. Replaced the color bars with a spiffy Beagle Bros. HGR
  kaleidoscopesque pattern.\*

  \* *Avoid staring at the kaleidoscope for extended periods. Not responsible
  for self-hypnosis.*

- **`CassoCli run` now requires `--as65` or `--merlin` for a source file.** It
  used to assume as65. Binaries are unaffected.
- **`as65 -x` now performs AS65's `JMP`-to-`BRA` optimization; `NOOPT` and `-n`
  disable it.** A backward, in-range `JMP` assembles to two bytes instead of
  three, so every label below it moves.

### Added
- **The assembler states what its write should do.** `as65` and `merlin` take
  `--on-change reload|restart` beside `--disk`, so a build that assembles
  straight onto a mounted image tells the running emulator what to do with it
  and the loop is one command. It changes no assembled byte.
- **Casso picks up a disk image changed outside it.** A build that
  writes onto a mounted image reaches the running guest without an eject and
  re-insert. `CassoCli disk put --on-change reload|restart` says what the write
  should do; without it Casso asks. A pick-up is a disk swap and cannot be made
  safe -- the guest holds the disk's directory in its own memory -- so every
  notice says why a reboot may be needed.
- **Neither version is discarded when a disk changes on both sides.** The
  external version is mounted and Casso's is saved beside it, timestamped. If
  that copy cannot be written, nothing is mounted and nothing is overwritten.
- **A disk whose file is deleted or becomes unreadable offers to save what is
  still in memory**, then empties the drive rather than reporting a disk that is
  no longer there.

- **The Harte vectors now check instruction timing, not just results.** The
  packed fixtures had discarded the per-cycle trace, so no depth of vectors
  could catch a timing error. It found three timing bugs on its first run.
- **The Harte vectors now cover the undocumented opcodes.** 77 of the 79
  illegal NMOS opcodes had no vectors at all, so their tests loaded nothing
  and passed. All 79 now run at full depth and came out clean.
- **An offline cycle reference at `docs/cycle-reference.md`.** Every opcode's
  mnemonic, addressing mode, length, and base cycle count for the 6502 and
  65C02, generated from the emulator's own instruction tables.

### Removed
- **`test-bands.hgr` and `lores-bars.lores`** -- both lived only on the demo
  disk, and `scripts/HgrPreprocess.py` still generates either on demand.

### Fixed

- Clicks no longer pass through the monitor to a drive behind it; back faces
  are not clickable.
- The drive door's click target follows the door.
- Switching machines no longer leaves the previous monitor's bezel behind.
- Command toolbar clicks are no longer swallowed by the scene.
- Reduce GPU use when idle.
- Losing the audio endpoint (undocking, switching the default output
  device) no longer trips an assertion; audio reopens the new default.
- Eject and mount take effect while the machine is paused. The CPU thread
  now services the command queue without stepping the emulation.

- **The casso-rocks demo signs off instead of just vanishing** -- a thank-you
  line that stays on screen with the BASIC prompt under it. It also wipes the
  monitor question the moment it is answered, so a reset partway through no
  longer lands on a screen that still looks like a prompt and no longer
  answers.
- **The demo stops the drive once the last track is read** instead of leaving
  the motor spinning for as long as it is up, and ESC out of the cycle no
  longer leaves the key in the latch for Applesoft to swallow.
- **`run` echoed flags back with a dash even on a `/`-style command line.**
- **`-c` listings omitted the cycle count for 59 65C02 opcodes, and printed the
  NMOS value for `JMP (abs)`.** Counts now come off the instruction itself, the
  same value the emulator bills. NMOS counts are unchanged.
- **Instruction timing was wrong in three places. This changes emulated timing.**
  - `ASL`, `LSR`, `ROL`, `ROR` in `abs,X` were billed seven; they take six, or
    seven across a page. 65C02 only.
  - A branch with a zero displacement was billed as untaken; it is taken, and
    costs the extra cycle. 6502 and 65C02, all conditional branches and `BRA`.
  - `BBRn` and `BBSn` were billed a flat five; they take five, six when taken,
    seven when taken across a page. 65C02 only.
- **Casso now reports a disk image that fails to mount, and why.** Previously
  the machine came up at a bare text screen with no message. The `disk`
  subcommand gives the same reasons.
- **A disk that never mounted no longer appears in the recent-disks list.**
- **`.do` images could not be created.** The command line refused them and the
  create dialog did not offer them. Both write a DOS-ordered image now, byte
  for byte the same as `.dsk`.

## [1.20.1]: The one that gets physical--and logical--and liberates more silicon secrets

### Fixed
- **`disk sectorread` and `sectorwrite` wrote to physical sector rather than logical.**
  Both routed the sector number through an incorrectly named physical-to-logical map
  so they actually treated sectors as physical rather than logical as stated in help.
  Sector numbers are now correctly mapped and can be specified in either physical or
  logical numbers.

### Changed
- **`disk sectorread` and `disk sectorwrite` require `--logical` or
  `--physical`.** Sector numbers can be specified two ways, logical (what
  catalogs, DOS tools and reference books use), and physical (the
  address-field order a boot loader reads off the drive). The commands
  now specify which to use.
- **The voice's frequency scale is now silicon-derived too.** Cross-checking
  our SSI 263A ROM extraction against the 2007 SC-01A decap (the data every
  other emulator substitutes) showed the two chips share one parameter code
  scale — 22 of 46 name-matched phonemes carry identical formant codes, the
  closure flag agrees 46/46 — which validates the extraction end-to-end and
  licenses the SC-01's measured capacitor network as the code-to-Hz mapping.
  The phoneme table now derives its frequencies from that network instead of
  literature-fitted lines (AH1's F1 code lands on 731 Hz; the textbook value
  for "father" is 730). Comparison data and stats:
  `specs/024-mockingboard-speech/rom-extraction/decoded-data.md`.

### Added
- **`disk blockread` and `disk blockwrite` read and write 512-byte ProDOS
  blocks.** Block numbers run 0 to 279 on a 5.25-inch image. The disk
  doesn't need ProDOS on it and any container works, not just `.po`.

## [1.20.0]: The one where CassoCLI gets disk super powers

### Added
- **A `disk` subcommand.** `list`, `get`, `put`, `delete`, `boot`, `create`,
  `init`, `sectorread` and `sectorwrite`, on DOS 3.3 and ProDOS, in `.dsk`,
  `.do`, `.po` and `.woz`. Writes are atomic, so a locked file, a full volume
  or an image that changed underneath leaves the original intact. The DOS 3.3
  and ProDOS words work, and so do host-shell habits: `cat`, `catalog`, `dir`,
  `ls`, `rm`, `read`, `write`.
- **`--basic` converts between an Applesoft listing and a tokenized program.**
  Program to program is byte-exact; listing to listing is not, since Applesoft
  normalizes on entry.
- **`--bootable` copies an operating system onto the disk**, finding the right
  master in the emulator's cache, or using the one you point it at.
- **A program can boot with no operating system on the disk.** See
  `disk create --boot` or `disk boot`.
- **Unpadded and DOS 3.3 assembler output.** The assembled span is the default
  now, and `--dos-bin` adds the BLOAD header.
- **PowerShell affordances**: an address as `$6000` or `0x6000`, `-o` with a
  separated filename, and a command line PowerShell cut at a dot rejoined
  before parsing.

### Changed
- **Command-line parsing moved to the core library**, shared by Casso and
  CassoCli, so now Casso allows / or -- style switches
- **And so did the rest of CLI**, where the unit tests reach it
  (GH #85).

### Removed
- **`--raw` is gone.** It's already default behavior

### Fixed
- GH #115: A damaged track silently truncated the image on eject. 
- Assembler exit codes now match AS65
- Assembling defaults to writing assembled bytes rather than a padded 64K bin.
- Pasting into the guest garbled the text. Ctrl+V leaked a `^V` ahead of it,
  and characters outran the guest.
- AS65 now requires no space between a flag and its value, e.g., `-lout.lst`, `-dDEBUG`, etc.
  `-o` takes either form.
- Various command line parsing improvements across all CassoCli modes

## [1.19.0]: The one where the Mockingboard speaks

### Added
- **Mockingboard C — the voice chip speaks** (#123). The SSI 263A phoneme
  speech synthesizer joins the Mockingboard as a clean-room core written from
  its datasheet: five attribute registers, all 64 phoneme codes, the published
  duration/inflection/filter formulas, and formant synthesis (glottal or noise
  excitation through three gliding resonators) with the amplitude transitioning
  the datasheet describes. The chip's ready line drives VIA #1's CA1, so speech
  software paces by polling or by interrupt, exactly as on hardware.
- **Two card models, matching the product line.** `mockingboard` remains the
  sound-only **Mockingboard A** (two empty speech sockets), byte-for-byte what
  shipped before; the new `mockingboard-c` is the **Mockingboard C** — the A
  with one SSI 263A installed in socket 1 (`$Cn40`/`$Cn60`). The ][+, //e, and
  //e Enhanced profiles now install the C by default. As on the real board, the
  speech chip is a tap: its address ranges still mirror the VIA for writes, so
  sound-only software sees nothing new, and an unprogrammed chip powers up in
  the part's own quiescent Power Down state — no audio, no interrupts.
- `Apple2/Demos/mockingboard-speech-test.a65` (+ committed `.dsk`, built by the
  new `scripts/BuildBootSectorDisk.ps1`): a boot-sector smoke test that speaks
  "CASSO ROCKS" on repeat — the speech counterpart of the tone test beside it,
  with per-phoneme durations (stop bursts, full vowels, a closure before the
  K) as a worked example of driving the chip.
- `CASSO_AUDIO_DUMP=<file>` diagnostic tap: dumps the generated stereo mix as
  raw float32 at the hand-off to the audio device, so an audible artifact can
  be attributed to Casso's stream or to the device path (see #125).

### Changed
- **The voice speaks from the chip's own ROM — a first.** The SSI 263A's
  phoneme parameter ROM — never published, substituted-for by every emulator
  — was read off the visual6502 die photographs and fully decoded: 64
  phonemes × 29 bits, comprising six significance-interleaved 4-bit fields
  (F1/F2/F3 filter codes, vocal and fricative amplitudes, nasal coupling)
  plus closure/class/fricative/voiced flags, with the column address decoder
  read off the metal to prove the phoneme mapping. Extraction data, method,
  and validation live under `specs/024-mockingboard-speech/rom-extraction/`.
  The built-in phoneme table is generated from the silicon's values; filter
  codes map to Hz through fitted curves pending the chip's exact capacitor
  weights.
- **The synthesis chain earned its ears.** Diagnosed from instrumented
  captures rather than knobs: the formant cascade gained the +6 dB/oct lip-
  radiation tilt every Klatt-style synthesizer needs (without it F2/F3 sat
  ~12 dB dark and vowels smeared together), frication moved to front-cavity
  injection after the first formant stage (at the glottis it was crushed
  into a sub-1 kHz rumble), voiced/noise gains were rebalanced from measured
  band levels with a glided 1/F1 compensation so close vowels stay audible,
  and a ~5.5 kHz output low-pass stands in for the card's analog output
  stage.
- The Hardware tab names the card model — "Mockingboard A (sound)" /
  "Mockingboard C (sound + speech)" — instead of a raw device string.
- The 6522 VIA models CA1/CB1 control-line inputs with PCR edge selection —
  the seam the speech chip's ready line needs, reusable by any future card. A
  VIA whose lines are never driven behaves exactly as before, verified by test.
- Machine profiles now ship the speech-equipped default via the embedded-config
  version mechanism (2Plus v11, 2e v10, Enhanced v4), so existing installs
  pick up the change on next launch; hand-edited profiles are backed up first,
  as always.
- internal: the shell addresses its video modes and its //e keyboard / soft-switch bank by name instead of by position in a vector and by repeated downcast — the positional lookups made every use site restate which slot held which renderer, and the `size()` guards around them were unreachable
- internal: CheckStyle's declaration-block and banner rules now see wrapped signatures, constructor-form declarations, and a statement sitting directly under the block; the ~400 pre-existing hits across the tree were swept

### Fixed
- **Audio clicks and pops under sustained sound** (#125). The endpoint was
  fed only when the emulation thread finished a CPU slice, so scheduling
  jitter or clock drift drained the device buffer and the device inserted
  hard silence — a click per starvation. Rendering now runs on a dedicated
  event-driven WASAPI thread that keeps the device fed independent of
  emulation cadence, holds a ~20 ms floor, and on genuine starvation fades
  out and crossfades back in rather than stepping. Verified by loopback
  capture: zero click events and zero dropout gaps over 20 s, from a ~1/s
  floor before.

## [1.18.2]: The one with honest execution traces

### Fixed
- **`--trace` now records operand bytes from the memory bank that actually
  executed.** The execution trace read each instruction's operands from the
  CPU's flat backing array, but on the //e and //c the MMU rebinds pages
  across all of `$0000-$BFFF` — so a trace could print operands from main RAM
  while the guest executed from aux, and the disassembly read as fact while
  describing code that never ran (a branch that plainly jumped backwards
  showed a forward operand). Operands now come through the same read-page
  table the instruction fetch uses. I/O space (`$C000-$CFFF`) deliberately
  still bypasses the bus: reading a soft switch toggles it, and recording the
  machine must never change it.

## [1.18.1]: The one with monochrome graphics fidelity

The green, amber, and white monitors were showing a tinted copy of what a
*color* monitor makes from the dots — and that decode has already thrown away
exactly the detail a monochrome monitor exists to show. Both graphics modes
now decode for the monitor you picked. Surfaced by
[(Apple IIe) Sixies](https://dskilton.itch.io/apple-sixies), which asks for
"560x192 monochrome double-hi-resolution graphics" and was unreadable on
every monitor Casso offered.

### Fixed
- **Hi-res on a monochrome monitor now shows the 560 half-dot stream.** Green,
  amber, and white monitors used to luminance-tint the artifact-color decode,
  so an isolated dot — violet or green on a color monitor — came out at about
  57% brightness where hardware shows it fully lit, and the half-dot shift was
  invisible because the color decode folds it into a palette pair instead.
  Monochrome monitors now decode the dots: every lit dot is full brightness,
  and a byte with bit 7 set paints half a dot to the right of one without,
  which is the horizontal detail 280-pixel rendering cannot represent.

  Expect this to look different, not just better. On artwork authored for a
  *color* display — anything dithered to get violet and green — a monochrome
  monitor is supposed to look harsh and washed out, which is exactly why
  monochrome-targeted software dithers at dot resolution instead. Measured on
  a dithered photograph, 94.6% of lit pixels change and mean lit brightness
  rises from 157 to 255. The color monitor is untouched.
- **Double hi-res no longer renders auxiliary memory into both halves of the
  picture.** DHR needs main and aux RAM at the same instant, but it read its
  main half through the memory bus — whose `$2000-$3FFF` pages follow live
  MMU banking. With 80STORE and HIRES set, PAGE2 alone points that whole
  range at aux, so any program that left the switches there while a frame was
  scanned had the aux byte rendered into both halves of every 14-dot group
  and never saw main memory at all. DHR now reads both banks directly, the
  way it already read aux.
- **Double hi-res on a monochrome monitor now decodes all 560 dots.** The
  green, amber, and white monitors used to tint the 16-color decode rather
  than decode differently, and that decode has already thrown the detail
  away: it collapses each group of four dots to one color, so a lone lit dot
  came out as a four-dot-wide colored block that tinting could only turn into
  a four-dot-wide gray block. Software written for 560x192 monochrome — which
  is most DHR artwork, since shading it means dithering at single-dot
  resolution — was unreadable on every monitor Casso offered. Monochrome
  monitors now decode one dot per pixel, and lit dots reach full phosphor
  brightness. The color monitor is unchanged.

## [1.18.0]: The one that conjures Merlin

### Breaking changes
- **`--cpu` is gone; use `-x` for the 65C02.** `-x` is what AS65 itself
  documents, *"Use 65SC02 extensions. When this option is not specified the
  assembler rejects the 65SC02 extensions"*, and `--cpu` was a second flag
  Casso invented for the same question. There is deliberately nothing to name
  the narrow target with: omitting `-x` **is** naming it, which is what an AS65
  user already does, and a Merlin user selects the CPU with `XC` in the source.
  Replace `--cpu 65c02` with `-x`, and drop `--cpu 6502` entirely.

  `merlin -x` is refused by name, as `merlin --cpu` was, and says to use `XC`.
- **Assembling now names its dialect: `CassoCli as65 input.a65 -o out.bin`.**
  The old form, `CassoCli input.a65 -o out.bin`, no longer works. An
  unrecognized first argument used to be assumed to be an as65 source file;
  that guess is gone, because a dialect the tool inferred is a dialect nobody
  stated, and this release adds a second one.

  Update build scripts by inserting the word `as65` after the executable,
  nothing else about the invocation changes, and the output bytes are identical
  (verified byte-for-byte against the previous binary). A bare `CassoCli as65`
  also stops resolving `as65` as a source filename.

  The tool does not print usage when it hits the old form. It names the
  replacement literally, ready to paste back, because the population this
  breaks is build scripts and nobody re-reads one until the day it fails.

### Added
- **`CassoCli merlin <source>` assembles Merlin source.** The dialect is specified
  by a subcommand, the way `as65` is. Its flags are `-o <file>` (which beats any
  name the source gives itself), `-l[<file>]` for a listing, `-v`, and
  `-d <symbol>[=<value>]`. The object written is the assembled stream: Merlin's
  origin relocates rather than seeks, so one contiguous object may carry sections
  destined for several addresses, and the padded 64 KB image `as65` writes by
  default would scatter it.

  `-d` answers the questions a source asks. Merlin stops and prompts the operator
  for a keyboard-input symbol; a batch assembly has nobody to ask, so the answer
  is typed with the invocation, once per question, and a bare `-d SYMBOL` answers
  1. It is the same flag, written the way `as65` writes it. Three of the five
  vendor sources Casso is verified against ask questions, and one of them, 
  `CLOCK.S`, produces either of the two objects Merlin shipped depending on the
  answer, from one file and two command lines.

  **`--dos-bin` and `--flat` choose what wraps the bytes.** The default is
  unchanged, the assembled stream and nothing around it. `--dos-bin` writes it
  behind a 4-byte DOS 3.3 header carrying the origin and length, which is the
  form the file takes on an Apple II disk; `--flat` writes a full 64 KB image
  with the bytes at their origin. `--dos-bin` is worth reaching for because the
  header carries the ORIGIN: the default output throws it away, and a Merlin
  source usually takes its origin from an `ORG` line rather than from the
  command line, so wrapping the bytes afterward means already knowing an address
  only the assembler saw.

  There is no CPU switch, and passing `-x` is refused rather than ignored:
  Merlin selects its CPU in the source, and the refusal names the directive that
  does it. Exit codes are the ones every assembler subcommand already speaks (
  0 clean, 1 assembled with complaints, 2 no output) and a construct outside
  the supported Merlin subset earns the same 2 as a syntax error, distinguished
  by its message rather than by a number.

  `CassoCli --help` now lists the dialects, each one's flags, where each takes
  its CPU target from, and where Merlin support ends.
- **A dialect is a mechanism, not a second assembler.** Merlin arrives as a
  directive table and a line model behind one profile seam; the two-pass engine,
  the expression evaluator and the opcode tables are the same ones `as65` has
  always used. A source is assembled strictly under the dialect its invocation
  names; there is no lenient superset that quietly accepts a mixture, so a
  Merlin construct in an `as65` file is still rejected, and now says which
  dialect defines it instead of reporting an unknown instruction.

- **AS65's `-x` is accepted, and is now the only CPU flag.** It is how AS65
  asks for the extended CPU, and Casso rejected it as an unknown flag, then
  carried on with the *narrow* instruction table and reported every instruction
  the flag had asked to enable as invalid.
- **`run` takes the assembler flags that change what is assembled: `-x` and `-d`.** Without it there was a source
  Casso could assemble and could not run, `run` refused the flag as unknown
  and then reported every 65C02 instruction in the file as invalid. It remains
  the only assembler option `run` accepts; the rest have no meaning when no
  file is written.
- **`run` names its assembler: `--as65` or `--merlin`.** `run` given a source
  assembled it as as65 and nothing else, so a Merlin source could be assembled
  or run, but not both in one step. The default is unchanged, and the flag is
  ignored for a binary, which needs no assembler.
- **`-w` wraps the listing instead of being ignored.** It was parsed, stored,
  and read by no code, so a listing ran as wide as its widest source line
  however narrow a width was asked for. `-w <n>` wraps at that column, `-w`
  alone means 133, and `0` disables wrapping; the default is 79, as65's own. Continuations
  indent to the source column, so wrapped text lines up under the text rather
  than under the address and object bytes, which a listing's reader scans
  positionally.
- **The symbol table shows decimal beside hex.** AS65 lists "the name,
  hexadecimal and decimal value of each label"; Casso showed hex alone, which
  is the wrong half for a symbol standing for a count rather than an address.
- **`-g` writes its symbols in both useful orders.** The debug file lists them
  by address as before, then again by name, case-insensitively. Reading one is
  two different questions (what sits at an address, and where a name went)
  and each order answers one of them.
- **`--help` fills the terminal it is printed to.** Every line of usage is now
  written once, whole, and folded at print time to the console's width, so a
  wide terminal gets a wide reference instead of a column of text down its left
  third and a narrow one stops truncating. A wrapped flag description continues
  under the description rather than at the left margin, which is what keeps the
  two-column table reading as a table. Redirected output is folded at 80, since
  a file has no width to ask about and will be read in an editor.

### Changed
- **An argument the tool does not know is refused, with the help that applies.**
  A flag no subcommand's grammar recognized used to earn a one-line warning and
  then be ignored; the assembly ran, the exit code was 0, and the output file
  was written as though the flag had been honored, so a typo in a build script
  was silent in every way that mattered. Now the invocation is refused, at the
  status its own mode documents for a refusal, 1 when assembling, 2 under
  `run` and `disk`: the full usage is printed, and the line naming the argument
  comes LAST, so it is what is left on screen. A first word that names no
  subcommand gets the same treatment, with the line naming what to type instead
  at the end.
- **A run that specified no CPU now reports the target that stood.** Under `-v`
  it goes to stderr, and into the listing header when a listing is produced;
  never to stdout, which carries the listing itself when no listing file is
  specified. This affects `as65` as well as `merlin`: a build that passes `-v` or
  `-l` gains one line reading `cpu: 6502 (dialect default)`. The point is that
  "nothing selected a CPU, so the default stands" and "the flag I passed was
  quietly dropped" used to look identical, and a dialect that selects its CPU in
  source makes the difference matter.
- **Three assembler diagnostics now quote the directive the source wrote.** The
  origin and reserve-space messages specified `.org` and `.ds` as literal text,
  which is a form no dialect's table holds and simply wrong at a line that
  wrote something else. They now quote the active dialect's canonical name,
  so `as65` reads `.ORG` and `.DS`, the same directives, upper-cased. Nothing
  else about the messages changed, and no output byte moves.
- **A mistyped directive no longer silently drops the bytes it should have
  produced.** A dotted word the dialect does not define was discarded without a
  diagnostic: the line vanished from the listing, every address below it moved
  up by however many bytes the directive would have emitted, and the run exited
  zero. `.org $0300` / `.fill 8, $EA` / `rts` wrote a one-byte object and called
  it a success. The word is now reported by name, with its line and file, so
  a typo costs a message instead of an object nobody can explain, and where the
  word belongs to another dialect, the message says which. This applies to every
  dialect, since it is the shared engine that reports it. A source that has been
  quietly losing a line will now fail to assemble; the line was never being
  assembled, so the bytes have not changed, only the silence.
- **The usage line no longer advertises the removed bare-source form.** It read
  `CassoCli <source> [flags]`, which stopped working when the dialect became
  something the invocation names; it is now built from the subcommand table, as
  is the list of alternatives offered when the first word names nothing.

### Fixed
- **Naming two output formats is refused instead of resolved.** `-s --raw`
  silently wrote raw, and `--raw -s` silently wrote an S-record, because all
  four format flags set one field and the last assignment stood. Each flag
  is valid on its own, so nothing about the result looked wrong. The refusal
  names both flags. Repeating one flag is still fine; it asks for one thing,
  twice.
- **Merlin now takes its mnemonics in any case.** `lda` was refused as an
  invalid mnemonic while `org` on the line above was accepted, so the case rule
  disagreed with itself inside one dialect, and the diagnostic never mentioned
  case, leaving `Invalid mnemonic: lda` to be puzzled out. Instructions,
  directives and the alternate branch names are now all case-insensitive, and
  the bytes are identical either way. Symbols are untouched and stay
  case-sensitive: a label written `lda` remains legal and is warned about
  rather than refused, which is what period sources rely on.
- **A long option written with a slash now works instead of silently doing
  something else.** `CassoCli as65 src.a65 /raw` used to fall through to the
  single-letter parser as `-raw`: it warned about an unknown `-r` and `-a`,
  swallowed the `w`, and wrote the padded 64 KB image the flag exists to
  suppress. `merlin src.s /dos-bin` was worse, `-d` takes a value, so `os-bin`
  became a predefined symbol, no warning appeared at all, and the output simply
  had no header. Both exited 0. Every long option now accepts either prefix,
  and the two forms mean the same thing.
- **Messages write flags with the prefix the invocation used.** A `/`-style
  command line was answered in places with `--raw`, `--dos-bin` and `-d`,
  mixing both conventions inside a single help block, and, before the fix
  above, advising a form the parser would then have refused. Help, the
  unknown-flag warning, the CPU-flag refusal and the assembler's own
  "define it on the command line, for example `-d NAME=0`" now all follow the
  prefix the user typed. A command line mixing the two is answered in whichever
  it opened with, so the reply never depends on which flag happened to come
  last.
- **A dialect no longer borrows the other dialect's vocabulary.** A word the
  active dialect declined was offered to a second, fixed directive table, so 55
  words Merlin does not have still resolved, and eight of them steered
  conditional assembly. A Merlin source writing `IFDEF` or `.ENDIF` had its
  blocks honored by a directive Merlin has never had, so lines were included or
  skipped by a construct the real assembler would have rejected. The same table
  ran the other way too: the closers an early macro exit owes the conditionals
  it abandons were counted from the wrong vocabulary and written in the wrong
  word, and `NOP 3`, as65's way of asking for three of them, silently emitted
  three bytes in a dialect with no such form.

  Every one of those words is now refused by name in the dialect that does
  not define it. Nothing changes for `as65`, whose own words were never in
  question; what changes is that Merlin source is read only as Merlin.
- **A macro definition with no terminator of its own now falls into the next.**
  Merlin lets a family of macros that end the same way be written once, the
  longest first, each shorter one opening where its own body starts, and a
  single terminator at the bottom closing them all. The assembler collected one
  definition at a time, so the second opening line was swallowed as body text,
  the definition it should have started was never made, and the source was
  reported as one definition that never ends. The distribution disk's own macro
  library is written that way and could not be assembled at all.

  Every dialect gets the behavior, not just Merlin: the collector had no way to
  represent overlapping definitions in any dialect, so as65's `macro` / `endm`
  gains it too. An unterminated definition is now reported once per open level,
  each at the line it opened on.
- **An assembler directive whose operand names a file now keeps the spaces in
  it.** ` USE MACRO LIBRARY` names one file on the Merlin distribution disk; the
  operand was read to the first space, so the assembler asked for `MACRO` and
  reported the real file missing. A comment after the name still ends it. The
  same line was also being read as *defining a macro called `PUT`*, because the
  keyword that opens a definition from the operand field was a fixed word in the
  shared engine rather than the active dialect's own; it is the dialect's
  now, and Merlin, which opens definitions with `MAC` in the opcode field, has
  no operand form at all.
- **A symbol assigned twice now holds, at each reference, the value assigned
  most recently above it.** `name = expr` defines a *reassignable* symbol, and
  pass 2 resolved every reference against one symbol table built after pass 1
  had finished, so they all took the last value the file ever assigned, while
  pass 1 had already sized the lines between the assignments against the values
  in force at each. A file that assigns a symbol twice and refers to it between
  the assignments assembled cleanly and emitted the wrong bytes; it now emits
  the right ones. A file that assigns each symbol once is unaffected.
- **A diagnostic raised inside an included file now names that file.** Errors
  and warnings were all attributed to the top-level input, because by the time
  one is printed that is the only filename in hand, so an include's own line
  number arrived paired with the wrong file, sending the reader to whatever
  happened to sit at that line of the outer source. The originating file is now
  captured where the diagnostic is created and travels with it.

## [1.17.0]: The one that salvages a damaged disk

### Added
- **Salvage the readable sectors of a damaged disk.** A disk whose stored
  checksum does not match its contents is held read-only, which protects the
  evidence of the damage but leaves the user with a disk they cannot write to
  and no way forward. Casso can now build a salvaged copy beside it, named
  `<disk>.salvaged.woz`, offered from the Disk menu and from the report shown
  when the damaged image is inserted.

  Sectors come back in one of three states, and the difference decides what
  happens to each. A sector whose own checksums match is copied unchanged. A
  sector that decoded but failed verification is *recovered* rather than
  discarded: its bytes are kept and written with a freshly computed checksum,
  so a sector that would have made DOS report an I/O error reads normally
  instead. Only a sector with nothing usable (no data field, or an address
  field whose checksum fails, so its number cannot be trusted) is zeroed,
  because filing data under a number that might be wrong risks overwriting a
  good sector.

  Recovering rather than zeroing matters more than the counts suggest. The
  data field decodes as a running XOR chain, so a single bad nibble leaves
  every byte before it exactly right and skews the rest by one constant delta
, and if what rotted was the check nibble itself, the sector is perfect.
  Zeroing throws all 256 bytes away to avoid admitting to a few.

  The figures are shown before anything is written, because a lossy copy is a
  decision to make with the numbers in front of you, and the dialog warns that
  repairing the checksums makes the disk structurally sound but cannot recover
  corrupt data within the sectors. The original is never opened
  for writing: it keeps its damage, detectably, which is the guarantee the
  whole feature rests on. The salvaged copy carries the source's `META` across
, it is still the same disk, but takes Casso's own creator stamp,
  because Casso wrote that particular file and leaving an imaging tool's name
  on a lossy reconstruction would be its own kind of false record.

  Offered only for a disk that is both damaged and ordinarily formatted. An
  undamaged disk is already writable, so a lossy copy could only lose data; a
  copy-protected one has no standard sectors to recover and would be destroyed
  by a rebuild. Neither is offered the command.
- **`--raw` and `--dos-bin` assembler output.** The assembler could only write
  a full 64 KB memory image, padded with the fill byte, correct for ROM
  burning and reference comparison, useless for loading a 2 KB routine, which
  meant slicing 64 KB down by hand. `--raw` writes only the assembled span;
  `--dos-bin` writes that span behind the 4-byte load-address/length header an
  Apple DOS 3.3 binary file carries, so the result is ready to `BLOAD` once
  placed on a disk. The default is unchanged.

### Changed
- **The write-protect menu item names the flag it changes.** It read "Allow
  writes to <disk>", which promised an outcome it cannot deliver: the drive
  preference and the host file's read-only attribute also refuse a write, and
  neither is touched here. It also flipped its verb on the file attribute as
  well as the in-file flag, so a writable image inside a read-only file offered
  to allow writes and then could not. It now reads "Clear" or "Set", the
  disk, "internal write-protect flag", keyed to the WOZ flag alone.
- **Errors reach the user through Casso's own dialogs.** Every user-facing
  error in the tree (a settings file that will not parse, a disk that will
  not save, a damaged image) surfaced as a raw Win32 message box in the
  middle of a themed application, because the notification sink installed at
  startup called `MessageBoxW`. All twenty-one report sites now render as
  ordinary Casso dialogs, with no change at any call site.

  Two details that path has to respect. A report can be raised off the UI
  thread, a motor-idle auto-flush runs on the CPU thread, so those are
  marshaled rather than drawn from wherever they happened. And a failure
  during startup arrives before there is a window to parent a dialog to, which
  is why a system box was used here in the first place; those are now held and
  shown once the window exists, so they arrive themed a moment later rather
  than unthemed and early, and none are dropped.

  Three system message boxes remain deliberately, each a case where depending
  on Casso's own interface is what would make it unreliable: the assertion
  reporter, which exists to report that an internal invariant broke and cannot
  assume the interface still works; the fallback for when the dialog backend
  itself fails to start; and the console path, which `CassoCli` takes.
- **An explicit output-format flag now wins over the filename's extension.**
  Extension matching remains as the fallback when no flag is given, so as65-era
  scripts naming a `.s19` or `.hex` output keep working. Previously the
  extension always won, which meant `-s -o out.dat` silently wrote a flat
  binary despite the flag asking for an S-record.
- Command-line option modelling and parsing moved from the `CassoCli`
  executable into `CassoCore`, where the test project can link it. Parsing was
  previously unreachable from any test. Behavior is unchanged and now pinned by
  tests; the grammar's one filesystem question, does `build` name a real
  `build.a65`?, is injected rather than probed directly.

### Fixed
- **Decoding a sector now verifies it rather than merely parsing it.** Three
  integrity signals are recorded on every Apple II disk and all three were
  being discarded: the address field's checksum was read into locals and
  explicitly thrown away, the data field's 343rd checksum nibble, the boot
  ROM's own success gate, was never read at all, and a byte that is not a
  legal 6-and-2 code was already detected and ignored. All three are checked
  now.

  A fourth check needed no new data and fixes real corruption. The scan for a
  sector's data field ran on past the next address field if the data field was
  missing, took that sector's data and filed it under the earlier number,
  so it decoded cleanly, passed its own checksum, and was simply wrong. One
  point of damage produced two bad sectors, and the second was undetectable
  because nothing about it looked wrong. The scan now stops and rewinds, so a
  damaged sector costs exactly itself: measured on an image with one data
  field destroyed, sectors differing from the original drop from two to one.

  This also sharpens what "unformatted" means. A copy-protected track used to
  yield one spurious decoded sector from garbage that happened to parse;
  protected disks now report cleanly as having no standard structure, which is
  the distinction the salvage gate depends on.

  One finding worth recording: `AppleStellarInvaders.woz`, whose file-level
  checksum is valid and which round-trips byte for byte, has a sector that
  fails its own data checksum. It is a genuine defect in a 1980 preservation
  dump that nothing had noticed, not a false positive, every other sector
  on that disk and all 560 on three other intact dumps verify.
- **The write-protect toggle is no longer offered for a damaged disk.**
  Changing that flag means patching the file and recomputing its header
  checksum, and that checksum failing to match *is* the evidence of damage, so
  the operation refuses a damaged image. The menu item stayed enabled anyway,
  which meant the only way to learn any of this was to click something that
  could never work. An already-protected image still offers the toggle, 
  clearing the flag is exactly what a user does before writing to a disk,
  and only damage disables it.
- **A menu no longer clips what it has to say.** The Disk menu's dropdown was a
  fixed width, so any row wider than it wrapped and ran into the row beneath.
  That was already truncating stock accelerators, "Ctrl+Shift+1" rendered
  as "Ctrl+Shift", and it left no room for a label that names a file. The
  dropdown now measures its widest row, label plus accelerator, and keeps the
  old width as a floor, so every menu that fitted before is unchanged.
- **The write-protect toggle is no longer offered when the write cannot
  succeed.** The flag it changes lives inside the file, so setting or clearing
  it means writing the file, which a read-only backing file or a
  permissions failure makes impossible. The item stayed enabled anyway and
  failed on the click. It now requires a writable file, exactly as it already
  required an undamaged one.
- **A dialog button no longer clips a long label.** Buttons were laid out at a
  fixed width, so any label longer than about a dozen characters wrapped and
  spilled outside the button. Buttons now size to their label and never go
  below the previous width, so every existing dialog is unchanged.
- **Saving a WOZ no longer strips its metadata.** A round trip through Casso
  deleted the `META` chunk outright and overwrote most of `INFO`, so every WOZ
  that passed through a flush came back degraded: a preservation dump lost its
  title, publisher, developer, copyright, language and imaging provenance, and
  its creator field was replaced with `Casso`. The cause was the write-back
  path itself, it rebuilds the file from the live per-track buffers, which is
  what makes guest writes survive, and a writer that reconstructs from the
  track model can only emit what that model holds. The image now retains the
  source `INFO` chunk and every chunk Casso does not parse, and re-emits them
  unchanged, so a flush preserves them byte for byte while still writing guest
  writes out. Verified across the demo library: all six intact WOZ 2 dumps now
  round-trip byte for byte.

  Two consequences worth knowing. Casso stamps its own name into `creator`
  only on a disk it authored itself and preserves whoever imaged the disk
  otherwise, overwriting that field destroys the one record of where a dump
  came from. And a chunk Casso has never heard of no longer ends the chunk
  walk: it is stepped over and kept, where before it stopped the scan and took
  every chunk after it down with it.
- **A WOZ 1 image upgraded to WOZ 2 keeps what version 1 recorded.** Casso
  always writes version 2. The fields version 1 never had are now filled for
  the two with no legal zero, disk sides and optimal bit timing, while the
  three Casso cannot derive stay `unknown` rather than being guessed at.
  Creator, synchronized and cleaned carry across.
- **Toggling a WOZ's write-protect no longer rewrites the disk.** The flag
  lives inside the file (`INFO` byte 2), so changing it has to write, and the
  only writer available was the one that rebuilds the whole image from the
  track model. One menu click therefore relaid out an entire disk to carry one
  bit, with no guest write and no emulation involved, and it fired in both
  directions: un-protecting a preservation dump before writing to it rewrote
  it too, which is the one thing a user does to those files. Seven of the
  eleven demo images ship write-protected. The toggle now patches the single
  flag byte, recomputes the header checksum and writes the result back
  atomically. Measured across the demo library: un-protecting an image changes
  five bytes, the flag and the four checksum bytes, where the old path
  changed up to 223,700 and dropped the `META` chunk.

  This is deliberately a guarantee by construction rather than one that
  depends on the rebuild path being complete: bytes that are never parsed
  cannot be damaged. The store no longer offers any way to force a flush past
  its dirty and write-protect gates, because changing this flag was the only
  thing that ever wanted one.
- **A sector image that cannot be fully decoded is no longer saved as if it
  were** ([#115](https://github.com/relmer/Casso/issues/115)). Writing a
  `.dsk`, `.do` or `.po` back out decodes every track's nibble stream to
  sectors, and it reported success no matter how much of that failed, so a
  track Casso could only partly read was written over the user's file as a
  mixture of real sectors, zeroed sectors and wrong ones, reported as a clean
  save. Measured on an image with a single sector's data field destroyed: that
  sector comes back as zeros, *and* a second one comes back holding the
  following sector's data, because the search for the missing data field runs
  on and finds the next one, filing it under the sector number the address
  field gave. Two sectors wrong from one point of damage, silently.

  Denibblizing now reports what it decoded, per track and per sector, and a
  track that decoded some of its sectors but not all fails the whole
  operation. The flush path already handles that correctly: it reports the
  loss and keeps the image dirty, leaving the existing file untouched. A track
  that decodes nothing at all still succeeds, an unformatted track in a
  sector image legitimately is zeros, and treating that as damage would make
  every blank disk refuse to save.

  WOZ images are unaffected: they are written by the WOZ serializer and never
  go through this path.
- **A damaged disk image is now held read-only instead of being quietly
  repaired-looking.** Casso has detected a WOZ whose stored checksum does not
  match its contents since 1.16.2, but it went on to treat the file as
  writable, and rewriting it stamps a freshly computed, correct checksum over
  the same damage, so nothing afterwards can tell the file is wrong. The image
  now becomes write-protected for the session, as a fifth and separate cause
  alongside the in-file flag, the user preference and the two file-state
  causes, so the interface can say *why* rather than just refusing. The disk
  still loads and still reads: being able to open a damaged preservation dump
  is the point.

  Deliberately session state and not the image's own flag, because that flag
  lives inside the file, setting it would mean writing the very file being
  protected from writes. The write-protect toggle also now refuses a damaged
  image outright, since patching its flag byte recomputes the header checksum,
  and that checksum failing to match *is* the damage report.

  The emulated machine sees the disk as write-protected too. That is the
  intended trade: telling the guest the truth beats accepting its writes and
  discarding them later.
- **A damaged image gets its own badge, not the padlock.** An ordinary
  write-protect is something the user chose and can undo; a damaged file is
  neither, and showing the same brass padlock for both invited a hunt for the
  toggle that would now refuse them. Damaged disks show an amber warning
  triangle in the padlock's place, on the drive faceplate and in the compact
  drive row alike, and the hover text leads with what is wrong and why Casso
  will not write to it.
- **A disk image that cannot be serialized is no longer "saved" by reverting
  it.** `DiskImage::Flush`, reached on eject and on //e soft reset, fell back
  to writing the file's pre-session bytes over the user's disk when
  serialization failed, and returned success: every guest write of that
  session vanished silently. It also opened the target directly, truncating it
  before the first byte landed, and never checked the stream afterwards, so a
  full volume or a disconnected share destroyed the image and reported nothing
, the same defect fixed in the store's flush path in 1.16.2, in a second
  write path that fix did not cover. Both are gone: the bytes go through the
  atomic write, and a failed serialize now fails loudly and leaves the image
  dirty so a later flush retries.

## [1.16.2]: The one with safer disk image writes

### Fixed
- **A failed disk save no longer destroys the disk.** Flushing a mounted image
  opened its own file for writing, which truncates it before the first byte is
  written, and then never checked whether the write actually succeeded, so a
  full volume or a disconnected share replaced a working image with a truncated
  one, or with nothing at all, and the image was marked saved either way. The
  bytes now land in a temporary file beside the target, are verified in full,
  and only then replace it. A failure at any point leaves the original file
  exactly as it was, reports the loss, and keeps the image dirty so a later
  flush retries.
- **WOZ images Casso writes can now be read by other tools.** Every WOZ Casso
  saved (a mounted disk flushed after a guest write, or a blank disk it
  created) declared a `TRKS` chunk size covering only the 160-entry track
  record table, leaving the track data that follows it outside the chunk it
  belongs to. Casso read its own files back correctly, so the damage was
  invisible from inside the emulator: a conformant parser adds that size to the
  chunk start, lands in the middle of track data, finds no valid chunk id and
  stops, making every chunk after `TRKS` unreachable to every tool but ours.
  The size now spans the record table plus the block-aligned bit streams, which
  is byte-for-byte what Applesauce writes for the same disk. No track data
  changes, only the size field and the header CRC.

  A round trip still drops the `META` chunk and most of `INFO` (creator,
  synchronized, boot sector format, compatible hardware, required RAM). Those
  are separate defects in the same writer and are not fixed here, so a
  preservation dump still loses its provenance on flush.
- **A damaged WOZ is now reported instead of quietly repaired-looking.** The
  loader never checked the checksum stored in a WOZ header, while the writer
  always stamped a freshly computed one, so a corrupt image opened silently,
  and the first save replaced it with a correctly checksummed copy of the same
  damage, after which nothing could tell it had ever been wrong. Casso now
  validates the stored checksum at load and says so when it does not match. The
  image still loads, because being able to open a damaged preservation dump is
  the point, and a checksum of zero still means "none computed" as the format
  specifies. Saving such an image warns first, since that save is what makes the
  damage undetectable.
- **Pasting into the guest no longer garbles the text.** A valid Applesoft line
  pasted at the `]` prompt produced a SYNTAX ERROR while typing the same line
  by hand worked. Two independent causes, both fixed: Ctrl+V is claimed as a
  host shortcut but Windows synthesizes its control character anyway, so a
  `^V` reached the guest keyboard ahead of the pasted text; and characters were
  fed faster than the guest could take them. Feeding is now paced in emulated
  cycles and waits for the keyboard strobe to stay clear, because the guest
  clears it the moment it takes a key but may flush the keyboard again while
  processing one, so a character sent on the first clear reading races into
  that window and is dropped.

## [1.16.1]: The one where the //c mouse works with VBL software

### Fixed
- **The //c mouse was dead in MousePaint** (and in anything else that runs
  the mouse from the vertical-blank interrupt rather than from movement
  interrupts). Mouse mode only came alive once guest software enabled the
  IOU's X/Y movement interrupt, but `SETMOUSE` does not program that for
  every active mode: MousePaint's main app asks for mouse-on plus VBL
  interrupt and leaves X/Y masked, so the pointer sat frozen wherever the
  firmware left it and clicks went nowhere. Either interrupt enable now
  counts, and Mouse mode is still invisible at a BASIC prompt.
- **The IOU register gate ignored half its addresses.** `$C07E`/`$C07F`
  (SETIOUDIS / CLRIOUDIS) drive the same latch as `$C078`/`$C079`, and
  Apple //c Technical Note #9 documents that pair for VBL polling, so
  the published sequence was programming annunciators instead of the
  mouse switches.
- The Input Debug panel called a held //c mouse button released: `$C063`
  is active low there (`$00` = pressed), unlike the //e's shift-key mod
  at the same address.

## [1.16.0]: The one that creates blank disks

### Added
- **Create blank disks in-app.** The insert-disk picker gains a pinned
  `<Create new disk...>` first row that opens a themed save-style dialog:
  browse folders (with a `..` row) right in the dialog, pick the format
  (DOS 3.3, ProDOS 1.1.1, or unformatted) and the image type (WOZ, DSK,
  or PO, the format drives which types are offered, so only legal
  pairings exist), and name the file. The new disk mounts straight into
  the drive that opened the picker. A freshly created DOS 3.3 or ProDOS
  disk is immediately usable, the guest can `SAVE` / `CAT` with no
  `INIT` step, and an optional Make-bootable checkbox installs the real
  OS from the stock master disks (downloaded on demand with an explicit
  click when not cached): DOS 3.3 disks boot to a clean Applesoft prompt
  via a `HELLO` greeting, ProDOS disks boot through `PRODOS` into
  BASIC.SYSTEM. The dialog refuses a target that is currently mounted in
  a drive, asks before overwriting an existing file or replacing an
  occupied drive's disk, and remembers the folder the last disk was
  created in. Navigation behaves like a real save dialog: double-click
  descends, `..` re-selects and centers the folder you left, clicking any
  control moves keyboard focus there, the name field supports
  click-to-place-caret / drag selection / Ctrl+word-jumps, and the
  Modified column formats in the user's locale.
- **Write-protect toggle for mounted disks.** The Disk menu names its
  target and states the action: "Write-protect \"Blank Disk.woz\"" flips
  to "Allow writes to \"Blank Disk.woz\"" once protected (enabled only
  while mounted). WOZ images flip the write-protect flag that travels
  inside the file, pending guest writes are flushed first so nothing is
  lost, while DSK/DO/PO images set or clear the host file's read-only
  attribute. The drive widget's padlock and its tooltip re-read reality
  after each toggle; the tooltip names the disk and the exact cause
  ("WOZ write-protect flag", "file is read-only", "no write permission",
  or the drive-level Settings preference), and a protected disk fails a
  guest `SAVE` with `WRITE PROTECTED` exactly like the notch tab on real
  media.

### Changed
- dxui: menu-bar subitems support live labels and enabled state; hotkey-less
  rows use the dropdown's full width
- dxui: tooltips word-wrap instead of growing past the window edge
- dxui: list views support double-click activation, always-visible selection,
  and center-on-row scrolling
- dxui: text inputs place the caret from clicks, drag-select, and take
  Ctrl+Left/Right word jumps (host supplies the text renderer)
- ui: the file-open picker no longer advertises `*.nib` (never mountable)

## [1.15.2]: The one with the settings live-preview fixes

### Fixed
- fix(settings): the Display live-preview reveal shows the emulator instead of an opaque gray block (DWM frame fill behind the composited sheet; reveal now hugs the CRT image and pins the emulator window behind the sheet)
- fix(settings): the emulator responds to Display slider drags immediately instead of in bursts when the drag pauses (message-drain deadline + the sheet no longer stacks a second vblank wait)

## [1.15.1]: The one with the drive-door polish

### Fixed
- fix(chrome): drive door animates in parallel with the disk picker instead of stalling until it closes
- fix(chrome): canceling the picker closes a mounted drive's door again (the restore was dead code)
- fix(chrome): eject-click door flap (open-close-reopen race with the CPU-thread eject)
- fix(chrome): Disk > Insert gets the same door choreography as a drive-widget click
- fix(printer): 3D preview scales instead of clipping; preview minimum size raised to a renderable size
- fix(asm): diagnostics report in source order; an unclosed `.if` is reported at the `.if`, not end-of-file
- fix(config): a corrupt or unreadable prefs file notifies (with the path) instead of asserting
- fix(theme): an unknown theme name reports failure instead of silently falling back
- fix(dxui): checkboxes / toggles no longer paint with zero area (WM_PAINT before first layout)

### Changed
- shell: window caption drops `[Running]` and the Debug tag
- trace: interrupt-handler entries tagged; IRQ/NMI rates summarized
- internal: repo-wide style sweep, now gated by CheckStyle in the pre-push hook and CI

## [1.15.0]: The one with the //c mouse fixes

### Fixed
- **fix(2c): acknowledge the //c VBL interrupt across the whole $C070-$C07F
  PTRIG page**; the paddle-timer strobe is only partially decoded, so any
  access in $C070-$C07F fires PTRIG and clears the VBL interrupt latch; Casso
  honored only the literal $C070. Apps that run the mouse in VBL-interrupt mode
  acknowledge the VBL as a side effect of a $C07x access (MousePaint does
  `STA $C079` each interrupt), so the latch was never cleared and the interrupt
  re-fired every time interrupts were enabled, pinning the CPU in its handler.
  MousePaint's main app was effectively unusable: menus and tools ignored
  clicks and the cursor lagged badly. Both are fixed. (Karateka's sticky
  `$C019` VBL poll is unaffected, it never touches $C07x during its wait.)

### Changed
- **perf(mouse): batch `AppleMouse::Tick` bookkeeping off the per-instruction
  path**; keep only the movement latch per-instruction (so the cursor tracks
  without lag) and run the retarget countdown, VBL onset sample, and host-motion
  drain at a coarse cadence (`kSampleQuantum`). ~31% cheaper per tick
  (4.07 → 2.81 ns, microbenchmarked); //c-only, feel-neutral.

## [1.14.0]: The one with the ImageWriter II (spec 015)

### Added
- **feat(printer): parallel printer card + original slot firmware**: a dumb,
  output-only parallel interface card (default slot 1, optional per machine)
  with firmware assembled from in-repo source (parity- and CPU-execution-
  tested): honors the COUT contract (A/X/Y preserved), injects LF after CR
  like Apple's real card, and deliberately claims no Pascal 1.1 signature.
- **feat(printer): ImageWriter II interpreter**: the command subset locked
  from real Print Shop byte captures: `ESC G` (native 160 dpi, LSB = top pin)
  and `ESC L` (120 dpi, MSB = top pin) bit image, `ESC T`/`A`/`B` line
  spacing, `ESC K` seven-color select with overprint composites, the
  documented pitch family, and reset. Card status byte `$83` satisfies both
  the Apple II Parallel and Grappler+ driver probes.
- **feat(printer): draft text printing**: an original 95-glyph dot-matrix
  font (5×7 body + descender row, designed in-repo): `PR#1` + `LIST` /
  `CATALOG` print readable text at every documented pitch with 80-column
  wrap and high-bit ASCII handling.
- **feat(printer): live skeuomorphic preview**: a real-3D ImageWriter II
  (the project's own CAD model via an OBJ import pipeline) with fanfold
  paper, tractor strips and perforations, a one-page live viewport with
  scrollback, and paper that reads through the smoked cover like the real
  machine.
- **feat(printer): one print-head clock**: carriage animation, ink reveal,
  and audio share a single pacing model: bidirectional passes at real draft
  speed, logic seeking (a pass spans only the printed region), blank feeds
  slewing with the head parked, and jump-cut coalescing for huge backlogs.
- **feat(printer): mechanical audio**: Scott Lawrence's CC BY 4.0
  ImageWriter II recordings (bundled, attributed): quality-keyed carriage
  loops gated by an edge-triggered ink detector, per-line feed clacks, page
  feeds, and tear-offs; window-relative stereo auto-pan with a manual
  override; printer volume + mute on Settings > Printing.
- **feat(printer): delivery**: non-destructive Print / Save PNG / Copy (the
  paper stays until Discard tears it off); the modern Windows print dialog
  with a live preview of the real paginated pages (PrintManager interop +
  Direct2D print control, classic dialog fallback); pending printouts
  persist per machine across sessions.
- **feat(chrome): command toolbar**: Settings, Printer (with an event-only
  status LED: green printing, amber page waiting, red delivery error),
  master volume + mute, Screenshot, Reset, and Power below the menu bar;
  responsive three-tier presentation (icon+label → ribbon → icon-only with
  tooltips); replaces the standalone printer indicator.
- **feat(audio): master output volume**: one gain over the completed mix
  (speaker, drives, printer, Mockingboard), persisted with mute.
- **feat(printer): Settings > Printing info banner**: a themed banner at the
  top of the Printing tab states what printer the current machine emulates and
  how it connects ("Apple ImageWriter II ... parallel interface"), or that a
  slotless machine (//c) has none; built on a new reusable `DxuiInfoBanner`
  control with theme-derived colors.

### Fixed
- **fix(shell):** the OS modal move/size loop (title-bar hold, resize) no
  longer freezes the preview and sound; a host keep-alive timer pumps the
  UI frame.
- **fix(shell):** machine switches no longer touch chrome from the CPU
  thread (the //c → //e assert); thread-affinity guards added at the chrome
  entry points.
- **fix(dxui):** showing an already-visible window without activation keeps
  it maximized (`SW_SHOWNA`).
- **fix(printer):** honest Windows spool failure mapping, SP_* return codes
  and mid-job Save-As cancels handled; failure dialogs name the failing
  stage in plain language with the OS detail attached.
- **fix(cpu):** unimplemented undocumented opcodes log instead of breaking
  into the debugger (Space Quarks, GH #95).

### Changed
- Print delivery destination is chosen per action (Print / Save / Copy), not
  a stored preference; Settings > Printing keeps output resolution + dot
  style and gains a Restore-defaults button.
- The window caption reads a clean "Casso - &lt;machine&gt;" in retail (Release)
  builds; the run-state tag and build-identity stamp are kept in Debug only,
  and a paused / stopped emulator is still flagged in every build.

## [1.13.0]: The one that got faster

### Changed
- **perf(emulation): per-instruction hot path**: a sweep of the work that runs
  on every emulated instruction. RAM/ROM reads now serve inline from a page
  table instead of a virtual dispatch; I/O ($C000–$FFFF) resolves through a
  direct device map instead of scanning the device list; the language-card
  ($D000–$FFFF) and the //c internal ($C100–$CFFF) ROM windows are page-mapped
  so they read inline; and the interrupt poll, the video-timing tick, and the
  //c mouse tick shed redundant per-instruction work (the mouse's vertical-blank
  edge is now sampled on a coarse cadence rather than checked every
  instruction). Net: lower steady-state emulation CPU, most visibly on the //c.
- **perf(video): dirty-row text rendering**: the 40- and 80-column text
  screens re-rasterize only the rows whose characters changed since the last
  frame (plus, on the flash blink, the rows holding a flashing glyph) instead
  of redrawing all 24 rows every frame, so a scrolling catalog or a blinking
  cursor no longer repaints the whole screen.
- **perf(render): cached text shaping and geometry**: the UI chrome caches its
  shaped text layouts and solid brushes instead of re-shaping every label each
  frame, and appends quad geometry in bulk.
- **perf(audio): idle Mockingboard**: AY-3-8910 tone/noise synthesis is skipped
  while the chip is fully muted.

### Fixed
- **fix(shell): machine switch**: switching machine model (e.g. //c → //e) from
  the menu no longer trips a UI-thread assertion; the pointer and joystick
  controls are re-synced on the UI thread once the switch completes.

## [1.12.0]: The one with the CRT monitor

### Added
- **feat(chrome): skeuomorphic CRT monitor desk scene**: an opt-in
  "CRT monitor desk scene" checkbox on **Settings → Theme** (skeuo themes
  only, default off) frames the emulator display in a procedurally-drawn
  period **Apple Monitor //c**, snow-white/platinum shell with an even
  chunky bezel, straight sides with rounded corners and a slightly bowed
  glass, a recessed screen, the rainbow cassowary brand and a lit power lamp
  on the chin. The display sits inside the glass at true 100% zoom by default
  (the window sizes the whole scene around it), and the drive widgets scale
  down to sit in proportion under the monitor. The whole scene zooms together
  as the window resizes; toggling the checkbox applies live, and turning it
  off restores the classic bare display at full drive sizes. Off by default
  because the scene trades screen real estate for the look.

### Changed
- **perf(shell): idle CPU and GPU**: the emulator no longer re-rasterizes the
  screen and re-runs the full CRT post-process + Present every 60 Hz when
  nothing on screen has changed. The memory bus now tracks writes into the
  display pages (dirty-tracking), so the video frame is regenerated only when
  the picture's actual inputs move (a write that changes displayed memory, a
  video mode / soft-switch change, the text flash phase, or the monitor color)
  and a static screen (a menu, a BASIC prompt) drops render + present work
  toward zero instead of pinning the GPU at a constant load. The UI thread now
  blocks until the next frame or input arrives rather than spin-polling, and at
  Maximum speed the CPU runs flat-out while the picture is still only shown ~60
  times a second, no more burning cores on frames no one ever sees.
  Drive-activity lights keep animating through a disk load even behind an
  otherwise static screen.

### Fixed
- **fix(shell): saved CPU speed now applies at startup**: a saved emulation
  speed of Double or Maximum is applied when Casso launches, instead of only
  showing in Settings while the CPU quietly ran at Authentic speed until the
  setting was re-selected. The cold-boot path applied the saved color mode but
  overlooked the speed mode.
- **fix(window): Alt+Enter fullscreen**: several fullscreen defects are
  resolved. DXGI's built-in Alt+Enter handler (installed on the HWND swap
  chain) was double-handling the keystroke and racing the app's own
  borderless toggle; it is now disabled via `DXGI_MWA_NO_ALT_ENTER` so the
  app owns the transition. Entering fullscreen no longer stomps the user's
  saved windowed placement (a synchronous resize used to persist the
  full-monitor rect as the window size), a maximized window round-trips back
  to maximized with its underlying size intact, and a mid-transition assert
  dialog can no longer strand the window as an unescapable, taskbar-covering
  borderless popup, the toggle self-heals from a state desync and always
  hands back a movable, closable window.

## [1.11.0]: The one with the undocumented opcodes

### Added
- **feat(cpu): stable undocumented NMOS 6502 opcodes** (#95), the CPU now
  executes the stable undocumented opcodes real Apple II software relies on,
  SAX, LAX, DCP, ISC, SLO, RLA, SRE, RRA across their addressing modes, plus
  the implied / 2-byte (DOP) / 3-byte (TOP) NOP family, instead of tripping
  the illegal-opcode assert. Each is validated against the Tom Harte
  SingleStepTests (10,000 vectors per opcode) and composes the existing ALU
  primitives so flag and decimal-mode behavior is inherited. The unstable
  "magic constant" opcodes (ANE, LXA, SHA, SHX, SHY, TAS) remain unimplemented
  by design, and the undocumented opcodes are hidden from the assembler so
  `NOP` still assembles to `$EA`.

### Fixed
- **fix(chrome): resize the top-right window corner** (#98), the diagonal
  resize grab is now a larger corner square than the straight edges, so the
  top-right corner is draggable even though the close button sits on it. Every
  Dxui-chromed window (main window + dialogs) is fixed at once.

## [1.10.0]: The one with the //c case-switch strip

### Added
- **feat(machine): //c case-switch strip**: the two latching switches on
  the top of the //c case are now modeled and exposed. The **80/40 switch**
  drives `$C060` (RD80SW) bit 7, pressed in (down) selects 80-column
  startup, out selects 40; it is a software-read switch (a booting disk's
  `PR#3` acts on it), so the bare ROM screen is unaffected, matching real
  hardware. The **keyboard switch** remaps the typed character stream to the
  Dvorak layout while engaged (QWERTY when out); the remap is skipped when the
  host OS layout is itself Dvorak (a Dvorak-host user types normally, so the
  received character is already correct), and paste is never remapped. A
  new skeuomorphic control strip (painted in the //c's platinum case
  color, between the emulator viewport and the joystick/paddle/mouse
  bar, //c-only) reproduces the case top: a **reset** button (inert unless
  Ctrl is held, mirroring the real Control-Reset key; Open/Closed-Apple ride
  it for cold boot), the two latching switches, and lit **disk-use** /
  **power** indicator LEDs. The reset button and switches are drawn as the
  real hardware's right-slanted parallelogram caps, sitting proud of the case
  and depressing below its surface when clicked, an unmistakable pressed cue.
  Both latching switch positions are remembered per machine across runs.

### Changed
- **refactor(shell): harden emulator startup**: `EmulatorShell::Initialize`
  (previously ~600 lines at 9+ levels of nesting) is decomposed into focused,
  single-purpose steps with consistent EHM error handling. Genuine
  infrastructure failures (window, renderer, CPU, and UI-shell bring-up) abort
  startup and assert in debug builds; a corrupt or unreadable settings file
  now recovers to defaults, asserting in debug so a developer can dig in,
  instead of being silently swallowed; and `DiskImageStore` surfaces a failed
  disk-flush through the shared error notifier itself rather than through a
  reporter callback the shell had to wire up.

### Fixed
- **fix(ui): Copy Text on 80-column screens**: Edit ▸ Copy Text now reads the
  interleaved auxiliary + main text banks when the 80-column display is active,
  instead of capturing only every other character.

## [1.9.0]: The one with the write-protect indicator

### Added
- **feat(disk): write-protect indicator**: a write-protected drive now
  shows a small brass padlock on its face (both the skeuomorphic and
  compact drive widgets), and hovering the drive raises a tooltip that
  states the disk is write-protected and names the source(s): the
  write-protect setting, the image's own flag (WOZ), a read-only backing
  file, or missing write permission for the file.

### Fixed
- **fix(disk): honor the write-protect setting**: the Settings ▸ Disk
  "Write protect" checkboxes previously did nothing. They now actually
  protect the mounted disk (the guest sees the write-protect sense bit and
  writes are rejected), the preference is re-applied across ejects/remounts
  and restored on next launch, and a read-only or unwritable backing file
  is likewise treated as write-protected so in-emulator writes can't be
  silently lost.

## [1.8.0]: The one with the //c and the Enhanced //e (spec 016 + #86)

### Added
- **feat(machine): Apple //c**: a new machine profile on the //e substrate,
  scoped to the 5.25"/128K //c with the **Memory Expansion ROM (ROM 4)**
  (managed asset, download-on-demand). Boots real DOS 3.3 / ProDOS media;
  with no disk it reaches the authentic "Check Disk Drive." state.
- **feat(cpu): Rockwell R65C02 core**: full 65C02 instruction set including
  the Rockwell bit ops (`RMB`/`SMB`/`BBR`/`BBS`, used by the //c firmware),
  new addressing modes, and CMOS behavioral fixes (indirect-`JMP` page bug,
  decimal-mode flags/cycles). Validated against Klaus Dormann's 65C02
  functional test and Tom Harte's SingleStepTests.
- **feat(asm): 65C02 assembly (`--cpu 65c02`)**: the built-in as65 assembler
  can now target the 65C02. `--cpu 65c02` unlocks the CMOS opcodes (`STZ`,
  `BRA`, `TSB`/`TRB`, `RMB`/`SMB`/`BBR`/`BBS`, the `(zp)` and `(abs,X)` modes);
  the default stays a strict 6502, so 65C02-only opcodes are rejected unless
  requested. The Rockwell bit ops accept both as65's `<bit>,<zp>[,<target>]`
  operand form and the suffixed spelling (`RMB0 $zp`, `BBR3 $zp,target`), and
  the `.a65c` extension is recognized. `NOP` assembles to the canonical `$EA`.
- **feat(machine): slotless phantom-slot firmware map**: the //c's built-in
  peripherals answer at their fixed firmware pages (serial 1/2, 80-column,
  disk at $C600, mouse at $C700 on ROM 4) through the internal 32K
  bank-switched ROM ($C028 bank flip-flop); no user-insertable slots.
- **feat(disk): IWM mode**: the built-in slot-6 drive is an Integrated Woz
  Machine over the shared WOZ nibble engine (MODE/STATUS registers; reads,
  writes, and boot verified end-to-end), plus an optional **external drive**
  with a Connected / Not connected toggle on the Machine tab.
- **feat(serial): dual 6551 ACIA serial ports** (port 1 printer, port 2
  modem) with loopback/file endpoints; one shared ACIA implementation also
  consumed by the printer feature. (Serial *printing* lands via issue #87.)
- **feat(mouse): //c IOU mouse**: full hardware model (X0/Y0 movement
  interrupts with $C048 acknowledge, $C015/$C017 status, VBL latch at
  $C019 cleared by $C070, IOUDIS-gated $C058-$C05F programming) driven by
  the REAL ROM 4 mouse firmware; the host pointer maps absolutely onto the
  guest mouse (non-capturing) while over the emulator viewport. The mouse
  is a connectable peripheral (Machine-tab toggle, default connected).
- **feat(input): split input model + device selector**: input mapping is
  now two orthogonal selections (Keys: arrows→joystick; Pointer: paddle or
  mouse), replacing the single cycle mode, with a new segmented drive-bar
  selector drawing skeuomorphic glyphs of the real Apple peripherals
  (perspective on the skeuomorphic theme, top-down on DarkModern/retro).
- **feat(cpu): live hardware IRQ dispatch**: the CPU loop now services
  maskable interrupts from the shared interrupt controller (the //c mouse
  and VBL are the first sources); the 65C02 vector prologue is accounted
  like an instruction, leaving existing machines byte-identical.
- **feat(machine): Apple //e Enhanced** (issue #86), the //e with a 65C02
  and the enhanced firmware + MouseText video ROM, so it runs the CMOS
  titles that misbehave on the NMOS //e. Reuses the //e substrate (128K,
  slot-4 Mockingboard, slot-6 Disk ][); the enhanced ROM is a managed
  download-on-demand asset. Auto-discovered by the machine picker; cold-boots
  its firmware on the 65C02 (headless boot test).

## [1.7.0]: The one with the Mockingboard (GH #66)

### Added
- **feat(emu): Mockingboard A/C sound card emulation.** Adds the de-facto
  Apple II audio standard: two clean-room chip cores written from the
  datasheets, a generic **6522 VIA** (full register file, Timer 1
  one-shot/continuous, Timer 2, IFR/IER and a level-sensitive IRQ line,
  ports A/B with data-direction registers) and an **AY-3-8910 PSG** (three
  square-wave tone channels, a 17-bit-LFSR noise generator, a 16-level
  envelope generator, and a logarithmic DAC rendered to float32 PCM
  resampled to the host rate). A **MockingboardCard** aggregates two of
  each in a slot's `$Cn00` I/O page, decoding address bit 7 to the two VIAs
  and translating each VIA's port A/B into the AY data bus and BC1/BDIR/RESET
  control lines. Timer 1 IRQs, the tempo source Mockingboard music players
  rely on, feed the existing interrupt controller, and the two PSGs mix
  into the stereo bus (PSG #1 hard-left, PSG #2 hard-right) through a
  dedicated audio mixer so they sum cleanly with the speaker and Disk II
  audio. The card is installed in slot 4 of the Apple ][+ and //e profiles
  and is enabled or removed from its slot in the **Hardware** tab's device
  list (the card's slot entry is the single Mockingboard control).
  On the //e the `CxxxRomRouter` now delegates a slot's `$Cn00` page to an
  active I/O card when `INTCXROM=0`, so the card is reachable behind the
  MMU. Clean-room throughout: no GPL emulator code was copied.

### Fixed
- **fix(cpu): the emulator run loop now services maskable interrupts.**
  The shell drove the CPU exclusively through the interrupt-blind
  `StepOne`, so no device IRQ was ever dispatched, latent until the
  Mockingboard became the first device in Casso to assert one. Music
  players (e.g. *Zaxxon*) arm a Timer 1 IRQ and depend on its handler
  firing; without dispatch they stayed silent. Each step site now
  dispatches a pending NMI/IRQ before executing the next instruction.

## [1.6.4]: The one with the virtualized debug panel (GH #88)

### Fixed
- **fix(ui): the Disk ][ Debug panel no longer wedges under disk-heavy
  activity (GH #88).** Every render frame rebuilt and re-materialized the
  entire event list into the `DxuiListView` (up to 100,000 rows, each six
  freshly-heap-allocated `std::wstring` cells) even when only a handful of
  events were visible and nothing had changed. Under a `SAVE` or a Print
  Shop print (thousands of head-step / address-mark events per second) the
  per-frame allocation storm starved the UI thread and the app appeared to
  hang. The list now uses a **virtual (provider) row model**: it holds no
  materialized rows, tracks only a count, and pulls cells for just the
  visible window (`O(visible)` per frame instead of `O(total)`). A
  per-frame **change-gate** skips the filter/sort rebuild entirely on idle
  frames (no new events), and columns auto-fit from the visible window as
  rows scroll in.

### Changed
- **Selection and keyboard focus in the Disk ][ Debug panel now track the
  selected event by a stable monotonic identity (`seq`) rather than by row
  or deque index.** A column re-sort keeps the same event selected *and*
  scrolls it back into view, and a selection that is filtered out or
  evicted from the rolling history snaps to the nearest surviving event
  instead of silently jumping. The resolution logic is a pure, headlessly
  unit-tested helper (`DebugDialogProjection::ResolveSelection`).

## [1.6.3]: The one where disk writes persist

Follows GH #89 (which fixed the emulated write *bit*) by fixing the
*persistence* layer that #89 never touched, the path from the in-memory
track bit streams back to the host file.

### Fixed
- **fix(disk): WOZ writes now persist (write-back was never implemented).**
  `DiskImage::Serialize`'s WOZ arm returned the untouched *source* bytes and
  ignored every guest write, so edits to a writable `.woz` were silently
  discarded on flush. A new `WozLoader::Serialize` emits a valid WOZ v2 image
  (INFO + TMAP + TRKS + block-aligned bit streams, correct header CRC32) from
  the live per-track buffers, preserving the write-protect flag. Guest writes
  now round-trip; WOZ becomes the reliable writable format (raw bit stream, no
  lossy sector re-encode). This is also the serializer 017 (blank-disk
  creation) needs.
- **fix(disk): disk-flush failures are no longer silently swallowed.** Every
  flush caller dropped `FlushEntry`'s `HRESULT` (`Eject`/`PowerCycle` are
  void; the exit path and `SoftReset` `IGNORE_RETURN_VALUE` it), so a failed
  write-back vanished with no error and the user lost writes. `DiskImageStore`
  now invokes an injected flush-error reporter on a genuine failure to persist
  a dirty image; the shell logs it and warns the user via the shared EHM
  notifier.

### Added
- **feat(disk): motor-idle auto-flush.** Dirty disk images are now persisted
  when the drive motor spins down (a naturally debounced, ~1s-after-last-access
  "operation complete" boundary), so writes survive a crash/kill before the
  next eject or exit. The flush fires on the CPU thread inside
  `Disk2Controller::Tick`, the thread that owns the disk writes, so it races
  nothing, and skips clean images.
- **feat(disk): the disk picker now lists sibling images from recent-disk
  folders.** Both the boot-time and runtime insert pickers previously showed
  only disks that were already in the recent-disks MRU (plus bundled demos), so
  a freshly created or copied image sitting right next to a recent disk was
  invisible until it had been mounted once via Browse. The pickers now also
  enumerate every folder that contains a recent disk and offer all supported
  images found there (`.dsk`/`.do`/`.po`/`.nib`/`.woz`), de-duplicated against
  the MRU and excluding disks from other repo checkouts.
  `DiskMru::DistinctFolders` computes the folder set (pure/unit-tested);
  `AssetBootstrap::AppendSiblingDisksFromMruFolders` does the scan.

### Changed
- **The recent-disks prune is now scoped to the running *checkout*, not just
  foreign worktrees (dev builds only).** `IsForeignWorktreeDisk` →
  `IsForeignCheckoutDisk`: the shared `%LOCALAPPDATA%` MRU is populated by
  every checkout of the repo on the machine (the main tree plus each
  `.claude/worktrees/<name>` copy), so the picker now hides recent disks that
  live in *any* checkout other than the one the running exe belongs to,
  including the **main tree** when running from a worktree, which the old rule
  let through. Disks outside the repo entirely (the user's own folders,
  `%LOCALAPPDATA%`) always show, so this is invisible to installed users (no
  worktrees). The classification moved to a pure, unit-tested `RepoCheckout.h`.

### Notes
- Confirmed the `.dsk` write-back round-trips a full reformat: `Denibblize`
  scans for GCR address/data markers (byte-sync), so a standard DOS 3.3 format
  survives regardless of gap/sync layout. The earlier "initialized data disk
  still shows the old files" symptom was flush *timing*, now addressed by the
  motor-idle auto-flush above.

## [1.6.2]: The one where disk writes round-trip (GH #89)

### Fixed
- **fix(disk): writes to `.dsk` images now round-trip (GH #89).** DOS 3.3
  `SAVE` returned `I/O ERROR` and Print Shop ground forever because the
  Logic State Sequencer sampled the *sequencer state* bit (`state & 0x8`)
  as the outgoing write-data line. On real hardware that bit tracks the
  write shift-register MSB only because the P6 sequencer and the register
  are clocked in lockstep by the 2 MHz Q3; a cycle-stepped emulator that
  catches the LSS up in bursts at each soft-switch access cannot hold that
  sub-clock lockstep, so the sampled bit desynced and deposited ~`AA`
  garbage where `FF` sync belongs, corrupting every data field DOS wrote.
  The write bit is now sourced straight from the shift-register MSB
  (`Disk2NibbleEngine::StepLss`), the physically correct write-head signal,
  which is robust to catch-up granularity. Address fields were never
  rewritten by DOS so they survived, which is why reads (CATALOG) worked
  while writes silently corrupted the disk.
- Added hermetic write→read-back regression gates: an engine-level
  `FF`-sync round trip, a CPU-driven known-nibble round trip through the
  raw bitstream, and an end-to-end DOS 3.3 `SAVE`/`LOAD`/`LIST` round trip
, closing the bit-exact write-fidelity gap deferred in #67.

## [1.6.1]: The one with the keyboard accelerator fixes

Emulator keyboard shortcuts moved off plain `Ctrl+<letter>` so they stop
stealing valid //e control keystrokes from the running software.

### Fixed
- **fix(input): emulator accelerators no longer swallow //e `Ctrl+<letter>`
  keys.** The joystick-mode toggle, Reset, and Power cycle used plain
  `Ctrl+<letter>` chords, which are valid //e keystrokes the guest reads,
  so, e.g., Rocky's Boots never saw its Ctrl+I/J/K/M fine-movement keys.
  They now use Ctrl+Shift: **Ctrl+J → Ctrl+Shift+J** (joystick),
  **Ctrl+R → Ctrl+Shift+R** (Reset), and Power cycle **Ctrl+Shift+R →
  Ctrl+Shift+P**. Also removed a dead Ctrl+O binding (fired a no-op
  machine-picker stub) and a phantom "Ctrl+Alt+R autoboot reset" keymap
  entry that was never implemented.

### Added
- **feat(demos): Rocky's Boots and Where in the World is Carmen Sandiego.**
  Added to the source-checkout demo disks listed in the picker (Carmen as
  its side A / side B flip-disk pair).

## [1.6.0]: The one with the disk picker and Dxui (spec 013)

The boot / Insert-Disk picker gained search and sort and is preloaded with
the repo's demo disks; Settings added an "Apply now" theme, a
restart-required notice, and support for a machine with no Disk ][
controller. Under all of it, the window chrome was extracted into a
standalone, reusable **Dxui** library, with the window host owning the
Direct3D swap chain directly.

### Added
- **feat(settings): restart-required notice + "OK (reboot)" button.**
  Staging a machine switch (Machine tab) or toggling hardware on the
  Hardware tab now shows an amber notice next to the OK button naming the
  pending change, and the OK button itself relabels to "OK (reboot)" so
  it's clear that committing will power-cycle the machine. The notice
  clears if you revert the change.
- **feat(settings): "Apply now" button on the Theme page.** A theme pick
  used to take effect only when you clicked OK. There is now an "Apply
  now" button beside the theme dropdown that reskins the running chrome
  immediately without closing Settings, so you can try a theme live.
  Clicking Cancel afterward reverts to the theme you started with; OK
  keeps it.
- **feat(ui): searchable, sortable, keyboard-navigable disk picker with a
  last-loaded date column.** The boot and Insert-Disk pickers gain a search
  box, type to filter recent disks (case-insensitive; space-separated
  terms must all match, across name, location, and date), with the matched
  text highlighted in each row; the magnifier glyph slides away on focus
  and an X button clears the field. The leading "Last loaded" column shows
  each disk's load time in the user's regional date/time format and is the
  default sort (newest on top). Columns auto-fit to their widest value (the
  header and its sort arrow included), are resizable (drag a header
  divider), and clickable to sort (re-click to reverse; strings A–Z, dates
  newest-first) with an up/down indicator on the active column. The picker
  is fully keyboard-navigable: Tab walks the search box → list body (arrows
  move the highlight, Enter/Space mounts) → column header (arrows move
  between columns, Enter/Space sorts or reverses) → the dialog buttons. The
  dialog is resizable (drag any edge; it opens at a sensible size clamped to
  your monitor) and the list shows horizontal and vertical scrollbars as
  needed, Shift+wheel or the bottom scrollbar reaches a long location path.
- **feat(settings): optional Disk ][ controller.** A machine can now run
  with no Disk ][ controller. Settings shows the **Disk** tab only when a
  controller is present (the old Hardware tab folds into the Machine tab),
  and when there is none the bottom drive band is hidden and its space
  reclaimed, boot skips the disk step, and drive-sound preview still works.
- **feat(ui): auto-fit debug-panel list columns.** The Disk II and Input
  debug panels size each event-list column to the widest value seen, so
  large cycle counts no longer wrap; dragging a column divider still
  pins that column to the user's width.
- **feat(ui): list in-repo demo disks in the disk picker.** When Casso
  runs from a source checkout, the boot and Insert-Disk pickers now list
  the disk images under `Apple2/Demos/` (Choplifter, Karateka, Lode
  Runner, …) as directly-mountable rows alongside recent disks and stock
  downloads. No-op in an installed build.
- **feat(dxui): automatic tab order for interactive widgets.** Buttons,
  checkboxes, radios, sliders, toggles, dropdowns, tabs, text inputs,
  search boxes, tree views, list views, and icon buttons now opt into the
  Dxui focus tree walk; icon buttons also draw a themed focus ring and
  activate from Space / Enter.

### Changed
- **refactor(dxui): extract the window chrome into a reusable Dxui
  library.** Casso's panels, layouts, widgets, menu bar, popup host, and
  dialog primitives moved into a standalone `Dxui` static library built
  on Direct2D / DirectWrite, decoupled from the emulator shell.
- **refactor(render): host-owned swap chain.** The window host now owns
  the Direct3D swap chain and presents directly; the separate child
  render-surface window is gone, so a single window procedure owns all
  painting and input.
- **feat(ui): scroll long disk-image names on hover.** A mounted disk's
  basename that overflows the drive label used to truncate with an
  ellipsis; the label now scrolls the full name once when the drive is
  hovered (clipped to the drive bounds), then rests at the head, so long
  filenames can be read on demand.
- **refactor(theme): unified theme contract across every Dxui-rendered
  control.** Checkboxes, radio buttons, dropdowns, tooltips, sliders, and
  toggles all paint from the active theme, so the Skeuomorphic, Dark, and
  Retro Terminal presets apply consistently throughout the chrome, the
  Settings window, and both debug panels (no more off-theme blue-gray
  controls in the green Retro Terminal preset).
- **feat(settings): pressed-state feedback on the settings tabs.** A
  settings tab now darkens while the mouse button is held on it, matching
  the press feedback the page's buttons, dropdowns, sliders, and toggles
  already give.
- **perf(shell): the disk picker opens promptly instead of after a
  visible pause.** Clicking a drive door no longer blocks on the
  drive-door open animation before showing the picker when the door is
  already open (an empty drive rests open), removing a ~350 ms dead wait
  on the common case. The picker's pre-dialog work is also trimmed: the
  bundled-demo directory is located once per process (its contents are
  still enumerated each open so newly added demos appear), and the
  stock-master dedup compares disk images a block at a time and stops at
  the first difference instead of reading each image in full.

### Fixed
- **fix(shell): machine switch no longer trips the UI-thread guard.**
  `SwitchMachine` runs on the CPU thread and refreshed the window title
  there, but `DxuiHwndSource::SetTitle` mutates the caption bar and is
  UI-thread-only, so a machine switch / hardware-reset reboot asserted in
  debug (and raced the caption in release). `UpdateWindowTitle` now
  marshals the refresh onto the UI message loop when called off-thread.
- **fix(input): show host joystick and paddle movement in the Input Debug
  panel.** Arrow-key / X / Z joystick updates and trackpad paddle movement
  now log immediately, even before the guest program reads the game port.
- **fix(ui): debug-panel window-management polish.** The maximize glyph
  toggles to the restore glyph when a panel is maximized, re-pressing a
  panel's hotkey restores and foregrounds it when minimized, and the
  panels show the Casso icon in Alt-Tab.
- **fix(ui): enlarge the About dialog app picture.** The photoreal app
  image now renders at a prominent size instead of a small thumbnail.
- **fix(disk): suppress reset-time drive-door audio.** Warm reset and
  programmatic remounts no longer play the Disk II door-close sound for
  already-mounted disks.
- **fix(ui): remember startup-mounted disks.** A disk mounted at startup
  (the boot-disk picker result or `--disk1` / `--disk2`) is now recorded
  in the recent-disks MRU, so it appears under recent disks on the next
  launch instead of the picker reporting "no recent disks".
- **fix(ui): label already-downloaded stock disks "Installed".** A stock
  master already present on disk now shows "Installed" in the picker
  rather than "Asimov archive (Download)"; selecting it mounts the local
  copy without re-downloading.

## [1.5.1566]: The one with the drive-audio mixer

### Added
- **feat(audio): per-drive volume, stereo pan, and sound audition.** The
  Settings → Machine tab gains Motor / Head / Door volume sliders
  (defaulting to 90 / 100 / 100 %) and an independent Left…Center…Right pan
  slider per drive (defaults −0.5 / +0.5, equal-power placement), so the two
  floppy drives sit on opposite sides of the stereo image. A play button
  beside each control auditions that sound at the dialed level (volume
  previews play balanced at center, pan previews at the dialed position,
  both using the currently-selected mechanism) and a **Reset** button
  restores the audio defaults. Live changes marshal to the CPU/mixing
  thread (`IDM_AUDIO_DRIVE_VOLUMES` / `_PAN` / `_TEST`). The Alps mechanism,
  which ships no door sample, now falls back to the Shugart door sound
  instead of going silent.
- **feat(settings): custom text-color picker.** The Color-monitor text color
  can now be an arbitrary RGB value chosen in a themed HSV picker (hue /
  saturation / value sliders, a live preview swatch, and a hex entry)
  launched from the Display tab; the hex field has a copy-to-clipboard icon.
- **feat(ui): Windows-style text input.** `TextInput` was rewritten to behave
  like a native edit control, a correctly sized blinking caret, arrow /
  Ctrl+arrow / Home / End movement, Shift / Ctrl+Shift selection, mouse
  click-drag and double-click word selection, and clipboard cut / copy /
  paste.

### Changed
- **feat(theme): settings widgets follow the active theme.** Sliders,
  toggles, dropdowns, the tab strip, buttons, and the joystick LED in the
  Settings panel now derive every color from the active ChromeTheme (e.g.
  green under Retro Terminal) instead of a fixed blue. The `Button` widget no
  longer exposes free-form color overrides, a themed Default / Primary
  variant makes it impossible to draw a button that ignores the theme.
  Slider pucks and toggle pills are darkened to keep ≥3:1 contrast (WCAG
  1.4.11) against their white sub-elements, and the primary (OK) button
  clears 4.5:1 (WCAG 1.4.3) against its white label.

### Fixed
- **fix(audio): a machine set to Alps booted playing Shugart.** The boot path
  read the persisted lower-case mechanism token (`alps` / `shugart`) and
  compared it against the mixed-case sample-directory names with `==`, so
  the token failed validation and the mixer stayed on its Shugart default
  until the setting was re-applied. `DriveAudioMixer` now matches mechanism
  names case-insensitively and stores the canonical mixed-case form, so the
  selected mechanism is applied at startup.

## [1.5.1555]: The one with the ][ plus game port

### Added
- **feat(machine): Apple ][ / ][ plus game port (paddles, buttons, trigger).**
  The original Apple ][ / ][ plus had no game-port emulation (nothing mapped the
  pushbuttons (`$C061`–`$C063`), analog paddles (`$C064`–`$C067`), or the
  PTRIG strobe (`$C070`)) so paddle/joystick games (e.g. *Space Quarks*,
  Brøderbund 1981) had dead controls on those machines. A new
  `apple2-gameport` device models all three, using the same 558 one-shot
  paddle timer as the //e (≈11 CPU cycles per paddle unit). The existing
  **Map Arrows to Joystick** mode now drives the ][ / ][ plus game port too
  (arrows → paddle 0/1, X / Z / Alt → buttons 0 / 1), and the Input Debug
  panel logs its reads. Added to the Apple ][ and ][ plus machine configs
  (`$cassoMachineVersion` 6 → 7; existing on-disk copies upgrade
  automatically).
- **feat(debug): `--trace` execution-trace switch.** `--trace [N]` records
  the last N executed instructions (default 20,000,000; size with a plain
  count or a `20M` / `2G` suffix) in a runtime-gated ring and dumps them (
  PC, opcode, operand bytes, mnemonic, and full register state) to a
  timestamped `casso-trace-*.txt` in the working directory on a clean exit
  or a crash, with a progress dialog during the write.

### Fixed
- **fix(video): inverse text on ][ / ][ plus was invisible.** The 2 KB Apple ][ / ][ plus
  video ROM stores the same normal glyph in the low 7 bits of all four
  64-character ranges (bit 7 is only a range marker, not a per-glyph invert
  flag), but `Decode2K` XOR-inverted the `$00`–`$3F` range while the renderer
  *also* inverts it, so inverse characters rendered identically to normal
  text, hiding inverse menu highlights (as seen on the *Space Quarks* options
  screen). `Decode2K` now bit-reverses the low 7 bits with no conditional
  invert. The prior inverse-space test passed only because a blank glyph is
  symmetric; a regression test now renders an inverse *letter*.
- **fix(disk): non-ASCII disk filenames corrupted in paths.** Disk paths were
  shuttled between `std::wstring` and ANSI-narrowed `std::string`, mangling
  any non-ASCII filename (e.g. the *ø* in "Brøderbund"). The boot-disk MRU
  silently dropped such entries (its `exists` prune saw an invalid path), the
  remembered last-disk failed to auto-load, and the drive-widget filename
  label rendered a tofu box (a sign-extended `0xF8` → `U+FFF8`). Path ↔ string
  conversions now use UTF-8 throughout.
- **fix(trace): `--trace` progress dialog rendering.** The progress dialog
  selected no font (GDI fell back to the fixed bitmap System font), never
  erased between repaints (so progress values overlapped into mush), and used
  unscaled pixel metrics (clipping the progress bar at high DPI). It now
  selects the OS themed message font at the window DPI, erases each repaint,
  sizes the window and all metrics for the monitor DPI, and gives the bar a
  bottom margin matching the top.
- **fix(cpu): debug trace records the actually-fetched opcode.** The
  fault/trace ring read raw `memory[PC]` instead of the bus-routed fetch, so
  it misreported instructions in banked regions (Language Card / ROM). It now
  logs the byte the CPU actually executed.

## [1.5.1526]: The one with Space Quarks' quarky opcodes

### Added
- **feat(cpu): NMOS 6502 undocumented opcodes DOP and DCP.** Implements
  `$04` (DOP zp (double NOP, reads and discards a zero-page byte, 3 cycles)
  and `$CF` (DCP abs) decrement memory then CMP A with the result, 6 cycles).
  Real Apple ][ / ][ plus software such as *Space Quarks* (Brøderbund, 1981) relies on
  these undocumented but consistently-behaving NMOS opcodes; Casso previously
  asserted on them in debug builds and silently mishandled them in release.

### Fixed
- **fix(ui): correct dialog body text overlap on scaled displays.**
  The dialog computed its layout using the owner window's DPI. At startup
  the owner is the desktop window, which is system-DPI aware and reports the
  system DPI (e.g. 96) rather than the dialog's per-monitor DPI (e.g. 144 on
  a 150% display), so even with a single monitor the two can differ. The
  body paragraph was therefore measured and wrapped at one scale while its
  text rendered at another, leaving each wrapped line's cell too narrow for
  the rendered glyphs; DWrite re-wrapped the text inside the cell, spilling
  lines on top of one another. The dialog now re-syncs its layout (and window
  size) to the window's actual per-monitor DPI once the window exists, the
  same way it already handles `WM_DPICHANGED`.
- **fix(ui): render dialog text via an offscreen D2D composite.** Dialog
  rendering interleaved D3D and Direct2D on the same swap-chain surface:
  `m_painter.End()` issued a D3D draw while D2D's `BeginDraw` was still active,
  triggering an implicit mid-frame D2D flush followed by a second flush at
  `EndDraw`, and asserting in debug builds (`CBRA(m_drawing)`). The dialog now
  renders text to an offscreen D2D bitmap and composites it onto the back
  buffer after the geometry pass, the same pattern the debug panels use, so
  the D3D and D2D pipelines never interleave on the swap-chain surface.

### Changed
- **feat(ui): consolidate first-launch boot-disk prompts into one picker.**
  First launch previously showed two overlapping dialogs: the unified asset
  downloader (which offered boot disks among ROMs/audio) and then the legacy
  boot-disk picker, so the user was asked about boot disks twice, and the
  picker even re-offered a download for a master that was already installed.
  The startup downloader no longer handles boot disks; it appears only when
  required ROMs or optional drive audio are missing. Boot-disk selection is
  now owned solely by the picker, which appears only when no disk is mounted
  and now lists stock masters already present on disk as mountable rows
  ("Installed") alongside download rows for the ones that are absent. The
  runtime Insert-Disk picker gains the same present-master rows.

## [1.5.1523]: The one with the game input revamp

Authentic //e keyboard handling that makes real-time action games
playable, a keyboard-mapped game-port joystick with an on-screen
toggle, a new Input Debug panel, a themed native Disk II debug
window, and a batch of disk/render correctness fixes. Validated by
booting and playing *Karateka* end-to-end from its unmodified,
copy-protected WOZ image ([#68](https://github.com/relmer/Casso/issues/68)).

### Added
- **feat(input): authentic //e keyboard auto-repeat.** The core
  keyboard now generates hardware-faithful auto-repeat (initial delay
  then steady cadence) instead of relying on host OS key repeat, so
  timing-sensitive games behave correctly, *Karateka* movement on the
  left/right arrow keys plays as it did on real hardware.
- **feat(input): Map Arrows to Joystick mode.** An optional input mode
  that drives the emulated game port from the host keyboard so joystick
  games are playable without a physical controller: the arrow keys are
  mapped to paddle 0/1 (last-pressed-wins on opposing keys) and the **X**
  and **Z** keys act as buttons 0/1 (the same Open-Apple `$C061` /
  Closed-Apple `$C062` soft-switches the host Alt keys drive, so both
  input sources coexist). While the mode is on, the arrow and X/Z keys
  are not sent as standard keyboard input via the //e keyboard latch so
  they can't also type. Toggling the mode resolves axes and buttons from
  the live key state and recenters/releases them on exit.
  Three ways to flip it: **Machine → Map Arrows to Joystick**, the
  **Ctrl+J** accelerator, or a dedicated **Joystick Mode** toggle button
  in the bottom drive bar, a frameless press-to-pin button with a blue
  glowing LED, a hover tooltip, and a focus ring; all three paths share
  one choke point so the button LED, menu checkmark, and held-key
  neutralization stay in sync.
- **feat(ui): keyboard chrome focus ring.** Press **F10** to enter the
  painted chrome with the keyboard. **Tab / Shift+Tab** cycle across the
  seven menu titles, the Joystick Mode button, and the two drive
  widgets; **Enter / Space / Down** open a dropdown or activate the
  focused button/drive; **Esc / F10** return focus to the //e. While the
  ring (or a dropdown) owns focus, no `WM_KEYDOWN` or `WM_CHAR` leaks to
  the emulated keyboard, so navigating chrome can't drop stray letters
  into a //e prompt. Mouse clicks on chrome elements update the ring;
  clicking the emulator viewport drops it.
- **feat(ui): Input Debug panel.** New themed, non-modal debug window
  logging host→//e key events, the `$C000`/`$C010` keyboard strobe,
  Open/Closed-Apple button state, and synthesized joystick/paddle reads
  (`$C064`–`$C067` PREAD, `$C070` PTRIG), with per-lane filter checkboxes
  (emulator keyboard/joystick/paddle, host keyboard) and per-pair
  Joystick-vs-Paddle view dropdowns, column sorting, pause, and a Copy
  button that puts the visible log on the clipboard as tab-separated text.
  Fed by a lock-free event ring drained on the render thread.
- **feat(ui): native Disk II debug window.** DX/themed replacement for
  the legacy Win32 Disk II debug dialog, sharing the same event-ring
  projection and multi-column ListView.

### Fixed
- **fix(disk): keep the nibble engine advancing after a power cycle.**
  A power cycle zeroes the CPU cycle counter, but the Disk II
  controller's `CatchUpToCpu` retained a stale sync anchor and froze the
  bit cursor for as long as the prior session's uptime, a minute-plus
  boot hang when rebooting a machine that had been running a while.
  `CatchUpToCpu` now re-anchors whenever the counter moves backward.
- **fix(ui): render tall debug panels via an offscreen D2D composite.**
  Debug panels above a threshold height stopped painting their text;
  text now renders to an offscreen target that is composited into the
  panel, so content paints at any height.
- **fix(debug-panel): marshal the reset clear onto the render thread.**
  Warm reset / power cycle run on the CPU thread and cleared the debug
  panels' event deque and ListView rows directly, racing the render
  thread's per-frame drain and double-freeing the row strings (a
  heap-corruption assert). The clear is now staged behind an atomic flag
  and serviced on the render thread, keeping that state single-threaded.

## [1.5.1405]: The one with the disk-insert picker polish

Field-test fixes for the themed disk-insert MRU picker and the
underlying dialog primitives.

### Fixed
- **fix(picker): match stock-disk aliases by content for friendly
  labeling.** Stock disks that lived under a different filename
  (e.g. `dos33-master.dsk` next to the canonical `DOS 3.3 System
  Master.dsk`) still rendered with their raw basename, since the prior
  fix only matched MRU entries via `fs::equivalent` (same physical
  file). Added a size-then-`memcmp` fallback so byte-identical aliases
  at different paths also inherit the friendly label. Both copies
  remain visible in the MRU; the download row is suppressed only when
  some MRU entry matches.
- **fix(picker): show friendly label for stock MRU entries.** After
  the dup-suppression fix, a stock disk in the MRU still rendered as
  its raw filename (`DOS 3.3 System Master.dsk`) because the friendly
  label only lived on the now-suppressed download row. Pickers now
  carry the label across: MRU entries that match a stock-download
  target render as `DOS 3.3` / `ProDOS`.
- **fix(picker): suppressed duplicate DOS 3.3 / ProDOS rows.** The
  boot and runtime MRU pickers always appended both stock-download
  rows even when the canonical disk file was already in the MRU,
  producing two entries that mounted the same file. Now skip the
  download row when its target path matches an MRU entry (compared
  via `fs::equivalent`).
- **fix(picker): disk inserted into wrong drive.** Picking a disk in
  Drive 1 (widget click → MRU picker → Browse → file dialog) was
  mounting it into Drive 2. `WindowCommandManager::PromptInsertDiskMru`
  and `PromptForDiskImage` were forwarding the 1-indexed display drive
  number straight to `Mount`, which expects 0-indexed (drive `1` then
  maps to `IDM_DISK_INSERT2`). Subtract 1 at the `Mount` call sites.
- **fix(picker): boot-disk row index collided with `IDCANCEL`.**
  `PromptBootDiskMru` used `IDCANCEL` (= 2) as the Skip button ID, so
  once the MRU grew to ≥ 3 rows a click on row index 2 would have
  been mis-classified as Skip. Switched to a negative sentinel
  (`s_kSkipResult = -2002`), mirroring `PromptInsertDiskMru`. Rows
  keep their natural `0..N-1` indexing; buttons live in non-overlapping
  negative space.
- **fix(widget): `ListView` empty body when `SetRows` precedes
  `SetRect`.** Sticky-tail clamp in `SetRows` pinned `m_topRow` to
  `rows.size()` because `VisibleRowCapacity()` returned 0 against an
  empty rect, so the paint loop drew zero data rows. `SetRect` now
  re-clamps `m_topRow` once the real rect is known.
- **fix(dialog): button border invisible when colors overridden.**
  `Button::Paint` only drew the 1dip auto-border in the
  `!m_useOverrides` branch, and `DialogPrimitive::BuildButtons` was
  forcing overrides with colors close to the dialog body. Border now
  paints whenever `borderColor != 0`, and `BuildButtons` lets theme
  defaults apply.
- **fix(dialog): `&` accelerator marker.** `Button::SetLabel` now
  strips the marker and captures the accelerator; `DialogPrimitive`
  handles `WM_SYSCHAR` to dispatch Alt+letter to the matching
  button.
- **fix(dialog): draggable title bar on `WS_POPUP` dialogs.**
  `DialogPrimitive::OnMouse` forwards in-title clicks (outside the
  close box) as `WM_NCLBUTTONDOWN HTCAPTION`.
- **fix(picker): drive number is 1-indexed.** `BrowseForDisk` now
  passes `drive + 1` to `PromptInsertDiskMru` so the title reads
  "Insert Disk, Drive 1/2" and the mount call gets the right slot.

## [1.5.1398]: The one with the themed MRU picker

The runtime disk-insert flow (Disk → Insert Disk Image, drive-widget
click, or `IDM_DISK_INSERT1`/`2`) now opens the themed MRU picker
instead of jumping straight to `IFileOpenDialog`. Lists the same
existing-on-disk recents as the boot picker plus the always-available
DOS 3.3 / ProDOS download rows. A **Browse...** button preserves the
native `IFileOpenDialog` path for off-MRU images.

### Added
- **feat(011): `AssetBootstrap::PromptInsertDiskMru`.** Sibling to the
  boot-time `PromptBootDiskMru`, same `ListView`-based DialogPrimitive
  surface, theme-aware, but titled per drive and wired with a Browse
  fallback instead of Skip. Uses out-of-range negative sentinel result
  codes to avoid colliding with row indices.
- **feat(011): `WindowCommandManager::PromptInsertDiskMru`.** Loads MRU
  from `GlobalUserPrefs::recentDisks`, prunes vanished files, invokes
  the themed picker, and routes the result: chosen row →
  `EmulatorShell::Mount(6, drive, path)`; Browse → existing
  `PromptForDiskImage` (`IFileOpenDialog`); Cancel/Esc/close → no-op.

### Changed
- **refactor(011): `OnDiskCommand` IDM_DISK_INSERT1/2** now invoke
  `PromptInsertDiskMru` instead of `PromptForDiskImage` directly.

## [1.5.1395]: The one that went native on dialogs (spec 011)

Themed DX-based modal dialogs now replace every Win32 `MessageBoxW` /
`TaskDialogIndirect` consumer in the app (except the pre-shell EHM
notification fallback in `Main.cpp`). The bootstrap-time prompts, the
in-app Help/About/Keymap/Machine Info dialogs, and the SettingsPanel
ROM-error notification all paint through the new `DialogPrimitive` and
honor the active chrome theme. The `IFileOpenDialog`-based disk picker
is preserved as the lone deliberate Win32 surface.

### Added
- **feat(011): DialogPrimitive modal window (T006–T009).** New
  `Casso/Ui/Dialog/DialogPrimitive` pair implements the themed blocking
  modal dialog. `RegisterClass` registers the Win32 class once per
  instance; `Show()` creates the window, runs a private `GetMessage`
  loop (disabling the owner window for the duration), and returns the
  chosen button's `resultCode` (or -1 on Alt+F4 / WM_CLOSE). Keyboard:
  Enter = default button, Escape = cancel button, Tab/Shift-Tab cycle
  focus. Hyperlinks launch via `ShellExecuteW`.
- **feat(011): DialogPrimitiveRenderer.** Split renderer class owns the
  `CreateSwapChainForHwnd` swap chain (DXGI_ALPHA_MODE_IGNORE, no DComp
  / no blur), RTV, `DxUiPainter` (geometry), and `DwriteTextRenderer`
  (text). Paints a gradient title bar, solid dialog background with
  theme colors, icon circle (Info / Warning / Error / App), word-wrapped
  body text, hyperlink underlines, optional custom-body callback, and
  `Button` widgets. DPI-aware: recomputes layout on WM_DPICHANGED.
- **feat(011): DialogDefinition + DialogLayout primitives.** Pure value
  types and headless `LayoutDialog` free function; all measurement is
  injected so the math is unit-tested without DirectWrite.
- **feat(011): StandaloneDialog wrapper.** Bootstrap-friendly helper
  that spins up a transient `D3D11CreateDevice (HARDWARE)` + one-shot
  `DialogPrimitive` for callers that fire before `EmulatorShell` exists
  (e.g. `AssetBootstrap`'s missing-asset prompts). The caller passes
  `GlobalUserPrefs::activeTheme` so startup dialogs honor the user's
  persisted `ChromeTheme` choice (`Skeuomorphic` / `DarkModern` /
  `RetroTerminal`) instead of always painting Skeuomorphic.
- **feat(011): themed startup prompts.** `AssetBootstrap::PromptUser`
  (missing-asset download approval, now with clickable URL hyperlink),
  `PromptBootDisk` (DOS 3.3 / ProDOS / Skip), and
  `PromptDiskAudioConsent` (download / skip with GPL-3 disclosure) all
  paint through `StandaloneDialog`. The legacy `TaskDialogIndirect`
  + `MessageBoxW` fallback paths are removed.
- **feat(011): themed in-app dialogs.** `IDM_MACHINE_INFO`,
  `IDM_HELP_KEYMAP`, `IDM_HELP_ABOUT` (with the photoreal Cassowary app
  icon + clickable GitHub URL) and the SettingsPanel ROM-download
  failure notification all route through `EmulatorShell::ShowModalDialog`.
- **feat(011): `DiskMru` helper + `recentDisks` persistence.**
  Most-recently-used disk image list (cap = 16, move-to-front dedup,
  oldest eviction) round-trips through the new `recentDisks` JSON
  array in `GlobalUserPrefs`. Every successful `EmulatorShell::Mount`
  pushes the image path onto the MRU and persists.
- **feat(011): drive widget filename label.** The faceplate now shows
  the mounted disk's basename below `DRIVE N`, hidden when no disk is
  mounted, ellipsis-truncated (single U+2026) when wider than the
  available space. Truncation algorithm is a pure binary search,
  unit-tested with a deterministic measure stub.
- **refactor(011): single disk-insert file picker.** Legacy
  `GetOpenFileNameW` branch removed; both `IDM_DISK_INSERT*` route
  through the modern `IFileOpenDialog`-backed `PromptForDiskImage`.
- **feat(011): boot-disk MRU picker (US2).** When no disk is configured
  at startup, a themed picker lists every still-present recent disk
  image as a clickable row (basename + hover highlight) above
  `Download…` / `Skip` footer buttons. Selecting a row mounts that
  image; `Download…` falls through to the asset bootstrap; `Skip` /
  Esc boots without a disk. Existence-prunes the MRU on show so
  deleted files don't reappear.
- **feat(011): DialogPrimitive custom-body input.** `DialogDefinition`
  gained an `onInputCustomBody` hook (`std::optional<int>` return =
  close-request); `DialogPrimitive` dispatches mouse / keyboard events
  through it before its own handling. `DialogPaintContext` now carries
  the `DwriteTextRenderer` pointer so custom-body paint callbacks can
  render text in addition to geometry.
- **feat(011): Win11 dark-mode pass on debug dialogs (US6/US7).**
  `DebugConsole` and `DiskIIDebugDialog` apply
  `DWMWA_USE_IMMERSIVE_DARK_MODE`, dark control brushes, the
  `DarkMode_Explorer` window theme, and `WM_CTLCOLORSTATIC` overrides so
  the developer-only dialogs match the rest of the Win11 dark chrome.
  ListView header (`ItemsView` theme) + row colors picked up too; the
  Disk II Debug dialog still flags invalid track/sector input in red.
- **chore(011): link uxtheme.lib.** Required for `SetWindowTheme` calls
  used by the dark-mode pass; added to all six `AdditionalDependencies`
  entries in `Casso.vcxproj`.
- **chore(011): named Unicode constants.** New `s_kchAlmostEqual`
  (U+2248) in `UnicodeSymbols.h`; all dialog body strings consume
  named constants rather than inline `\xNNNN` escapes.
- **feat(011): DX Disk II Debug Panel (US7, T044-T055, T059).** Brand
  new `Disk2DebugPanel` replaces the legacy Win32 `DiskIIDebugDialog`
  (-3073 lines). Hosts itself in the shared `ChromedPanelWindow`
  chrome shell with the active theme; lays out filter checkboxes,
  audio toggles, drive radios, themed track / sector text inputs with
  validation feedback, pause / clear buttons, and a sortable virtual
  event ListView. Adds shared widget primitives `Checkbox`, `Radio`,
  `TextInput` (cursor + selection + clipboard + keyboard nav), and
  the `RequiredRowsForHeightPx` helper on `ListView`. All 18
  `IDiskIIEventSink` + `IDriveAudioEventSink` overrides preserved so
  the panel slots into the existing EmulatorShell wiring with no
  contract change.
- **feat(011): DX Debug Console Panel (US6, T039-T043).** New
  `DebugConsolePanel` replaces the legacy `DebugConsole` EDIT-control
  window. Themed monospace log body inside the shared chrome shell,
  mouse-wheel + PgUp/PgDn/Home/End/arrow scrolling, Ctrl+C copies the
  full buffer to the clipboard. Thread-safe `Log` / `LogConfig`
  contract preserved verbatim; existing call sites needed no change.
- **feat(011): Disk2DebugPanel column toggle + filter tooltips
  (T056–T058).** Right-clicking a `ListView` column header now opens a
  themed `PopupMenu` listing every column with its current visibility as
  a check; selecting an entry hides or shows the column and re-runs
  layout. Hovering any filter control surfaces a themed `Tooltip` (DX
  overlay, no Win32 `TOOLTIPS_CLASS`) explaining the control after the
  standard dwell delay. Layout pass walked under Skeuomorphic,
  DarkModern, and RetroTerminal, every widget family renders without
  overlap and the ListView header shows all six columns.
- **chore(011): shared `PopupMenu` widget.**
  `Casso/Ui/Widgets/PopupMenu` provides a reusable themed popup with
  check glyph, keyboard navigation (arrow keys + Enter / Escape), and
  host-rectangle clamping so panels can host context menus without
  pulling in Win32 menu APIs.
- **chore(011): chrome shell extracted.** `ChromedPanelWindow` and
  `IChromedPanelContent` factored out from the dialog primitives so
  both new panels (and any future child window) share NC chrome,
  title bar, sys buttons, DPI handling, and input routing without
  copy-paste.

- **feat(011): DebugConsolePanel text selection (T041).** Click-drag
  selects a character range; Shift+arrows / Shift+Home/End extend the
  selection (Shift+Ctrl+Home/End jump to the buffer extremes), plain
  caret-move keys collapse it. Ctrl+A selects the whole buffer; Ctrl+C
  copies the current selection to the clipboard as `CF_UNICODETEXT`
  (CR/LF between lines), or is a no-op when the selection is empty.
  Selection highlight paints under the text using the active theme's
  nav-hover color and tracks the viewport while dragging past the
  body edges.
- **feat(011): Disk2DebugPanel per-column sortable header (T055).**
  Clicking any ListView header now sorts by that column; clicking the
  active column flips ascending / descending. All six columns
  (Wall / Uptime / Cycle / Drive / Event / Detail) participate, with
  a numeric-aware comparator for the comma-grouped cycle string and
  the projection's `EventLabel` for the event column. A ▲ / ▼ glyph
  paints in the active sort header. `ListView::HitTestHeaderColumn`
  is the new shared hit-test helper.

### Fixed
- **fix(disk2debug): Z-pattern Tab order, list keyboard nav, selection
  preservation.** The Disk II debug panel's Tab focus now follows a
  Z-pattern top-to-bottom / left-to-right (Motor through DriveSel,
  Audio master + sub-checks, Drive radio, Track/Sector edits, raw QT
  checkbox, Pause, Clear) instead of starting on Pause. Past stop 18
  the Tab cycle continues through dynamic per-visible-column stops:
  each visible column gets a header stop (Space sorts) and a divider
  stop (Left/Right resizes by 8dp); the last dynamic stop is the list
  body, where Up/Down/Home/End/PageUp/PageDown move the selected row.
  Selection is now persisted by event identity (index into `m_events`)
  and remapped via `lower_bound` on every filter/sort/clear rebuild,
  snapping to the nearest still-visible neighbour when the selected
  event falls outside the current filter. `OnKey` now routes input to
  the focused widget only (the old broadcast-to-all path swallowed
  arrows for the wrong widgets) and `PushListViewRows` now skips
  stale `m_filteredIndices` entries defensively to harden against any
  index/deque desync that would have tripped a `vector subscript out
  of range` assertion in ARM64 Debug.
- **fix(011): SettingsPanel + dialog body share one themed background.**
  The settings popup hardcoded a dark-navy panel fill while the
  themed dialog body used `dropdownBgArgb`, so the two surfaces drew
  visibly different colors when stacked. New `panelBgArgb` /
  `panelEdgeArgb` entries on `ChromeTheme` are now consumed by both
  the settings popup and `DialogPrimitiveRenderer`, and pick up the
  active theme (Skeuomorphic / Dark / RetroTerminal) instead of the
  former blue-only constants.
- **fix(011): mounted disks now follow the user across machine switches.**
  Switching machines (e.g. //e → ][ plus) tore down the old Disk II
  controller and brought up a fresh empty one, leaving the previously
  mounted image orphaned in `DiskImageStore`. The new machine's boot
  ROM then seeked to track 0, found no data, and spun forever. The
  switch now snapshots slot-6 source paths before teardown and
  re-mounts them on the new controller, matching the user's physical
  mental model (the disk stays in the drive). Per-machine saved-disk
  prefs are still consulted when no disk is mounted at switch time.
  Carry is also guarded by `HasSlot6Controller()` so destinations
  without a Disk II (future non-Apple families, or //e → IIgs where
  the 3.5" SmartPort lives in slot 5) cleanly drop incompatible media
  rather than silently losing it.
- **fix(011): Disk II debug panel polish.** A handful of cosmetic /
  ergonomic issues in the new themed Disk II debug panel:
  - Tooltips now scale with DPI (font, padding, border, anchor gap)
    and measure real text width instead of estimating from a magic
    character-width constant.
  - Filter checkboxes, radio buttons, and the "Raw qt" toggle were
    too narrow at default fonts, wrapping their labels; widened the
    per-control slot widths in the layout helper.
  - The `Drive` column header in the event list was clipping to
    `Driv\ne`; bumped `kColDriveWidth` to fit the bold header glyphs.
  - Non-modal panels (Disk II debug, Debug console) no longer pin
    themselves above the main window, the new
    `IChromedPanelContent::IsNonModal()` hook passes `nullptr` as
    the parent HWND so the user can park them behind Casso.
  - General-purpose `Button` widgets now use a dedicated themed
    palette (`buttonIdle/Hover/Pressed/BorderArgb`) instead of
    inheriting the transparent chrome min/max/close colors, and
    paint a default 1dip border so a button actually looks like a
    button. Chrome titlebar buttons are unaffected (they paint
    themselves).
  - Renamed the event list's `Wall` column header to `Time`.
  - Tooltips are now fully opaque (background alpha bumped from 0xF0
    to 0xFF) so text underneath no longer bleeds through. The Disk II
    debug panel additionally flushes both `DxUiPainter` and
    `DwriteTextRenderer` between underlying widgets and the
    tooltip/column-menu overlays, so opaque overlay backgrounds
    actually composite *above* widget text (both renderers batch, so
    submission order alone wasn't enough, text from underlying widgets
    would otherwise paint on top of the overlay's geometry).
  - **Disk II debug panel: scrollable event list + scrollbar.** The
    panel's `ListView` now keeps its full filtered history rather than
    truncating to whatever fits in the slot, and paints a vertical
    scrollbar at the right edge whenever the row count exceeds the
    visible capacity. Mouse-wheel / trackpad scrolling is wired
    through `IChromedPanelContent::OnMouseWheel`. Sticky-tail behavior
    keeps the latest events in view when parked at the bottom; once the
    user scrolls back, new events accumulate without yanking the view.
  - **Disk II debug panel: keyboard focus + Tab navigation.** Tab /
    Shift+Tab cycle focus through the 19 focusable stops (Pause, Clear,
    8 event-type checkboxes, audio master + 4 sub-checks, raw-Qt check,
    drive radio group, track edit, sector edit). Mouse-clicks on any
    widget also acquire focus. The existing per-widget focus rings
    finally light up; Enter / Space activate buttons, Space toggles
    checkboxes, arrows cycle radios, all without touching the mouse.
  - Non-modal panels (Disk II debug, Debug console) now get a taskbar
    button via `WS_EX_APPWINDOW` and drop `WS_EX_TOOLWINDOW`, so they
    re-appear in Alt+Tab and can be raised even when fully occluded
    by Casso.
  - Event list column headers are now user-resizable: faint vertical
    separators mark each column edge, the cursor switches to
    `IDC_SIZEWE` over the right-edge handle, and click-drag adjusts
    the column width via `ListView::SetColumnOverrideWidthPx`.
    Plumbed through a new `IChromedPanelContent::OnSetCursor` hook
    that `ChromedPanelWindow::WndProc` routes from `WM_SETCURSOR`.
  - "Invalid" feedback under the track / sector filters now restores
    the rejected-token detail (e.g. `Invalid track: foo, 99`) instead
    of the bare `Invalid` placeholder, slicing the original spans out
    of the edit's text via `TrackSectorPredicate::RejectedSpans()`.
  - The drive-filter radio row gets a `Drive:` label so the three
    radios aren't anonymous. New layout slot + `Label m_driveFilterLabel`.
  - `All` is now the default selected drive filter again: the bug was
    that `ConfigureWidgets` called `SetSelected(0)` before
    `LayoutWidgets` had populated `SetOptions`, so the out-of-range
    clamp silently fell back to -1. `LayoutWidgets` now re-applies
    `SetSelected (m_filter.driveFilter)` after `SetOptions`.
  - Right-aligned column headers no longer underlap the sort triangle.
    `ListView::Paint` reserves the sort-glyph width plus a small gap
    on the right edge of the title's draw rectangle on the sorted
    column, so right-aligned titles shift left and the triangle has
    its own clear lane.
  - `TextInput` no longer wraps when text exceeds the visible width.
    `DwriteTextRenderer::DrawString` gained an optional `wrap` flag
    that toggles `DWRITE_WORD_WRAPPING_NO_WRAP`, and the renderer
    exposes `PushClipRect`/`PopClipRect` so a widget can clip text
    to its inner rect. `TextInput::Paint` measures the caret pixel
    position, slides a per-widget `m_scrollPx` offset so the caret
    stays in view (and trailing edge doesn't leave dead space), and
    pushes a clip rect across the inner area. `CaretFromX` adjusts
    by `m_scrollPx` so mouse clicks still hit the right character
    once the text has scrolled.
  - `ListView` scrollbar thumb is now draggable with live scrolling.
    New `HitTestScrollbarThumb`/`HitTestScrollbarTrack` and
    `BeginThumbDrag`/`UpdateThumbDrag`/`EndThumbDrag` on the widget;
    the Disk II debug panel routes `WM_LBUTTONDOWN` / `WM_MOUSEMOVE`
    / `WM_LBUTTONUP` through them (with capture) ahead of other
    hit-tests. Track clicks above / below the thumb page-scroll by
    one visible-row capacity.
  - Disk II debug panel: the event-type and audio-event checkbox
    rows now have leading `Disk events:` / `Audio events:` labels
    (same width on both rows, so the checkbox columns line up
    vertically). The audio master checkbox is relabeled `All`
    (the row label carries the "audio" word now), and the four
    sub-checkboxes are now disabled when `All` is unchecked.

### Deferred
- None: all spec 011 tasks shipped.

### Removed
- **chore(debug): remove DX Debug Console panel.** The themed Debug
  Console (Help → Debug, Ctrl+D) shipped earlier in this spec turned
  out to carry no signal worth keeping (DEBUGMSG output is already
  visible in the VS debugger and machine-config dumps duplicate what
  the Hardware Info dialog shows). Removed `DebugConsolePanel.{h,cpp}`,
  `IDM_HELP_DEBUG`, the Ctrl+D accelerator, the Help menu entry, the
  keymap dialog line, and the per-frame render hook.

### Fixed (post-merge polish)
- **fix(disk2debug): allow reopening after the close box.** Clicking
  the title-bar X destroyed the panel's HWND but left the owning
  `unique_ptr` in `EmulatorShell` non-null with a stale handle. The
  next menu / Ctrl+Shift+D click hit the cached pointer's `Show()`
  on a dead HWND and silently did nothing. The open path now treats
  `Hwnd() == nullptr` the same as `panel == nullptr` and reconstructs.

### Tests
- **+24 headless unit tests** across `DialogLayoutTests` (6),
  `DiskMruTests` (9), `DriveLabelTruncationTests` (7), and
  `GlobalUserPrefsTests` (+2 round-trip / malformed-entry cases). All
  measurement and filesystem dependencies injected; no Win32, no real
  file I/O. Plus `Disk2DebugPanelLayoutTests` (10) covering the new
  layout slots. Total suite: 1653/1653 passing.

## [1.5.1289]: The one where copy-protected games boot

This release celebrates a milestone: Casso now boots original,
copy-protected Apple II games straight from their unmodified WOZ
preservation images. *Karateka*, *Choplifter*, and *Lode Runner*, 
three Broderbund classics whose protection schemes defeated naive
emulation, all load and run.

The fidelity work behind this milestone landed across the 1.4 series
(motor spin-up delay, MC3470 weak-bit emulation, a real 16-state Logic
State Sequencer, the quarter-track read pipeline for half-track
protection, and a bit-level write path). This entry marks the point at
which those pieces came together well enough to run real protected
software, and bumps Casso to **1.5**.


## [1.4.1288]: The one with the quarter-track pipeline

### Added
- **feat(disk2): quarter-track read pipeline for half-track copy
  protection.** The disk pipeline now resolves reads at quarter-track
  (0-159) resolution via a TMAP-derived slot map, and the head stepper
  uses the apple2js PHASE_DELTA model so it always rests on a valid
  detent. Disks formatted on half-track boundaries (*Choplifter*,
  *Karateka*, and *Lode Runner*) now boot from their original WOZ
  images (previously stalled in the protected region around track 12).

### Fixed
- **fix(disk2): boot recalibrate audio is a machine-gun ratchet, not a
  buzz.** During a DOS boot recalibrate the head pins at the track-0 stop
  and emits a steady ~52 Hz stream of identical thunks; restarting the
  same sample every 19 ms smeared into a continuous buzz. The audio layer
  now cycles rapid consecutive bumps through a 4-slot ratchet pattern,
  restoring the realistic slow "machine-gun" ratchet a real Disk II makes.
  Isolated bumps stay firm thunks; a genuine step re-arms the pattern.


## [1.4.1279]: The one with the copy-protection foundations

### Added
- **feat(disk2): MC3470 weak-bit emulation.** WOZ floating (fake-bit)
  regions now return AppleWin-style randomized bits when the read head
  sits over a weakly-magnetized area, matching real read-data-register
  behavior.
- **feat(disk2): real Logic State Sequencer.** The nibble engine now
  drives reads/writes through a faithful 16-state LSS ROM (clean-room
  from the MIT-licensed apple2js), replacing the simplified bit-shift
  model.
- **feat(disk2): motor spin-up delay.** Reads within ~70 ms of motor-on
  return drive noise rather than instantly-valid data, matching the
  mechanical spin-up a real Disk II requires.

### Changed
- **perf(cpu): sub-instruction (per-bus-access) cycle accounting.** The
  CPU now exposes a bus-access-precise cycle counter; the Disk II
  controller samples it so rotational position reflects the exact cycle
  a `$C0Ex` access occurs rather than the end-of-instruction rollup.


## [1.4.1260]: The one with the drive widget

### Changed
- **perf(chrome): snappier drive-widget click-to-dialog.** The post-door-open
  linger before the file picker appears dropped from 600ms to 0ms (total
  delay now matches the door animation, ~350ms). Door animation still
  completes before the dialog covers the drive.

### Added
- **feat(chrome): drive widgets are now interactive.** Clicking a
  drive's body or eject button opens the door, presents a file picker,
  and either mounts the chosen disk or closes the door again if you
  cancel. Default cold-boot door state is now `Open` (rather than
  `Closed`) so empty drives look right out of the box; auto-close
  animates when a saved disk is restored on launch.
- **feat(chrome): mouse-leave virtual.** `Window::OnMouseLeave` is
  routed through `EmulatorShell` → `UiShell` to clear latched hover
  state on `TitleBar` and `NavLayer` without the old fake-mouse-move
  hack.

### Fixed
- **fix(prefs): mounted disk path no longer wiped on every launch.**
  `GlobalUserPrefs::Save` previously wrote a hardcoded empty
  `"machines": {}` block, clobbering the per-machine prefs that
  `UserConfigStore` had persisted (including `disk1Path` /
  `disk2Path`). Main's pre-flight disk-audio consent save then fired
  on every launch, so the disk you mounted last session was always
  gone by the time the boot-disk prompt ran. `GlobalUserPrefs::Save`
  now reads the existing file and preserves its machines section;
  `UserConfigStore::SaveCombinedJson` does the same as a
  belt-and-suspenders second line of defence. Regression test added.
- **fix(chrome): door geometry, perspective, and visible recess strip.**
  Door is now hinged inside the recess (not at the recess top),
  retracts 75 % when fully open, and is sized to recess width rather
  than the full eject-button rect. The recess's "finger notch" stays
  visible as a strip below the closed door instead of being painted
  as a separate sub-rect that disappeared at the wrong moments.
- **fix(chrome): door animation actually animates.** `visual.nowMs` is
  now populated from `steady_clock` in `UiShell::Render` (same time
  base as `DiskManager::NowMs`); `m_redrawForced` is reset before the
  after-blit hook instead of after, so the hook can request follow-up
  frames; and `EmulatorShell::BrowseForDisk` pumps
  `D3DRenderer::UploadAndPresent` from its own busy-wait so chrome
  keeps painting while the UI thread is blocked waiting for the file
  picker.





## [1.4.1229]: The one that moved UserPrefs to JSON

### Added
- **feat(window): per-window DPI ownership.** `Window` base class now
  owns the authoritative `DpiScaler` for its HWND, seeded on
  `WM_NCCREATE` and updated on `WM_DPICHANGED` via a non-virtual
  interface (`OnDpiChanging`/`OnDpiChanged` hooks). Subclasses query
  DPI through `Window::Scaler()`, no more cached copies, no more
  raw `GetDpiForWindow` calls, no `dpi` parameters threaded through
  call chains.
- **feat(window): proper `OnMouseLeave` virtual.** Replaces the
  previous fake-mouse-move-to-(-1,-1) hack that the chrome painter
  relied on for clearing latched caption-button and nav-menu hover
  state when the cursor exited the window.

### Changed
- **refactor(prefs): all settings now live in `UserPrefs.json`**,
  no more Windows Registry usage. The registry stack
  (`RegistrySettings`, `Win32RegistrySettings`, `IRegistrySettings`,
  `InMemoryRegistry`) has been deleted, including the registry →
  JSON migration path. Existing installs lose their registry-only
  settings on upgrade (last machine, last disks, drive audio
  preferences, audio-download consent, window placement); all are
  cheap to re-customize on first launch.
  - `GlobalUserPrefs.window.placements` is a per-topology map (FNV-1a
    hash of monitor configuration) so multi-monitor users get
    distinct remembered positions per layout.
  - Per-machine `Disk1Path`/`Disk2Path`, `DiskIIMechanism`,
    `DriveAudioEnabled`, `PromptForAudioDownload` all moved into
    each machine's `$cassoUiPrefs` JSON block.
- **refactor(layout): `ChromeLayout` renamed to `LayoutManager`.** It
  owns more than just chrome now, it's the single source of truth
  for chrome insets *and* framebuffer-to-client sizing math.
  `LayoutManager::ClientSizeForFramebuffer(fbW, fbH)` is the only
  place that decides how the Apple ][ framebuffer scales relative
  to the chrome at any given DPI (linear scaling today).
- **refactor(window): `s_WndProc` slimmed to a flat dispatch table.**
  Per-message processing extracted into named `Handle*` helpers
  (`HandleNcLButtonDown`, `HandleNcLButtonUp`, `HandleSettingChange`,
  `HandleDpiChanged`, etc.). Single `LRESULT retval` local, single
  return point, EHM bailout pattern for the null-`this` guard.
- **refactor(boot): default to Apple //e when no machine is selected
  or the discovery scanner finds none installed**, previously the
  app bailed with a "no machines found" message-box.

### Fixed
- **fix(layout): initial window now matches `Ctrl+0` reset size.**
  Two bugs were producing a smaller initial window:
  1. The framebuffer sizing math at create time disagreed with the
     Ctrl+0 reset path (one used linear DPI scaling, the other
     integer; one included the drive-bar inset, the other inlined
     its own formula). Both paths now route through
     `LayoutManager::ClientSizeForFramebuffer`.
  2. The post-create reconcile measured non-client overhead before
     `ShowWindow` had triggered the first `WM_NCCALCSIZE`, so it
     saw 0 and concluded no resize was needed. The reconcile is
     now deferred until after `ShowWindow` and forces a
     `SWP_FRAMECHANGED` before measuring.
  The reconcile also re-centers on the current monitor's work area
  with the final size, so the window doesn't visually shift when
  the user presses `Ctrl+0` immediately after launch.



## [1.4.1171]: The one with the UI overhaul (spec 007)

### Added
- **feat(settings): Display-page live preview uses a gaussian-blurred dark
  overlay with per-pixel emulator clipping when the Settings popup overlaps
  the emulator output, keeping only the focused CRT control sharp.**
- **feat(menus,settings): menu restructure and settings relocation.**
  - File, Edit, Machine, Disk, View, Debug, Help nav menus group
    commands by user workflow: machine info moved to Settings →
    Hardware, write mode moved to Settings → Machine, debug tools live
    under Debug (positioned between View and Help), and Disk/View
    menus now include visual separators.
- **feat(ui): full UI overhaul, native DirectX chrome, themes, Settings
  panel, CRT post-processing, drive widgets.**
  - Borderless main window with custom native chrome, title bar with
    Win11-style caption buttons, drag region, drive widgets, and nav
    layer with keyboard mnemonics (Alt-held + keyboard-opened menus
    show underlined access keys), all rendered through `DxUiPainter`
    + `DwriteTextRenderer` on top of the D3D11 framebuffer. No
    third-party UI engine; native Direct2D / DirectWrite end-to-end.
  - **Chrome layout manager** (`ChromeLayout`) is the single source of
    truth for chrome inset math. Edge contributors (`IEdgeContributor`)
    reserve thickness on one of the four window edges; center layers
    (`ICenterLayer`) reserve non-uniform padding around the center
    rect (interface plumbed for future monitor-frame work, no
    implementations shipped). Pure-function `Resolve()` reduces
    contributor state into a canonical inset snapshot; both the
    initial window-size calculation and the `Ctrl+0` reset-window-size
    path read from the same source, eliminating the historical drift
    class that produced pillarboxing.
  - Three built-in themes: **Skeuomorphic**, **Dark Modern**, and
    **Retro Terminal**, hot-swappable from `Settings → Theme` with
    no restart and no machine reset. Each theme drives its own chrome
    color palette and CRT shader defaults; Dark Modern + Retro
    Terminal additionally request compact drive widgets so the bottom
    chrome strip shrinks dramatically, and the window auto-resizes by
    the inset delta on theme swap so the emulator pixel grid is
    preserved.
  - **Skeuomorphic drive widgets**: realistic Apple Disk II faceplates
    with a perspective-projected receding case top (two indented lid
    panels that taper toward the back, nine vent slits down each side
    aligned with the rear panel), beige case wrapping the black inset
    faceplate on all four sides, a cantilever door hinged at the slot
    top that tilts up and back while the bulk of its length tucks
    inside the case (leaving a 20% flap visible when fully open) and
    reveals a recessed finger-pull behind it, slot, "DRIVE N" /
    "IN USE ▶" labels, status LED, and the Cassowary rainbow logo in
    the cassowary-on-the-bay corner. Click to mount; drop a `.dsk` /
    `.do` / `.po` / `.nib` onto a widget to insert it. Eject clicks
    animate the door open even on an empty drive.
  - **Compact drive widgets** (Dark Modern + Retro Terminal themes):
    small flat rounded cards with label + status LED on the right.
    Same mount / hit / drag-drop semantics as the skeuomorphic widgets
    but a fraction of the visual footprint.
  - Consolidated **Settings** panel replaces the old `OptionsDialog`
    and `MachinePickerDialog`. Machine selection, CPU speed, monitor
    type (color / green / amber / white), floppy sound + mechanism
    (Shugart / Alps), write-protect per drive, theme picker, and CRT
    controls all live in one non-modal in-window panel with full
    keyboard navigation (Tab/Shift+Tab through focusables, Enter on
    OK / Cancel buttons). Opens from `View → Settings…` and `Ctrl+,`.
  - The Settings popup now uses the same ChromeTheme-driven custom
    title bar as the main window, including gradient chrome, matching
    caption buttons, drag/resize hit-testing, and the Casso app icon.
  - **CRT post-processing**: scanlines, phosphor bloom, color bleed.
    Each effect is independently toggleable; a single brightness
    slider gates the master mix. Per-theme defaults live in each
    theme's JSON.
  - Auto-remount of the last-inserted disks on machine load so a
    typical "boot Apple ][ plus" workflow is one click.
  - **Unified user preferences persistence** in `UserPrefs.json` stores
    global UI state and per-machine deltas together, replacing both the
    registry-based `RegistrySettings` path and the old split-file model.

### Removed
- **Legacy Win32 menu bar** (FR-026). All commands previously served
  by the File/Edit/Machine/Disk/View/Help menu bar now route through
  the native `NavLayer`; the parity table at
  `specs/007-ui-overhaul/menu-command-parity.md` is the source of
  truth and `NavLayerTraceabilityTests` enforces it in CI.
- **`OptionsDialog` and `MachinePickerDialog`** (FR-027). Both Win32
  dialogs are deleted; the Settings panel hosts the same controls.
- **`ChromeMetrics::*Px()` inset functions**: replaced by
  `ChromeLayout::Resolve()` and `ChromeLayout::ClientSizeForCenter()`.
  `ChromeMetrics` slims down to a constants-only header (framebuffer
  dimensions + base DPI).

### Tech notes
- All UI rendering is native Direct2D / DirectWrite, no third-party
  UI engine, no FreeType. Native ClearType + emoji on Win10+.
- Win11 effects (**Mica backdrop**, rounded corners, immersive dark
  caption) are runtime-gated via `Win11DwmHelpers`; the app falls
  back gracefully on Win10.
- All user-visible strings use **sentence casing** (menu items, page
  titles, dialog captions, file-filter strings) per the in-repo
  convention.
- **48 functional requirements delivered**: FR-001..FR-047 + FR-022b.
  See `specs/007-ui-overhaul/spec.md` for the full traceability.
- **17 new `ChromeLayoutTests`** cover the planner end-to-end: empty
/ single / multi-edge contributors, DPI scaling, center-layer
padding, over-allocation clamping, `ClientSizeForCenter`
inverse-roundtrip, contributor mutation, and a regression test for
the historical Ctrl+0 pillarbox.

## [1.3.808]: The one with the plain silhouette icon

### Changed
- **Default app icon is now the plain cassowary silhouette.** The
silhouette+accent variant (previously default) was hard to read at
title-bar and small-icon sizes, the rainbow stripe and scanline
overlay both compete for the same pixels. The new
`IDI_CASSO_SILHOUETTE` is a cream silhouette on a dark warm tile
with no extra ornamentation, so it stays legible at 16x16. The
other four motifs remain embedded for callers that want them.

## [1.3.807]: The one with the app icon

### Added
- **Casso has an app icon.** The window title bar, taskbar, and Windows
Explorer now show a cassowary silhouette with a retro Apple-rainbow
accent stripe instead of the generic Windows EXE icon. Four motifs
ship embedded in the binary (silhouette+accent, silhouette+rainbow,
flat-color head, photoreal); the silhouette+accent variant is the
default. PNG masters and multi-resolution ICOs live in
`Resources/Icons/`, regeneratable via `Assets/Icon/build_icons.py`.

## [1.3.772]: The one with the machine picker fixes

### Fixed
- **Machine picker showed empty list.** `MachinePickerDialog::ScanMachines`
  searched for `Machines/*.json` but configs live at
  `Machines/<Name>/<Name>.json`, scanner now walks the nested layout.
- **Switching machines could crash.** `SwitchMachine` runs on the CPU
  thread (MTA COM); pre-launch ROM/disk-audio bootstrap was happening
  there and showing modal `TaskDialog`s parented to a window owned by
  another thread. Bootstrap is now performed on the UI thread inside
  `ShowMachinePicker` *before* the switch command is enqueued.
- **Switching to a machine with un-downloaded ROMs failed.** Bootstrap
  in `ShowMachinePicker` now mirrors `Main.cpp`'s startup flow
  (`CheckAndFetchRoms` + best-effort `CheckAndFetchDiskAudio`) so any
  machine the user picks can actually launch.
- **Apple ][ and ][ plus shipped with no Disk II.** Embedded default
  configs (`Resources/Machines/Apple2/Apple2.json`,
  `Resources/Machines/Apple2Plus/Apple2Plus.json`) had empty `slots`,
  so the status bar showed no drives and "Insert Drive 1" was inert.
  Both now ship with a Disk II in slot 6 (matches //e). Existing
  installs are refreshed automatically; see the embedded-default
  versioning change below.
- **Embedded machine configs are now versioned and auto-refresh.**
  Each embedded JSON carries a `// DO NOT EDIT` header and a
  `"$cassoDefault"` version stamp. On launch,
  `AssetBootstrap::EnsureMachineConfigs`:
    - Extracts when the file is missing (unchanged behavior).
    - Overwrites when the on-disk stamp is older than the embedded
      version.
    - Overwrites when the on-disk file is unstamped *and* its
      normalized (BOM-stripped, LF-normalized) SHA-256 matches one of
      a small list of historical embedded defaults, i.e. it's a
      verbatim extract from an earlier Casso release, safe to
      replace.
    - For anything else (unstamped + unrecognized = presumed
      user-edited), renames the existing file to
      `<name>.json.user-backup-<YYYYMMDD-HHMMSS>` and installs the
      current embedded default in its place. Users always boot a
      working machine; their edits aren't gone, just moved. The
      header comment in every embedded default points future
      customizers at "copy first, then edit the copy."
- **Drive audio went silent after a machine switch.** `SwitchMachine`
  unregisters the old per-drive `DiskIIAudioSource`s and builds new
  ones, but never re-invoked `DriveAudioMixer::SetMechanism`, so
  `LoadSamples` never ran on the fresh sources and every drive event
  played nothing. Mixer is now re-poked with the current mechanism
  immediately after `CreateMemoryDevices`. Drive sounds work on every
  machine you switch into, not just whichever one was active at
  launch.
- **Ctrl+0 pillarbox.** Window-size math for `Ctrl+0` reset omitted
  the bottom chrome inset; framebuffer was scaled to the wrong client
  area and the emulator viewport letterboxed. Now goes through
  `ChromeLayout::ClientSizeForCenter` along with the initial
  window-size path so the two call sites cannot drift.

### Changed
- **`MachineScanner` extracted to `CassoEmuCore/Core`** as a pure
  module with injectable `DirectoryLister` and `FileReader` functors,
  letting `MachinePickerDialog`'s scan logic be unit-tested without
  filesystem access. Production callers use the default overload that
  wraps `std::filesystem`.

### Tests
- Added `MachineScannerTests` (8 cases): nested vs flat layout,
  first-search-path-wins, missing-Machines-dir handling, missing
  per-machine JSON, unparseable JSON fallback, JSON without `name`
  field, empty search paths.
- Added `ChromeLayoutTests` (17 cases): see Tech notes above.

## [1.3.764]: The one with the Disk II debug window (spec 006)

### Added
- **Disk II Debug Window**: modeless live event log of motor / head /
  address-mark / data-mark / drive-select / insert-eject / audio
  events from the active Disk II controller. Opens via
  **View → Disk II Debug...** or **Ctrl+Shift+D**. Filterable by
  event type, drive (per-drive or all), track, sector, and audio
  outcome (started / restarted / continued / silent). Track / Sector
  filters accept integers, decimals, ranges, and comma lists;
  out-of-range or unparseable tokens are highlighted with a
  red wave squiggle (RichEdit) and listed in an "Ignored:" label.
  Track values clamp to whole-track 40 / quarter-track 160; sector
  to 16. Six columns: Time (local HH:MM:SS.mmm), Uptime
  (MM:SS.mmm since most recent //e reset), Cycle count, Drive
  (1-based), Event (sentence-cased), Detail, content-auto-sized
  with periodic re-grow as wider data arrives; user-dragged widths
  are preserved. Detail flex-fills the remaining ListView width
  and re-flows on dialog resize. Right-click the column header to
  show / hide individual columns. Auto-tail scrolling while at the
  bottom; pause / resume / clear; Ctrl+C copies selected rows
  tab-separated in visible-column order. Machine reset clears the
  view and re-zeroes Uptime. The menu item is grayed out on
  machines without a Disk II controller (FR-001a); when more than
  one Disk II controller is wired, the title becomes
  "Disk II Debug (controller #0 only)" (FR-017). The dialog
  survives `SwitchMachine`, sinks reattach to the freshly built
  controller / audio source. Events emitted before the dialog
  opens are not retained, open it *before* the operation you
  want to investigate. Four bundled WOZ fixtures (Apple Stellar
  Invaders, Choplifter, Hard Hat Mack, Karateka) live in
  `Apple2/Demos/` for manual A/B observation.
- **`IDiskIIEventSink` / `IDriveAudioEventSink`**: two new sink
  interfaces (controller side + audio side) the debug dialog
  implements simultaneously; the shell attaches and revokes both
  in the same lifecycle window. Sinks are nullptr-default and
  controller behavior with no sink attached is byte-identical to
  pre-feature (SC-007, SC-010).
- **`DiskIIEventRing`**: lock-free SPSC ring (4096 capacity)
  buffering producer-side events between the CPU thread and the
  UI-thread drain timer; overflow returns false without corruption
  and coalesces into a single `[N events lost]` marker on the next
  drain (FR-010).
- **`DiskIIAddressMarkWatcher`**: two state machines that decode
  address marks (with volume number) and data marks from the
  controller's nibble stream; bad checksums and mid-stream resync
  are tolerated without false positives.
- **`SilentReason::NoDiskPresent`**: surfaced in the Detail column
  when the motor is commanded on (or kept on across an eject)
  with no disk in the drive bay.

### Changed
- **Drive LED** now paints the red activity dot only when the
  drive bay actually holds a loaded disk. The controller's motor
  stays commanded on across an eject (real-hardware behavior),
  but with no media to read the user expects the LED to go dark.
- **`DiskIIAudioSource`** motor loop is gated on
  `m_motorRunning && m_diskPresent`. Modeled after real Disk II
  hardware: the motor spins regardless of media presence, but
  with no disk loaded there is no media noise to hear. Eject-
  while-motor-on emits `OnAudioLoopStopped` +
  `OnAudioSilent (MotorLoop, NoDiskPresent)`; re-insert emits
  `OnAudioRestarted (MotorLoop)`.
- **`EmulatorShell::{Mount,Eject}DiskInSlot6`** now routes through
  new `DiskIIController::NotifyDiskInserted` / `NotifyDiskEjected`
  entry points so the user-facing insert / eject events fire even
  when the shell mounts via `DiskImageStore + SetExternalDisk`
  (which bypasses the controller's own MountDisk path).
- `IDriveAudioSink` audio-event method names normalised so the
  controller-side and audio-side sinks present a parallel surface
  to `DiskIIDebugDialog`.
- `EmulatorShell` owns a shell-wide uptime anchor (`steady_clock`)
  re-zeroed on `SoftReset` / `PowerCycle`; the debug dialog reads
  it on every drain so the Uptime column tracks the active //e's
  power-on age, not the host process.
- `Window` base class gains a virtual `OnInitMenuPopup` hook to
  support FR-001a runtime menu-item gating.

### Tests
- 156 new tests across the spec-006 surface: SPSC ring (push fills,
  pop drains, wrap, overflow), address-mark watcher (stock cadence,
  bad checksum, mid-stream resync), projection (FormatEvent column
  shapes, DrainAndProject FIFO + EventsLost ordering, rolling cap),
  filter state and the track/sector predicate parser (including
  out-of-range clamp + RejectedSpan placement), RichEdit squiggle
  helpers, clipboard payload builder, column-model planner,
  FR-001a enablement decision, FR-004a uptime-reset path,
  insert / eject / SwitchMachine regression coverage.

## [1.3.684]: The one with the mechanism dropdown

### Added
- **Options dialog mechanism dropdown (FR-006 / SC-010)**:
  *View → Options...* now offers a "Disk II mechanism" combobox
  with "Shugart SA400" (default) and "Alps 2124A" entries. Flipping
  the dropdown reloads every registered drive's sample buffers via
  `DriveAudioMixer::SetMechanism` and takes effect on the next
  audio frame, no restart, no disk remount.
- **`DriveAudioMixer::SetMechanism / SetSampleLoadContext /
  GetMechanism / IsValidMechanism`**: the mixer now owns the
  asset-load context (devices dir + sample rate) and a
  validated-on-set mechanism string. Bad input returns
  `E_INVALIDARG` without mutating mixer state (SC-010 invariant).
- **Per-machine persistence (Q4)**: both the Drive Audio toggle
  and the active mechanism round-trip through
  `HKCU\Software\relmer\Casso\Machines\<MachineName>\` using new
  `RegistrySettings::ReadDword / WriteDword` helpers. Defaults are
  enabled + Shugart when the registry is empty. State is reapplied
  at `EmulatorShell::Initialize` after the machine config loads,
  before the CPU thread first touches the mixer.

### Changed
- `OptionsDialog::Show` signature gains a current/out mechanism
  pair; the procedurally-built `DLGTEMPLATE` adds STATIC and
  COMBOBOX entries (atoms 0x0082 / 0x0085).
- `EmulatorShell` Options OK handler diffs both knobs separately
  so changing one does not rewrite the other's registry value.

### Tests
- `UnitTest/Audio/DriveAudioMixerMechanismTests.cpp` adds four
  tests covering: invalid mechanism (no state change),
  multi-source reload, Alps→Shugart round trip with distinct
  amplitudes, and pre-context SetMechanism (defers load).

## [1.3.682]: The one with the disk audio bootstrap

### Added
- **Bootstrap fetch (FR-017, FR-018, NFR-006)**: on first launch with
  a machine that has a Disk II controller, Casso offers (TaskDialog
  with three command links: *Download* / *Skip* / *Don't ask again
  this session*) to download the OpenEmulator drive-noise samples
  from `raw.githubusercontent.com/openemulator/libemulation`,
  decode them in memory with `stb_vorbis`, resample to 44.1 kHz
  via linear interpolation, and write 16-bit mono PCM WAVs to
  `Devices/DiskII/<Mechanism>/`. The compressed `.ogg` bytes are
  discarded before the function returns, no `.ogg` files ever
  touch disk (NFR-006).
- The consent dialog explicitly discloses GPL-3 licensing and
  recipient obligations and links to OpenEmulator's COPYING file
  and the GPL-3 text. *Don't ask again this session* is per-process
  and resets at next launch (deleting the per-mechanism subfolders
  re-triggers the prompt).
- Five Shugart sounds (motor, head step, head stop, door open,
  door close) and three Alps sounds (motor, head step, head stop;
  Alps drives have no door) covered by `s_kDiskAudioCatalog` in
  `AssetBootstrap.cpp`.
- `CassoEmuCore/External/stb_vorbis.c` vendored from
  [github.com/nothings/stb](https://github.com/nothings/stb)
  (public domain / MIT). Included exclusively through
  `StbVorbisWrapper.cpp` which disables PCH, code analysis, and a
  documented set of upstream-rejected warnings so the rest of the
  codebase stays clean. Compiled with
  `STB_VORBIS_NO_PUSHDATA_API` + `STB_VORBIS_NO_STDIO` to drop the
  half of the library we don't need.

### Changed
- `AssetBootstrap::DownloadHttp` now treats `expectedSize == 0` as
  "no integrity check" (only "non-empty"), enabling the OGG fetch
  to reuse the existing WinHTTP plumbing.

### Tests
- `UnitTest/EmuTests/DiskAudioFetchTests.cpp` adds four tests:
  null / garbage-bytes guards for `StbVorbisWrapper`, a WAV
  write + `DiskIIAudioSource::LoadSamples` round-trip that asserts
  a non-silent motor loop after decode, and the FR-019 per-file
  precedence rule. The network-touching `AssetBootstrap` glue is
  exercised by the manual integration test in T138 (per
  constitution §II, automated tests do not hit the network).

## [1.3.675]: The one with the per-machine asset layout

### Changed
- **Per-machine ROM directories**: ROM images now live under
  `Machines/<MachineName>/` (e.g., `Machines/Apple2e/Apple2e.rom`,
  `Machines/Apple2e/Apple2e_Video.rom`) instead of a single
  top-level `ROMs/` folder. Shared device boot ROMs (Disk II
  controller firmware) live under `Devices/<Family>/` (e.g.,
  `Devices/DiskII/Disk2.rom`). The in-app missing-ROM downloader
  and `scripts/FetchRoms.ps1` both target the new layout. The
  Apple ][ / ][ plus character generator and the //e character
  generator are duplicated into each owning machine's folder so
  every machine's asset set is self-contained (a handful of bytes
  of redundancy in exchange for portability).
- **Machine configs moved**: `Machines/Apple2.json` →
  `Machines/Apple2/Apple2.json` (and the same pattern for
  `Apple2Plus`, `Apple2e`). `Casso.exe`'s `--machine` flag still
  takes the bare machine identifier (e.g., `--machine Apple2e`);
  the loader resolves the new nested path internally. Embedded
  default-config extraction (`AssetBootstrap::EnsureMachineConfigs`)
  writes to the per-machine subdir on first run.
- **`.gitignore` is now a whitelist** for `Machines/**` and
  `Devices/**`: only `*.json` manifests are tracked, ROMs and
  future drive-audio WAVs stay out of source control without
  per-file rules.
- `Assets/Sounds/DiskII/README.md` moved to
  `Devices/DiskII/README.md` to co-locate documentation with the
  device's other assets.

### Migration
Users with an existing install:

- The old top-level `ROMs/` directory is **no longer searched**.
- After updating, either delete `ROMs/` and re-run
  `scripts/FetchRoms.ps1` (which now places files in the new
  layout), or move each ROM file into the corresponding new
  location (see the table at the top of `scripts/FetchRoms.ps1`).

## [1.3.670]: The one with the Disk II audio

### Added
- **Disk II mechanical audio**: motor hum (looping while
  `m_motorOn`), head-step click (per quarter-track movement), track-0
  / max-track bump (when the stepper energizes against the travel
  stop), and disk insert / eject door sounds.
- **Step-vs-seek discrimination** (FR-005): contiguous step bursts
  fuse into a continuous seek buzz instead of N overlapping clicks.
  OpenEmulator-style cycle-gap timer; threshold = 16,368 cycles
  (~16 ms at 1.023 MHz), idle clear = 51,150 cycles (~50 ms).
- **Stereo mixing into the existing WASAPI pipeline**: speaker is
  centered (equal-power), drives are panned per-drive using
  equal-power coefficients. Single-drive profiles play centered;
  two-drive profiles place Drive 1 left-of-center and Drive 2
  right-of-center. Per-channel clamp to `[-1, +1]`; downmix to mono
  when the device is mono.
- **View → Options... dialog** (new, runtime-built `DLGTEMPLATE`)
  hosting the "Drive Audio" check toggle; default-on, takes effect
  within one audio frame.
- **Cold-boot mount suppression** (FR-013): command-line / restored /
  autoload mounts do not fire the disk-insert sound. User-initiated
  mid-session mounts and all eject events fire normally.
- **Generic drive-audio abstraction**: `IDriveAudioSink`,
  `IDriveAudioSource`, `DriveAudioMixer`, and `DiskIIAudioSource` are
  decoupled so future drive types (`//c` internal 5.25, DuoDisk,
  Apple 5.25 Drive, Apple /// drive, ProFile, …) can plug into the
  same mixer without modifying it or the sink interface (FR-016).
- `Assets/Sounds/DiskII/` directory with a `README.md` documenting the
  expected sample set (PascalCase WAVs decoded at startup via
  `IMFSourceReader` to mono float32 at the WASAPI device rate). The
  directory may be absent or empty, the emulator launches and runs
  normally with the affected sounds silently muted (FR-009).

### Changed
- `WasapiAudio` now negotiates stereo float32 from WASAPI (falls back
  to mono if the device demands it). The internal pending-samples
  buffer is interleaved stereo; mono devices downmix at drain time.
  `SubmitFrame` gained two optional parameters: a `DriveAudioMixer*`
  and the current CPU cycle count, both of which preserve
  pre-feature behavior when omitted (FR-011 / SC-006).
- `DiskIIController` exposes `SetAudioSink (IDriveAudioSink*)` and
  fires `OnMotorStart` / `OnMotorStop` / `OnHeadStep(qt)` /
  `OnHeadBump` at the documented call sites. Mount/eject door events
  fire from the shell layer (with cold-boot suppression) rather than
  the controller.

### Tests
- 34 new unit tests in `UnitTest/Audio/` and
  `UnitTest/Devices/DiskIIControllerAudioTests.cpp` covering source
  state machines, mixer panning / clamp, controller event firing,
  step-vs-seek timing, and graceful-degradation behavior. All
  tests use in-memory buffers and a recording mock sink; no host
  filesystem reads, no audio device (constitution §II).
- All pre-existing speaker tests pass identically (FR-011 / SC-006).

## [1.3.660]: 2026-05-14, The one where the demo first frame got faster

### Changed (demo)
- **Disk layout reordered so DHGR loads first.** Previously the disk
  was laid out cassowary→stage2→bands→dhgr-aux→dhgr-main, and stage 2
  init read all 9 tracks before showing the cassowary (~2.25s).
  Now: dhgr-aux→stage2→dhgr-main→cassowary→bands. Stage 1 reads
  3 tracks (DHGR aux + stage 2/lores), stage 2 reads 2 more tracks
  (DHGR main) and immediately enters DHGR mode, visible at ~5
  tracks (~1.25s, ~45% faster). HGR1 cassowary and HGR2 bands
  load in the BACKGROUND while the user is looking at the
  cassowary; both done by the time the user can react with a
  keystroke.
- **HGR1 cassowary now loads directly to its `$A000` stash
  location**, eliminating the boot-time `$2000`→`$A000` memcpy.

### Fixed
- **`enter_dhgr` was clobbering X.** `copy_block` (called by
  `enter_dhgr`) used X as its 32-page counter, leaving X=0 on
  return. With the new background-load phase needing X=$60 (slot 6
  << 4) for indexed disk-controller soft-switch reads, this caused
  the next `lda $C087,x` head-step to read the wrong address,
  head never moved, RWTS spun forever in `chk_w` waiting for a
  non-existent disk byte. The previous flow happened to call
  `enter_dhgr` AFTER all disk I/O was done so the bug was latent.
  Stage 2 now reloads `ldx #$60` after `enter_dhgr` and before
  resuming disk I/O; documented in a new comment.

### Tests
- Demo test cycle budget shrunk from 60M to 10M cycles
  (~9.8 sec emulated vs ~58 sec). Test runtime dropped from
  ~9s to <1s. Full suite now ~93s instead of ~180s.

## [1.3.652]: 2026-05-14, The one where DHGR matches HGR framing

### Changed (demo)
- **DHGR cassowary now uses the same crop, letterbox, and "Casso"
  title as the HGR cassowary.** `DhgrCassowaryGen.py` now imports
  `HgrPreprocess` and feeds the source through
  `HgrPreprocess.crop_and_fit` (HGR's 280×192 letterbox + title
  pipeline) before resizing to DHGR's 140×192 color resolution
  for quantization. On screen the two modes show the bird at
  identical framing, only the color treatment differs (HGR's
  6-color per-byte classification vs DHGR's 16-color
  Floyd-Steinberg dither).

### Fixed
- **`HgrPreprocess.py` was broken.** A prior edit had pasted the
  body of `paint_title` into `generate_dhgr_bands` after its
  `return` statement and lost the `def paint_title(...)` line, so
  any invocation with a title argument would fail with
  `NameError: paint_title is not defined`. The committed
  `cassowary.hgr` was generated before the regression and nobody
  noticed because the generator isn't wired into CI; only the
  consumed bytes are. Restored as a proper top-level function and
  generalised the centering to use `canvas.width` so the same
  helper works for HGR (280) and any future width.

## [1.3.651]: 2026-05-14, The one with the amber monitor

### Changed (demo)
- **DHGR cassowary is now mode 0**: first thing you see at boot,
  in glorious 16-color dithered double-hi-res. Cycle order:
  DHGR → HGR1 → HGR2 → LoRes → exit. Stage 2 init now stashes the
  HGR1 cassowary at main `$A000` so mode 1 can restore it after
  DHGR's `$2000` clobber.
- **DHGR cassowary aspect ratio fixed.** The image was previously
  squashed horizontally because the source photo (880×1600
  portrait) was force-resized to 140×192 without aspect
  correction. Now uses `ImageOps.fit` to center-crop to the
  display's 4:3 aspect first (560:384, what the renderer
  actually shows), then resamples to 140×192.

### Fixed
- **Amber Monochrome mode now actually shows amber, not blue.**
  After the framebuffer-format switch from RGBA to BGRA, the
  monochrome-tint code in `EmulatorShell::RenderFramebuffer`
  was reading R/G/B from the wrong byte positions AND
  reconstructing in the wrong order, so amber's `(L, L×0.75,
  0)` triple landed as `B=L, G=L×0.75, R=0`, a cyan-blue
  pixel. Refactored into `Video/MonochromeTint.h` (new pure
  header in CassoEmuCore) so the BGRA arithmetic is now
  unit-tested.
- **Demo exit no longer leaves 80-col garbage on screen.** The
  previous fix attempted to clear AUX text page 1 by toggling
  RAMWRT, but with 80STORE on (which DHGR mode set) the writes
  to `$0400-$07FF` were still routed by PAGE2, not RAMWRT,
  so AUX never got cleared. `do_exit` now turns 80STORE off
  first, then RAMWRT-toggles its way through both pages via
  a new shared `clear_text_page1` subroutine.

### Added (tests)
- **`MonochromeTintTests`** (7 new tests). Pins the green/amber/
  white tint helpers against the Rec.601 luma weights and BGRA
  byte order. Suite is now 1053/1053.
- **Demo test was rewritten for the new mode order** and now
  also verifies the cassowary stash at main `$A000` and the
  DHGR boot-landing state (DHGR aux at aux `$2000`, DHGR main
  at main `$2000`).

### Refactor
- **`Video/MonochromeTint.h`**: new header in CassoEmuCore.
  Provides `Luminance()`, `TintGreenMono()`, `TintAmberMono()`,
  `TintWhiteMono()` as pure inline functions over the BGRA
  pixel format. EmulatorShell now calls these instead of
  open-coding the byte-shuffling.

### Known limitations
- HGR (single hi-res) cassowary still uses per-byte palette
  classification with no error diffusion, looks blocky next
  to the dithered DHGR version. A real fix would require
  rewriting `HgrPreprocess.py` to do bit-on/bit-off Floyd-
  Steinberg within each byte's palette-pair constraint;
  deferred.

## [1.3.645]: 2026-05-14, The one with the DHGR cassowary

### Added (demo)
- **DHGR mode now shows a cassowary**, not just test bars. New
  generator `scripts/DhgrCassowaryGen.py` quantizes the source
  photo (`Assets/3a Mrs Cassowary closeup 8167.jpg`) to the //e
  16-color LoRes/DHGR palette via Floyd-Steinberg dithering and
  encodes it into the DHGR aux+main interleaved byte stream.
  Resulting payloads (`Apple2/Demos/dhgr-cassowary-{aux,main}.bin`,
  8 KB each) replace the test-bars patterns on tracks 6+7 / 8+9
  of the demo disk. A preview PNG is also emitted alongside so
  the quantization can be sanity-checked without booting the
  demo.

### Fixed (demo)
- **Exit from DHGR mode no longer leaves garbage glyphs on the
  BASIC screen.** Two issues:
  1. The reset handler doesn't clear the screen, only the soft
     switches. With 80COL still on after DHGR, BASIC was showing
     main+aux text page 1 interleaved, main had the LoRes
     pattern, aux still held power-on PRNG noise we never
     touched. `do_exit` now clears AUX text page 1 (under
     RAMWRT-on) in addition to main before jumping through
     the reset vector.
  2. `JMP $E000` (Applesoft cold start) is fragile even after
     manual soft-switch cleanup, it JMPs through stale work-
     area pointers and lands in video memory. `JMP ($FFFC)`
     (the //e reset vector) goes through `RESET.MGR` which
     does the full power-on cleanup before entering Applesoft
     and is the canonical way to bail to BASIC from any video
     state.

### Removed
- `Apple2/Demos/dhgr-bars-{aux,main}.bin`: superseded by the
  cassowary payload. The generator (`scripts/DhgrBarsGen.py`)
  is kept for future test-pattern needs (regenerates the .bin
  files on demand).

## [1.3.640]: 2026-05-14, The one where DHGR joins the demo

### Added (demo)
- **Mode 3: DHGR**: 16-color test bars rendered through the //e
  Double Hi-Res pipeline (560x192 monochrome / 140x192 in 16
  colors, aux+main interleaved at $2000-$3FFF). Cycles cleanly
  from LoRes via any keystroke. Adds 16 KB of new payload to the
  demo disk: tracks 6+7 hold the DHGR aux pattern (8 KB) staged
  to main $6000-$7FFF, tracks 8+9 hold the DHGR main pattern
  (8 KB) staged to main $8000-$9FFF.
- **Demo exit now goes through the //e reset vector** (`JMP
  ($FFFC)`) instead of jumping directly to `$E000`. The reset
  handler does the full power-on cleanup so any mode of the
  cycle can land on a vanilla BASIC prompt, important now that
  the cycle includes DHGR (which leaves 80STORE / 80COL / DHIRES
  on, and Applesoft cold start at `$E000` doesn't tolerate that
  state and ends up executing video memory).

### How the DHGR-from-disk technique works
- Stage 1's RWTS uses pages `$02-$03` as a 6-and-2 secondary-nibble
  scratch buffer. Loading directly to aux RAM via that RWTS
  doesn't work because RAMWRT-on routes the scratch writes to
  aux while the readbacks still fetch from main, scrambling
  the GCR decode.
- The fix is a **two-step staged load**: read the DHGR aux bytes
  into a main-RAM scratch area (no soft-switch wrangling, RWTS
  works normally), then memcpy into aux $2000 with RAMWRT
  toggled around just the copy loop. The copy uses indirect-
  indexed `(zp),y` so the page-counter increments live in zero
  page, which is ALTZP-routed (always main with ALTZP off) and
  unaffected by RAMWRT. Self-modifying-code variants would
  break here because `inc`'s writeback would land in aux,
  corrupting the running code.
- Stage 2 grew from 125 to 232 bytes (still in one sector); the
  new `copy_block` subroutine is 29 bytes and is reusable for any
  future feature that needs to populate aux RAM from disk.

### Added (tooling)
- **`scripts/DhgrBarsGen.py`**: generates the 16-color DHGR test
  pattern (8 KB aux + 8 KB main) by walking the 560-dot row,
  picking each 4-dot group's color from the bar containing the
  group's center dot, and packing nibbles LSB-first into the
  aux/main interleaved byte stream.

## [1.3.632]: 2026-05-14, The one that loosened the perf tolerance

### Changed (tests)
- **`CycleEmulation_StableRunToRun` tolerance bumped 30% → 60%.** A
  GitHub Actions runner produced a 42% outlier on an otherwise
  clean run (median 13.28 ms, worst 18.83 ms vs 17.27 ms tolerance)
  and failed the build. Shared cloud hardware can stall any given
  process for tens of ms without warning; the test still catches
  any real perf regression, but no longer trips on hypervisor
  scheduling hiccups.

## [1.3.627]: 2026-05-14, The one that swapped to BGRA

### Changed (rendering)
- **Framebuffer format is now `DXGI_FORMAT_B8G8R8A8_UNORM`** (was
  `R8G8B8A8_UNORM`). On-screen colors are visually identical;
  every Windows pixel surface (CF_DIB clipboard, GDI bitmaps, BMP,
  WIC) natively uses BGRA, so image-export paths no longer need to
  swizzle R/B on the way out. All palette literals (NTSC HGR, LoRes,
  DHGR) re-encoded into the human-natural `0xAARRGGBB` form;
  see new `Video/PixelFormat.h` for the project-wide convention.

### Fixed
- **Menu → Copy Screenshot now produces correct colors.** The
  clipboard path used `CF_DIB` (BGRA) but blindly memcpy'd from the
  RGBA framebuffer, so screenshots had R and B swapped, most
  visible on the HGR cassowary demo (orange head appeared blue).
  With the framebuffer format change above, the screenshot copy is
  now a straight memcpy with no swizzle, so this class of bug can
  no longer arise.

### Added (tests)
- **`PaletteByteOrderTests`** (8 new tests). Decomposes every named
  palette color via `Video/PixelFormat.h` extractors and asserts
  the bytes match the human-documented R/G/B intent. Catches both
  hand-typed nibble swaps and any future format flip that isn't
  propagated to the palette literals. Brings the suite to 1046
  tests.

### Fixed (demo)
- **Exit to BASIC no longer leaves LoRes garbage on the text page.**
  After cycling past the LoRes test pattern, page 1 still held the
  LoRes byte pattern. Once we flipped back to TEXT mode those bytes
  rendered as character codes, anything in `$40-$7F` is in the
  flash range, so half the screen was blinking nonsense around the
  `]` prompt. Stage 2 now clears `$0400-$07FF` to `$A0` (space)
  before `JMP $E000`. Stage 2 size is now 125 bytes (still in one
  sector).

### Changed (docs)
- **README screenshot updated** to show the HGR cassowary demo
  instead of the older GR color-bands placeholder. Retired
  `Assets/Apple ][ GR Color Bands.png`.

## [1.3.619]: 2026-05-14, The one with the American spellings

### Fixed (demo)
- **Cycling past LoRes now actually drops to a usable BASIC prompt.**
  The previous revision did `JMP $E000` (Applesoft cold start) but
  Applesoft's cold start doesn't reset the video soft-switches, so
  we landed at the `]` prompt with the screen still rendering as
  LoRes graphics, characters typed afterward updated text page 1
  but were invisible (or appeared as colored blocks). Now stage 2
  flips `TXT` on (and clears `HIRES`/`PAGE2` for good measure)
  before the `JMP $E000`.

### Changed (docs / source)
- **Spelling: standardized on American English** in source, comments,
  CHANGELOG, and stage-2 demo header. Replaced `color` →
  `color`, `artefact` → `artifact`, `behavior` → `behavior`,
  `synthesise` → `synthesize` in newly authored content.

## [1.3.618]: 2026-05-14, The one with the LoRes test pattern

### Added (demo)
- **LoRes (Apple `GR`) 16-color bar test pattern.** New
  `scripts/HgrPreprocess.py --pattern lores-bars` emits a 1 KB
  text-page-1 ($0400-$07FF) image with 16 horizontal stripes of
  LoRes palette indices 0..15. The pattern is shipped on the
  bootable demo disk on track 3 logical sectors 1-4 (which stage
  1 of casso-rocks reads to $1100-$14FF as part of staging
  stage 2).
- **Demo cycle now walks the standard Apple //e graphics modes
  with one keystroke each, then exits to Applesoft.** Stage 2
  starts in HGR1 (cassowary), advances on each keystroke through
  HGR2 (the existing 6-color bands), GR (the new LoRes bars),
  and on the next keystroke `JMP $E000` lands at Applesoft cold
  start (`]` prompt). Pressing **ESC** at any time also exits.
  Verified end-to-end in
  `BootDiskTests::CassoRocks_DemoDisk_DisplaysHgrCassowary`.

### Notes
- DHGR is intentionally not part of this cycle yet. Loading the
  DHGR aux page via stage 1's RWTS doesn't work directly: stage 1
  uses zero-page-adjacent pages $02-$03 as a 6-and-2 secondary-
  nibble buffer (writes via `STA $0256,Y` then reads back via
  `LDA $0300,Y`), and forcing RAMWRT-on around the disk read
  would route those scratch writes to aux while the reads still
  come from main, scrambling the expand. A clean way to add DHGR
  later would be to load the aux pattern into a main-RAM scratch
  area first (e.g. $6000-$7FFF) and then memcpy it to aux with
  RAMWRT toggled around just the copy.
- The //e text mode is monochrome on stock hardware (no per-glyph
  color), so there's no "TEXT" color test to add.

## [1.3.603]: 2026-05-14, The one with the HGR color fix

### Fixed (video)
- **HGR/LoRes/DHGR color palettes were rendering with R and B swapped**
  due to a byte-layout mismatch between the `0xAARRGGBB` notation
  used in `CassoEmuCore/Video/NtscColorTable.h`,
  `AppleLoResMode.cpp`, and `AppleDoubleHiResMode.cpp` and the
  `DXGI_FORMAT_R8G8B8A8_UNORM` swap-chain format set by
  `D3DRenderer.cpp`. Symptom: HGR `BLUE` rendered as orange and
  vice versa (anything in the //e's blue/orange palette pair came
  out swapped); LoRes and DHGR color indices 1 (Magenta), 2 (Dark
  Blue), 7 (Light Blue), 8 (Brown) all rendered as red shades.
  Violet/Green and the grays happened to be R/B-symmetric and
  rendered correctly by accident, hiding the bug from any HGR
  content that didn't lean on the blue/orange pair.
  Constants are now stored in R8G8B8A8 byte layout (so the
  little-endian `uint32_t` literal reads as `0xAA BB GG RR`).
  Affected golden-hash tests (`DhrTestPattern_HashMatches_Golden`,
  `US4_MixedMode_80Col_GoldenOutput`) and pixel-equality tests
  (`Render_SinglePixelPalette*`, `HiRes_NTSCArtifact_*`,
  `LoRes_TopBottomNybbles_*`) were updated with the corrected
  expected values.

### Added (demo)
- **Two-stage `casso-rocks` boot disk now toggles between the
  cassowary and a synthetic 6-color HGR test pattern on every
  keystroke.** Stage 1 (boot sector) loads the cassowary into HGR
  page 1 and reads track 3 (which holds 16 identical copies of
  stage 2) into `$1000-$1FFF`, then JMPs to the canonical stage 2
  copy at `$1000`. Stage 2 loads tracks 4+5 (the test bands) into
  HGR page 2, flips into HGR1, and runs a tiny self-modifying-code
  polling loop that flips PAGE2 between cassowary and bands on
  each keystroke. Stage 2 calls back into stage 1's still-resident
  RWTS subroutines (`load_a`, `load_b`, `read_track`,
  `read_sector`, `chk`, `wait_d5_aa`) via hard-coded entry-point
  addresses. Both source files (`Apple2/Demos/casso-rocks.a65` +
  `casso-rocks-stage2.a65`) plus the regenerated `casso-rocks.dsk`
  and the new `test-bands.hgr` framebuffer are committed.
- **`scripts/HgrPreprocess.py --pattern bands`** generates an 8 KB
  HGR framebuffer with 6 horizontal stripes
  (black/violet/green/white/blue/orange) covering all NTSC artifact
  colors the //e renderer can produce. Useful for diagnosing
  palette / byte-layout issues end-to-end through the disk + RWTS
  + renderer pipeline.

### Fixed (RWTS)
- **`read_track` in the casso-rocks demo now reads 18 sectors per
  track instead of 16** so that a phantom address mark caused by
  LSS resync immediately after a head step doesn't leave one real
  sector unread. Duplicate reads of the same logical sector just
  overwrite the destination page with identical bytes; the cost is
  ~25 KCycles per track. Symptom before the fix: stage 2's first
  read after the head step from track 3 to track 4 reliably
  dropped logical sector 3, leaving 256 bytes of HGR page 2 as
  random startup data. Only matters for boot-loader-style RWTS
  that doesn't have the standard DOS 3.3 head-settle delay; real
  DOS / ProDOS code is unaffected.

### Fixed (shell)
- **`Reset` and `Power Cycle` menu commands now re-read mounted slot-6
  disk images from the host filesystem.** Previously a Reset left the
  in-memory disk byte buffer untouched and a Power Cycle re-mounted
  using the cached path but didn't pick up external rewrites in a way
  the user could rely on (the dev workflow "regenerate `.dsk` outside
  Casso, hit Reset/Power Cycle to see the new image" silently kept
  showing the old image). Both menu commands now go through a new
  `RemountSlot6Disks` helper that snapshots the per-drive source
  paths and re-runs `MountDiskInSlot6` against each one, so the
  controller picks up whatever the file currently contains. Reset
  still preserves user RAM (real Apple ][ Ctrl-Reset semantics);
  Power Cycle still re-seeds DRAM. Auto-flush of dirty in-memory
  bytes still runs first so live writes aren't lost.

### Changed (demo)
- **Lighter title font in `scripts/HgrPreprocess.py`.** The previous
  Segoe UI Semibold 18px was too chunky on the cassowary HGR;
  defaulted to Segoe UI Regular 18px with no extra stroke. Two new
  CLI flags expose tuning without editing the script:
  `--title-size N` (default 18) and `--title-stroke N` (default 0;
  bump to 1 for a heavier look).

## [1.3.582]: 2026-05-13, The one where reset reloads disks

### Fixed (shell)
- **`Reset` and `Power Cycle` menu commands now re-read mounted slot-6
  disk images from the host filesystem.** Previously a Reset left the
  in-memory disk byte buffer untouched and a Power Cycle re-mounted
  using the cached path but didn't pick up external rewrites in a way
  the user could rely on (the dev workflow "regenerate `.dsk` outside
  Casso, hit Reset/Power Cycle to see the new image" silently kept
  showing the old image). Both menu commands now go through a new
  `RemountSlot6Disks` helper that snapshots the per-drive source
  paths and re-runs `MountDiskInSlot6` against each one, so the
  controller picks up whatever the file currently contains. Reset
  still preserves user RAM (real Apple ][ Ctrl-Reset semantics);
  Power Cycle still re-seeds DRAM. Auto-flush of dirty in-memory
  bytes still runs first so live writes aren't lost.

### Changed (demo)
- **Lighter title font in `scripts/HgrPreprocess.py`.** The previous
  Segoe UI Semibold 18px was too chunky on the cassowary HGR;
  defaulted to Segoe UI Regular 18px with no extra stroke. Two new
  CLI flags expose tuning without editing the script:
  `--title-size N` (default 18) and `--title-stroke N` (default 0;
  bump to 1 for a heavier look).

## [1.3.581]: 2026-05-13, The one with the better cassowary crop

### Changed
- **Tightened the cassowary crop** to capture the casque + head + neck
  through wattles, framed similar to the standard photo crop instead of
  the wider center-strip the first encoder produced. Source crop box
  is now baked into `scripts/HgrPreprocess.py` as `DEFAULT_CROP` and
  can still be overridden with `--crop`.
- **Per-byte HGR palette selection** in `scripts/HgrPreprocess.py`
  replaces the previous monochrome bit-pack. The encoder now
  classifies each source pixel into the 6-color HGR palette
  ({black, white, violet, green, blue, orange}), votes on a per-byte
  palette pair (bit 7 = 0 → violet+green, bit 7 = 1 → blue+orange,
  with blue/orange weighted 2× so a single color pixel doesn't lose
  to neighbouring leaf-greens), then places ON bits at the absolute
  pixel positions whose NTSC artifact phase matches the target
  color. Result: the leafy background renders solid green, the
  casque/wattles render orange/violet, and the head/neck reads as
  blue instead of every byte collapsing to a wash of white +
  green/violet stripes.
- **Letterbox-fit** added to the preprocessor for portrait subjects:
  fit the cropped image entirely inside 280×192 with black side
  bars instead of further center-cropping to landscape, so the
  cassowary keeps its full vertical proportion. Behavior can be
  reverted to the old fill-the-screen mode with `--no-letterbox`.

### Notes
- Apple ][ HGR's per-7-pixel-byte palette restriction is fundamental:
  within any 7-pixel column you get either {black, white, violet,
  green} OR {black, white, blue, orange}, never both. Thin features
  that straddle a palette boundary (a blue feather next to a green
  leaf) will compromise, one or the other gets the right color, or
  both wash to white via NTSC artifacting. The encoder is a
  best-effort first pass; future work could add Floyd-Steinberg
  error diffusion across byte boundaries.

## [1.3.579]: 2026-05-13, The one with the HGR cassowary

### Added
- **HGR cassowary demo on the bootable demo disk.** The
  `Apple2/Demos/casso-rocks.a65` boot sector now contains a from-
  scratch 6502 RWTS that, after the disk2.rom boot PROM hands off
  control, steps the head from track 0 to tracks 1 and 2, reads all
  16 sectors of each via direct LSS scanning of D5/AA/96 address
  marks and D5/AA/AD data prologues, performs the standard 6-and-2
  expansion in-place, and lands the 8 KB cassowary framebuffer at
  `$2000-$3FFF`. It then flips `TXTOFF` and `HIRESON` to reveal the
  image. The whole loader (including the 32-sector RWTS) fits in
  the 255-byte sector-0 boot stage. The framebuffer is generated
  deterministically by `scripts/HgrPreprocess.py` (Pillow,
  Floyd-Steinberg dither) from `Assets/3a Mrs Cassowary closeup
  8167.jpg` and committed alongside the source as
  `Apple2/Demos/cassowary.hgr`. Test renamed to
  `BootDiskTests::CassoRocks_DemoDisk_DisplaysHgrCassowary` and
  verifies both the soft-switch state (graphics on, mixed off,
  page2 off, hires on) and that `$2000-$3FFF` matches the on-disk
  framebuffer byte-for-byte.

## [1.3.577]: 2026-05-13, The one where the cursor investigation closed

### Documented (video)
- **80-col cursor at the BASIC prompt is intentionally a steady
  solid block, not a blink.** Stack-trace investigation confirmed
  Casso is now ROM-faithful: the //e enhanced firmware's wrapper
  at `$C905-$C90E` toggles the cursor ON, blocks in `$CB15` (the
  no-timeout keyboard poll), then toggles OFF on keypress. There
  is no outer software-blink loop. Per UTAIIe ch. 8 the //e video
  circuitry physically disables the FLASH attribute whenever
  `ALTCHARSET=1` (forced ON in 80-col mode), so the hardware blink
  used in 40-col mode is gone and the firmware does not replace
  it. 40-col mode keeps its checkerboard (`$7F`) flashing cursor
  via the normal video FLASH path. No code change required.

### Removed (test)
- Removed the temporary `Pr3_DiagnoseCursorBlink` diagnostic test;
  it served its purpose during investigation and is no longer
  needed now that the 80-col cursor behavior is understood.

## [1.3.576]: 2026-05-13, The one with the 80-column cursor fix

### Fixed (video)
- **The 80-col cursor (and any inverse-character cell) is now
  visible on the //e.** The //e enhanced video ROM stores the
  inverse-range slots ($00-$3F, in BOTH primary and alt sets)
  already in their visual / pre-inverted bitmap form (UTAIIe
  Tables 8.2/8.3, slot $20 holds the bitmap of "inverse SPACE"
  = solid block, not the bitmap of normal " "). The text-mode
  renderers were applying their own XOR-inversion on top of that
  (the ][ / ][ plus Decode2K convention), which re-inverted the
  pre-inverted bytes back to empty cells. Symptom: the BASIC
  cursor at main $0480 (= $20, inverse-space) rendered as
  invisible; any inverse-text screen output was blank. Fix:
  detect the //e ROM via `CharacterRomData::HasAltCharSet()` and
  skip the renderer's XOR-inversion for inverse slots when it's
  loaded; flash slots ($40-$7F primary) keep their XOR-toggle
  because those bytes alias the inverse range and the toggle is
  what alternates between stored-inverse and XORed-normal phase.
  Affects both `AppleTextMode` (40-col) and `Apple80ColTextMode`
  (80-col). New `VideoRenderTests::IIeRom_AppleTextMode_*` and
  `IIeRom_Apple80ColTextMode_InverseSpace_RendersSolidBlock` pin
  the contract using the real `Apple2e_Video.rom`.

## [1.3.575]: 2026-05-13, The one with the PR#3 cursor investigation

### Added (test)
- New `Pr3AuxClearTest::Pr3_StaticCursor_Lands_At_Main0480` pins the
  byte-level state of the //e PR#3 cursor cell as Casso currently
  produces it: prompt `]` ($DD) at aux $0480, inverse-space ($20)
  at main $0480, OURCH=1, CV=1.

### Investigation (incomplete)
- The `Pr3AuxClearTest::Pr3_Clears_AuxTextPage1_AllRows` FIXME
  comment previously hypothesized that the //e cursor-blink loop
  was "missing a VBL interrupt or 1MHz mouse-card clock". The
  comment now reflects a wider-scope investigation: an end-to-end
  scan of the main //e ROM ($C100-$FFFC) found no `LDA $C019`, no
  `EOR #$40`, no `ORA #$40`. **However**, on real //e hardware in
  80-column mode the cursor IS supposed to blink; that loop lives
  in the 80-column firmware at $C300-$C3FF, which is bank-switched
  in via the `INTC8ROM` / `SLOTC3ROM` soft switches when `PR#3` is
  active and is NOT in the main-ROM scan range. The PC trace
  observed spinning in main ROM's BASIC keyboard poll
  ($CB15-$CB1E) instead of the slot-3 firmware suggests Casso's
  bank-switch for the 80-column firmware isn't taking effect after
  `PR#3`; that is the actual bug to chase next.

## [1.3.574]: 2026-05-13, The one with the PowerCycle drive-state fix

### Fixed (disk)
- **Ctrl+Shift+R no longer leaves the drives empty.** The
  `IDM_MACHINE_POWERCYCLE` handler now snapshots the source paths of
  every disk currently mounted in `DiskImageStore` slot 6 before
  calling `EmulatorShell::PowerCycle`, then re-mounts them via
  `MountDiskInSlot6` once the controller has cycled. Previously the
  controller's `PowerCycle` re-pointed each engine at its empty
  internal sentinel disk, so a manual cold boot would leave Drive 1
  / Drive 2 ejected and the boot ROM had no nibbles to read. Test:
  unchanged controller-level `PowerCycleUnmountsAndFlushesAllDisks`
  contract continues to pass; the shell-level remount is wired in
  `EmulatorShell::ProcessCommands`.
- **Drive tooltip nibble counters reset on PowerCycle.**
  `DiskIINibbleEngine::Reset` now zeroes `m_readNibbles` and
  `m_writeNibbles` alongside the rest of the engine state, so the
  status-bar tooltip's `R:`/`W:` columns restart at 0 after Ctrl+Shift+R
  instead of carrying the pre-cycle counts. New
  `DiskIINibbleEngineTests::ResetClearsLifetimeNibbleCounters`
  pins the contract.

## [1.3.573]: 2026-05-13, The one with the friendly first run

### Added
- **Friendly first-run boot disk.** When a machine config has a Disk
  ][ controller (e.g. //e) and no disk is mounted in drive 1 from
  the CLI, the registry, or the user's session, Casso now prompts
  the user to download a stock Apple system master disk from the
  Asimov archive (https://www.apple.asimov.net), DOS 3.3 System
  Master (680-0210-A, 1982) or ProDOS Users Disk (680-0224-C). Both
  are size- and host-pinned. ][ / ][ plus configs (which have no Disk ][
  slot) are unaffected. A stale registry entry pointing at a
  deleted disk is treated as "no disk"; the entry gets cleared
  and the user is prompted again, instead of silently swallowing
  the boot.
- **Disk-image registry paths are now stored relative to `Casso.exe`.**
  When the disk lives under or beside the exe (the common case for
  the default `Disks/` peer dir), the per-machine `Disk1` /
  `Disk2` registry values hold a path like `Disks\Foo.dsk` instead
  of `D:\source\Casso\Disks\Foo.dsk`. The `Casso.exe` + `Disks/`
  tree is now portable across moves. Disks the user explicitly
  mounted from elsewhere (`E:\Games\Foo.dsk`) continue to be
  remembered as absolute paths. Existing absolute entries are
  read back as-is and rewritten relative on the next save.
- **Per-machine registry helpers extracted to `Casso/DiskSettings.h/.cpp`**
  so `Main.cpp` can read the saved disk before deciding whether to
  offer a boot-disk download. Disk-path conversion lives there; the
  registry layout (HKCU\Software\relmer\Casso\Machines\<machine>\
  {Disk1,Disk2}) is unchanged.
- **Apple-styled display strings throughout.** All user-visible
  Apple references now use the styling Apple's marketing did
  (`Apple ][`, `Apple ][ plus`, `Apple //e`, `Disk ][`). Comments still
  use plain ASCII (`Apple II`) since they're developer-facing.
- **Friendly first-run ROM bootstrap.** When a needed Apple ROM
  image is missing, Casso now lists the missing files and offers to
  download them from the AppleWin project in a single Yes/No dialog
  (HTTPS via WinHTTP), instead of failing with a terse error and
  exiting. The download placement honors the existing repo layout
  when present, otherwise drops the files into `ROMs/` next to
  `Casso.exe`. The set of ROMs Casso fetches is decided strictly
  from the JSON config embedded in `Casso.exe` for the chosen
  machine, if you've edited your on-disk `Machines/<machine>.json`
  to add extra slot ROMs, sourcing those is on you. New
  `AssetBootstrapTests` verify each shipped machine's required ROM
  list and disk-controller status end-to-end (loads `Casso.exe` as
  a resource module, parses its embedded JSON, asserts exact ROM
  filenames + slot 6 disk-ii presence).
- **Embedded default machine configs.** `Apple2.json`,
  `Apple2Plus.json`, and `Apple2e.json` are now bundled as resources
  in `Casso.exe` and extracted on first run when no `Machines/`
  directory is found, so a loose `Casso.exe` is enough to get a
  picker on screen.
- **In-house bootable demo disk** under `Apple2/Demos/`. The
  `casso-rocks.a65` source assembles to a 45-byte sector-0 program
  that displays "CASSO ROCKS!" centered on the text screen. New
  `BootDiskTests::CassoRocks_DemoDisk_PrintsBanner` runtime-assembles
  the demo through Casso's own assembler, builds a synthetic `.dsk`,
  boots through the real `Disk2.rom`, and verifies the banner, and
  also emits `casso-rocks.dsk` next to the source for direct GUI use.
  Replaces the project's previous reliance on the copyrighted DOS 3.3
  master image for end-to-end boot validation.

### Changed (CI)
- **ROM and machine-config filenames now use `<MachineType>[_Suffix]`
  casing**: `Apple2.rom`, `Apple2Plus.rom`, `Apple2e.rom`,
  `Apple2_Video.rom`, `Apple2e_Video.rom`, `Disk2.rom` (and
  `Apple2eEnhanced.rom` / `Disk2_13Sector.rom` in the AppleWin
  catalog); machine configs renamed to `Machines/Apple2.json`,
  `Apple2Plus.json`, `Apple2e.json`. Affects machine configs,
  `scripts/FetchRoms.ps1`, the embedded resources + in-app
  downloader catalog, fixtures, tests, and docs. Existing local
  `ROMs/` directories should be renamed to match (case-only on
  Windows requires a two-step `git mv`). The `--machine` CLI flag,
  registry-stored last machine, and `fs::exists` lookups remain
  case-insensitive on NTFS, so old values still resolve.
- `Disks/` (local disk image cache, may contain copyrighted images)
  is now `.gitignore`d.
- `CatalogReproductionTest` and `Pr3AuxClearTest` now resolve repo
  files via an upward walk (matching `BackwardsCompatTests`) instead
  of hardcoded `C:\Users\…` paths, and skip cleanly when their input
  disk image / ROM is absent (CI runners don't have them).

## [1.3.536]: 2026-05-10, The one with Disk II and //e text fidelity

### Fixed (disk)
- **DOS 3.3 boots from `.dsk` images.** Disk II nibblization corrected:
  10-bit sync nibbles (`PackSyncNibbleBits`); standard data-field XOR
  convention (each on-disk nibble = `encoded[i] XOR encoded[i-1]`,
  checksum nibble = final raw encoded byte); standard Disk II LSS read
  latch model with continuous-shift + 7 µs data-ready hold.
- **CATALOG works on real DOS 3.3 master disk.** Two latent bugs in
  the Disk II controller surfaced once boot succeeded:
  - Motor spindown delay added (~1 second per UTAIIe ch. 9 / AppleWin
    `SPINNING_CYCLES`). DOS RWTS toggles motor off / on between
    commands and depends on the disk physically continuing to spin
    during that window.
  - Phase-stepper now uses the adjacency-pull model (`±2` quarter-tracks
    per single-magnet pull, `±1` for the four "two-adjacent-magnets-on"
    states `$3/$6/$C/$9`) instead of the old "highest set bit"
    approximation, which accumulated drift across multi-track seeks
    and landed CATALOG's 17-track seek on the wrong sectors.
- **`DiskIIController::Tick`** now actually runs on the GUI CPU thread
  (previously was wired only in tests).
- **Per-machine disk auto-mount** persists in
  `HKCU\Software\relmer\Casso\Machines\{machine}\Disk1|Disk2` and is
  restored on machine switch / power cycle.
- **PowerCycle before MountCommandLineDisks** at startup: PowerCycle
  ejects every drive, so the previous order silently discarded the
  freshly-mounted image.

### Fixed (//e video)
- **PR#3 (80-column mode) renders blank cells, not garbage.** The
  `Decode4K` path for the //e enhanced video ROM was loading the
  alternate character set from the wrong half of the 4 KB file. Now
  matches UTAIIe ch. 8 (Sather) Tables 8.2 / 8.3: alt set's 256 chars
  all live in the first 2 KB of the file. Bug was latent until Phase
  12 added ALTCHARSET support to the 80-col renderer (audit M13).

### Fixed (//e UI / keyboard)
- **Edit > Copy Text** now reads the text page through `MemoryBus`
  rather than `m_cpu->GetMemory()`. The MMU owns its own RAM device(s)
  on the //e, so writes from firmware land in the bus-side buffer; the
  CPU's internal `memory[]` mirror was a stale copy unrelated to what
  appears on screen.
- **Alt key** routes through the emulated keyboard so Open / Closed
  Apple modifiers work; the Win32 layer no longer eats the modifier.
- **Soft reset** preserves modifier-key latches.

### Added (UI)
- **Drive 1 / Drive 2 activity LEDs and tooltips** in the status bar.
  Tooltips show mount path, current track, and read / write nibble
  counters.

### Added (tests)
- Real-ROM boot-decoder tests (`BootRomDecoderTests`), drive the
  actual `Disk2.rom` slot 6 firmware on the emulated 6502 against
  synthetic disks; gates the on-disk format against the real Apple
  firmware's checksum routines.
- Direct-bus readback tests (`DiskReadbackTests`), all 35 tracks ×
  16 sectors round-trip bit-perfect through the nibblizer + LSS
  without a CPU.
- End-to-end CATALOG repro test (`CatalogReproductionTest`), boots
  real `dos33-master.dsk`, types `CATALOG`, asserts directory listing
  is printed (no I/O ERROR).
- 80-col PR#3 alt-set decoder gates (`Pr3AuxClearTest`).

### Known follow-ups
- 80-col cursor invisible after PR#3 (cursor-blink loop never runs;
  likely VBL-interrupt or similar timer wiring).
- Tooltip stats stale after PowerCycle.
- Disk subsystem broken after PowerCycle (counters don't advance).
- Bursty cosmetic update of nibble counters in the status bar.



## [1.3.509]: 2026-05-09, The one with the Apple //e (spec 004, Phases 0-16)

The bulk of this entry completes Apple //e fidelity work begun in
`[1.3.416]`. After this release the //e cold-boots to BASIC, runs Disk II
images (`.dsk`/`.do`/`.po`/`.woz`), renders 80-column text and Double
Hi-Res, honors auxiliary RAM and the Language Card state machine,
distinguishes soft reset from power cycle, and exposes a cycle-accurate
IRQ/NMI infrastructure.

### Added (CPU + interrupts: Phase 1)
- **`Cpu6502`** adapter implementing the new `ICpu` and `I6502DebugInfo`
  contracts. Lets the emulator core be re-targeted without reaching into
  legacy `EmuCpu` internals.
- **`InterruptController`** with up to 32 named sources, edge/level
  semantics, and per-source assert/clear. Wired into the CPU dispatch so
  `IRQ` and `NMI` vectors fire on the next instruction boundary.
- **IRQ / NMI dispatch path** validated against the 6502 hardware spec
  (7-cycle entry, status-register I-bit set on entry, B-bit clear on
  vectoring, vector fetch from `$FFFE/$FFFF` and `$FFFA/$FFFB`).

### Added (memory + Language Card: Phase 2 / 3)
- **`AppleIIeMmu`** owns the //e bank-switching state (`RDRAMRD`,
  `RDRAMWRT`, `RDCXROM`, `RDC3ROM`, `RDALTZP`, `RD80STORE`, `RDPAGE2`,
  `RDHIRES`) and replaces the legacy `AuxRamCard`. `Apple2.json` and
  `Apple2Plus.json` continue using their legacy banks; `Apple2e.json` is
  the only config that wires the MMU.
- **64 KB auxiliary RAM** mapped through the MMU. Aux Zero Page / Stack
  toggled via `ALTZP`. 80STORE forces the page-2 / Hi-Res text + graphics
  windows onto the aux bank when set.
- **Audit-correct Language Card state machine** with read-source decoded
  from bits 0 **and** 1 (the old bit-0-only path missed `$C083`),
  bank-1 / bank-2 selection per `BSRBANK2`, and write-enable latched on
  the second consecutive read of an `$C08x` write-enable address.
- **`INTCXROM` physical remap**: `$C100-$CFFF` switches between the
  internal //e ROM and slot peripheral ROMs.

### Added (reset: Phase 4)
- **`SoftReset` vs. `PowerCycle`** semantics on every device and on the
  CPU. Soft reset preserves RAM contents, leaves the Language Card in
  its current bank state, keeps soft switches that survive Ctrl-Reset on
  real hardware, and re-vectors via `$FFFC`. Power cycle re-randomises
  RAM, returns Language Card / MMU / video-mode bits to their cold-boot
  defaults, and clears the keyboard latch + IRQ controller.
- **`EmulatorShell` reset dispatch** routed through a single `Reset(IDM)`
  contract (`IDM_RESET_SOFT` / `IDM_RESET_POWER`) so the menu, debug
  console, and remote (headless) command paths all funnel through one
  authoritative path.

### Added (video timing + RDVBLBAR: Phase 5)
- **`VideoTiming`** model: 65 cycles per scanline × 262 scanlines =
  17,030 cycles per frame; tracks current scanline, cycle-in-frame, and
  vertical-blank window. Exposed to soft-switch reads so `RDVBLBAR`
  ($C019) reflects real hardware polarity (bit 7 = 1 outside vblank).
- **FR-033** (vblank polarity) covered with dedicated tests.

### Added (keyboard + soft-switch read surface: Phase 6, baseline 1.3.416)
- **Open Apple / Closed Apple / Shift modifiers** at `$C061-$C063`,
  wired to host **Left Alt / Right Alt / Shift**.
- Strobe-clear isolation (`$C010` only) and a consolidated
  `$C011-$C01F` status-read surface where bit 7 is sourced from the
  canonical owner.

### Added (cold boot to BASIC: Phase 7, US1 MVP)
- **//e cold boot reaches the AppleSoft prompt.** `EmulatorShell` now
  pumps reset → boot wait → `]` prompt detection. Verified via the
  HOME / `PRINT "HELLO"` / `PR#3` 80-column / Open-Apple modifier
  scenarios in `Phase7ColdBootTests` and `EmulatorShellColdBootTests`.
- **Scraper / injector helpers** for headless tests: video-text scraper
  reads the canonical 40/80-column buffer; keyboard injector queues
  ASCII strings at the bus level without host-window dependencies.

### Added (US3 //e memory + Language Card scenarios: Phase 8)
- 24 acceptance scenarios in `EmuValidationSuiteTests` covering aux RAM
  hot-swap, ALTZP, 80STORE + PAGE2 + HIRES interactions, Language Card
  bank-1 / bank-2 / write-enable transitions, and `INTCXROM` slot ROM
  remapping. Validates SC-006 / SC-007.

### Added (Disk II nibble engine + WOZ: Phase 9 / 10)
- **`DiskIINibbleEngine` rewrite**: cycle-accurate bit-stream model
  (4 µs / bit at 1.023 MHz), Q3 sample timing, Q6/Q7 latch, write-protect
  flag, and per-track read/write head. Replaces the previous
  byte-oriented stub.
- **`NibblizationLayer`** for `.dsk` / `.do` / `.po` images. 16-sector
  6&2 group code with valid prologue / epilogue triplets, address-field
  + data-field checksums, DOS 3.3 vs. ProDOS sector skews, and a
  reverse `Denibblize` path for write-back.
- **`WozLoader`** for WOZ v1 and v2 images including TMAP / TRKS chunks,
  variable-length tracks (`bitCount`, not byte count), large-track
  support up to ~51,200 bits, and signature validation against the
  WOZ-spec `kSigV1` / `kSigV2` headers.

### Added (DiskImageStore + headless wiring: Phase 11)
- **`DiskImageStore`**: uniform handle layer. Open / GetTrackBitCount /
  ReadBit / WriteBit / IsDirty / Save. Supports both nibblized and WOZ
  images behind one interface.
- **Auto-flush on eject** and on shell shutdown, with dirty-tracking so
  unmodified images are not rewritten.
- **`HeadlessHost`** test harness, drives the emulator without a host
  window; lets test fixtures schedule cycles, read framebuffers, inject
  keystrokes, and mount / eject disks deterministically.

### Added (text + DHR video: Phase 12)
- **`Apple80ColTextMode`** with `ALTCHARSET`, `FLASH` half-second blink
  cadence, and composed mixed-mode (top 160 lines graphics, bottom 32
  lines text) from a single shared character ROM source.
- **`AppleDoubleHiResMode`**: 560×192 monochrome / 140×192 16-color
  Double Hi-Res with proper aux/main interleave (aux byte first, then
  main, packing 7 pixels per byte pair). DHR mode-select gated on
  `RDHIRES & RD80VID & RDDHIRES`.
- **Golden-hash framebuffer tests** that re-execute canonical software
  patterns and compare exact frame hashes; covers BASIC `]` prompt,
  GR / HGR / HGR2 mode patterns, and 80-column DOS catalogs.

### Added (disk boot end-to-end: Phase 13)
- 8 disk-boot integration scenarios: synthetic DOS 3.3 boot, mixed-mode
  scroll, 80-column ProDOS catalog, write-protect honored, save +
  reload round-trip, WOZ copy-protected sample boot, multi-sided image
  fallthrough, and motor-off head-park.

### Added (backwards-compat: Phase 14)
- `BackwardsCompatTests` regression-protect the unchanged Apple ][ and
  ][ plus behavior: keyboard latch, soft-switch surface, video modes, no
  MMU activity, no aux RAM, no IRQ controller. Audit log
  (`audit-backwards-compat.md`) documents the verification.

### Added (perf budget: Phase 15)
- **Performance gate**: `PerformanceTests` measures emulator throughput
  on a workload of `kPerfMeasureCycles` and asserts elapsed wall-clock
  ≤ `kPerformanceCeilingMs`. Stability run (`kStabilityRunCount`)
  enforces ≤ `kStabilityToleranceFraction` variance. Released-only
  (skipped in Debug). Documented in `phase15-perf-protocol.md`.

### Added (constitution audits + final gate: Phase 16)
- 8 constitution audits under `specs/004-apple-iie-fidelity/audit-*.md`
  covering header comments, macro arguments, function spacing,
  EHM-on-fallible, scope blocks, function size, declaration alignment,
  and magic numbers. All audits report PASS.
- 23 declaration-alignment cleanups (whitespace only, T130) across 16
  files.
- Dormann functional test PASS, Harte single-step suite PASS.
- 0 warnings / 0 errors on all four configurations
  (Debug/Release × x64/ARM64) with `/W3 /WX /sdl /analyze`.

### Tests
- Test count: **1013 / 1013 passing** in Release (1012 / 1012 in Debug,
  the +1 is the `PerformanceTests` sentinel that skips in Debug).
  Confirmed clean across x64 Debug + Release and ARM64 Debug + Release.
  Code analysis 0/0 on all four configurations.
- New test surface (selected): `Cpu6502Tests`, `InterruptControllerTests`,
  `MmuTests`, `LanguageCardTests`, `ResetSemanticsTests`,
  `EmulatorShellResetTests`, `VideoTimingTests`, `Phase7ColdBootTests`,
  `EmuValidationSuiteTests` (US3, US5), `DiskIINibbleEngineTests`,
  `NibblizationTests`, `WozLoaderTests`, `DiskImageStoreTests`,
  `DiskIITests`, `Phase11IntegrationTests`, `Apple80ColTextModeTests`,
  `AppleDoubleHiResModeTests`, `Phase12GoldenHashTests`,
  `Phase13DiskBootTests`, `BackwardsCompatTests`, `PerformanceTests`,
  `HeadlessHostTests`, `PrngTests`.

### Notes
- Closes spec 004 (Apple //e fidelity), Phases 0 through 16. SC-001
  through SC-009 met. The headless test harness (`HeadlessHost`,
  `FixtureProvider`, scraper / injector helpers) is now the canonical
  path for emulator integration tests.

## [1.3.416]: 2026-05-06

### Added (Apple //e fidelity: Phase 6: keyboard + soft-switch read surface)
- **Open Apple / Closed Apple / Shift modifiers** are now reachable at the
  expected //e addresses:
  - `$C061`: Open Apple (bit 7 = pressed). Wired to host **Left Alt**.
  - `$C062`: Closed Apple (bit 7 = pressed). Wired to host **Right Alt**.
  - `$C063`: Shift key (bit 7 = pressed). Wired to host **Shift**.
  Previously the modifier-key fields existed on `AppleIIeKeyboard` but the
  device's bus range stopped at `$C01F`, making them dead code.
- **Strobe-clear isolation**. Reads of `$C011-$C01F` (BSRBANK2 / BSRREADRAM /
  RDRAMRD / RDRAMWRT / RDCXROM / RDALTZP / RDC3ROM / RD80STORE / RDVBLBAR /
  RDTEXT / RDMIXED / RDPAGE2 / RDHIRES / RDALTCHAR / RD80VID) no longer
  clear the keyboard strobe. Only `$C010` clears it, matching the //e
  hardware. (Audit §4 C-item closed.)
- **Consolidated `$C011-$C01F` status read surface** in
  `AppleIIeSoftSwitchBank::ReadStatusRegister()`. Bit 7 is sourced from the
  canonical owner of each flag (LanguageCard for BSRBANK2/BSRREADRAM, MMU for
  RDRAMRD/RDRAMWRT/RDCXROM/RDALTZP/RDC3ROM/RD80STORE, VideoTiming for
  RDVBLBAR, the bank for the display-mode flags), and bits 0-6 mirror the
  keyboard latch (floating-bus behavior).
- **`AppleIIeKeyboard` is now a `$C000-$C063` facade** that forwards
  non-owned addresses to its sibling devices (soft-switch bank for
  `$C00C-$C00F` / `$C011-$C01F` / `$C050-$C05F`; speaker for `$C030-$C03F`).
  This preserves the unchanged ][ / ][ plus behavior where `AppleKeyboard` only
  owns `$C000-$C01F`.

### Tests
- `+10` keyboard tests in `KeyboardTests.cpp` covering modifier reachability,
  strobe-clear isolation, and audit-closure assertions.
- `+15` new tests in `SoftSwitchReadSurfaceTests.cpp`, one per `$C011-$C01F`
  address, that assert (a) bit 7 reflects the canonical source, (b) bits
  0-6 mirror the keyboard latch, (c) the read does not clear strobe, and (d)
  repeat reads do not perturb state.
- Test count: **906 / 906 passing** (was 881; +25). Confirmed clean across
  x64 Debug + Release and ARM64 Debug + Release; code analysis 0/0.

### Notes
- Closes the foundational Apple //e fidelity work (spec 004 Phases 0-6).
  Phase 7 (User Story 1 MVP cold boot) is the next planned increment.

## [1.2.315]: 2026-05-04

### Added
- **Character generator ROM loading**: text mode renderers now load the real
  Apple character ROM file (`Apple2_Video.rom` for ][ / ][ plus, `Apple2e_Video.rom`
  for the //e) instead of the embedded 96-character fallback. Fixes:
  - **//e cursor** is now visible (the cursor character was outside our embedded
    range)
  - **//e boot logo** "Apple ][" displays fully (the missing characters were also
    outside our embedded range)
  - All 256 character codes render correctly across inverse, flash, and normal regions
- **CharacterRomData** loader (`CassoEmuCore/Video/CharacterRomData.h/.cpp`)
  decodes both 2KB (][ / ][ plus) and 4KB (//e enhanced) ROM formats including the
  bit-reversed 2KB layout and the //e's primary + alt char set arrangement.
  Falls back to embedded $20-$5F glyphs if no ROM file is configured.

## [1.1.311]: 2026-05-04

### Changed
- **Machine config schema v2**: breaking change. Refactored from a single `memory[]`
  array with conditional fields into clear sections:
  - `ram[]`: RAM regions with `address` + `size` (and optional `bank`)
  - `systemRom`: singular system ROM (`address` + `file`; size derived from file)
  - `characterRom`: character generator ROM (file only)
  - `internalDevices[]`: motherboard I/O (just `type`)
  - `slots[]`: expansion cards (`slot`, optional `device`, optional `rom`)
  All three machine configs (`Apple2.json`, `Apple2Plus.json`, `Apple2e.json`)
  migrated to the new schema.
- **ROM size validation**: system ROM file size now determines the end address
  automatically (no more start/end mismatch bugs)
- **Slot ROM auto-mapping**: slot ROMs auto-map to `$C000 + slot * 0x100`,
  required to be exactly 256 bytes

### Added
- **FetchRoms.ps1**: expanded to download all peripheral card ROMs from AppleWin:
  Disk II 13-sector, Mockingboard, Mouse Interface, Parallel printer, Super Serial
  Card, ThunderClock Plus, HDC SmartPort, Hard Disk drivers, Apple //e Enhanced
  system ROM
- **Apple //e Disk II slot ROM**: `Disk2.rom` now loads at $C600-$C6FF (slot 6)
  via the new schema, satisfying the //e autostart scan

## [1.0.307]: 2026-05-04

### Added
- **Machine picker dialog**: modal Win32 ListView showing all `Machines/*.json` configs
  with display names from the JSON `name` field; shown at startup if no last-used
  machine, when clicking the status bar Machine panel, or via File > Switch Machine
- **Last-used machine persistence**: stored in registry at `HKCU\Software\relmer\Casso`
- **Hot-swap machine switching**: pause CPU, tear down devices/bus/cpu/video,
  reload config, reinitialize, resume, works from menu, status bar, or startup
- **Random RAM on cold boot**: RAM ($0000-$BFFF) initialized with random values to
  match real DRAM power-on behavior (Apple II shows random characters at boot)
- **80STORE soft switch tracking**: //e keyboard intercepts $C000/$C001 writes to
  track 80STORE state; video mode selection suppresses page2 when 80STORE is active
- **ROM size validation**: RomDevice rejects ROM files that don't match the configured
  address range size, with a clear error message
- **Illegal opcode handling**: CPU treats illegal opcodes as 1-byte NOPs (2 cycles)
  with a debug log message instead of crashing

### Fixed
- **//e boot**: corrected ROM start address from $C100 to $C000 (16KB ROM); slot ROM
  trimmed to $C100-$CFFF to avoid shadowing I/O space at $C000-$C0FF
- **Language Card state machine**: corrected read source decoding to use both bits 0
  and 1 (was using only bit 0); $C083 now correctly enables Read RAM + Write Enable
- **CpuOperations RMW operations**: Decrement, Increment, RotateLeft, RotateRight now
  use ReadByte/WriteByte instead of direct memory[] access, so they correctly route
  through the bus for I/O-mapped addresses
- **EmuCpu memory routing**: reads and writes for $C000+ now go through the
  MemoryBus, so the LanguageCardBank is consulted for $D000-$FFFF (was reading stale
  ROM from memory[] which caused //e BASIC to fail)
- **CreateMemoryDevices aux RAM handling**: RAM regions with a `bank` field (e.g.
  "aux") are skipped in main RAM creation; aux memory is handled by AuxRamCard
- **MemoryBus::Validate**: overlapping I/O devices are now warnings (logged via
  DEBUGMSG) instead of errors; first-match-wins is the correct hardware behavior

### Changed
- **Machine display names**: "Apple ][", "Apple ][ plus", "Apple //e"
- **File menu**: "Open Machine Config" renamed to "Switch Machine..." and ungrayed
- **Status bar**: clicking the Machine panel opens the picker dialog
- **Cpu::Reset**: removed all hardcoded test instructions and PC=$8000 setup;
  now just initializes registers/flags/memory
- **Cpu member initializers**: moved from constructor initializer list to in-class
  defaults
- **VS Code IntelliSense config**: added CassoEmuCore/Pch.h to forcedInclude so
  `<random>` and other STL headers resolve correctly

### Removed
- **Cpu::Run()**: dead code (never called); CLI uses its own StepOne loop

## [1.0.244]: 2026-05-03

### Added
- **Apple II platform emulator (Casso.exe)**: GUI-based Apple ][, ][ plus, and //e emulator
  with D3D11 rendering, WASAPI audio, data-driven JSON machine configs, and keyboard input
- **CPU thread architecture**: dedicated CPU thread for 6502 execution and audio,
  UI thread for Win32 messages and D3D Present with vsync
- **Status bar**: shows CPU type, clock speed (MHz), machine name, and device count;
  clicking devices shows a popup listing all bus-mapped devices with address ranges
- **Edit menu**: Copy Text (reads 40×24 text screen as ASCII), Copy Screenshot
  (framebuffer as DIB bitmap), Paste (Ctrl+V feeds clipboard into keyboard)
- **Cycle-accurate instruction timing**: baseCycles in Microcode with runtime page-cross
  and branch-taken penalties
- **Pending audio buffer**: decouples PCM generation from WASAPI drain to prevent stutter
- **DPI-scaled debug console font**: uses GetDpiForWindow + MulDiv

### Changed
- **Project rename**: Casso65Core → CassoCore, Casso65EmuCore → CassoEmuCore,
  Casso65Emu → Casso, Casso65 → CassoCli; repo renamed to relmer/Casso
- **Exact NTSC timing**: CPU clock 1,022,727 Hz (was 1,023,000), cycles/frame 17,030
  (was 17,050); derived from 14.31818 MHz crystal
- **Speaker amplitude**: ±0.25f (was ±1.0f) to match reference audio levels
- **WASAPI buffer**: 100ms (was 33ms) for jitter headroom
- **D3D vsync**: Present(1) on UI thread, Present(0) was double-gating with frame timer
- **using namespace std** + **namespace fs** in both Emu Pch.h files
- **In-class member initialization** preferred over constructor initializer lists
- **Casso65Emu flattened**: removed Audio/, Shell/, Resources/, shaders/ subdirectories
- **machines/ → Machines/, roms/ → ROMs/**: directory casing standardized

### Fixed
- **Mixed-mode text flicker**: framebuffer race condition; CPU thread now copies completed
  framebuffer to UI buffer under mutex
- **Hi-res NTSC colors**: two-pass renderer correctly handles cross-byte-boundary adjacent
  pixels; HCOLOR=3 renders as solid white
- **Power cycle**: now clears RAM ($0000-$BFFF) for cold boot with APPLE ][ banner
- **Paste drops characters**: DrainPasteBuffer now checks keyboard strobe before feeding
  next character
- **Duplicate AddDevice**: every device was registered twice on the memory bus
- **Bus overlap detection**: Validate() uses CBRN with specific conflicting address ranges
- **Title bar garbage**: em-dash encoded as UTF-8 in source, replaced with \\u2014 escape
- **Debug console newlines**: bare LF converted to CRLF for Win32 EDIT control
- **Audio buzz during boot**: capped submission to one frame, pre-filled silence
- **Green screen**: CPU opcode fetch now uses virtual ReadByte through MemoryBus
- **Black screen**: D3D11 shaders implemented via runtime D3DCompile
- **ParseHexAddress**: overflow and invalid-char validation added

## [0.9.32]: 2026-04-28

### Added
- Tom Harte SingleStepTests: per-opcode validation against 151 legal-opcode test sets (10,000 vectors each)
- `run` subcommand, load and execute a binary or assembly source from the CLI
- **Full AS65-compatible assembler**: from-scratch reimplementation of Frank A. Kingswood's AS65
  - All 56 mnemonics, all 14 addressing modes
  - Two-pass assembly with forward-reference resolution
  - Full expression evaluator: `+ - * / % & | ^ ~ << >>`, `<`/`>` byte selectors, current-PC `*`
  - Constants: `equ`, `=`, `set` with forward-reference chains
  - Conditional assembly: `if`/`ifdef`/`ifndef`/`else`/`endif`
  - Macros: `name macro`…`endm`, positional `\1`–`\9` and named parameters, `local`, `exitm`, `\?` unique suffix
  - Directives: `.org`, `.byte`, `.word`, `.text`, `.ds`, `.dd`, `.align`, `.end`, `.error`, `.include`
  - Struct definitions with `struct`/`end struct` and dot-qualified member access
  - Character map (`.cmap`) for custom character encodings
  - Three-segment model: `code`/`data`/`bss`
  - Binary file includes: `.bin`, `.s19`, `.hex` (incbin-style)
  - Backslash line continuation in macros
  - Colon-less labels (AS65 compatibility)
  - Listing output: `-l [file]`, `-c` cycle counts, `-m` macro expansion, `-t` symbol table
  - Output formats: flat binary (default), `-s` Motorola S-record, `-s2` Intel HEX
  - Warning control: `--warn`, `--no-warn`, `--fatal-warnings`
  - Flag concatenation: `-tlfile` ≡ `-t -l file` (AS65 style)
  - 10-test conformance suite verifying AS65 parity
- Dormann and Harte regression test suites with on-demand runner scripts

### Fixed
- BCD flag behavior (N/V/Z flags match real 6502 hardware)
- JMP indirect page-boundary wrap bug
- JSR operand read overlapping stack push
- DEY/PLA opcode table swap
- STY missing source register in absolute mode
- Addressing mode wrapping for zero-page indexed modes
- Assembler: `equ` forward-reference chain resolution, `ifdef`/`ifndef`, `-s2` Intel HEX output, listing column layout, `.org` gap fill
- LDX/INC/DEC addressing mode table entries
- STY missing Absolute addressing mode (#37)

## 2026-04-25

### Changed
- Project renamed from **My6502** to **Casso**

## 2026-04-24

### Added
- **Assembler v1** (spec 001), basic two-pass assembler with labels, branches, directives, expressions, listing output, and CLI subcommands
- `LoadBinary()`: load pre-assembled binaries into CPU memory
- CI pipeline with GitHub Actions (x64 + ARM64, Debug + Release)

### Fixed
- `ShiftLeft`/`ShiftRight` dispatch (was calling `RotateLeft`/`RotateRight`)
- `BIT` instruction V/N flags (were read from AND result instead of operand)
- `Compare` carry flag for boundary values
- `PushWord`/`PopWord` read/write outside stack page

## 2026-04-23

### Added
- Extracted `CassoCore` static library from monolithic project
- `UnitTest` project with 66 initial tests (Microsoft Native CppUnitTest)
- Build/test automation scripts and VS Code tasks

## 2024-12-15

### Added
- Stack and memory helpers, rewritten addressing mode resolution
- `BRK` (software interrupt) implementation

### Fixed
- `LDX`, `DEX`, `BMI`, `BPL`, `INX` behavior corrections

## 2024-12-08

### Added
- Flag manipulation (`CLC`, `SEC`, `CLI`, `SEI`, `CLV`, `CLD`, `SED`), register transfers (`TAX`, `TXA`, `TAY`, `TYA`, `TXS`, `TSX`), `NOP`

## 2024-11-24: 2024-11-30

### Added
- Initial 6502 emulator: fetch-decode-execute cycle, all 56 standard mnemonics
- **Group 01**: `ORA`, `AND`, `EOR`, `ADC`, `STA`, `LDA`, `CMP`, `SBC`, all 8 addressing modes
- **Group 00**: `BIT`, `JMP`, `STY`, `LDY`, `CPY`, `CPX`
- **Group 10**: `ASL`, `LSR`, `ROL`, `ROR`, `STX`, `LDX`, `DEC`, `INC`
- All 14 addressing modes (immediate, zero page, ZP,X, ZP,Y, absolute, abs,X, abs,Y, (ZP,X), (ZP),Y, indirect, relative, accumulator, implied, jump absolute)
