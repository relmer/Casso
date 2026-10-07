# Debugger command reference

Generated from the debugger's help table; do not edit by hand. A unit test
fails when this file and `help all` differ. Regenerate it with
`pwsh scripts/UpdateDebuggerCommands.ps1`.

## AppleWin mode

A bare number is hex; $ also marks hex, and # or 0n marks decimal, as in 0n192.

```text
AppleWin commands:
  Running and stepping
    = addr                                               Set the program counter
    CYCLES [ABS|REL|PART]                                Show the cycles in total, in the last run, or since RCC
    FRAME [count]                                        Run one video frame, or count of them
    G [addr [skip[,len|:last]]]                          Run, stopping at addr or when PC leaves the skip range
    G-                                                   Run backward through the recorded history to the latest breakpoint or watchpoint
    GG [addr [skip[,len|:last]]]                         Run at full speed, stopping as G does
    GU-                                                  Step back out to the JSR that called the current subroutine
    JSR addr                                             Call a subroutine and run until it returns
    KEY byte [byte ...]                                  Queue key codes for the machine to read
    LBR                                                  Show the last branch taken
    LIVE                                                 Leave the recorded history and return to where the machine was running
    P [count]                                            Step over
    P-                                                   Step back over, taking a whole subroutine call as one step
    RCC                                                  Reset the cycle counter that CYCLES PART reads
    RTS [count]                                          Step out of the current subroutine
    T, TL, TRACE [count]                                 Step into
    T-                                                   Step back one instruction
    TF [file] [V]                                        Turn tracing to a file on or off; V records the video position
  Breakpoints
    BP addr[,len|:last]|file:line [IF expr]              Set an execution breakpoint, or one on PC with < > = !
    BPA addr[,len|:last]                                 Set an execution breakpoint and a memory watchpoint
    BPBEAM line cycle|VBL                                Stop when the beam reaches a scanline and cycle, or vertical blank
    BPC #|*                                              Clear a breakpoint, or all of them
    BPCHANGE # flags                                     Change a breakpoint's flags: E enabled, T temporary, S stops
    BPD #|*                                              Disable a breakpoint, or all of them
    BPE #|*                                              Enable a breakpoint, or all of them
    BPEDIT # definition                                  Replace a breakpoint with a new definition
    BPL                                                  List the breakpoints
    BPM, BPIO addr[,len|:last] [BEFORE|AFTER] [IF expr]  Stop on a read or write of memory
    BPMR addr[,len|:last] [BEFORE|AFTER] [IF expr]       Stop on a read of memory
    BPMV addr byte [IF expr]                             Stop when a write leaves addr holding byte
    BPMW addr[,len|:last] [BEFORE|AFTER] [IF expr]       Stop on a write to memory
    BPR reg [op] value                                   Stop when a register meets a condition
    BPSAVE file                                          Save the breakpoints as a script
    BPV line[,len|:last]                                 Stop at a video scanline
    BPX addr[,len|:last]|file:line [IF expr]             Set an execution breakpoint, as BP does
    BRK [0|1|2|3|ALL] [ON|OFF]                           Stop on BRK, or on invalid opcodes of a length
    BRKINT [ON|OFF]                                      Stop on an interrupt
    BRKOP [opcode ...]                                   Stop on an opcode, or list the opcode breakpoints
    BRKUNINIT [ON|OFF]                                   Stop on a read of RAM nothing has written since power-on
  Registers and flags
    CL flag                                              Clear a flag: C, Z, I, D, B, R, V or N
    CLB, RB                                              Clear the break flag
    CLC, RC                                              Clear the carry flag
    CLD, RD                                              Clear the decimal flag
    CLI, RI                                              Clear the interrupt disable flag
    CLN, RN                                              Clear the negative flag
    CLR, RR                                              Clear the reserved flag
    CLV, RV                                              Clear the overflow flag
    CLZ, RZ                                              Clear the zero flag
    POP                                                  Pop a byte off the stack
    PPOP                                                 Pop a word off the stack
    PUSH byte [byte ...]                                 Push bytes onto the stack
    R, REGISTER [reg [=] value]                          Show the registers, or set one
    SE flag                                              Set a flag: C, Z, I, D, B, R, V or N
    SEB, SB                                              Set the break flag
    SEC, SC                                              Set the carry flag
    SED, SD                                              Set the decimal flag
    SEI, SI                                              Set the interrupt disable flag
    SEN, SN                                              Set the negative flag
    SER, SR                                              Set the reserved flag
    SEV, SV                                              Set the overflow flag
    SEZ, SZ                                              Set the zero flag
  Memory
    @                                                    Show the last search's results
    BLOAD file [addr[,len|:last]]                        Load a file into memory
    BSAVE file addr[,len|:last]                          Save memory to a file
    D, MDB [addr[,len|:last]]                            Show memory
    F addr[,len|:last] byte ...                          Fill memory with bytes; F first last byte ... also works
    IN, INPUT addr                                       Read an I/O address
    M, MM dest src[,len|:last]                           Copy memory
    MC dest src[,len|:last]                              Compare memory
    ME addr value ...                                    Store values at addr, a value over $FF as a word
    MEB, ME8 addr value ...                              Store values at addr, a value over $FF as a word
    MEW, ME16 addr word ...                              Store words at addr, low byte first
    NOP, ZAP                                             Replace the instruction at PC with NOPs
    OUT addr byte [byte ...]                             Write to an I/O address
    S, MS addr[,len|:last] item ...                      Search memory for bytes, "text", 'text' or wildcards
    SH addr[,len|:last] item ...                         Search memory, as S does
    TSAVE file                                           Save the text screen to a file
    W [addr]                                             Add a watch, or list them
    WA [addr]                                            Add a watch, or list them
    WC #|*                                               Clear a watch, or all of them
    WD #|*                                               Disable a watch, or all of them
    WE #|*                                               Enable a watch, or all of them
    WL                                                   List the watches
    WSAVE file                                           Save the watches as a script
    ZP [addr]                                            Add a zero-page pointer, or list them
    ZP0, P0 [addr]                                       Set zero-page pointer 0, or list them
    ZP1, P1 [addr]                                       Set zero-page pointer 1, or list them
    ZP2, P2 [addr]                                       Set zero-page pointer 2, or list them
    ZP3, P3 [addr]                                       Set zero-page pointer 3, or list them
    ZP4, P4 [addr]                                       Set zero-page pointer 4, or list them
    ZP5 [addr]                                           Set zero-page pointer 5, or list them
    ZP6 [addr]                                           Set zero-page pointer 6, or list them
    ZP7 [addr]                                           Set zero-page pointer 7, or list them
    ZPA [addr]                                           Add a zero-page pointer, or list them
    ZPC #|*                                              Clear a zero-page pointer, or all of them
    ZPD #|*                                              Disable a zero-page pointer, or all of them
    ZPE #|*                                              Enable a zero-page pointer, or all of them
    ZPL                                                  List the zero-page pointers
    ZPSAVE file                                          Save the zero-page pointers as a script
  Disassembly and data
    A [addr]                                             Assemble at addr, or at PC; a blank line ends it
    ASC [name [=]] [addr[,len|:last]]                    Mark memory as text, or list the data blocks
    B                                                    List the data blocks
    BM [addr]                                            Add a bookmark, or list them
    BMA [addr]                                           Add a bookmark, or list them
    BMC #|*                                              Clear a bookmark, or all of them
    BMG #                                                Disassemble at a bookmark
    BML                                                  List the bookmarks
    BMSAVE file                                          Save the bookmarks as a script
    DA [name [=]] [addr[,len|:last]]                     Mark memory as addresses, or list the data blocks
    DB, Z [name [=]] [addr[,len|:last]]                  Mark memory as bytes, or list the data blocks
    DB2 [name [=]] [addr[,len|:last]]                    Mark memory as bytes, 2 a line
    DB4 [name [=]] [addr[,len|:last]]                    Mark memory as bytes, 4 a line
    DB8 [name [=]] [addr[,len|:last]]                    Mark memory as bytes, 8 a line
    DF [name [=]] [addr[,len|:last]]                     Mark memory as floating-point numbers
    DISASM [setting [0|1]]                               Show or change the disassembly settings
    DW [name [=]] [addr[,len|:last]]                     Mark memory as words, or list the data blocks
    DW2 [name [=]] [addr[,len|:last]]                    Mark memory as words, 2 a line
    DW4 [name [=]] [addr[,len|:last]]                    Mark memory as words, 4 a line
    U [addr[,len|:last]]                                 Disassemble, or continue the last listing
    X addr[,len|:last]                                   Make a data block code again
  Symbols and source
    SYM [name|addr|name=addr|cmd]                        Find or add a symbol in any table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMASM [name|addr|name=addr|cmd]                     Find or add a symbol in the assembler table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMBASIC [name|addr|name=addr|cmd]                   Find or add a symbol in the Applesoft table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMDOS33, SYMDOS [name|addr|name=addr|cmd]           Find or add a symbol in the DOS 3.3 table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMINFO                                              Show each symbol table's count and whether it is on
    SYMLIST [table]                                      List a symbol table, the user table by default
    SYMMAIN [name|addr|name=addr|cmd]                    Find or add a symbol in the main table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMPRODOS, SYMPRO [name|addr|name=addr|cmd]          Find or add a symbol in the ProDOS table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMSRC [name|addr|name=addr|cmd]                     Find or add a symbol in the source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMSRC2 [name|addr|name=addr|cmd]                    Find or add a symbol in the second source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMUSER [name|addr|name=addr|cmd]                    Find or add a symbol in the user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMUSER2 [name|addr|name=addr|cmd]                   Find or add a symbol in the second user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
  Disks
    DISK [INFO|SLOT]                                     Show the drives and their disks, or the disk slot
  Display and panels
    ->                                                   Move the code pane to the operand's address
    .                                                    Make the code pane follow PC again
    ^                                                    Move the code pane up one instruction
    CODE                                                 Bring the disassembly forward
    CODE1                                                Bring the first disassembly forward
    CODE2                                                Bring the second disassembly forward, opening it
    CONSOLE                                              Bring the console forward
    DATA                                                 Bring the memory window forward
    DATA1                                                Bring the first memory window forward
    DATA2                                                Bring the second memory window forward, opening it
    M1 addr                                              Move the memory pane to a hex address
    M2 addr                                              Move the memory pane to a hex address
    MA1 addr                                             Move the memory pane to a hex address
    MA2 addr                                             Move the memory pane to a hex address
    MD1 addr                                             Move the memory pane to a hex address
    MD2 addr                                             Move the memory pane to a hex address
    MT1 addr                                             Move the memory pane to a hex address
    MT2 addr                                             Move the memory pane to a hex address
    PAGEDN                                               Move the code pane down a page
    PAGEDOWN256                                          Move the code pane ahead $100 bytes
    PAGEDOWN4K                                           Move the code pane ahead $1000 bytes
    PAGEUP                                               Move the code pane up a page
    PAGEUP256                                            Move the code pane back $100 bytes
    PAGEUP4K                                             Move the code pane back $1000 bytes
    RET                                                  Move the code pane to the return address
    V                                                    Move the code pane down one instruction
    VIDEOINFO                                            Show the video scanner's position
  Session and settings
    ? [command]                                          List the commands, or describe one
    CALC expr                                            Evaluate an expression
    CD dir                                               Change the current directory
    ECHO text                                            Print text
    HELP [all|section|command|text]                      List the help sections, one section, every command, one command or the matches for text
    LOAD file                                            Run a script of commands, such as one SAVE wrote
    LOG [level]                                          Show or set which notifications print
    MOTD                                                 Show the message of the day
    PRINT item[,item ...]                                Print strings, and expressions in hex
    PRINTF "format"[,expr ...]                           Print expressions through a format
    PWD                                                  Show the current directory
    RUN file                                             Run a script of commands
    SAVE file                                            Save breakpoints, watches, pointers and bookmarks
    STARTUP                                              Run the startup script, DebuggerAutoRun.txt
    VERSION                                              Show Casso's version

Casso commands:
  Running and stepping
    BUDGET cycles                                        Stop each run after a number of cycles, in decimal; 0 removes the limit
    HISTORY [ON|OFF|SAVE file|first [n]]                 Show the instruction trace, turn it on or off, or save it
    PAUSE                                                Stop the running machine
    PROFILE [ON|OFF|RESET|LIST [ADDR]|SAVE [file]]       Profile where execution goes, by routine or by address
    SKIP [CLEAR|[-]name|addr[.last]]                     Show or change the routines stepping goes over
    SOUNDLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]         Log speaker toggles and Mockingboard writes with their cycles
    STOPWATCH [start [stop]|OFF|RESET]                   Count the cycles between two addresses in debugger runs
  Registers and flags
    CALLS [MODE [RECORDED|WALK|HYBRID]]                  Show the call stack, or choose how it is found
    STACK                                                Show the stack
  Memory
    MAP                                                  Show where each address range reads and writes now
    PATCH addr value ...                                 Store values at addr, ROM included
    SWITCHES                                             Show the soft switches
  Symbols and source
    SRC [ON|OFF]                                         Show the source line at PC, or step by source lines
  Display and panels
    PANEL [LIST|[OPEN|CLOSE] name [OPEN|CLOSE]]          List the device panels, or open or close one
    VIDEOLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]         Log video mode changes with the frame and beam position
  Session and settings
    LOADSTATE file                                       Replace the whole machine, disks included, with one a state file holds
    MODE [mode]                                          Show or set the command mode, which sets the output too
    OUTPUT [format]                                      Show or set the format replies are written in
    SAVESTATE file                                       Save the whole machine, disks included, to a state file
```

## Monitor mode

Numbers are hex digits, with no prefix.

```text
Monitor commands:
  Running and stepping
    ^B                                                     BASIC cold start
    ^C                                                     BASIC warm start
    ^Y                                                     Jump through the user vector at $03F8
    addrG                                                  Run from addr
    addrS                                                  Step one instruction
    addrT                                                  Trace from addr
  Registers and flags
    ^E                                                     Show the registers; a : after it changes them
  Memory
    addr                                                   Show the byte at addr
    addr: bb bb ...                                        Store bytes starting at addr
    dest<first.lastM                                       Copy first..last to dest
    dest<first.lastV                                       Compare first..last with the bytes at dest
    first.last                                             Show the bytes from first to last
    first.lastR name                                       Read a file into first..last
    first.lastW name                                       Write first..last to a file
    Return                                                 Show the next row of bytes
    value<first.lastS                                      Search first..last for value
  Disassembly and data
    !                                                      Enter the mini-assembler
    addrL                                                  Disassemble from addr, or on from the last listing
  Display and panels
    I                                                      Inverse text
    N                                                      Normal text
  Session and settings
    a+b                                                    Add two hex bytes, eight-bit
    a-b                                                    Subtract two hex bytes, eight-bit
    slot^K                                                 Take input from a slot
    slot^P                                                 Send output to a slot

Casso commands:
  Running and stepping
    /= addr                                                Set the program counter
    /BUDGET cycles                                         Stop each run after a number of cycles, in decimal; 0 removes the limit
    /CYCLES [ABS|REL|PART]                                 Show the cycles in total, in the last run, or since RCC
    /FRAME [count]                                         Run one video frame, or count of them
    /G-                                                    Run backward through the recorded history to the latest breakpoint or watchpoint
    /GG [addr [skip[,len|:last]]]                          Run at full speed, stopping as G does
    /GU-                                                   Step back out to the JSR that called the current subroutine
    /HISTORY [ON|OFF|SAVE file|first [n]]                  Show the instruction trace, turn it on or off, or save it
    /JSR addr                                              Call a subroutine and run until it returns
    /KEY byte [byte ...]                                   Queue key codes for the machine to read
    /LBR                                                   Show the last branch taken
    /LIVE                                                  Leave the recorded history and return to where the machine was running
    /P [count]                                             Step over
    /P-                                                    Step back over, taking a whole subroutine call as one step
    /PAUSE                                                 Stop the running machine
    /PROFILE [ON|OFF|RESET|LIST [ADDR]|SAVE [file]]        Profile where execution goes, by routine or by address
    /RCC                                                   Reset the cycle counter that CYCLES PART reads
    /RTS [count]                                           Step out of the current subroutine
    /SKIP [CLEAR|[-]name|addr[.last]]                      Show or change the routines stepping goes over
    /SOUNDLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]          Log speaker toggles and Mockingboard writes with their cycles
    /STOPWATCH [start [stop]|OFF|RESET]                    Count the cycles between two addresses in debugger runs
    /T-                                                    Step back one instruction
    /TF [file] [V]                                         Turn tracing to a file on or off; V records the video position
  Breakpoints
    /BP addr[,len|:last]|file:line [IF expr]               Set an execution breakpoint, or one on PC with < > = !
    /BPA addr[,len|:last]                                  Set an execution breakpoint and a memory watchpoint
    /BPBEAM line cycle|VBL                                 Stop when the beam reaches a scanline and cycle, or vertical blank
    /BPC #|*                                               Clear a breakpoint, or all of them
    /BPCHANGE # flags                                      Change a breakpoint's flags: E enabled, T temporary, S stops
    /BPD #|*                                               Disable a breakpoint, or all of them
    /BPE #|*                                               Enable a breakpoint, or all of them
    /BPEDIT # definition                                   Replace a breakpoint with a new definition
    /BPL                                                   List the breakpoints
    /BPM, /BPIO addr[,len|:last] [BEFORE|AFTER] [IF expr]  Stop on a read or write of memory
    /BPMR addr[,len|:last] [BEFORE|AFTER] [IF expr]        Stop on a read of memory
    /BPMV addr byte [IF expr]                              Stop when a write leaves addr holding byte
    /BPMW addr[,len|:last] [BEFORE|AFTER] [IF expr]        Stop on a write to memory
    /BPR reg [op] value                                    Stop when a register meets a condition
    /BPSAVE file                                           Save the breakpoints as a script
    /BPV line[,len|:last]                                  Stop at a video scanline
    /BPX addr[,len|:last]|file:line [IF expr]              Set an execution breakpoint, as BP does
    /BRK [0|1|2|3|ALL] [ON|OFF]                            Stop on BRK, or on invalid opcodes of a length
    /BRKINT [ON|OFF]                                       Stop on an interrupt
    /BRKOP [opcode ...]                                    Stop on an opcode, or list the opcode breakpoints
    /BRKUNINIT [ON|OFF]                                    Stop on a read of RAM nothing has written since power-on
  Registers and flags
    /CALLS [MODE [RECORDED|WALK|HYBRID]]                   Show the call stack, or choose how it is found
    /CL flag                                               Clear a flag: C, Z, I, D, B, R, V or N
    /CLB, /RB                                              Clear the break flag
    /CLC, /RC                                              Clear the carry flag
    /CLD, /RD                                              Clear the decimal flag
    /CLI, /RI                                              Clear the interrupt disable flag
    /CLN, /RN                                              Clear the negative flag
    /CLR, /RR                                              Clear the reserved flag
    /CLV, /RV                                              Clear the overflow flag
    /CLZ, /RZ                                              Clear the zero flag
    /POP                                                   Pop a byte off the stack
    /PPOP                                                  Pop a word off the stack
    /PUSH byte [byte ...]                                  Push bytes onto the stack
    /SE flag                                               Set a flag: C, Z, I, D, B, R, V or N
    /SEB, /SB                                              Set the break flag
    /SEC, /SC                                              Set the carry flag
    /SED, /SD                                              Set the decimal flag
    /SEI, /SI                                              Set the interrupt disable flag
    /SEN, /SN                                              Set the negative flag
    /SER, /SR                                              Set the reserved flag
    /SEV, /SV                                              Set the overflow flag
    /SEZ, /SZ                                              Set the zero flag
    /STACK                                                 Show the stack
  Memory
    /@                                                     Show the last search's results
    /F addr[,len|:last] byte ...                           Fill memory with bytes; F first last byte ... also works
    /IN, /INPUT addr                                       Read an I/O address
    /MAP                                                   Show where each address range reads and writes now
    /MEW, /ME16 addr word ...                              Store words at addr, low byte first
    /NOP, /ZAP                                             Replace the instruction at PC with NOPs
    /OUT addr byte [byte ...]                              Write to an I/O address
    /PATCH addr value ...                                  Store values at addr, ROM included
    /SWITCHES                                              Show the soft switches
    /TSAVE file                                            Save the text screen to a file
    /W [addr]                                              Add a watch, or list them
    /WA [addr]                                             Add a watch, or list them
    /WC #|*                                                Clear a watch, or all of them
    /WD #|*                                                Disable a watch, or all of them
    /WE #|*                                                Enable a watch, or all of them
    /WL                                                    List the watches
    /WSAVE file                                            Save the watches as a script
    /ZP [addr]                                             Add a zero-page pointer, or list them
    /ZP0, /P0 [addr]                                       Set zero-page pointer 0, or list them
    /ZP1, /P1 [addr]                                       Set zero-page pointer 1, or list them
    /ZP2, /P2 [addr]                                       Set zero-page pointer 2, or list them
    /ZP3, /P3 [addr]                                       Set zero-page pointer 3, or list them
    /ZP4, /P4 [addr]                                       Set zero-page pointer 4, or list them
    /ZP5 [addr]                                            Set zero-page pointer 5, or list them
    /ZP6 [addr]                                            Set zero-page pointer 6, or list them
    /ZP7 [addr]                                            Set zero-page pointer 7, or list them
    /ZPA [addr]                                            Add a zero-page pointer, or list them
    /ZPC #|*                                               Clear a zero-page pointer, or all of them
    /ZPD #|*                                               Disable a zero-page pointer, or all of them
    /ZPE #|*                                               Enable a zero-page pointer, or all of them
    /ZPL                                                   List the zero-page pointers
    /ZPSAVE file                                           Save the zero-page pointers as a script
  Disassembly and data
    /ASC [name [=]] [addr[,len|:last]]                     Mark memory as text, or list the data blocks
    /B                                                     List the data blocks
    /BM [addr]                                             Add a bookmark, or list them
    /BMA [addr]                                            Add a bookmark, or list them
    /BMC #|*                                               Clear a bookmark, or all of them
    /BMG #                                                 Disassemble at a bookmark
    /BML                                                   List the bookmarks
    /BMSAVE file                                           Save the bookmarks as a script
    /DA [name [=]] [addr[,len|:last]]                      Mark memory as addresses, or list the data blocks
    /DB, /Z [name [=]] [addr[,len|:last]]                  Mark memory as bytes, or list the data blocks
    /DB2 [name [=]] [addr[,len|:last]]                     Mark memory as bytes, 2 a line
    /DB4 [name [=]] [addr[,len|:last]]                     Mark memory as bytes, 4 a line
    /DB8 [name [=]] [addr[,len|:last]]                     Mark memory as bytes, 8 a line
    /DF [name [=]] [addr[,len|:last]]                      Mark memory as floating-point numbers
    /DISASM [setting [0|1]]                                Show or change the disassembly settings
    /DW [name [=]] [addr[,len|:last]]                      Mark memory as words, or list the data blocks
    /DW2 [name [=]] [addr[,len|:last]]                     Mark memory as words, 2 a line
    /DW4 [name [=]] [addr[,len|:last]]                     Mark memory as words, 4 a line
    /X addr[,len|:last]                                    Make a data block code again
  Symbols and source
    /SRC [ON|OFF]                                          Show the source line at PC, or step by source lines
    /SYM [name|addr|name=addr|cmd]                         Find or add a symbol in any table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    /SYMASM [name|addr|name=addr|cmd]                      Find or add a symbol in the assembler table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    /SYMBASIC [name|addr|name=addr|cmd]                    Find or add a symbol in the Applesoft table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    /SYMDOS33, /SYMDOS [name|addr|name=addr|cmd]           Find or add a symbol in the DOS 3.3 table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    /SYMINFO                                               Show each symbol table's count and whether it is on
    /SYMLIST [table]                                       List a symbol table, the user table by default
    /SYMMAIN [name|addr|name=addr|cmd]                     Find or add a symbol in the main table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    /SYMPRODOS, /SYMPRO [name|addr|name=addr|cmd]          Find or add a symbol in the ProDOS table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    /SYMSRC [name|addr|name=addr|cmd]                      Find or add a symbol in the source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    /SYMSRC2 [name|addr|name=addr|cmd]                     Find or add a symbol in the second source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    /SYMUSER [name|addr|name=addr|cmd]                     Find or add a symbol in the user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    /SYMUSER2 [name|addr|name=addr|cmd]                    Find or add a symbol in the second user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
  Disks
    /DISK [INFO|SLOT]                                      Show the drives and their disks, or the disk slot
  Display and panels
    /->                                                    Move the code pane to the operand's address
    /.                                                     Make the code pane follow PC again
    /^                                                     Move the code pane up one instruction
    /CODE                                                  Bring the disassembly forward
    /CODE1                                                 Bring the first disassembly forward
    /CODE2                                                 Bring the second disassembly forward, opening it
    /CONSOLE                                               Bring the console forward
    /DATA                                                  Bring the memory window forward
    /DATA1                                                 Bring the first memory window forward
    /DATA2                                                 Bring the second memory window forward, opening it
    /M1 addr                                               Move the memory pane to a hex address
    /M2 addr                                               Move the memory pane to a hex address
    /MA1 addr                                              Move the memory pane to a hex address
    /MA2 addr                                              Move the memory pane to a hex address
    /MD1 addr                                              Move the memory pane to a hex address
    /MD2 addr                                              Move the memory pane to a hex address
    /MT1 addr                                              Move the memory pane to a hex address
    /MT2 addr                                              Move the memory pane to a hex address
    /PAGEDN                                                Move the code pane down a page
    /PAGEDOWN256                                           Move the code pane ahead $100 bytes
    /PAGEDOWN4K                                            Move the code pane ahead $1000 bytes
    /PAGEUP                                                Move the code pane up a page
    /PAGEUP256                                             Move the code pane back $100 bytes
    /PAGEUP4K                                              Move the code pane back $1000 bytes
    /PANEL [LIST|[OPEN|CLOSE] name [OPEN|CLOSE]]           List the device panels, or open or close one
    /RET                                                   Move the code pane to the return address
    /V                                                     Move the code pane down one instruction
    /VIDEOINFO                                             Show the video scanner's position
    /VIDEOLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]          Log video mode changes with the frame and beam position
  Session and settings
    /? [command]                                           List the commands, or describe one
    /CALC expr                                             Evaluate an expression
    /CD dir                                                Change the current directory
    /ECHO text                                             Print text
    /HELP [all|section|command|text]                       List the help sections, one section, every command, one command or the matches for text
    /LOAD file                                             Run a script of commands, such as one SAVE wrote
    /LOADSTATE file                                        Replace the whole machine, disks included, with one a state file holds
    /LOG [level]                                           Show or set which notifications print
    /MODE [mode]                                           Show or set the command mode, which sets the output too
    /MOTD                                                  Show the message of the day
    /OUTPUT [format]                                       Show or set the format replies are written in
    /PRINT item[,item ...]                                 Print strings, and expressions in hex
    /PRINTF "format"[,expr ...]                            Print expressions through a format
    /PWD                                                   Show the current directory
    /RUN file                                              Run a script of commands
    /SAVE file                                             Save breakpoints, watches, pointers and bookmarks
    /SAVESTATE file                                        Save the whole machine, disks included, to a state file
    /STARTUP                                               Run the startup script, DebuggerAutoRun.txt
    /VERSION                                               Show Casso's version
```

## GSSquared mode

A bare number is hex; 0n marks decimal, as in 0n192.

```text
GSSquared commands:
  Running and stepping
    g, or Return                                    Run
    g-                                              Run backward to a breakpoint
    gu-                                             Step back out
    o                                               Step over
    p-                                              Step back over
    r                                               Step out
    s, or Space                                     Step into
    t-                                              Step back into
  Breakpoints
    bp addr [IF expr]                               Set an execution breakpoint
    bpd addr r|w|rw                                 Break on a read or write of addr
    bpi addr r|w|rw                                 Break on an I/O access, $C000-$C0FF
    nobp id|addr                                    Clear a breakpoint
  Memory
    addr                                            Show the byte at addr
    addr: bb bb ...                                 Store bytes starting at addr
    first.last                                      Show the bytes from first to last
    load "file" addr                                Read a file into memory at addr
    move first.last dest                            Copy first..last to dest
    nowatch id                                      Stop watching
    save "file" first.last                          Write first..last to a file
    set addr bb bb ...                              Store bytes starting at addr
    watch addr                                      Watch addr in the watch pane
  Disassembly and data
    l [addr]                                        Disassemble from addr, or on from the last listing
    list [addr]                                     Disassemble from addr, or on from the last listing
  Symbols and source
    sclear                                          Clear the loaded symbols
    sload "file"                                    Load a symbol file
    slookup addr                                    Show the symbol at addr
  Session and settings
    help [word]                                     This list, or one command

Casso commands:
  Running and stepping
    = addr                                          Set the program counter
    BUDGET cycles                                   Stop each run after a number of cycles, in decimal; 0 removes the limit
    CYCLES [ABS|REL|PART]                           Show the cycles in total, in the last run, or since RCC
    FRAME [count]                                   Run one video frame, or count of them
    GG [addr [skip[,len|:last]]]                    Run at full speed, stopping as G does
    HISTORY [ON|OFF|SAVE file|first [n]]            Show the instruction trace, turn it on or off, or save it
    JSR addr                                        Call a subroutine and run until it returns
    KEY byte [byte ...]                             Queue key codes for the machine to read
    LBR                                             Show the last branch taken
    LIVE                                            Leave the recorded history and return to where the machine was running
    PAUSE                                           Stop the running machine
    PROFILE [ON|OFF|RESET|LIST [ADDR]|SAVE [file]]  Profile where execution goes, by routine or by address
    RCC                                             Reset the cycle counter that CYCLES PART reads
    SKIP [CLEAR|[-]name|addr[.last]]                Show or change the routines stepping goes over
    SOUNDLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]    Log speaker toggles and Mockingboard writes with their cycles
    STOPWATCH [start [stop]|OFF|RESET]              Count the cycles between two addresses in debugger runs
    TF [file] [V]                                   Turn tracing to a file on or off; V records the video position
  Breakpoints
    BPA addr[,len|:last]                            Set an execution breakpoint and a memory watchpoint
    BPBEAM line cycle|VBL                           Stop when the beam reaches a scanline and cycle, or vertical blank
    BPCHANGE # flags                                Change a breakpoint's flags: E enabled, T temporary, S stops
    BPE #|*                                         Enable a breakpoint, or all of them
    BPEDIT # definition                             Replace a breakpoint with a new definition
    BPL                                             List the breakpoints
    BPMV addr byte [IF expr]                        Stop when a write leaves addr holding byte
    BPR reg [op] value                              Stop when a register meets a condition
    BPSAVE file                                     Save the breakpoints as a script
    BPV line[,len|:last]                            Stop at a video scanline
    BRK [0|1|2|3|ALL] [ON|OFF]                      Stop on BRK, or on invalid opcodes of a length
    BRKINT [ON|OFF]                                 Stop on an interrupt
    BRKOP [opcode ...]                              Stop on an opcode, or list the opcode breakpoints
    BRKUNINIT [ON|OFF]                              Stop on a read of RAM nothing has written since power-on
  Registers and flags
    CALLS [MODE [RECORDED|WALK|HYBRID]]             Show the call stack, or choose how it is found
    CL flag                                         Clear a flag: C, Z, I, D, B, R, V or N
    CLB, RB                                         Clear the break flag
    CLC, RC                                         Clear the carry flag
    CLD, RD                                         Clear the decimal flag
    CLI, RI                                         Clear the interrupt disable flag
    CLN, RN                                         Clear the negative flag
    CLR, RR                                         Clear the reserved flag
    CLV, RV                                         Clear the overflow flag
    CLZ, RZ                                         Clear the zero flag
    POP                                             Pop a byte off the stack
    PPOP                                            Pop a word off the stack
    PUSH byte [byte ...]                            Push bytes onto the stack
    REGISTER [reg [=] value]                        Show the registers, or set one
    SE flag                                         Set a flag: C, Z, I, D, B, R, V or N
    SEB, SB                                         Set the break flag
    SEC, SC                                         Set the carry flag
    SED, SD                                         Set the decimal flag
    SEI, SI                                         Set the interrupt disable flag
    SEN, SN                                         Set the negative flag
    SER, SR                                         Set the reserved flag
    SEV, SV                                         Set the overflow flag
    SEZ, SZ                                         Set the zero flag
    STACK                                           Show the stack
  Memory
    @                                               Show the last search's results
    IN, INPUT addr                                  Read an I/O address
    MAP                                             Show where each address range reads and writes now
    MC dest src[,len|:last]                         Compare memory
    MEW, ME16 addr word ...                         Store words at addr, low byte first
    MS addr[,len|:last] item ...                    Search memory for bytes, "text", 'text' or wildcards
    NOP, ZAP                                        Replace the instruction at PC with NOPs
    OUT addr byte [byte ...]                        Write to an I/O address
    PATCH addr value ...                            Store values at addr, ROM included
    SH addr[,len|:last] item ...                    Search memory, as S does
    SWITCHES                                        Show the soft switches
    TSAVE file                                      Save the text screen to a file
    WD #|*                                          Disable a watch, or all of them
    WE #|*                                          Enable a watch, or all of them
    WL                                              List the watches
    WSAVE file                                      Save the watches as a script
    ZP [addr]                                       Add a zero-page pointer, or list them
    ZP0, P0 [addr]                                  Set zero-page pointer 0, or list them
    ZP1, P1 [addr]                                  Set zero-page pointer 1, or list them
    ZP2, P2 [addr]                                  Set zero-page pointer 2, or list them
    ZP3, P3 [addr]                                  Set zero-page pointer 3, or list them
    ZP4, P4 [addr]                                  Set zero-page pointer 4, or list them
    ZP5 [addr]                                      Set zero-page pointer 5, or list them
    ZP6 [addr]                                      Set zero-page pointer 6, or list them
    ZP7 [addr]                                      Set zero-page pointer 7, or list them
    ZPA [addr]                                      Add a zero-page pointer, or list them
    ZPC #|*                                         Clear a zero-page pointer, or all of them
    ZPD #|*                                         Disable a zero-page pointer, or all of them
    ZPE #|*                                         Enable a zero-page pointer, or all of them
    ZPL                                             List the zero-page pointers
    ZPSAVE file                                     Save the zero-page pointers as a script
  Disassembly and data
    ASC [name [=]] [addr[,len|:last]]               Mark memory as text, or list the data blocks
    BM [addr]                                       Add a bookmark, or list them
    BMA [addr]                                      Add a bookmark, or list them
    BMC #|*                                         Clear a bookmark, or all of them
    BMG #                                           Disassemble at a bookmark
    BML                                             List the bookmarks
    BMSAVE file                                     Save the bookmarks as a script
    DISASM [setting [0|1]]                          Show or change the disassembly settings
    DW [name [=]] [addr[,len|:last]]                Mark memory as words, or list the data blocks
    DW2 [name [=]] [addr[,len|:last]]               Mark memory as words, 2 a line
    DW4 [name [=]] [addr[,len|:last]]               Mark memory as words, 4 a line
    Z [name [=]] [addr[,len|:last]]                 Mark memory as bytes, or list the data blocks
  Symbols and source
    SRC [ON|OFF]                                    Show the source line at PC, or step by source lines
    SYMASM [name|addr|name=addr|cmd]                Find or add a symbol in the assembler table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMBASIC [name|addr|name=addr|cmd]              Find or add a symbol in the Applesoft table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMDOS33, SYMDOS [name|addr|name=addr|cmd]      Find or add a symbol in the DOS 3.3 table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMINFO                                         Show each symbol table's count and whether it is on
    SYMLIST [table]                                 List a symbol table, the user table by default
    SYMMAIN [name|addr|name=addr|cmd]               Find or add a symbol in the main table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMPRODOS, SYMPRO [name|addr|name=addr|cmd]     Find or add a symbol in the ProDOS table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMSRC [name|addr|name=addr|cmd]                Find or add a symbol in the source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMSRC2 [name|addr|name=addr|cmd]               Find or add a symbol in the second source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMUSER [name|addr|name=addr|cmd]               Find or add a symbol in the user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMUSER2 [name|addr|name=addr|cmd]              Find or add a symbol in the second user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
  Disks
    DISK [INFO|SLOT]                                Show the drives and their disks, or the disk slot
  Display and panels
    PANEL [LIST|[OPEN|CLOSE] name [OPEN|CLOSE]]     List the device panels, or open or close one
    VIDEOINFO                                       Show the video scanner's position
    VIDEOLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]    Log video mode changes with the frame and beam position
  Session and settings
    CALC expr                                       Evaluate an expression
    ECHO text                                       Print text
    LOADSTATE file                                  Replace the whole machine, disks included, with one a state file holds
    LOG [level]                                     Show or set which notifications print
    MODE [mode]                                     Show or set the command mode, which sets the output too
    MOTD                                            Show the message of the day
    OUTPUT [format]                                 Show or set the format replies are written in
    PRINT item[,item ...]                           Print strings, and expressions in hex
    PRINTF "format"[,expr ...]                      Print expressions through a format
    PWD                                             Show the current directory
    RUN file                                        Run a script of commands
    SAVESTATE file                                  Save the whole machine, disks included, to a state file
    STARTUP                                         Run the startup script, DebuggerAutoRun.txt
    VERSION                                         Show Casso's version
```

## WinDbg mode

A bare number is hex; 0x or $ also marks hex, and 0n marks decimal, as in 0n192.

```text
WinDbg commands:
  Running and stepping
    g [addr]                                         Run, stopping at addr if given
    g-                                               Run backward to a breakpoint
    gu                                               Step out
    gu-                                              Step back out
    p [count]                                        Step over
    p-                                               Step back over
    pa addr                                          Run to addr
    t [count]                                        Step into
    t-                                               Step back into
    ta addr                                          Run to addr
  Breakpoints
    ba r1|w1|e1 addr                                 Break on a read, write or execution of addr
    bc id                                            Clear a breakpoint
    bd id                                            Disable a breakpoint
    be id                                            Enable a breakpoint
    bl                                               List the breakpoints
    bp addr                                          Set an execution breakpoint
  Registers and flags
    k                                                Show the call stack
    r [reg[=value]]                                  Show or change the registers
  Memory
    da addr [l n]                                    Show text
    db addr [l n]                                    Show bytes
    dd addr [l n]                                    Show double words
    dw addr [l n]                                    Show words
    ea addr "text"                                   Store text
    eb addr bb bb ...                                Store bytes
    ew addr ww ...                                   Store words
    f addr l n bb ...                                Fill memory with bytes
    m addr l n dest                                  Copy memory to dest
    s addr l n bb ...                                Search memory for bytes
  Disassembly and data
    u [addr]                                         Disassemble
  Symbols and source
    l+s                                              Step by source line
    l-s                                              Step by instruction
    lsa                                              Show the source line at the PC
    x name                                           Look up a symbol
  Session and settings
    .formats expr                                    Show a value in every base
    .help [word]                                     This list, or one command
    ? expr                                           Evaluate an expression

Casso commands:
  Running and stepping
    != addr                                          Set the program counter
    !BUDGET cycles                                   Stop each run after a number of cycles, in decimal; 0 removes the limit
    !CYCLES [ABS|REL|PART]                           Show the cycles in total, in the last run, or since RCC
    !FRAME [count]                                   Run one video frame, or count of them
    !GG [addr [skip[,len|:last]]]                    Run at full speed, stopping as G does
    !HISTORY [ON|OFF|SAVE file|first [n]]            Show the instruction trace, turn it on or off, or save it
    !JSR addr                                        Call a subroutine and run until it returns
    !KEY byte [byte ...]                             Queue key codes for the machine to read
    !LBR                                             Show the last branch taken
    !LIVE                                            Leave the recorded history and return to where the machine was running
    !PAUSE                                           Stop the running machine
    !PROFILE [ON|OFF|RESET|LIST [ADDR]|SAVE [file]]  Profile where execution goes, by routine or by address
    !RCC                                             Reset the cycle counter that CYCLES PART reads
    !SKIP [CLEAR|[-]name|addr[.last]]                Show or change the routines stepping goes over
    !SOUNDLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]    Log speaker toggles and Mockingboard writes with their cycles
    !STOPWATCH [start [stop]|OFF|RESET]              Count the cycles between two addresses in debugger runs
    !TF [file] [V]                                   Turn tracing to a file on or off; V records the video position
  Breakpoints
    !BPA addr[,len|:last]                            Set an execution breakpoint and a memory watchpoint
    !BPBEAM line cycle|VBL                           Stop when the beam reaches a scanline and cycle, or vertical blank
    !BPCHANGE # flags                                Change a breakpoint's flags: E enabled, T temporary, S stops
    !BPEDIT # definition                             Replace a breakpoint with a new definition
    !BPMV addr byte [IF expr]                        Stop when a write leaves addr holding byte
    !BPR reg [op] value                              Stop when a register meets a condition
    !BPSAVE file                                     Save the breakpoints as a script
    !BPV line[,len|:last]                            Stop at a video scanline
    !BRK [0|1|2|3|ALL] [ON|OFF]                      Stop on BRK, or on invalid opcodes of a length
    !BRKINT [ON|OFF]                                 Stop on an interrupt
    !BRKOP [opcode ...]                              Stop on an opcode, or list the opcode breakpoints
    !BRKUNINIT [ON|OFF]                              Stop on a read of RAM nothing has written since power-on
  Registers and flags
    !CL flag                                         Clear a flag: C, Z, I, D, B, R, V or N
    !CLB, !RB                                        Clear the break flag
    !CLC, !RC                                        Clear the carry flag
    !CLD, !RD                                        Clear the decimal flag
    !CLI, !RI                                        Clear the interrupt disable flag
    !CLN, !RN                                        Clear the negative flag
    !CLR, !RR                                        Clear the reserved flag
    !CLV, !RV                                        Clear the overflow flag
    !CLZ, !RZ                                        Clear the zero flag
    !POP                                             Pop a byte off the stack
    !PPOP                                            Pop a word off the stack
    !PUSH byte [byte ...]                            Push bytes onto the stack
    !SE flag                                         Set a flag: C, Z, I, D, B, R, V or N
    !SEB, !SB                                        Set the break flag
    !SEC, !SC                                        Set the carry flag
    !SED, !SD                                        Set the decimal flag
    !SEI, !SI                                        Set the interrupt disable flag
    !SEN, !SN                                        Set the negative flag
    !SER, !SR                                        Set the reserved flag
    !SEV, !SV                                        Set the overflow flag
    !SEZ, !SZ                                        Set the zero flag
    !STACK                                           Show the stack
  Memory
    !@                                               Show the last search's results
    !BLOAD file [addr[,len|:last]]                   Load a file into memory
    !BSAVE file addr[,len|:last]                     Save memory to a file
    !IN, !INPUT addr                                 Read an I/O address
    !MAP                                             Show where each address range reads and writes now
    !MC dest src[,len|:last]                         Compare memory
    !NOP, !ZAP                                       Replace the instruction at PC with NOPs
    !OUT addr byte [byte ...]                        Write to an I/O address
    !PATCH addr value ...                            Store values at addr, ROM included
    !SWITCHES                                        Show the soft switches
    !TSAVE file                                      Save the text screen to a file
    !W [addr]                                        Add a watch, or list them
    !WA [addr]                                       Add a watch, or list them
    !WC #|*                                          Clear a watch, or all of them
    !WD #|*                                          Disable a watch, or all of them
    !WE #|*                                          Enable a watch, or all of them
    !WL                                              List the watches
    !WSAVE file                                      Save the watches as a script
    !ZP [addr]                                       Add a zero-page pointer, or list them
    !ZP0, !P0 [addr]                                 Set zero-page pointer 0, or list them
    !ZP1, !P1 [addr]                                 Set zero-page pointer 1, or list them
    !ZP2, !P2 [addr]                                 Set zero-page pointer 2, or list them
    !ZP3, !P3 [addr]                                 Set zero-page pointer 3, or list them
    !ZP4, !P4 [addr]                                 Set zero-page pointer 4, or list them
    !ZP5 [addr]                                      Set zero-page pointer 5, or list them
    !ZP6 [addr]                                      Set zero-page pointer 6, or list them
    !ZP7 [addr]                                      Set zero-page pointer 7, or list them
    !ZPA [addr]                                      Add a zero-page pointer, or list them
    !ZPC #|*                                         Clear a zero-page pointer, or all of them
    !ZPD #|*                                         Disable a zero-page pointer, or all of them
    !ZPE #|*                                         Enable a zero-page pointer, or all of them
    !ZPL                                             List the zero-page pointers
    !ZPSAVE file                                     Save the zero-page pointers as a script
  Disassembly and data
    !A [addr]                                        Assemble at addr, or at PC; a blank line ends it
    !ASC [name [=]] [addr[,len|:last]]               Mark memory as text, or list the data blocks
    !B                                               List the data blocks
    !BM [addr]                                       Add a bookmark, or list them
    !BMA [addr]                                      Add a bookmark, or list them
    !BMC #|*                                         Clear a bookmark, or all of them
    !BMG #                                           Disassemble at a bookmark
    !BML                                             List the bookmarks
    !BMSAVE file                                     Save the bookmarks as a script
    !DA [name [=]] [addr[,len|:last]]                Mark memory as addresses, or list the data blocks
    !DB, !Z [name [=]] [addr[,len|:last]]            Mark memory as bytes, or list the data blocks
    !DB2 [name [=]] [addr[,len|:last]]               Mark memory as bytes, 2 a line
    !DB4 [name [=]] [addr[,len|:last]]               Mark memory as bytes, 4 a line
    !DB8 [name [=]] [addr[,len|:last]]               Mark memory as bytes, 8 a line
    !DF [name [=]] [addr[,len|:last]]                Mark memory as floating-point numbers
    !DISASM [setting [0|1]]                          Show or change the disassembly settings
    !DW [name [=]] [addr[,len|:last]]                Mark memory as words, or list the data blocks
    !DW2 [name [=]] [addr[,len|:last]]               Mark memory as words, 2 a line
    !DW4 [name [=]] [addr[,len|:last]]               Mark memory as words, 4 a line
    !X addr[,len|:last]                              Make a data block code again
  Symbols and source
    !SYMASM [name|addr|name=addr|cmd]                Find or add a symbol in the assembler table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    !SYMBASIC [name|addr|name=addr|cmd]              Find or add a symbol in the Applesoft table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    !SYMDOS33, !SYMDOS [name|addr|name=addr|cmd]     Find or add a symbol in the DOS 3.3 table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    !SYMINFO                                         Show each symbol table's count and whether it is on
    !SYMLIST [table]                                 List a symbol table, the user table by default
    !SYMMAIN [name|addr|name=addr|cmd]               Find or add a symbol in the main table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    !SYMPRODOS, !SYMPRO [name|addr|name=addr|cmd]    Find or add a symbol in the ProDOS table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    !SYMSRC [name|addr|name=addr|cmd]                Find or add a symbol in the source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    !SYMSRC2 [name|addr|name=addr|cmd]               Find or add a symbol in the second source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    !SYMUSER [name|addr|name=addr|cmd]               Find or add a symbol in the user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    !SYMUSER2 [name|addr|name=addr|cmd]              Find or add a symbol in the second user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
  Disks
    !DISK [INFO|SLOT]                                Show the drives and their disks, or the disk slot
  Display and panels
    !PANEL [LIST|[OPEN|CLOSE] name [OPEN|CLOSE]]     List the device panels, or open or close one
    !VIDEOINFO                                       Show the video scanner's position
    !VIDEOLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]    Log video mode changes with the frame and beam position
  Session and settings
    !CD dir                                          Change the current directory
    !ECHO text                                       Print text
    !LOAD file                                       Run a script of commands, such as one SAVE wrote
    !LOADSTATE file                                  Replace the whole machine, disks included, with one a state file holds
    !LOG [level]                                     Show or set which notifications print
    !MODE [mode]                                     Show or set the command mode, which sets the output too
    !MOTD                                            Show the message of the day
    !OUTPUT [format]                                 Show or set the format replies are written in
    !PRINT item[,item ...]                           Print strings, and expressions in hex
    !PRINTF "format"[,expr ...]                      Print expressions through a format
    !PWD                                             Show the current directory
    !RUN file                                        Run a script of commands
    !SAVE file                                       Save breakpoints, watches, pointers and bookmarks
    !SAVESTATE file                                  Save the whole machine, disks included, to a state file
    !STARTUP                                         Run the startup script, DebuggerAutoRun.txt
    !VERSION                                         Show Casso's version
```

## Casso mode

A bare number is hex; $ also marks hex, and # or 0n marks decimal, as in 0n192.

```text
Casso commands:
  Running and stepping
    = addr                                               Set the program counter
    BUDGET cycles                                        Stop each run after a number of cycles, in decimal; 0 removes the limit
    CYCLES [ABS|REL|PART]                                Show the cycles in total, in the last run, or since RCC
    FRAME [count]                                        Run one video frame, or count of them
    G [addr [skip[,len|:last]]]                          Run, stopping at addr or when PC leaves the skip range
    G-                                                   Run backward through the recorded history to the latest breakpoint or watchpoint
    GG [addr [skip[,len|:last]]]                         Run at full speed, stopping as G does
    GU-                                                  Step back out to the JSR that called the current subroutine
    HISTORY [ON|OFF|SAVE file|first [n]]                 Show the instruction trace, turn it on or off, or save it
    JSR addr                                             Call a subroutine and run until it returns
    KEY byte [byte ...]                                  Queue key codes for the machine to read
    LBR                                                  Show the last branch taken
    LIVE                                                 Leave the recorded history and return to where the machine was running
    P [count]                                            Step over
    P-                                                   Step back over, taking a whole subroutine call as one step
    PAUSE                                                Stop the running machine
    PROFILE [ON|OFF|RESET|LIST [ADDR]|SAVE [file]]       Profile where execution goes, by routine or by address
    RCC                                                  Reset the cycle counter that CYCLES PART reads
    RTS [count]                                          Step out of the current subroutine
    SKIP [CLEAR|[-]name|addr[.last]]                     Show or change the routines stepping goes over
    SOUNDLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]         Log speaker toggles and Mockingboard writes with their cycles
    STOPWATCH [start [stop]|OFF|RESET]                   Count the cycles between two addresses in debugger runs
    T, TL, TRACE [count]                                 Step into
    T-                                                   Step back one instruction
    TF [file] [V]                                        Turn tracing to a file on or off; V records the video position
  Breakpoints
    BP addr[,len|:last]|file:line [IF expr]              Set an execution breakpoint, or one on PC with < > = !
    BPA addr[,len|:last]                                 Set an execution breakpoint and a memory watchpoint
    BPBEAM line cycle|VBL                                Stop when the beam reaches a scanline and cycle, or vertical blank
    BPC #|*                                              Clear a breakpoint, or all of them
    BPCHANGE # flags                                     Change a breakpoint's flags: E enabled, T temporary, S stops
    BPD #|*                                              Disable a breakpoint, or all of them
    BPE #|*                                              Enable a breakpoint, or all of them
    BPEDIT # definition                                  Replace a breakpoint with a new definition
    BPL                                                  List the breakpoints
    BPM, BPIO addr[,len|:last] [BEFORE|AFTER] [IF expr]  Stop on a read or write of memory
    BPMR addr[,len|:last] [BEFORE|AFTER] [IF expr]       Stop on a read of memory
    BPMV addr byte [IF expr]                             Stop when a write leaves addr holding byte
    BPMW addr[,len|:last] [BEFORE|AFTER] [IF expr]       Stop on a write to memory
    BPR reg [op] value                                   Stop when a register meets a condition
    BPSAVE file                                          Save the breakpoints as a script
    BPV line[,len|:last]                                 Stop at a video scanline
    BPX addr[,len|:last]|file:line [IF expr]             Set an execution breakpoint, as BP does
    BRK [0|1|2|3|ALL] [ON|OFF]                           Stop on BRK, or on invalid opcodes of a length
    BRKINT [ON|OFF]                                      Stop on an interrupt
    BRKOP [opcode ...]                                   Stop on an opcode, or list the opcode breakpoints
    BRKUNINIT [ON|OFF]                                   Stop on a read of RAM nothing has written since power-on
  Registers and flags
    CALLS [MODE [RECORDED|WALK|HYBRID]]                  Show the call stack, or choose how it is found
    CL flag                                              Clear a flag: C, Z, I, D, B, R, V or N
    CLB, RB                                              Clear the break flag
    CLC, RC                                              Clear the carry flag
    CLD, RD                                              Clear the decimal flag
    CLI, RI                                              Clear the interrupt disable flag
    CLN, RN                                              Clear the negative flag
    CLR, RR                                              Clear the reserved flag
    CLV, RV                                              Clear the overflow flag
    CLZ, RZ                                              Clear the zero flag
    POP                                                  Pop a byte off the stack
    PPOP                                                 Pop a word off the stack
    PUSH byte [byte ...]                                 Push bytes onto the stack
    R, REGISTER [reg [=] value]                          Show the registers, or set one
    SE flag                                              Set a flag: C, Z, I, D, B, R, V or N
    SEB, SB                                              Set the break flag
    SEC, SC                                              Set the carry flag
    SED, SD                                              Set the decimal flag
    SEI, SI                                              Set the interrupt disable flag
    SEN, SN                                              Set the negative flag
    SER, SR                                              Set the reserved flag
    SEV, SV                                              Set the overflow flag
    SEZ, SZ                                              Set the zero flag
    STACK                                                Show the stack
  Memory
    @                                                    Show the last search's results
    BLOAD file [addr[,len|:last]]                        Load a file into memory
    BSAVE file addr[,len|:last]                          Save memory to a file
    D, MDB [addr[,len|:last]]                            Show memory
    F addr[,len|:last] byte ...                          Fill memory with bytes; F first last byte ... also works
    IN, INPUT addr                                       Read an I/O address
    M, MM dest src[,len|:last]                           Copy memory
    MAP                                                  Show where each address range reads and writes now
    MC dest src[,len|:last]                              Compare memory
    ME addr value ...                                    Store values at addr, a value over $FF as a word
    MEB, ME8 addr value ...                              Store values at addr, a value over $FF as a word
    MEW, ME16 addr word ...                              Store words at addr, low byte first
    NOP, ZAP                                             Replace the instruction at PC with NOPs
    OUT addr byte [byte ...]                             Write to an I/O address
    PATCH addr value ...                                 Store values at addr, ROM included
    S, MS addr[,len|:last] item ...                      Search memory for bytes, "text", 'text' or wildcards
    SH addr[,len|:last] item ...                         Search memory, as S does
    SWITCHES                                             Show the soft switches
    TSAVE file                                           Save the text screen to a file
    W [addr]                                             Add a watch, or list them
    WA [addr]                                            Add a watch, or list them
    WC #|*                                               Clear a watch, or all of them
    WD #|*                                               Disable a watch, or all of them
    WE #|*                                               Enable a watch, or all of them
    WL                                                   List the watches
    WSAVE file                                           Save the watches as a script
    ZP [addr]                                            Add a zero-page pointer, or list them
    ZP0, P0 [addr]                                       Set zero-page pointer 0, or list them
    ZP1, P1 [addr]                                       Set zero-page pointer 1, or list them
    ZP2, P2 [addr]                                       Set zero-page pointer 2, or list them
    ZP3, P3 [addr]                                       Set zero-page pointer 3, or list them
    ZP4, P4 [addr]                                       Set zero-page pointer 4, or list them
    ZP5 [addr]                                           Set zero-page pointer 5, or list them
    ZP6 [addr]                                           Set zero-page pointer 6, or list them
    ZP7 [addr]                                           Set zero-page pointer 7, or list them
    ZPA [addr]                                           Add a zero-page pointer, or list them
    ZPC #|*                                              Clear a zero-page pointer, or all of them
    ZPD #|*                                              Disable a zero-page pointer, or all of them
    ZPE #|*                                              Enable a zero-page pointer, or all of them
    ZPL                                                  List the zero-page pointers
    ZPSAVE file                                          Save the zero-page pointers as a script
  Disassembly and data
    A [addr]                                             Assemble at addr, or at PC; a blank line ends it
    ASC [name [=]] [addr[,len|:last]]                    Mark memory as text, or list the data blocks
    B                                                    List the data blocks
    BM [addr]                                            Add a bookmark, or list them
    BMA [addr]                                           Add a bookmark, or list them
    BMC #|*                                              Clear a bookmark, or all of them
    BMG #                                                Disassemble at a bookmark
    BML                                                  List the bookmarks
    BMSAVE file                                          Save the bookmarks as a script
    DA [name [=]] [addr[,len|:last]]                     Mark memory as addresses, or list the data blocks
    DB, Z [name [=]] [addr[,len|:last]]                  Mark memory as bytes, or list the data blocks
    DB2 [name [=]] [addr[,len|:last]]                    Mark memory as bytes, 2 a line
    DB4 [name [=]] [addr[,len|:last]]                    Mark memory as bytes, 4 a line
    DB8 [name [=]] [addr[,len|:last]]                    Mark memory as bytes, 8 a line
    DF [name [=]] [addr[,len|:last]]                     Mark memory as floating-point numbers
    DISASM [setting [0|1]]                               Show or change the disassembly settings
    DW [name [=]] [addr[,len|:last]]                     Mark memory as words, or list the data blocks
    DW2 [name [=]] [addr[,len|:last]]                    Mark memory as words, 2 a line
    DW4 [name [=]] [addr[,len|:last]]                    Mark memory as words, 4 a line
    U [addr[,len|:last]]                                 Disassemble, or continue the last listing
    X addr[,len|:last]                                   Make a data block code again
  Symbols and source
    SRC [ON|OFF]                                         Show the source line at PC, or step by source lines
    SYM [name|addr|name=addr|cmd]                        Find or add a symbol in any table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMASM [name|addr|name=addr|cmd]                     Find or add a symbol in the assembler table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMBASIC [name|addr|name=addr|cmd]                   Find or add a symbol in the Applesoft table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMDOS33, SYMDOS [name|addr|name=addr|cmd]           Find or add a symbol in the DOS 3.3 table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMINFO                                              Show each symbol table's count and whether it is on
    SYMLIST [table]                                      List a symbol table, the user table by default
    SYMMAIN [name|addr|name=addr|cmd]                    Find or add a symbol in the main table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMPRODOS, SYMPRO [name|addr|name=addr|cmd]          Find or add a symbol in the ProDOS table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMSRC [name|addr|name=addr|cmd]                     Find or add a symbol in the source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMSRC2 [name|addr|name=addr|cmd]                    Find or add a symbol in the second source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMUSER [name|addr|name=addr|cmd]                    Find or add a symbol in the user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
    SYMUSER2 [name|addr|name=addr|cmd]                   Find or add a symbol in the second user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !
  Disks
    DISK [INFO|SLOT]                                     Show the drives and their disks, or the disk slot
  Display and panels
    ->                                                   Move the code pane to the operand's address
    .                                                    Make the code pane follow PC again
    ^                                                    Move the code pane up one instruction
    CODE                                                 Bring the disassembly forward
    CODE1                                                Bring the first disassembly forward
    CODE2                                                Bring the second disassembly forward, opening it
    CONSOLE                                              Bring the console forward
    DATA                                                 Bring the memory window forward
    DATA1                                                Bring the first memory window forward
    DATA2                                                Bring the second memory window forward, opening it
    M1 addr                                              Move the memory pane to a hex address
    M2 addr                                              Move the memory pane to a hex address
    MA1 addr                                             Move the memory pane to a hex address
    MA2 addr                                             Move the memory pane to a hex address
    MD1 addr                                             Move the memory pane to a hex address
    MD2 addr                                             Move the memory pane to a hex address
    MT1 addr                                             Move the memory pane to a hex address
    MT2 addr                                             Move the memory pane to a hex address
    PAGEDN                                               Move the code pane down a page
    PAGEDOWN256                                          Move the code pane ahead $100 bytes
    PAGEDOWN4K                                           Move the code pane ahead $1000 bytes
    PAGEUP                                               Move the code pane up a page
    PAGEUP256                                            Move the code pane back $100 bytes
    PAGEUP4K                                             Move the code pane back $1000 bytes
    PANEL [LIST|[OPEN|CLOSE] name [OPEN|CLOSE]]          List the device panels, or open or close one
    RET                                                  Move the code pane to the return address
    V                                                    Move the code pane down one instruction
    VIDEOINFO                                            Show the video scanner's position
    VIDEOLOG [ON|OFF|CLEAR|LIST [n]|SAVE [file]]         Log video mode changes with the frame and beam position
  Session and settings
    ? [command]                                          List the commands, or describe one
    CALC expr                                            Evaluate an expression
    CD dir                                               Change the current directory
    ECHO text                                            Print text
    HELP [all|section|command|text]                      List the help sections, one section, every command, one command or the matches for text
    LOAD file                                            Run a script of commands, such as one SAVE wrote
    LOADSTATE file                                       Replace the whole machine, disks included, with one a state file holds
    LOG [level]                                          Show or set which notifications print
    MODE [mode]                                          Show or set the command mode, which sets the output too
    MOTD                                                 Show the message of the day
    OUTPUT [format]                                      Show or set the format replies are written in
    PRINT item[,item ...]                                Print strings, and expressions in hex
    PRINTF "format"[,expr ...]                           Print expressions through a format
    PWD                                                  Show the current directory
    RUN file                                             Run a script of commands
    SAVE file                                            Save breakpoints, watches, pointers and bookmarks
    SAVESTATE file                                       Save the whole machine, disks included, to a state file
    STARTUP                                              Run the startup script, DebuggerAutoRun.txt
    VERSION                                              Show Casso's version
```
