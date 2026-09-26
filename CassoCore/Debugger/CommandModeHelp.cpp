#include "Pch.h"

#include "Debugger/CommandModeHelp.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/WinDbgParser.h"





using C = HelpCategory;





////////////////////////////////////////////////////////////////////////////////
//
//  s_kMonitor
//
//  The Apple II Monitor's commands, as the parser reads them: an address or
//  range first, then the command character. The last field is the Casso
//  commands each one stands for.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr CommandModeHelp::Entry  s_kMonitor[] =
{
    { "",       C::Memory,             "addr",              "Show the byte at addr",                              "D"      },
    { ".",      C::Memory,             "first.last",        "Show the bytes from first to last",                  "D"      },
    { "",       C::Memory,             "Return",            "Show the next row of bytes",                         "D"      },
    { ":",      C::Memory,             "addr: bb bb ...",   "Store bytes starting at addr",                       "ME MEB" },
    { "L",      C::DisassemblyAndData, "addrL",             "Disassemble from addr, or on from the last listing", "U"      },
    { "G",      C::RunningAndStepping, "addrG",             "Run from addr",                                      "G"      },
    { "S",      C::RunningAndStepping, "addrS",             "Step one instruction",                               "T"      },
    { "T",      C::RunningAndStepping, "addrT",             "Trace from addr",                                    "T"      },
    { "M",      C::Memory,             "dest<first.lastM",  "Copy first..last to dest",                           "M"      },
    { "V",      C::Memory,             "dest<first.lastV",  "Compare first..last with the bytes at dest",         "MC"     },
    { "",       C::Memory,             "value<first.lastS", "Search first..last for value",                       "S SH"   },
    { "R",      C::Memory,             "first.lastR name",  "Read a file into first..last",                       "BLOAD"  },
    { "W",      C::Memory,             "first.lastW name",  "Write first..last to a file",                        "BSAVE"  },
    { "",       C::RegistersAndFlags,  "Ctrl+E",            "Show the registers; a : after it changes them",      "R"      },
    { "!",      C::DisassemblyAndData, "!",                 "Enter the mini-assembler",                           "A"      },
    { "I",      C::DisplayAndPanels,   "I",                 "Inverse text",                                       ""       },
    { "N",      C::DisplayAndPanels,   "N",                 "Normal text",                                        ""       },
    { "",       C::SessionAndSettings, "slot Ctrl+K",       "Take input from a slot",                             ""       },
    { "",       C::SessionAndSettings, "slot Ctrl+P",       "Send output to a slot",                              ""       },
    { "",       C::RunningAndStepping, "Ctrl+B",            "BASIC cold start",                                   ""       },
    { "",       C::RunningAndStepping, "Ctrl+C",            "BASIC warm start",                                   ""       },
    { "",       C::RunningAndStepping, "Ctrl+Y",            "Jump through the user vector at $03F8",              ""       },
};





////////////////////////////////////////////////////////////////////////////////
//
//  s_kGSSquared
//
////////////////////////////////////////////////////////////////////////////////

static constexpr CommandModeHelp::Entry  s_kGSSquared[] =
{
    { "",        C::Memory,             "addr",                     "Show the byte at addr",                              "D"             },
    { "",        C::Memory,             "first.last",               "Show the bytes from first to last",                  "D"             },
    { "",        C::Memory,             "addr: bb bb ...",          "Store bytes starting at addr",                       "ME MEB"        },
    { "set",     C::Memory,             "set addr bb bb ...",       "Store bytes starting at addr",                       "ME MEB"        },
    { "l",       C::DisassemblyAndData, "l [addr]",                 "Disassemble from addr, or on from the last listing", "U"             },
    { "list",    C::DisassemblyAndData, "list [addr]",              "Disassemble from addr, or on from the last listing", "U"             },
    { "move",    C::Memory,             "move first.last dest",     "Copy first..last to dest",                           "M"             },
    { "bp",      C::Breakpoints,        "bp addr [IF expr]",        "Set an execution breakpoint",                        "BP BPX"        },
    { "bpd",     C::Breakpoints,        "bpd addr r|w|rw",          "Break on a read or write of addr",                   "BPM BPMR BPMW" },
    { "bpi",     C::Breakpoints,        "bpi addr r|w|rw",          "Break on an I/O access, $C000-$C0FF",                "BPM"           },
    { "nobp",    C::Breakpoints,        "nobp id|addr",             "Clear a breakpoint",                                 "BPC"           },
    { "watch",   C::Memory,             "watch addr",               "Watch addr in the watch pane",                       "W WA"          },
    { "nowatch", C::Memory,             "nowatch id",               "Stop watching",                                      "WC"            },
    { "load",    C::Memory,             "load \"file\" addr",       "Read a file into memory at addr",                    "BLOAD"         },
    { "save",    C::Memory,             "save \"file\" first.last", "Write first..last to a file",                        "BSAVE"         },
    { "sload",   C::SymbolsAndSource,   "sload \"file\"",           "Load a symbol file",                                 ""              },
    { "slookup", C::SymbolsAndSource,   "slookup addr",             "Show the symbol at addr",                            "SYM"           },
    { "sclear",  C::SymbolsAndSource,   "sclear",                   "Clear the loaded symbols",                           ""              },
    { "s",       C::RunningAndStepping, "s, or Space",              "Step into",                                          "T"             },
    { "o",       C::RunningAndStepping, "o",                        "Step over",                                          "P"             },
    { "r",       C::RunningAndStepping, "r",                        "Step out",                                           "RTS"           },
    { "g",       C::RunningAndStepping, "g, or Return",             "Run",                                                "G"             },
    { "help",    C::SessionAndSettings, "help [word]",              "This list, or one command",                          "HELP ?"        },
};





////////////////////////////////////////////////////////////////////////////////
//
//  s_kWinDbg
//
////////////////////////////////////////////////////////////////////////////////

static constexpr CommandModeHelp::Entry  s_kWinDbg[] =
{
    { "t",        C::RunningAndStepping, "t [count]",         "Step into",                                   "T"             },
    { "p",        C::RunningAndStepping, "p [count]",         "Step over",                                   "P"             },
    { "gu",       C::RunningAndStepping, "gu",                "Step out",                                    "RTS"           },
    { "g",        C::RunningAndStepping, "g [addr]",          "Run, stopping at addr if given",              "G"             },
    { "pa",       C::RunningAndStepping, "pa addr",           "Run to addr",                                 "G"             },
    { "ta",       C::RunningAndStepping, "ta addr",           "Run to addr",                                 "G"             },
    { "bp",       C::Breakpoints,        "bp addr",           "Set an execution breakpoint",                 "BP BPX"        },
    { "ba",       C::Breakpoints,        "ba r1|w1|e1 addr",  "Break on a read, write or execution of addr", "BPM BPMR BPMW" },
    { "bl",       C::Breakpoints,        "bl",                "List the breakpoints",                        "BPL"           },
    { "bc",       C::Breakpoints,        "bc id",             "Clear a breakpoint",                          "BPC"           },
    { "bd",       C::Breakpoints,        "bd id",             "Disable a breakpoint",                        "BPD"           },
    { "be",       C::Breakpoints,        "be id",             "Enable a breakpoint",                         "BPE"           },
    { "r",        C::RegistersAndFlags,  "r [reg[=value]]",   "Show or change the registers",                "R"             },
    { "db",       C::Memory,             "db addr [l n]",     "Show bytes",                                  "D"             },
    { "dw",       C::Memory,             "dw addr [l n]",     "Show words",                                  "D"             },
    { "da",       C::Memory,             "da addr [l n]",     "Show text",                                   "D"             },
    { "eb",       C::Memory,             "eb addr bb bb ...", "Store bytes",                                 "ME MEB"        },
    { "ew",       C::Memory,             "ew addr ww ...",    "Store words",                                 "MEW"           },
    { "ea",       C::Memory,             "ea addr \"text\"",  "Store text",                                  ""              },
    { "f",        C::Memory,             "f addr l n bb ...", "Fill memory with bytes",                      "F"             },
    { "s",        C::Memory,             "s addr l n bb ...", "Search memory for bytes",                     "S SH"          },
    { "m",        C::Memory,             "m addr l n dest",   "Copy memory to dest",                         "M"             },
    { "u",        C::DisassemblyAndData, "u [addr]",          "Disassemble",                                 "U"             },
    { "k",        C::RegistersAndFlags,  "k",                 "Show the call stack",                         "CALLS"         },
    { "x",        C::SymbolsAndSource,   "x name",            "Look up a symbol",                            "SYM"           },
    { "?",        C::SessionAndSettings, "? expr",            "Evaluate an expression",                      "CALC"          },
    { ".formats", C::SessionAndSettings, ".formats expr",     "Show a value in every base",                  "CALC"          },
    { "l+s",      C::SymbolsAndSource,   "l+s, l-s",          "Show or hide source lines",                   "SRC"           },
    { "lsa",      C::SymbolsAndSource,   "lsa",               "Show the source line at the PC",              "SRC"           },
    { ".help",    C::SessionAndSettings, ".help [word]",      "This list, or one command",                   "HELP ?"        },
};





////////////////////////////////////////////////////////////////////////////////
//
//  s_kModes
//
//  Every mode, in the order a list of them names them.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr CommandMode  s_kModes[] =
{
    CommandMode::AppleWin,
    CommandMode::Monitor,
    CommandMode::GSSquared,
    CommandMode::WinDbg,
    CommandMode::Casso,
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
    case CommandMode::Casso:     return "Casso";
    default:                     return "AppleWin";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::GetMarker
//
////////////////////////////////////////////////////////////////////////////////

const char * CommandModeHelp::GetMarker (CommandMode mode)
{
    switch (mode)
    {
    case CommandMode::Monitor: return "/";
    case CommandMode::WinDbg:  return "!";
    default:                   return "";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::IsCassoCommandReachable
//
//  AppleWin and Casso modes run the whole table. Elsewhere a name runs when
//  it is an engine command, or when the mode does not use the name itself --
//  a command the mode has its own form of runs too, so the debugger's
//  controls and a script need not know every dialect's words, but help lists
//  the mode's form instead. A command that only touches the debugger
//  window's layout runs through Monitor's `/` alone, since GSSquared and
//  WinDbg lines never reach the window's own commands.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::IsCassoCommandReachable (CommandMode mode, const std::string & name)
{
    const AppleWinCommand  * command = AppleWinCommandTable::Find (name);



    if (command == nullptr)
    {
        return false;
    }

    if (mode == CommandMode::AppleWin || mode == CommandMode::Casso)
    {
        return true;
    }

    if (CassoCommandReference::Find (name) == nullptr)
    {
        return false;
    }

    if (command->availability == CommandAvailability::WindowOnly && mode != CommandMode::Monitor)
    {
        return false;
    }

    return command->family == AppleWinCommandFamily::Engine || !IsWordOfMode (mode, name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::BuildHelp
//
//  The mode's own commands, then the Casso commands it reaches and has no
//  form of its own for, one syntax column for both so the two lists read as
//  one. Casso mode's commands are all Casso's, and AppleWin mode's are
//  Casso's less the engine commands.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CommandModeHelp::BuildHelp (CommandMode mode)
{
    std::vector<Row>          own;
    std::vector<Row>          casso;
    std::vector<std::string>  lines;
    size_t                    width = 0;



    for (const Entry & entry : GetEntries (mode))
    {
        own.push_back ({ entry.category, entry.syntax, entry.description });
    }

    for (const CassoCommandReference::Entry & entry : CassoCommandReference::GetAll())
    {
        const AppleWinCommand  * command  = AppleWinCommandTable::Find (entry.name);
        bool                     isEngine = command != nullptr && command->family == AppleWinCommandFamily::Engine;
        std::string              syntax   = GetShownSyntax (mode, entry);

        if (syntax.empty() || IsCoveredByMode (mode, entry.name))
        {
            continue;
        }

        if (mode == CommandMode::AppleWin && !isEngine)
        {
            own.push_back ({ entry.category, syntax, entry.description });
        }
        else
        {
            casso.push_back ({ entry.category, syntax, entry.description });
        }
    }

    for (const std::vector<Row> * rows : { &own, &casso })
    {
        for (const Row & row : *rows)
        {
            width = (std::max) (width, row.syntax.size());
        }
    }

    if (!own.empty())
    {
        AppendSection (std::string (GetTitle (mode)) + " commands", std::move (own), width, lines);
    }

    if (!casso.empty())
    {
        if (!lines.empty())
        {
            lines.push_back ("");
        }

        AppendSection ("Casso commands", std::move (casso), width, lines);
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::TryDescribe
//
//  The mode's own word first, then a Casso command it runs. A Casso command
//  it cannot run is answered with the modes that can, never described as if
//  it ran here.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::TryDescribe (CommandMode mode, const std::string & word, std::string & line)
{
    const Entry                          * own       = Find (mode, word);
    const CassoCommandReference::Entry   * reference = CassoCommandReference::Find (word);
    const AppleWinCommand                * command   = AppleWinCommandTable::Find (word);
    std::string                            upper     = ToUpper (word);



    if (own != nullptr)
    {
        line = std::format ("{}: {}", own->syntax, own->description);
        return true;
    }

    if (command == nullptr)
    {
        return false;
    }

    if (reference != nullptr && IsCassoCommandReachable (mode, word))
    {
        line = std::format ("{}: {}", GetShownSyntax (mode, *reference), reference->description);
        return true;
    }

    if (command->availability == CommandAvailability::NotAvailable && command->reason != nullptr)
    {
        line = std::format ("{}: {}", upper, command->reason);
        return true;
    }

    //  A name AppleWin's scripts use that has no effect in Casso.
    if (reference == nullptr)
    {
        line = std::format ("{}: not available in Casso", upper);
        return true;
    }

    line = std::format ("{} does not run in {} mode. It runs in {}.", upper, GetTitle (mode), GetModesThatRun (word));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::IsCoveredByMode
//
//  Whether one of the mode's own commands stands for the Casso command.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::IsCoveredByMode (CommandMode mode, const std::string & cassoName)
{
    for (const Entry & entry : GetEntries (mode))
    {
        std::istringstream  names (entry.casso);
        std::string         each;

        while (names >> each)
        {
            if (_stricmp (each.c_str(), cassoName.c_str()) == 0)
            {
                return true;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::IsWordOfMode
//
//  Whether the mode already reads the name as something of its own. GSSquared
//  takes any bare hex token as an address, so a name made only of hex digits
//  is its, as are its command words. WinDbg keeps some `!` words for itself.
//  Monitor's `/` sets its Casso commands apart from everything it reads.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::IsWordOfMode (CommandMode mode, const std::string & name)
{
    std::string  bang = "!" + name;



    if (mode == CommandMode::GSSquared)
    {
        if (IsHexWord (name))
        {
            return true;
        }

        for (const GSSquaredCommand & command : GSSquaredParser::GetCommands())
        {
            if (_stricmp (command.name, name.c_str()) == 0)
            {
                return true;
            }
        }
    }

    if (mode == CommandMode::WinDbg)
    {
        for (const WinDbgExclusion & exclusion : WinDbgParser::GetExclusions())
        {
            if (_stricmp (exclusion.name, bang.c_str()) == 0)
            {
                return true;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::IsHexWord
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::IsHexWord (const std::string & name)
{
    return !name.empty() && std::all_of (name.begin(), name.end(), [] (char ch) { return isxdigit ((unsigned char) ch) != 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::GetShownSyntax
//
//  The command's syntax as it is typed in the mode: each of its names the
//  mode runs, marker first, then the arguments. Empty when the mode runs
//  none of them.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandModeHelp::GetShownSyntax (CommandMode mode, const CassoCommandReference::Entry & entry)
{
    std::string  marker = GetMarker (mode);
    std::string  names;
    std::string  syntax = entry.syntax;
    std::string  rest   = syntax.substr ((std::min) (syntax.size(), strlen (entry.name)));



    if (IsCassoCommandReachable (mode, entry.name))
    {
        names = marker + entry.name;
    }

    for (const AppleWinCommand & alias : AppleWinCommandTable::GetAll())
    {
        if (alias.aliasOf != nullptr && _stricmp (alias.aliasOf, entry.name) == 0 && IsCassoCommandReachable (mode, alias.name))
        {
            names += (names.empty() ? "" : ", ") + marker + alias.name;
        }
    }

    return names.empty() ? std::string() : names + rest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::GetModesThatRun
//
//  "AppleWin, Monitor as /DISK and Casso modes": each mode that runs the
//  name, with the name as it is typed there where that differs.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandModeHelp::GetModesThatRun (const std::string & name)
{
    std::vector<std::string>  modes;
    std::string               list;
    std::string               upper = ToUpper (name);



    for (CommandMode mode : s_kModes)
    {
        if (IsCassoCommandReachable (mode, name))
        {
            modes.push_back (GetMarker (mode)[0] == '\0' ? std::string (GetTitle (mode))
                                                         : std::format ("{} as {}{}", GetTitle (mode), GetMarker (mode), upper));
        }
    }

    for (size_t i = 0; i < modes.size(); i++)
    {
        list += (i == 0) ? "" : (i + 1 == modes.size()) ? " and " : ", ";
        list += modes[i];
    }

    return list + (modes.size() == 1 ? " mode" : " modes");
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::AppendSection
//
//  A heading, then each category's heading and its commands, alphabetical
//  within it.
//
////////////////////////////////////////////////////////////////////////////////

void CommandModeHelp::AppendSection (const std::string & heading, std::vector<Row> rows, size_t width, std::vector<std::string> & lines)
{
    std::optional<HelpCategory>  current;



    std::stable_sort (rows.begin(), rows.end(), [] (const Row & a, const Row & b)
    {
        if (a.category != b.category)
        {
            return a.category < b.category;
        }

        return _stricmp (a.syntax.c_str(), b.syntax.c_str()) < 0;
    });

    lines.push_back (heading + ":");

    for (const Row & row : rows)
    {
        if (!current.has_value() || *current != row.category)
        {
            current = row.category;
            lines.push_back (std::string ("  ") + CassoCommandReference::GetCategoryTitle (row.category));
        }

        lines.push_back (std::format ("    {:<{}}  {}", row.syntax, width, row.description));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandModeHelp::ToUpper (const std::string & text)
{
    std::string  upper (text);



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return upper;
}
