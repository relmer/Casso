# Feature Specification: Apple //e Fidelity

**Feature Branch**: `004-apple-iie-fidelity`
**Created**: 2026-05-05
**Status**: Draft
**Input**: User description: "Deliver a fully correct, architecturally sound emulation of the Apple //e (non-Enhanced) such that the Casso emulator can boot the //e ROM, drive all internal devices and ROMs correctly, run real //e software (including disk-based software with copy protection), and pass a comprehensive automated test suite."

**Authoritative Requirements Input**: [`docs/iie-audit.md`](../../docs/iie-audit.md) — full gap analysis and desired-correct-behavior reference for every //e subsystem touched by this feature. Every [CRITICAL] and [MAJOR] finding in that document is in scope here unless explicitly listed under "Out of Scope" below.

## Clarifications

### Session 2026-05-05

- Q: When MIXED mode is active and 80COL=1, should the bottom 4 text rows render in 80-column or 40-column? → A: 80-column (option A) — matches real //e hardware and addresses audit item M12. Captured as FR-017a; routed through the composed video subsystem (FR-020), not a branched code path.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Boot a stock Apple //e to a working BASIC prompt (Priority: P1)

A retro-computing user launches the Casso emulator, selects the "Apple //e" machine configuration, and the machine cold-boots through the //e ROM to the Applesoft BASIC `]` prompt with the cursor blinking, exactly as a real //e would. The user can type `HOME`, `PRINT "HELLO"`, and short Applesoft programs and see correct output on the 40-column text screen. The user can also issue `PR#3` and see the screen switch to 80-column text mode driven by the internal 80-column firmware.

**Why this priority**: This is the absolute baseline of "the //e works at all." Without it, no other //e capability is reachable. It exercises the soft-switch bus, ROM mapping, keyboard, video text modes, and CPU end-to-end.

**Independent Test**: Launch the emulator with a //e profile, observe the boot sequence reach the `]` prompt, type a short program, and verify the rendered output via the headless test harness scraping the text screen memory. Delivers a usable //e without requiring disks.

**Acceptance Scenarios**:

1. **Given** a fresh Apple //e machine configuration with the official `apple2e.rom`, **When** the emulator is powered on, **Then** the machine reaches the Applesoft `]` prompt within the equivalent of one real //e boot cycle and the text screen shows the standard "Apple //e" banner.
2. **Given** the //e is at the BASIC prompt, **When** the user injects the keystrokes `HOME` then Return then `PRINT "HELLO"` then Return, **Then** the text screen scrape shows `HELLO` followed by the prompt on the next line.
3. **Given** the //e is at the BASIC prompt, **When** the user injects `PR#3` followed by Return, **Then** the screen switches to 80-column mode (RD80VID reads as set, the 80STORE soft switch is engaged by firmware as required) and subsequent text output occupies all 80 columns correctly across main and auxiliary text memory.
4. **Given** the //e has booted, **When** the user presses the Open Apple, Closed Apple, and Shift modifier keys, **Then** the corresponding status reads at $C061, $C062, and $C063 reflect those key states.

---

### User Story 2 - Run real //e software from disk, including copy-protected titles (Priority: P1)

A user mounts an Apple disk image (`.dsk`, `.do`, `.po`, or `.woz`) into Drive 1 of an emulated //e, boots, and the disk loads and runs to its title screen. This includes:

- Standard DOS 3.3 disks (CATALOG works after boot).
- Standard ProDOS disks (CAT or PREFIX works after boot).
- WOZ-format disks containing nibble-accurate dumps.
- A simple copy-protected disk that relies on bit-level timing and non-standard track formatting.

When the user ejects a disk, switches machines, or exits the emulator after writing to a disk, the modified disk image is flushed back to its source file with no data loss.

**Why this priority**: The Apple //e ecosystem is overwhelmingly disk-based. A //e emulator that cannot reliably load period software — including copy-protected software — fails its primary purpose. This drives the requirement for nibble-level Disk II controller emulation rather than sector-level shortcuts.

**Independent Test**: Mount each fixture disk image (DOS 3.3, ProDOS, WOZ, copy-protected — all bundled in-repo as test fixtures), boot the //e from slot 6, and verify via text-screen scrape and/or framebuffer hash that the expected post-boot state is reached. Verify that a write-then-eject cycle produces a modified image file equal to the expected reference.

**Acceptance Scenarios**:

1. **Given** a DOS 3.3 `.dsk` fixture mounted in Drive 1, **When** the //e is cold-booted, **Then** the disk boots, DOS prints its greeting, and the `CATALOG` command produces the expected file listing.
2. **Given** a ProDOS `.po` fixture mounted in Drive 1, **When** the //e is cold-booted, **Then** ProDOS loads, the prompt appears, and `CAT` or `PREFIX` produces expected output.
3. **Given** a WOZ-format fixture mounted in Drive 1, **When** the //e is cold-booted, **Then** the disk's first track executes correctly and the title screen for the bundled WOZ test image renders to its expected framebuffer hash.
4. **Given** a public-domain copy-protected disk fixture mounted in Drive 1, **When** the //e is cold-booted, **Then** the protection check passes and the title screen loads.
5. **Given** a writable disk image is mounted and the running software has issued track writes, **When** the user ejects the disk, switches the machine configuration, or exits the emulator, **Then** the host-side disk image file is updated with the modified contents before the operation completes.

---

### User Story 3 - Programs that depend on //e-specific memory and Language Card behavior run correctly (Priority: P1)

Software that exercises the //e's auxiliary memory bank, Language Card RAM, 80-column store, and double hi-res display must behave indistinguishably from real hardware. A short Applesoft test program (or assembled test ROM) that touches aux RAM via RAMRD/RAMWRT, banks the Language Card via the two-odd-read pre-write sequence, toggles 80STORE/PAGE2/HIRES in concert, and renders to double hi-res produces exactly the expected memory state and rendered framebuffer.

**Why this priority**: The //e's distinguishing features over the ][+ are precisely auxiliary memory, the //e Language Card semantics, the //e MMU's 80STORE behavior, and double hi-res. If these are wrong, the machine is "][+ in a //e costume" and most //e-targeted software breaks subtly.

**Independent Test**: Drive a deterministic test program through the headless harness that writes patterns to main RAM, aux RAM, Language Card bank 1, Language Card bank 2, and aux Language Card RAM (ALTZP=1), then reads them back through every soft-switch combination. Compare against a precomputed expected-state table. For DHR, render a known DHR test image and compare its framebuffer hash.

**Acceptance Scenarios**:

1. **Given** RAMRD=0, RAMWRT=0 and a known byte at $4000 in main RAM, **When** RAMWRT is enabled and a different byte is stored at $4000, **Then** that byte goes to aux RAM, leaving main RAM at $4000 unchanged; reading $4000 with RAMRD=1 returns the aux value, and reading with RAMRD=0 returns the main value.
2. **Given** the //e is at power-on with the Language Card defaulting to `MF_BANK2 | MF_WRITERAM`, **When** software performs the two-consecutive-odd-read pre-write sequence (any two odd $C08x addresses, not necessarily the same one) followed by a write into $D000-$DFFF, **Then** that write lands in the selected LC bank; **When** an intervening write breaks the pre-write state, **Then** subsequent writes do not enable RAM-write.
3. **Given** ALTZP=1, **When** the program reads or writes $0000-$01FF, **Then** the access is routed to auxiliary zero-page/stack; **And** the Language Card window at $D000-$FFFF maps to the auxiliary Language Card bank.
4. **Given** 80STORE=1 and HIRES=1 and PAGE2=1, **When** the program writes into the hi-res page, **Then** the write is routed to auxiliary hi-res memory (not aux RAM via RAMWRT) per the //e MMU rules.
5. **Given** a soft reset is issued, **When** the machine comes back to the prompt, **Then** the contents of auxiliary RAM and Language Card RAM are preserved (not zeroed), the soft switches are reset per the documented //e MMU reset rules (most reset, some preserved), and the CPU registers/flags are in their post-reset state.

---

### User Story 4 - Headless automated validation of the entire //e (Priority: P1)

A developer or CI system runs the test suite for Casso. The suite includes a headless //e harness that, with no window/audio/host-I/O dependencies, spins up //e instances, mounts in-repo fixture disks, injects keystrokes, runs validation programs, scrapes text-screen memory, hashes framebuffers, and asserts against expected outputs. The full //e validation suite runs to completion in a reasonable time and is deterministic.

**Why this priority**: Without an automated headless harness, every //e regression would require manual GUI testing. The harness is the foundation that lets us defend correctness over time. It is also the only way to validate User Stories 1-3 in CI.

**Independent Test**: Run the unit + integration test target. The headless //e suite executes without opening a window, playing audio, or touching any host file other than in-repo test fixtures. All assertions pass. Re-running produces identical results.

**Acceptance Scenarios**:

1. **Given** the test runner is invoked on a clean checkout, **When** the //e integration suite runs, **Then** no window is created, no audio device is opened, no host file outside the repo is read or written, and all asserts pass.
2. **Given** the suite is run twice in succession, **When** results are compared, **Then** every test produces identical pass/fail status and identical scraped/hashed outputs (deterministic).
3. **Given** a regression is introduced in any in-scope //e subsystem (soft switches, aux RAM, LC, keyboard, video, Disk II, ROM mapping, IRQ, VBL, reset), **When** the suite runs, **Then** at least one targeted test fails with a diagnostic that points to the affected subsystem.
4. **Given** the //e is in MIXED+80COL mode with a known fixture program drawing graphics in the top region and printing 80-column text in the bottom 4 rows, **When** the harness scrapes both main and aux text page memory and the framebuffer, **Then** the bottom 4 rows render as 80-column interleaved text (not 40-column), the graphics region is unchanged, and the result matches the recorded golden output.

---

### User Story 5 - Existing Apple ][ and ][+ machines continue to work (Priority: P1)

Users who had configured Casso for the Apple ][ or ][+ machine experience no regression. Their existing test suite continues to pass, and their existing usage (boot, BASIC, disk II via the legacy path) is unaffected by the //e additions. The //e is layered on top via composition; the ][ and ][+ machine definitions are not branched, forked, or invasively modified.

**Why this priority**: Backwards compatibility is non-negotiable. Adding //e fidelity must not become an excuse to break or rewrite the simpler machines.

**Independent Test**: Run the existing ][ and ][+ test suites that predate this feature. They must all continue to pass without modification of their assertions.

**Acceptance Scenarios**:

1. **Given** the pre-existing ][ and ][+ test suite, **When** it is run after this feature lands, **Then** every previously-passing test still passes.
2. **Given** the ][ or ][+ machine is selected at runtime, **When** the machine boots, **Then** its behavior is unchanged from before this feature (no //e-only soft switches respond, no aux RAM exists, the legacy LC behavior is preserved).

---

### User Story 6 - The //e emulator runs at real-machine speed using a tiny share of the host CPU (Priority: P2)

A user running the Casso //e on a modern host system observes that the emulator runs at the correct effective clock rate (1.023 MHz CPU, real-time video and audio) while consuming no more than approximately 1% of one host CPU core under typical load (BASIC prompt, idle, normal BASIC programs).

**Why this priority**: The //e is a 1 MHz machine; emulating it should be cheap. Bloated emulation invites slowness, fan noise, and battery drain. P2 (not P1) only because correctness gates speed; an incorrect-but-fast //e is useless.

**Independent Test**: A performance regression test runs a representative //e workload for a fixed wall-clock interval and measures host CPU usage by the emulator process. The measurement is asserted against the budget.

**Acceptance Scenarios**:

1. **Given** the //e is idle at the BASIC prompt and the emulator is throttled to real //e speed, **When** host CPU usage is sampled over a multi-second window, **Then** average usage by the emulator process is at or below approximately 1% of one host core.
2. **Given** a representative BASIC workload, **When** measured the same way, **Then** average usage remains within the same approximate budget.

---

### Edge Cases

- **Floating-bus reads on $C011-$C01F**: The high bit carries the soft-switch flag; the low 7 bits must reflect floating-bus behavior, not be zero, and not return whatever the data bus last latched on a different instruction.
- **Two-consecutive-odd-read pre-write of the Language Card**: The reads do not need to target the same odd address; any two odd $C08x reads in succession arm the pre-write state. Any intervening write to the LC region resets it.
- **Power-on Language Card state**: Defaults to `MF_BANK2 | MF_WRITERAM` (matching AppleWin), not all-zero, not "ROM read / RAM write disabled."
- **Power-cycle vs soft reset**: Power-cycle initializes RAM to indeterminate values (not zero — the Casso harness must seed it to a deterministic-but-non-zero pattern for test repeatability). Soft reset preserves RAM (main, aux, and LC).
- **80STORE interaction with PAGE2 and HIRES**: When 80STORE=1, the routing of writes to text/hi-res pages is decided by PAGE2 and HIRES (per //e MMU rules), independent of RAMRD/RAMWRT.
- **Slot 6 ROM no longer shadowed**: A prior bug had INTCXROM-like behavior masking slot 6's ROM and breaking disk boot. INTCXROM and SLOTC3ROM must be honored exactly per audit §8 so that slot 6 is reachable for boot.
- **$C800-$CFFF expansion ROM with INTC8ROM**: The expansion ROM window must follow the documented INTC8ROM rules including the access-pattern that disables it.
- **Only $C010 clears the keyboard strobe**: $C011-$C01F are reads of status flags only; they must not have the side effect of clearing the strobe.
- **VBL ($C019, RDVBLBAR) tied to video timing**: Programs spin-wait on bit 7 for vsync; the bit must transition based on the video timing model, not be stubbed constant.
- **Disk write durability**: A power-loss/exit during an unflushed write to a mounted image must, at minimum, be addressed by an eject/switch/exit auto-flush so the user does not silently lose written data.
- **Headless harness must never touch host state**: No real audio device, no real window, no real keyboard/mouse capture, no host filesystem reads outside the in-repo fixtures, no registry, no network. All such dependencies must be injected via interfaces and replaced with mocks/in-memory implementations in tests.
- **IRQ infrastructure with no current asserter**: The CPU IRQ line and interrupt-controller abstraction must exist and dispatch via $FFFE on assert, even though no in-scope device asserts it. The mechanism must be unit-testable with a mock asserter.

## Requirements *(mandatory)*

> The functional requirements below are organized by subsystem and trace back to sections of `docs/iie-audit.md`. Every [CRITICAL] and [MAJOR] finding in that audit is covered by at least one FR (excluding items listed under "Out of Scope").

### Functional Requirements

#### Soft switches and memory bus (audit §1)

- **FR-001**: The system MUST implement the full $C000-$C0FF //e soft-switch surface, including write switches at $C000-$C00F (80STORE, RAMRD, RAMWRT, INTCXROM, ALTZP, SLOTC3ROM, 80COL, ALTCHARSET and their counterparts) and status reads at $C011-$C01F (BSRBANK2, BSRREADRAM, RDRAMRD, RDRAMWRT, RDINTCXROM, RDALTZP, RDSLOTC3ROM, RD80STORE, RDVBLBAR, RDTEXT, RDMIXED, RDPAGE2, RDHIRES, RDALTCHAR, RD80VID).
- **FR-002**: The system MUST implement the display-related soft switches at $C050-$C05F (TEXT/GRAPHICS, MIXED/FULL, PAGE1/PAGE2, LORES/HIRES) and route them through the same soft-switch subsystem.
- **FR-003**: Reads from $C011-$C01F MUST place the soft-switch flag in bit 7 and floating-bus content in bits 0-6, matching the documented //e behavior.
- **FR-004**: The soft-switch subsystem MUST be layered (composition, not branching) such that the ][ subset, the ][+ additions, the //e additions, and (future) the //c additions can be stacked on a common base without modifying the lower layers.

#### Auxiliary memory and 80STORE banking (audit §2)

- **FR-005**: When RAMRD=1, reads from $0200-$BFFF (excluding regions overridden by 80STORE) MUST return auxiliary RAM contents; when RAMWRT=1, writes to that range MUST go to auxiliary RAM.
- **FR-006**: When ALTZP=1, accesses to $0000-$01FF MUST be routed to auxiliary zero-page and stack memory; when ALTZP=0, they MUST use main memory. ALTZP MUST also select the auxiliary Language Card RAM bank for $D000-$FFFF when LC RAM-read or RAM-write is active.
- **FR-007**: When 80STORE=1, writes to the text page ($0400-$07FF) MUST be routed by PAGE2 (PAGE2=0 → main, PAGE2=1 → aux), and when 80STORE=1 with HIRES=1, writes to the hi-res page ($2000-$3FFF) MUST be routed by PAGE2 the same way — independent of RAMRD/RAMWRT.

#### Language Card (audit §3)

- **FR-008**: The Language Card state machine MUST treat the pre-write arming as "two consecutive odd $C08x reads" where the two reads may target *any* two odd addresses in the bank (not the same address twice).
- **FR-009**: Any write to the LC region MUST reset the pre-write state so a subsequent single odd read does not enable RAM-write.
- **FR-010**: When ALTZP=1, the Language Card window at $D000-$FFFF MUST be backed by the auxiliary LC RAM bank; when ALTZP=0, by the main LC RAM bank.
- **FR-011**: At power-on, the Language Card MUST default to `MF_BANK2 | MF_WRITERAM` (matching AppleWin's behavior).
- **FR-012**: A soft reset MUST preserve the contents of the Language Card RAM (both banks, both main and aux); a power-cycle MUST initialize it to an indeterminate pattern (deterministic seeded pattern in tests).

#### Keyboard (audit §4)

- **FR-013**: The keyboard MUST expose Open Apple at $C061, Closed Apple at $C062, and Shift at $C063, in addition to the standard $C000 keyboard data and $C010 strobe-clear.
- **FR-014**: Only a read or write at $C010 MUST clear the keyboard strobe; reads at $C011-$C01F MUST NOT clear the strobe (they are pure status reads).

#### Speaker (audit §5)

- **FR-015**: The speaker MUST toggle on access to $C030 with timing accurate enough that audio frequency tests match expected real-//e behavior; the existing speaker behavior MUST be confirmed by tests added under this feature where coverage is missing.

#### Video and character ROM (audit §6 incl. §6.5)

- **FR-016**: 40-column text mode MUST render correctly using the //e character ROM, including the FLASH and INVERSE attributes for the standard character set, and the ALTCHARSET-selected MouseText alternate set.
- **FR-017**: 80-column text mode MUST render correctly by interleaving main and auxiliary text memory and MUST honor ALTCHARSET. Flashing characters MUST flash only when ALTCHARSET selects the flashing set, matching //e behavior.
- **FR-017a**: When MIXED mode is active (TEXT bottom rows over GRAPHICS top rows) AND 80COL=1, the bottom 4 text rows MUST render using the 80-column text path (aux/main interleave with ALTCHARSET and flash semantics matching FR-017). When 80COL=0, the bottom 4 text rows MUST render using the 40-column path (FR-016). The mixed-mode rendering MUST reach the 80-column text renderer through the same composed video subsystem path used for full-screen 80-column text (per FR-020), not via a branched/duplicated code path.
- **FR-018**: Lo-res and standard hi-res graphics modes MUST render correctly. Hi-res rendering MUST include NTSC artifact color rendering (so that programs which rely on artifact color to produce 6-color output look correct).
- **FR-019**: Double hi-res (DHR) MUST render correctly with proper aux/main interleaving per the //e DHR memory layout.
- **FR-020**: The video subsystem MUST be composed (not branched) so that adding new modes or future machine variants does not require forking the existing modes.

#### Disk II controller (audit §7 + issue #61)

- **FR-021**: The Disk II controller MUST be emulated at the nibble level (the actual hardware abstraction): track buffers are nibble streams, the LSS (or an equivalent state machine) drives the read/write head, and timing/byte-rate is faithful enough for copy-protected media to function.
- **FR-022**: The system MUST natively load and serve WOZ-format images at the nibble level.
- **FR-023**: The system MUST load `.dsk`, `.do`, and `.po` images by passing them through a nibblization layer that converts the sector-level image into the nibble track form expected by the controller (and the inverse for writes/save-back).
- **FR-024**: Copy-protected disks that depend on nibble-level timing or non-standard track formats MUST be supportable by virtue of the nibble-level emulation, validated against at least one bundled public-domain copy-protected fixture.
- **FR-025**: Disk writes MUST be auto-flushed to the source host file when the disk is ejected, when the active machine configuration is switched, or when the emulator exits — with no silent loss of written data.

#### ROM mapping (audit §8)

- **FR-026**: The internal/peripheral ROM mapping MUST honor INTCXROM (internal $C100-$CFFF ROM vs slot ROMs) and SLOTC3ROM (when 0, the //e 80-column firmware is mapped at $C300; when 1, slot 3's card ROM is mapped) per the //e specification.
- **FR-027**: The $C800-$CFFF expansion ROM window MUST track INTC8ROM correctly, including the access pattern that disables it.
- **FR-028**: Slot 6's ROM ($C600-$C6FF) MUST be reachable when expected (no longer masked by an over-broad internal-ROM shadow), enabling disk boot.
- **FR-029**: The 16 KB `apple2e.rom` MUST be mapped into $C000-$FFFF correctly per the //e ROM image layout.

#### CPU and IRQ infrastructure (audit §9)

- **FR-030**: The CPU implementation MUST remain a correct 6502 (no 65C02 instructions in this feature) and MUST be structured behind a pluggable interface/strategy so that a 65C02 (or other) variant can be substituted with no caller-side changes.
- **FR-031**: The CPU MUST expose an IRQ line. When IRQ is asserted and the I-flag is clear, the CPU MUST dispatch through the $FFFE/$FFFF vector following standard 6502 semantics (push PC+state, set I, jump).
- **FR-032**: An interrupt-controller abstraction MUST exist that devices can use to assert/clear IRQ; the abstraction MUST be unit-testable via a mock asserter and MUST be ready for future devices (clock card, mouse card, Mockingboard) without redesign.

#### Vertical blanking (VBL / $C019 / RDVBLBAR)

- **FR-033**: $C019 (RDVBLBAR) bit 7 MUST reflect the current vertical-blanking-interval state, driven by the video timing model, so that programs spin-waiting on it for frame sync function correctly.

#### Reset semantics (audit §10)

- **FR-034**: A soft reset MUST reset the soft switches per the documented //e MMU reset rules (most reset to known states; certain switches preserved), MUST set the CPU's I-flag and adjust SP per 6502 reset semantics, and MUST preserve all RAM contents (main, aux, LC main, LC aux).
- **FR-035**: A power-cycle MUST initialize all RAM to an indeterminate (random) pattern by default, with the test harness seeding a deterministic pattern for repeatability.

#### Headless test harness

- **FR-036**: The system MUST provide a headless //e test harness that instantiates an emulator with no window, no audio device, no real keyboard/mouse capture, and no host I/O outside in-repo fixtures.
- **FR-037**: The harness MUST allow tests to inject keystrokes via the keyboard device, scrape main-text and aux-text memory for 40- and 80-column text validation, hash the rendered framebuffer for graphics validation, and mount disk images from in-repo fixture files.
- **FR-038**: The harness MUST be deterministic: identical test invocations MUST produce identical observable outputs, including framebuffer hashes and scraped memory.

#### Backwards compatibility

- **FR-039**: The existing Apple ][ and ][+ machine configurations MUST continue to function and pass their pre-existing test suite without modification of those tests' assertions.
- **FR-040**: //e features MUST be added by composition over the ][/][+ subsystems; the legacy machine code paths MUST NOT be branched, forked, or destructively modified to accommodate //e additions.

#### Architectural future-proofing

- **FR-041**: The architecture MUST keep open, with no anticipated redesign, the future addition of: a 65C02 CPU variant, an Apple //c machine, an Apple //e Enhanced machine, a ProDOS clock card (to read host datetime), a mouse card (to read host mouse position), and a Mockingboard sound card (two AY-3-8910). Specifically: the CPU is a pluggable strategy, soft switches are layered, video is composable, and the IRQ controller is in place.

#### Performance

- **FR-042**: When emulating a 1.023 MHz //e under typical load (idle BASIC prompt, normal BASIC programs), the emulator process MUST consume on average no more than approximately 1% of one host CPU core, validated by a performance regression test.

#### Test isolation (constitution: Testing Discipline)

- **FR-043**: Tests MUST NOT read or write any host state outside in-repo fixtures: no real filesystem (other than fixture reads), no registry, no environment dependencies, no network, no real audio device, no real display. All such dependencies MUST be injected through interfaces and mocked.
- **FR-044**: Disk images required by tests MUST be bundled in-repo as test fixtures (DOS 3.3, ProDOS, WOZ, copy-protected — all public-domain or original to this project).

#### Acceptance / validation programs (in-harness)

- **FR-045**: The headless harness's validation suite MUST demonstrably:
    - cold-boot a //e to the BASIC prompt;
    - execute `PR#3` and verify 80-column text mode is active and rendering correctly;
    - execute `HOME`, `PRINT`, `GR`, `HGR`, and `HGR2` and verify expected screen state;
    - run a small Applesoft program that writes to and reads from auxiliary memory and Language Card RAM, asserting on the resulting memory state;
    - perform a soft reset and verify the prompt is reachable again with the documented soft-switch state;
    - boot a DOS 3.3 disk image (`CATALOG` works);
    - boot a ProDOS disk image (`CAT` or `PREFIX` works);
    - boot a WOZ disk image (first track executes correctly);
    - boot a public-domain copy-protected disk image to its title screen.

### Key Entities

- **Machine Configuration (Apple //e)**: A composed definition selecting the 6502 CPU, //e memory bus, //e soft-switch bank (layered over ][+/][), //e keyboard, speaker, //e video subsystem with character ROM, two Disk II drives in slot 6, the `apple2e.rom`, and the IRQ controller. Coexists with the existing Apple ][ and ][+ configurations.
- **Soft-Switch Bank**: A layered registry of $C000-$C0FF read/write handlers contributed by each layer (][ base, ][+ additions, //e additions). Resolves which handler services each address based on the active machine.
- **Memory Bus (with //e MMU)**: Routes CPU reads and writes through the soft-switch state (RAMRD, RAMWRT, ALTZP, 80STORE, PAGE2, HIRES, INTCXROM, SLOTC3ROM, INTC8ROM, BSRBANK2, BSRREADRAM, BSRWRITERAM) to one of: main RAM, aux RAM, main LC RAM bank 1, main LC RAM bank 2, aux LC RAM bank 1, aux LC RAM bank 2, internal ROM, slot ROM, expansion ROM.
- **Language Card State Machine**: Pre-write arming via two consecutive odd reads; write-resets-arming; bank selection; RAM-read vs ROM-read selection; preserves contents across soft reset.
- **Disk II Controller and Drive**: Nibble-level controller with LSS-equivalent sequencer, two drive slots, head position state, write-protect state, motor state. Reads/writes nibble-stream track buffers.
- **Disk Image (Nibble Track Buffer)**: In-memory nibble representation of a disk's tracks, sourced from WOZ natively or from `.dsk`/`.do`/`.po` via nibblization. Writes flow back to the source file on eject/switch/exit.
- **Video Subsystem**: Composable mode set — 40-col text, 80-col text, lo-res, hi-res (with NTSC artifact color), double hi-res — driven by a video timing model that also produces VBL state.
- **Character ROM**: The //e character ROM data including primary and ALTCHARSET (MouseText) sets.
- **Keyboard**: Surfaces $C000 data, $C010 strobe-clear, and $C061/$C062/$C063 modifier reads. Accepts injected keystrokes from the test harness.
- **Speaker**: Toggles on $C030 access and feeds the audio generator; in tests, fed to a mock audio sink.
- **CPU (6502, pluggable)**: Behind a CPU interface so a 65C02 (or other) variant can be substituted. Exposes an IRQ input line driven by the interrupt controller.
- **Interrupt Controller**: Aggregates device IRQ assertions and drives the CPU IRQ line. Empty of asserters in this feature, ready for clock/mouse/Mockingboard later.
- **ROM Mapper**: Resolves the $C000-$FFFF ROM/RAM landscape per INTCXROM, SLOTC3ROM, INTC8ROM, slot card ROMs, expansion ROM window, LC RAM/ROM, and the `apple2e.rom` image.
- **Headless Test Harness**: Owns a //e instance with mocked window/audio/keyboard/mouse/host-IO; provides keystroke injection, text-screen scrape, framebuffer hash, fixture-disk mount, and assertion utilities.
- **Test Fixtures (in-repo)**: A DOS 3.3 disk, a ProDOS disk, a WOZ disk, a public-domain copy-protected disk, and any small reference framebuffer hashes / expected memory snapshots.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of [CRITICAL] and [MAJOR] findings in `docs/iie-audit.md` are addressed (excluding the items explicitly listed under "Out of Scope" in this spec), with each addressed finding traceable to one or more functional requirements and one or more passing tests.
- **SC-002**: From a cold start, the emulated //e reaches the Applesoft BASIC `]` prompt and accepts keyboard input within the equivalent of one real //e boot cycle, verified by the headless harness.
- **SC-003**: All four disk-format scenarios in User Story 2 (DOS 3.3, ProDOS, WOZ, copy-protected) boot to their expected post-boot state in the headless harness, and a write-then-eject cycle on a writable image produces a byte-equal modified image relative to the expected reference.
- **SC-004**: The pre-existing Apple ][ and ][+ test suite continues to pass at 100% with no modifications to its assertions.
- **SC-005**: The full Casso test suite (unit + //e integration) is deterministic: two consecutive runs on the same checkout produce identical pass/fail results and identical scraped/hashed outputs.
- **SC-006**: No test in the suite reads or writes host state outside in-repo fixtures (no host filesystem outside the repo, no registry, no network, no real audio device, no real window). This is verifiable by static review and by running the suite in a sandboxed environment with those resources unavailable.
- **SC-007**: Under typical //e workload (idle BASIC prompt, light BASIC program), the emulator process consumes on average ≤ ~1% of one host CPU core, measured by a performance regression test over a multi-second sampling window.
- **SC-008**: The CPU, soft-switch, video, and interrupt-controller layers are structured such that adding any one of (a) 65C02 variant, (b) //c machine, (c) //e Enhanced machine, (d) ProDOS clock card, (e) mouse card, (f) Mockingboard, requires only additive changes — no modification of existing layer interfaces is needed. Verified by architectural review at feature completion.
- **SC-009**: The full set of validation programs listed in FR-045 runs successfully in the headless harness as part of the standard test target.

## Out of Scope *(explicitly deferred)*

- 65C02 CPU instruction set additions (BRA, STZ, PHX/PHY/PLX/PLY, TRB, TSB, BIT #imm, etc.). The CPU stays 6502; only the architectural seam for a future swap is required.
- Apple //e **Enhanced** machine configuration.
- Apple //c machine configuration.
- ProDOS clock card peripheral (deferred to a post-//e spec; the IRQ infrastructure required for its eventual integration **is** in scope here).
- Mouse card peripheral (deferred; same rationale).
- Mockingboard or any non-built-in sound card (deferred; same rationale).
- Network cards (Uthernet, etc.).
- Any feature that requires a //e subsystem under test to depend on host filesystem, registry, network, real audio device, or real display.

## Assumptions

- The official `apple2e.rom` (16 KB, $C000-$FFFF layout) and the //e character ROM (including ALTCHARSET) are available to the emulator at runtime through the existing ROM-loading mechanism. Tests that need ROM behavior either use a bundled, redistributable ROM fixture or stub the relevant ROM bytes deterministically.
- Bundled disk fixtures will be either public-domain or original to this project. The "simple copy-protected disk" fixture will be a public-domain image suitable for redistribution.
- "Approximately 1% of one host CPU core" is interpreted as the average over a multi-second window on a representative modern host (single recent x64/ARM64 core in the multi-GHz class), not a strict per-instant cap. The performance test will define the exact sampling protocol.
- The "two consecutive odd reads" Language Card pre-write semantics align with AppleWin's implementation, which the audit cites as the reference. Where AppleWin and Sather disagree on an obscure detail, AppleWin's observed real-hardware behavior wins.
- Power-on RAM "indeterminate" pattern is implemented as a deterministic seeded pseudo-random pattern in tests and as a true (or seeded) random pattern in production, configurable by the harness for repeatability.
- The headless harness exposes the same machine API used by the GUI shell, so tests exercise the same code paths the GUI uses (no test-only emulator core).
- This spec adheres to the Casso constitution (Code Quality, Testing Discipline including the NON-NEGOTIABLE test isolation rule, UX Consistency where applicable). Conflicts are resolved in favor of the constitution.

## References

- `docs/iie-audit.md` — authoritative gap analysis and desired-correct-behavior reference (cited per subsystem in the FRs above).
- AppleWin — https://github.com/AppleWin/AppleWin — primary implementation reference for behaviors observed on real //e hardware.
- Jim Sather, *Understanding the Apple IIe* — primary documentary reference for //e MMU, soft switches, video, and Disk II behavior.
- Existing Casso specs: `specs/001-assembler`, `specs/002-as65-assembler-compat`, `specs/003-apple2-platform-emulator` — for project conventions and prior context.
- `.specify/memory/constitution.md` — project-wide standards (Code Quality, Testing Discipline, UX Consistency).
