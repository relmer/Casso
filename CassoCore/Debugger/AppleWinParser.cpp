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
//  AppleWinParser::TryParseShorthand
//
//  The two classic Monitor forms AppleWin also accepts: `addr:bytes` to
//  deposit, and `addrG` to set PC and go.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseShorthand (const std::string & first, const Arguments & args, AppleWinParseResult & result)
{
    size_t        colon  = first.find (':');
    Tokens        values;
    std::string   upper  = ToUpper (first);
    bool          isGo   = upper.size() > 1 && upper.back() == 'G';



    if (colon != std::string::npos && colon > 0)
    {
        values = args.tokens;

        if (colon + 1 < first.size())
        {
            values.insert (values.begin(), first.substr (colon + 1));
        }

        result.command.verb = DebugVerb::EnterBytes;
        result.status       = TryEvaluate (first.substr (0, colon), *args.context, result.command.a1, result.error) &&
                              TryParseValues (values, 0, false, *args.context, result.command, result.error)
                            ? ParseStatus::Ok : ParseStatus::Invalid;
        result.command.hasA1 = true;
        return true;
    }

    if (isGo && TryEvaluate (upper.substr (0, upper.size() - 1), *args.context, result.command.a3, result.error))
    {
        result.command.verb  = DebugVerb::Go;
        result.command.hasA3 = true;
        result.status        = ParseStatus::Ok;
        return true;
    }

    result.error.clear();
    return false;
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
    case AppleWinCommandFamily::Data:
    case AppleWinCommandFamily::Assembler:   return TryParseMemoryArguments     (args, command, error);
    case AppleWinCommandFamily::Watch:
    case AppleWinCommandFamily::ZeroPage:
    case AppleWinCommandFamily::Bookmarks:   return TryParseListArguments       (args, command, error);
    case AppleWinCommandFamily::Symbols:     return TryParseSymbolArguments     (args, command, error);
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
    case DebugVerb::TraceLine:
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
               TryParseValues (args.tokens, 1, false, *args.context, command, error);

    case DebugVerb::InjectKey:
    case DebugVerb::PushStack:
        return TryParseValues (args.tokens, 0, false, *args.context, command, error);

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

    case DebugVerb::SetIoBreakpoint:
    case DebugVerb::SetMemoryWatchpoint:
    case DebugVerb::SetReadWatchpoint:
    case DebugVerb::SetWriteWatchpoint:
    case DebugVerb::BreakOnVideoLine:
        if (args.tokens.empty())
        {
            error = std::format ("{} needs an address.", ToUpper (command.sourceName));
            return false;
        }

        // A watchpoint takes a trailing BEFORE or AFTER; AFTER is the default.
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

    case DebugVerb::SetConditionalBreakpoint:
        return TryParseCondition ("PC", args.tokens, 0, command, error);

    case DebugVerb::SetRegisterBreakpoint:
        if (args.tokens.empty())
        {
            error = "BPR needs a register and a value.";
            return false;
        }

        command.text = ToUpper (args.tokens[0]);
        return TryParseCondition (command.text, args.tokens, 1, command, error);

    case DebugVerb::BreakOnOpcode:
        return TryParseValues (args.tokens, 0, false, *args.context, command, error);

    case DebugVerb::ClearBreakpoint:
    case DebugVerb::DisableBreakpoint:
    case DebugVerb::EnableBreakpoint:
    case DebugVerb::EditBreakpoint:
    case DebugVerb::ChangeBreakpoint:
        return TryParseIdOrAll (args.tokens, command, error);

    default:
        command.text = ToUpper (args.rest);
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseMemoryArguments
//
//  `M dest src,len`, `MC dest src,len`, `F range value`, `S range bytes`,
//  `ME addr bytes`, `MEW addr words`, `BLOAD file addr[,len]`, and a range
//  for D, U, A and the data directives.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseMemoryArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    DebugCommand  source;
    size_t        count = args.tokens.size();



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

    case DebugVerb::FillMemory:
    case DebugVerb::SearchMemory:
    case DebugVerb::SearchHex:
        return count >= 2 &&
               TryParseRange (args.tokens[0], *args.context, command, error) &&
               TryParseValues (args.tokens, 1, false, *args.context, command, error);

    case DebugVerb::EnterBytes:
    case DebugVerb::EnterWords:
        command.hasA1 = count > 0;
        return count >= 2 &&
               TryEvaluate (args.tokens[0], *args.context, command.a1, error) &&
               TryParseValues (args.tokens, 1, command.verb == DebugVerb::EnterWords, *args.context, command, error);

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
//  `SYM` alone reports counts; `SYM name = addr` adds a user symbol;
//  `SYM ! name` removes one; `SYM file.sym` loads a file; anything else is
//  looked up. The table is carried by the command's name.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseSymbolArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    size_t  equals = args.rest.find ('=');



    if (command.verb != DebugVerb::LookupSymbol || args.tokens.empty())
    {
        command.text = args.rest;
        return true;
    }

    if (equals != std::string::npos)
    {
        command.verb  = DebugVerb::AddSymbol;
        command.text  = Split (args.rest.substr (0, equals)).empty() ? std::string() : Split (args.rest.substr (0, equals))[0];
        command.hasA1 = true;
        return !command.text.empty() && TryEvaluate (args.rest.substr (equals + 1), *args.context, command.a1, error);
    }

    if (args.tokens[0] == "!")
    {
        command.verb = DebugVerb::RemoveSymbol;
        command.text = args.tokens.size() > 1 ? args.tokens[1] : std::string();
        return !command.text.empty();
    }

    command.verb = (args.tokens[0].find ('.') != std::string::npos) ? DebugVerb::LoadSymbols : DebugVerb::LookupSymbol;
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
    bool                             isWords,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr Word  kMaxByte = 0xFF;
    Word                   value    = 0;



    for (size_t i = first; i < tokens.size(); ++i)
    {
        if (!TryEvaluate (tokens[i], context, value, error))
        {
            return false;
        }

        if (!isWords && value > kMaxByte)
        {
            error = std::format ("{} is not a byte.", tokens[i]);
            return false;
        }

        command.values.push_back ((Byte) value);

        if (isWords)
        {
            command.values.push_back ((Byte) (value >> 8));
        }
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
