#include "Pch.h"

#include "Debugger/CassoCommandReference.h"

#include "Debugger/AppleWinCommandTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kEntries
//
//  One row per command the AppleWin command table runs, by category and then
//  by name. In a syntax, [x] is optional, a|b is one or the other, and a
//  range is addr, addr,len or addr:last.
//
////////////////////////////////////////////////////////////////////////////////

using H = HelpCategory;

static constexpr CassoCommandReference::Entry  s_kEntries[] =
{
    { "=",           H::RunningAndStepping, "= addr",                                         "Set the program counter"                                                                 },
    { "BUDGET",      H::RunningAndStepping, "BUDGET cycles",                                  "Stop each run after a number of cycles, in decimal; 0 removes the limit"                 },
    { "CYCLES",      H::RunningAndStepping, "CYCLES [ABS|REL|PART]",                          "Show the cycles in total, in the last run, or since RCC"                                 },
    { "G",           H::RunningAndStepping, "G [addr [skip[,len|:last]]]",                    "Run, stopping at addr or when PC leaves the skip range"                                  },
    { "GG",          H::RunningAndStepping, "GG [addr [skip[,len|:last]]]",                   "Run at full speed, stopping as G does"                                                   },
    { "HISTORY",     H::RunningAndStepping, "HISTORY [ON|OFF|SAVE file|first [n]]",           "Show the instruction trace, turn it on or off, or save it"                               },
    { "JSR",         H::RunningAndStepping, "JSR addr",                                       "Call a subroutine and run until it returns"                                              },
    { "KEY",         H::RunningAndStepping, "KEY byte [byte ...]",                            "Queue key codes for the machine to read"                                                 },
    { "LBR",         H::RunningAndStepping, "LBR",                                            "Show the last branch taken"                                                              },
    { "P",           H::RunningAndStepping, "P [count]",                                      "Step over"                                                                               },
    { "PAUSE",       H::RunningAndStepping, "PAUSE",                                          "Stop the running machine"                                                                },
    { "PROFILE",     H::RunningAndStepping, "PROFILE [ON|OFF|RESET|LIST [ADDR]|SAVE [file]]", "Profile where execution goes, by routine or by address"                                  },
    { "RCC",         H::RunningAndStepping, "RCC",                                            "Reset the cycle counter that CYCLES PART reads"                                          },
    { "RTS",         H::RunningAndStepping, "RTS [count]",                                    "Step out of the current subroutine"                                                      },
    { "SKIP",        H::RunningAndStepping, "SKIP [CLEAR|[-]name|addr[.last]]",               "Show or change the routines stepping goes over"                                          },
    { "T",           H::RunningAndStepping, "T [count]",                                      "Step into"                                                                               },
    { "TF",          H::RunningAndStepping, "TF [file] [V]",                                  "Turn tracing to a file on or off; V records the video position"                          },

    { "BP",          H::Breakpoints,        "BP addr[,len|:last]|file:line [IF expr]",        "Set an execution breakpoint, or one on PC with < > = !"                                  },
    { "BPA",         H::Breakpoints,        "BPA addr[,len|:last]",                           "Set an execution breakpoint and a memory watchpoint"                                     },
    { "BPC",         H::Breakpoints,        "BPC #|*",                                        "Clear a breakpoint, or all of them"                                                      },
    { "BPCHANGE",    H::Breakpoints,        "BPCHANGE # flags",                               "Change a breakpoint's flags: E enabled, T temporary, S stops"                            },
    { "BPD",         H::Breakpoints,        "BPD #|*",                                        "Disable a breakpoint, or all of them"                                                    },
    { "BPE",         H::Breakpoints,        "BPE #|*",                                        "Enable a breakpoint, or all of them"                                                     },
    { "BPEDIT",      H::Breakpoints,        "BPEDIT # definition",                            "Replace a breakpoint with a new definition"                                              },
    { "BPL",         H::Breakpoints,        "BPL",                                            "List the breakpoints"                                                                    },
    { "BPM",         H::Breakpoints,        "BPM addr[,len|:last] [BEFORE|AFTER] [IF expr]",  "Stop on a read or write of memory"                                                       },
    { "BPMR",        H::Breakpoints,        "BPMR addr[,len|:last] [BEFORE|AFTER] [IF expr]", "Stop on a read of memory"                                                                },
    { "BPMV",        H::Breakpoints,        "BPMV addr byte [IF expr]",                       "Stop when a write leaves addr holding byte"                                              },
    { "BPMW",        H::Breakpoints,        "BPMW addr[,len|:last] [BEFORE|AFTER] [IF expr]", "Stop on a write to memory"                                                               },
    { "BPR",         H::Breakpoints,        "BPR reg [op] value",                             "Stop when a register meets a condition"                                                  },
    { "BPSAVE",      H::Breakpoints,        "BPSAVE file",                                    "Save the breakpoints as a script"                                                        },
    { "BPV",         H::Breakpoints,        "BPV line[,len|:last]",                           "Stop at a video scanline"                                                                },
    { "BPX",         H::Breakpoints,        "BPX addr[,len|:last]|file:line [IF expr]",       "Set an execution breakpoint, as BP does"                                                 },
    { "BRK",         H::Breakpoints,        "BRK [0|1|2|3|ALL] [ON|OFF]",                     "Stop on BRK, or on invalid opcodes of a length"                                          },
    { "BRKINT",      H::Breakpoints,        "BRKINT [ON|OFF]",                                "Stop on an interrupt"                                                                    },
    { "BRKOP",       H::Breakpoints,        "BRKOP [opcode ...]",                             "Stop on an opcode, or list the opcode breakpoints"                                       },

    { "CALLS",       H::RegistersAndFlags,  "CALLS [MODE [RECORDED|WALK|HYBRID]]",            "Show the call stack, or choose how it is found"                                          },
    { "CL",          H::RegistersAndFlags,  "CL flag",                                        "Clear a flag: C, Z, I, D, B, R, V or N"                                                  },
    { "CLB",         H::RegistersAndFlags,  "CLB",                                            "Clear the break flag"                                                                    },
    { "CLC",         H::RegistersAndFlags,  "CLC",                                            "Clear the carry flag"                                                                    },
    { "CLD",         H::RegistersAndFlags,  "CLD",                                            "Clear the decimal flag"                                                                  },
    { "CLI",         H::RegistersAndFlags,  "CLI",                                            "Clear the interrupt disable flag"                                                        },
    { "CLN",         H::RegistersAndFlags,  "CLN",                                            "Clear the negative flag"                                                                 },
    { "CLR",         H::RegistersAndFlags,  "CLR",                                            "Clear the reserved flag"                                                                 },
    { "CLV",         H::RegistersAndFlags,  "CLV",                                            "Clear the overflow flag"                                                                 },
    { "CLZ",         H::RegistersAndFlags,  "CLZ",                                            "Clear the zero flag"                                                                     },
    { "POP",         H::RegistersAndFlags,  "POP",                                            "Pop a byte off the stack"                                                                },
    { "PPOP",        H::RegistersAndFlags,  "PPOP",                                           "Pop a word off the stack"                                                                },
    { "PUSH",        H::RegistersAndFlags,  "PUSH byte [byte ...]",                           "Push bytes onto the stack"                                                               },
    { "R",           H::RegistersAndFlags,  "R [reg [=] value]",                              "Show the registers, or set one"                                                          },
    { "SE",          H::RegistersAndFlags,  "SE flag",                                        "Set a flag: C, Z, I, D, B, R, V or N"                                                    },
    { "SEB",         H::RegistersAndFlags,  "SEB",                                            "Set the break flag"                                                                      },
    { "SEC",         H::RegistersAndFlags,  "SEC",                                            "Set the carry flag"                                                                      },
    { "SED",         H::RegistersAndFlags,  "SED",                                            "Set the decimal flag"                                                                    },
    { "SEI",         H::RegistersAndFlags,  "SEI",                                            "Set the interrupt disable flag"                                                          },
    { "SEN",         H::RegistersAndFlags,  "SEN",                                            "Set the negative flag"                                                                   },
    { "SER",         H::RegistersAndFlags,  "SER",                                            "Set the reserved flag"                                                                   },
    { "SEV",         H::RegistersAndFlags,  "SEV",                                            "Set the overflow flag"                                                                   },
    { "SEZ",         H::RegistersAndFlags,  "SEZ",                                            "Set the zero flag"                                                                       },
    { "STACK",       H::RegistersAndFlags,  "STACK",                                          "Show the stack"                                                                          },

    { "@",           H::Memory,             "@",                                              "Show the last search's results"                                                          },
    { "BLOAD",       H::Memory,             "BLOAD file [addr[,len|:last]]",                  "Load a file into memory"                                                                 },
    { "BSAVE",       H::Memory,             "BSAVE file addr[,len|:last]",                    "Save memory to a file"                                                                   },
    { "D",           H::Memory,             "D [addr[,len|:last]]",                           "Show memory"                                                                             },
    { "F",           H::Memory,             "F addr[,len|:last] byte ...",                    "Fill memory with bytes; F first last byte ... also works"                                },
    { "IN",          H::Memory,             "IN addr",                                        "Read an I/O address"                                                                     },
    { "M",           H::Memory,             "M dest src[,len|:last]",                         "Copy memory"                                                                             },
    { "MC",          H::Memory,             "MC dest src[,len|:last]",                        "Compare memory"                                                                          },
    { "ME",          H::Memory,             "ME addr value ...",                              "Store values at addr, a value over $FF as a word"                                        },
    { "MEB",         H::Memory,             "MEB addr value ...",                             "Store values at addr, a value over $FF as a word"                                        },
    { "MEW",         H::Memory,             "MEW addr word ...",                              "Store words at addr, low byte first"                                                     },
    { "NOP",         H::Memory,             "NOP",                                            "Replace the instruction at PC with NOPs"                                                 },
    { "OUT",         H::Memory,             "OUT addr byte [byte ...]",                       "Write to an I/O address"                                                                 },
    { "PATCH",       H::Memory,             "PATCH addr value ...",                           "Store values at addr, ROM included"                                                      },
    { "S",           H::Memory,             "S addr[,len|:last] item ...",                    "Search memory for bytes, \"text\", 'text' or wildcards"                                  },
    { "SH",          H::Memory,             "SH addr[,len|:last] item ...",                   "Search memory, as S does"                                                                },
    { "SWITCHES",    H::Memory,             "SWITCHES",                                       "Show the soft switches"                                                                  },
    { "TSAVE",       H::Memory,             "TSAVE file",                                     "Save the text screen to a file"                                                          },
    { "W",           H::Memory,             "W [addr]",                                       "Add a watch, or list them"                                                               },
    { "WA",          H::Memory,             "WA [addr]",                                      "Add a watch, or list them"                                                               },
    { "WC",          H::Memory,             "WC #|*",                                         "Clear a watch, or all of them"                                                           },
    { "WD",          H::Memory,             "WD #|*",                                         "Disable a watch, or all of them"                                                         },
    { "WE",          H::Memory,             "WE #|*",                                         "Enable a watch, or all of them"                                                          },
    { "WL",          H::Memory,             "WL",                                             "List the watches"                                                                        },
    { "WSAVE",       H::Memory,             "WSAVE file",                                     "Save the watches as a script"                                                            },
    { "ZP",          H::Memory,             "ZP [addr]",                                      "Add a zero-page pointer, or list them"                                                   },
    { "ZP0",         H::Memory,             "ZP0 [addr]",                                     "Set zero-page pointer 0, or list them"                                                   },
    { "ZP1",         H::Memory,             "ZP1 [addr]",                                     "Set zero-page pointer 1, or list them"                                                   },
    { "ZP2",         H::Memory,             "ZP2 [addr]",                                     "Set zero-page pointer 2, or list them"                                                   },
    { "ZP3",         H::Memory,             "ZP3 [addr]",                                     "Set zero-page pointer 3, or list them"                                                   },
    { "ZP4",         H::Memory,             "ZP4 [addr]",                                     "Set zero-page pointer 4, or list them"                                                   },
    { "ZP5",         H::Memory,             "ZP5 [addr]",                                     "Set zero-page pointer 5, or list them"                                                   },
    { "ZP6",         H::Memory,             "ZP6 [addr]",                                     "Set zero-page pointer 6, or list them"                                                   },
    { "ZP7",         H::Memory,             "ZP7 [addr]",                                     "Set zero-page pointer 7, or list them"                                                   },
    { "ZPA",         H::Memory,             "ZPA [addr]",                                     "Add a zero-page pointer, or list them"                                                   },
    { "ZPC",         H::Memory,             "ZPC #|*",                                        "Clear a zero-page pointer, or all of them"                                               },
    { "ZPD",         H::Memory,             "ZPD #|*",                                        "Disable a zero-page pointer, or all of them"                                             },
    { "ZPE",         H::Memory,             "ZPE #|*",                                        "Enable a zero-page pointer, or all of them"                                              },
    { "ZPL",         H::Memory,             "ZPL",                                            "List the zero-page pointers"                                                             },
    { "ZPSAVE",      H::Memory,             "ZPSAVE file",                                    "Save the zero-page pointers as a script"                                                 },

    { "A",           H::DisassemblyAndData, "A [addr]",                                       "Assemble at addr, or at PC; a blank line ends it"                                        },
    { "ASC",         H::DisassemblyAndData, "ASC [name [=]] [addr[,len|:last]]",              "Mark memory as text, or list the data blocks"                                            },
    { "B",           H::DisassemblyAndData, "B",                                              "List the data blocks"                                                                    },
    { "BM",          H::DisassemblyAndData, "BM [addr]",                                      "Add a bookmark, or list them"                                                            },
    { "BMA",         H::DisassemblyAndData, "BMA [addr]",                                     "Add a bookmark, or list them"                                                            },
    { "BMC",         H::DisassemblyAndData, "BMC #|*",                                        "Clear a bookmark, or all of them"                                                        },
    { "BMG",         H::DisassemblyAndData, "BMG #",                                          "Disassemble at a bookmark"                                                               },
    { "BML",         H::DisassemblyAndData, "BML",                                            "List the bookmarks"                                                                      },
    { "BMSAVE",      H::DisassemblyAndData, "BMSAVE file",                                    "Save the bookmarks as a script"                                                          },
    { "DA",          H::DisassemblyAndData, "DA [name [=]] [addr[,len|:last]]",               "Mark memory as addresses, or list the data blocks"                                       },
    { "DB",          H::DisassemblyAndData, "DB [name [=]] [addr[,len|:last]]",               "Mark memory as bytes, or list the data blocks"                                           },
    { "DB2",         H::DisassemblyAndData, "DB2 [name [=]] [addr[,len|:last]]",              "Mark memory as bytes, 2 a line"                                                          },
    { "DB4",         H::DisassemblyAndData, "DB4 [name [=]] [addr[,len|:last]]",              "Mark memory as bytes, 4 a line"                                                          },
    { "DB8",         H::DisassemblyAndData, "DB8 [name [=]] [addr[,len|:last]]",              "Mark memory as bytes, 8 a line"                                                          },
    { "DF",          H::DisassemblyAndData, "DF [name [=]] [addr[,len|:last]]",               "Mark memory as floating-point numbers"                                                   },
    { "DISASM",      H::DisassemblyAndData, "DISASM [setting [0|1]]",                         "Show or change the disassembly settings"                                                 },
    { "DW",          H::DisassemblyAndData, "DW [name [=]] [addr[,len|:last]]",               "Mark memory as words, or list the data blocks"                                           },
    { "DW2",         H::DisassemblyAndData, "DW2 [name [=]] [addr[,len|:last]]",              "Mark memory as words, 2 a line"                                                          },
    { "DW4",         H::DisassemblyAndData, "DW4 [name [=]] [addr[,len|:last]]",              "Mark memory as words, 4 a line"                                                          },
    { "U",           H::DisassemblyAndData, "U [addr[,len|:last]]",                           "Disassemble, or continue the last listing"                                               },
    { "X",           H::DisassemblyAndData, "X addr[,len|:last]",                             "Make a data block code again"                                                            },

    { "SRC",         H::SymbolsAndSource,   "SRC [ON|OFF]",                                   "Show the source line at PC, or step by source lines"                                     },
    { "SYM",         H::SymbolsAndSource,   "SYM [name|addr|name=addr|cmd]",                  "Find or add a symbol in any table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !"               },
    { "SYMASM",      H::SymbolsAndSource,   "SYMASM [name|addr|name=addr|cmd]",               "Find or add a symbol in the assembler table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !"     },
    { "SYMBASIC",    H::SymbolsAndSource,   "SYMBASIC [name|addr|name=addr|cmd]",             "Find or add a symbol in the Applesoft table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !"     },
    { "SYMDOS33",    H::SymbolsAndSource,   "SYMDOS33 [name|addr|name=addr|cmd]",             "Find or add a symbol in the DOS 3.3 table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !"       },
    { "SYMINFO",     H::SymbolsAndSource,   "SYMINFO",                                        "Show each symbol table's count and whether it is on"                                     },
    { "SYMLIST",     H::SymbolsAndSource,   "SYMLIST [table]",                                "List a symbol table, the user table by default"                                          },
    { "SYMMAIN",     H::SymbolsAndSource,   "SYMMAIN [name|addr|name=addr|cmd]",              "Find or add a symbol in the main table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !"          },
    { "SYMPRODOS",   H::SymbolsAndSource,   "SYMPRODOS [name|addr|name=addr|cmd]",            "Find or add a symbol in the ProDOS table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !"        },
    { "SYMSRC",      H::SymbolsAndSource,   "SYMSRC [name|addr|name=addr|cmd]",               "Find or add a symbol in the source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !"        },
    { "SYMSRC2",     H::SymbolsAndSource,   "SYMSRC2 [name|addr|name=addr|cmd]",              "Find or add a symbol in the second source table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !" },
    { "SYMUSER",     H::SymbolsAndSource,   "SYMUSER [name|addr|name=addr|cmd]",              "Find or add a symbol in the user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !"          },
    { "SYMUSER2",    H::SymbolsAndSource,   "SYMUSER2 [name|addr|name=addr|cmd]",             "Find or add a symbol in the second user table; cmd is LOAD, SAVE, CLEAR, ON, OFF or !"   },

    { "DISK",        H::Disks,              "DISK [INFO|SLOT]",                               "Show the drives and their disks, or the disk slot"                                       },

    { "->",          H::DisplayAndPanels,   "->",                                             "Move the code pane to the operand's address"                                             },
    { ".",           H::DisplayAndPanels,   ".",                                              "Make the code pane follow PC again"                                                      },
    { "M1",          H::DisplayAndPanels,   "M1 addr",                                        "Move the memory pane to a hex address"                                                   },
    { "M2",          H::DisplayAndPanels,   "M2 addr",                                        "Move the memory pane to a hex address"                                                   },
    { "MA1",         H::DisplayAndPanels,   "MA1 addr",                                       "Move the memory pane to a hex address"                                                   },
    { "MA2",         H::DisplayAndPanels,   "MA2 addr",                                       "Move the memory pane to a hex address"                                                   },
    { "MD1",         H::DisplayAndPanels,   "MD1 addr",                                       "Move the memory pane to a hex address"                                                   },
    { "MD2",         H::DisplayAndPanels,   "MD2 addr",                                       "Move the memory pane to a hex address"                                                   },
    { "MT1",         H::DisplayAndPanels,   "MT1 addr",                                       "Move the memory pane to a hex address"                                                   },
    { "MT2",         H::DisplayAndPanels,   "MT2 addr",                                       "Move the memory pane to a hex address"                                                   },
    { "PAGEDN",      H::DisplayAndPanels,   "PAGEDN",                                         "Move the code pane down a page"                                                          },
    { "PAGEDOWN256", H::DisplayAndPanels,   "PAGEDOWN256",                                    "Move the code pane ahead $100 bytes"                                                     },
    { "PAGEDOWN4K",  H::DisplayAndPanels,   "PAGEDOWN4K",                                     "Move the code pane ahead $1000 bytes"                                                    },
    { "PAGEUP",      H::DisplayAndPanels,   "PAGEUP",                                         "Move the code pane up a page"                                                            },
    { "PAGEUP256",   H::DisplayAndPanels,   "PAGEUP256",                                      "Move the code pane back $100 bytes"                                                      },
    { "PAGEUP4K",    H::DisplayAndPanels,   "PAGEUP4K",                                       "Move the code pane back $1000 bytes"                                                     },
    { "PANEL",       H::DisplayAndPanels,   "PANEL [LIST|name|CLOSE name]",                   "List the device panels, or open or close one"                                            },
    { "RET",         H::DisplayAndPanels,   "RET",                                            "Move the code pane to the return address"                                                },
    { "V",           H::DisplayAndPanels,   "V",                                              "Move the code pane down one instruction"                                                 },
    { "VIDEOINFO",   H::DisplayAndPanels,   "VIDEOINFO",                                      "Show the video scanner's position"                                                       },
    { "^",           H::DisplayAndPanels,   "^",                                              "Move the code pane up one instruction"                                                   },

    { "?",           H::SessionAndSettings, "? [command]",                                    "List the commands, or describe one"                                                      },
    { "CALC",        H::SessionAndSettings, "CALC expr",                                      "Evaluate an expression"                                                                  },
    { "CD",          H::SessionAndSettings, "CD dir",                                         "Change the current directory"                                                            },
    { "ECHO",        H::SessionAndSettings, "ECHO text",                                      "Print text"                                                                              },
    { "HELP",        H::SessionAndSettings, "HELP [command]",                                 "List the commands, or describe one"                                                      },
    { "LOAD",        H::SessionAndSettings, "LOAD file",                                      "Run a script of commands, such as one SAVE wrote"                                        },
    { "LOG",         H::SessionAndSettings, "LOG [level]",                                    "Show or set which notifications print"                                                   },
    { "MODE",        H::SessionAndSettings, "MODE [mode]",                                    "Show or set the command mode, which sets the output too"                                 },
    { "MOTD",        H::SessionAndSettings, "MOTD",                                           "Show the message of the day"                                                             },
    { "OUTPUT",      H::SessionAndSettings, "OUTPUT [format]",                                "Show or set the format replies are written in"                                           },
    { "PRINT",       H::SessionAndSettings, "PRINT item[,item ...]",                          "Print strings, and expressions in hex"                                                   },
    { "PRINTF",      H::SessionAndSettings, "PRINTF \"format\"[,expr ...]",                   "Print expressions through a format"                                                      },
    { "PWD",         H::SessionAndSettings, "PWD",                                            "Show the current directory"                                                              },
    { "RUN",         H::SessionAndSettings, "RUN file",                                       "Run a script of commands"                                                                },
    { "SAVE",        H::SessionAndSettings, "SAVE file",                                      "Save breakpoints, watches, pointers and bookmarks"                                       },
    { "STARTUP",     H::SessionAndSettings, "STARTUP",                                        "Run the startup script, DebuggerAutoRun.txt"                                             },
    { "VERSION",     H::SessionAndSettings, "VERSION",                                        "Show Casso's version"                                                                    },
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoCommandReference::GetAll
//
////////////////////////////////////////////////////////////////////////////////

std::span<const CassoCommandReference::Entry> CassoCommandReference::GetAll()
{
    return s_kEntries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoCommandReference::Find
//
//  An alias is looked up as the command it stands for, since only that
//  command has an entry.
//
////////////////////////////////////////////////////////////////////////////////

const CassoCommandReference::Entry * CassoCommandReference::Find (const std::string & name)
{
    const AppleWinCommand  * command = AppleWinCommandTable::Find (name);
    const char             * target  = name.c_str();



    if (command != nullptr && command->aliasOf != nullptr)
    {
        target = command->aliasOf;
    }

    for (const Entry & entry : s_kEntries)
    {
        if (_stricmp (entry.name, target) == 0)
        {
            return &entry;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoCommandReference::GetCategoryTitle
//
////////////////////////////////////////////////////////////////////////////////

const char * CassoCommandReference::GetCategoryTitle (HelpCategory category)
{
    switch (category)
    {
    case HelpCategory::RunningAndStepping: return "Running and stepping";
    case HelpCategory::Breakpoints:        return "Breakpoints";
    case HelpCategory::RegistersAndFlags:  return "Registers and flags";
    case HelpCategory::Memory:             return "Memory";
    case HelpCategory::DisassemblyAndData: return "Disassembly and data";
    case HelpCategory::SymbolsAndSource:   return "Symbols and source";
    case HelpCategory::Disks:              return "Disks";
    case HelpCategory::DisplayAndPanels:   return "Display and panels";
    case HelpCategory::SessionAndSettings: return "Session and settings";
    }

    return "";
}
