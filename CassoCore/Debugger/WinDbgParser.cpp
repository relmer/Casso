#include "Pch.h"

#include "Debugger/WinDbgParser.h"

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  The WinDbg-mode commands, each with the AppleWin-mode command it is
//  rewritten as. The sweep test reads this table, so a command added here
//  without a matching case there fails it.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr WinDbgCommand s_kCommands[] =
{
    { "t",        "T"     },
    { "p",        "P"     },
    { "gu",       "RTS"   },
    { "g",        "G"     },
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
//  `!ext.foo`, `$t0`, `$$>a<`) are matched in TryFindExclusion.
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
    { "!handle",     s_kpszThreads    },
    { "!heap",       s_kpszThreads    },
    { "!address",    s_kpszThreads    },
    { "!gle",        s_kpszThreads    },
    { "lm",          s_kpszModules    },
    { ".reload",     s_kpszModules    },
    { ".sympath",    s_kpszModules    },
    { ".srcpath",    s_kpszModules    },
    { "ld",          s_kpszModules    },
    { "sx",          s_kpszExceptions },
    { "sxe",         s_kpszExceptions },
    { "sxd",         s_kpszExceptions },
    { ".lastevent",  s_kpszExceptions },
    { ".exr",        s_kpszExceptions },
    { ".cxr",        s_kpszExceptions },
    { ".ecxr",       s_kpszExceptions },
    { "!analyze",    s_kpszExceptions },
    { "!pte",        s_kpszKernel     },
    { "!pool",       s_kpszKernel     },
    { "!irql",       s_kpszKernel     },
    { "!process",    s_kpszKernel     },
    { "!thread",     s_kpszKernel     },
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
    { "dqs",         s_kpszTypes      },
    { "dpa",         s_kpszTypes      },
    { "dpu",         s_kpszTypes      },
    { "dpp",         s_kpszTypes      },
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
    { "$$>a<",       s_kpszScripting  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::Parse
//
//  `!name` is an engine command. `?` needs no space before its expression,
//  as in WinDbg. `*` and `$$` begin a comment, and a line beginning `;` is
//  one too, as a script's is. Numbers are rewritten from WinDbg's prefixes
//  to AppleWin's (`0x300` to `$300`, `0n10` to `#10`) except inside quoted
//  text.
//
////////////////////////////////////////////////////////////////////////////////

WinDbgParseResult WinDbgParser::Parse (const std::string & line, const IDebugExpressionContext & context)
{
    WinDbgParseResult          result;
    AppleWinParseResult        parsed;
    Rewrite                    rewrite;
    std::string                name;
    std::string                rest;
    size_t                     first     = line.find_first_not_of (" \t");
    size_t                     restFrom  = 0;
    const WinDbgExclusion    * exclusion = nullptr;



    if (first == std::string::npos || IsComment (line.substr (first)))
    {
        return result;
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
        SetExclusion (name, *exclusion, result);
        return result;
    }

    if (HasSeparator (line.substr (first)))
    {
        result.status = ParseStatus::NotAvailable;
        result.error  = "Several commands on one line are not available yet. Put each command on a line of its own.";
        return result;
    }

    if (!TryRewrite (name, rest, context, rewrite))
    {
        result.status = rewrite.isDeferred ? ParseStatus::NotAvailable
                      : rewrite.error.empty() ? ParseStatus::Unknown
                      :                         ParseStatus::Invalid;
        result.error  = rewrite.error.empty() ? std::format ("{} is not a WinDbg-mode command.", name) : rewrite.error;
        return result;
    }

    parsed         = AppleWinParser::Parse (rewrite.appleWinLine, context);
    result.status  = parsed.status;
    result.command = parsed.command;
    result.error   = parsed.error;

    if (parsed.status == ParseStatus::Ok)
    {
        ApplyRewrite (name, rewrite, result.command);
    }

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
//  WinDbgParser::IsComment
//
//  WinDbg's `*` comment line and `$$` comment. A `$$<` or `$$>` runs a
//  script and is left to the exclusions.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::IsComment (const std::string & line)
{
    bool  isScript = line.starts_with ("$$<") || line.starts_with ("$$>");



    return line.starts_with ('*') || line.starts_with (';') || (line.starts_with ("$$") && !isScript);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::HasSeparator
//
//  A `;` outside quotes, which WinDbg reads as the end of one command and the
//  start of the next.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::HasSeparator (const std::string & line)
{
    char  quote = '\0';



    for (char ch : line)
    {
        if (quote == '\0' && (ch == '"' || ch == '\''))
        {
            quote = ch;
        }
        else if (ch == quote)
        {
            quote = '\0';
        }
        else if (quote == '\0' && ch == ';')
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryParseEngine
//
//  `!name ...`, or `! name ...`, for a Casso command WinDbg reaches
//  (CommandModeHelp), parsed as AppleWin mode parses the same line without
//  the `!`. An excluded `!` command replies with its family.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryParseEngine (const std::string & line, const IDebugExpressionContext & context, WinDbgParseResult & result)
{
    std::string                text;
    Tokens                     tokens;
    std::string                bang;
    const WinDbgExclusion    * exclusion = nullptr;
    AppleWinParseResult        parsed;



    if (line.empty() || line[0] != '!')
    {
        return false;
    }

    text   = line.substr (1);
    tokens = Split (text);

    if (tokens.empty())
    {
        return false;
    }

    bang = "!" + ToLower (tokens[0]);

    if (TryFindExclusion (bang, exclusion))
    {
        SetExclusion (bang, *exclusion, result);
        return true;
    }

    result.status = ParseStatus::Unknown;

    if (AppleWinCommandTable::Find (tokens[0]) == nullptr)
    {
        result.error = std::format ("!{} is not a command.", tokens[0]);
        return true;
    }

    if (!CommandModeHelp::IsCassoCommandReachable (CommandMode::WinDbg, tokens[0]))
    {
        result.error = std::format ("!{} does not run in WinDbg mode. Use .help {} to see where it runs.", tokens[0], tokens[0]);
        return true;
    }

    parsed         = AppleWinParser::Parse (NormalizeEngineLine (Trim (text)), context);
    result.status  = parsed.status;
    result.command = parsed.command;
    result.error   = parsed.error;
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

    if      (name.starts_with ('~') || name.starts_with ('|'))       { family = s_kpszThreads;    }
    else if (name.starts_with ("sx"))                                { family = s_kpszExceptions; }
    else if (name.starts_with ("!ext."))                             { family = s_kpszScripting;  }
    else if (name.starts_with ("$$<") || name.starts_with ("$$>"))   { family = s_kpszScripting;  }
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
//  WinDbgParser::SetExclusion
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgParser::SetExclusion (const std::string & name, const WinDbgExclusion & exclusion, WinDbgParseResult & result)
{
    result.status = ParseStatus::NotAvailable;
    result.label  = "no meaning on this machine";
    result.error  = std::format ("{} belongs to WinDbg's {} commands.", name, exclusion.family);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::ApplyRewrite
//
//  What the AppleWin line could not carry. The command keeps the WinDbg
//  word as its name, so a reply or an error says `eb` where the user typed
//  it; x keeps SYM's, which is how the symbol handlers choose the tables.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgParser::ApplyRewrite (const std::string & name, const Rewrite & rewrite, DebugCommand & command)
{
    if (name != "x")
    {
        command.sourceName = name;
    }

    if (rewrite.startPc.has_value())
    {
        command.a1    = *rewrite.startPc;
        command.hasA1 = true;
    }

    if (rewrite.sourceFile.has_value())
    {
        command.text = *rewrite.sourceFile;
    }

    if (rewrite.lookup.has_value())
    {
        command.verb = DebugVerb::LookupSymbol;
        command.text = *rewrite.lookup;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewrite
//
//  The AppleWin-mode line for one WinDbg command. False with no error for a
//  name that is not a WinDbg-mode command.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewrite (
    const std::string              & name,
    const std::string              & rest,
    const IDebugExpressionContext  & context,
    Rewrite                        & rewrite)
{
    static constexpr const char * kRuns[]        = { "t", "p", "gu", "g", "pa", "ta" };
    static constexpr const char * kBreakpoints[] = { "bp", "ba", "bl", "bc", "bd", "be" };
    static constexpr const char * kMemory[]      = { "db", "dw", "dd", "da", "eb", "ew", "ea", "f", "s", "m" };



    if (std::find (std::begin (kRuns), std::end (kRuns), name) != std::end (kRuns))
    {
        return TryRewriteRun (name, Split (NormalizeExpression (rest)), context, rewrite);
    }

    if (std::find (std::begin (kBreakpoints), std::end (kBreakpoints), name) != std::end (kBreakpoints))
    {
        return TryRewriteBreakpoints (name, rest, context, rewrite);
    }

    if (std::find (std::begin (kMemory), std::end (kMemory), name) != std::end (kMemory))
    {
        return TryRewriteMemory (name, rest, context, rewrite);
    }

    return TryRewriteOther (name, rest, rewrite);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteRun
//
//  t and p take a count and `=addr` to start from; g takes `=addr` to start
//  from or one address to stop at; pa and ta take the address to stop at.
//  WinDbg's g stops at any of several addresses, which a Casso run cannot,
//  so a second address is an error rather than a skip range.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteRun (
    const std::string              & name,
    const Tokens                   & args,
    const IDebugExpressionContext  & context,
    Rewrite                        & rewrite)
{
    std::string  & line = rewrite.appleWinLine;
    size_t         next = 0;



    if (name == "gu")
    {
        rewrite.error = "gu takes no arguments.";
        line          = "RTS";
        return args.empty();
    }

    if (name == "pa" || name == "ta")
    {
        rewrite.error = std::format ("{} takes the address to stop at.", name);
        line          = "G " + Join (args, 0);
        return args.size() == 1 && !args[0].starts_with ('=');
    }

    if (!TryRewriteStart (args, context, rewrite, next))
    {
        return false;
    }

    if (name == "g" && rewrite.startPc.has_value())
    {
        rewrite.isDeferred = next < args.size();
        rewrite.error      = "g =addr with an address to stop at is not available yet. Use g =addr, or r pc=addr and then g addr.";
        line               = std::format ("${:04X}G", *rewrite.startPc);
        rewrite.startPc.reset();
        return next == args.size();
    }

    if (name == "g")
    {
        rewrite.error = "g takes one address to stop at.";
        line          = "G " + Join (args, next);
        return args.size() - next <= 1;
    }

    rewrite.error = std::format ("{} takes a count, and =addr to start from: {} =300 3.", name, name);
    line          = (name == "t" ? "T " : "P ") + Join (args, next);
    return args.size() - next <= 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteStart
//
//  WinDbg's `=addr` start address, written `=300` or `= 300`, ahead of any
//  other argument. next is the index of the argument after it.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteStart (const Tokens & args, const IDebugExpressionContext & context, Rewrite & rewrite, size_t & next)
{
    std::string  text;
    uint32_t     start = 0;



    next = 0;

    if (args.empty() || !args[0].starts_with ('='))
    {
        return true;
    }

    text = args[0].substr (1);
    next = 1;

    if (text.empty() && args.size() > 1)
    {
        text = args[1];
        next = 2;
    }

    if (text.empty())
    {
        rewrite.error = "= takes the address to start from, as =300.";
        return false;
    }

    if (!TryEvaluateAddress (text, context, start, rewrite.error))
    {
        return false;
    }

    rewrite.startPc = (Word) start;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteBreakpoints
//
//  bp, ba, bl, bc, bd and be. bl takes ids to list, which the formatter
//  picks from the whole list; bc, bd and be take one id or *, since a Casso
//  line acts on one.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteBreakpoints (
    const std::string              & name,
    const std::string              & rest,
    const IDebugExpressionContext  & context,
    Rewrite                        & rewrite)
{
    static constexpr std::pair<const char *, const char *>  kIdCommands[] =
    {
        { "bc", "BPC" }, { "bd", "BPD" }, { "be", "BPE" },
    };
    Tokens  args = Split (NormalizeExpression (rest));



    if (name == "bp")
    {
        return TryRewriteBreakpoint (rest, rewrite);
    }

    if (name == "ba")
    {
        return TryRewriteAccess (args, context, rewrite);
    }

    if (name == "bl")
    {
        rewrite.error        = "bl takes ids to list, or * for all.";
        rewrite.appleWinLine = "BPL";

        return std::all_of (args.begin(), args.end(), [] (const std::string & id)
        {
            return id == "*" || id.find_first_not_of ("0123456789") == std::string::npos;
        });
    }

    for (const auto & [windbg, appleWin] : kIdCommands)
    {
        if (name == windbg)
        {
            rewrite.error        = std::format ("{} takes one id, or * for all.", name);
            rewrite.appleWinLine = std::format ("{} {}", appleWin, Join (args, 0));
        }
    }

    return args.size() <= 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteBreakpoint
//
//  `bp addr [IF expr]`, and a source line with or without WinDbg's
//  backquotes. An address written with spaces around an operator, as
//  `bp start + 3`, is one expression. WinDbg's pass count, command string
//  and options have no Casso equivalent and say so.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteBreakpoint (const std::string & rest, Rewrite & rewrite)
{
    size_t       open       = rest.find ('`');
    size_t       close      = (open == std::string::npos) ? std::string::npos : rest.find ('`', open + 1);
    Tokens       tokens;
    std::string  expression;
    size_t       ifAt       = 0;
    size_t       next       = 1;



    rewrite.error = "bp takes an address or a source line.";

    if (open != std::string::npos)
    {
        return close != std::string::npos && Trim (rest.substr (0, open)).empty() &&
               TryRewriteSourceLine (rest.substr (open + 1, close - open - 1), rest.substr (close + 1), rewrite);
    }

    tokens = Split (NormalizeExpression (rest));

    if (tokens.empty())
    {
        return false;
    }

    ifAt = (size_t) (std::find_if (tokens.begin(), tokens.end(), [] (const std::string & token) { return ToLower (token) == "if"; }) - tokens.begin());

    if (tokens[0].starts_with ('/'))
    {
        rewrite.isDeferred = true;
        rewrite.error      = std::format ("bp {} is not available yet. Casso breakpoints take no options.", tokens[0]);
        return false;
    }

    expression = tokens[0];

    while (next < ifAt && (IsOperatorEdge (expression.back()) || IsOperatorEdge (tokens[next].front())))
    {
        expression += tokens[next++];
    }

    if (next < ifAt)
    {
        rewrite.isDeferred = true;
        rewrite.error      = tokens[next].starts_with ('"')
                           ? "bp with a command string is not available yet. Use bp addr IF expr to stop only when a condition holds."
                           : "bp with a pass count is not available yet. Use bp addr IF expr to stop only when a condition holds.";
        return false;
    }

    rewrite.appleWinLine = "BP " + expression + (ifAt < tokens.size() ? " " + Join (tokens, ifAt) : std::string());
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteSourceLine
//
//  ``bp `file:line` ``: always a source line, whatever the file is called,
//  even a name that would evaluate as an address or one holding a space. A
//  module before `!` is dropped, since a 6502 program has none. The file is
//  set on the command after parsing, so the AppleWin line carries a stand-in
//  that cannot be read as an address.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteSourceLine (const std::string & inner, const std::string & after, Rewrite & rewrite)
{
    std::string  text   = Trim (inner);
    size_t       bang   = text.rfind ('!');
    size_t       colon  = 0;
    std::string  file;
    std::string  number;



    text   = (bang == std::string::npos) ? text : text.substr (bang + 1);
    colon  = text.rfind (':');
    file   = (colon == std::string::npos) ? text : Trim (text.substr (0, colon));
    number = (colon == std::string::npos) ? std::string() : Trim (text.substr (colon + 1));

    if (colon != std::string::npos && file.empty())
    {
        rewrite.isDeferred = true;
        rewrite.error      = "bp `:line` is not available yet. Give the file, as bp `main.s:12`.";
        return false;
    }

    if (number.empty() || number.find_first_not_of ("0123456789") != std::string::npos)
    {
        rewrite.error = "A backquoted source line is a file and a line number, as bp `main.s:12`.";
        return false;
    }

    rewrite.sourceFile   = file;
    rewrite.appleWinLine = "BP (:" + number + (Trim (after).empty() ? std::string() : " " + Trim (after));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteAccess
//
//  `ba r1|w1|e1 addr [IF expr] [BEFORE|AFTER]`: a read or write watchpoint,
//  or for execute a breakpoint. The size is decimal, as in WinDbg, and a
//  size above one covers that many bytes, stopping at $FFFF as a dump does.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteAccess (const Tokens & args, const IDebugExpressionContext & context, Rewrite & rewrite)
{
    static constexpr size_t    kMaxSizeDigits = 5;
    static constexpr uint32_t  kLastAddress   = 0xFFFF;
    std::string                access         = args.empty() ? std::string() : ToLower (args[0]);
    std::string                size           = access.size() > 1 ? access.substr (1) : std::string();
    uint32_t                   address        = 0;
    uint32_t                   count          = 0;
    uint32_t                   last           = 0;
    std::string                target;



    rewrite.error = "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.";

    if (args.size() < 2 || size.empty() || size.size() > kMaxSizeDigits || size.find_first_not_of ("0123456789") != std::string::npos)
    {
        return false;
    }

    count = (uint32_t) std::stoul (size);

    if (count == 0 || (access[0] != 'r' && access[0] != 'w' && access[0] != 'e'))
    {
        return false;
    }

    if (!TryEvaluateAddress (args[1], context, address, rewrite.error))
    {
        return false;
    }

    last   = std::min (address + count - 1, kLastAddress);
    target = (last == address) ? std::format ("${:04X}", address) : std::format ("${:04X}:${:04X}", address, last);

    rewrite.appleWinLine = (access[0] == 'r') ? "BPMR " : (access[0] == 'w') ? "BPMW " : "BP ";
    rewrite.appleWinLine += target + (args.size() > 2 ? " " + Join (args, 2) : std::string());
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteMemory
//
//  The dumps, the edits, and fill, search and move.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteMemory (
    const std::string              & name,
    const std::string              & rest,
    const IDebugExpressionContext  & context,
    Rewrite                        & rewrite)
{
    std::string  normalized = NormalizeExpression (rest);



    if (name == "ea")
    {
        return TryRewriteText (rest, rewrite);
    }

    if (name == "eb" || name == "ew")
    {
        return TryRewriteEnter (name, Split (normalized), context, rewrite);
    }

    if (name == "f" || name == "s" || name == "m")
    {
        return TryRewriteRange (name, normalized, context, rewrite);
    }

    return TryRewriteDump (name, Split (normalized), context, rewrite);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteDump
//
//  `db addr [l count]` and `db start end`, as a D range. The count is in the
//  command's own units, and the default is WinDbg's: 128 bytes whatever the
//  unit. dw and dd show whole units, so an end inside one takes it all. A
//  range running past $FFFF stops there. No address continues where the
//  last dump stopped, as D alone does.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteDump (
    const std::string              & name,
    const Tokens                   & args,
    const IDebugExpressionContext  & context,
    Rewrite                        & rewrite)
{
    static constexpr uint64_t  kDefaultBytes = 0x80;
    static constexpr uint64_t  kLastAddress  = 0xFFFF;
    uint64_t                   unit          = (name == "dw") ? 2 : (name == "dd") ? 4 : 1;
    uint32_t                   address       = 0;
    uint32_t                   end           = 0;
    uint32_t                   count         = 0;
    uint64_t                   last          = 0;
    std::string                length;
    size_t                     next          = 1;



    if (args.empty())
    {
        rewrite.appleWinLine = "D";
        return true;
    }

    if (!TryEvaluateAddress (args[0], context, address, rewrite.error))
    {
        return false;
    }

    last = address + kDefaultBytes - 1;

    if (TrySplitLength (args, 1, length, next))
    {
        if (!TryEvaluate (length, context, count, rewrite.error))
        {
            return false;
        }

        if (count == 0)
        {
            rewrite.error = "A length must be at least 1.";
            return false;
        }

        last = address + (uint64_t) count * unit - 1;
    }
    else if (args.size() > 1)
    {
        if (!TryEvaluateAddress (args[1], context, end, rewrite.error))
        {
            return false;
        }

        if (end < address)
        {
            rewrite.error = std::format ("The end, ${:04X}, is before the start, ${:04X}.", end, address);
            return false;
        }

        last = address + ((end - address) / unit + 1) * unit - 1;
        next = 2;
    }

    if (next < args.size())
    {
        rewrite.error = std::format ("{} takes an address and a length, as {} 2000 l20.", name, name);
        return false;
    }

    rewrite.appleWinLine = std::format ("D ${:04X}:${:04X}", address, std::min (last, kLastAddress));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteEnter
//
//  `eb addr bytes` and `ew addr words`. eb takes bytes only, as WinDbg does;
//  a larger value is an error rather than two bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteEnter (
    const std::string              & name,
    const Tokens                   & args,
    const IDebugExpressionContext  & context,
    Rewrite                        & rewrite)
{
    static constexpr uint32_t  kMaxByte = 0xFF;
    uint32_t                   value    = 0;



    if (args.size() < 2)
    {
        rewrite.error = (name == "eb") ? "eb takes an address and bytes: eb 300 a9 41."
                                       : "ew takes an address and words: ew 300 1234.";
        return false;
    }

    for (size_t i = 1; name == "eb" && i < args.size(); i++)
    {
        if (!TryEvaluate (args[i], context, value, rewrite.error))
        {
            return false;
        }

        if (value > kMaxByte)
        {
            rewrite.error = std::format ("{} is not a byte.", args[i]);
            return false;
        }
    }

    rewrite.appleWinLine = (name == "eb" ? "MEB " : "MEW ") + Join (args, 0);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteText
//
//  `ea addr "text"`: the text's characters as bytes, as MEB takes them, with
//  the high bit clear and spaces kept. Only the address is normalized.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteText (const std::string & rest, Rewrite & rewrite)
{
    size_t       open    = rest.find ('"');
    size_t       close   = (open == std::string::npos) ? std::string::npos : rest.find ('"', open + 1);
    Tokens       before  = Split (rest.substr (0, open == std::string::npos ? rest.size() : open));
    std::string  after   = (close == std::string::npos) ? std::string() : rest.substr (close + 1);



    rewrite.error = "ea takes an address and quoted text: ea 400 \"HELLO\".";

    if (before.size() != 1 || close == std::string::npos || close == open + 1 || !Trim (after).empty())
    {
        return false;
    }

    rewrite.appleWinLine = "MEB " + NormalizeExpression (before[0]);

    for (size_t i = open + 1; i < close; i++)
    {
        rewrite.appleWinLine += std::format (" ${:02X}", (Byte) rest[i]);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteRange
//
//  `f addr l n bytes`, `s addr l n items` and `m src l n dest`, whose length
//  is required. A range running past $FFFF stops there, as a dump does. The
//  bytes and items keep their spacing, so quoted text searches as typed.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteRange (
    const std::string              & name,
    const std::string              & normalized,
    const IDebugExpressionContext  & context,
    Rewrite                        & rewrite)
{
    static constexpr uint64_t  kLastAddress = 0xFFFF;
    Tokens                     args         = Split (normalized);
    std::string                length;
    size_t                     next         = 0;
    uint32_t                   address      = 0;
    uint32_t                   count        = 0;
    std::string                range;
    std::string                tail;



    if (name == "s" && !args.empty() && args[0].starts_with ('-'))
    {
        rewrite.isDeferred = true;
        rewrite.error      = std::format ("s {} is not available yet. Use s addr l n bytes to search for bytes.", args[0]);
        return false;
    }

    rewrite.error = (name == "m") ? "m takes a source, a length and a destination: m 300 l10 2000."
                                  : std::format ("{} takes an address, a length and bytes: {} 2000 l20 00.", name, name);

    if (args.empty() || !TrySplitLength (args, 1, length, next) || next >= args.size())
    {
        return false;
    }

    if (name == "m" && next + 1 != args.size())
    {
        return false;
    }

    if (!TryEvaluateAddress (args[0], context, address, rewrite.error) || !TryEvaluate (length, context, count, rewrite.error))
    {
        return false;
    }

    if (count == 0)
    {
        rewrite.error = "A range length must be at least 1.";
        return false;
    }

    range = std::format ("${:04X}:${:04X}", address, std::min ((uint64_t) address + count - 1, kLastAddress));
    tail  = GetTail (normalized, next);

    rewrite.appleWinLine = (name == "m") ? std::format ("M {} {}", args[next], range)
                         : (name == "f") ? std::format ("F {} {}", range, tail)
                         :                 std::format ("S {} {}", range, NormalizeSearchItems (tail));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteOther
//
//  Registers, disassembly, symbols, the call stack, evaluation, source and
//  help. False with no error for a name that is none of them.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteOther (const std::string & name, const std::string & rest, Rewrite & rewrite)
{
    std::string    normalized = NormalizeExpression (rest);
    Tokens         args       = Split (normalized);
    std::string  & line       = rewrite.appleWinLine;



    if      (name == "r")   { return TryRewriteRegister    (rest, rewrite); }
    else if (name == "u")   { return TryRewriteDisassemble (args, rewrite); }
    else if (name == "x")   { return TryRewriteLookup      (NormalizeNumbers (rest), rewrite); }
    else if (name == "l+s") { line = "SRC ON";  }
    else if (name == "l-s") { line = "SRC OFF"; }
    else if (name == ".help")
    {
        line = "HELP " + (rest.starts_with ('!') ? rest.substr (1) : rest);
    }
    else if (name == "?" || name == ".formats")
    {
        rewrite.error = std::format ("{} needs an expression.", name);
        line          = "CALC " + normalized;
        return !args.empty();
    }
    else if (name == "k")
    {
        rewrite.error = "k takes no arguments. Use !calls mode to choose how the chain is found.";
        line          = "CALLS";
        return args.empty();
    }
    else if (name == "lsa")
    {
        rewrite.isDeferred = !args.empty();
        rewrite.error      = "lsa with a line is not available yet. Use lsa alone to show the source line at PC.";
        line               = "SRC";
        return args.empty();
    }
    else if (name == "wt")
    {
        rewrite.isDeferred = true;
        rewrite.error      = "wt is not available yet. Use !history or !profile to record what a run executes.";
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
//  WinDbgParser::TryRewriteRegister
//
//  `r` shows the registers, `r a` one of them, and `r a=41` sets one.
//  WinDbg's names for the stack pointer and the flags are sp and fl, and it
//  may write a register @a. The formatter picks the one register `r a` asks
//  for from the whole set.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteRegister (const std::string & rest, Rewrite & rewrite)
{
    static constexpr std::pair<const char *, const char *>  kNames[] =
    {
        { "a", "A" }, { "x", "X" }, { "y", "Y" }, { "sp", "S" }, { "pc", "PC" }, { "fl", "P" }, { "p", "P" },
    };
    std::string  text  = Trim (rest);
    size_t       split = text.find_first_of ("= \t");
    std::string  name  = ToLower (text.substr (0, split));
    std::string  value = (split == std::string::npos) ? std::string() : Trim (text.substr (split));



    rewrite.appleWinLine = "R";

    if (text.empty())
    {
        return true;
    }

    name  = name.starts_with ('@') ? name.substr (1) : name;
    value = value.starts_with ('=') ? Trim (value.substr (1)) : value;

    for (const auto & [windbg, applewin] : kNames)
    {
        if (name != windbg)
        {
            continue;
        }

        if (value.find (',') != std::string::npos)
        {
            rewrite.error = "r sets one register at a time, as r a=41.";
            return false;
        }

        rewrite.appleWinLine = value.empty() ? std::string ("R") : std::format ("R {} {}", applewin, NormalizeExpression (value));
        return true;
    }

    rewrite.error = std::format ("{} is not a register. The registers are a, x, y, sp, pc and fl.", name);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteDisassemble
//
//  `u` continues, `u addr` disassembles from addr, and `u start end` the
//  instructions between. WinDbg's length counts instructions, which a U
//  range cannot give.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteDisassemble (const Tokens & args, Rewrite & rewrite)
{
    std::string  length;
    size_t       next   = 0;



    rewrite.error = "u takes an address, or a start and an end, or none to continue.";

    if (args.size() == 2 && TrySplitLength (args, 1, length, next))
    {
        rewrite.isDeferred = true;
        rewrite.error      = "u with a length is not available yet. Use u start end.";
        return false;
    }

    rewrite.appleWinLine = (args.size() == 2) ? std::format ("U {}:{}", args[0], args[1]) : "U " + Join (args, 0);
    return args.size() <= 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteLookup
//
//  `x pattern` looks symbols up and does nothing else, even when the
//  pattern is a word SYM would take as a subcommand (CLEAR, LOAD). WinDbg's
//  `module!` qualifier is dropped, since every table is searched.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteLookup (const std::string & rest, Rewrite & rewrite)
{
    std::string  pattern = Trim (rest);
    size_t       bang    = pattern.rfind ('!');



    pattern = (bang == std::string::npos) ? pattern : Trim (pattern.substr (bang + 1));

    rewrite.appleWinLine = "SYM";

    if (!pattern.empty())
    {
        rewrite.lookup = pattern;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TrySplitLength
//
//  `l20` or `l 20` at args[first], leaving the count in length and the
//  index after it in next. The count takes WinDbg's prefixes as any number
//  does, so `l0x20` is 32.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TrySplitLength (const Tokens & args, size_t first, std::string & length, size_t & next)
{
    std::string  token = (first < args.size()) ? ToLower (args[first]) : std::string();



    if (token.size() > 1 && token[0] == 'l')
    {
        length = NormalizeExpression (args[first].substr (1));
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
//  WinDbgParser::TryEvaluateAddress
//
//  A value that must be an address, $0000-$FFFF.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryEvaluateAddress (
    const std::string              & text,
    const IDebugExpressionContext  & context,
    uint32_t                       & value,
    std::string                    & error)
{
    static constexpr uint32_t  kLastAddress = 0xFFFF;



    if (!TryEvaluate (text, context, value, error))
    {
        return false;
    }

    if (value > kLastAddress)
    {
        error = std::format ("{} is outside $0000-$FFFF.", text);
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::NormalizeNumbers
//
//  WinDbg's `0x` hex and `0n` decimal prefixes as AppleWin's `$` and `#`,
//  where the prefix starts a number rather than sitting inside a name or
//  quoted text. A bare number is hex in both, so it needs nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::NormalizeNumbers (const std::string & text)
{
    std::string  result;
    size_t       i        = 0;
    char         quote    = '\0';
    bool         isStart  = false;
    char         prefix   = 0;



    while (i < text.size())
    {
        if (quote == '\0' && (text[i] == '"' || text[i] == '\''))
        {
            quote = text[i];
        }
        else if (text[i] == quote)
        {
            quote = '\0';
        }

        isStart = i == 0 || !(isalnum ((unsigned char) text[i - 1]) || text[i - 1] == '_' || text[i - 1] == '$');
        prefix  = (i + 1 < text.size()) ? (char) tolower ((unsigned char) text[i + 1]) : 0;

        if (quote == '\0' && isStart && text[i] == '0' && i + 2 < text.size() &&
            ((prefix == 'x' && isxdigit ((unsigned char) text[i + 2])) || (prefix == 'n' && isdigit ((unsigned char) text[i + 2]))))
        {
            result += (prefix == 'x') ? '$' : '#';
            i      += 2;
            continue;
        }

        result += text[i];
        i++;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::NormalizeExpression
//
//  The numbers, and WinDbg's `@` register syntax as the registers' own
//  names: `@pc`, `@a`, `@sp` and `@fl`. `@` and digits stays a search
//  result.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::NormalizeExpression (const std::string & text)
{
    static constexpr std::pair<const char *, const char *>  kRegisters[] =
    {
        { "pc", "PC" }, { "sp", "S" }, { "fl", "P" }, { "a", "A" }, { "x", "X" }, { "y", "Y" }, { "p", "P" },
    };
    std::string  numbers = NormalizeNumbers (text);
    std::string  result;
    size_t       i       = 0;
    char         quote   = '\0';
    const char * own     = nullptr;
    size_t       length  = 0;



    while (i < numbers.size())
    {
        quote = (quote == '\0' && (numbers[i] == '"' || numbers[i] == '\'')) ? numbers[i] : (numbers[i] == quote) ? '\0' : quote;
        own   = nullptr;

        for (const auto & [windbg, name] : kRegisters)
        {
            size_t  end = i + 1 + strlen (windbg);



            if (own == nullptr && quote == '\0' && numbers[i] == '@' && end <= numbers.size() &&
                ToLower (numbers.substr (i + 1, strlen (windbg))) == windbg &&
                (end == numbers.size() || !(isalnum ((unsigned char) numbers[end]) || numbers[end] == '_')))
            {
                own    = name;
                length = strlen (windbg);
            }
        }

        if (own != nullptr)
        {
            result += own;
            i      += 1 + length;
            continue;
        }

        result += numbers[i];
        i++;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::NormalizeEngineLine
//
//  An engine command's line takes WinDbg's number prefixes in its values,
//  but not in its text: ECHO's words are left as typed, and so is any word
//  holding a path separator or a period, which is a file name. BUDGET counts
//  in decimal, so `0n1000` and `0x100` become its digits.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::NormalizeEngineLine (const std::string & text)
{
    Tokens       tokens = Split (text);
    std::string  word   = tokens.empty() ? std::string() : ToLower (tokens[0]);
    std::string  result;
    std::string  token;
    char         quote  = '\0';
    size_t       i      = 0;



    if (word == "echo")
    {
        return text;
    }

    if (word == "budget" && tokens.size() > 1)
    {
        return tokens[0] + " " + GetDecimalBudget (tokens[1]) + (tokens.size() > 2 ? " " + Join (tokens, 2) : std::string());
    }

    while (i <= text.size())
    {
        bool  isEnd = i == text.size() || (quote == '\0' && isspace ((unsigned char) text[i]));



        if (!isEnd)
        {
            quote  = (quote == '\0' && (text[i] == '"' || text[i] == '\'')) ? text[i] : (text[i] == quote) ? '\0' : quote;
            token += text[i];
            i++;
            continue;
        }

        result += (token.find_first_of ("/\\.") != std::string::npos) ? token : NormalizeNumbers (token);
        result += (i < text.size()) ? std::string (1, text[i]) : std::string();
        token.clear();
        i++;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::GetDecimalBudget
//
//  `0n1000` as `1000` and `0x100` as `256`; any other token as it is.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::GetDecimalBudget (const std::string & token)
{
    static constexpr size_t  kPrefixLength = 2;
    static constexpr size_t  kMaxHexDigits = 8;
    static constexpr int     kHexBase      = 16;
    std::string              lower         = ToLower (token);
    std::string              digits        = lower.size() > kPrefixLength ? lower.substr (kPrefixLength) : std::string();



    if (lower.starts_with ("0n") && !digits.empty() && digits.find_first_not_of ("0123456789") == std::string::npos)
    {
        return digits;
    }

    if (lower.starts_with ("0x") && !digits.empty() && digits.size() <= kMaxHexDigits &&
        digits.find_first_not_of ("0123456789abcdef") == std::string::npos)
    {
        return std::to_string (std::stoul (digits, nullptr, kHexBase));
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::NormalizeSearchItems
//
//  A search item written with a prefix, `0x41` or `0n65`, is the byte or
//  word its value is. S reads an item longer than two digits as a word, and
//  `$41` is three, so each becomes its bare hex digits: two for a byte and
//  four for a word. Quoted text is left alone.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::NormalizeSearchItems (const std::string & text)
{
    static constexpr uint32_t  kMaxByte    = 0xFF;
    static constexpr uint32_t  kMaxWord    = 0xFFFF;
    static constexpr size_t    kMaxDigits  = 5;
    static constexpr int       kHexBase    = 16;
    static constexpr int       kDecimal    = 10;
    std::string                result;
    std::string                token;
    std::string                digits;
    size_t                     i           = 0;
    char                       quote       = '\0';
    uint32_t                   value       = 0;
    bool                       isHex       = false;
    bool                       isDecimal   = false;



    while (i <= text.size())
    {
        bool  isEnd = i == text.size() || (quote == '\0' && isspace ((unsigned char) text[i]));



        if (!isEnd)
        {
            quote  = (quote == '\0' && (text[i] == '"' || text[i] == '\'')) ? text[i] : (text[i] == quote) ? '\0' : quote;
            token += text[i];
            i++;
            continue;
        }

        digits    = (token.size() > 1 && token.size() <= kMaxDigits + 1) ? token.substr (1) : std::string();
        isHex     = token.starts_with ('$') && !digits.empty() && digits.find_first_not_of ("0123456789ABCDEFabcdef") == std::string::npos;
        isDecimal = token.starts_with ('#') && !digits.empty() && digits.find_first_not_of ("0123456789") == std::string::npos;

        if (isHex || isDecimal)
        {
            value = (uint32_t) std::stoul (digits, nullptr, isHex ? kHexBase : kDecimal);
            token = (value <= kMaxByte) ? std::format ("{:02X}", value)
                  : (value <= kMaxWord) ? std::format ("{:04X}", value)
                  :                       token;
        }

        result += token;
        result += (i < text.size()) ? std::string (1, text[i]) : std::string();
        token.clear();
        i++;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::IsOperatorEdge
//
//  Whether a character joins the words either side of it into one
//  expression, as the `+` in `start + 3` does.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::IsOperatorEdge (char ch)
{
    static constexpr std::string_view  kOperators = "+-*/%&|^<>=!(";



    return kOperators.find (ch) != std::string_view::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::GetTail
//
//  text from its index-th word onward, spacing kept.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::GetTail (const std::string & text, size_t index)
{
    size_t  at = text.find_first_not_of (" \t");



    for (size_t word = 0; word < index && at != std::string::npos; word++)
    {
        at = text.find_first_of (" \t", at);
        at = (at == std::string::npos) ? at : text.find_first_not_of (" \t", at);
    }

    return (at == std::string::npos) ? std::string() : text.substr (at);
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
//  WinDbgParser::Trim
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::Trim (const std::string & text)
{
    size_t  first = text.find_first_not_of (" \t");
    size_t  last  = text.find_last_not_of  (" \t");



    return (first == std::string::npos) ? std::string() : text.substr (first, last - first + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::Join
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::Join (const Tokens & tokens, size_t first)
{
    std::string  result;



    for (size_t i = first; i < tokens.size(); i++)
    {
        result += (result.empty() ? "" : " ") + tokens[i];
    }

    return result;
}





