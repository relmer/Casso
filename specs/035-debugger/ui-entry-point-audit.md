# Debugger console command vs UI entry point audit

Branch 035-debugger, read-only audit. `DebugVerb` in `CassoCore/Debugger/DebugCommand.h` has 165 enumerators including `None`; this table covers the 164 real verbs (`None` is the "not available" placeholder for SHR, SOURCE, SYNC, NTSC).

Console words come from `CassoCore/Debugger/AppleWinCommandTable.cpp` (AppleWin names first, aliases included), then the other dialects (`MonitorParser.cpp`, `GSSquaredParser.cpp`, `WinDbgParser.cpp`) and the Casso sub-forms parsed in `AppleWinParser.cpp`.

## Legend

- **covered**: a UI control runs this verb directly (menu, toolbar, context menu, dialog, in-place edit, key, drag and drop).
- **shown by pane**: a pane displays what the "show" command prints.
- **equivalent**: the UI performs the same operation through a different verb, or the feature lives in the main emulator window.
- **console-only by nature**: only makes sense in a console (reason given). Listed so the owner can decide.
- **MISSING**: no UI entry point found. **MISSING (partial)**: some UI exists but does not cover the verb.

Path abbreviations (all under `CassoEmuCore/` unless noted; line numbers are for the working tree at audit time):

| Abbrev | File |
|---|---|
| DW | `Ui/Debugger/DebuggerWindow.cpp` |
| MB | `Ui/Debugger/DebuggerWindowMenuBar.cpp` (menu bar, console bar, disassembly toolbars) |
| BPW | `Ui/Debugger/DebuggerWindowBreakpoints.cpp` |
| BBC | `Ui/Debugger/BreakpointBarCommands.cpp` |
| DA | `Ui/Debugger/DebuggerActions.cpp` |
| DC | `Ui/Debugger/DebuggerCommands.cpp` (command bar rows) |
| DVS | `Ui/Debugger/DebuggerViewState.cpp` |
| EC | `Ui/Chrome/EmulatorCommands.cpp` (main window menus) |
Other files are named in full or relative to `Ui/Debugger/`.

## 1. All 164 verbs


### Execution

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `Go` | G; Monitor nnnG; GSSquared g; WinDbg g, pa, ta | covered | Toolbar Run (DC:42); Debug > Run (MB:302); key via DW:2804; built at DA:51-53. Run to cursor is Go with an address (DA:148-150) |
| `GoFullSpeed` | GG | MISSING | none: no full-speed run in the toolbar, menus or DebuggerActions |
| `StepInto` | T TL TRACE; Monitor S; GSSquared s; WinDbg t | covered | Toolbar Step into (DC:44); Debug > Step into (MB:306); DA:57-59 |
| `StepOver` | P; GSSquared o; WinDbg p | covered | Toolbar Step over (DC:45); Debug > Step over (MB:307); DA:77-79 |
| `StepOut` | RTS; GSSquared r (no args); WinDbg gu | covered | Toolbar Step out (DC:46); Debug > Step out (MB:308); DA:97-99 |
| `Trace` | Monitor T | console-only by nature | Monitor T runs on, reporting each instruction to the console; the window's equivalent is Run plus the Trace pane. Dialect form |
| `TraceToFile` | TF | MISSING | none. NOT the same as Debug > Save CPU trace in the main window (EC:66), which dumps the --trace ring (EmulatorShellDebug.cpp:369), nor File > Save trace... (MB:240), which saves HISTORY |
| `SetProgramCounter` | = | equivalent | Code pane context menu Set next statement (DW:6261, DW:10503-10505, runs R PC); Registers pane PC edit (DW:8393, 8633) |
| `CallSubroutine` | JSR | MISSING | none |
| `WriteNop` | NOP ZAP | MISSING | none |
| `InjectKey` | KEY | equivalent | Typing in the emulator window feeds the machine the same keys; no debugger-window entry (main-window keyboard path unverified, no file:line) |
| `ShowBranchRecord` | LBR | MISSING | none |
| `Profile` | PROFILE | MISSING | none |
| `Stopwatch` | STOPWATCH | MISSING | none |
| `VideoLog` | VIDEOLOG | MISSING | none |
| `SoundLog` | SOUNDLOG | MISSING | none |
| `Benchmark` | BENCHMARK BENCH | console-only by nature | Handler replies "not available in this session" (ExecutionHandlers.cpp:1167); no operation behind it |
| `ExitBenchmark` | EXITBENCH | console-only by nature | Same handler as BENCHMARK (ExecutionHandlers.cpp:39-40) |
| `ShowCycles` | CYCLES | shown by pane | Cycles row of the Registers pane (DVS:146) and the Cycles row of the Clock device panel (MachineHost.cpp:2034) |
| `ResetCycles` | RCC | MISSING | none |
| `BreakOnVideoLine` | BPV | MISSING | none |
| `BreakOnBeam` | BPBEAM | MISSING | none. Debug > Show beam on screen (MB:312-320) only draws the beam; it sets no stop |
| `RunFrame` | FRAME | covered | Debug > Run one frame (MB:310), key F6 in all schemes (DebuggerKeySchemes.h header comment); DA:173-175. Not in the toolbar rows (DC:42-53) |
| `ShowVideoInfo` | VIDEOINFO | shown by pane | Scanline and Cycle in line rows of the Clock device panel (MachineHost.cpp:2041-2042), View > Device panels |
| `Pause` | PAUSE | covered | Toolbar Pause (DC:43); Debug > Break (MB:303); key goes to PauseDebugger (DW:2791). Main window Debug > Pause (EC:59) |
| `SetBudget` | BUDGET | MISSING | none (the reverse-options budget in Tools > Options, MB:401, is the history size, not a cycle budget) |
| `StepBack` | T- | covered | Toolbar Step back into (DC:48); Debug > Step back into (MB:324); DA:817 |
| `StepBackOver` | P- | covered | Toolbar (DC:49); Debug > Step back over (MB:325); DA:128, 818 |
| `StepBackOut` | GU- | covered | Toolbar (DC:50); Debug > Step back out (MB:326); DA:129, 819 |
| `ReverseGo` | G- | covered | Toolbar Reverse continue (DC:51); Debug > Reverse continue (MB:327); DW:944 |
| `GoLive` | LIVE | covered | Debug > Go live (MB:328); DW:944. No toolbar row (DC:42-53 has none) |
| `SaveState` | SAVESTATE | covered | Main window File > Save state... (EC:22) |
| `LoadState` | LOADSTATE | covered | Main window File > Load state... (EC:23) |

### Breakpoints and watchpoints

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `SetBreakpoint` | BP BPX; BPX; GSSquared bp; WinDbg bp | covered | Gutter click (DW:10371, 10480); code context menu Insert/Remove breakpoint (DW:6255-6256); Breakpoints pane New > Breakpoint at address / Function breakpoint (BPW:76-77); DA:194-208 |
| `SetConditionalBreakpoint` | BP addr IF expr (AppleWinParser.cpp:558); WinDbg bp with condition (WinDbgParser.cpp:925) | covered | Condition field of the breakpoint dialog (BreakpointDialog.cpp:64, 79) via New > Breakpoint at address (BPW:76) |
| `SetRegisterBreakpoint` | BPR | covered | Breakpoints pane New > Register condition... (BPW:79); BreakpointDialog.cpp:75 |
| `SetBreakpointAndWatchpoint` | BPA | MISSING | none: the dialog's types (BreakpointDialog.cpp:206) have no execute+watch type |
| `SetMemoryWatchpoint` | BPIO BPM; GSSquared bpd/bpi rw; WinDbg ba | covered | New > Data breakpoint... and I/O range... (BPW:78, 81); BreakpointDialog.cpp:73 |
| `SetReadWatchpoint` | BPMR; GSSquared bpd r | covered | Data breakpoint dialog, type Read (BreakpointDialog.cpp:71) |
| `SetWriteWatchpoint` | BPMW; GSSquared bpd w | covered | Data breakpoint dialog, type Write (BreakpointDialog.cpp:72) |
| `SetValueBreakpoint` | BPMV | covered | Dialog type Memory value (BreakpointDialog.cpp:74) |
| `BreakOnBrk` | BRK | covered | Dialog type BRK (BreakpointDialog.cpp:77) |
| `BreakOnOpcode` | BRKOP | covered | New > Opcode... (BPW:80); BreakpointDialog.cpp:76 |
| `BreakOnInterrupt` | BRKINT | covered | Dialog type BRK on interrupt (BreakpointDialog.cpp:78) |
| `BreakOnUnwrittenRead` | BRKUNINIT | MISSING | none |
| `ClearBreakpoint` | BPC; GSSquared nobp; WinDbg bc | covered | Breakpoints pane context Remove (DW:6294), toolbar Delete / Delete all (BBC:34-35, BPW:279-285); gutter toggle (DA:247-249) |
| `DisableBreakpoint` | BPD; WinDbg bd | covered | Row checkbox (DW:463); context Disable (DW:6293); toolbar Disable all (BBC:36, BPW:286); DA:226-228 |
| `EnableBreakpoint` | BPE; WinDbg be | covered | Row checkbox (DW:463); context Enable (DW:6293); toolbar Disable all toggles to enable (BPW:286) |
| `ListBreakpoints` | BPL; GSSquared bp (no args); WinDbg bl | shown by pane | Shown by the Breakpoints pane |
| `EditBreakpoint` | BPEDIT | covered | Breakpoints pane context Edit... (DW:6299-6307); DA:717-719 |
| `ChangeBreakpoint` | BPCHANGE | MISSING (partial) | Only undo/redo (BreakpointHistory.cpp:152) and file import (BreakpointImport.cpp:257) reach it; no control sets Temporary or Stops (the Trigger column only displays them, BreakpointColumns.cpp:607) |
| `SaveBreakpoints` | BPSAVE | covered | File > Save breakpoints... (MB:237); pane toolbar Export (BBC:42); BPW:415-435 runs BPSAVE |

### Registers and stack

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `ShowRegisters` | R REGISTER; REGISTER; WinDbg r | shown by pane | Shown by the Registers pane |
| `SetRegister` | R reg [=] value (AppleWinParser.cpp:456); GSSquared r reg val; WinDbg r reg=val | covered | Registers pane in-place edit (DW:8393, 8447, 8465, 8633); DA:398-422; Set next statement for PC (DW:10503) |
| `ClearFlag` | CL CLC CLZ CLI CLD CLB CLR CLV CLN RC RZ RI RD RB RR RV RN | equivalent | Flags dialog on the P row (DW:8403, FlagsDialog.cpp) and auto-watch flag cells (DVS:1733) both write P through SetRegister (DA:398-422) |
| `SetFlag` | SE SEC SEZ SEI SED SEB SER SEV SEN SC SZ SI SD SB SR SV SN | equivalent | Same as ClearFlag |
| `PopStack` | POP | MISSING | none (the Stack pane only edits existing bytes: DW:6361) |
| `PopStackWord` | PPOP | MISSING | none |
| `PushStack` | PUSH | MISSING | none |
| `ShowStack` | STACK | shown by pane | Shown by the Stack pane |

### Memory

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `DumpMemory` | D MDB; GSSquared addr, first.last; WinDbg db, dw, dd, da; Monitor addr (Examine) | shown by pane | Shown by Memory panes (View > Memory); Address box (MemoryBarCommands.cpp:30) |
| `EnterBytes` | ME MEB ME8; GSSquared addr: / set; WinDbg eb, ea | covered | In-place byte edits in the Stack pane (DW:8606), Watches pane auto rows (DVS:1713); DA:627-652. Memory panes use PatchBytes instead |
| `EnterWords` | MEW ME16; WinDbg ew | covered | Watches pane value edit (DVS:1692); DA:581-606 |
| `PatchBytes` | PATCH | covered | Memory pane in-place edit (MemoryPane.cpp:30-35); DA:442-454 |
| `MoveMemory` | M MM; GSSquared move; Monitor M; WinDbg m | MISSING | none |
| `CompareMemory` | MC | MISSING | none |
| `FillMemory` | F; WinDbg f | MISSING | none |
| `SearchMemory` | S MS; Monitor S (with value and range); WinDbg s | MISSING | none (Find, DW:3816, searches only the console and source text) |
| `SearchHex` | SH | MISSING | none |
| `ShowSearchResults` | @ | MISSING | none |
| `LoadBinary` | BLOAD; GSSquared load | MISSING | none |
| `SaveBinary` | BSAVE; GSSquared save | MISSING | none |
| `SaveText` | TSAVE | MISSING | none |
| `ReadIo` | IN INPUT | MISSING | none |
| `WriteIo` | OUT | MISSING | none |
| `ShowSwitches` | SWITCHES | shown by pane | Video device panel (AppleSoftSwitchBank.h:44-45) and MMU panel (Apple2eMmu.h:88-89), opened from View > Device panels |
| `ShowMemoryMap` | MAP | shown by pane | Memory map bar in the Memory panes (Panes/MemoryMapBar.cpp) |

### Disassembly and data directives

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `Disassemble` | U; GSSquared l, list, addrl; WinDbg u | shown by pane | Shown by the Disassembly panes (View > Disassembly) |
| `DefineBytes` | Z DB DB2 DB4 DB8 | MISSING | none |
| `DefineWords` | DW DW2 DW4 | MISSING | none |
| `DefineAddress` | DA | MISSING | none |
| `DefineText` | ASC | MISSING | none |
| `DefineFloat` | DF | MISSING | none |
| `RemoveData` | X | MISSING | none |
| `ListData` | B | MISSING | none |
| `EnterAssembler` | A; Monitor !, F666G | MISSING | none |

### Watches, zero-page pointers and bookmarks

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `AddWatch` | W WA; GSSquared watch | covered | Watches pane add row (DW:7615 -> DA:557); DA:536 |
| `ClearWatch` | WC; GSSquared nowatch | covered | Watches pane context Remove (DW:6326); Delete key (DW:7681); DA:290-292 |
| `DisableWatch` | WD | MISSING | none: context menu is Show in memory / Remove / Copy / Undo (DW:6316-6349); the snapshot has an enabled flag (DebuggerViewState.h WatchLine) but no control |
| `EnableWatch` | WE | MISSING | none |
| `ListWatches` | WL; GSSquared watch (no args) | shown by pane | Shown by the Watches pane |
| `SaveWatches` | WSAVE | MISSING | none |
| `AddZeroPagePointer` | ZP ZP0 ZP1 ZP2 ZP3 ZP4 ZP5 ZP6 ZP7 ZPA P0 P1 P2 P3 P4 | MISSING | none |
| `ClearZeroPagePointer` | ZPC | MISSING | none |
| `DisableZeroPagePointer` | ZPD | MISSING | none |
| `EnableZeroPagePointer` | ZPE | MISSING | none |
| `ListZeroPagePointers` | ZPL | MISSING | none (the pane does not list them) |
| `SaveZeroPagePointers` | ZPSAVE | MISSING | none |
| `AddBookmark` | BM BMA | MISSING | none |
| `ClearBookmark` | BMC | MISSING | none |
| `ListBookmarks` | BML | MISSING | none |
| `GoToBookmark` | BMG | MISSING | none |
| `SaveBookmarks` | BMSAVE | MISSING | none |

### Symbols

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `LoadSymbols` | SYM LOAD file (AppleWinParser.cpp:1304); GSSquared sload | covered | File > Open symbol or debug file... (MB:234, 716-734); drag and drop (DW:2096-2112, 2177); source document Load symbols button (DW:266, 2265) |
| `SaveSymbols` | SYM SAVE file | MISSING | none |
| `ClearSymbols` | SYM CLEAR; GSSquared sclear | MISSING | none |
| `EnableSymbols` | SYM ON, SYM OFF (AppleWinParser.cpp:1297) | MISSING (partial) | Disassembly toolbar Symbols check box (MB:1033-1073) toggles symbol display in the pane; unverified that it changes the session's symbol enable state |
| `LookupSymbol` | SYM SYMMAIN SYMBASIC SYMASM SYMUSER SYMUSER2 SYMSRC SYMSRC2 SYMDOS33 SYMPRODOS SYMDOS SYMPRO; GSSquared slookup; WinDbg x name | MISSING (partial) | Memory Address box accepts a symbol (MemoryBarCommands.cpp:30); no symbol finder |
| `AddSymbol` | SYM name = addr (AppleWinParser.cpp:1276) | MISSING | none |
| `RemoveSymbol` | SYM ! name, SYM ~ name (AppleWinParser.cpp:1318) | MISSING | none |
| `ShowSymbolInfo` | SYMINFO; bare SYM; WinDbg x with no args | MISSING (partial) | Symbols check box tip lists the loaded symbol sources (MB:1073) |
| `ListSymbols` | SYMLIST | MISSING | none |

### Configuration and output

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `PrintDirectory` | PWD | console-only by nature | Session working directory for relative file names; the window uses file pickers |
| `ChangeDirectory` | CD | console-only by nature | Same: GUI file pickers replace it |
| `LoadConfig` | LOAD | MISSING (partial) | File > Load breakpoints... (MB:236, BreakpointImport.cpp) replays breakpoints only; LOAD also replays watches, zero-page pointers and bookmarks (ConfigHandlers.cpp:327-337) |
| `SaveConfig` | SAVE | MISSING (partial) | File > Save breakpoints... (MB:237) saves breakpoints only; SAVE writes all four lists (ConfigHandlers.cpp:327-337) |
| `ConfigureDisassembly` | DISASM | MISSING (partial) | Disassembly toolbar check boxes (MB:1033-1073) are a different option set (addresses, code bytes, source, symbols, line numbers) from DISASM's BRANCH, CLICK, COLON, FENCE, OPCODE, POINTER, SPACES, TARGET (ConfigHandlers.cpp) |
| `RunStartup` | STARTUP | console-only by nature | Runs the startup script; scripting is a console idea |
| `RunScript` | RUN | console-only by nature | Runs a script of console lines. (File > Load breakpoints is only the breakpoint subset.) |
| `DiskCommand` | DISK | equivalent | Main window Disk menu: Insert, Eject, Write-protect per drive (EC:39-46); INFO and SLOT are queries (Disk II debug dialog, EC:61) |
| `Log` | LOG | console-only by nature | Console transcript logging |
| `Echo` | ECHO | console-only by nature | Writes text to the console |
| `Print` | PRINT | console-only by nature | Writes an expression's value to the console |
| `PrintFormatted` | PRINTF | console-only by nature | Formatted print to the console |
| `Calculate` | CALC; WinDbg ?, .formats | console-only by nature | Expression calculator; the memory Address box and watch rows evaluate the same expressions (MemoryBarCommands.cpp:30, DW:7615) |
| `Help` | ? HELP; GSSquared help; WinDbg .help | console-only by nature | Console help text; the panes' info buttons cover colors (MB:189-196 comment) |
| `ShowVersion` | VERSION | console-only by nature | Prints the version to the console |
| `ShowMessageOfTheDay` | MOTD | console-only by nature | Prints the message of the day to the console |

### Monitor

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `Examine` | Monitor addr, first.last | shown by pane | Dialect form of DumpMemory: Memory panes |
| `Deposit` | Monitor addr:bytes | equivalent | Dialect form of EnterBytes/PatchBytes: Memory pane in-place edit (MemoryPane.cpp:30-35) |
| `List` | Monitor L | shown by pane | Dialect form of Disassemble: Disassembly panes |
| `Verify` | Monitor V | MISSING | Dialect form of CompareMemory (MISSING) |
| `Arithmetic` | Monitor a+b, a-b | console-only by nature | Monitor calculator (8-bit), like CALC |
| `SetInverse` | Monitor I | console-only by nature | Monitor ROM idiom: sets the INVFLG text mask. Owner may want a UI |
| `SetNormal` | Monitor N | console-only by nature | Monitor ROM idiom: clears the INVFLG mask. Owner may want a UI |
| `SetInputSlot` | Monitor n Ctrl-K | console-only by nature | Monitor IN# idiom (points the input hook at slot n) |
| `SetOutputSlot` | Monitor n Ctrl-P | console-only by nature | Monitor PR# idiom (points the output hook at slot n) |
| `BasicColdStart` | Monitor Ctrl-B | console-only by nature | Monitor idiom: run BASIC cold start; equivalent is Set next statement + Run. Owner may want a UI |
| `BasicWarmStart` | Monitor Ctrl-C | console-only by nature | Monitor idiom: run BASIC warm start; same |
| `UserVector` | Monitor Ctrl-Y | console-only by nature | Monitor idiom: run the user vector at $3F8; same |
| `ShowRegistersForEdit` | Monitor Ctrl-E | shown by pane | Dialect form of ShowRegisters: Registers pane |
| `EditRegisters` | Monitor : after Ctrl-E | equivalent | Dialect form of SetRegister: Registers pane in-place edit (DW:8465, 8633) |
| `ReadFile` | Monitor R file | MISSING | Dialect form of LoadBinary (MISSING); the window opens a file picker only when the prompt line is typed (DW:8254) |
| `WriteFile` | Monitor W file | MISSING | Dialect form of SaveBinary (MISSING); picker only from the typed prompt line (DW:8254) |

### Casso engine

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `SetMode` | MODE name (AppleWinParser.cpp:1401-1409) | covered | Console bar Mode drop-down (MB:833-863); DA:378-380 |
| `ShowMode` | MODE; MODE | shown by pane | Mode label on the console bar (MB:763-772) |
| `ShowSource` | SRC; SRC; WinDbg l-s, lsa | shown by pane | Shown by the Source pane |
| `SetSourceStepping` | SRC ON, SRC OFF (AppleWinParser.cpp:1425-1435) | covered | Debug > Step by source line (MB:331-340); DW:1964 |
| `SetSourceBreakpoint` | BP file:line; GSSquared bp file:line | covered | Source pane gutter click (SourcePane.cpp:1228-1236); DA:471-473 |
| `ListStepFilter` | SKIP | MISSING | none |
| `AddStepFilter` | SKIP name, SKIP addr, SKIP first.last | MISSING | none |
| `RemoveStepFilter` | SKIP -name | MISSING | none |
| `ClearStepFilter` | SKIP CLEAR | MISSING | none |
| `ShowCallStack` | CALLS; WinDbg k | shown by pane | Shown by the Call stack pane |
| `SetCallStackMode` | CALLS MODE RECORDED, WALK, HYBRID (AppleWinParser.cpp:1483) | covered | Call stack pane mode button (CallStackPane.cpp:52-57); DA:492-494 |
| `ShowHistory` | HISTORY; HISTORY [first [n]] | shown by pane | Shown by the Trace pane (and the history timeline) |
| `SetHistory` | HISTORY ON, HISTORY OFF (AppleWinParser.cpp:1535) | covered | Toolbar Trace toggle (DC:53 -> DW:938); Debug > Trace (MB:342); DA:358-360 |
| `SaveHistory` | HISTORY SAVE file (AppleWinParser.cpp:1542) | covered | File > Save trace... (MB:240; DW:13367-13387); DA:512-516 |
| `ListPanels` | PANEL; PANEL LIST; GSSquared debug (no args) | shown by pane | View > Device panels cascade with the open ones checked (MB:173-176, 255-293) |
| `OpenPanel` | PANEL name, PANEL OPEN name; GSSquared debug name | covered | View > Device panels > name (MB:267, DW:5666); DA:310-312 |
| `ClosePanel` | PANEL CLOSE name; GSSquared nodebug name | covered | Pane close / View menu toggle (DW:5753); DA:310-312 |
| `ShowOutputFormat` | OUTPUT; OUTPUT | console-only by nature | Console reply format; the window has no console-format choice (dialect plumbing) |
| `SetOutputFormat` | OUTPUT name (AppleWinParser.cpp:1413-1421) | console-only by nature | Same |

### Window-only views, carried by name

| Verb | Console words | Status | UI entry point |
|---|---|---|---|
| `View` | Window-only words: . RET ^ V -> PAGEUP... (cursor); WIN WINDOW CODE CODE1 CODE2 CONSOLE DATA DATA1 DATA2 SOURCE1 SOURCE2 \ (panes); MD1 MD2 MA1 MA2 MT1 MT2 M1 M2 (mini memory); TEXT* GR* DGR* HGR* DHGR* (screen); BW COLOR FONT HCOLOR MONO (appearance) | covered | Panes: View menu (MB:255-298). Cursor and mini-memory words move panes (DVS:2502-2616 ExecuteWindowLine). Screen views are answered "not available: the emulator window shows the screen" (DVS:2609-2610). Appearance words: Tools > Theme (MB:372-397) |

## 2. Summary

| Category | Count |
|---|---|
| covered | 44 |
| shown by pane | 18 |
| equivalent | 7 |
| console-only by nature | 25 |
| MISSING (partial) | 7 |
| MISSING | 63 |
| **Total** | **164** |

Reachable from the UI (covered + shown + equivalent): 69. Needing a decision or a UI entry (MISSING + partial): 70. Console-only: 25.

Not verbs, but worth knowing: **Debug > Detach** (MB:304) and the **Tools** and **Machine** menus have no `DebugVerb`. `Trace` (Monitor `T`) is filed as console-only; Monitor ROM idioms `SetInverse`, `SetNormal`, `SetInputSlot`, `SetOutputSlot`, `BasicColdStart`, `BasicWarmStart`, `UserVector` are filed as console-only (dialect) but are flagged "owner may want a UI" where noted.

### Notes on judgment calls

- `ClearFlag`/`SetFlag` are filed "equivalent": the UI edits P through `SetRegister`, never through the flag verbs.
- `SetProgramCounter` is filed "equivalent": Set next statement runs `R PC addr`.
- `InjectKey` is filed "equivalent" on the strength of main-window typing; I did not trace the keyboard path to a file:line (unverified).
- `DiskCommand` is filed "equivalent" for insert, eject and write-protect (main window Disk menu). Whether `DISK INFO` and `DISK SLOT` have any UI is unverified.
- The Monitor/GSSquared/WinDbg dialect verbs that duplicate an AppleWin verb inherit that verb's status (`Verify`, `ReadFile` and `WriteFile` are therefore MISSING).

### MISSING list by area

Proposals follow Visual Studio's debugger where it has an equivalent; "no VS equivalent" says so.

**Execution**

| Verb(s) | Proposal |
|---|---|
| `GoFullSpeed` (GG) | Debug > Run at full speed (checked while on). No VS equivalent. |
| `TraceToFile` (TF) | Debug > Trace to file... toggle (File > Save trace... saves the history, which is a different thing). No VS equivalent (closest: IntelliTrace). |
| `CallSubroutine` (JSR) | Code pane context menu > Call subroutine here (VS: Immediate window function call). |
| `WriteNop` (NOP, ZAP) | Code pane context menu > Replace with NOPs (VS has none; hex editors do). |
| `ShowBranchRecord` (LBR) | A Last branch row in the Registers pane (VS Registers window has the same kind of row), or a Trace pane toolbar entry. |
| `Profile` (PROFILE) | View > Profiler pane with Start, Stop, Reset, Save (VS: Debug > Performance Profiler). |
| `Stopwatch` (STOPWATCH) | Code pane context menu > Start stopwatch here / Stop stopwatch here, result in the Clock device panel. No VS equivalent. |
| `VideoLog`, `SoundLog` (VIDEOLOG, SOUNDLOG) | Log toggle, Clear and Save buttons on the Video and Mockingboard device panels (View > Device panels). |
| `ResetCycles` (RCC) | Context menu on the Cycles row of the Registers pane > Reset cycle counter. |
| `BreakOnVideoLine`, `BreakOnBeam` (BPV, BPBEAM) | Breakpoints pane toolbar New > Video line... and Beam position... (add to `kKinds`, BPW:74-82, and the dialog types in BreakpointDialog.cpp:206). Debug > Show beam on screen (MB:312) only draws the beam. |
| `SetBudget` (BUDGET) | Debug > Run budget... (cycles). Tools > Options (MB:401) already holds the history budget; this is a different setting. No VS equivalent. |

**Breakpoints**

| Verb(s) | Proposal |
|---|---|
| `SetBreakpointAndWatchpoint` (BPA) | Add "Execute and watch" to the breakpoint dialog type list (BreakpointDialog.cpp:206, MakeDefinition at :62). |
| `BreakOnUnwrittenRead` (BRKUNINIT) | Breakpoints pane toolbar New > Break on read of uninitialized RAM; or a "Break on" group of toggles for BRK, BRKINT and BRKUNINIT (VS: Debug > Windows > Exception Settings). |
| `ChangeBreakpoint` (BPCHANGE, partial) | Breakpoints pane context menu > Settings... with Break once and Stops checkboxes (VS: Breakpoint Settings). Only undo/redo reaches it now. |

**Memory, stack and I/O**

| Verb(s) | Proposal |
|---|---|
| `MoveMemory`, `FillMemory`, `CompareMemory` (M, F, MC; Monitor M, V) | Memory pane context menu > Copy range to..., Fill range..., Compare with... (dialogs). VS has none; hex editors do. |
| `SearchMemory`, `SearchHex`, `ShowSearchResults` (S, SH, @) | Make Edit > Find (Ctrl+F, DW:3816) work in Memory panes with hex/text/wildcard modes; Find next/previous (F3) is the `@` results walk. |
| `LoadBinary`, `SaveBinary` (BLOAD, BSAVE; Monitor R, W; GSSquared load, save) | File > Load file into memory... and Save memory range to file...; also in the Memory pane context menu. |
| `SaveText` (TSAVE) | File > Save text screen... (the main window already has Edit > Copy text, EC:30). |
| `ReadIo`, `WriteIo` (IN, OUT) | Tools > I/O access... small dialog (address, value, Read/Write), or a Memory pane context menu entry on $C0xx. |
| `PushStack`, `PopStack`, `PopStackWord` (PUSH, POP, PPOP) | Stack pane context menu > Push byte..., Pop byte, Pop word. VS has none. |

**Disassembly and data directives**

| Verb(s) | Proposal |
|---|---|
| `DefineBytes`, `DefineWords`, `DefineAddress`, `DefineText`, `DefineFloat`, `RemoveData`, `ListData` (Z/DB*, DW*, DA, ASC, DF, X, B) | Code pane context menu > Mark as data > Bytes, Words, Address, Text, Float, and > Clear data marking; a data-ranges list (View > Data ranges, or in the Breakpoints-pane style with Delete and Go to). VS has no data marking in disassembly. |
| `EnterAssembler` (A; Monitor !, F666G) | Code pane context menu > Edit instruction... (inline assembler dialog) and an Edit > Assemble entry. No VS equivalent. |

**Watches, zero-page pointers and bookmarks**

| Verb(s) | Proposal |
|---|---|
| `DisableWatch`, `EnableWatch`, `SaveWatches` (WD, WE, WSAVE) | Watches pane: row checkbox and context Disable/Enable (as the Breakpoints pane, DW:463), and a pane toolbar Export (as BBC:42). |
| `AddZeroPagePointer`, `ClearZeroPagePointer`, `DisableZeroPagePointer`, `EnableZeroPagePointer`, `ListZeroPagePointers`, `SaveZeroPagePointers` (ZP*, P0-P4, ZPC, ZPD, ZPE, ZPL, ZPSAVE) | New View > Zero-page pointers pane in the style of the Watches pane (rows with checkbox, New, Delete, Export in a toolbar); context menu on a zero-page address in Memory > Add as pointer. |
| `AddBookmark`, `ClearBookmark`, `ListBookmarks`, `GoToBookmark`, `SaveBookmarks` (BM, BMA, BMC, BML, BMG, BMSAVE) | View > Bookmarks pane (VS: View > Bookmark Window) with New, Delete, Go to, Export; code pane context menu > Toggle bookmark; Edit > Next/Previous bookmark. |

**Symbols, source and stepping**

| Verb(s) | Proposal |
|---|---|
| `SaveSymbols`, `ClearSymbols`, `AddSymbol`, `RemoveSymbol`, `ListSymbols` (SYM SAVE, SYM CLEAR, SYM name = addr, SYM ! name, SYMLIST) | View > Symbols pane (tables, search box, New, Delete, Clear, Export, per-table enable). File > Save symbols.... Code pane context menu > Add symbol here. |
| `EnableSymbols`, `LookupSymbol`, `ShowSymbolInfo` (partial; SYM ON/OFF, SYM name, SYMINFO) | Same Symbols pane: per-table checkboxes (SYM ON/OFF), search box (SYM name), table list with counts (SYMINFO). The disassembly Symbols check box (MB:1033-1073) stays as the display toggle. |
| `ListStepFilter`, `AddStepFilter`, `RemoveStepFilter`, `ClearStepFilter` (SKIP ...) | Debug > Step filters... list dialog; call stack and code pane context menu > Never step into this routine (VS: Just My Code, Debug > Options > Step over properties and operators). |

**Files and state**

| Verb(s) | Proposal |
|---|---|
| `LoadConfig`, `SaveConfig` (LOAD, SAVE; partial) | File > Load debugger session... / Save debugger session... covering breakpoints, watches, zero-page pointers and bookmarks (the current Load/Save breakpoints entries, MB:236-237, keep their meaning). |
| `ConfigureDisassembly` (DISASM; partial) | Tools > Options > Disassembly page for BRANCH, CLICK, COLON, FENCE, OPCODE, POINTER, SPACES, TARGET (the toolbar check boxes are a different set). |

**History, display and panels**

None missing: every history, display and panel command has a UI entry point or a pane that shows its output (see the table above).

**Misc and owner decisions**

| Verb(s) | Proposal |
|---|---|
| `SetInverse`, `SetNormal`, `SetInputSlot`, `SetOutputSlot`, `BasicColdStart`, `BasicWarmStart`, `UserVector` (Monitor I, N, n Ctrl-K, n Ctrl-P, Ctrl-B, Ctrl-C, Ctrl-Y) | Filed console-only (Monitor ROM idioms). If the owner wants UI: Debug > Run BASIC cold start / warm start / user vector (Ctrl-B, Ctrl-C, Ctrl-Y equivalents), and a Tools > Text mode > Inverse/Normal entry. |
| `Trace` (Monitor T) | Filed console-only; if wanted, Debug > Run with trace echo to the console. |
| `Benchmark`, `ExitBenchmark` | Handler answers "not available in this session" (ExecutionHandlers.cpp:1167): no operation exists to expose. Consider removing the verbs or implementing first. |

