#include "Pch.h"

#include "Debugger/WinDbgParser.h"

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/WinDbgExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  The WinDbg-mode commands, each with the AppleWin-mode command whose
//  DebugCommand it builds. The sweep test reads this table, so a command added here
//  without a matching case there fails it.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr WinDbgCommand s_kCommands[] =
{
    { "t",        "T"     },
    { "p",        "P"     },
    { "gu",       "RTS"   },
    { "g",        "G"     },
    { "t-",       "T-"    },
    { "p-",       "P-"    },
    { "gu-",      "GU-"   },
    { "g-",       "G-"    },
    { "pa",       "G"     },
    { "ta",       "G"     },
    { "bp",       "BP"    },
    { "bl",       "BPL"   },
    { "bc",       "BPC"   },
    { "bd",       "BPD"   },
    { "be",       "BPE"   },
    { "ba",       "BPMR"  },
    { "db",       "D"     },
    { "dw",       "D"     },
    { "dd",       "D"     },
    { "da",       "D"     },
    { "eb",       "MEB"   },
    { "ew",       "MEW"   },
    { "ea",       "MEB"   },
    { "f",        "F"     },
    { "s",        "S"     },
    { "m",        "M"     },
    { "r",        "R"     },
    { "u",        "U"     },
    { "x",        "SYM"   },
    { "k",        "CALLS" },
    { "?",        "CALC"  },
    { ".formats", "CALC"  },
    { "l+s",      "SRC"   },
    { "l-s",      "SRC"   },
    { "lsa",      "SRC"   },
    { ".help",    "HELP"  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  The WinDbg commands that assume what a 6502 does not have, by family.
//  Names that are a family by their first characters (`~0s`, `sxe`,
//  `!ext.foo`, `$t0`) are matched in TryFindExclusion.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char * s_kpszThreads    = "threads and processes";
static constexpr const char * s_kpszModules    = "modules and symbol paths";
static constexpr const char * s_kpszExceptions = "exceptions and events";
static constexpr const char * s_kpszKernel     = "kernel";
static constexpr const char * s_kpszDumps      = "dump files";
static constexpr const char * s_kpszTypes      = "types and locals";
static constexpr const char * s_kpszScripting  = "extensions and scripting";

static constexpr WinDbgExclusion s_kExclusions[] =
{
    { "~",           s_kpszThreads    },
    { "|",           s_kpszThreads    },
    { ".process",    s_kpszThreads    },
    { ".thread",     s_kpszThreads    },
    { ".attach",     s_kpszThreads    },
    { ".kill",       s_kpszThreads    },
    { "!peb",        s_kpszThreads    },
    { "!teb",        s_kpszThreads    },
    { "!process",    s_kpszThreads    },
    { "!thread",     s_kpszThreads    },
    { "!handle",     s_kpszThreads    },
    { "!heap",       s_kpszThreads    },
    { "!address",    s_kpszThreads    },
    { "lm",          s_kpszModules    },
    { ".reload",     s_kpszModules    },
    { ".sympath",    s_kpszModules    },
    { ".srcpath",    s_kpszModules    },
    { "ld",          s_kpszModules    },
    { "sx",          s_kpszExceptions },
    { "sxe",         s_kpszExceptions },
    { "sxd",         s_kpszExceptions },
    { ".lastevent",  s_kpszExceptions },
    { "!analyze",    s_kpszExceptions },
    { "!gle",        s_kpszExceptions },
    { "!pte",        s_kpszKernel     },
    { "!pool",       s_kpszKernel     },
    { "!irql",       s_kpszKernel     },
    { ".trap",       s_kpszKernel     },
    { ".dump",       s_kpszDumps      },
    { ".opendump",   s_kpszDumps      },
    { "dt",          s_kpszTypes      },
    { "dv",          s_kpszTypes      },
    { "dx",          s_kpszTypes      },
    { "??",          s_kpszTypes      },
    { "dq",          s_kpszTypes      },
    { "dp",          s_kpszTypes      },
    { "dps",         s_kpszTypes      },
    { "dds",         s_kpszTypes      },
    { "dqs",         s_kpszTypes      },
    { "dpa",         s_kpszTypes      },
    { "dpu",         s_kpszTypes      },
    { ".frame",      s_kpszTypes      },
    { ".load",       s_kpszScripting  },
    { ".chain",      s_kpszScripting  },
    { "!ext.*",      s_kpszScripting  },
    { ".scriptload", s_kpszScripting  },
    { ".foreach",    s_kpszScripting  },
    { ".if",         s_kpszScripting  },
    { ".while",      s_kpszScripting  },
    { "as",          s_kpszScripting  },
    { "$t0",         s_kpszScripting  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::Parse
//
//  `!name` is an engine command. `?` needs no space before its expression,
//  as in WinDbg. Every argument is read as typed, through a context that
//  reads WinDbg's prefixes (`0x300`, `0n10`) and registers (`@a`); no text
//  is rewritten and parsed again.
//
////////////////////////////////////////////////////////////////////////////////

WinDbgParseResult WinDbgParser::Parse (const std::string & line, const IDebugExpressionContext & context)
{
    WinDbgParseResult          result;
    Build                      build;
    Tokens                     tokens;
    std::string                name;
    std::string                rest;
    size_t                     first     = line.find_first_not_of (" \t");
    size_t                     restFrom  = 0;
    size_t                     bangName  = (first == std::string::npos) ? std::string::npos : line.find_first_not_of (" \t", first + 1);
    const WinDbgExclusion    * exclusion = nullptr;
    WinDbgExpressionContext    windbg (context);



    if (first == std::string::npos)
    {
        return result;
    }

    //  WinDbg reads `! analyze` as `!analyze`.
    if (line[first] == '!' && bangName != std::string::npos && bangName > first + 1)
    {
        return Parse ("!" + line.substr (bangName), context);
    }

    if (TryParseEngine (line.substr (first), context, result))
    {
        return result;
    }

    if (line[first] == '?' && line.compare (first, 2, "??") != 0)
    {
        name     = "?";
        restFrom = first + 1;
    }
    else
    {
        restFrom = line.find_first_of (" \t", first);
        name     = line.substr (first, restFrom == std::string::npos ? std::string::npos : restFrom - first);
    }

    restFrom = (restFrom == std::string::npos) ? std::string::npos : line.find_first_not_of (" \t", restFrom);
    rest     = (restFrom == std::string::npos) ? std::string() : line.substr (restFrom);
    name     = ToLower (name);

    if (TryFindExclusion (name, exclusion))
    {
        result.status = ParseStatus::NotAvailable;
        result.label  = "no meaning on this machine";
        result.error  = std::format ("{} belongs to WinDbg's {} commands.", name, exclusion->family);
        return result;
    }

    tokens                   = Split (rest);
    build.command.sourceName = name;

    if (!TryBuild (name, tokens, rest, windbg, build))
    {
        result.status = build.isDeferred ? ParseStatus::NotAvailable
                      : build.error.empty() ? ParseStatus::Unknown
                      :                       ParseStatus::Invalid;
        result.error  = build.error.empty() ? std::format ("{} is not a WinDbg-mode command.", name) : build.error;
        return result;
    }

    result.status  = ParseStatus::Ok;
    result.command = build.command;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::GetCommands
//
////////////////////////////////////////////////////////////////////////////////

std::span<const WinDbgCommand> WinDbgParser::GetCommands()
{
    return s_kCommands;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::GetExclusions
//
////////////////////////////////////////////////////////////////////////////////

std::span<const WinDbgExclusion> WinDbgParser::GetExclusions()
{
    return s_kExclusions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryParseEngine
//
//  `!name ...` for a Casso command WinDbg reaches (CommandModeHelp), parsed
//  as AppleWin mode parses the same line without the `!`. An excluded `!`
//  command is left to Parse, which reports its family.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryParseEngine (const std::string & line, const IDebugExpressionContext & context, WinDbgParseResult & result)
{
    std::string                text;
    Tokens                     tokens;
    const WinDbgExclusion    * exclusion = nullptr;
    AppleWinParseResult        parsed;
    std::string                described;



    if (line.empty() || line[0] != '!')
    {
        return false;
    }

    text   = line.substr (1);
    tokens = Split (text);

    if (tokens.empty() || TryFindExclusion ("!" + ToLower (tokens[0]), exclusion))
    {
        return false;
    }

    //  A Casso command WinDbg does not reach is answered as .help answers it,
    //  with the same status AppleWin mode gives it.
    if (!CommandModeHelp::IsCassoCommandReachable (CommandMode::WinDbg, tokens[0]))
    {
        if (CommandModeHelp::TryDescribe (CommandMode::WinDbg, tokens[0], described))
        {
            result.status = ParseStatus::NotAvailable;
            result.error  = described;
        }
        else
        {
            result.status = ParseStatus::Unknown;
            result.error  = std::format ("!{} is not a command.", tokens[0]);
        }

        return true;
    }

    parsed                    = AppleWinParser::Parse (text, WinDbgExpressionContext (context));
    result.status             = parsed.status;
    result.command            = parsed.command;
    result.command.sourceName = "!" + tokens[0];
    result.error              = parsed.error;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryFindExclusion
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryFindExclusion (const std::string & name, const WinDbgExclusion *& exclusion)
{
    static constexpr size_t    kPseudoRegisterLength = 3;
    const char               * family                = nullptr;



    for (const WinDbgExclusion & entry : s_kExclusions)
    {
        if (name == entry.name)
        {
            exclusion = &entry;
            return true;
        }
    }

    if      (name.starts_with ('~') || name.starts_with ('|'))  { family = s_kpszThreads;    }
    else if (name.starts_with ("sx"))                           { family = s_kpszExceptions; }
    else if (name.starts_with ("!ext."))                        { family = s_kpszScripting;  }
    else if (name.size() >= kPseudoRegisterLength && name.starts_with ("$t") && isdigit ((unsigned char) name[2]))
    {
        family = s_kpszScripting;
    }

    for (const WinDbgExclusion & entry : s_kExclusions)
    {
        if (family != nullptr && entry.family == family)
        {
            exclusion = &entry;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuild
//
//  The DebugCommand for one WinDbg command. False with no error for a name
//  that is not a WinDbg-mode command.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuild (
    const std::string              & name,
    const Tokens                   & args,
    const std::string              & rest,
    const IDebugExpressionContext  & context,
    Build                          & build)
{
    DebugCommand  & command = build.command;



    if (name == "t" || name == "p")
    {
        command.verb = (name == "t") ? DebugVerb::StepInto : DebugVerb::StepOver;
        return TryBuildStep (args, context, build);
    }
    else if (name == "gu")
    {
        command.verb  = DebugVerb::StepOut;
        command.count = 1;
    }
    else if (name == "t-" || name == "p-" || name == "gu-" || name == "g-")
    {
        if (!args.empty())
        {
            build.error = std::format ("{} takes no arguments.", name);
            return false;
        }

        command.verb = (name == "t-")  ? DebugVerb::StepBack
                     : (name == "p-")  ? DebugVerb::StepBackOver
                     : (name == "gu-") ? DebugVerb::StepBackOut
                     :                   DebugVerb::ReverseGo;
    }
    else if (name == "bl")
    {
        command.verb = DebugVerb::ListBreakpoints;
    }
    else if (name == "bc" || name == "bd" || name == "be")
    {
        command.verb = (name == "bc") ? DebugVerb::ClearBreakpoint
                     : (name == "bd") ? DebugVerb::DisableBreakpoint
                     :                  DebugVerb::EnableBreakpoint;
        return TryBuildIdOrAll (name, args, build);
    }
    else if (name == "eb")       { return TryBuildBytes (args, context, build); }
    else if (name == "ew")       { return TryBuildEnter (name, args, true, context, build); }
    else if (name == "x")
    {
        command.verb = rest.empty() ? DebugVerb::ShowSymbolInfo : DebugVerb::LookupSymbol;
        command.text = rest;
    }
    else if (name == "?" || name == ".formats")
    {
        command.verb  = DebugVerb::Calculate;
        command.hasA1 = true;

        if (args.empty())
        {
            build.error = std::format ("{} needs an expression.", name);
            return false;
        }

        return AppleWinParser::TryEvaluate (rest, context, command.a1, build.error);
    }
    else if (name == "l+s" || name == "l-s")
    {
        command.verb  = DebugVerb::SetSourceStepping;
        command.count = (name == "l+s") ? 1 : 0;
    }
    else if (name == ".help")
    {
        command.verb = DebugVerb::Help;
        command.text = rest;
    }
    else if (name == "bp")       { return TryBuildBreakpoint (args, rest, context, build); }
    else if (name == "ba")       { return TryBuildAccess     (args, context, build); }
    else if (name == "r")        { return TryBuildRegister   (rest, context, build); }
    else if (name == "ea")       { return TryBuildText       (args, rest, context, build); }
    else if (name == "db" || name == "dw" || name == "dd" || name == "da")
    {
        return TryBuildDump (name, args, context, build);
    }
    else if (name == "f" || name == "s" || name == "m")
    {
        return TryBuildRange (name, args, rest, context, build);
    }
    else if (name == "g" || name == "pa" || name == "ta")
    {
        build.error = (name == "g") ? std::string ("g takes one address to stop at, or none to resume.")
                                    : std::format ("{} takes the address to stop at.", name);

        if ((name == "g") ? args.size() > 1 : args.size() != 1)
        {
            return false;
        }

        build.error.clear();
        command.verb  = DebugVerb::Go;
        command.hasA1 = !args.empty();
        return args.empty() || AppleWinParser::TryEvaluate (args[0], context, command.a1, build.error);
    }
    else if (name == "u")
    {
        if (args.size() > 1)
        {
            build.error = "u takes one address, or none to continue.";
            return false;
        }

        command.verb = DebugVerb::Disassemble;
        return args.empty() || AppleWinParser::TryParseRange (args[0], context, command, build.error);
    }
    else if (name == "k")
    {
        if (!args.empty())
        {
            build.error = "k takes no arguments. Use !calls mode to choose how the chain is found.";
            return false;
        }

        command.verb = DebugVerb::ShowCallStack;
    }
    else if (name == "lsa")
    {
        if (!args.empty())
        {
            build.isDeferred = true;
            build.error      = "lsa with a line is not available yet. Use lsa alone to show the source line at PC.";
            return false;
        }

        command.verb = DebugVerb::ShowSource;
    }
    else if (name == "wt")
    {
        build.isDeferred = true;
        build.error      = "wt is not available yet. Use !history or !profile to record what a run executes.";
        return false;
    }
    else
    {
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildStep
//
//  `t [count]` and `p [count]`: one step without a count.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildStep (const Tokens & args, const IDebugExpressionContext & context, Build & build)
{
    Word  value = 0;



    if (!args.empty() && !AppleWinParser::TryEvaluate (args[0], context, value, build.error))
    {
        return false;
    }

    build.command.count = args.empty() ? 1 : value;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildIdOrAll
//
//  `bc`, `bd` and `be` take one breakpoint id, or * for all.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildIdOrAll (const std::string & name, const Tokens & args, Build & build)
{
    bool  isComparison = !args.empty() && std::string_view ("<>=!").find (args[0][0]) != std::string_view::npos;



    if (isComparison && HasIf (args))
    {
        build.error = std::format ("{} with a comparison does not take IF.", name);
        return false;
    }

    if (args.size() > 1)
    {
        build.error = std::format ("{} takes one argument, not {}.", name, args.size());
        return false;
    }

    return AppleWinParser::TryParseIdOrAll (args, build.command, build.error, NumberSyntax::WinDbg);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildBytes
//
//  `eb addr values`: each value is checked to be a byte, as WinDbg's eb
//  takes.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildBytes (const Tokens & args, const IDebugExpressionContext & context, Build & build)
{
    static constexpr uint32_t  kMaxByte = 0xFF;
    uint32_t                   value    = 0;



    for (size_t i = 1; i < args.size(); i++)
    {
        if (!TryEvaluate (args[i], context, value, build.error))
        {
            return false;
        }

        if (value > kMaxByte)
        {
            build.error = std::format ("{} is not a byte. eb takes bytes, 00-FF.", args[i]);
            return false;
        }
    }

    return TryBuildEnter ("eb", args, false, context, build);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildEnter
//
//  An address and the bytes or words to write there.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildEnter (
    const std::string              & name,
    const Tokens                   & args,
    bool                             isWords,
    const IDebugExpressionContext  & context,
    Build                          & build)
{
    DebugCommand  & command = build.command;



    command.verb  = isWords ? DebugVerb::EnterWords : DebugVerb::EnterBytes;
    command.hasA1 = !args.empty();

    if (args.size() < 2)
    {
        build.error = std::format ("{} needs an address and one or more values.", name);
        return false;
    }

    return AppleWinParser::TryEvaluate (args[0], context, command.a1, build.error) &&
           TryAddValues (args, 1, isWords, context, command, build.error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryAddValues
//
//  Bytes, or words stored low byte first, each with a mask that matches
//  every bit.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryAddValues (
    const Tokens                   & tokens,
    size_t                           first,
    bool                             isWords,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr Word  kMaxByte = 0xFF;
    Word                   value    = 0;



    for (size_t i = first; i < tokens.size(); i++)
    {
        if (!AppleWinParser::TryEvaluate (tokens[i], context, value, error))
        {
            return false;
        }

        if (!isWords && value > kMaxByte)
        {
            error = std::format ("{} is not a byte.", tokens[i]);
            return false;
        }

        command.values.push_back ((Byte) value);
        command.mask.push_back ((Byte) kMaxByte);

        if (isWords)
        {
            command.values.push_back ((Byte) (value >> 8));
            command.mask.push_back ((Byte) kMaxByte);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildDump
//
//  `db addr [l count]` and `db start end`, as a range. The count is in the
//  command's own units, and the default is WinDbg's: 128 bytes whatever the
//  unit. A range running past $FFFF stops there. No address continues where
//  the last dump stopped.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildDump (
    const std::string              & name,
    const Tokens                   & args,
    const IDebugExpressionContext  & context,
    Build                          & build)
{
    static constexpr uint32_t  kDefaultBytes = 0x80;
    static constexpr uint32_t  kLastAddress  = 0xFFFF;
    uint32_t                   unit          = (name == "dw") ? 2 : (name == "dd") ? 4 : 1;
    uint32_t                   address       = 0;
    uint32_t                   last          = 0;
    uint32_t                   count         = 0;
    std::string                length;
    size_t                     next          = 1;



    build.command.verb = DebugVerb::DumpMemory;

    if (args.empty())
    {
        return true;
    }

    if (!TryEvaluate (args[0], context, address, build.error))
    {
        return false;
    }

    last = address + kDefaultBytes - 1;

    if (TrySplitLength (args, 1, length, next))
    {
        if (!TryEvaluate (length, context, count, build.error))
        {
            return false;
        }

        if (count == 0)
        {
            build.error = "A length must be at least 1.";
            return false;
        }

        last = (uint32_t) std::min<uint64_t> ((uint64_t) address + (uint64_t) count * unit - 1, kLastAddress);
    }
    else if (args.size() > 1)
    {
        if (!TryEvaluate (args[1], context, last, build.error))
        {
            return false;
        }

        next = 2;
    }

    if (last < address)
    {
        build.error = std::format ("{} needs an end at or after its start.", name);
        return false;
    }

    if (next < args.size())
    {
        build.error = std::format ("{} takes an address and a length, as {} 2000 l20.", name, name);
        return false;
    }

    if (address > kLastAddress)
    {
        build.error = std::format ("{} is outside $0000-$FFFF.", args[0]);
        return false;
    }

    build.command.a1    = (Word) address;
    build.command.a2    = (Word) std::min (last, kLastAddress);
    build.command.hasA1 = true;
    build.command.hasA2 = true;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildAccess
//
//  `ba r1|w1|e1 addr [IF expr]`: a watchpoint on reads and writes (WinDbg's
//  r covers both) or on writes, or for execute a breakpoint. A size above
//  one covers that many bytes; it is decimal, as WinDbg reads it.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildAccess (const Tokens & args, const IDebugExpressionContext & context, Build & build)
{
    std::string  access = args.empty() ? std::string() : ToLower (args[0]);
    std::string  size   = access.size() > 1 ? access.substr (1) : std::string();
    Tokens       target;



    build.error = "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.";

    if (args.size() < 2 || size.empty() || size.find_first_not_of ("0123456789") != std::string::npos || size == "0")
    {
        return false;
    }

    if (access[0] != 'r' && access[0] != 'w' && access[0] != 'e')
    {
        return false;
    }

    build.error.clear();
    target.assign (args.begin() + 1, args.end());

    if (access[0] == 'e')
    {
        return TryBuildAddressBreakpoint ("ba", target, size, context, build);
    }

    build.command.verb = (access[0] == 'r') ? DebugVerb::SetMemoryWatchpoint : DebugVerb::SetWriteWatchpoint;
    return TryBuildWatchpoint (target, size, context, build);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildBreakpoint
//
//  `bp addr` and `bp file:line`, the source line with or without WinDbg's
//  backquotes. An IF clause may follow the address.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildBreakpoint (
    const Tokens                   & args,
    const std::string              & rest,
    const IDebugExpressionContext  & context,
    Build                          & build)
{
    if (args.empty())
    {
        build.error = "bp takes an address or a source line.";
        return false;
    }

    return TryBuildAddressBreakpoint ("bp", Split (StripBackquotes (rest)), "1", context, build);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildAddressBreakpoint
//
//  An address, a range of size bytes, a source line, or a comparison
//  against PC, with an IF clause after anything but the comparison.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildAddressBreakpoint (
    const std::string              & name,
    Tokens                           tokens,
    const std::string              & size,
    const IDebugExpressionContext  & context,
    Build                          & build)
{
    DebugCommand  & command      = build.command;
    bool            isComparison = !tokens.empty() && std::string_view ("<>=!").find (tokens[0][0]) != std::string_view::npos;



    command.verb = DebugVerb::SetBreakpoint;

    if (!isComparison && !AppleWinParser::TryParseIfClause (tokens, command, build.error, context.GetNumberSyntax()))
    {
        return false;
    }

    if (isComparison && HasIf (tokens))
    {
        build.error = std::format ("{} with a comparison does not take IF.", name);
        return false;
    }

    if (!isComparison && tokens.size() > 1)
    {
        build.error = std::format ("{} takes one argument, not {}.", name, tokens.size());
        return false;
    }

    if (isComparison)
    {
        command.verb = DebugVerb::SetConditionalBreakpoint;
        return AppleWinParser::TryParseCondition ("PC", tokens, 0, command, build.error, context.GetNumberSyntax());
    }

    if (tokens.empty())
    {
        build.error = std::format ("{} needs an address.", name);
        return false;
    }

    if (size == "1" && AppleWinParser::TryParseSourceLine (tokens[0], context, command))
    {
        return true;
    }

    return TryBuildAccessRange (tokens[0], size, context, command, build.error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildWatchpoint
//
//  An address or a range of size bytes, an optional BEFORE or AFTER (AFTER
//  by default), and an optional IF clause.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildWatchpoint (
    Tokens                           tokens,
    const std::string              & size,
    const IDebugExpressionContext  & context,
    Build                          & build)
{
    DebugCommand  & command = build.command;



    if (!AppleWinParser::TryParseIfClause (tokens, command, build.error, context.GetNumberSyntax()))
    {
        return false;
    }

    if (tokens.empty())
    {
        build.error = "ba needs an address.";
        return false;
    }

    command.text = "AFTER";

    if (tokens.size() > 1)
    {
        command.text = ToUpper (tokens[1]);

        if (command.text != "BEFORE" && command.text != "AFTER")
        {
            build.error = "A watchpoint's mode is BEFORE or AFTER.";
            return false;
        }
    }

    return TryBuildAccessRange (tokens[0], size, context, command, build.error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildAccessRange
//
//  One address, or with a size above one, the range of that many bytes from
//  it. size is decimal digits.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildAccessRange (
    const std::string              & text,
    const std::string              & size,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr size_t    kMaxDigits = 9;
    static constexpr uint32_t  kMaxWord   = 0xFFFF;
    uint32_t                   length     = 0;



    if (size == "1")
    {
        return AppleWinParser::TryParseRange (text, context, command, error);
    }

    length = (size.size() > kMaxDigits) ? kMaxWord + 1 : (uint32_t) std::stoul (size);

    if (length > kMaxWord)
    {
        error = std::format ("{} is outside $0000-$FFFF.", size);
        return false;
    }

    command.hasA1 = true;
    command.hasA2 = true;

    if (!AppleWinParser::TryEvaluate (text, context, command.a1, error))
    {
        return false;
    }

    return TrySetLength (command.a1, length, command, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TrySetLength
//
//  a2 as the last of length bytes from address. A range that runs past
//  $FFFF is an error.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TrySetLength (Word address, uint32_t length, DebugCommand & command, std::string & error)
{
    static constexpr uint32_t  kLastAddress = 0xFFFF;



    if (length == 0)
    {
        error = "A range length must be at least 1.";
        return false;
    }

    if ((uint32_t) address + length - 1 > kLastAddress)
    {
        error = std::format ("The range runs past $FFFF from ${:04X}.", address);
        return false;
    }

    command.a2 = (Word) (address + length - 1);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildRegister
//
//  `r` shows the registers; `r a=41` sets one. WinDbg's names for the stack
//  pointer and the flags are sp and fl.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildRegister (const std::string & rest, const IDebugExpressionContext & context, Build & build)
{
    static constexpr std::pair<const char *, const char *>  kNames[] =
    {
        { "a", "A" }, { "x", "X" }, { "y", "Y" }, { "sp", "S" }, { "pc", "PC" }, { "fl", "P" }, { "p", "P" },
    };
    DebugCommand  & command = build.command;
    std::string     joined  = rest;
    size_t          split   = 0;
    std::string     name;
    std::string     value;



    command.verb = DebugVerb::ShowRegisters;

    if (rest.empty())
    {
        return true;
    }

    std::replace (joined.begin(), joined.end(), '=', ' ');
    split = joined.find_first_of (" \t");
    name  = ToLower (joined.substr (0, split));
    value = (split == std::string::npos) ? std::string() : joined.substr (split);

    for (const auto & [windbg, register6502] : kNames)
    {
        if (name != windbg)
        {
            continue;
        }

        //  A name alone shows the registers, that one among them.
        if (value.find_first_not_of (" \t") == std::string::npos)
        {
            return true;
        }

        command.verb  = DebugVerb::SetRegister;
        command.text  = register6502;
        command.hasA1 = true;
        return AppleWinParser::TryEvaluate (value, context, command.a1, build.error);
    }

    build.error = std::format ("{} is not a register. The registers are a, x, y, sp, pc and fl.", name);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildText
//
//  `ea addr "text"`: the text's characters as bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildText (
    const Tokens                   & args,
    const std::string              & rest,
    const IDebugExpressionContext  & context,
    Build                          & build)
{
    static constexpr Byte  kAllBits = 0xFF;
    DebugCommand         & command  = build.command;
    size_t                 open     = rest.find ('"');
    size_t                 close    = (open == std::string::npos) ? std::string::npos : rest.find ('"', open + 1);



    if (args.empty() || close == std::string::npos || close == open + 1)
    {
        build.error = "ea takes an address and quoted text: ea 400 \"HELLO\".";
        return false;
    }

    command.verb  = DebugVerb::EnterBytes;
    command.hasA1 = true;

    if (!AppleWinParser::TryEvaluate (args[0], context, command.a1, build.error))
    {
        return false;
    }

    for (size_t i = open + 1; i < close; i++)
    {
        command.values.push_back ((Byte) rest[i]);
        command.mask.push_back (kAllBits);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryBuildRange
//
//  `f addr l n bytes`, `s addr l n bytes` and `m src l n dest`, whose
//  length is required.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryBuildRange (
    const std::string              & name,
    const Tokens                   & args,
    const std::string              & rest,
    const IDebugExpressionContext  & context,
    Build                          & build)
{
    DebugCommand  & command = build.command;
    std::string     length;
    size_t          next    = 0;
    Word            count   = 0;



    if (name == "s" && !args.empty() && args[0].starts_with ('-'))
    {
        build.isDeferred = true;
        build.error      = std::format ("s {} is not available yet. Use s addr l n bytes to search for bytes.", args[0]);
        return false;
    }

    build.error = (name == "m") ? "m takes a source, a length and a destination: m 300 l10 2000."
                                : std::format ("{} takes an address, a length and bytes: {} 2000 l20 00.", name, name);

    if (args.empty() || !TrySplitLength (args, 1, length, next) || next >= args.size() || (name == "m" && next + 1 != args.size()))
    {
        return false;
    }

    build.error.clear();
    command.verb  = (name == "m") ? DebugVerb::MoveMemory : (name == "f") ? DebugVerb::FillMemory : DebugVerb::SearchMemory;
    command.hasA1 = true;
    command.hasA2 = true;

    if (name == "m")
    {
        command.hasA3 = true;

        if (!AppleWinParser::TryEvaluate (args[next], context, command.a3, build.error))
        {
            return false;
        }
    }

    if (!AppleWinParser::TryEvaluate (args[0], context, command.a1, build.error) ||
        !AppleWinParser::TryEvaluate (length,  context, count,      build.error) ||
        !TrySetLength (command.a1, count, command, build.error))
    {
        return false;
    }

    if (name == "f")
    {
        return TryAddValues (args, next, false, context, command, build.error);
    }

    return name == "m" || AppleWinParser::TryParseSearchItems (GetTail (rest, next), context, command, build.error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TrySplitLength
//
//  `l20` or `l 20` at args[first], leaving the count in length and the
//  index after it in next.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TrySplitLength (const Tokens & args, size_t first, std::string & length, size_t & next)
{
    std::string  token = (first < args.size()) ? ToLower (args[first]) : std::string();



    if (token.size() > 1 && token[0] == 'l')
    {
        length = args[first].substr (1);
        next   = first + 1;
        return true;
    }

    if (token == "l" && first + 1 < args.size())
    {
        length = args[first + 1];
        next   = first + 2;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryEvaluate
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryEvaluate (
    const std::string              & text,
    const IDebugExpressionContext  & context,
    uint32_t                       & value,
    std::string                    & error)
{
    int32_t  result = 0;
    HRESULT  hr     = DebugExpressionEvaluator::ParseAndEvaluate (text, context, result, error);



    if (FAILED (hr))
    {
        return false;
    }

    if (result < 0)
    {
        error = std::format ("{} is negative.", text);
        return false;
    }

    value = (uint32_t) result;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::StripBackquotes
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::StripBackquotes (const std::string & text)
{
    std::string  result;



    for (char ch : text)
    {
        if (ch != '`')
        {
            result += ch;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::Split
//
////////////////////////////////////////////////////////////////////////////////

WinDbgParser::Tokens WinDbgParser::Split (const std::string & text)
{
    Tokens              tokens;
    std::istringstream  stream (text);
    std::string         token;



    while (stream >> token)
    {
        tokens.push_back (token);
    }

    return tokens;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::ToLower
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::ToLower (const std::string & text)
{
    std::string  result = text;



    for (char & ch : result)
    {
        ch = (char) tolower ((unsigned char) ch);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::ToUpper (const std::string & text)
{
    std::string  result = text;



    for (char & ch : result)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::HasIf
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::HasIf (const Tokens & tokens)
{
    for (const std::string & token : tokens)
    {
        if (ToUpper (token) == "IF")
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::GetTail
//
//  The text from its token at index first to the end, as typed, so the
//  spaces inside quoted text are kept.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::GetTail (const std::string & text, size_t first)
{
    size_t  at = text.find_first_not_of (" \t");



    for (size_t i = 0; i < first && at != std::string::npos; i++)
    {
        at = text.find_first_of (" \t", at);
        at = (at == std::string::npos) ? at : text.find_first_not_of (" \t", at);
    }

    return (at == std::string::npos) ? std::string() : text.substr (at);
}
