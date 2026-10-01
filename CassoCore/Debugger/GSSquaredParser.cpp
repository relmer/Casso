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
    { "verify",  "verify is accepted by GSSquared and does nothing there either." },
    { "watch",   nullptr },
    { "nowatch", nullptr },
    { "help",    nullptr },
    { "bp",      nullptr },
    { "bpd",     nullptr },
    { "bpi",     nullptr },
    { "nobp",    nullptr },
    { "list",    nullptr },
    { "l",       nullptr },
    { "map",     "map needs a IIgs: it shows the IIgs memory map, and this machine has no IIgs MMU." },
    { "debug",   nullptr },
    { "nodebug", nullptr },
    { "sload",   nullptr },
    { "sclear",  nullptr },
    { "slookup", nullptr },
    { "m",       "M sets a 65816 register width (the accumulator's) on a IIgs; it does not apply to this machine's CPU." },
    { "x",       "X sets a 65816 register width (the index registers') on a IIgs; it does not apply to this machine's CPU." },
    { "video",   "video is not available here: the emulator window shows the screen." },
    { "novideo", "novideo is not available here: the emulator window shows the screen." },
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
    std::string           described;



    if (current.tokens.empty())
    {
        return result;
    }

    if (CommandModeHelp::IsCassoCommandReachable (CommandMode::GSSquared, current.tokens[0]))
    {
        ParseCassoCommand (current, line);
    }
    else if (!TryParseForm (current) && !TryParseWord (current))
    {
        //  A Casso command GSSquared does not reach is answered as help
        //  answers it, with the same status AppleWin mode gives it.
        if (CommandModeHelp::TryDescribe (CommandMode::GSSquared, current.tokens[0], described))
        {
            result.status = ParseStatus::NotAvailable;
            result.error  = described;
        }
        else
        {
            result.status = ParseStatus::Unknown;
            result.error  = std::format ("{} is not a command.", current.tokens[0]);
        }
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
            AddRange (line, MakeCommand (DebugVerb::DumpMemory, first), value, second, true);
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
            AddRange (line, MakeCommand (DebugVerb::Disassemble, "l"), value, value, false);
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
        AddRange (line, MakeCommand (DebugVerb::DumpMemory, first), value, value, true);
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
            SetInvalid (line, "set takes an address and one or more bytes.");
            return true;
        }

        ParseDeposit (line, line.tokens[1], Tokens (line.tokens.begin() + 2, line.tokens.end()));
    }
    else if (word == "move")
    {
        if (count != 3 || line.tokens[1].find ('.') == std::string::npos)
        {
            SetInvalid (line, "move takes a range and a destination: move first.last dest.");
        }
        else if (TryParseRange (line, line.tokens[1], value, last) && TryParseAddress (line, line.tokens[2], dest))
        {
            ParseMove (line, word, value, last, dest);
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
            AddCommand (line, MakeCommand (DebugVerb::Disassemble, word));
        }
        else if (TryParseAddress (line, line.tokens[1], value))
        {
            AddRange (line, MakeCommand (DebugVerb::Disassemble, word), value, value, false);
        }
    }
    else if (word == "bp")                          { ParseBreakpoint      (line); }
    else if (word == "bpd" || word == "bpi")        { ParseDataBreakpoint  (line, word == "bpi"); }
    else if (word == "nobp")                        { ParseClearBreakpoint (line); }
    else if (word == "watch" || word == "nowatch")  { ParseWatch           (line); }
    else if (word == "load" || word == "save")      { ParseFile            (line, word == "load"); }
    else if (word == "sload" || word == "slookup" || word == "sclear") { ParseSymbols (line, word); }
    else if (word == "debug" || word == "nodebug")  { ParsePanel           (line, word == "nodebug"); }
    else if (word == "help" && line.tokens.size() == 2) { ParseHelp        (line); }
    else if (word == "help")                        { ParseNoArguments     (line, DebugVerb::Help); }
    else if (word == "s")                           { ParseNoArguments     (line, DebugVerb::StepInto); }
    else if (word == "o")                           { ParseNoArguments     (line, DebugVerb::StepOver); }
    else if (word == "r" && count > 1)              { ParseRegister        (line); }
    else if (word == "r")                           { ParseNoArguments     (line, DebugVerb::StepOut); }
    else                                            { ParseNoArguments     (line, DebugVerb::Go); }

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
    static constexpr Byte    kAllBits    = 0xFF;
    Word                     value       = 0;
    DebugCommand             command     = MakeCommand (DebugVerb::EnterBytes, ToLower (line.tokens[0]) == "set" ? "set" : address + ":");



    if (!TryParseAddress (line, address, start))
    {
        return;
    }

    if (values.empty())
    {
        SetInvalid (line, "A deposit takes one or more bytes.");
        return;
    }

    command.a1    = start;
    command.hasA1 = true;

    for (const std::string & text : values)
    {
        if (!TryParseHex (text, kByteDigits, value))
        {
            SetInvalid (line, std::format ("{} is not a byte. A deposit takes hex bytes, 00-FF.", text));
            return;
        }

        command.values.push_back ((Byte) value);
        command.mask.push_back (kAllBits);
    }

    AddCommand (line, command);
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
    DebugCommand  command = MakeCommand (DebugVerb::SetBreakpoint, "bp");
    Word          first   = 0;
    Word          last    = 0;
    std::string   expression;



    if (line.tokens.size() == 1)
    {
        AddCommand (line, MakeCommand (DebugVerb::ListBreakpoints, "bp"));
        return;
    }

    if (!TryGetIfClause (line.tokens, 2, expression))
    {
        SetInvalid (line, "bp takes an address or a range, and optionally IF and an expression.");
        return;
    }

    // A GSSquared range uses a period and a bank a slash, so a colon is a
    // source line, file:line, or an expression range, first:last.
    if (line.tokens[1].find (':') != std::string::npos)
    {
        if (TryParseIfExpression (line, line.tokens.size() > 2, expression, command) && TryParseColonTarget (line, line.tokens[1], command))
        {
            AddCommand (line, command);
        }

        return;
    }

    if (TryParseRange (line, line.tokens[1], first, last) && TryParseIfExpression (line, line.tokens.size() > 2, expression, command))
    {
        AddRange (line, command, first, last, first != last);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryParseColonTarget
//
//  `file:line` sets a breakpoint on a source line: its left side does not
//  evaluate as an address and its right is a decimal line number. Anything
//  else with a colon is a range of two expressions, first:last.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryParseColonTarget (Line & line, const std::string & token, DebugCommand & command)
{
    size_t       colon  = token.rfind (':');
    size_t       split  = token.find (':');
    std::string  error;
    Word         unused = 0;
    uint32_t     number = 0;



    if (colon != 0 && colon + 1 < token.size() && TryParseId (token.substr (colon + 1), number) &&
        !AppleWinParser::TryEvaluate (token.substr (0, colon), line.context, unused, error))
    {
        command.verb  = DebugVerb::SetSourceBreakpoint;
        command.text  = token.substr (0, colon);
        command.count = number;
        return true;
    }

    error.clear();

    if (!AppleWinParser::TryEvaluate (token.substr (0, split), line.context, command.a1, error) ||
        !AppleWinParser::TryEvaluate (token.substr (split + 1), line.context, command.a2, error))
    {
        SetInvalid (line, error);
        return false;
    }

    if (command.a2 < command.a1)
    {
        SetInvalid (line, std::format ("The range ends at ${:04X}, before its start at ${:04X}.", command.a2, command.a1));
        return false;
    }

    command.hasA1 = true;
    command.hasA2 = true;
    return true;
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
    std::string            name     = isIo ? "bpi" : "bpd";
    DebugCommand           command  = MakeCommand (DebugVerb::None, name);
    std::string            access;
    std::string            expression;
    Word                   first    = 0;
    Word                   last     = 0;



    // GSSquared's bpd always takes an access, so bpd with one argument is
    // AppleWin's BPD: disable the breakpoint with that id.
    if (!isIo && line.tokens.size() == 2)
    {
        command.verb = DebugVerb::DisableBreakpoint;

        if (TryParseIdOrAll (line, line.tokens[1], command))
        {
            AddCommand (line, command);
        }

        return;
    }

    if (line.tokens.size() < 3 || !TryGetIfClause (line.tokens, 3, expression))
    {
        SetInvalid (line, std::format ("{} takes an address or a range, then r, w, or rw.", name));
        return;
    }

    access = ToLower (line.tokens[2]);

    if      (access == "r")  { command.verb = DebugVerb::SetReadWatchpoint;   }
    else if (access == "w")  { command.verb = DebugVerb::SetWriteWatchpoint;  }
    else if (access == "rw") { command.verb = DebugVerb::SetMemoryWatchpoint; }
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
        SetInvalid (line, "bpi takes an address in the I/O page, $C000-$C0FF.");
        return;
    }

    if (TryParseIfExpression (line, line.tokens.size() > 3, expression, command))
    {
        command.text = "AFTER";
        AddRange (line, command, first, last, first != last);
    }
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
    DebugCommand             command        = MakeCommand (DebugVerb::ClearBreakpoint, "nobp");
    std::string              token;
    bool                     isDecimal      = false;
    bool                     hasBank        = false;



    if (line.tokens.size() != 2)
    {
        SetInvalid (line, "nobp takes a breakpoint id or address.");
        return;
    }

    token     = line.tokens[1];
    isDecimal = token.find_first_not_of ("0123456789") == std::string::npos && token.size() <= kMaxIdDigits;
    hasBank   = token.find ('/') != std::string::npos;

    command.text = token;

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

    AddCommand (line, command);
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
    DebugCommand               command   = MakeCommand (DebugVerb::ClearWatch, "nowatch");
    Word                       first     = 0;
    Word                       last      = 0;



    if (isClear)
    {
        if (line.tokens.size() != 2 || line.tokens[1].find_first_not_of ("0123456789") != std::string::npos)
        {
            SetInvalid (line, "nowatch takes a watch id.");
            return;
        }

        if (TryParseIdOrAll (line, line.tokens[1], command))
        {
            AddCommand (line, command);
        }

        return;
    }

    if (line.tokens.size() == 1)
    {
        AddCommand (line, MakeCommand (DebugVerb::ListWatches, "watch"));
        return;
    }

    if (line.tokens.size() > 2)
    {
        SetInvalid (line, "watch takes an address or a range.");
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
        AddRange (line, MakeCommand (DebugVerb::AddWatch, "watch"), (Word) address, (Word) address, false);
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
    DebugCommand  command = MakeCommand (isLoad ? DebugVerb::LoadBinary : DebugVerb::SaveBinary, isLoad ? "load" : "save");
    Word          first   = 0;
    Word          last    = 0;



    if (line.tokens.size() != 3)
    {
        SetInvalid (line, isLoad ? "load takes a file name and an address." : "save takes a file name and a range.");
        return;
    }

    command.text = Unquote (line.tokens[1]);

    if (isLoad)
    {
        if (TryParseAddress (line, line.tokens[2], first))
        {
            AddRange (line, command, first, first, false);
        }

        return;
    }

    if (TryParseRange (line, line.tokens[2], first, last))
    {
        AddRange (line, command, first, last, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseSymbols
//
//  `sload "file"`, `slookup addr` and `sclear`, as SYM LOAD, SYM addr and
//  SYM CLEAR. sload and sclear act on the User table, so sclear clears what
//  sload loaded and the ROM symbols stay. Each keeps the word as typed; the table travels as
//  the command's symbolTable.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseSymbols (Line & line, const std::string & word)
{
    size_t  count = line.tokens.size();
    Word    value = 0;



    if (word == "sclear")
    {
        ParseNoArguments (line, DebugVerb::ClearSymbols);
    }
    else if (count != 2)
    {
        SetInvalid (line, word == "sload" ? "sload takes a file name." : "slookup takes an address.");
    }
    else if (word == "sload" && line.tokens[1].find (',', line.tokens[1].rfind ('"') + 1) != std::string::npos)
    {
        SetInvalid (line, "sload takes a file name and no offset.");
    }
    else if (word == "sload")
    {
        AddSymbolCommand (line, word, DebugVerb::LoadSymbols, std::format ("\"{}\"", Unquote (line.tokens[1])));
    }
    else if (TryParseAddress (line, line.tokens[1], value))
    {
        AddSymbolCommand (line, word, DebugVerb::LookupSymbol, FormatHex (value));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::AddSymbolCommand
//
//  A symbol command under the word typed: sload loads the User table, and
//  slookup searches every enabled table.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::AddSymbolCommand (Line & line, const std::string & word, DebugVerb verb, const std::string & text)
{
    DebugCommand  command = MakeCommand (verb, word);



    command.text        = text;
    command.symbolTable = (verb == DebugVerb::LookupSymbol) ? std::string() : "USER";
    AddCommand (line, command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseRegister
//
//  GSSquared's r takes nothing, so r with arguments sets a register as
//  AppleWin's R does: `r a 41`, `r a=41` and `r pc = fa62`, as `bpd` with one
//  argument disables a breakpoint as AppleWin's BPD does.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseRegister (Line & line)
{
    static constexpr const char * kRegisters[] = { "A", "X", "Y", "P", "S", "PC" };
    DebugCommand                  command      = MakeCommand (DebugVerb::SetRegister, "r");
    std::string                   joined;
    std::string                   name;
    std::string                   value;
    std::string                   error;
    size_t                        split        = 0;



    for (size_t i = 1; i < line.tokens.size(); ++i)
    {
        joined += (joined.empty() ? "" : " ") + line.tokens[i];
    }

    std::replace (joined.begin(), joined.end(), '=', ' ');
    split = joined.find_first_of (" \t");
    name  = ToUpper (joined.substr (0, split));
    value = (split == std::string::npos) ? std::string() : joined.substr (split);

    if (name.empty())
    {
        SetInvalid (line, "r takes a register and a value. The registers are A, X, Y, P, S, and PC.");
        return;
    }

    if (std::find (std::begin (kRegisters), std::end (kRegisters), name) == std::end (kRegisters))
    {
        SetInvalid (line, std::format ("{} is not a register. The registers are A, X, Y, P, S, and PC.", name));
        return;
    }

    if (!AppleWinParser::TryEvaluate (value, line.context, command.a1, error))
    {
        SetInvalid (line, error);
        return;
    }

    command.text  = name;
    command.hasA1 = true;
    AddCommand (line, command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParsePanel
//
//  `debug` lists the device panels, `debug "name"` opens one and
//  `nodebug "name"` closes it. `debug list` lists them too, as PANEL LIST
//  does.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParsePanel (Line & line, bool isClose)
{
    size_t        count   = line.tokens.size();
    DebugCommand  command = MakeCommand (isClose ? DebugVerb::ClosePanel : DebugVerb::OpenPanel, ToLower (line.tokens[0]));



    if (count > 2 || (isClose && count != 2))
    {
        SetInvalid (line, isClose ? "nodebug takes a panel name." : "debug takes a panel name or nothing.");
        return;
    }

    command.text = (count == 1) ? std::string() : Unquote (line.tokens[1]);

    if (!isClose && (count == 1 || ToUpper (command.text) == "LIST"))
    {
        command.verb = DebugVerb::ListPanels;
        command.text.clear();
        AddCommand (line, command);
        return;
    }

    if (!isClose && ToUpper (command.text) == "CLOSE")
    {
        SetInvalid (line, "debug takes a panel name or nothing. Use nodebug name to close a panel.");
        return;
    }

    AddCommand (line, command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseHelp
//
//  `help word`: help on one command, which the help handler looks up.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseHelp (Line & line)
{
    DebugCommand  command = MakeCommand (DebugVerb::Help, "help");



    command.text = line.tokens[1];
    AddCommand (line, command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseMove
//
//  `move first.last dest`: the source range in a1/a2 and the destination in
//  a3, as AppleWin's M carries them.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseMove (Line & line, const std::string & word, Word first, Word last, Word dest)
{
    DebugCommand  command = MakeCommand (DebugVerb::MoveMemory, word);



    command.a3    = dest;
    command.hasA3 = true;
    AddRange (line, command, first, last, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseNoArguments
//
//  A word that takes nothing: help, sclear, and the step and run additions.
//  sclear acts on the User symbol table.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseNoArguments (Line & line, DebugVerb verb)
{
    static constexpr uint32_t  kOneStep = 1;
    std::string                word     = ToLower (line.tokens[0]);
    DebugCommand               command  = MakeCommand (verb, word);



    if (line.tokens.size() > 1)
    {
        SetInvalid (line, std::format ("{} takes nothing after it.", word));
        return;
    }

    if (verb == DebugVerb::StepInto || verb == DebugVerb::StepOver || verb == DebugVerb::StepOut)
    {
        command.count = kOneStep;
    }

    if (verb == DebugVerb::ClearSymbols)
    {
        command.symbolTable = "USER";
    }

    AddCommand (line, command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::ParseCassoCommand
//
//  A Casso engine command, reached by its bare name as in AppleWin mode. The
//  line goes to AppleWinParser exactly as typed.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::ParseCassoCommand (Line & line, const std::string & text)
{
    AppleWinParseResult  parsed = AppleWinParser::Parse (text, line.context);



    line.result.status = parsed.status;
    line.result.error  = parsed.error;

    if (parsed.status != ParseStatus::Ok)
    {
        return;
    }

    if (parsed.command.verb != DebugVerb::SetMode)
    {
        parsed.command.mode = CommandMode::GSSquared;
    }

    line.result.commands.push_back (parsed.command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::MakeCommand
//
////////////////////////////////////////////////////////////////////////////////

DebugCommand GSSquaredParser::MakeCommand (DebugVerb verb, const std::string & word)
{
    DebugCommand  command;



    command.verb       = verb;
    command.sourceName = word;
    command.mode       = CommandMode::GSSquared;
    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::AddCommand
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::AddCommand (Line & line, const DebugCommand & command)
{
    line.result.commands.push_back (command);
    line.result.status = ParseStatus::Ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::AddRange
//
//  The command with first in a1 and, when hasLast, last in a2.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredParser::AddRange (Line & line, DebugCommand command, Word first, Word last, bool hasLast)
{
    command.a1    = first;
    command.hasA1 = true;

    if (hasLast)
    {
        command.a2    = last;
        command.hasA2 = true;
    }

    AddCommand (line, command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryParseIdOrAll
//
//  An id in decimal, as ids are listed, or * for every entry, carried as
//  text.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryParseIdOrAll (Line & line, const std::string & token, DebugCommand & command)
{
    if (token == "*")
    {
        command.text = "*";
        return true;
    }

    if (!TryParseId (token, command.count))
    {
        SetInvalid (line, std::format ("{} is not an id.", token));
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryParseId
//
//  Decimal digits, with or without a leading #, that fit in 32 bits.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryParseId (const std::string & text, uint32_t & value)
{
    static constexpr size_t  kMaxDigits = 18;
    std::string              digits     = text.starts_with ('#') ? text.substr (1) : text;
    uint64_t                 wide       = 0;



    if (digits.empty() || digits.size() > kMaxDigits || digits.find_first_not_of ("0123456789") != std::string::npos)
    {
        return false;
    }

    wide = std::stoull (digits);

    if (wide > UINT32_MAX)
    {
        return false;
    }

    value = (uint32_t) wide;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser::TryParseIfExpression
//
//  The expression after IF, parsed into the command's condition. hasIf is
//  false when the line had no IF, and then there is nothing to parse.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryParseIfExpression (Line & line, bool hasIf, const std::string & expression, DebugCommand & command)
{
    std::string  error;
    HRESULT      hr    = S_OK;



    if (!hasIf)
    {
        return true;
    }

    if (expression.empty())
    {
        SetInvalid (line, "IF needs an expression.");
        return false;
    }

    hr = DebugExpressionEvaluator::Parse (expression, command.expression, error);

    if (FAILED (hr))
    {
        SetInvalid (line, error);
        return false;
    }

    return true;
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



    if (token.empty())
    {
        SetInvalid (line, "An address is missing. An address is one to four hex digits.");
        return false;
    }

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

    if (period == 0)
    {
        SetInvalid (line, std::format ("{} is not a range. A range is first.last in hex.", token));
        return false;
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
//  for TryParseIfExpression to parse. Anything else there is an error.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredParser::TryGetIfClause (const Tokens & tokens, size_t first, std::string & expression)
{
    if (tokens.size() <= first)
    {
        return true;
    }

    if (ToLower (tokens[first]) != "if")
    {
        return false;
    }

    for (size_t i = first + 1; i < tokens.size(); ++i)
    {
        expression += (expression.empty() ? "" : " ") + tokens[i];
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
//  GSSquaredParser::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string GSSquaredParser::ToUpper (const std::string & text)
{
    std::string  upper (text);



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return upper;
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
