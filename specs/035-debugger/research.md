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
| Merlin listing symbol table | `NAME =$ADDR` entries after the listing's symbol table heading | Merlin itself, and `CassoCli merlin -l` |
| AppleWin `.SYM` | `ADDR NAME` | users' own files |
| VICE label file | `al ADDR .NAME` | ld65 `-Ln`, ACME `--vicelabels` |

ROM symbol tables per machine are authored in the `-g` format from the
entry-point names published in Apple's Reference Manuals (for example
`COUT $FDED`, `GETLN $FD6A`, `MONZ $FF69`) and loaded by `SYMMAIN`. `SYMDOS33`
and `SYMPRODOS` start from the published DOS 3.3 and ProDOS entry points.

**Merlin prerequisite (FR-033)**: `CassoCli merlin` writes no symbol output
today. `MerlinMode` gains a `-g` symbol file per `SAV` output, following
spec 026's rule that each artifact splits per output, and its `-l` listing
gains a trailing symbol table in Merlin's format.

**The layout, read off Merlin Pro 2.23 assembling `MAKE DUMP.S` under Casso.**
Two sections in this order, each introduced by its own heading and each listing
four entries per row:

```
Symbol table - alphabetical order:

   BYTESPER=$10     MD CALL    =$8000      CALLER  =$0313      CALLMAIN=$0A92
M  LP      =$09F9      MAIN    =$0900      MAINPROG=$0900      MAINWRT =$C004

Symbol table - numerical order:

   SOURCE  =$0A        HIMEM   =$0C        ENDSRC  =$0E        BYTESPER=$10
```

An entry is a two-character flag field, then the name padded to eight columns,
then `=$` and the value: two hex digits for a zero-page address, four otherwise.
`MD` flags a macro definition, and every one carries `$8000`, the default origin
rather than an address, so the value means nothing for those entries.

Three things this capture settles and one it does not:

- **Macros are listed.** All nine `MAC` definitions in `MAKE DUMP.S` appear,
  each flagged `MD`.
- **`]` variables are not listed.** The source defines `]1`, `]1END` and `]2`
  and none appears in either section.
- **Local labels are not listed.** The source defines twelve (`:BIGLOOP`,
  `:EXIT`, `:FINISH`, `:GK`, `:LOOP`, `:MAKNIB`, `:NI`, `:NXT`, `:OK`, `:ON`,
  `:PCHR`, `:ZT`) and none appears. This is a comparison against a known source,
  not an argument from an empty table.
- **The `M ` flag marks a label generated inside a macro expansion.** Settled by
  assembling the same source with Casso: Merlin's `M  ND =$09C4`, `M  LP =$09F9`
  and `M  NI =$0A4F` carry the addresses Casso gives its own generated labels
  `ND0025`, `LP0028` and `NI0030`. Three addresses agreeing is what identifies
  the flag; the resemblance to local-label names alone would not have.

**Merlin's table holds the source's top-level symbols and no local label.**
Casso's did list them at first -- locals qualified by their owner
(`MAIN.BIGLOOP`, `SENDMSG.NXT`) and macro-generated labels under per-invocation
names (`LP0022`) -- which put about fifteen entries in `MAKE DUMP.S`'s table
that Merlin never printed, and broke the column grid wherever one of those names
ran past the eight-column field. Both kinds are now recorded where the stored
name is built (`AssemblyResult::localSymbols` and `macroSymbols`) and left out
of the table.

Recorded at creation rather than recognized afterwards, because the finished
name cannot be classified: the scope separator is a legal label character in
some dialects, and an ordinary source symbol may end in digits exactly as a
per-invocation macro label does.

**What still differs, in the other direction.** Against `MAKE DUMP.S`, Casso's
table now holds nothing Merlin's does not, and Merlin's holds twelve entries
Casso's does not: its nine `MAC` definitions flagged `MD`, and the three
macro-generated labels `ND`, `LP` and `NI` flagged `M `. Listing those would
mean giving macros a value they do not have, and recovering the unnumbered base
name of a generated label through nested expansions. Neither is done.

**Four entries per row, on an 80-column screen.** Merlin runs the listing in
80 columns and a full row measures 76 characters, so the count is Merlin's own
layout rather than an artifact of a narrow capture.

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
033-casso-explorer uses it for put, get, preview and its host naming style. Casso
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

- Conventional precedence, loosest first: `||`, `&&`, `|`, `^`, `&`, `= == !=`,
  `< > <= >=`, `+ -`, `* / // %`, then all unary operators. Parentheses group.
- `/` is accepted as a synonym for `//`.
- Unary `<` and `>` take the low and high byte, and unary `*` reads one byte
  through the side-effect-free peek.
- A bare `A`, `X`, `Y`, `P`, `S` or `PC` is the register, so hex `A` is
  written `$A` or `0A`. A name made only of hex digits is a number, and any
  other name is a symbol. `$` forces hex.
- `~` complements within 16 bits. `!`, `&&` and `||` are logical, so a
  breakpoint condition can negate and combine comparisons; they and the
  comparisons produce 1 or 0. AppleWin's `!` is a bitwise complement; Casso
  gives that to `~`.

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


## R-020: What the 2026-09-17 comparison found

**Decision**: the engine stays; the work is the window, the trace, device
panels, and source-level debugging.

**Rationale**: measured against GSSquared's and AppleWin's published
documentation, the shipped engine is at parity or ahead on command language,
scripting, attach, symbols, watchpoints and conditional breakpoints (spec
Overview). The gaps are all in what a user sees: the first window's row
height, blank symbol column and one-row panes; no retained instruction trace;
no device panels; no source view.

**Alternatives considered**: shipping the engine and window as they stood and
doing the rest as a later feature. Rejected by the owner: nothing here is on a
schedule, and a first release that trails both competitors in the visible
parts would waste the launch.

## R-021: cc65's debug-info format, version 2

**Decision**: both assemblers emit cc65's format; the reader accepts it from
any assembler; the earlier `NAME=$ADDR` file is still read.

**Facts, from cc65's reader source (`src/dbginfo/dbginfo.c`) and
`src/common/lidefs.h`**:

- Records and keys: `version major=2,minor=0`; `info` with per-type counts;
  `file` with `id`, `name`, `size`, `mtime`, `mod`; `line` with `id`, `file`,
  `line`, `type`, `count`, `span` (several, joined with `+`); `span` with
  `id`, `seg`, `start`, `size`, `type`; `seg` with `id`, `name`, `start`,
  `size`, `addrsize`, `type`, `oname`, `ooffs`; `sym` with `id`, `name`,
  `addrsize`, `size`, `scope`, `def`, `ref`, `val`, `seg`, `type`, `exp`,
  `parent`; `scope` with `id`, `name`, `type`, `size`, `parent`, `module`;
  `mod` with `id`, `name`, `file`, `lib`; `csym` and `type` for C.
- `span.start` is relative to its segment; `seg.start` places the segment.
  That is the relocatable form FR-033 asks for.
- `line.type` is `LI_TYPE_ASM` (0), `LI_TYPE_EXT` (1), `LI_TYPE_MACRO` (2) or
  `LI_TYPE_MACPARAM` (3), and `line.count` is the macro nesting depth. cc65
  writes one `line` record for the macro body line (type 2, count n) and
  another for the invocation line (type 0), both listing the same spans, which
  is exactly the "map to both" behavior FR-033a asks for.
- An unknown record type, or an unknown key inside a known record, is skipped
  with a warning. So a `sha1=` key on `file` records is read by Casso and
  ignored by cc65-based tools.
- cc65's linker writes `.dbg` by default, the same extension as Casso's
  earlier symbol file. `SymbolFileReader::Detect` already recognizes formats
  from contents, so the extension collision costs nothing.
- cc65's own wiki says the format is subject to change; this feature pins
  version 2 and its reader rejects any other major version.

**What Casso emits**: `version`, `info`, `file` (with `sha1`), `line`, `span`,
`seg`, `sym`, `mod`, `scope`. No `csym` or `type`. One `seg` per output for
as65's flat model; Merlin's `SAV` outputs become one `mod` each. `sym` records
carry `val` and `seg`; local and macro-generated labels get `scope` records so
the source view can tell them from top-level symbols.

**SHA-1**: computed over the file's text with `\r\n` and `\r` normalized to
`\n`, over UTF-8 bytes. Implemented in `CassoCore/Core/Sha1` from RFC 3174
(about 120 lines) and tested against the RFC's vectors, rather than through a
system API seam, because a pure function is what the constitution's
testability rule asks for and the algorithm is small.

**Alternatives considered**: keeping `NAME=$ADDR` and adding line records to
it. Rejected: it would be a third private format when a documented one with
existing readers (Mesen, VS Code extensions) fits.

## R-022: Merlin 8/16 listings as a source view

**Decision**: a Merlin 8/16 listing loads as a debug file whose source is the
listing; Merlin 32 is GH #153.

**What is known**: R-009 captured a MAKE DUMP.S listing from Merlin Pro 2.23
under emulation, which fixed the symbol-table layout. The listing body has a
line number, an address, up to three bytes, and the source text per line,
with `>` marking macro-expansion lines.

**What is not known**: how a `PUT` or `USE` file appears. The corpus has two
sources that use them (`PI.ADD.S` with `PUT SENDMSG` and `USE PI.MACS`;
`PI.START.S` with `USE PI.MACS`) but no captured listing of either. Whether
Merlin restarts line numbers inside the included file, and whether it prints
the file name, decides how the listing maps to lines.

**Plan**: capture `PI.ADD.S`'s listing from Merlin under emulation by the
procedure in `UnitTest/MerlinCorpus/README.md` before the listing importer is
built, check it in as a fixture under the corpus license, and record the
layout here. If included lines are not marked, they are mapped to the listing
itself (the listing is the source) and no `PUT` file is opened, which is still
correct because the listing carries the text.

**Captured 2026-09-18** (partial, lines 1 to 137 of `PI.ADD.S`, by a
`COUT` breakpoint over the debug pipe): a `PUT` file's lines are printed
with `>` after the bytes, in the column the line number otherwise holds, and
their numbering restarts at 1 for the included file; the `PUT` line itself
prints as an ordinary line of the outer file. The listing carries the text of
both, so the importer maps every line to the listing itself and needs no
second file; a `>` line is a line of the listing like any other. The capture
technique and its two traps (a breakpoint left armed drops keystrokes; a
Return sent while the machine is paused overwrites the keyboard latch) are
recorded in `UnitTest/MerlinCorpus/README.md`.

## R-023: Moving a pane between windows

**Decision**: a pane's controls are moved, not recreated, when it floats or
docks back.

**Facts**: `DxuiPanel` owns children through `std::unique_ptr` in `ChildSlot`
entries and has an owned and a non-owned slot form; controls hold no Direct2D
or DirectWrite resources (the painter and text renderer own them per window),
and `DxuiHwndSource` rescales on `WM_DPICHANGED`. So detaching a control's
`unique_ptr` from one panel and appending it to a panel in another window
carries no device state across; the receiving window lays it out at its own
DPI on the next `Layout`. What `DxuiPanel` lacks is a detach that returns the
`unique_ptr`; that is added.

**Alternatives considered**: recreating a pane's widgets in the target window
and copying state. Rejected: every pane draws from a snapshot, so recreation
would work, but moving is cheaper and keeps focus, selection and scroll
positions.

## R-024: Where a ROM patch lands

**Decision**: `DebugMemoryView` gains a `Patch` for addresses whose region is
ROM, which writes the loaded image the current banking reads from.

**Facts**: ROM bytes live in three places: `RomDevice::m_data` (system ROM on
machines without a language card at that range), `LanguageCard::m_romData`
(the ROM the card shows when its RAM is not selected), and
`CxxxRomRouter::m_internal` and `m_slotRom[]` (the `$C100-$CFFF` window). Each
exposes read access; none exposes a write. Each gets a `PatchByte (offset,
value)` reachable only through the memory view, which already resolves the
region an address belongs to and so knows which image the CPU reads.
`Apple2cRomBank` re-points the router's internal image on a `$C028` flip, so a
patch is applied to the image object, not to a copy, and survives the flip.

## R-025: The instruction trace ring

**Decision**: the debugger's trace is the CPU's existing ring, extended with
the cycle count and the bus access, filled only while on.

**Facts**: `Cpu` already keeps a ring of `TraceEntry {pc, opcode, op1, op2, a,
x, y, sp, p, intr}` sized by `EnableTrace (capacity)`, gated by
`m_traceEnabled` with one predicted branch per step that exists today whether
or not the debugger is attached, and dumped by `--trace`. It lacks the cycle
count, the effective address, the direction and the data byte.

**Design**: the entry gains `cycles` (from the CPU's counter) and one access
record `{address, direction, data}` filled by the bus. While the trace is on,
every page is published to the bus's watched-page path (the mechanism
watchpoints use), and the watch sink records the instruction's last access
into the pending entry. That routes every read and write through the slow
path while tracing, which is the accepted cost; while off, the bus runs the
same code as before the debugger existed, because the watch mask is empty.
The window's trace pane reads a window of entries through a snapshot, never
the whole ring. `HISTORY SAVE` writes every retained entry through the file
system seam.

**Alternatives considered**: a separate ring in the debugger fed by the
per-instruction hook. Rejected: it duplicates the CPU's ring and the hook
cannot see bus accesses.

## R-026: Profiling and avoidable cycles

**Decision**: profiling counts in the per-instruction hook while on, using
the CPU's own cycle attribution.

**Facts**: `Cpu::StepOne` already decides, per instruction, whether an
indexed read crossed a page (`isReadOp` plus the addressing mode and the
65C02's `crossingAPageCostsACycle`) and whether a branch was taken and
crossed a page, and adds those cycles to the instruction's base count. The
decision is made but not exposed.

**Design**: the CPU records the last instruction's penalty kinds (page
crossing, branch taken, branch crossed) in a byte beside
`GetLastInstructionCycles`. While profiling is on, the hook adds the
instruction's base cycles to its opcode-and-mode bucket and its penalty
cycles to the penalty buckets, and counts cycles per address. `PROFILE LIST`
renders the buckets; `PROFILE LIST ADDR` the per-address table with symbols;
`PROFILE SAVE` writes the same rows. Off, nothing is counted and the hook is
absent, as today.

## R-027: GSSquared's output format

**Decision**: GSSquared's output is fixed by fixtures captured from
GSSquared, with its documented examples where they exist.

**Facts**: GSSquared's docs give the input grammar in full and a few output
examples (a 16-byte hex dump with ASCII, breakpoint and watch listings). They
do not show every reply. GSSquared builds from source on Windows with SDL3;
capturing its output means building it and running its documented commands
against a known program.

**Plan**: capture, from a built GSSquared, the replies for each command in
the mode's table against a small known program, and check them in as
fixtures under a license note. The formatter is written to those fixtures.
Where GSSquared prints machine-specific values, the fixture records the
values from the same program on the same machine type. This is the one
research item that needs a build of another project; if it cannot be built,
the documented examples decide the format and the rest follows their style,
recorded here as a deviation.

**As built (2026-09-18), a deviation**: GSSquared was not built. It needs SDL3
and its other dependencies, which this machine lacks, and its documentation
shows almost no replies. Its source does: `src/debugger/Monitor.cpp` prints
every console reply through a handful of format strings, and `trace.cpp` with
`line_buffer.hpp` fixes the columns of `list`. The fixtures under
`UnitTest/Fixtures/Debugger/GSSquared/` were written from those strings
(commit 4a14e98), one file per reply kind, for the program in
`Scripts/stop.txt` and the data the formatter tests build; the `LICENSE` note
there says so. None is a capture. The layouts, in brief: every address as
`00/0300`; examine `00/0300: A9`; a dump of sixteen bytes a line, each byte
followed by a space, then a space and the characters with the high bit
dropped, then a blank line; `list` as `M=8 X=8` and `0300: AD 19 C0    LDA
$C019` (bytes from column 6, mnemonic at 18, operand at 23); `Current
breakpoints:` then `[id] exec|data|io addr[.last] [r|w|rw]`; `Current memory
watches:` then `[id] addr`; `Breakpoint id=N set`, `Watch id=N set`, `Saved N
bytes to file`, `addr: NAME`, `Cleared symbol table`. GSSquared prints nothing
for a deposit and has no stop line (its window shows the stop), so Casso keeps
AppleWin's stop text in this format. Known differences from what GSSquared
would print: `list` shows 20 instructions, not 30; no line carries trailing
spaces; a watch range is one watch per address, since Casso's watches are
single addresses.
## R-028: Layout persistence

**Decision**: the layout is one JSON subtree in the global preferences,
versioned, with monitors identified the way window placement already does.

**Facts**: `GlobalUserPrefs` is a versioned JSON document loaded and saved by
`UserConfigStore` with merge and diff, and `WindowPlacementProfile` already
keys placements by a monitor-topology hash and knows how to fall back onto a
monitor's work area. The layout tree (split and tab nodes, ratios, pane ids,
floating pane rectangles and monitors, auto-hidden edges) serializes to JSON
with a version number; an unknown pane id is dropped on load, an unknown
version falls back to the default layout, and a floating pane whose monitor
is absent from the current topology opens on the primary monitor at its saved
size (FR-044).

## R-029: Dense list panes

**Decision**: `DxuiListView` gets per-instance row height, cell padding and
font family, and a measured auto-fit that does not stretch.

**Facts**: today every list uses `s_kRowHeightDip = 30`, 12 and 16 DIP cell
padding, and the theme's body face at 13 DIP; the theme defines
`kMonoFace = "Cascadia Mono"` but nothing uses it. Auto-fit exists in an
estimating form and a measured form (`SetPreciseAutoFit`). The debugger's
lists use the monospace face, a row of the font's line height plus 2 DIP,
4 DIP padding, measured auto-fit, and no stretch column.

## R-030: The memory editor

**Decision**: 033's `DxuiHexView` with editing added, not a new control.

**Rationale**: in-place editing needs a per-cell caret, a partial-value
state, focus that advances on completion, and a text column whose cells are
single characters. `DxuiListView` has whole-row selection and no caret;
`DxuiTextInput` has a caret but is one line. `DxuiHexView` on
`origin/033-casso-explorer` already has grouping by 1, 2, 4 or 8, hex and text
columns as tab stops, a host-supplied `IDxuiHexSource` that is asked only
for the rows it draws, and per-byte marks; it is read-only. Agreed with 033:
`IDxuiHexSource` gains a `WriteBytes` that defaults to refusing, and the
view gains an overwrite caret per nibble in the hex column and per byte in
the text column. The debugger's source is a `MemoryEditModel` that reads
from the snapshot, writes through the host as a poke, and keeps the
per-window undo list of `{address, written, replaced}`.

## R-031: Keyboard schemes

**Decision**: a scheme is a `DxuiKeyMap`, a generic Dxui table from key
chord to command id, chosen in preferences and swapped at run time.

**Facts**: 033's `DxuiCommandRouter` owns one key table for the seven
standard focus-following commands and nothing else; Casso Explorer keeps a private
`kKeys` chord table. No generic application key map exists, and 033 has no
objection to one provided the chord struct matches `kKeys` and a window can
swap maps. `DxuiWindow` consults its active map in `OnKeyDown` after the
standard router finds nothing; the three schemes (Visual Studio, AppleWin,
GSSquared) are data tables tested by driving the map, not the window.

## R-032: Source files, hashing and the path list

**Decision**: the source service resolves a file by relative path, then the
program's path list, then the global path list, then a dragged file; size is
the filter before hashing.

**Facts**: hashing a source file takes well under a millisecond, so the size
filter exists to avoid opening files, not to save hashing. The path lists
live in the global preferences, per program keyed by the debug file's own
hash, and globally as a most-recent-first list. A dragged file is hashed and
matched against every `file` record; a match adds its folder to both lists.
Everything reads through the `IFileSystem` seam, so tests use the in-memory
file system.

## R-033: Step over by stack pointer

**Decision**: `RunStopHook`'s step-over completes when the stack pointer
rises above its value before the call, then stops at the next instruction
boundary.

**Facts**: today it records the `JSR`'s return address and stops when the
program counter reaches it with the stack pointer restored. That fails for a
call followed by inline parameters (ProDOS MLI) because the routine returns
past them. The stack-pointer rule needs no return address at all: it watches
`SP` on each instruction and completes once `SP > startSp`. Recursion still
works, because a deeper call lowers `SP` further. Source-level step over
repeats the rule until the program counter's line changes; step out stops at
the first instruction after `SP > startSp` without a preceding `JSR` test.

**Refined during implementation: the level alone is not enough.** A routine
that reads inline parameters with `PLA`, or discards its return address, brings
the stack back to the caller's level while it is still running, so a step that
ended on the level alone stopped inside it. A call is over when the stack
pointer is back at its level before the `JSR` and the instruction just
executed was a return or a jump (`RTS`, `RTI`, any `JMP`). The
`PLA`/`PHA` inline-parameter routine then steps over to the instruction
after its parameters, and a routine that pulls its return address and jumps
away steps over to where it jumps. Step out uses the same test with the level
when the step began. Both are in `SourceStepTests.cpp`.

**Granularity follows the view, not a command name.** The session holds a
step granularity, `instruction` or `source` (`SRC ON|OFF` for batch and
the pipe); in the window it follows which of the source and disassembly panes
has focus. AppleWin's `T`/`P`/`RTS`, the Monitor's `S` and GSSquared's
Space/`o`/`r` keep their meanings and step by whichever granularity is
set, so no dialect gains a step command, and GSSquared's Space, which its
console reads as a key on an empty line rather than as text, needs no second
form.

## R-034: Device diagnostics

**Decision**: each device publishes rows through one interface; the window
renders rows generically; three visuals are separate small controls fed by
typed payloads.

**Facts**: the state exists as getters today: `Apple2eMmu` (every switch),
`Disk2Controller` (active drive, quarter track, spin-up), `Via6522` (ports,
timers, IFR, IER), `Ay8910` (registers, envelope, periods), the printer head
and carriage, `AppleKeyboard`'s latch. GSSquared's equivalent is a per-device
callback returning lines of text; AppleWin shows the soft switches only.

**Design**: `IDiagnosticsProvider::GetDiagnostics (DiagnosticsSnapshot &)`
fills groups of `{label, value, bits}` rows plus an optional visual payload
(memory map pages, disk head position, meter levels). `MachineHost` lists the
providers of the current machine. The CPU thread builds the snapshot on the
same cadence as the debugger view, and the panel widget draws rows; the
memory-map bar, head graphic and meters are three controls that take their
payload from the same snapshot. A device on a future machine gets a panel by
implementing the interface.

## R-035: The call stack

**Decision**: three mechanisms behind one pane, hybrid by default, every
frame labeled with its provenance, and every way a mechanism can be defeated
detected and shown as a break rather than hidden.

**Facts**: the 6502 has no frame pointer; a call is two bytes on the stack
page. AppleWin shows the raw stack bytes only. Mesen tracks `JSR`, `RTS`,
interrupt entry and `RTI` as they execute and shows a call-stack window;
VICE's monitor has a `bt` backtrace. Tracking as calls execute is exact for
ordinary code and fails only when the program manipulates the stack itself;
walking the stack page needs no history but is fooled by pushed data.

**The mechanisms**:

- *Recorded*: a shadow stack fed by the run hook. Push on `JSR`, `BRK` and
  interrupt dispatch; pop on `RTS` and `RTI`. Kept only while the debugger is
  attached, so the unattached path is unchanged (FR-064).
- *Walk*: scan the stack page from SP+1 to $01FF for a word W where the
  opcode at W-2 is `JSR` ($20); the return lands at W+1. With a debug file
  loaded, keep a candidate only if that `JSR` targets a routine entry. This
  is the corrected candidate test: the pushed word is the address of the
  `JSR`'s third byte, not the return address.
- *Hybrid* (default): recorded frames where they exist, the walk below the
  point where recording began.

**Breaks the recorder detects** (FR-069), each shown as a separator row with
the instruction and its address, frames below it marked unverified:

| Signal | What happened |
|---|---|
| `TXS` | the stack pointer was reloaded; everything recorded above the new level is suspect |
| a pull (`PLA`, `PLP`) into a frame's return address | a routine is reading or discarding its return address |
| a frame ending by `JMP` with SP back at level | the return was faked; the same test the step-over rule uses (R-033) |
| `RTS` popping an address other than the one the matching `JSR` pushed | the return address was rewritten in place |
| SP wrapping past $0100 | the stack overflowed into itself |
| reset | every recorded frame is void |
| tracking began after the program started | frames below the attach point are unknown to the recorder; the walk supplies them, labeled guessed |

A mismatch where the popped address is a few bytes past the pushed one is
the inline-parameter idiom (ProDOS MLI) and is reported as "returned past
inline parameters", not as a break. A routine that edits its return address
in place and then returns normally has no signal until the `RTS` itself.

**Rejected**: rebuilding the chain from the trace history. The trace runs
only while the debugger is attached, when the recorder is running too, so it
would see the same calls; a fourth label would cost more than it adds.
Symbol-checking as a separate mode, likewise: it is a refinement of the walk.

## R-036: A WinDbg-flavored mode

**Decision**: a fourth command mode, scoped to the commands a 6502 session
uses, with the rest of WinDbg answered by a defined reply and the mode
documented as WinDbg-flavored.

**Facts**: WinDbg has several hundred commands in three families: plain
commands (`g`, `bp`, `db`) that act on the target, `.` meta-commands that
act on the debugger itself (`.reload`, `.formats`), and `!` extension
commands that live in extension DLLs and are qualified by DLL name
(`!ext.analyze`). Most of the set assumes processes, threads, modules, C
types and an OS a 6502 lacks. WinDbg's default number base is hex, `0x` is
accepted, and source lines are written `` `file:line` ``.

**In**: `t`, `p`, `g`, `gu`, `pa`, `ta`; `bp`, `bl`, `bc`, `bd`, `be`, `ba
r1|w1|e1`; `db`, `dw`, `dd`, `da` with `l<count>`; `eb`, `ew`, `ea`, `f`,
`s`, `m`; `r`, `u`, `x`, `k`, `?`; `l+s`, `lsa`, `file:line` with or
without backquotes; `.formats`. Casso's engine commands are reached as `!`
commands (`!switches`), which a WinDbg user reads as "extension commands
this debugger adds", which is what they are.

**Out**, with the reply "has no meaning on this machine": threads and
processes (`~`, `|`, `.process`, `.attach`), modules and symbol paths (`lm`,
`.reload`, `.sympath`), exceptions and events (`sx*`, `!analyze`), kernel
(`!pte`, `!pool`), dumps (`.dump`), types and locals (`dt`, `dv`, `dx`,
`??`, `dq`, `dp`), extension loading and scripting (`.load`, `.foreach`,
`.if`, aliases, pseudo-registers). Deferred with a reason: `wt`, which maps
onto the trace and the profile once they exist; `s -a`/`s -b`, which are
syntax only over the search that exists.

**Order**: after GSSquared's mode, since `k` needs the call stack (R-035).

## R-037: Engine-command markers per mode, and GSSquared's source

**Decision**: each mode reaches Casso's engine commands through the marker
native to it; a mode with none uses bare names.

**Facts, per mode**:

- *Monitor*: `/`, as today. Ctrl-Y is the Monitor's own user-vector command
  and only means anything on the machine; `!` is the mini-assembler on the
  enhanced //e and //c, so it cannot be the marker here.
- *AppleWin*: no marker exists; its table is flat. Bare names, which collide
  with nothing in its table.
- *GSSquared*: no marker exists. Its parser (`src/debugger/Monitor.cpp`,
  read 2026-09-18) classifies each space-separated token by form: hex is a
  number, `addr:` deposits, `a.b` is a range, `bank/addr` is a 24-bit
  address, a quoted string is a string, and anything else is looked up in a
  flat command table (`set`, `load`, `save`, `move`, `verify`, `watch`,
  `nowatch`, `help`, `bp`, `bpd`, `bpi`, `nobp`, `list`/`l`, `map`, `debug`,
  `nodebug`, `sload`, `sclear`, `slookup`, `m`, `x`, `video`, `novideo`).
  Stepping is not typed at all: its window steps on Space and F10, steps
  over on O, out on R, resumes on Return, toggles trace on T and a
  breakpoint on B. So `/` is taken (bank separator) and `!` is free, but
  bare names collide with nothing, so bare names it is. Typed `o` and `r`
  stay as Casso's additions so a script can step in this mode. Its
  tokenizer splits any token ending in `l` into `l` plus an address, which
  Casso does not copy. No code from GSSquared is copied; the spec's
  clean-room assumption is amended to say its source was read for its
  command table and input behavior.
- *WinDbg*: `!`, WinDbg's own extension-command prefix.

The engine-command table exists once; each parser strips its marker and
hands the rest to it, so a command added to the table is reachable in every
mode without touching a parser.

## R-038: The step filter

**Decision**: a session-level list of routines, by symbol, address or range,
that a step into treats as a step over; applied in the hook, so it works in
every mode, every key scheme and both step granularities.

**Facts**: WinDbg's `.step_filter` does this by symbol pattern. On the Apple
II the routines worth skipping are the ROM's (`COUT`, `RDKEY`, `PRBYTE`,
the disk and MLI entries), which every program calls and nobody wants to
step into. The hook already sees each `JSR` before it executes and already
knows how to run a call to completion by the stack-pointer rule (R-033), so
the filter is one lookup at the step-into decision.

**Command**: `SKIP name|addr|first.last` adds, `SKIP` lists, `SKIP - name`
removes, `SKIP CLEAR` empties. Reachable in every mode through its marker.
The list is session state, not a preference, in this feature.

## R-039: 6502 assembly color schemes

T420 asks which color scheme the disassembly, source, trace and call stack
should follow. These are the schemes worth considering, from what each one
publishes for its token kinds (from the extensions' grammars and the tools' defaults as recalled, not
re-measured; the owner should look at any one before adopting it):

| Scheme | Where it comes from | Mnemonic | Immediate | Address / label | Comment | Notes |
|---|---|---|---|---|---|---|
| Visual Studio / VS Code Dark+ and Light+ | the editor's own theme, applied to any 6502 grammar | keyword blue (#569CD6 / #0000FF) | number green (#B5CEA8 / #098658) | variable or function (#9CDCFE, #DCDCAA / #001080, #795E26) | green | What Casso follows. Every 6502 grammar below maps its scopes onto it, so it is the scheme a VS Code user already sees. |
| ca65 and generic 6502 grammars on the VS Code marketplace | TextMate scopes only, no colors of their own | `keyword.other.opcode` | `constant.numeric` | `entity.name.function` / `variable` | `comment.line` | Separate immediates from addresses only by the `#`; colors come from the active theme. |
| Kick Assembler extensions | own grammar, theme colors | keyword | numeric, with `#` in operator color | label as function | green | Gives illegal opcodes and pseudo-ops scopes of their own, which a theme can color apart. |
| Retro Assembler, 64tass, ACME extensions | TextMate scopes | keyword | numeric | label | comment | As above; nothing to adopt beyond the scope split. |
| C64 Debugger / VICE monitor | fixed palette on black | white or light gray | cyan | yellow addresses | gray | High contrast on black, but not theme-aware. |
| AppleWin debugger | fixed palette (configurable in its color scheme) | white mnemonic on blue | yellow immediate | cyan address, green symbol | -- | The Apple II reference most users know; its blue page is unlike any Casso theme. |
| Mesen debugger | per-token colors in its settings | blue | green-ish | address and label colors of their own | green | Mesen's split -- opcode, immediate, address, label, comment -- matches Casso's tokens one for one. |

Every scheme splits the same five kinds: mnemonic, immediate, address,
label/symbol, comment. The ones that differ only pick different hues, and
the fixed-palette debuggers (AppleWin, VICE) assume a page color no Casso
theme has.

Recommendation: keep the Visual Studio / VS Code family, now with the address
operand in the editor's function color so it is apart from the immediate
(T412), and every color held to AA against the PC, branch-destination and
navigated row fills (T415). It is the only scheme with light and dark variants
designed together, it matches what users of the VS Code 6502 extensions
already see, and it needs no new theme roles. If the owner wants a second
choice, AppleWin's palette is the one worth offering as an option, because
it is the Apple II debugger users know; it would need its own page color
rather than the theme's. The owner chooses.
## R-040: Reverse execution (step back, step back over, step back out)
**Status**: design options for the owner; nothing is built.
**The problem**: a 6502 instruction is not invertible from the CPU state
alone. `LDA #$00` destroys the old A, `STA` destroys the old byte, and a
soft-switch or I/O access (`$C0xx`) changes device state the CPU never sees.
Going backward therefore needs either recorded undo data or a way to replay
forward from an earlier point. Every debugger that offers it (rr, WinDbg Time
Travel, GDB `record`, and emulators such as Mesen and bsnes-plus) uses one
or both of these.
**Option A: per-instruction undo log.** The CPU hook records, for each
executed instruction, the prior PC, A, X, Y, S, P, cycle count, and the old
value of each byte written (at most 3 on a 6502, 7 for `BRK`/interrupt
entry). A ring buffer of 16 bytes per entry holds about a million
instructions (roughly one second of 1 MHz execution) in 16 MB. Step back pops
one entry and restores it.
- Cost: low to build; a few percent of CPU-hook time while recording.
- Limit: it restores CPU and RAM only. Device state (disk head position and
  nibble stream, the language-card and auxiliary-memory switches, the video
  mode, the speaker, the 6522/AY timers, the ACIA) does not rewind. Undoing
  a soft-switch access is possible for the MMU switches, which are plain
  flags, but not for the disk or audio. Stepping back across a disk read
  leaves the drive where it was.
- Only reachable range: as far back as the ring holds.
**Option B: snapshots plus deterministic replay.** Take a full machine
snapshot (CPU, all RAM banks, every device) every N frames, and record the
nondeterministic inputs between them (keystrokes, paddle and joystick
values, mouse, host time if any device reads it). Step back to instruction K
restores the nearest earlier snapshot and runs forward K-1 instructions
with breakpoints and UI updates suppressed.
- Cost: high. Casso has no machine save state today (only
  `DiagnosticsSnapshot`, which is read-only diagnostics). Every device needs
  serialize and restore, and the emulation must be proven deterministic
  given the same inputs, which needs a test that replays a recorded session
  and compares RAM and cycle count. Audio and disk timing are the likely
  nondeterminism sources.
- Gain: exact rewind of the whole machine, unlimited depth (bounded by
  snapshot memory, about 200 KB each on a 128 KB //e plus disk-track
  state), and it also delivers save states and a "rewind" feature for
  ordinary use.
**Option C: A for short range, B for long range.** Use the undo log for the
last million instructions (instant step back) and fall back to snapshot
replay beyond it. This is what Mesen-class debuggers effectively offer.
**Commands on either option.** All three reduce to "run backward until a
predicate holds", evaluated over the undo log or the replay:
- Step back: undo one instruction.
- Step back over: undo until S and the PC return to the instruction before
  the current one at the same stack depth, treating a whole `JSR`...`RTS`
  body as one step (the stack-pointer rule of R-033 run in reverse).
- Step back out: undo until the `JSR` that entered the current routine,
  that is, until S rises above its value at routine entry; land on the
  `JSR`.
- Reverse continue: undo until a breakpoint or watchpoint fires. Write
  watchpoints are exact on the undo log, because each entry holds the
  addresses written.
The trace pane and the call stack can read the same log, so the trace
becomes a view of history instead of a separate buffer.
**UI**: three toolbar buttons and menu items beside the forward steps
(VS uses Step back and Step forward in IntelliTrace; WinDbg TTD uses
`t-`, `p-`, `g-`). Suggested keys: Shift plus the forward-step key. While
the machine is in a rewound state, panes show a "history" indicator, and
any forward execution that diverges (a user edit of memory or registers)
discards the future part of the log.
**Recommendation**: build Option A first. It is self-contained in the CPU
hook, gives step back, step back over, step back out and reverse continue
for CPU and memory, and covers nearly all debugging of program logic. State
plainly in the UI that device state is not rewound. Treat Option B as a
separate feature (machine save states), since its cost is in every device
and it pays off outside the debugger as well; once it exists, Option C
follows with little extra work.
**Open questions for the owner**: whether the device-state limit of Option A
is acceptable; the ring size (memory against reach); and whether machine
save states are wanted on their own merits, which decides whether Option B
is worth its cost.

**Owner decision 2026-10-03:** option C, rewinding the whole machine including devices. Disk writes made while reverse execution is on stay in memory and never reach the image file until they are committed later, or are dropped. See T439.


### R-040 design: option C

**Status**: designed 2026-10-01 for T439; nothing is built. Tasks T441-T462.

**Two layers, one timeline.** Every executed instruction gets a sequence
number, its *position*: the CPU's retired-instruction count since the history
began. History is a contiguous range of positions `[oldest, now]`.

- **Undo log (recent).** A ring of per-instruction records covering the newest
  positions. Step back within the ring is an O(1) pop per instruction.
- **Keyframes (older).** A whole-machine snapshot every *K* cycles (default
  one video frame, 17,030 cycles, or every Nth frame by setting). To reach a
  position older than the ring, restore the nearest earlier keyframe and
  replay forward with the recorded inputs, debug hooks muted, to the target
  position. The ring is then rebuilt from that point, so further steps back are
  instant again.
- The ring is an accelerator only. Correctness rests on keyframes plus
  deterministic replay; the ring must agree with replay, which is the main test
  (T452).

**What the undo record holds.** Prior PC, A, X, Y, S, P, the cycle counter,
and an undo list of `(target, old value)` pairs for everything the instruction
changed. Targets are not just CPU-visible bytes: a write to `$C0xx` or a slot
I/O range can change device state the CPU never reads back, so devices log
their own changes through the same journal (see below). Fixed part 16 bytes;
the variable part averages under 4 bytes for ordinary code. A 32 MB default
ring holds about 1.5 million instructions, a little over a second at 1 MHz.

#### Machine state that must rewind (survey of the tree)

| State | Where | Notes |
|---|---|---|
| CPU registers, cycle counters, interrupt lines | `Core/Cpu65C02.h`, `Core/EmuCpu.h`, `Core/MemoryBusCpu.h`, `Core/InterruptController.h` | `m_totalCycles` and the bus-cycle pointer that devices read (`GetBusCyclePtr`) |
| Main and aux RAM | `Devices/RamDevice.h` | 48/64/128 KB; bytes logged per write |
| Bank routing, soft switches | `Apple2e/Apple2eMmu.h`, `Apple2e/Apple2eSoftSwitchBank.h`, `Common/AppleSoftSwitchBank.h`, `Common/LanguageCard.h`, `Apple2c/Apple2cRomBank.h`, `Common/CxxxRomRouter.h` | Plain flags plus the LC pre-write latch; snapshot is a few dozen bytes |
| Video mode and beam position | `Devices/IVideoMode.h`, `Common/VideoTiming.h` | Beam derives from the cycle counter; the floating bus reads it, so it must be exact |
| Speaker | `Common/AppleSpeaker.h` | Toggle state only; the emitted audio is output, not state (see audio below) |
| Keyboard | `Common/AppleKeyboard.h`, `Apple2e/Apple2eKeyboard.h` | Latch, strobe, any-key-down atomic, auto-repeat timer |
| Game port, mouse, Joyport | `Common/AppleGamePort.h`, `Common/AppleMouse.h`, `Common/SiriusJoyport.h` | Paddle timers run off the cycle source; host paddle positions are inputs |
| Disk II | `Common/Disk2Controller.h`, `Common/Disk2NibbleEngine.h` | Phases, quarter track, Q6/Q7, latch, bit cursor, motor spin-up/spin-down counters, `m_lastCpuSync`, per-drive engines |
| Disk media | `Devices/Disk/DiskImage.h` (`m_trackBits`, `m_trackDirty`) | Guest writes change track bits; see disk writes below |
| Mockingboard | `Common/MockingboardCard.h`, `Devices/Via6522.h` x2, `Devices/Ay8910.h` x2, `Devices/Ssi263.h` | Timers, IFR/IER, ports, AY registers and envelope/noise generators |
| ACIA (//c) | `Devices/Acia6551.h` | Registers; host endpoint traffic is an input/output boundary |
| Printer card | `Common/PrinterCard.h` | Bytes sent to the printer are output (see open questions) |
| Mouse/IWM/other cards | per card under `Machines/Apple2/Common` | Each `MemoryDevice` with state |

Every one of these is a `MemoryDevice` or is reached from `MachineHost`'s
refs (`Shell/MachineRefs.h`). There is **no save-state support today**: the
only serialization near it is `Debugger/DiagnosticsSnapshot.h`, which formats
display rows, and `IDiskImage::Serialize`, which writes media to file formats.

#### The state interface

A new `IMachineState` interface (in `Core/IMachineState.h`) that each stateful
device implements:

- `SaveState (StateWriter &) const` and `LoadState (StateReader &)`: flat,
  versioned, little-endian blobs. Readers check a per-device tag and size, so a
  mismatch is a hard failure rather than silent corruption.
- `MachineHost::SaveState` walks CPU, bus devices, MMU, video timing and slot
  cards in a fixed order. `LoadState` restores the same order, then re-derives
  caches (bus page tables, the active video mode object) instead of storing
  them.
- Pointers are never stored. Anything held as a pointer (active drive engine,
  current read bank) is saved as an index.

This is also a machine save state, which the earlier recommendation noted pays
off outside the debugger; whether to expose it as a user feature is an open
question.

#### Device changes in the undo log

The cheapest correct scheme is a **journal hook**: a device that changes its
own state on a bus access calls `journal.Record (deviceId, fieldOffset,
oldBytes)` before the change. Two ways to cover a device:

1. Fine-grained: the device journals each field it changes (soft switches,
   LC flags, keyboard strobe, Disk II phases and latch). Cheap per access.
2. Coarse: the device's whole state is small (VIA, AY, ACIA, MMU: tens of
   bytes), so on its first touch within an instruction the journal copies its
   entire `SaveState` blob. Simplest and good enough for anything under about
   64 bytes.

**Time-driven state** (VIA timers counting down, the Disk II bit cursor
advancing, motor spin-down, AY envelopes) changes on every cycle without a bus
access. It need not be journaled: it is a pure function of the cycle count and
the state at the last access, *provided* devices advance lazily from the cycle
source rather than in host-time ticks. Disk II already syncs lazily from
`m_cpuCycleSource` (issue #67 path in `Disk2Controller.cpp`); the VIA and AY
must be checked and, where they tick in bulk from `Tick (cpuCycles)`, the bulk
tick boundaries must be part of the deterministic schedule (they are, as long as
the CPU thread calls them at cycle-determined points). Where that cannot be
guaranteed, the device journals its whole state each time a tick changes it,
which bounds cost to once per instruction.

#### Disk writes

Today the in-memory `DiskImage` already holds guest writes (`m_trackBits`,
`m_trackDirty`), and `DiskImageStore` writes them to the host file at fixed
moments: eject, machine switch, soft reset, power cycle, shutdown, and the
**motor-off auto-flush** installed in `Shell/MachineBuilder.cpp`
(`SetMotorOffFlushCallback` -> `FlushAll`). While reverse execution is on:

- A store-level **hold**: `DiskImageStore::SetFlushHold (bool)`. Under hold,
  the motor-off callback and soft-reset flushes do nothing; every write stays in
  `DiskImage`.
- Guest writes to track bits are journaled like RAM (bit index and old byte of
  the packed track buffer), so a rewind restores the media exactly, and keyframes
  store only the tracks dirtied since the previous keyframe (copy-on-write by
  track, about 6.5 KB each), not the whole disk.
- When the hold is released (reverse execution turned off, or the user chooses
  *Commit disk writes*), the image as it stands at the *current* position is
  written through the normal `FlushAll` path. *Discard disk writes* reloads the
  image from the host file instead (the existing reload path the store already
  uses for external changes).
- Eject, machine switch and shutdown under hold must ask: commit, discard, or
  cancel. A crash under hold loses the uncommitted writes; that is the price of
  the owner's rule and the UI must say so (the history indicator shows
  "N disk writes not saved").

#### Determinism for replay

Replay from a keyframe must reproduce the same instruction stream to the
cycle. Sources of nondeterminism found in the tree:

| Source | Where | Treatment |
|---|---|---|
| Keystrokes | `AppleKeyboard::PressKey`, `SetKeyDown` (atomic, set from the UI thread), `ClipboardManager` paste | Record every input with the cycle at which the CPU thread applied it; replay applies it at the same cycle |
| Key auto-repeat | `TickAutoRepeat (elapsedMicroseconds)` from `EmulatorShellCpuThread.cpp`, host time | Record the elapsed value per call, or (better) convert repeat to cycle time |
| Paddles, buttons, mouse | `MachineGamePortSink.cpp` -> `SetPaddle`/`SetButton`, `AppleMouse` | Record with cycle stamp |
| Power-on RAM pattern | `Prng` seeded from `time(nullptr)` in `MachineHost` | Keyframe captures RAM; replay never power-cycles past a keyframe. A power cycle or reset is recorded as an input event |
| Disk mount, eject, write-protect change | `DiskImageStore`, drive widgets | Recorded events; a mount under history is a keyframe boundary |
| Debugger memory and register edits | debugger panes and console | Not replayed: an edit truncates the future and starts a new keyframe |
| ACIA endpoint input | `Devices/AciaEndpoints.h` | Recorded with cycle stamp |
| Frame pacing and host timing | `CpuManager.cpp` (QueryPerformanceCounter), `FramePacing` | Must only decide *when* emulation runs, never *what* it computes. Audit that no device reads host time |

**Host-time audit (T449, 2026-10-02).** Every `steady_clock`, `system_clock`,
`time()`, `GetTickCount64` and `QueryPerformanceCounter` read in `CassoEmuCore`
was checked for a path into emulated state. Only two reach it:

| Source | Where | Handled by |
|---|---|---|
| Key auto-repeat cadence | `FrameClock::TakeKeyRepeatElapsedUs` -> `AppleKeyboard::TickAutoRepeat`, once per CPU-thread frame | NOT moved to cycle time; see below. Each repeat that fires already reports `IInputEventSink::OnHostAutoRepeat`; T450 must journal it as a cycle-stamped key press, and replay must skip `TickKeyboardAutoRepeat` and apply the journaled presses instead |
| Power-on RAM seed | `EmulatorShell.cpp`, `time (nullptr) ^ (pid << 32)` into `Prng` | Keyframe holds RAM and every `PowerCycle`-seeded state; a power cycle is a journaled input. The `Prng` itself must be in the keyframe (or the power cycle journaled with the RAM it produced), or a replayed power cycle draws a different pattern |

Every other read decides only when emulation runs or what the host draws:
`CpuManager` pacing (QPC), `FrameClock` frame pacing, drive-widget door
animation (`DiskManager::GetNowMs`, `BrowseForDisk`), printer engine and
viewport `Tick (nowMs)`, the change banner, debugger and settings tooltips, the
debug panels' uptime stamps, `WasapiAudio`, CLI and channel timeouts, and file
timestamps. Two are host inputs and are covered by the journal rather than by a
code change: `ControllerInputService::MeasureElapsedLocked` (a rate binding
moves a paddle by real time; the resulting paddle value is the journaled input)
and `DiskImageStore::GetNowMs` (external-change settling; the reload it leads to
is a journaled mount). Device timing is all cycle-driven: `Via6522`, `Ay8910`
and `Ssi263` tick from `MockingboardCard::Tick (cycles)`, the Disk II motor
spin-up and spin-down count `m_motorSpinupRemaining` / `m_motorSpindownCycles`,
`AppleMouse` and `VideoTiming` tick by cycles, and the nibble engine's weak-bit
LCG (`m_weakRngState`) is in its `SaveState`. The motor-off auto-flush is a host
side effect, covered by the flush hold above.

**Why auto-repeat stays in real time (owner decision needed).** The cadence was
counted in guest cycles before, and was moved to real time on purpose: the //e
generates repeat in its keyboard encoder, not off the 6502, and at Maximum speed
(uncapped, tens of times real) a cycle-counted repeat fired hundreds of
characters a second. The comments on `AppleKeyboard::TickAutoRepeat` and
`EmulatorShell::TickKeyboardAutoRepeat` record this. A cycle cadence cannot
match real time at Maximum without measuring the host rate, which is host time
again. Journaling each fired repeat makes replay exact and keeps the typing
behavior, so that is the recommendation; moving repeat back to cycle time would
reintroduce the Maximum-speed bug.

All inputs go through one **input journal** on the CPU thread: the thread that
drains `InputEventRing` and posted commands stamps each event with the current
cycle and position before applying it. Replay feeds the journal back at the
same positions. Applying inputs only at instruction boundaries (which the CPU
thread already does) makes "same position" sufficient.

#### Output during replay and rewind

Audio, video and printer output are produced by the forward run and are not
rewound. During a replay the speaker, Mockingboard, drive audio and printer
sinks are muted and video presents only the final frame. Stepping back shows
the screen as video RAM and soft switches stand at that position, rendered by
a full-frame redraw from state (the renderer already draws from memory each
frame). Printer output already sent is not retracted (open question).

#### Costs

- **Recording**: one record per instruction, roughly 20-40 ns on the CPU hook,
  about 3-8% of emulation time at 1x; still far above real speed. The hook is
  off unless reverse execution is enabled.
- **Ring memory**: setting, default 32 MB (about 1.5 M instructions).
- **Keyframes**: a //e keyframe is about 140 KB (128 KB RAM, 16 KB LC, device
  state) plus dirty disk tracks. One per frame for 60 seconds is 3,600 frames,
  about 500 MB, so the default is one per 10 frames with a 60-second window
  (about 50 MB). Replay to an arbitrary position then costs at most 10 frames of
  emulation, about 2 ms with hooks off.
- **Reverse continue** over the keyframe range runs each keyframe interval
  forward with the breakpoint predicate active and keeps the last hit, then
  repeats for the earlier interval if none fired. Worst case scales with the
  history length: about 60 s of emulation at full speed in under a second.

#### Commands and UI

- **Commands** (all reduce to "go back to the latest position before now where a
  predicate holds"): Step back (one instruction), Step back over (stack-depth
  rule of R-033 run backward), Step back out (land on the `JSR` that entered the
  current routine), Reverse continue (latest breakpoint or watchpoint hit
  before now). Console verbs follow the WinDbg TTD pattern: `t-`, `p-`, `g-`,
  plus `gu-` for step back out.
- **Keys**: the owner suggested Shift plus the step key, but in the VS scheme
  Shift+F11 is already Step out (`DebuggerKeySchemes.cpp`). Proposal:
  Ctrl+Shift+F11 step back, Ctrl+Shift+F10 step back over, Alt+Shift+F11 step
  back out, Shift+F5 is Stop in VS so reverse continue takes Ctrl+Shift+F5. The
  other schemes get their own mapping. Open question.
- **Toolbar**: four buttons beside the forward steps, same icons mirrored.
- **History indicator**: while the position is behind *now*, a band across the
  disassembly and register panes reads "History: 1,234 instructions behind
  live" with the position on a scrubber; unsaved disk writes appear in it. The
  trace pane becomes a view of the ring.
- **Leaving history**: forward stepping from a past position replays the
  recorded future (it does not diverge). Any edit of memory, registers or
  disks, or a new input, **truncates** the future after a confirmation the
  first time in a session.

#### Open questions for the owner

1. **Keys**: Shift+F11 is already Step out in the VS scheme. Accept the
   Ctrl+Shift / Alt+Shift proposal above, or another mapping?
2. **Default limits**: 32 MB ring and a 60-second keyframe window at one
   keyframe per 10 frames, both settable?
3. **On by default?** Recording costs a few percent; enable it only while the
   debugger window is open, always, or by a menu toggle?
4. **Disk hold on exit or eject**: always ask commit/discard, or commit by
   default with an undo note?
5. **Printer and serial output** sent before a rewind cannot be recalled. Accept
   that, or hold printer output in the same way as disk writes?
6. **User-facing save states**: the state interface makes save/load state to a
   file nearly free. Ship it as a feature, or keep it internal for now?
7. **Running forward from history**: replay the recorded future (proposed), or
   always run live and discard it?

**Owner answers 2026-10-03 (T464).** Keys follow Visual Studio's Ctrl+R chords; depth is a setting; recording always on if T461 shows full speed holds; saved by default; output already sent stays sent; save states ship as a feature.

**Disk safety rules (2026-10-03).**
1. The in-memory disk is machine state. Every guest write to a track is journaled with its old bits, and snapshots carry the changed tracks, so a step back restores the disk exactly as it was at that position. The emulated disk never disagrees with the CPU and memory.
2. The host file is only ever written with a state the machine actually reached at its current position, never a mix of two positions.
3. The motor-off auto-flush is suspended while the machine is behind live; flushes happen on eject, machine switch, exit and an explicit save, of the current position.
4. Flushes write to a temporary file and replace the image atomically, so a crash during a flush cannot leave a half-written image. A crash while writes are held loses only the writes since the last flush, as on a real drive with the power cut.
5. An eject inside history keeps the ejected image's in-memory copy, with its unsaved writes, until that point leaves history, so stepping back across an eject or insert restores the right disk with the right contents. Its file is still flushed on eject (save by default).
6. If the image file changes on the host while writes are held (another program wrote it), the flush detects it by size and timestamp and asks before overwriting.
7. Write protect toggles, mounts and ejects are journaled inputs, so replay repeats them.
8. If history passes back over a flush, the file is ahead of the machine until the next flush writes the current position. That is the owner's "save by default": the last flush always wins.

**Running forward from the past (owner question 7).** Any change made while in the past (memory, registers, disk, a mount) makes the recorded future wrong, so it is dropped at that point and the machine runs live from there; the history band says so. With no change, two choices remain. Replaying the recorded future repeats exactly what happened, keystrokes included, so the user can step back and forth over a bug as often as they like; it costs the replayer and a divergence check. Running live is simpler but diverges at the first recorded keystroke or paddle reading, so the same bug may not recur. Recommendation: replay until the end of history or the first change, then run live.

#### Prior art (2026-10-03)

Sources are documentation, release notes, forum posts and papers only; no GPL
source was read. Where a source does not say, this says "unclear".

| Tool | Mechanism | Interval and budget | Restores devices? |
|---|---|---|---|
| Mesen (NES), Mesen 2 | Rewind keeps periodic save states in memory and re-emulates forward from the nearest one ([preferences](https://www.mesen.ca/docs/configuration/preferences.html), [forum, author](https://forums.nesdev.org/viewtopic.php?t=24391&start=75)) | Documented as roughly 1 MB per minute, window set in minutes; the author describes one state per second in Mesen 2, other secondary sources say every 30 frames ([libretro](https://docs.libretro.com/library/mesen/)), so the exact interval is unclear | Yes, whole-machine states |
| Mesen 2 debugger | Step back, including by scanline and frame, and undo of ROM/RAM edits ([releases](https://github.com/SourMesen/Mesen2/releases)) | Whether instruction step back uses a per-instruction log or replay from a rewind state is not documented: unclear | Presumably, since it builds on states; unclear |
| ZEsarUX + DeZog | CPU history ring of registers and stack only ([DeZog usage](https://raw.githubusercontent.com/maziac/DeZog/main/documentation/Usage.md)) | About 40 bytes per instruction; default 10,000 instructions; 1 s at 4 MHz is about 40 MB | No: memory and hardware are not rewound; memory-based conditions, watchpoints and logpoints are not evaluated in reverse |
| rr | Forward checkpoints plus deterministic replay; reverse execution is "go to an earlier checkpoint and run forward" ([rr](https://rr-project.org/), [paper](https://www.usenix.org/system/files/conference/atc17/atc17-o_callahan.pdf)) | Checkpoint interval not fixed in the user docs: unclear | Yes, whole process |
| WinDbg TTD | Full instruction trace plus an index built after recording ([overview](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/time-travel-debugging-overview), [file size](https://learn.microsoft.com/en-us/archive/blogs/windbg/time-travel-debugging-ttd-file-size)) | 1 bit to 1 byte per instruction, 5-50 MB/s, index about twice the trace; 5x-20x slowdown ([analysis](https://whiteknightlabs.com/2025/10/14/microsoft-windbg-ttd-versus-intel-pt/)) | Process memory, yes |

**Determinism checking.** rr stores registers, PC and a branch counter at
each recorded event and declares divergence when replay reaches the event
with different values; diagnosis then narrows by memory checksums and dumps
([O'Callahan](https://robert.ocallahan.org/2016/06/how-to-track-down-divergence-bugs-in-rr.html),
[issue 3341](https://github.com/rr-debugger/rr/issues/3341)). No emulator
source above documents a runtime divergence check: unclear.

**Input recording, save RAM and disk.** Mesen's movies record input and are
debuggable ([forum](https://forums.nesdev.org/viewtopic.php?t=23004)); how
battery RAM is treated during rewind is not documented by any source found:
unclear. No prior art found addresses media writes; Casso's disk rules are
ahead of it.

**Reported pitfalls.** Rewind start lag of up to one interval while the
emulator re-runs from the state, and a request for a visible "rewinding"
indicator ([forum](https://forums.nesdev.org/viewtopic.php?t=24391&start=75));
a step-back crash fixed in MesenCE 2.2.1
([release](https://github.com/nesdev-org/MesenCE/releases/tag/2.2.1)); an
uninitialized-memory read reported in rewind
([forum](https://forums.nesdev.org/viewtopic.php?t=23004)); ZEsarUX's
register-only history silently shows wrong memory, which DeZog has to warn
about (link above); TTD traces grow without a cap.

**Comparison.** Casso's option C is the rr model (keyframes plus replay as the
source of truth) with a ZEsarUX-style ring as an accelerator, but unlike
ZEsarUX the ring journals memory and devices, so it avoids the best
documented failure. Its budget (about 50 MB for 60 s) is far above Mesen's
1 MB per minute because Mesen compresses its states and keeps fewer.

**Recommended changes.**
1. Add a divergence check in the rr pattern: store PC, registers, cycle count
   and a cheap RAM hash in each keyframe, and on every replay that reaches a
   keyframe compare them; a mismatch is a hard error in Debug and a logged
   fault plus "history unavailable" in Release. Make this the T452 test's
   oracle as well.
2. Compress keyframes (delta against the previous keyframe, then a fast
   compressor); most RAM pages do not change in 10 frames. This should let
   the default window grow well past 60 s inside the same memory.
3. Hide replay lag: keep the ring large enough to cover at least one keyframe
   interval, so a step back never waits on replay; Mesen's users noticed the
   lag.
4. Evaluate memory-based conditions, watchpoints and logpoints in reverse
   continue against the restored state, and say so in the docs, since that is
   exactly where ZEsarUX/DeZog fall short.
5. Show the history band whenever the position is not live (already planned);
   Mesen users asked for that indicator after the fact.
6. Consider step back by frame and by scanline as cheap extra commands, as in
   Mesen 2; both are keyframe-relative predicates.
