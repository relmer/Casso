#include "Pch.h"

#include "Debugger/AppleWinParser.h"

#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::Parse
//
////////////////////////////////////////////////////////////////////////////////

AppleWinParseResult AppleWinParser::Parse (const std::string & line, const IDebugExpressionContext & context)
{
    AppleWinParseResult  result;
    Arguments            args     = { nullptr, Split (line), std::string(), &context };
    std::string          name;
    size_t               nameEnd  = 0;
    size_t               restFrom = 0;



    if (args.tokens.empty())
    {
        return result;
    }

    name     = args.tokens[0];
    nameEnd  = line.find (name) + name.size();
    restFrom = line.find_first_not_of (" \t", nameEnd);
    args.rest = (restFrom == std::string::npos) ? std::string() : line.substr (restFrom);
    args.tokens.erase (args.tokens.begin());

    result.command.sourceName = name;
    args.entry                = AppleWinCommandTable::Find (name);

    if (args.entry == nullptr)
    {
        if (!TryParseShorthand (name, args, result))
        {
            result.status = ParseStatus::Unknown;
            result.error  = std::format ("{} is not a command.", name);
        }

        return result;
    }

    if (args.entry->availability == CommandAvailability::NotAvailable)
    {
        result.status = ParseStatus::NotAvailable;
        result.error  = args.entry->reason;
        return result;
    }

    if (args.entry->availability == CommandAvailability::WindowOnly)
    {
        result.status = ParseStatus::WindowOnly;
        result.error  = std::format ("{} needs the debugger window.", ToUpper (name));
        return result;
    }

    result.command.verb = args.entry->verb;
    result.status       = TryParseArguments (args, result.command, result.error) ? ParseStatus::Ok : ParseStatus::Invalid;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::Split
//
////////////////////////////////////////////////////////////////////////////////

AppleWinParser::Tokens AppleWinParser::Split (const std::string & text)
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
//  AppleWinParser::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinParser::ToUpper (const std::string & text)
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
//  AppleWinParser::Join
//
//  Tokens from first onward, space-separated, in their original case.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinParser::Join (const Tokens & tokens, size_t first)
{
    std::string  joined;



    for (size_t i = first; i < tokens.size(); ++i)
    {
        joined += (joined.empty() ? "" : " ") + tokens[i];
    }

    return joined;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseShorthand
//
//  The classic Monitor forms AppleWin also accepts: `addr:bytes` deposits,
//  `addrG` sets PC and goes, `addrL` disassembles, and `dest<start.endM`
//  moves.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseShorthand (const std::string & first, const Arguments & args, AppleWinParseResult & result)
{
    size_t        colon  = first.find (':');
    Tokens        values;
    std::string   upper  = ToUpper (first);
    char          last   = upper.empty() ? '\0' : upper.back();
    std::string   head   = upper.size() > 1 ? upper.substr (0, upper.size() - 1) : std::string();



    if (colon != std::string::npos && colon > 0)
    {
        values = args.tokens;

        if (colon + 1 < first.size())
        {
            values.insert (values.begin(), first.substr (colon + 1));
        }

        result.command.verb = DebugVerb::EnterBytes;
        result.status       = TryEvaluate (first.substr (0, colon), *args.context, result.command.a1, result.error) &&
                              TryParseValues (values, 0, ValueWidth::BytesOrWords, *args.context, result.command, result.error)
                            ? ParseStatus::Ok : ParseStatus::Invalid;
        result.command.hasA1 = true;
        return true;
    }

    if (last == 'M' && TryParseMoveShorthand (upper, args, result))
    {
        return true;
    }

    if (last == 'G' && TryEvaluate (head, *args.context, result.command.a3, result.error))
    {
        result.command.verb  = DebugVerb::Go;
        result.command.hasA3 = true;
        result.status        = ParseStatus::Ok;
        return true;
    }

    if (last == 'L' && TryEvaluate (head, *args.context, result.command.a1, result.error))
    {
        result.command.verb  = DebugVerb::Disassemble;
        result.command.hasA1 = true;
        result.status        = ParseStatus::Ok;
        return true;
    }

    result.error.clear();
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseMoveShorthand
//
//  `dest<start.endM`, the Monitor's move, into the same command M produces:
//  the source range in a1/a2 and the destination in a3.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseMoveShorthand (const std::string & upper, const Arguments & args, AppleWinParseResult & result)
{
    size_t  less   = upper.find ('<');
    size_t  period = upper.find ('.', less == std::string::npos ? 0 : less);
    bool    isMove = less != std::string::npos && period != std::string::npos && less > 0 && period + 1 < upper.size() - 1;



    if (!isMove)
    {
        return false;
    }

    result.command.verb  = DebugVerb::MoveMemory;
    result.command.hasA1 = true;
    result.command.hasA2 = true;
    result.command.hasA3 = true;
    result.status        = TryEvaluate (upper.substr (0, less), *args.context, result.command.a3, result.error) &&
                           TryEvaluate (upper.substr (less + 1, period - less - 1), *args.context, result.command.a1, result.error) &&
                           TryEvaluate (upper.substr (period + 1, upper.size() - period - 2), *args.context, result.command.a2, result.error)
                         ? ParseStatus::Ok : ParseStatus::Invalid;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseArguments
//
//  Commands with no arguments accept and ignore anything after the name, as
//  AppleWin does.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    switch (args.entry->family)
    {
    case AppleWinCommandFamily::Cpu:         return TryParseRunArguments        (args, command, error);
    case AppleWinCommandFamily::Flags:       return TryParseFlagArguments       (args, command, error);
    case AppleWinCommandFamily::Breakpoints: return TryParseBreakpointArguments (args, command, error);
    case AppleWinCommandFamily::Memory:
    case AppleWinCommandFamily::Assembler:   return TryParseMemoryArguments     (args, command, error);
    case AppleWinCommandFamily::Data:        return TryParseDataArguments       (args, command, error);
    case AppleWinCommandFamily::Watch:
    case AppleWinCommandFamily::ZeroPage:
    case AppleWinCommandFamily::Bookmarks:   return TryParseListArguments       (args, command, error);
    case AppleWinCommandFamily::Symbols:     return TryParseSymbolArguments     (args, command, error);
    case AppleWinCommandFamily::Output:      return TryParseOutputArguments     (args, command, error);
    case AppleWinCommandFamily::Engine:      return TryParseEngineArguments     (args, command, error);
    default:
        command.text = args.rest;
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseRunArguments
//
//  G and GG take a stop address and a skip range; T, TL, P and RTS a count;
//  R shows or sets a register; =, JSR and IN an address; OUT an address and
//  a value; KEY and PUSH values; TF a file.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseRunArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    DebugCommand  skip;
    Word          value = 0;



    switch (command.verb)
    {
    case DebugVerb::Go:
    case DebugVerb::GoFullSpeed:
        if (!args.tokens.empty() && !TryEvaluate (args.tokens[0], *args.context, command.a1, error)) { return false; }
        command.hasA1 = !args.tokens.empty();
        if (args.tokens.size() > 1 && !TryParseRange (args.tokens[1], *args.context, skip, error))   { return false; }
        command.a2    = skip.a1;
        command.a3    = skip.a2;
        command.hasA2 = args.tokens.size() > 1;
        command.hasA3 = command.hasA2;
        return true;

    case DebugVerb::StepInto:
    case DebugVerb::StepOver:
    case DebugVerb::StepOut:
        if (!args.tokens.empty() && !TryEvaluate (args.tokens[0], *args.context, value, error)) { return false; }
        command.count = args.tokens.empty() ? 1 : value;
        return true;

    case DebugVerb::ShowRegisters:
        return TryParseRegisterArguments (args, command, error);

    case DebugVerb::Disassemble:
        return args.tokens.empty() || TryParseRange (args.tokens[0], *args.context, command, error);

    case DebugVerb::SetProgramCounter:
    case DebugVerb::CallSubroutine:
    case DebugVerb::ReadIo:
        command.hasA1 = !args.tokens.empty();
        return args.tokens.empty() || TryEvaluate (args.tokens[0], *args.context, command.a1, error);

    case DebugVerb::WriteIo:
        command.hasA1 = !args.tokens.empty();
        return command.hasA1 && TryEvaluate (args.tokens[0], *args.context, command.a1, error) &&
               TryParseValues (args.tokens, 1, ValueWidth::Bytes, *args.context, command, error);

    case DebugVerb::InjectKey:
    case DebugVerb::PushStack:
        return TryParseValues (args.tokens, 0, ValueWidth::Bytes, *args.context, command, error);

    default:
        command.text = args.rest;
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseRegisterArguments
//
//  R alone shows the registers. `R A 41`, `R A=41` and `R PC = FA62` set one.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseRegisterArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    static constexpr const char * kRegisters[] = { "A", "X", "Y", "P", "S", "PC" };
    std::string  joined = args.rest;
    size_t       split  = 0;
    std::string  name;
    std::string  value;



    if (args.tokens.empty())
    {
        return true;
    }

    std::replace (joined.begin(), joined.end(), '=', ' ');
    split = joined.find_first_of (" \t");
    name  = ToUpper (joined.substr (0, split));
    value = (split == std::string::npos) ? std::string() : joined.substr (split);

    if (std::find (std::begin (kRegisters), std::end (kRegisters), name) == std::end (kRegisters))
    {
        error = std::format ("{} is not a register. The registers are A, X, Y, P, S, and PC.", name);
        return false;
    }

    command.verb  = DebugVerb::SetRegister;
    command.text  = name;
    command.hasA1 = true;
    return TryEvaluate (value, *args.context, command.a1, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseFlagArguments
//
//  CLC and SEC carry the flag in the name, RC and SC likewise; CL and SE
//  take it as an argument.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseFlagArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    static constexpr std::string_view  kFlagLetters = "CZIDBRVN";
    std::string  name = ToUpper (command.sourceName);



    command.text = (name.size() == 2 && args.entry->aliasOf == nullptr)
                 ? (args.tokens.empty() ? std::string() : ToUpper (args.tokens[0]))
                 : name.substr (name.size() - 1);

    if (command.text.size() != 1 || kFlagLetters.find (command.text[0]) == std::string_view::npos)
    {
        error = "A flag is one of C, Z, I, D, B, R, V, or N.";
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseBreakpointArguments
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseBreakpointArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    static constexpr std::string_view  kOperators = "<>=!";
    bool  isConditional = !args.tokens.empty() && kOperators.find (args.tokens[0][0]) != std::string_view::npos;



    switch (command.verb)
    {
    case DebugVerb::SetBreakpoint:
        if (isConditional)
        {
            command.verb = DebugVerb::SetConditionalBreakpoint;
            return TryParseCondition ("PC", args.tokens, 0, command, error);
        }

        [[fallthrough]];

    case DebugVerb::SetBreakpointAndWatchpoint:
    case DebugVerb::BreakOnVideoLine:
        if (args.tokens.empty())
        {
            error = std::format ("{} needs an address.", ToUpper (command.sourceName));
            return false;
        }

        return TryParseRange (args.tokens[0], *args.context, command, error);

    case DebugVerb::SetMemoryWatchpoint:
    case DebugVerb::SetReadWatchpoint:
    case DebugVerb::SetWriteWatchpoint:
        return TryParseWatchpointArguments (args, command, error);

    case DebugVerb::SetRegisterBreakpoint:
        if (args.tokens.empty())
        {
            error = "BPR needs a register and a value.";
            return false;
        }

        command.text = ToUpper (args.tokens[0]);
        return TryParseCondition (command.text, args.tokens, 1, command, error);

    case DebugVerb::BreakOnOpcode:
        return TryParseValues (args.tokens, 0, ValueWidth::Bytes, *args.context, command, error);

    case DebugVerb::ClearBreakpoint:
    case DebugVerb::DisableBreakpoint:
    case DebugVerb::EnableBreakpoint:
        return TryParseIdOrAll (args.tokens, command, error);

    // BPCHANGE # flags keeps the flags' case: E and e differ. BPEDIT # def
    // carries the new definition for the handler to parse as its own line.
    case DebugVerb::ChangeBreakpoint:
    case DebugVerb::EditBreakpoint:
        command.text = Join (args.tokens, 1);

        if (!TryParseIdOrAll (args.tokens, command, error))
        {
            return false;
        }

        if (command.text.empty())
        {
            error = (command.verb == DebugVerb::EditBreakpoint) ? "BPEDIT # takes the breakpoint's new definition."
                                                                : "BPCHANGE # takes flags: E or e, T or t, S or s.";
            return false;
        }

        return true;

    default:
        command.text = ToUpper (args.rest);
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseWatchpointArguments
//
//  A range and an optional trailing BEFORE or AFTER; AFTER is the default.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseWatchpointArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    if (args.tokens.empty())
    {
        error = std::format ("{} needs an address.", ToUpper (command.sourceName));
        return false;
    }

    command.text = "AFTER";

    if (args.tokens.size() > 1)
    {
        command.text = ToUpper (args.tokens[1]);

        if (command.text != "BEFORE" && command.text != "AFTER")
        {
            error = "A watchpoint's mode is BEFORE or AFTER.";
            return false;
        }
    }

    return TryParseRange (args.tokens[0], *args.context, command, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseMemoryArguments
//
//  `M dest src,len`, `MC dest src,len`, `F range value` or `F start end
//  value`, `S range items`, `ME addr bytes`, `MEW addr words`, `BLOAD file
//  addr[,len]`, and a range for D, U and A.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseMemoryArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    DebugCommand  source;
    size_t        count      = args.tokens.size();
    size_t        valuesFrom = 1;
    bool          isThreeArg = false;



    switch (command.verb)
    {
    case DebugVerb::MoveMemory:
    case DebugVerb::CompareMemory:
        if (count < 2)
        {
            error = std::format ("{} needs a destination and a source range.", ToUpper (command.sourceName));
            return false;
        }

        command.hasA3 = true;
        return TryEvaluate (args.tokens[0], *args.context, command.a3, error) &&
               TryParseRange (args.tokens[1], *args.context, command, error);

    // F start end value is the three-argument form AppleWin also takes.
    case DebugVerb::FillMemory:
        isThreeArg = count >= 3 && args.tokens[0].find_first_of (",:") == std::string::npos;
        valuesFrom = isThreeArg ? 2 : 1;

        if (isThreeArg)
        {
            command.hasA1 = true;
            command.hasA2 = true;
            return TryEvaluate (args.tokens[0], *args.context, command.a1, error) &&
                   TryEvaluate (args.tokens[1], *args.context, command.a2, error) &&
                   TryParseValues (args.tokens, valuesFrom, ValueWidth::Bytes, *args.context, command, error);
        }

        return count >= 2 &&
               TryParseRange (args.tokens[0], *args.context, command, error) &&
               TryParseValues (args.tokens, valuesFrom, ValueWidth::Bytes, *args.context, command, error);

    case DebugVerb::SearchMemory:
    case DebugVerb::SearchHex:
        if (count < 2)
        {
            error = std::format ("{} needs a range and what to search for.", ToUpper (command.sourceName));
            return false;
        }

        return TryParseRange (args.tokens[0], *args.context, command, error) &&
               TryParseSearchItems (args.rest.substr (args.rest.find (args.tokens[0]) + args.tokens[0].size()), *args.context, command, error);

    case DebugVerb::EnterBytes:
    case DebugVerb::EnterWords:
        command.hasA1 = count > 0;
        return count >= 2 &&
               TryEvaluate (args.tokens[0], *args.context, command.a1, error) &&
               TryParseValues (args.tokens, 1, command.verb == DebugVerb::EnterWords ? ValueWidth::Words : ValueWidth::BytesOrWords, *args.context, command, error);

    case DebugVerb::LoadBinary:
    case DebugVerb::SaveBinary:
        if (count == 0)
        {
            error = std::format ("{} needs a file name.", ToUpper (command.sourceName));
            return false;
        }

        command.text = args.tokens[0];
        return count < 2 || TryParseRange (args.tokens[1], *args.context, command, error);

    case DebugVerb::SaveText:
        command.text = args.rest;
        return true;

    default:
        return count == 0 || TryParseRange (args.tokens[0], *args.context, command, error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseDataArguments
//
//  The data directives take `[name] [addr | range]` or `name = addr`. A
//  first argument that evaluates is the address; otherwise it is the block's
//  name and the address follows. B takes nothing; X takes a range.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseDataArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    size_t       count     = args.tokens.size();
    size_t       rangeAt   = 0;
    std::string  discarded;



    if (command.verb == DebugVerb::ListData || count == 0)
    {
        return true;
    }

    if (command.verb == DebugVerb::RemoveData)
    {
        return TryParseRange (args.tokens[0], *args.context, command, error);
    }

    if (!TryParseRange (args.tokens[0], *args.context, command, discarded))
    {
        command.text  = args.tokens[0];
        command.hasA1 = false;
        command.hasA2 = false;
        rangeAt       = (count > 1 && args.tokens[1] == "=") ? 2 : 1;
    }

    return rangeAt == 0 || rangeAt >= count || TryParseRange (args.tokens[rangeAt], *args.context, command, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseOutputArguments
//
//  CALC evaluates one expression; the rest take the line as text.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseOutputArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    if (command.verb == DebugVerb::Calculate)
    {
        if (args.tokens.empty())
        {
            error = "CALC needs an expression.";
            return false;
        }

        command.hasA1 = true;
        return TryEvaluate (args.rest, *args.context, command.a1, error);
    }

    command.text = args.rest;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseListArguments
//
//  Watches, zero-page pointers and bookmarks: an address to add, an id or *
//  to clear, enable, disable or go to. ZP0-ZP7 and P0-P4 carry a slot.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseListArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    std::string  name  = ToUpper (command.sourceName);
    char         digit = name.back();



    switch (command.verb)
    {
    case DebugVerb::AddWatch:
    case DebugVerb::AddZeroPagePointer:
    case DebugVerb::AddBookmark:
        if (isdigit ((unsigned char) digit) && (name.starts_with ("ZP") || name.starts_with ("P")))
        {
            command.count = (uint32_t) (digit - '0');
        }

        command.hasA1 = !args.tokens.empty();
        return args.tokens.empty() || TryEvaluate (args.tokens[0], *args.context, command.a1, error);

    case DebugVerb::ClearWatch:
    case DebugVerb::DisableWatch:
    case DebugVerb::EnableWatch:
    case DebugVerb::ClearZeroPagePointer:
    case DebugVerb::DisableZeroPagePointer:
    case DebugVerb::EnableZeroPagePointer:
    case DebugVerb::ClearBookmark:
    case DebugVerb::GoToBookmark:
        return TryParseIdOrAll (args.tokens, command, error);

    default:
        command.text = args.rest;
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseSymbolArguments
//
//  `SYM` alone reports counts. `SYM<table>` takes `CLEAR`, `LOAD "file"
//  [,offset]`, `SAVE "file"`, `ON`, `OFF`, `name = addr`, `! name` or
//  `~ name`, or a name or address to look up. The table is carried by the
//  command's name.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseSymbolArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    size_t       equals = args.rest.find ('=');
    std::string  first  = args.tokens.empty() ? std::string() : ToUpper (args.tokens[0]);



    if (command.verb != DebugVerb::LookupSymbol)
    {
        command.text = args.rest;
        return true;
    }

    if (args.tokens.empty())
    {
        command.verb = DebugVerb::ShowSymbolInfo;
        return true;
    }

    if (first == "CLEAR")
    {
        command.verb = DebugVerb::ClearSymbols;
        return true;
    }

    if (first == "ON" || first == "OFF")
    {
        command.verb  = DebugVerb::EnableSymbols;
        command.count = (first == "ON") ? 1 : 0;
        return true;
    }

    if (first == "LOAD" || first == "SAVE")
    {
        command.verb = (first == "LOAD") ? DebugVerb::LoadSymbols : DebugVerb::SaveSymbols;
        command.text = Join (args.tokens, 1);

        if (command.text.empty())
        {
            error = std::format ("{} needs a file name.", first);
            return false;
        }

        return true;
    }

    if (equals != std::string::npos)
    {
        command.verb  = DebugVerb::AddSymbol;
        command.text  = Split (args.rest.substr (0, equals)).empty() ? std::string() : Split (args.rest.substr (0, equals))[0];
        command.hasA1 = true;
        return !command.text.empty() && TryEvaluate (args.rest.substr (equals + 1), *args.context, command.a1, error);
    }

    if (first == "!" || first == "~")
    {
        command.verb = DebugVerb::RemoveSymbol;
        command.text = args.tokens.size() > 1 ? args.tokens[1] : std::string();
        return !command.text.empty();
    }

    command.text = args.rest;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseEngineArguments
//
//  MODE [APPLEWIN | MONITOR], and BUDGET n with n in decimal, as
//  --max-cycles takes it.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseEngineArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    std::string  mode;



    if (command.verb == DebugVerb::ShowMode && !args.tokens.empty())
    {
        mode = ToUpper (args.tokens[0]);

        if (mode != "APPLEWIN" && mode != "MONITOR")
        {
            error = "The modes are APPLEWIN and MONITOR.";
            return false;
        }

        command.verb = DebugVerb::SetMode;
        command.mode = (mode == "MONITOR") ? CommandMode::Monitor : CommandMode::AppleWin;
        return true;
    }

    if (command.verb == DebugVerb::SetBudget)
    {
        if (args.tokens.empty() || args.tokens[0].find_first_not_of ("0123456789") != std::string::npos)
        {
            error = "BUDGET takes a number of cycles in decimal. BUDGET 0 removes the budget.";
            return false;
        }

        command.count = (uint32_t) std::stoul (args.tokens[0]);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryEvaluate
//
//  A value that does not fit in 16 bits is an error.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryEvaluate (
    const std::string              & text,
    const IDebugExpressionContext  & context,
    Word                           & value,
    std::string                    & error)
{
    static constexpr int32_t  kMaxWord = 0xFFFF;
    int32_t                   result   = 0;
    HRESULT                   hr       = DebugExpressionEvaluator::ParseAndEvaluate (text, context, result, error);



    if (FAILED (hr))
    {
        return false;
    }

    if (result < 0 || result > kMaxWord)
    {
        error = std::format ("{} is outside $0000-$FFFF.", text);
        return false;
    }

    value = (Word) result;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseRange
//
//  `addr`, `addr,len` or `addr:last`, into a1 and a2.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseRange (
    const std::string              & text,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    size_t  separator = text.find_first_of (",:");
    Word    second    = 0;



    command.hasA1 = true;
    command.hasA2 = true;

    if (separator == std::string::npos)
    {
        command.hasA2 = false;
        return TryEvaluate (text, context, command.a1, error);
    }

    if (!TryEvaluate (text.substr (0, separator), context, command.a1, error) ||
        !TryEvaluate (text.substr (separator + 1), context, second, error))
    {
        return false;
    }

    if (text[separator] == ':')
    {
        command.a2 = second;
    }
    else if (second == 0)
    {
        error = "A range length must be at least 1.";
        return false;
    }
    else
    {
        command.a2 = (Word) (command.a1 + second - 1);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseValues
//
//  Bytes, or words stored low byte first.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseValues (
    const Tokens                   & tokens,
    size_t                           first,
    ValueWidth                       width,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr Word  kMaxByte = 0xFF;
    Word                   value    = 0;
    bool                   isWord   = false;



    for (size_t i = first; i < tokens.size(); ++i)
    {
        if (!TryEvaluate (tokens[i], context, value, error))
        {
            return false;
        }

        if (width == ValueWidth::Bytes && value > kMaxByte)
        {
            error = std::format ("{} is not a byte.", tokens[i]);
            return false;
        }

        isWord = width == ValueWidth::Words || (width == ValueWidth::BytesOrWords && value > kMaxByte);

        command.values.push_back ((Byte) value);
        command.mask.push_back (kMaxByte);

        if (isWord)
        {
            command.values.push_back ((Byte) (value >> 8));
            command.mask.push_back (kMaxByte);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseSearchItems
//
//  The S and SH item syntax: "text" with the high bit clear, 'text' with it
//  set, a byte, a 16-bit value matched low byte first, and the wildcards ?
//  (any byte), ?n (any high nibble) and n? (any low nibble). ?? is one
//  wildcard. Each byte gets a mask of the bits that must match.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseSearchItems (
    const std::string              & items,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr Byte  kHighBit = 0x80;
    static constexpr Byte  kAllBits = 0xFF;
    size_t                 pos      = 0;



    while (pos < items.size())
    {
        char    quote = items[pos];
        size_t  close = 0;
        size_t  end   = 0;



        if (isspace ((unsigned char) quote))
        {
            ++pos;
            continue;
        }

        if (quote == '"' || quote == '\'')
        {
            close = items.find (quote, pos + 1);

            if (close == std::string::npos)
            {
                error = "A search string needs a closing quote.";
                return false;
            }

            for (size_t i = pos + 1; i < close; ++i)
            {
                command.values.push_back ((Byte) ((items[i] & ~kHighBit) | (quote == '\'' ? kHighBit : 0)));
                command.mask.push_back (kAllBits);
            }

            pos = close + 1;
            continue;
        }

        end = items.find_first_of (" 	", pos);
        end = (end == std::string::npos) ? items.size() : end;

        if (!TryParseSearchWord (items.substr (pos, end - pos), context, command, error))
        {
            return false;
        }

        pos = end;
    }

    if (command.values.empty())
    {
        error = "Give something to search for.";
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseSearchWord
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseSearchWord (
    const std::string              & word,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr Byte    kAllBits   = 0xFF;
    static constexpr Byte    kLowBits   = 0x0F;
    static constexpr Byte    kHighBits  = 0xF0;
    static constexpr int     kNibble    = 4;
    static constexpr size_t  kByteWidth = 2;
    Word                     value      = 0;
    bool                     isHexish   = word.size() <= kByteWidth && word.find_first_not_of ("0123456789ABCDEFabcdef?") == std::string::npos;



    if (word == "?" || word == "??")
    {
        command.values.push_back (0);
        command.mask.push_back (0);
        return true;
    }

    if (isHexish && word.size() == kByteWidth && word[0] == '?')
    {
        command.values.push_back ((Byte) std::stoul (word.substr (1), nullptr, 16));
        command.mask.push_back (kLowBits);
        return true;
    }

    if (isHexish && word.size() == kByteWidth && word[1] == '?')
    {
        command.values.push_back ((Byte) (std::stoul (word.substr (0, 1), nullptr, 16) << kNibble));
        command.mask.push_back (kHighBits);
        return true;
    }

    if (!TryEvaluate (word, context, value, error))
    {
        return false;
    }

    command.values.push_back ((Byte) value);
    command.mask.push_back (kAllBits);

    if (value > kAllBits || word.size() > kByteWidth)
    {
        command.values.push_back ((Byte) (value >> 8));
        command.mask.push_back (kAllBits);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseIdOrAll
//
//  An id in decimal as AppleWin lists them, or * for every entry, which is
//  carried as text.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseIdOrAll (const Tokens & tokens, DebugCommand & command, std::string & error)
{
    if (tokens.empty())
    {
        error = "Give an id, or * for all.";
        return false;
    }

    if (tokens[0] == "*")
    {
        command.text = "*";
        return true;
    }

    if (tokens[0].find_first_not_of ("0123456789") != std::string::npos)
    {
        error = std::format ("{} is not an id.", tokens[0]);
        return false;
    }

    command.count = (uint32_t) std::stoul (tokens[0]);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseCondition
//
//  `[op] value` against subject, where op is = ! < > <= >= and defaults to
//  =. AppleWin's `!` means not equal.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseCondition (
    const std::string  & subject,
    const Tokens       & tokens,
    size_t               first,
    DebugCommand       & command,
    std::string        & error)
{
    static constexpr const char * kOperators[] = { "<=", ">=", "!=", "!", "=", "<", ">" };
    std::string  rest;
    std::string  op     = "=";
    HRESULT      hr     = S_OK;



    for (size_t i = first; i < tokens.size(); ++i)
    {
        rest += tokens[i];
    }

    for (const char * candidate : kOperators)
    {
        if (rest.starts_with (candidate))
        {
            op   = (std::string (candidate) == "!") ? "!=" : candidate;
            rest = rest.substr (strlen (candidate));
            break;
        }
    }

    if (rest.empty())
    {
        error = "A condition needs a value.";
        return false;
    }

    hr = DebugExpressionEvaluator::Parse (subject + op + rest, command.expression, error);
    return SUCCEEDED (hr);
}
