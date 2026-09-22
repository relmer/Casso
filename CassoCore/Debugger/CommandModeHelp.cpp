#include "Pch.h"

#include "Debugger/CommandModeHelp.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kMonitor
//
//  The Apple II Monitor's commands, as the parser reads them: an address or
//  range first, then the command character.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr CommandModeHelp::Entry  s_kMonitor[] =
{
    { "",       "addr",                 "Show the byte at addr"                                    },
    { ".",      "first.last",           "Show the bytes from first to last"                        },
    { "",       "Return",               "Show the next row of bytes"                               },
    { ":",      "addr: bb bb ...",      "Store bytes starting at addr"                             },
    { "L",      "addrL",                "Disassemble from addr, or on from the last listing"       },
    { "G",      "addrG",                "Run from addr"                                            },
    { "S",      "addrS",                "Step one instruction"                                     },
    { "T",      "addrT",                "Trace from addr"                                          },
    { "M",      "dest<first.lastM",     "Copy first..last to dest"                                 },
    { "V",      "dest<first.lastV",     "Compare first..last with the bytes at dest"               },
    { "",       "value<first.lastS",    "Search first..last for value"                             },
    { "R",      "first.lastR name",     "Read a file into first..last"                             },
    { "W",      "first.lastW name",     "Write first..last to a file"                              },
    { "",       "Ctrl+E",               "Show the registers; a : after it changes them"            },
    { "!",      "!",                    "Enter the mini-assembler"                                 },
    { "I",      "I",                    "Inverse text"                                             },
    { "N",      "N",                    "Normal text"                                              },
    { "",       "slot Ctrl+K",          "Take input from a slot"                                   },
    { "",       "slot Ctrl+P",          "Send output to a slot"                                    },
    { "",       "Ctrl+B",               "BASIC cold start"                                         },
    { "",       "Ctrl+C",               "BASIC warm start"                                         },
    { "",       "Ctrl+Y",               "Jump through the user vector at $03F8"                    },
};





////////////////////////////////////////////////////////////////////////////////
//
//  s_kGSSquared
//
////////////////////////////////////////////////////////////////////////////////

static constexpr CommandModeHelp::Entry  s_kGSSquared[] =
{
    { "",        "addr",                   "Show the byte at addr"                              },
    { "",        "first.last",             "Show the bytes from first to last"                  },
    { "",        "addr: bb bb ...",        "Store bytes starting at addr"                       },
    { "set",     "set addr bb bb ...",     "Store bytes starting at addr"                       },
    { "l",       "l [addr]",               "Disassemble from addr, or on from the last listing" },
    { "list",    "list [addr]",            "Disassemble from addr, or on from the last listing" },
    { "move",    "move first.last dest",   "Copy first..last to dest"                           },
    { "bp",      "bp addr [IF expr]",      "Set an execution breakpoint"                        },
    { "bpd",     "bpd addr r|w|rw",        "Break on a read or write of addr"                   },
    { "bpi",     "bpi addr r|w|rw",        "Break on an I/O access, $C000-$C0FF"                },
    { "nobp",    "nobp id|addr",           "Clear a breakpoint"                                 },
    { "watch",   "watch addr",             "Watch addr in the watch pane"                       },
    { "nowatch", "nowatch id",             "Stop watching"                                      },
    { "load",    "load \"file\" addr",     "Read a file into memory at addr"                    },
    { "save",    "save \"file\" first.last", "Write first..last to a file"                      },
    { "sload",   "sload \"file\"",         "Load a symbol file"                                 },
    { "slookup", "slookup addr",           "Show the symbol at addr"                            },
    { "sclear",  "sclear",                 "Clear the loaded symbols"                           },
    { "s",       "s, or Space",            "Step into"                                          },
    { "o",       "o",                      "Step over"                                          },
    { "r",       "r",                      "Step out"                                           },
    { "g",       "g, or Return",           "Run"                                                },
    { "help",    "help [word]",            "This list, or one command"                          },
};





////////////////////////////////////////////////////////////////////////////////
//
//  s_kWinDbg
//
////////////////////////////////////////////////////////////////////////////////

static constexpr CommandModeHelp::Entry  s_kWinDbg[] =
{
    { "t",        "t [count]",              "Step into"                                    },
    { "p",        "p [count]",              "Step over"                                    },
    { "gu",       "gu",                     "Step out"                                     },
    { "g",        "g [addr]",               "Run, stopping at addr if given"               },
    { "pa",       "pa addr",                "Run to addr"                                  },
    { "ta",       "ta addr",                "Run to addr"                                  },
    { "bp",       "bp addr",                "Set an execution breakpoint"                  },
    { "ba",       "ba r1|w1|e1 addr",       "Break on a read, write or execution of addr"  },
    { "bl",       "bl",                     "List the breakpoints"                         },
    { "bc",       "bc id",                  "Clear a breakpoint"                           },
    { "bd",       "bd id",                  "Disable a breakpoint"                         },
    { "be",       "be id",                  "Enable a breakpoint"                          },
    { "r",        "r [reg[=value]]",        "Show or change the registers"                 },
    { "db",       "db addr [l n]",          "Show bytes"                                   },
    { "dw",       "dw addr [l n]",          "Show words"                                   },
    { "da",       "da addr [l n]",          "Show text"                                    },
    { "eb",       "eb addr bb bb ...",      "Store bytes"                                  },
    { "ew",       "ew addr ww ...",         "Store words"                                  },
    { "ea",       "ea addr \"text\"",       "Store text"                                   },
    { "f",        "f addr l n bb ...",      "Fill memory with bytes"                       },
    { "s",        "s addr l n bb ...",      "Search memory for bytes"                      },
    { "m",        "m addr l n dest",        "Copy memory to dest"                          },
    { "u",        "u [addr]",               "Disassemble"                                  },
    { "k",        "k",                      "Show the call stack"                          },
    { "x",        "x name",                 "Look up a symbol"                             },
    { "?",        "? expr",                 "Evaluate an expression"                       },
    { ".formats", ".formats expr",          "Show a value in every base"                   },
    { "l+s",      "l+s, l-s",               "Show or hide source lines"                    },
    { "lsa",      "lsa",                    "Show the source line at the PC"               },
    { ".help",    ".help [word]",           "This list, or one command"                    },
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::GetEntries
//
////////////////////////////////////////////////////////////////////////////////

std::span<const CommandModeHelp::Entry> CommandModeHelp::GetEntries (CommandMode mode)
{
    switch (mode)
    {
    case CommandMode::Monitor:   return s_kMonitor;
    case CommandMode::GSSquared: return s_kGSSquared;
    case CommandMode::WinDbg:    return s_kWinDbg;
    default:                     return {};
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::Find
//
//  An entry with no word is a form, not a name, and is only ever listed.
//
////////////////////////////////////////////////////////////////////////////////

const CommandModeHelp::Entry * CommandModeHelp::Find (CommandMode mode, const std::string & word)
{
    for (const Entry & entry : GetEntries (mode))
    {
        if (entry.word[0] != '\0' && _stricmp (entry.word, word.c_str()) == 0)
        {
            return &entry;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::GetTitle
//
////////////////////////////////////////////////////////////////////////////////

const char * CommandModeHelp::GetTitle (CommandMode mode)
{
    switch (mode)
    {
    case CommandMode::Monitor:   return "Monitor";
    case CommandMode::GSSquared: return "GSSquared";
    case CommandMode::WinDbg:    return "WinDbg";
    default:                     return "AppleWin";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::GetEngineRoute
//
////////////////////////////////////////////////////////////////////////////////

const char * CommandModeHelp::GetEngineRoute (CommandMode mode)
{
    switch (mode)
    {
    case CommandMode::Monitor:   return "after /, as in /BPL";
    case CommandMode::WinDbg:    return "after !, as in !bpl";
    default:                     return "by name, as in BPL";
    }
}
