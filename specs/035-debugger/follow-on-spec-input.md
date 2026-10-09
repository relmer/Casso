# Follow-on debugger spec: input for /speckit-specify

Items split out of 035 on 2026-09-29, after comparing with GSSquared's
debugger (`gs2_debugger_in_use.jpg` and GS2's "Using the Debugger" page).
Run `/speckit-specify` with the text below as a new feature once 035 closes.

## Feature description

1. **Video thumbnails.** A Video pane of small views of guest memory decoded
   as a video mode, independent of what the main window shows. Presets:
   text1, text2, 80text1, 80text2, gr1, gr2, hgr1, hgr2, dhgr1, dhgr2. Decode
   modes: text40, text80, lores40, lores80, hires, hires without shift, dhgr.
   Render: mono, NTSC, RGB. Each view has buttons to cycle mode and render, an
   address field and a remove button. GSSquared's `video` and `novideo`
   become real commands (today they answer "not available"); Casso commands
   in the other modes. Beyond GS2: views refresh live every frame while
   running, not only as snapshots.
2. **Beam view while stepping.** While single-stepping, the main window can
   show the partly scanned frame with a crosshair at the beam position,
   instead of a whole frame per step; a toggle chooses beam or whole frame.
   Needs the video renderer to draw a frame up to a given cycle.
3. **Per-session trace history.** Each debugging session writes its own
   trace file. Scrolling past the oldest entry in memory loads the previous
   session's file, with a divider row showing that session's date and
   machine, so history reads as one list. A setting caps how many sessions
   are kept. (GS2 writes one fixed binary file, overwritten on every quit.)
4. **Debug projects.** Moved into 035 on 2026-10-09 at the owner's request:
   a debugger project that keeps all debugger state ships with 035's first
   release. Item 3's per-session trace files live in those projects.
5. **MCP adapter.** A small program speaking MCP over stdio that forwards
   typed tools (run a command in a mode, step, set a breakpoint, read memory,
   take a screenshot) to the existing debug channel, for AI clients without
   shell access. No change inside Casso. GS2's author ships one.
   GS2's demo of its own, which ours should be able to match: an agent is
   asked to start the emulator, wait five seconds for the boot to fail,
   press Ctrl+Reset, then type in a BASIC program that draws a lo-res (GR)
   color bar demo. So the tools must include starting and stopping the
   emulator, waiting, keys and key chords (Ctrl+Reset among them), typing
   text into the machine (GS2's paste_text fills the paste buffer in one
   round trip), and reading the screen back to check the result. GS2's
   author also said his binary debug protocol was meant to let a richer
   debugger front end drive GS2, its own being limited.
6. **Code and data analysis in the code pane.** A recursive-descent pass from
   known entry points (reset, IRQ and NMI vectors, PC, breakpoints, symbols)
   follows branches, JSR and JMP, stops at RTS, RTI and indirect jumps, and
   marks what it reaches as code and the rest as data. Combined with an
   executed map: every address the CPU has fetched as an opcode is code,
   which settles indirect jumps, jump tables and pushed-address returns that
   a static pass cannot. User data directives and debug files still win.
   The same pass also serves Casso Explorer's disassembly of binary files,
   which today is linear (`Disassembler::Disassemble`): a DOS 3.3 `B` file
   carries its load address and a ProDOS `BIN` file its aux type, and BRUN
   starts at that address, so it seeds the walk. The header comment in
   `CassoCore/Disassembler.h` claiming a file has no entry point is wrong
   for those and gets corrected.

7. **Debugging other languages, one by one by popularity.** 035 ships
   Applesoft BASIC source-level debugging (owner, 2026-10-09). Next, each its
   own spec, in order of popularity: Integer BASIC with SWEET16 (same
   framework as Applesoft: listing, current line, statement stepping, line and
   variable breakpoints, variables, its own stack); Apple Pascal (UCSD p-System:
   p-code disassembly, p-machine registers, procedure names from the segment
   dictionary, breakpoints on procedures and offsets, source lines where the
   compiler kept a listing); Forth (GraFORTH, fig-FORTH: word-level stepping,
   data and return stacks); Logo; Applesoft compilers (TASC, Einstein: line
   mapping from the compiler's listing); Aztec C debug info.

## Notes for planning

- 035 FR-141 adds the agent doc for `CassoCli --attach`; the MCP adapter
  builds on it.
- Item 3 stores its trace files in 035's debug projects (item 4, now in 035).
- Out of scope: IIgs modes (SHR, `map` of IIgs banks) until a IIgs machine
  exists.
