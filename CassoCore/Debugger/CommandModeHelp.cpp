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
    { "",       C::SessionAndSettings, "a+b",               "Add two hex bytes, eight-bit",                       ""       },
    { "",       C::SessionAndSettings, "a-b",               "Subtract two hex bytes, eight-bit",                  ""       },
    { "",       C::RegistersAndFlags,  "^E",                "Show the registers; a : after it changes them",      "R"      },
    { "!",      C::DisassemblyAndData, "!",                 "Enter the mini-assembler",                           "A"      },
    { "I",      C::DisplayAndPanels,   "I",                 "Inverse text",                                       ""       },
    { "N",      C::DisplayAndPanels,   "N",                 "Normal text",                                        ""       },
    { "",       C::SessionAndSettings, "slot^K",            "Take input from a slot",                             ""       },
    { "",       C::SessionAndSettings, "slot^P",            "Send output to a slot",                              ""       },
    { "",       C::RunningAndStepping, "^B",                "BASIC cold start",                                   ""       },
    { "",       C::RunningAndStepping, "^C",                "BASIC warm start",                                   ""       },
    { "",       C::RunningAndStepping, "^Y",                "Jump through the user vector at $03F8",              ""       },
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
    { "dd",       C::Memory,             "dd addr [l n]",     "Show double words",                           "D"             },
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
    { "l+s",      C::SymbolsAndSource,   "l+s",               "Step by source line",                         "SRC"           },
    { "l-s",      C::SymbolsAndSource,   "l-s",               "Step by instruction",                         "SRC"           },
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
//  CommandModeHelp::GetTypedName
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandModeHelp::GetTypedName (CommandMode mode, const std::string & cassoName)
{
    for (const Entry & entry : GetEntries (mode))
    {
        std::istringstream  names (entry.casso);
        std::string         each;

        while (entry.word[0] != '\0' && names >> each)
        {
            if (_stricmp (each.c_str(), cassoName.c_str()) == 0)
            {
                return entry.word;
            }
        }
    }

    return GetMarker (mode) + ToUpper (cassoName);
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
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CommandModeHelp::BuildHelp (CommandMode mode)
{
    return BuildListing (mode, [] (const Row &) { return true; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::BuildSectionIndex
//
//  One line per section that holds any of the mode's commands, then how to
//  list every command and how to search.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CommandModeHelp::BuildSectionIndex (CommandMode mode)
{
    std::string               help  = GetTypedName (mode, "HELP");
    std::vector<std::string>  all   = BuildHelp (mode);
    std::vector<std::string>  lines;
    std::vector<std::string>  forms;
    std::vector<std::string>  texts;
    size_t                    width = 0;



    for (int i = (int) HelpCategory::RunningAndStepping; i <= (int) HelpCategory::SessionAndSettings; i++)
    {
        HelpCategory  category = (HelpCategory) i;
        std::string   heading  = std::string ("  ") + CassoCommandReference::GetCategoryTitle (category);

        if (std::find (all.begin(), all.end(), heading) == all.end())
        {
            continue;
        }

        forms.push_back (std::format ("{} {}", help, GetSectionWord (category)));
        texts.push_back (CassoCommandReference::GetCategoryTitle (category));
    }

    forms.push_back (help + " all");
    texts.push_back ("Every command");
    forms.push_back (help + " text");
    texts.push_back ("Commands whose syntax or description contains text; * and ? are wildcards, /text/ a regular expression");

    for (const std::string & form : forms)
    {
        width = (std::max) (width, form.size());
    }

    lines.push_back (std::format ("{} help sections:", GetTitle (mode)));

    for (size_t i = 0; i < forms.size(); i++)
    {
        lines.push_back (std::format ("  {:<{}}  {}", forms[i], width, texts[i]));
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::GetSectionWord
//
//  The word HELP takes for a section: its heading's first word, lowercase.
//
////////////////////////////////////////////////////////////////////////////////

const char * CommandModeHelp::GetSectionWord (HelpCategory category)
{
    switch (category)
    {
    case HelpCategory::RunningAndStepping: return "running";
    case HelpCategory::Breakpoints:        return "breakpoints";
    case HelpCategory::RegistersAndFlags:  return "registers";
    case HelpCategory::Memory:             return "memory";
    case HelpCategory::DisassemblyAndData: return "disassembly";
    case HelpCategory::SymbolsAndSource:   return "symbols";
    case HelpCategory::Disks:              return "disks";
    case HelpCategory::DisplayAndPanels:   return "display";
    case HelpCategory::SessionAndSettings: return "session";
    }

    return "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::TryFindSection
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::TryFindSection (const std::string & word, HelpCategory & category)
{
    for (int i = (int) HelpCategory::RunningAndStepping; i <= (int) HelpCategory::SessionAndSettings; i++)
    {
        if (_stricmp (word.c_str(), GetSectionWord ((HelpCategory) i)) == 0)
        {
            category = (HelpCategory) i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::BuildSection
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CommandModeHelp::BuildSection (CommandMode mode, HelpCategory category)
{
    return BuildListing (mode, [category] (const Row & row) { return row.category == category; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::TrySearch
//
//  Text between slashes is a regular expression as typed; any other text is
//  matched anywhere in a line, * standing for any run of characters and ?
//  for any one. Case never matters.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::TrySearch (CommandMode mode, const std::string & text, std::vector<std::string> & lines, std::string & error)
{
    bool         isRegex = text.size() >= 2 && text.front() == '/' && text.back() == '/';
    std::string  pattern;
    std::regex   expression;



    if (isRegex)
    {
        pattern = text.substr (1, text.size() - 2);
    }
    else
    {
        for (char ch : text)
        {
            if (ch == '*')
            {
                pattern += ".*";
            }
            else if (ch == '?')
            {
                pattern += '.';
            }
            else
            {
                if (std::strchr ("\\^$.|+()[]{}", ch) != nullptr)
                {
                    pattern += '\\';
                }

                pattern += ch;
            }
        }
    }

    try
    {
        expression = std::regex (pattern, std::regex::ECMAScript | std::regex::icase);
    }
    catch (const std::regex_error &)
    {
        error = std::format ("{} is not a valid regular expression.", text);
        return false;
    }

    lines = BuildListing (mode, [&expression] (const Row & row)
    {
        return std::regex_search (row.syntax, expression) || std::regex_search (row.description, expression);
    });

    if (lines.empty())
    {
        lines.push_back (std::format ("No command matches {}.", text));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::TryBuildWordHelp
//
//  ALL lists every command; a section word lists the section; a command is
//  described, and when its word is also a section's, a line on how to ask
//  for the section follows; anything else is searched for.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::TryBuildWordHelp (CommandMode mode, const std::string & text, std::vector<std::string> & lines, std::string & error)
{
    HelpCategory              category  = HelpCategory::RunningAndStepping;
    bool                      isSection = TryFindSection (text, category);
    std::string               described;
    std::vector<std::string>  matches;



    lines.clear();

    if (_stricmp (text.c_str(), "all") == 0)
    {
        lines = BuildHelp (mode);
        return true;
    }

    if (TryDescribe (mode, text, described))
    {
        lines.push_back (described);

        //  Another mode's word is also searched for here, since the mode
        //  may have the same thing under a word of its own.
        if (Find (mode, text) == nullptr && AppleWinCommandTable::Find (text) == nullptr &&
            TrySearch (mode, text, matches, error) && !matches.empty() && matches[0].find ("No command matches") != 0)
        {
            lines.push_back ("");
            lines.insert (lines.end(), matches.begin(), matches.end());
        }

        if (isSection)
        {
            lines.push_back (std::format ("The {} section: {} {}",
                                          CassoCommandReference::GetCategoryTitle (category),
                                          GetTypedName (mode, "HELP"),
                                          GetSectionWord (category)));
        }

        return true;
    }

    if (isSection)
    {
        lines = BuildSection (mode, category);
        return true;
    }

    return TrySearch (mode, text, lines, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::BuildReference
//
//  Every mode's HELP ALL, each under its own heading, in a fenced block so
//  the columns survive.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandModeHelp::BuildReference()
{
    std::string  text;



    text += "# Debugger command reference\n";
    text += "\n";
    text += "Generated from the debugger's help table; do not edit by hand. A unit test\n";
    text += "fails when this file and `help all` differ. Regenerate it with\n";
    text += "`pwsh scripts/UpdateDebuggerCommands.ps1`.\n";

    for (CommandMode mode : s_kModes)
    {
        text += std::format ("\n## {} mode\n\n```text\n", GetTitle (mode));

        for (const std::string & line : BuildHelp (mode))
        {
            text += line;
            text += '\n';
        }

        text += "```\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::BuildListing
//
//  The mode's own commands, then the Casso commands it reaches and has no
//  form of its own for, one syntax column for both so the two lists read as
//  one. Casso mode's commands are all Casso's, and AppleWin mode's are
//  Casso's less the engine commands. Only the rows keep accepts are listed.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CommandModeHelp::BuildListing (CommandMode mode, const std::function<bool (const Row &)> & keep)
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

    for (std::vector<Row> * rows : { &own, &casso })
    {
        std::erase_if (*rows, [&keep] (const Row & row) { return !keep (row); });
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
//  The mode's own word first, then a Casso command it runs. A word written
//  with the mode's marker, /W or !DB, is the Casso command alone, since the
//  bare word may be the mode's own. A Casso command it cannot run is
//  answered with the modes that can, never described as if it ran here.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::TryDescribe (CommandMode mode, const std::string & word, std::string & line)
{
    std::string                            marker    = GetMarker (mode);
    bool                                   isMarked  = !marker.empty() && word.size() > marker.size() && word.starts_with (marker);
    std::string                            name      = isMarked ? word.substr (marker.size()) : word;
    const Entry                          * own       = isMarked ? nullptr : Find (mode, word);
    const CassoCommandReference::Entry   * reference = CassoCommandReference::Find (name);
    const AppleWinCommand                * command   = AppleWinCommandTable::Find (name);
    std::string                            upper     = ToUpper (name);



    if (own != nullptr)
    {
        line = std::format ("{}: {}", own->syntax, own->description);
        return true;
    }

    if (command == nullptr)
    {
        return TryDescribeOtherModesWord (mode, word, line);
    }

    if (reference != nullptr && IsCassoCommandReachable (mode, name))
    {
        line = std::format ("{}: {}", GetShownSyntax (mode, *reference), reference->description);
        return true;
    }

    if (command->availability == CommandAvailability::NotAvailable && command->reason != nullptr)
    {
        line = std::format ("{}: {}", upper, command->reason);
        return true;
    }

    //  A layout name the debugger window takes and answers without a change.
    if (reference == nullptr && command->family == AppleWinCommandFamily::Window)
    {
        line = std::format ("{}: changes nothing; the debugger window shows every pane at once", upper);
        return true;
    }

    //  A name AppleWin's scripts use that has no effect in Casso.
    if (reference == nullptr)
    {
        line = std::format ("{}: not available in Casso", upper);
        return true;
    }

    //  The mode has forms of its own that stand for the command.
    if (IsCoveredByMode (mode, name))
    {
        line = std::format ("{} is written {} in {} mode.", upper, GetCoveringForms (mode, name), GetTitle (mode));
        return true;
    }

    line = std::format ("{} does not run in {} mode. It runs in {}.", upper, GetTitle (mode), GetModesThatRun (name));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp::TryDescribeOtherModesWord
//
//  A word that only other modes' own tables hold: which modes those are.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeHelp::TryDescribeOtherModesWord (CommandMode mode, const std::string & word, std::string & line)
{
    std::vector<std::string>  modes;
    std::string               list;



    for (CommandMode other : s_kModes)
    {
        if (other != mode && Find (other, word) != nullptr)
        {
            modes.push_back (GetTitle (other));
        }
    }

    if (modes.empty())
    {
        return false;
    }

    for (size_t i = 0; i < modes.size(); i++)
    {
        list += (i == 0) ? "" : (i + 1 == modes.size()) ? " and " : ", ";
        list += modes[i];
    }

    line = std::format ("{} does not run in {} mode. It runs in {} {}.", ToUpper (word), GetTitle (mode), list, modes.size() == 1 ? "mode" : "modes");
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
//  CommandModeHelp::GetCoveringForms
//
//  The syntax of each of the mode's own commands that stands for the Casso
//  command, as a list: "addr or first.last".
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandModeHelp::GetCoveringForms (CommandMode mode, const std::string & cassoName)
{
    std::vector<std::string>  forms;
    std::string               text;



    for (const Entry & entry : GetEntries (mode))
    {
        std::istringstream  names (entry.casso);
        std::string         each;

        while (names >> each)
        {
            if (_stricmp (each.c_str(), cassoName.c_str()) == 0)
            {
                forms.push_back (entry.syntax);
                break;
            }
        }
    }

    for (size_t i = 0; i < forms.size(); ++i)
    {
        if (i > 0)
        {
            text += (i + 1 == forms.size()) ? " or " : ", ";
        }

        text += forms[i];
    }

    return text;
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
