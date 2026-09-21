#include "Pch.h"

#include "Debugger/WinDbgParser.h"

#include "Debugger/AppleWinCommandTable.h"
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
//  as in WinDbg. Numbers are rewritten from WinDbg's prefixes to AppleWin's
//  (`0x300` to `$300`, `0n10` to `#10`) except inside `ea`'s quoted text.
//
////////////////////////////////////////////////////////////////////////////////

WinDbgParseResult WinDbgParser::Parse (const std::string & line, const IDebugExpressionContext & context)
{
    WinDbgParseResult          result;
    AppleWinParseResult        parsed;
    Rewrite                    rewrite;
    Tokens                     tokens;
    std::string                name;
    std::string                rest;
    size_t                     first     = line.find_first_not_of (" \t");
    size_t                     restFrom  = 0;
    const WinDbgExclusion    * exclusion = nullptr;



    if (first == std::string::npos)
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
        result.status = ParseStatus::NotAvailable;
        result.label  = "no meaning on this machine";
        result.error  = std::format ("{} belongs to WinDbg's {} commands.", name, exclusion->family);
        return result;
    }

    if (name != "ea")
    {
        rest = NormalizeNumbers (rest);
    }

    tokens = Split (rest);

    if (!TryRewrite (name, tokens, rest, context, rewrite))
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
//  `!name ...` for an engine command, parsed as AppleWin mode parses the
//  same line without the `!`, so every command in the Engine family is
//  reachable here with no change to this parser. An excluded `!` command is left to Parse,
//  which reports its family.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryParseEngine (const std::string & line, const IDebugExpressionContext & context, WinDbgParseResult & result)
{
    std::string                text;
    Tokens                     tokens;
    const WinDbgExclusion    * exclusion = nullptr;
    AppleWinParseResult        parsed;



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

    if (!AppleWinCommandTable::IsEngineCommand (tokens[0]))
    {
        result.status = ParseStatus::Unknown;
        result.error  = std::format ("!{} is not an engine command.", tokens[0]);
        return true;
    }

    parsed         = AppleWinParser::Parse (NormalizeNumbers (text), context);
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
//  WinDbgParser::TryRewrite
//
//  The AppleWin-mode line for one WinDbg command. False with no error for a
//  name that is not a WinDbg-mode command.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewrite (
    const std::string              & name,
    const Tokens                   & args,
    const std::string              & rest,
    const IDebugExpressionContext  & context,
    Rewrite                        & rewrite)
{
    std::string  & line = rewrite.appleWinLine;



    if      (name == "t")        { line = "T " + rest; }
    else if (name == "p")        { line = "P " + rest; }
    else if (name == "gu")       { line = "RTS"; }
    else if (name == "g")        { line = "G " + rest; }
    else if (name == "bl")       { line = "BPL"; }
    else if (name == "bc")       { line = "BPC " + rest; }
    else if (name == "bd")       { line = "BPD " + rest; }
    else if (name == "be")       { line = "BPE " + rest; }
    else if (name == "eb")       { line = "MEB " + rest; }
    else if (name == "ew")       { line = "MEW " + rest; }
    else if (name == "x")        { line = "SYM " + rest; }
    else if (name == "?")        { line = "CALC " + rest; }
    else if (name == ".formats") { line = "CALC " + rest; }
    else if (name == "l+s")      { line = "SRC ON"; }
    else if (name == "l-s")      { line = "SRC OFF"; }
    else if (name == ".help")    { line = "HELP " + rest; }
    else if (name == "bp")       { return TryRewriteBreakpoint (args, rest, rewrite); }
    else if (name == "ba")       { return TryRewriteAccess     (args, rewrite); }
    else if (name == "r")        { return TryRewriteRegister   (rest, rewrite); }
    else if (name == "ea")       { return TryRewriteText       (args, rest, rewrite); }
    else if (name == "db" || name == "dw" || name == "dd" || name == "da")
    {
        return TryRewriteDump (name, args, context, rewrite);
    }
    else if (name == "f" || name == "s" || name == "m")
    {
        return TryRewriteRange (name, args, rewrite);
    }
    else if (name == "pa" || name == "ta")
    {
        rewrite.error = std::format ("{} takes the address to stop at.", name);
        line          = "G " + rest;
        return args.size() == 1;
    }
    else if (name == "u")
    {
        rewrite.error = "u takes one address, or none to continue.";
        line          = "U " + rest;
        return args.size() <= 1;
    }
    else if (name == "k")
    {
        rewrite.error = "k takes no arguments; !calls mode chooses how the chain is found.";
        line          = "CALLS";
        return args.empty();
    }
    else if (name == "lsa")
    {
        rewrite.isDeferred = !args.empty();
        rewrite.error      = "lsa with a line is not available yet; lsa alone shows the source line at PC.";
        line               = "SRC";
        return args.empty();
    }
    else if (name == "wt")
    {
        rewrite.isDeferred = true;
        rewrite.error      = "wt is not available yet; !history and !profile record what a run executes.";
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
//  WinDbgParser::TryRewriteDump
//
//  `db addr [l count]` and `db start end`, as a D range. The count is in the
//  command's own units, and the default is WinDbg's: 128 bytes whatever the
//  unit. A range running past $FFFF stops there. No address continues where
//  the last dump stopped, as D alone does.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteDump (
    const std::string              & name,
    const Tokens                   & args,
    const IDebugExpressionContext  & context,
    Rewrite                        & rewrite)
{
    static constexpr uint32_t  kDefaultBytes = 0x80;
    static constexpr uint32_t  kLastAddress  = 0xFFFF;
    uint32_t                   unit          = (name == "dw") ? 2 : (name == "dd") ? 4 : 1;
    uint32_t                   address       = 0;
    uint32_t                   last          = 0;
    uint32_t                   count         = 0;
    std::string                length;
    size_t                     next          = 1;



    if (args.empty())
    {
        rewrite.appleWinLine = "D";
        return true;
    }

    if (!TryEvaluate (args[0], context, address, rewrite.error))
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

        last = address + count * unit - 1;
    }
    else if (args.size() > 1)
    {
        if (!TryEvaluate (args[1], context, last, rewrite.error))
        {
            return false;
        }

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
//  WinDbgParser::TryRewriteAccess
//
//  `ba r1|w1|e1 addr [IF expr]`: a read or write watchpoint, or for execute
//  a breakpoint. A size above one covers that many bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteAccess (const Tokens & args, Rewrite & rewrite)
{
    std::string  access = args.empty() ? std::string() : ToLower (args[0]);
    std::string  size   = access.size() > 1 ? access.substr (1) : std::string();
    std::string  target;



    rewrite.error = "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.";

    if (args.size() < 2 || size.empty() || size.find_first_not_of ("0123456789") != std::string::npos || size == "0")
    {
        return false;
    }

    target = (size == "1") ? args[1] : std::format ("{},{}", args[1], size);

    switch (access[0])
    {
    case 'r': rewrite.appleWinLine = "BPMR "; break;
    case 'w': rewrite.appleWinLine = "BPMW "; break;
    case 'e': rewrite.appleWinLine = "BP ";   break;
    default:  return false;
    }

    rewrite.appleWinLine += target + " " + Join (args, 2);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteBreakpoint
//
//  `bp addr` and `bp file:line`, the source line with or without WinDbg's
//  backquotes. Anything after the address, such as an IF clause, passes on.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteBreakpoint (const Tokens & args, const std::string & rest, Rewrite & rewrite)
{
    if (args.empty())
    {
        rewrite.error = "bp takes an address or a source line.";
        return false;
    }

    rewrite.appleWinLine = "BP " + StripBackquotes (rest);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteRegister
//
//  `r` shows the registers; `r a=41` sets one. WinDbg's names for the stack
//  pointer and the flags are sp and fl.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteRegister (const std::string & rest, Rewrite & rewrite)
{
    static constexpr std::pair<const char *, const char *>  kNames[] =
    {
        { "a", "A" }, { "x", "X" }, { "y", "Y" }, { "sp", "S" }, { "pc", "PC" }, { "fl", "P" }, { "p", "P" },
    };
    std::string  joined = rest;
    size_t       split  = 0;
    std::string  name;



    if (rest.empty())
    {
        rewrite.appleWinLine = "R";
        return true;
    }

    std::replace (joined.begin(), joined.end(), '=', ' ');
    split = joined.find_first_of (" \t");
    name  = ToLower (joined.substr (0, split));

    for (const auto & [windbg, applewin] : kNames)
    {
        if (name == windbg)
        {
            rewrite.appleWinLine = std::format ("R {} {}", applewin, split == std::string::npos ? std::string() : joined.substr (split));
            return true;
        }
    }

    rewrite.error = std::format ("{} is not a register. The registers are a, x, y, sp, pc and fl.", name);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser::TryRewriteText
//
//  `ea addr "text"`: the text's characters as bytes, as MEB takes them.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteText (const Tokens & args, const std::string & rest, Rewrite & rewrite)
{
    size_t  open  = rest.find ('"');
    size_t  close = (open == std::string::npos) ? std::string::npos : rest.find ('"', open + 1);



    if (args.empty() || close == std::string::npos || close == open + 1)
    {
        rewrite.error = "ea takes an address and quoted text: ea 400 \"HELLO\".";
        return false;
    }

    rewrite.appleWinLine = "MEB " + NormalizeNumbers (args[0]);

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
//  `f addr l n bytes`, `s addr l n bytes` and `m src l n dest`, whose
//  length is required.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgParser::TryRewriteRange (const std::string & name, const Tokens & args, Rewrite & rewrite)
{
    std::string  length;
    size_t       next   = 0;
    std::string  range;



    if (name == "s" && !args.empty() && args[0].starts_with ('-'))
    {
        rewrite.isDeferred = true;
        rewrite.error      = std::format ("s {} is not available yet; s addr l n bytes searches for bytes.", args[0]);
        return false;
    }

    rewrite.error = (name == "m") ? "m takes a source, a length and a destination: m 300 l10 2000."
                                  : std::format ("{} takes an address, a length and bytes: {} 2000 l20 00.", name, name);

    if (args.empty() || !TrySplitLength (args, 1, length, next) || next >= args.size())
    {
        return false;
    }

    range = std::format ("{},{}", args[0], length);

    if (name == "m")
    {
        rewrite.appleWinLine = std::format ("M {} {}", args[next], range);
        return next + 1 == args.size();
    }

    rewrite.appleWinLine = std::format ("{} {} {}", name == "f" ? "F" : "S", range, Join (args, next));
    return true;
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
//  WinDbgParser::NormalizeNumbers
//
//  WinDbg's `0x` hex and `0n` decimal prefixes as AppleWin's `$` and `#`,
//  where the prefix starts a number rather than sitting inside a name. A
//  bare number is hex in both, so it needs nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgParser::NormalizeNumbers (const std::string & text)
{
    std::string  result;
    size_t       i        = 0;
    bool         inQuotes = false;
    bool         isStart  = false;
    char         prefix   = 0;



    while (i < text.size())
    {
        inQuotes = (text[i] == '"') ? !inQuotes : inQuotes;
        isStart  = i == 0 || !(isalnum ((unsigned char) text[i - 1]) || text[i - 1] == '_' || text[i - 1] == '$');
        prefix   = (i + 1 < text.size()) ? (char) tolower ((unsigned char) text[i + 1]) : 0;

        if (!inQuotes && isStart && text[i] == '0' && i + 2 < text.size() &&
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




