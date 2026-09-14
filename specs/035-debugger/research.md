# Research: Debugger

Each entry records a decision, the reason for it, and the alternatives
considered. Code references are to this branch at `a9c36cc9`.

## R-001: Where the engine attaches

**Decision**: Attach to `MachineHost`, the windowless machine that both
`EmulatorShell` and `TestMachine` already sit on.

**Rationale**: `MachineHost` owns the CPU, bus and devices, and has `StepOne`
(`Shell/MachineHost.cpp:184`) and `RunCycles` (`:227`). Attaching there gives
batch mode, the emulator and tests one integration point.
`MachineManager::SwitchMachine` (`MachineManager.cpp:190`) already re-attaches
debug sinks through `AttachDebugSinksIfOpen` (`:549`), which is where the
session re-attaches and clears breakpoints (spec edge case).

**Alternatives**: attaching to `Cpu` directly, which cannot see banking or
devices; attaching to `EmulatorShell`, which batch mode does not have.

## R-002: One intermediate command form

**Decision**: Both parsers emit `DebugCommand` values, and one `DebugSession`
executes them.

**Rationale**: FR-012 requires shared state, and SC-006 requires identical
behavior across the ways in. Many commands exist in both modes: AppleWin `M`
and Monitor `<...M` are the same move, and AppleWin `A` and Monitor `!` are the
same assembler. Implementing each once removes a whole class of divergence.

**Alternatives**: two engines sharing tables, which would duplicate every
behavior; translating Monitor text into AppleWin text, which is lossy for
Monitor-only forms like `^E` register edit and `I`/`N`.

## R-003: Side-effect-free memory view

**Decision**: A new `DebugMemoryView` resolves an address to its current
backing store (main RAM, aux RAM, language-card RAM bank 1 or 2, a ROM
region, slot ROM, internal //c ROM bank, or I/O) using the existing state
accessors: `IMmu::GetRamRd/GetRamWrt/GetAltZp/Get80Store/GetIntCxRom/
GetSlotC3Rom`, `LanguageCard::IsReadRam/IsBank2/ReadRom`, and
`Apple2cRomBank`. It reads and writes that store directly. The I/O page
$C000-$C0FF is never read through a device. Examine prints it as unreadable
(`--`) unless the user explicitly issues `IN`, which is a real bus read by
design.

**Rationale**: `MemoryBus::ReadByte` has side effects (`AppleSpeaker::Read`
toggles the speaker, and soft switches flip on read). `Cpu::PeekByte` reads
the raw array and misses banking. `Cpu::PeekForTrace` uses the read-page
table but cannot serve I/O pages. FR-006 requires "which of ROM, RAM or
language-card memory is mapped", so the view also returns a region label per
address.

**Writes**: poking RAM writes the store currently mapped for writing. Poking a
ROM region reports that the address is read-only and changes nothing. A write
that lands in I/O is a real bus write, since the user asked for it.

**Test**: for each machine, over every non-I/O address and a set of banking
configurations, the peek equals what a bus read would return, and device state
is unchanged after a full peek sweep.

**Alternatives**: a `Peek` method on every `MemoryDevice`, which touches
dozens of device classes to serve one consumer; snapshotting the bus, which
cannot capture device-backed ROM banking.

## R-004: Per-instruction hook

**Decision**: `MachineHost` holds a `DebugHook *` (null by default). Before
each instruction, `StepOne` and the `RunCycles` loop call
`hook->ShouldStopBefore(pc)` when the pointer is non-null. The session answers
from a 64 KB bitmap of enabled address breakpoints first, then from opcode and
condition breakpoints only if any exist. Memory watchpoints are served by a bus
observer that is installed only while at least one watchpoint exists; a hit
sets a pending-stop flag that the hook checks at the next instruction boundary.

**Rationale**: A pointer test per instruction is below measurement noise
(see the `reference-measure-per-instruction-perf` method: microbenchmark
before and after). Stopping before the instruction is what AppleWin and the
Monitor step semantics expect. Watchpoint stops land after the accessing
instruction completes, which is the only consistent point on a 6502; the stop
reply reports the accessing PC.

**Alternatives**: `BRK` patching, which fails on ROM and alters guest-visible
bytes; per-slice checks, which miss addresses inside a slice.

## R-005: Run requests and cycle budgets

**Decision**: Run-type commands produce a `RunRequest {kind, untilPc, budget}`.
Batch mode and the emulator satisfy it by calling `RunCycles` in chunks until
the hook stops, `untilPc` is reached, or the budget is spent.

- The default budget for batch and pipe runs is 100,000,000 cycles, about 98
  seconds of emulated time at 1.023 MHz. It can be overridden per run
  (`--max-cycles`, or a `budget` field in a pipe request).
- A run started from the window has no budget (FR-008 covers scripts and
  clients only).
- Reaching the budget stops the machine with reason `budget`.

**Rationale**: FR-008 and SC-005. The default is long enough to boot DOS or
ProDOS and reach a program, and short enough that a mistaken script ends within
a couple of minutes.

## R-006: Headless machine for batch mode

**Decision**: Promote the machine-building part of `UnitTest/EmuTests/
TestMachine` into `CassoEmuCore/Shell/HeadlessMachineFactory`. It takes a
machine name, disk paths and an `IRomSource` seam. The shipped CLI uses the
same ROM resolution as the emulator; tests use `FixtureProvider`. `TestMachine`
becomes a user of the factory.

**Rationale**: `CassoCli run` runs a bare `Cpu` (`Cli/RunMode.cpp:125`) and
cannot boot a disk. Story 1 requires booting a disk headlessly.

**Alternatives**: extending `run` with `--machine`, which would mix a bare-CPU
mode with a machine mode in one options set.

## R-007: Structured output format

**Decision**: JSON Lines (one JSON object per line, UTF-8, LF) for both
`--json` batch output and the pipe, produced by the existing `JsonWriter` with
pretty printing off. The record schemas are in
[contracts/debug-channel-protocol.md](contracts/debug-channel-protocol.md) and
are shared by both.

**Rationale**: FR-024 requires the pipe reply to have the same content as
batch structured output. Using the same records makes that literally true and
testable by comparison. JSON Lines needs no length framing, can be read with
any line reader, and is what a VS Code adapter in TypeScript parses most
simply.

**Alternatives**: length-prefixed binary, which is opaque to scripts and adds
nothing at this message size; the Debug Adapter Protocol itself, which belongs
to the later VS Code adapter feature and would put the adapter inside Casso.

## R-008: Pipe security, naming and discovery

**Decision**:

- **Name**: `\\.\pipe\Casso.Debug.<pid>`, with the PID in decimal.
- **Creation**:
  - Security descriptor whose DACL grants `GENERIC_READ | GENERIC_WRITE` to the
    current token's user SID only.
  - `PIPE_REJECT_REMOTE_CLIENTS`.
  - `FILE_FLAG_FIRST_PIPE_INSTANCE` on the first instance, so no other process
    can squat the name first.
  - `PIPE_UNLIMITED_INSTANCES` for many clients.
- **Discovery**: `CassoCli debug --list` enumerates `\\.\pipe\` for the
  `Casso.Debug.` prefix, connects to each, and sends `hello`. The reply carries
  pid, title label, machine and disks. An instance that refuses the connection
  (another user) or does not answer is omitted.

**Rationale**:

- The spec clarification chose PID plus a list.
- A default pipe DACL grants read access to Everyone and the anonymous
  account, so an explicit DACL is required for FR-023.
- `FILE_FLAG_FIRST_PIPE_INSTANCE` closes the name-squatting hole, where a
  hostile process creates the pipe before Casso does.
- Story 3 scenario 4 (another user is refused) is the DACL's test, run as a
  manual quickstart step because unit tests cannot switch accounts.

## R-009: Symbol file formats and ROM symbols

**Decision**: One in-memory `SymbolTable`, filled by four importers selected
from content (FR-031):

| Format | Line form | Source |
|---|---|---|
| Casso `-g` debug file | `NAME=$ADDR`, `;` comments | `CassoCli as65 -g`, and `CassoCli merlin` after FR-033 |
| Merlin listing symbol table | `NAME =$ADDR` entries after the listing's symbol table heading | Merlin itself (a listing printed to a file through Casso's printer), and `CassoCli merlin -l` |
| AppleWin `.SYM` | `ADDR NAME` | users' own files |
| VICE label file | `al ADDR .NAME` | ld65 `-Ln`, ACME `--vicelabels` |

ROM symbol tables per machine are authored in the `-g` format from the
entry-point names published in Apple's Reference Manuals (for example
`COUT $FDED`, `GETLN $FD6A`, `MONZ $FF69`) and loaded by `SYMMAIN`. `SYMDOS33`
and `SYMPRODOS` start from the published DOS 3.3 and ProDOS entry points.

**Merlin prerequisite (FR-033)**: `CassoCli merlin` writes no symbol output
today. `MerlinMode` gains a `-g` symbol file per `SAV` output, following
spec 026's rule that each artifact splits per output, and its `-l` listing
gains a trailing symbol table in Merlin's format. The exact layout (sections,
column widths, how `]` variables and local labels appear) is fixed from a
fixture: `LABELS.S` assembled by Merlin Pro running under Casso, its listing
printed to a file through Casso's printer, checked in under
`UnitTest/Fixtures/Merlin/` beside the existing `LICENSE`. The importer's tests
and the listing writer's tests both compare against that fixture.

**Rationale**: AppleWin's `.SYM` data files ship under GPL and the clean-room
rule forbids using them; reading the file format is not copying them. VICE
labels cover the cc65 and ACME toolchains. Merlin support is required because
Casso assembles Merlin source.

**Alternatives**: Merlin 32's output files, left out because their format is
unverified; ld65 `.dbg` debug info, deferred to source-level debugging with
`SOURCE`/`SYNC`; no ROM symbols, which would make `SYMMAIN` an empty command.

## R-016: Loadable binary formats

**Decision**: A `BinaryImageReader` returns `{segments: [{address, bytes}],
format}` for FR-032's formats:

| Format | Detection | Address |
|---|---|---|
| Intel HEX | lines start with `:` and checksum | per record |
| Motorola S-record | lines start with `S0`-`S9` and checksum | per record |
| AppleSingle | magic `$00051600` | ProDOS aux type entry, else required argument |
| DOS 3.3 binary | explicit (`BLOAD file,DOS`) | 4-byte header |
| Raw | explicit or default | required argument |

These are the formats `CassoCli as65` and `merlin` already write (`--dos-bin`,
`-s`, `-s2`), plus cc65's default Apple II output. `BSAVE` writes raw bytes.

**Rationale**: Content detection for the self-describing formats; an explicit
choice where a 4-byte header is indistinguishable from data.

**Alternatives**: ca65 `.o` and o65 objects, rejected because unlinked objects
are the linker's input, not a loadable image; CiderPress `#06xxxx` filename
suffixes, left as a possible later addition.

## R-010: The 1979 listing test

**Decision**: Transcribe, from the *Apple II Reference Manual* (1979) Monitor
listing, each instruction's address, bytes and mnemonic/operand into
`UnitTest/Fixtures/Debugger/AppleII-1979-MonitorListing.txt`, with a `LICENSE`
file recording source and attribution. Data regions in the listing are marked
`DATA start-end` lines. The test:

1. Confirms the fixture ROM `Apple2.rom`'s $F800-$FFFF bytes equal the
   listing's bytes, so it verifies the right ROM.
2. Disassembles instruction by instruction from $F800, skipping only declared
   data regions.
3. Asserts address, length, mnemonic and operand for every line.
4. Asserts the instruction count equals the transcription's line count and is
   non-zero.

**Rationale**: SC-003 requires a 100% match. Declaring data regions makes the
exclusions visible and reviewable. Checking the bytes first keeps a wrong ROM
from producing a confusing mnemonic diff.

**Formatting normalization**: the listing prints operands the Monitor's way
(`#$A0`, `($3E),Y`); the disassembler's structured output is compared field by
field, not as text, so column layout does not matter.

## R-011: ROM command-table decoding (FR-027)

**Decision**: For each shipped Apple II ROM, read 23 command bytes at $FFCC
and handler offsets at $FFE3, and decode each character with the inverse of
the Monitor's input transform (the routine at $FFBE). Exclude an entry only if
it matches a declared filler rule; the //c's last entry (character `$EA`,
handler `$00`) is the one known case. Assert each remaining decoded command is
in `MonitorParser`'s command set.

**Rationale**: The spec's Monitor ROM findings were read this way. The filler
rule is declared data, not a skip, per "Degraded Operation Must Be
Observable". The test also asserts that the table was found (23 entries read)
before checking entries.

**Open until verified**: the exact encoding transform is confirmed by the test
itself decoding known entries (`L`, `G`, `M`) on every ROM before the sweep
runs.

## R-012: FR-029 verification items

**Decision**: These are tests that run before Monitor-mode work depends on
them, not assumptions:

- **$F666 mini-assembler entry**: on the Enhanced //e and //c fixture ROMs,
  disassembly at $F666 lands on the mini-assembler entry, identified by its
  prompt output (`!`) and the call into the input routine. On ROMs without a
  mini-assembler (][+, //e), `F666G` still opens Casso's line assembler, per
  the spec, and the test documents what the ROM holds there.
- **Lowercase input**: on the Enhanced //e and //c ROMs, the Monitor's input
  path upshifts lowercase. The test drives the ROM itself: boot to the `*`
  prompt in a `TestMachine`, type `300l` through `KeystrokeInjector`, and
  assert a listing appears on the text screen through `TextScreenScraper`. On
  the original ][ and ][+, where the keyboard has no lowercase, nothing is
  asserted about ROM behavior; FR-021 is satisfied by the parser for all
  machines either way.

**Rationale**: The user required verification against the fixture ROMs. If
either check contradicts the spec, the spec changes before code relies on it.

## R-013: Monitor mode semantics

**Decision**: `MonitorParser` models the Monitor's own line scan: hex
accumulation into A1/A2/A3, the `.` range, `:` store mode, `<` destination,
and space or Return continuation from the last examined address. It is
implemented from the Reference Manual's description, and each command's effect
is implemented directly (FR-018), never by jumping into ROM:

| Command | Effect |
|---|---|
| `I` / `N` | Set `INVFLG` ($32) to $3F / $FF |
| `^K` / `^P` | `n^K` sets `KSWL/H` ($38/$39) to $Cn00; `n^P` sets `CSWL/H` ($36/$37) to $Cn00 |
| `^B` / `^C` | Run at the machine's BASIC cold / warm entry ($E000 / $E003) |
| `^Y` | Run at $03F8 |
| `G` | Push a return to the session's stop address, then run at the address |
| `^E` | Show A, X, Y, P and S in Monitor format, and set the Monitor's `:` target to the register save area, so the next `: bytes` edits the registers |
| `S` / `T` | Step or trace with Monitor-format register display. Trace runs until a stop or the budget |
| `R` / `W` | Host file I/O via `IFileSystem` |
| `value<start.endS` | Search, printing each matching address in Monitor format |

The step and trace display follows the original ][ step output.

**Filename syntax**: `R` and `W` take an optional filename after a space; a
filename containing spaces is quoted. Without one, batch and pipe return an
error, and the window prompts.

## R-014: AppleWin mode semantics source

**Decision**: Command names come from `Debugger_Commands.cpp`'s table, fetched
2026-09-13. Argument forms and behavior come from AppleWin's help pages under
`help/` (`dbg-*.html`), consulted page by page during implementation. Each
handler's test cites the help page's documented example as input, and the
expected output is authored from that documentation, not from running
AppleWin's code.

**Rationale**: Clean-room rule; the help pages document observable behavior.

## R-015: Determinism

**Decision**: In batch mode:

- The emulated clock is the only clock.
- The machine runs unthrottled with audio and video presentation off.
- Timestamps never appear in output.
- Disk writes go to a copy-on-write overlay unless `--write-disks` is passed,
  so a run cannot change the next run's input.
- `KEY` input is queued by cycle.

The test runs each fixture script twice and compares output bytes.

**Rationale**: FR-009, SC-004. Persisted disk writes are the one input that
would otherwise drift between runs.
