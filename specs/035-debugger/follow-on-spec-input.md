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
4. **Debug projects.** A project keeps a program's source, debug file,
   symbols, breakpoints, watches, layout and trace history together, opened
   and saved as one. The per-session trace files of item 3 live in it.
5. **MCP adapter.** A small program speaking MCP over stdio that forwards
   typed tools (run a command in a mode, step, set a breakpoint, read memory,
   take a screenshot) to the existing debug channel, for AI clients without
   shell access. No change inside Casso. GS2's author ships one.
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

## Notes for planning

- 035 FR-141 adds the agent doc for `CassoCli --attach`; the MCP adapter
  builds on it.
- Item 4 changes where item 3 stores files; spec them together.
- Out of scope: IIgs modes (SHR, `map` of IIgs banks) until a IIgs machine
  exists.
