#include "Pch.h"

#include "Debugger/GSSquaredParser.h"

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kCommands
//
//  GSSquared's command words as its debugger's own table lists them, then
//  Casso's four additions. GSSquared steps and resumes by key in its window;
//  `s`, `o`, `r` and `g` are how a script does the same here.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr GSSquaredCommand  s_kCommands[] =
{
    { "set",     nullptr },
    { "load",    nullptr },
    { "save",    nullptr },
    { "move",    nullptr },
    { "verify",  "VERIFY is accepted by GSSquared and does nothing there either." },
    { "watch",   nullptr },
    { "nowatch", nullptr },
    { "help",    nullptr },
    { "bp",      nullptr },
    { "bpd",     nullptr },
    { "bpi",     nullptr },
    { "nobp",    nullptr },
    { "list",    nullptr },
    { "l",       nullptr },
    { "map",     "MAP needs a IIgs: it shows the IIgs memory map, and this machine has no IIgs MMU." },
    { "debug",   "DEBUG needs a device panel in the debugger window, and this build has none." },
    { "nodebug", "NODEBUG needs a device panel in the debugger window, and this build has none." },
    { "sload",   nullptr },
    { "sclear",  nullptr },
    { "slookup", nullptr },
    { "m",       "M needs a IIgs: it sets the 65816's accumulator width, and this machine has a 6502." },
    { "x",       "X needs a IIgs: it sets the 65816's index width, and this machine has a 6502." },
    { "video",   "VIDEO is not available here: the emulator window shows the screen." },
    { "novideo", "NOVIDEO is not available here: the emulator window shows the screen." },
    { "s",       nullptr },
    { "o",       nullptr },
    { "r",       nullptr },
    { "g",       nullptr },
};





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::Parse
//
//  A Casso command GSSquared reaches goes to AppleWinParser as it stands.
//  Otherwise the first
//  token's form decides -- an address, a range, a deposit, or `addrl` -- and
//  failing that its word.
//
////////////////////////////////////////////////////////////////////////////////

GSSquaredParseResult GSSquaredParser::Parse (const std::string & line, const IDebugExpressionContext & context)
{
    GSSquaredParseResult  result;
    Line                  current = { Split (line), result, context };



    if (current.tokens.empty())
    {
        return result;
    }

    if (CommandModeHelp::IsCassoCommandReachable (CommandMode::GSSquared, current.tokens[0]))
    {
        ParseAppleWin (current, line, current.tokens[0]);
    }
    else if (!TryParseForm (current) && !TryParseWord (current))
    {
        result.status = ParseStatus::Unknown;
        result.error  = std::format ("{} is not a command.", current.tokens[0]);
    }

    if (result.status != ParseStatus::Ok)
    {
        result.commands.clear();
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryParseForm
//
//  A first token that is not a word: `addr:` or `addr:byte` deposits,
//  `first.last` dumps, `addrl` lists, and a bare address examines one byte.
//  GSSquared splits any token ending in `l`; here only one whose front is an
//  address, so no command word is ever cut in two.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryParseForm (Line & line)
{
    const std::string  & first  = line.tokens[0];
    size_t               colon  = first.find (':');
    std::string          front  = first.size() > 1 ? first.substr (0, first.size() - 1) : std::string();
    char                 last   = (char) tolower ((unsigned char) first.back());
    Tokens               values;
    Word                 value  = 0;
    Word                 second = 0;



    if (colon != std::string::npos)
    {
        values.assign (line.tokens.begin() + 1, line.tokens.end());

        if (colon + 1 < first.size())
        {
            values.insert (values.begin(), first.substr (colon + 1));
        }

        ParseDeposit (line, first.substr (0, colon), values);
        return true;
    }

    if (first.find ('.') != std::string::npos)
    {
        if (line.tokens.size() > 1)
        {
            SetInvalid (line, "A dump takes one range, first.last.");
        }
        else if (TryParseRange (line, first, value, second))
        {
            ParseAppleWin (line, std::format ("D {}:{}", FormatHex (value), FormatHex (second)), first);
        }

        return true;
    }

    if (last == 'l' && !front.empty() && front.find_first_not_of ("0123456789ABCDEFabcdef/") == std::string::npos)
    {
        if (line.tokens.size() > 1)
        {
            SetInvalid (line, "addrl takes nothing after it.");
        }
        else if (TryParseAddress (line, front, value))
        {
            ParseAppleWin (line, std::format ("U {}", FormatHex (value)), "l");
        }

        return true;
    }

    if (first.find_first_not_of ("0123456789ABCDEFabcdef/") != std::string::npos)
    {
        return false;
    }

    if (line.tokens.size() > 1)
    {
        SetInvalid (line, "An address to examine takes nothing after it.");
    }
    else if (TryParseAddress (line, first, value))
    {
        ParseAppleWin (line, std::format ("D {}:{}", FormatHex (value), FormatHex (value)), first);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryParseWord
//
//  A command word, with case ignored. A word GSSquared has and Casso cannot
//  carry out is reported with its reason rather than as unknown.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryParseWord (Line & line)
{
    std::string               word  = ToLower (line.tokens[0]);
    const GSSquaredCommand  * entry = nullptr;
    size_t                    count = line.tokens.size();
    Word                      value = 0;
    Word                      last  = 0;
    Word                      dest  = 0;



    for (const GSSquaredCommand & command : s_kCommands)
    {
        if (word == command.name)
        {
            entry = &command;
        }
    }

    if (entry == nullptr)
    {
        return false;
    }

    if (entry->reason != nullptr)
    {
        SetNotAvailable (line, entry->reason);
    }
    else if (word == "set")
    {
        if (count < 2)
        {
            SetInvalid (line, "SET takes an address and one or more bytes.");
            return true;
        }

        ParseDeposit (line, line.tokens[1], Tokens (line.tokens.begin() + 2, line.tokens.end()));
    }
    else if (word == "move")
    {
        if (count != 3 || line.tokens[1].find ('.') == std::string::npos)
        {
            SetInvalid (line, "MOVE takes a range and a destination: move first.last dest.");
        }
        else if (TryParseRange (line, line.tokens[1], value, last) && TryParseAddress (line, line.tokens[2], dest))
        {
            ParseAppleWin (line, std::format ("M {} {}:{}", FormatHex (dest), FormatHex (value), FormatHex (last)), word);
        }
    }
    else if (word == "l" || word == "list")
    {
        if (count > 2)
        {
            SetInvalid (line, "L takes an address or nothing.");
        }
        else if (count == 1)
        {
            ParseAppleWin (line, "U", word);
        }
        else if (TryParseAddress (line, line.tokens[1], value))
        {
            ParseAppleWin (line, std::format ("U {}", FormatHex (value)), word);
        }
    }
    else if (word == "bp")                          { ParseBreakpoint      (line); }
    else if (word == "bpd" || word == "bpi")        { ParseDataBreakpoint  (line, word == "bpi"); }
    else if (word == "nobp")                        { ParseClearBreakpoint (line); }
    else if (word == "watch" || word == "nowatch")  { ParseWatch           (line); }
    else if (word == "load" || word == "save")      { ParseFile            (line, word == "load"); }
    else if (word == "sload" || word == "slookup" || word == "sclear") { ParseSymbols (line, word); }
    else if (word == "help")                        { ParseNoArguments     (line, "HELP"); }
    else if (word == "s")                           { ParseNoArguments     (line, "T"); }
    else if (word == "o")                           { ParseNoArguments     (line, "P"); }
    else if (word == "r")                           { ParseNoArguments     (line, "RTS"); }
    else                                            { ParseNoArguments     (line, "G"); }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseDeposit
//
//  `addr: b b b` and `set addr b b b`. GSSquared takes each value as a byte,
//  so a value of more than two digits is an error here rather than the two
//  bytes MEB would make of it.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseDeposit (Line & line, const std::string & address, const Tokens & values)
{
    static constexpr size_t  kByteDigits = 2;
    Word                     start       = 0;
    Word                     value       = 0;
    std::string              appleWin;



    if (!TryParseAddress (line, address, start))
    {
        return;
    }

    if (values.empty())
    {
        SetInvalid (line, "A deposit takes one or more bytes.");
        return;
    }

    appleWin = std::format ("MEB {}", FormatHex (start));

    for (const std::string & text : values)
    {
        if (!TryParseHex (text, kByteDigits, value))
        {
            SetInvalid (line, std::format ("{} is not a byte. A deposit takes hex bytes, 00-FF.", text));
            return;
        }

        appleWin += std::format (" ${:02X}", value);
    }

    ParseAppleWin (line, appleWin, ToLower (line.tokens[0]) == "set" ? "set" : address + ":");
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseBreakpoint
//
//  `bp` lists; `bp addr` and `bp first.last` set an execution breakpoint,
//  with an optional trailing `IF expression` as AppleWin mode takes it.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseBreakpoint (Line & line)
{
    Word         first  = 0;
    Word         last   = 0;
    std::string  clause;



    if (line.tokens.size() == 1)
    {
        ParseAppleWin (line, "BPL", "bp");
        return;
    }

    if (!TryGetIfClause (line.tokens, 2, clause))
    {
        SetInvalid (line, "BP takes an address or a range, and optionally IF and an expression.");
        return;
    }

    // A GSSquared range uses a period and a bank a slash, so a colon is a
    // source line, file:line, which only the AppleWin BP reads.
    if (line.tokens[1].find (':') != std::string::npos)
    {
        ParseAppleWin (line, std::format ("BP {}{}", line.tokens[1], clause), "bp");
        return;
    }

    if (TryParseRange (line, line.tokens[1], first, last))
    {
        ParseAppleWin (line, std::format ("BP {}{}", FormatRange (first, last), clause), "bp");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseDataBreakpoint
//
//  `bpd addr r|w|rw` and `bpi addr r|w|rw`, each a watchpoint as BPMR, BPMW
//  or BPM sets. An I/O breakpoint is the same watchpoint, limited to the I/O
//  page as GSSquared limits it; the bus already sees every access there.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseDataBreakpoint (Line & line, bool isIo)
{
    static constexpr Word  kIoFirst = 0xC000;
    static constexpr Word  kIoLast  = 0xC0FF;
    std::string            name     = isIo ? "BPI" : "BPD";
    std::string            access;
    std::string            clause;
    std::string            appleWin;
    Word                   first    = 0;
    Word                   last     = 0;



    // GSSquared's bpd always takes an access, so bpd with one argument is
    // AppleWin's BPD: disable the breakpoint with that id.
    if (!isIo && line.tokens.size() == 2)
    {
        ParseAppleWin (line, "BPD " + line.tokens[1], "bpd");
        return;
    }

    if (line.tokens.size() < 3 || !TryGetIfClause (line.tokens, 3, clause))
    {
        SetInvalid (line, std::format ("{} takes an address or a range, then r, w, or rw.", name));
        return;
    }

    access = ToLower (line.tokens[2]);

    if      (access == "r")  { appleWin = "BPMR"; }
    else if (access == "w")  { appleWin = "BPMW"; }
    else if (access == "rw") { appleWin = "BPM";  }
    else
    {
        SetInvalid (line, std::format ("{} is not an access. The accesses are r, w, and rw.", line.tokens[2]));
        return;
    }

    if (!TryParseRange (line, line.tokens[1], first, last))
    {
        return;
    }

    if (isIo && (first < kIoFirst || last > kIoLast))
    {
        SetInvalid (line, "BPI takes an address in the I/O page, $C000-$C0FF.");
        return;
    }

    ParseAppleWin (line, std::format ("{} {}{}", appleWin, FormatRange (first, last), clause), ToLower (name));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseClearBreakpoint
//
//  `nobp N`: an id if some entry has it, and otherwise the address of an
//  execution breakpoint. The number is carried both ways for the session to
//  choose: as a decimal id, the way ids are listed, and as a hex address.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseClearBreakpoint (Line & line)
{
    static constexpr size_t  kAddressDigits = 4;
    static constexpr size_t  kMaxIdDigits   = 9;
    DebugCommand             command;
    std::string              token;
    bool                     isDecimal      = false;
    bool                     hasBank        = false;



    if (line.tokens.size() != 2)
    {
        SetInvalid (line, "NOBP takes a breakpoint id or address.");
        return;
    }

    token     = line.tokens[1];
    isDecimal = token.find_first_not_of ("0123456789") == std::string::npos && token.size() <= kMaxIdDigits;
    hasBank   = token.find ('/') != std::string::npos;

    command.verb       = DebugVerb::ClearBreakpoint;
    command.sourceName = "nobp";
    command.mode       = CommandMode::GSSquared;
    command.text       = token;

    if (hasBank)
    {
        if (!TryParseAddress (line, token, command.a1))
        {
            return;
        }

        command.hasA1 = true;
    }
    else
    {
        command.hasA1 = TryParseHex (token, kAddressDigits, command.a1);
    }

    if (!isDecimal && !command.hasA1)
    {
        SetInvalid (line, std::format ("{} is not a breakpoint id or address.", token));
        return;
    }

    if (isDecimal)
    {
        command.count = (uint32_t) std::stoul (token);
    }

    line.result.commands.push_back (command);
    line.result.status        = ParseStatus::Ok;
    line.result.isIdOrAddress = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseWatch
//
//  `watch` lists, `watch addr` watches one address, and `watch first.last`
//  watches each address of the range, since a Casso watch is one address.
//  `nowatch N` clears one.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseWatch (Line & line)
{
    static constexpr uint32_t  kMaxRange = 0x100;
    bool                       isClear   = ToLower (line.tokens[0]) == "nowatch";
    Word                       first     = 0;
    Word                       last      = 0;



    if (isClear)
    {
        if (line.tokens.size() != 2 || line.tokens[1].find_first_not_of ("0123456789") != std::string::npos)
        {
            SetInvalid (line, "NOWATCH takes a watch id.");
            return;
        }

        ParseAppleWin (line, "WC " + line.tokens[1], "nowatch");
        return;
    }

    if (line.tokens.size() == 1)
    {
        ParseAppleWin (line, "WL", "watch");
        return;
    }

    if (line.tokens.size() > 2)
    {
        SetInvalid (line, "WATCH takes an address or a range.");
        return;
    }

    if (!TryParseRange (line, line.tokens[1], first, last))
    {
        return;
    }

    if ((uint32_t) (last - first) + 1 > kMaxRange)
    {
        SetInvalid (line, std::format ("A watch range covers at most {} addresses.", kMaxRange));
        return;
    }

    for (uint32_t address = first; address <= last; ++address)
    {
        ParseAppleWin (line, std::format ("W {}", FormatHex ((Word) address)), "watch");

        if (line.result.status != ParseStatus::Ok)
        {
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseFile
//
//  `load "file" addr` and `save "file" first.last`, as BLOAD and BSAVE. The
//  quotes GSSquared requires are optional here.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseFile (Line & line, bool isLoad)
{
    std::string  file;
    Word         first = 0;
    Word         last  = 0;



    if (line.tokens.size() != 3)
    {
        SetInvalid (line, isLoad ? "LOAD takes a file name and an address." : "SAVE takes a file name and a range.");
        return;
    }

    file = Unquote (line.tokens[1]);

    if (isLoad)
    {
        if (TryParseAddress (line, line.tokens[2], first))
        {
            ParseAppleWin (line, std::format ("BLOAD \"{}\" {}", file, FormatHex (first)), "load");
        }

        return;
    }

    if (TryParseRange (line, line.tokens[2], first, last))
    {
        ParseAppleWin (line, std::format ("BSAVE \"{}\" {}:{}", file, FormatHex (first), FormatHex (last)), "save");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseSymbols
//
//  `sload "file"`, `slookup addr` and `sclear`, as SYM LOAD, SYM addr and
//  SYM CLEAR against the main table. These keep SYM as the command's name,
//  because the name is what selects the symbol table.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseSymbols (Line & line, const std::string & word)
{
    size_t  count = line.tokens.size();
    Word    value = 0;



    if (word == "sclear")
    {
        ParseNoArguments (line, "SYM CLEAR");
    }
    else if (count != 2)
    {
        SetInvalid (line, word == "sload" ? "SLOAD takes a file name." : "SLOOKUP takes an address.");
    }
    else if (word == "sload")
    {
        ParseAppleWin (line, std::format ("SYM LOAD \"{}\"", Unquote (line.tokens[1])), std::string());
    }
    else if (TryParseAddress (line, line.tokens[1], value))
    {
        ParseAppleWin (line, std::format ("SYM {}", FormatHex (value)), std::string());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseNoArguments
//
//  A word that takes nothing: help, sclear, and the step and run additions.
//  The command keeps its AppleWin name only where that name selects
//  something, which is SYM's case.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseNoArguments (Line & line, const std::string & appleWin)
{
    std::string  word = ToLower (line.tokens[0]);



    if (line.tokens.size() > 1)
    {
        SetInvalid (line, std::format ("{} takes nothing after it.", word));
        return;
    }

    ParseAppleWin (line, appleWin, appleWin.starts_with ("SYM") ? std::string() : word);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseAppleWin
//
//  The AppleWin line with the same effect, parsed as AppleWin mode parses
//  it and added to the result. word, when given, replaces the AppleWin name,
//  so a reply and an error quote what was typed.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseAppleWin (Line & line, const std::string & appleWin, const std::string & word)
{
    AppleWinParseResult  parsed = AppleWinParser::Parse (appleWin, line.context);



    line.result.status = parsed.status;
    line.result.error  = parsed.error;

    if (parsed.status != ParseStatus::Ok)
    {
        return;
    }

    if (!word.empty())
    {
        parsed.command.sourceName = word;
    }

    if (parsed.command.verb != DebugVerb::SetMode)
    {
        parsed.command.mode = CommandMode::GSSquared;
    }

    line.result.commands.push_back (parsed.command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryParseAddress
//
//  Hex, one to four digits, or `bank/addr`. Bank 00 is the address alone;
//  any other bank is refused, since a bank above 00 exists only on a IIgs.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryParseAddress (Line & line, const std::string & token, Word & address)
{
    static constexpr size_t  kAddressDigits = 4;
    static constexpr size_t  kBankDigits    = 2;
    size_t                   slash          = token.find ('/');
    Word                     bank           = 0;



    if (slash == std::string::npos)
    {
        if (!TryParseHex (token, kAddressDigits, address))
        {
            SetInvalid (line, std::format ("{} is not an address. An address is one to four hex digits.", token));
            return false;
        }

        return true;
    }

    if (!TryParseHex (token.substr (0, slash), kBankDigits, bank) || !TryParseHex (token.substr (slash + 1), kAddressDigits, address))
    {
        SetInvalid (line, std::format ("{} is not an address. A bank-qualified address is bank/addr in hex.", token));
        return false;
    }

    if (bank != 0)
    {
        SetNotAvailable (line, std::format ("{:02X}/{:04X} is in bank {:02X}. Only bank 00 exists on this machine.", bank, address, bank));
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryParseRange
//
//  `first.last`, inclusive, where first may carry a bank; or a lone address,
//  which is a range of one.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryParseRange (Line & line, const std::string & token, Word & first, Word & last)
{
    static constexpr size_t  kAddressDigits = 4;
    size_t                   period         = token.find ('.');



    if (period == std::string::npos)
    {
        if (!TryParseAddress (line, token, first))
        {
            return false;
        }

        last = first;
        return true;
    }

    if (!TryParseAddress (line, token.substr (0, period), first))
    {
        return false;
    }

    if (!TryParseHex (token.substr (period + 1), kAddressDigits, last))
    {
        SetInvalid (line, std::format ("{} is not a range. A range is first.last in hex.", token));
        return false;
    }

    if (last < first)
    {
        SetInvalid (line, std::format ("{} ends before it starts.", token));
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryParseHex
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryParseHex (const std::string & text, size_t maxDigits, Word & value)
{
    bool  isHex = !text.empty() && text.size() <= maxDigits &&
                  text.find_first_not_of ("0123456789ABCDEFabcdef") == std::string::npos;



    if (isHex)
    {
        value = (Word) std::stoul (text, nullptr, 16);
    }

    return isHex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryGetIfClause
//
//  Nothing from first on, or `IF` and an expression, which is kept as text
//  for AppleWinParser to parse. Anything else there is an error.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryGetIfClause (const Tokens & tokens, size_t first, std::string & clause)
{
    if (tokens.size() <= first)
    {
        return true;
    }

    if (ToLower (tokens[first]) != "if")
    {
        return false;
    }

    clause = " IF";

    for (size_t i = first + 1; i < tokens.size(); ++i)
    {
        clause += " " + tokens[i];
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::SetInvalid
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::SetInvalid (Line & line, const std::string & error)
{
    line.result.status = ParseStatus::Invalid;
    line.result.error  = error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::SetNotAvailable
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::SetNotAvailable (Line & line, const std::string & error)
{
    line.result.status = ParseStatus::NotAvailable;
    line.result.error  = error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::FormatHex
//
////////////////////////////////////////////////////////////////////////////////

std::string GSSquaredParser::FormatHex (Word value)
{
    return std::format ("${:04X}", value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::FormatRange
//
//  A range as AppleWin writes one, or the address alone for a range of one.
//
////////////////////////////////////////////////////////////////////////////////

std::string GSSquaredParser::FormatRange (Word first, Word last)
{
    return (first == last) ? FormatHex (first) : std::format ("{}:{}", FormatHex (first), FormatHex (last));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ToLower
//
////////////////////////////////////////////////////////////////////////////////

std::string GSSquaredParser::ToLower (const std::string & text)
{
    std::string  lower (text);



    for (char & ch : lower)
    {
        ch = (char) tolower ((unsigned char) ch);
    }

    return lower;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::Unquote
//
////////////////////////////////////////////////////////////////////////////////

std::string GSSquaredParser::Unquote (const std::string & text)
{
    bool  isQuoted = text.size() >= 2 && text.front() == '"' && text.back() == '"';



    return isQuoted ? text.substr (1, text.size() - 2) : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::Split
//
////////////////////////////////////////////////////////////////////////////////

GSSquaredParser::Tokens GSSquaredParser::Split (const std::string & text)
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
//  GSSquaredParser::GetCommands
//
////////////////////////////////////////////////////////////////////////////////

std::span<const GSSquaredCommand> GSSquaredParser::GetCommands()
{
    return std::span<const GSSquaredCommand> (s_kCommands);
}
