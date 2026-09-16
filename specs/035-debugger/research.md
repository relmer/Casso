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
backing store and reads or writes that store directly, never through
`MemoryBus::ReadByte`.

- **$0000-$BFFF**: the bus's page tables are the truth for banking there, and
  the view reads them rather than re-deriving main/aux/80STORE state from MMU
  flags. It reads the **shadow** tables (`MemoryBus::GetShadowReadPage`,
  `GetShadowWritePage`), which hold the MMU's real pointers, not the published
  tables the CPU uses, because a page under a watchpoint is published as null
  (R-004). The region label comes from comparing the page pointer against the
  RAM devices' buffers.
- **$C000-$C0FF**: never read. Examine prints it as unreadable (`--`) unless
  the user explicitly issues `IN`, which is a real bus read by design.
- **$C100-$CFFF**: read from the slot ROM and internal ROM images directly,
  selected by `IMmu::GetIntCxRom/GetSlotC3Rom` and the `CxxxRomRouter`'s
  INTC8ROM state. Reads through the bus here have side effects too: a `$C3xx`
  read latches INTC8ROM, a `$Cn00` read selects that slot's expansion ROM, and
  `$CFFF` deselects it.
- **$D000-$FFFF**: `LanguageCard::IsReadRam/IsWriteRam/IsBank2/ReadRom` and
  `Apple2cRomBank` select RAM bank 1 or 2 or the ROM image.

**Rationale**: `MemoryBus::ReadByte` has side effects (`AppleSpeaker::Read`
toggles the speaker, soft switches flip on read, and the `$Cxxx` router
latches on read). `Cpu::PeekByte` reads the raw array and misses banking.
`Cpu::PeekForTrace` uses the read-page table but cannot serve `$C000` and up.
Using the page tables below `$C000` removes a second copy of the banking
rules that could drift from the first. FR-006 requires "which of ROM, RAM or
language-card memory is mapped", so the view also returns a region label per
address.

**Writes**: poking RAM writes the store currently mapped for writing. Poking a
ROM region reports that the address is read-only and changes nothing. A write
that lands in I/O is a real bus write, since the user asked for it.

**Test**: for each machine and a set of banking configurations, the view's
result equals the bus's over `$0000`-`$BFFF` and `$D000`-`$FFFF`, where bus
reads have no side effects; over `$C100`-`$CFFF` it equals the ROM image the
router's state selects, compared without a bus read. A full peek sweep leaves
every soft switch, the router's latches and the speaker unchanged.

**Alternatives**: a `Peek` method on every `MemoryDevice`, which touches
dozens of device classes to serve one consumer; snapshotting the bus, which
cannot capture device-backed ROM banking.

## R-004: Per-instruction hook

**Decision**: `MachineHost` holds a `DebugHook *` (null by default). Before
each instruction, `StepOne` and the `RunCycles` loop call
`hook->ShouldStopBefore(pc)` when the pointer is non-null. The session answers
from a 64 KB bitmap of enabled address breakpoints first, then from opcode and
condition breakpoints only if any exist.

**When the hook is installed.** The session installs it whenever any enabled
stop condition exists (an address, opcode, register, memory, I/O, `BRK`,
`BRKOP` or `BRKINT` breakpoint, or a watchpoint) or a debugger run is active,
and removes it when none remain. A breakpoint set while the emulator runs
freely therefore fires without a `g`: the hit pauses `CpuManager` and emits
`stopped`, the same path a debugger run's stop takes.

**Memory watchpoints** live inside `MemoryBus` as a per-page watch mask.
`MemoryBus` has no per-access observer, and adding one would put a test on
every read and write. Instead, the bus keeps two page tables: the **shadow**
table holds whatever the MMU last set through `SetReadPage`/`SetWritePage`,
and the **published** table, which the CPU's inline read uses, holds the same
pointer except for watched pages, where it holds null. A watched page's access
therefore takes the existing slow path, where `ReadByte`/`WriteByte` test the
mask before `FindDevice`, serve the access from the shadow pointer, raise the
video-dirty flag on a changed displayed byte exactly as the fast path does,
record `{accessPc, address, value, access}`, and set a pending-stop flag that
the hook checks at the next instruction boundary. For `$C000`-`$FFFF`, which
already takes the slow path, the mask test precedes the device dispatch and
the device is called once. Pages without watchpoints keep today's fast path
unchanged; the setters gain one cold branch.

This design was chosen over unmapping pages from outside the bus because the
MMU, the language card and the machine builder all call the setters on every
banking change, which would silently re-map a watched page, and because the
fast write path alone raises the video-dirty flag, so an externally unmapped
screen page would stop repainting.

**Rationale**: A pointer test per instruction is below measurement noise
(see the `reference-measure-per-instruction-perf` method: microbenchmark
before and after). With a session attached but no stop condition, the hook is
absent, so an idle debugger costs the same as none. Stopping before the
instruction is what AppleWin and the Monitor step semantics expect.
`Cpu::PeekForTrace` reads the published table, so the trace ring shows a
watched page as unreadable; the trace formatter reads the shadow table for
those pages instead. Watchpoint stops land after the accessing
instruction completes, which is the only consistent point on a 6502; the stop
reply reports the accessing PC.

**Alternatives**: `BRK` patching, which fails on ROM and alters guest-visible
bytes; per-slice checks, which miss addresses inside a slice.

## R-005: Run requests and cycle budgets

**Decision**: Run-type commands produce a `RunRequest {kind, untilPc, count,
budget}`, and `IDebugTarget::StartRun` begins it. The run ends when the hook
stops, `untilPc` is reached, or the budget is spent, and the target delivers a
`StopEvent` to the session, which implements `IRunObserver::OnStopped`.

`MachineDebugTarget` is one class; how a run executes comes from an injected
`IRunDriver`:

- **`SynchronousRunDriver`** (batch): `StartRun` calls `RunCycles` in chunks
  with the hook installed and delivers the stop before returning.
- **`CpuManagerRunDriver`** (emulator): `StartRun` must not block the CPU
  thread, which also drives frames, audio and the command queue (a blocked
  queue could never process `pause`). It records the run, un-pauses
  `CpuManager`, and returns. The existing frame loop runs slices with the hook
  installed; when the hook stops, `RunCycles` returns a short slice, which
  `ExecuteCpuSlices` already tolerates, the driver pauses `CpuManager` and
  delivers the stop from the CPU thread. Budgets are counted across slices.
  `RequestPause` stops a run the same way with reason `pause`.

**Session states**: `FreeRunning` (the emulator is running and no debugger
run is active, which is how an emulator session starts), `Paused`, `DebugRun`
(a run the debugger started, with its budget and stop conditions) and
`Stepping`. A run command while `FreeRunning` returns `ok` and adopts the
machine into a `DebugRun`, applying any budget, so "continue" from an attached
tool works on a game that is already playing. "Already running" is an error
only while a `DebugRun` or a step is active. A stop condition that fires while
`FreeRunning` pauses the machine like any other stop.
- **Budgets**: batch runs default to 100,000,000 cycles, about 98 seconds of
  emulated time at 1.023 MHz, overridable with `--max-cycles`. `--attach` runs
  are unattended and get the same default, sent as `budget` on each run. Runs
  from the window or from any other channel client are unbounded unless the
  client sets `budget` on the request or `BUDGET <n>` for the session;
  `BUDGET 0` restores unbounded. Reaching a budget stops with reason `budget`.
- **`GG`** in the emulator sets full speed for the run and restores the
  previous `SpeedMode` when it stops.

**Rationale**: FR-008 and SC-005 exist so an unattended script cannot hang.
A person using the machine while VS Code is attached must not have it stop by
itself after 98 seconds, so the bound applies only where nobody is watching.
The batch default is long enough to boot DOS or ProDOS and reach a program.

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

**AppleSingle is a shared codec.** `AppleSingleCodec` in `CassoEmuCore/Core/`
reads and writes the container (data fork, real name, ProDOS type and aux
type) and knows nothing about memory, disks or the debugger, which is why it
is not under `Debugger/`. `BinaryImageReader` therefore lives in
`CassoEmuCore/Debugger/`, not `CassoCore`, since `CassoCore` cannot reference
`CassoEmuCore`. `BinaryImageReader` uses the codec here, and
033-cassque uses it for put, get, preview and its host naming style. Casso
itself does nothing with a `.as` file, and a container is never treated as a
disk image.

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

**Pinned ROM**: `UnitTest/Fixtures/Apple2.rom`, 12,288 bytes ($D000-$FFFF),
SHA-256 `F34E573B9DE203203AC4A8C6CAB7AB0F974FACF13C40BF6E362FBB92197199F9`.

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

**Verified 2026-09-15** against the five fixture ROMs:

- **The transform is** `character = ((entry - $89) AND $FF) XOR $B0`, giving
  the high-bit-set ASCII the Monitor works in. It is the inverse of the
  accumulator's path through $FFA7-$FFBD: `EOR #$B0`, then `ADC #$88` with
  carry set by the `CMP #$0A` that rejected the digit range. `L`, `G` and `M`
  decode correctly on all five ROMs, and every one of the 23 entries decodes
  to a character in the documented Monitor set.
- **The table is read through the machine's bus, not at a file offset.** The
  //c's ROM file is 32 KB, two 16 KB banks mapped at $C000 and flipped by
  $C028; the Monitor's table is in bank 0, and the same offset in bank 1 is
  all zeros. A test that seeks to the end of the file reads the wrong bank and
  sees 23 zero entries. Reading $FFCC and $FFE3 through a built machine gets
  the bank the machine powers on with, on every machine, with no per-ROM
  arithmetic.
- **The filler rule holds exactly as declared**: the //c's last entry is
  character `$EA` (`Q`), handler `$00`, and it is the only entry on any ROM
  with a `$00` handler.
- **The ][+ and //e carry three `^Y` entries** (`$B2`) where the original ][
  has `^Y`, `T` and `S`. The duplicates have real, distinct handlers ($C9,
  $C1, $C4), so they are not filler; the Monitor's scan walks the table from
  the top and takes the first match, which makes the later two unreachable
  through the table. They need no exclusion, because `^Y` is a Monitor command
  and the sweep passes on it. The Enhanced //e replaces the same two slots
  with `!` and `S`, and the //c carries `S`, `T` and `!` as well. This is why
  FR-016's union of every ROM's commands on every machine is a real feature
  rather than a formality: no single ROM has all of them.

## R-012: FR-029 verification items

**Decision**: These are tests that run before Monitor-mode work depends on
them, not assumptions:

- **Where the `!` mini-assembler lives**: $F666 is the mini-assembler entry
  only in the original ]['s Integer BASIC ROM. On Applesoft machines $F666 is
  inside Applesoft, and the Enhanced //e and //c mini-assembler is in the
  internal `$Cxxx` ROM, reached through `!`. The test therefore locates it
  from the command table: decode the `!` entry at $FFCC and its handler at
  $FFE3 on `Apple2eEnhanced.rom` and `Apple2c.rom`, follow the handler, and
  assert it reaches the internal firmware. It also records what each ROM holds
  at $F666, which is why `F666G` is a Casso alias for `!` rather than a jump.

  **Verified 2026-09-15.** The handler address is `$FE00 + offset + 1`: the
  dispatcher at $FFBE pushes $FE and then the offset byte and returns, so the
  `RTS` adds one. What the handlers do:

  | ROM | `!` entry | Handler | What it does |
  |---|---|---|---|
  | Apple2 | absent | | slot 3 is `T`; `F666G` is the way in |
  | Apple2Plus | absent | | slot 3 is a `^Y` duplicate |
  | Apple2e | absent | | slot 3 is a `^Y` duplicate |
  | Apple2eEnhanced | `$9A` | $FEF1 | `STA $C007` (internal $C8 ROM in), `JSR $C5D1`, `STA $C006` (out) |
  | Apple2c | `$9A` | $FE6C | `JMP $C986`, straight into the always-present internal firmware |

  So the claim holds: on both machines `!` reaches the mini-assembler in the
  internal $Cxxx firmware, and on the Enhanced //e it has to page that
  firmware in first. The assertion is that the handler reaches $Cxxx, not that
  it prints a prompt: the prompt is printed inside the internal routine, not
  in the handler the table points at.

  At $F666 the original ][ holds `4C 92 F5` (`JMP $F592`, whose target bells,
  sets the `!` prompt character in $33, and calls GETLNZ -- the mini-assembler
  proper). The ][+, //e, Enhanced //e and //c all hold `85 D3 8A 29 0F AA BC
  BA` there, which is Applesoft, not an entry point at all.
- **Lowercase input**: on the Enhanced //e and //c ROMs, the Monitor's input
  path upshifts lowercase. The test drives the ROM itself: reach the `*`
  prompt in a `TestMachine`, type `300l`, and assert a listing appears on the
  text screen through `TextScreenScraper`. On the original ][ and ][+, where
  the keyboard has no lowercase, nothing is asserted about ROM behavior;
  FR-021 is satisfied by the parser for all machines either way.

  **Verified 2026-09-15**, with two corrections to how the test reaches the
  prompt:

  - **`CALL -151` is not available, because neither machine reaches BASIC
    without a disk.** Given no disk the //e spins in its startup firmware with
    only its banner on screen, and the //c stops at "Check Disk Drive." The
    test enters at **$FF59** instead, byte-identical on all five shipped ROMs
    (`SETNORM`, `INIT`, `SETVID`, `SETKBD`, `CLD`, `BELL`, then `MONZ` at
    $FF69, whose `LDA #$AA` is the `*` prompt). Entering there sets up the
    screen and the input and output hooks exactly as a reset does.
  - **Keys go in through `MachineRefs::keyboard`, not `iieKeyboard`.**
    `KeystrokeInjector` waits on the //e-specific pointer and landed no keys
    at all in this state. Pressing the base `AppleKeyboard` and running until
    the strobe clears works, and is the path
    `MachineDebugTarget::InjectKey` already uses.

  With those, the Enhanced //e echoes `*300l` and disassembles from $0300: the
  ROM upshifts the command without upshifting the echo. FR-021 stands as
  written.

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
| `^K` / `^P` | `n^K` sets `KSWL/H` ($38/$39) to $Cn00 and `n^P` sets `CSWL/H` ($36/$37) to $Cn00 for slots 1-7; `0^K` restores `KEYIN` ($FD1B) and `0^P` restores `COUT1` ($FDF0) |
| `^B` / `^C` | Run at the machine's BASIC cold / warm entry ($E000 / $E003) |
| `^Y` | Run at $03F8 |
| `G` | Push the return address the ROM's own `G` handler pushes, set an internal breakpoint there so an `RTS` from the program stops the debugger, and run at the address. The internal breakpoint takes no id, is absent from `BPL`, and is removed when it fires or when the run stops for any other reason. Registers are not reloaded from $45-$49 |
| `^E` | Show A, X, Y, P and S in Monitor format, and arm register edit, so the next `: bytes` sets the CPU registers and writes the same bytes to $45-$49 |
| `S` / `T` | Step or trace with Monitor-format register display. Trace runs until a stop or the budget |
| `R` / `W` | Host file I/O via `IFileSystem` |
| `value<start.endS` | Search, printing each matching address in Monitor format |

The step and trace display follows the original ][ step output. The CPU's
registers are the single truth: the ROM's `G`, `S` and `T` reload them from
$45-$49, and Casso's do not, so a register set in AppleWin mode survives a
Monitor `G`. Arithmetic is 8-bit, as the Monitor's is: `FF+FF` prints `=FE`.
Examine rows align to 8-byte boundaries: `303.30F` prints `0303-` with five
bytes, then `0308-` with eight.

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

**Expressions**: `dbg-calculator.html` documents hex by default, `#` for
decimal, `+ - * % // & | ^`, unary `!`, register names and bare symbol names,
but not precedence or dereference. Casso fills the gaps as follows:

- Conventional precedence, loosest first: `|`, `^`, `&`, `= == !=`,
  `< > <= >=`, `+ -`, `* / // %`, then all unary operators. Parentheses group.
- `/` is accepted as a synonym for `//`.
- Unary `<` and `>` take the low and high byte, and unary `*` reads one byte
  through the side-effect-free peek.
- A bare `A`, `X`, `Y`, `P`, `S` or `PC` is the register, so hex `A` is
  written `$A` or `0A`. A name made only of hex digits is a number, and any
  other name is a symbol. `$` forces hex.
- `!` complements within 16 bits; comparisons produce 1 or 0.

**Argument forms the help pages do not document** were read from AppleWin's
source (the owner authorized reading it for behavior; no code was copied), and
several earlier guesses here were wrong. The forms Casso accepts:

- `ECHO text` prints a quoted string or the rest of the line verbatim.
- `PRINT item[,item]` prints comma-separated items; a quoted string prints
  as itself and an expression as four hex digits.
- `PRINTF "format"[,expr]` supports `%x`/`%X` (four hex digits), `%d`/`%D`
  (decimal), `%z`/`%Z` (eight binary digits), `%c`, `%%`, and `\n`.
- `CALC expr` prints one line holding hex, binary, decimal and the character:
  `$0041  0z01000001     65  'A'`, with `(High)`, `(Ctrl)` or `(High Ctrl)`
  where the byte has the high bit set or is a control character.
- `LOG [NONE|ERROR|WARN|INFO|DEFAULT|ALL|OFF|ON]` sets how much the console
  prints. It writes no file; `TF` is what writes a trace file.
- `M dest range` and `MC dest range` put the destination first; a range is
  `addr`, `addr,len` or `addr:last` everywhere. `F` also accepts
  `F start end value`.
- `MEB`/`ME8` writes a value above `$FF` as two bytes, low byte first;
  `MEW`/`ME16` always writes words.
- `S` and `SH` share one syntax: `"text"` (high bit clear), `'text'` (high bit
  set), a byte, a 16-bit value matched low byte first, and the wildcards `?`
  (any byte), `?n` (any high nibble), `n?` (any low nibble). Results are
  numbered and reusable as `@1`, `@2` and so on, which `@` reprints.
- Ids for `BPC`, `BPD`, `BPE`, `WC`, `ZPC`, `BMC` and the like are decimal, as
  AppleWin lists them; `*` means all.
- `BPCHANGE # <flags>` toggles a breakpoint's enabled (`E`/`e`), temporary
  (`T`/`t`) and stop (`S`/`s`) settings.
- `BRK [0|1|2|3|ALL] [ON|OFF]` selects the BRK opcode (0) or invalid opcodes
  of one, two or three bytes, and reports the setting when given no value.
- `BPIO` is `BPM` under another name in AppleWin, and stays an alias here.
- `BPA addr` sets a breakpoint on the program counter and a memory watchpoint
  at the same address, or an I/O watchpoint for `$C000-$C0FF`.
- The data directives take `[name] [addr | range]` or `name = addr`, and
  name each new block automatically (`B_0300`, `W_03F0`, `T_0800`, `A_03F2`).
  `Z` is `DB`; `B` lists the blocks; `X` removes, trims or splits one.
- `SYM<table>` takes `CLEAR`, `LOAD "file"[,offset]`, `ON`, `OFF`,
  `name = addr`, `! name` and a lookup. A file is loaded only by `LOAD`.
- `BUDGET n` takes decimal, as `--max-cycles` does.
- `addr:bytes` deposits, `addrG` sets PC and goes, `addrL` disassembles, and
  `dest<start.endM` moves, as AppleWin accepts.

## R-017: Watchpoint timing and the value a write replaced (FR-004)

**Decision**: A watchpoint stops **after** the access by default, reported by
the bus through `IWatchSink`, and a write carries the value it replaced.
`BEFORE` on a watchpoint selects a stop before the instruction whose operand
addresses fall in the range, which the session predicts by decoding the
instruction at the program counter.

**Rationale**: the question a memory watchpoint answers is "who touched this
address, and with what". Stopping after is the only way to report the value,
to tell a read-modify-write's read from its write, and to see accesses no
decode predicts: stack pushes and pulls, interrupt vector reads and DMA.
AppleWin stops before, and therefore reports no value and deliberately skips
`JSR` operands and `RTS`/`RTI`/`BRK` vectors.

Stopping before is still worth having for `$C000-$C0FF`, where the access has
a side effect that has already happened by the time an after-stop reports it:
a write to `$C030` has clicked the speaker, a read of `$C0EC` has advanced the
disk sequencer. Before-mode is the only way to prevent one.

**Previous value**: the bus already reads the target byte on the write path to
decide whether to raise the video-dirty flag, so keeping it costs one byte
copy on a watched page. Where the page is served by a device rather than
memory, no previous value is reported: reading it back would disturb the
machine, which is exactly what the debugger must not do.

**Several accesses in one instruction**: the CPU's indexed fetches read the
target byte before a store writes it, so `STA $0400,X` reaches the bus as a
read of `$0400+X` followed by a write of it. Within one instruction, a write
to an address replaces a read of that address in the pending hit, and a later
write replaces an earlier one, so the stop reports the write and its value.
This rule holds whatever the CPU's bus behavior: a faithful 6502 makes more
such accesses (dummy reads on indexed stores and page crossings, the NMOS
old-then-new read-modify-write write), not fewer. The pre-read on indexed
stores is itself an emulation fidelity gap outside this feature: real hardware
reads the un-carried address, never the final one, and the Harte runner
compares only final state and cycle counts, so it cannot see the difference.

**What before-mode predicts**: the instruction's final effective address,
classified by its operation as a read (loads, compares, `BIT`), a write
(stores) or both (read-modify-write). The pre-read on indexed stores is not
predicted, so `BPMR` in before-mode does not stop on a store.

**One instruction, one stop**: an instruction that caused a before-stop does
not cause an after-stop for the same watchpoint range when the run resumes.

## R-018: What AppleWin's unimplemented commands were meant to do

**Decision**: AppleWin ships several commands that parse and then do nothing.
Casso implements the behavior their own help text and command table describe:

- `MC` compares two ranges and lists each difference.
- `BPEDIT # <definition>` replaces breakpoint `#` with the definition that
  follows, written as it would be for `BP`, `BPR`, `BPM` and the rest, keeping
  the id and resetting the hit count. It saves clearing the old entry and
  setting a new one. AppleWin's record is the table text "Edit breakpoint"
  and a SoftICE reference, where `BPE` re-opens the original line for editing;
  the window may offer that form later.
- `WSAVE`, `ZPSAVE`, `BMSAVE` and `BPSAVE` write a replayable command script,
  the form `BMSAVE` and `BPSAVE` already use: a clear line followed by one
  command per entry.
- `SAVE "file"` writes all four scripts as one file and `LOAD "file"` runs it.
- `SYM<table> SAVE "file"` writes the table in the format `LOAD` reads.
- `ME` is a full-screen memory editor, so it stays window-only.

**Rationale**: the names are in the table AppleWin users know, and a command
that accepts a line and does nothing is the "degraded state that reads as a
healthy one" the constitution forbids. Casso either performs the documented
effect or reports that the command needs the window.

## R-019: The tracing and timing commands

**Decision**, from AppleWin's behavior:

- **Recording scope**: `LBR`, `PROFILE` and `TF` record only during a
  debugger-driven run (`G`, `GG`, `T`, `P`, `RTS`), never while the machine
  runs freely. AppleWin's per-instruction work runs only while its debugger
  steps the CPU, which is also the only mode where its breakpoints fire.
  Casso's hook is installed whenever a stop condition exists, including while
  the user runs the machine normally, so a breakpoint fires without a `G`;
  without this scope, a game running with one breakpoint set would pay for
  profile counters and trace lines it never asked for.
- `LBR` reports the address of the last instruction that transferred control
  (a taken branch, `JSR`, `JMP`, `RTS`, `RTI`, `BRK`), not its destination.
- `PROFILE [RESET|SAVE|LIST]` counts each instruction the debugger runs, by
  opcode and by addressing mode, and reports percent, count and the cycles
  since the last reset, sorted by count. `SAVE` writes a tab-separated
  `Profile.txt`. Casso does not write that file automatically on close, as
  AppleWin does: a batch run must not produce a file nobody asked for.
- `TF ["file"] [v]` toggles a trace file, writing one line per instruction
  before it executes: cycles, `A`, `X`, `Y`, `SP`, the eight flag characters
  and the disassembly. With `v`, the video scanline, horizontal position,
  scanner address and data replace the cycle count. AppleWin runs `G` by
  stepping, so a trace covers every instruction a run executes, not only
  manual steps; Casso installs the hook for the same effect.
- `TL` is an alias of `T`: AppleWin's own cycle-count flag is never read.
- `CYCLES abs|rel|part` selects the cycle counter's reading, and `RCC`
  ("reset cycles counter") sets the point `part` counts from.
- `BENCHMARK` in AppleWin loads a loop of common opcodes at `$0300`, resumes
  the machine, and reports host seconds when the speaker next clicks. That
  measures the emulator, not the guest: in a throttled emulator host seconds
  for a guest routine are its cycle count over 1.023 MHz, which `CYCLES rel`
  already reports with no clock. Casso's `BENCHMARK` is therefore a throughput
  benchmark: it loads the same kind of loop, runs a fixed number of emulated
  cycles at full speed, reports emulated cycles per host second and the
  multiple of a real Apple II's speed, and restores the throttle. It is
  available in the window and over the channel, and `notAvailable` in batch
  mode, where a host clock would break determinism (FR-009). `EXITBENCH` ends
  a benchmark early; it has done nothing in AppleWin since 2014, when its
  handler was removed to stop `E` matching it.
- `LOG [level]` in AppleWin sets console verbosity. Casso has no console
  levels, so `LOG` selects which notifications batch mode and the channel
  print: `ERROR` prints only stops, `INFO` (the default) adds `resumed`,
  `reset`, `machineChanged` and `modeChanged`, and `ALL` adds every reply's
  text. It writes no file.

## R-015: Determinism

**Decision**: In batch mode:

- The emulated clock is the only clock.
- The machine runs unthrottled with audio and video presentation off.
- Timestamps never appear in output.
- Disk writes go to a copy-on-write overlay unless `--write-disks` is passed,
  so a run cannot change the next run's input.
- `KEY` input is queued by cycle.
- The DRAM power-on pattern comes from the shared `Prng` that `PowerCycleAll`
  threads through every device. The emulator seeds it from host time; batch
  mode pins it (`--seed <n>`, default `0xCA550001`, the value the test
  harness already uses) and prints the seed in verbose output.

The test runs each fixture script twice and compares output bytes.

**Rationale**: FR-009, SC-004. Persisted disk writes and the DRAM seed are the
two inputs that would otherwise drift between runs.
