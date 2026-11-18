#include "Pch.h"

#include "Debugger/AppleWinBreakpointParser.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/AppleWinText.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseBreakpointArguments
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseBreakpointArguments (const Arguments & source, DebugCommand & command, std::string & error)
{
    static constexpr std::string_view  kOperators = "<>=!";
    Arguments                          args          = source;
    bool                               isConditional = !args.tokens.empty() && kOperators.find (args.tokens[0][0]) != std::string_view::npos;
    bool                               takesIf       = command.verb == DebugVerb::SetMemoryWatchpoint        ||
                                                       command.verb == DebugVerb::SetReadWatchpoint          ||
                                                       command.verb == DebugVerb::SetWriteWatchpoint         ||
                                                       command.verb == DebugVerb::SetValueBreakpoint         ||
                                                       command.verb == DebugVerb::SetDiskBreakpoint          ||
                                                       command.verb == DebugVerb::BreakOnOpcode              ||
                                                       command.verb == DebugVerb::BreakOnBrk                 ||
                                                       command.verb == DebugVerb::BreakOnInterrupt           ||
                                                       command.verb == DebugVerb::SetBasicLineBreakpoint     ||
                                                       command.verb == DebugVerb::SetBasicVariableBreakpoint ||
                                                       command.verb == DebugVerb::BreakOnBasicError          ||
                                                       (command.verb == DebugVerb::SetBreakpoint && !isConditional);
    bool                               takesOne      = command.verb == DebugVerb::SetBreakpointAndWatchpoint ||
                                                       command.verb == DebugVerb::ClearBreakpoint            ||
                                                       command.verb == DebugVerb::DisableBreakpoint          ||
                                                       command.verb == DebugVerb::EnableBreakpoint           ||
                                                       (command.verb == DebugVerb::SetBreakpoint && !isConditional);
    auto                               isIf          = [] (const std::string & token) { return AppleWinText::ToUpper (token) == "IF"; };
    auto                               isLive        = [&] ()
    {
        if (command.hasA2 && command.a2 < command.a1)
        {
            error = "The range ends before it starts or runs past $FFFF, so nothing could ever match it.";
            return false;
        }

        return true;
    };



    if (takesIf && !TryParseIfClause (args.tokens, command, error, args.context->GetNumberSyntax()))
    {
        return false;
    }

    // A comparison is the whole condition; IF would be read as part of its value.
    // BPR IF, with IF first, is a condition of any form.
    if (!takesIf && (isConditional || command.verb == DebugVerb::SetRegisterBreakpoint) &&
        !(command.verb == DebugVerb::SetRegisterBreakpoint && !args.tokens.empty() && isIf (args.tokens[0])) &&
        std::find_if (args.tokens.begin(), args.tokens.end(), isIf) != args.tokens.end())
    {
        error = std::format ("{} with a comparison does not take IF.", AppleWinText::ToUpper (command.sourceName));
        return false;
    }

    if (takesOne && args.tokens.size() > 1)
    {
        error = std::format ("{} takes one argument, not {}.", AppleWinText::ToUpper (command.sourceName), args.tokens.size());
        return false;
    }

    switch (command.verb)
    {
    case DebugVerb::SetBreakpoint:
        if (isConditional)
        {
            command.verb = DebugVerb::SetConditionalBreakpoint;
            return TryParseCondition ("PC", args.tokens, 0, command, error, args.context->GetNumberSyntax());
        }

        [[fallthrough]];

    case DebugVerb::SetBreakpointAndWatchpoint:
    case DebugVerb::BreakOnVideoLine:
        if (command.verb == DebugVerb::BreakOnVideoLine && args.tokens.size() == 1 && AppleWinText::ToUpper (args.tokens[0]) == "OFF")
        {
            command.text = "OFF";
            return true;
        }

        if (args.tokens.empty())
        {
            error = std::format ("{} needs an address.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        if (command.verb == DebugVerb::SetBreakpoint && AppleWinParser::TryParseSourceLine (args.tokens[0], *args.context, command))
        {
            return true;
        }

        if (!AppleWinParser::TryParseRange (args.tokens[0], *args.context, command, error))
        {
            return false;
        }

        if (command.verb == DebugVerb::SetBreakpoint)
        {
            NoteSymbol (args.tokens[0], *args.context, command);
        }

        return isLive();

    case DebugVerb::BreakOnBeam:
        return TryParseBeamArguments (args, command, error);

    case DebugVerb::SetDiskBreakpoint:
        return TryParseDiskArguments (args, command, error);

    case DebugVerb::RunFrame:
        return TryParseFrameArguments (args, command, error);

    case DebugVerb::SetMemoryWatchpoint:
    case DebugVerb::SetReadWatchpoint:
    case DebugVerb::SetWriteWatchpoint:
        return TryParseWatchpointArguments (args, command, error) && isLive();

    case DebugVerb::SetValueBreakpoint:
        return TryParseValueBreakpoint (args, command, error);

    case DebugVerb::SetRegisterBreakpoint:
        if (!args.tokens.empty() && isIf (args.tokens[0]))
        {
            return TryParseFreeCondition (AppleWinText::Join (args.tokens, 1), command, error, args.context->GetNumberSyntax());
        }

        return TryParseRegisterCondition (args.tokens, command, error, args.context->GetNumberSyntax());

    case DebugVerb::BreakOnOpcode:
        return TryParseOpcodes (args, command, error);

    case DebugVerb::ClearBreakpoint:
    case DebugVerb::DisableBreakpoint:
    case DebugVerb::EnableBreakpoint:
        return AppleWinParser::TryParseIdOrAll (args.tokens, command, error, args.context->GetNumberSyntax());

    // BPCHANGE # flags keeps the flags' case: E and e differ. BPEDIT # def
    // carries the new definition for the handler to parse as its own line.
    case DebugVerb::ChangeBreakpoint:
    case DebugVerb::EditBreakpoint:
        command.text = AppleWinText::Join (args.tokens, 1);

        if (!args.tokens.empty() && args.tokens[0] == "*")
        {
            error = std::format ("{} takes one id, not *.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        if (!AppleWinParser::TryParseIdOrAll (args.tokens, command, error, args.context->GetNumberSyntax()))
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

    // BPNAME # "text": the id, then the name as typed, its quotes taken off.
    case DebugVerb::SetBreakpointName:
    case DebugVerb::SetBreakpointLog:
        return TryParseNameArguments (args, command, error);

    // A file name and a group's name are taken in the case typed; BRK's and
    // BRKINT's keywords are not.
    case DebugVerb::SaveBreakpoints:
    case DebugVerb::GroupBreakpoints:
        command.text = args.rest;
        return true;

    //  An IF clause is parsed above; the text is what comes before it.
    default:
        command.text = AppleWinText::ToUpper (takesIf ? CutIfClause (args.rest) : args.rest);
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseNameArguments
//
//  The name is the rest of the line after the id, with the spaces inside
//  it; in quotes, the quotes are taken off. Nothing after the id clears it.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseNameArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    std::string  rest  = args.rest;
    size_t       start = rest.find_first_not_of (" \t");
    size_t       end   = (start == std::string::npos) ? std::string::npos : rest.find_first_of (" \t", start);
    Tokens       id;



    if (start == std::string::npos || rest[start] == '*')
    {
        error = "BPNAME takes a breakpoint's id, then its name in quotes.";
        return false;
    }

    id.push_back (rest.substr (start, end - start));

    if (!AppleWinParser::TryParseIdOrAll (id, command, error, args.context->GetNumberSyntax()))
    {
        return false;
    }

    command.text = (end == std::string::npos) ? std::string() : rest.substr (end);
    command.text.erase (0, std::min (command.text.size(), command.text.find_first_not_of (" \t")));
    command.text.erase (command.text.find_last_not_of (" \t") + 1);

    if (command.text.size() >= 2 && command.text.front() == '"' && command.text.back() == '"')
    {
        command.text = command.text.substr (1, command.text.size() - 2);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseIfClause
//
//  `IF <expression>` after a breakpoint's own arguments: the expression is
//  parsed into the command and the clause removed from the tokens, so the
//  arguments before it parse as they would without it. Its symbols are
//  resolved when the breakpoint is set, not here.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseIfClause (Tokens & tokens, DebugCommand & command, std::string & error, NumberSyntax syntax)
{
    auto         isIf       = [] (const std::string & token) { return AppleWinText::ToUpper (token) == "IF"; };
    auto         found      = std::find_if (tokens.begin(), tokens.end(), isIf);
    size_t       index      = (size_t) (found - tokens.begin());
    std::string  expression;
    HRESULT      hr         = S_OK;



    if (found == tokens.end())
    {
        return true;
    }

    expression = AppleWinText::Join (tokens, index + 1);
    tokens.resize (index);

    if (expression.empty())
    {
        error = "IF needs an expression.";
        return false;
    }

    hr = DebugExpressionEvaluator::Parse (expression, command.expression, error, syntax);
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::CutIfClause
//
//  The text before a word IF, its trailing spaces taken off; all of it when
//  there is no IF.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinBreakpointParser::CutIfClause (const std::string & text)
{
    std::string  upper = AppleWinText::ToUpper (text);
    size_t       at    = 0;



    while ((at = upper.find ("IF", at)) != std::string::npos)
    {
        bool  isStart = at == 0 || isspace ((unsigned char) upper[at - 1]);
        bool  isEnd   = at + 2 == upper.size() || isspace ((unsigned char) upper[at + 2]);



        if (isStart && isEnd)
        {
            std::string  head = text.substr (0, at);
            size_t       end  = head.find_last_not_of (" \t");



            return (end == std::string::npos) ? std::string() : head.substr (0, end + 1);
        }

        at += 2;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::NoteSymbol
//
//  An address given as a symbol, one address and no range, is noted with
//  the address, so the breakpoint can follow the symbol to where a rebuilt
//  program puts it. A number written like a symbol's name is a number: the
//  symbol must resolve to the address the text evaluated to.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinBreakpointParser::NoteSymbol (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command)
{
    Word  address = 0;



    if (!command.hasA2 && context.TryResolveSymbol (text, address) && address == command.a1)
    {
        command.symbol = text;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseOpcodes
//
//  BRKOP [opcode | mnemonic]...: each opcode a byte, into values, and each
//  mnemonic, upper case, into text separated by spaces, for the handler to
//  find the opcodes of on the machine's CPU.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseOpcodes (const Arguments & args, DebugCommand & command, std::string & error)
{
    Tokens  bytes;



    command.text.clear();

    for (const std::string & token : args.tokens)
    {
        if (!IsMnemonicToken (token))
        {
            bytes.push_back (token);
            continue;
        }

        command.text += (command.text.empty() ? "" : " ") + AppleWinText::ToUpper (token);
    }

    return AppleWinParser::TryParseValues (bytes, 0, AppleWinParser::ValueWidth::Bytes, *args.context, command, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::IsMnemonicToken
//
//  Three letters, or three and a digit as the 65C02's RMB0 and BBS7 are: no
//  byte is written that way, since a byte takes at most two hex digits.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::IsMnemonicToken (const std::string & token)
{
    constexpr size_t  kLetters = 3;
    bool              isWord   = token.size() == kLetters || token.size() == kLetters + 1;
    size_t            i        = 0;



    for (i = 0; isWord && i < kLetters; i++)
    {
        isWord = std::isalpha ((unsigned char) token[i]) != 0;
    }

    return isWord && (token.size() == kLetters || std::isdigit ((unsigned char) token[kLetters]) != 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseValueBreakpoint
//
//  BPMV addr[,len|:last] value: stop after a write sets a byte of the range to
//  value.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseValueBreakpoint (const Arguments & args, DebugCommand & command, std::string & error)
{
    static constexpr Word  kMaxByte = 0xFF;
    Word                   value    = 0;



    if (args.tokens.size() != 2)
    {
        error = "BPMV needs an address or a range, and a byte value.";
        return false;
    }

    if (!AppleWinParser::TryParseRange (args.tokens[0], *args.context, command, error) || !AppleWinParser::TryEvaluate (args.tokens[1], *args.context, value, error))
    {
        return false;
    }

    if (command.hasA2 && command.a2 < command.a1)
    {
        error = "The range ends before it starts or runs past $FFFF, so nothing could ever match it.";
        return false;
    }

    if (value > kMaxByte)
    {
        error = std::format ("${:X} is not a byte. BPMV compares one byte, $00-$FF.", value);
        return false;
    }

    command.hasA1  = true;
    command.values = { (Byte) value };
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseRegisterCondition
//
//  BPR's register, comparison and value, with or without spaces between
//  them: `BPR A=0`, `BPR A = 0` and `BPR A 0` are the same breakpoint. With
//  no operator the register is the first token and = is implied.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseRegisterCondition (const Tokens & tokens, DebugCommand & command, std::string & error, NumberSyntax syntax)
{
    static constexpr const char * kRegisters[] = { "A", "X", "Y", "P", "S", "PC" };
    std::string  joined = AppleWinText::Join (tokens, 0);
    size_t       op     = joined.find_first_of ("<>=!");
    Tokens       name   = (op == std::string::npos) ? Tokens (tokens.begin(), tokens.begin() + std::min<size_t> (tokens.size(), 1))
                                                    : AppleWinText::Split (joined.substr (0, op));



    if (name.size() != 1)
    {
        error = "BPR needs a register and a value.";
        return false;
    }

    command.text = AppleWinText::ToUpper (name[0]);

    if (std::find (std::begin (kRegisters), std::end (kRegisters), command.text) == std::end (kRegisters))
    {
        error = std::format ("{} is not a register. BPR takes A, X, Y, P, S or PC.", name[0]);
        return false;
    }

    if (op == std::string::npos)
    {
        return TryParseCondition (command.text, tokens, 1, command, error, syntax);
    }

    return TryParseCondition (command.text, AppleWinText::Split (joined.substr (op)), 0, command, error, syntax);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseWatchpointArguments
//
//  A range and an optional trailing BEFORE or AFTER; AFTER is the default.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseWatchpointArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    if (args.tokens.empty())
    {
        error = std::format ("{} needs an address.", AppleWinText::ToUpper (command.sourceName));
        return false;
    }

    command.text = "AFTER";

    if (args.tokens.size() > 1)
    {
        command.text = AppleWinText::ToUpper (args.tokens[1]);

        if (command.text != "BEFORE" && command.text != "AFTER")
        {
            error = "A data breakpoint's mode is BEFORE or AFTER.";
            return false;
        }
    }

    return AppleWinParser::TryParseRange (args.tokens[0], *args.context, command, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseBeamArguments
//
//  `BPBEAM line cycle`, `BPBEAM VBL` or `BPBEAM OFF`. VBL and OFF are put in
//  the text; the handler turns VBL into the frame's first blanked scanline.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseBeamArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    std::string  word = (args.tokens.size() == 1) ? AppleWinText::ToUpper (args.tokens[0]) : std::string();



    if (word == "VBL" || word == "OFF")
    {
        command.text = word;
        return true;
    }

    if (args.tokens.size() != 2)
    {
        error = std::format ("{} takes a scanline and a cycle, or VBL.", AppleWinText::ToUpper (command.sourceName));
        return false;
    }

    command.hasA1 = true;
    command.hasA2 = true;

    return AppleWinParser::TryEvaluate (args.tokens[0], *args.context, command.a1, error) &&
           AppleWinParser::TryEvaluate (args.tokens[1], *args.context, command.a2, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseFrameArguments
//
//  FRAME [count] counts frames as T counts steps.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseFrameArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    Word  value = 1;



    if (args.tokens.size() > 1)
    {
        error = std::format ("{} takes one argument, not {}.", AppleWinText::ToUpper (command.sourceName), args.tokens.size());
        return false;
    }

    if (!args.tokens.empty() && !AppleWinParser::TryEvaluate (args.tokens[0], *args.context, value, error))
    {
        return false;
    }

    if (value == 0)
    {
        error = std::format ("{} needs a count of at least 1.", AppleWinText::ToUpper (command.sourceName));
        return false;
    }

    command.count = value;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseFreeCondition
//
//  The whole text as one expression, as an IF clause is.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseFreeCondition (const std::string & text, DebugCommand & command, std::string & error, NumberSyntax syntax)
{
    HRESULT  hr = S_OK;



    if (text.find_first_not_of (" \t") == std::string::npos)
    {
        error = "BPR IF needs a condition.";
        return false;
    }

    hr = DebugExpressionEvaluator::Parse (text, command.expression, error, syntax);
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser::TryParseCondition
//
//  `[op] value` against subject, where op is = ! < > <= >= and defaults to
//  =. AppleWin's `!` means not equal.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinBreakpointParser::TryParseCondition (
    const std::string  & subject,
    const Tokens       & tokens,
    size_t               first,
    DebugCommand       & command,
    std::string        & error,
    NumberSyntax         syntax)
{
    static constexpr const char * kOperators[] = { "<=", ">=", "!=", "!", "=", "<", ">" };
    std::string  rest;
    std::string  op     = "=";
    HRESULT      hr     = S_OK;



    rest = AppleWinText::Join (tokens, first);

    for (const char * candidate : kOperators)
    {
        if (rest.starts_with (candidate))
        {
            op   = (std::string (candidate) == "!") ? "!=" : candidate;
            rest = rest.substr (strlen (candidate));
            rest = rest.substr (std::min (rest.size(), rest.find_first_not_of (' ')));
            break;
        }
    }

    if (rest.empty())
    {
        error = "A condition needs a value.";
        return false;
    }

    //  Parsed as @subject, so a symbol of the register's name is never read
    //  for it, and with the value in parentheses, so an operator in it that
    //  binds looser than the comparison stays in the value. The text stays
    //  as typed, which listings and saved definitions are built from.
    hr = DebugExpressionEvaluator::Parse ("@" + subject + op + "(" + rest + ")", command.expression, error, syntax);

    if (SUCCEEDED (hr))
    {
        command.expression.text = subject + op + rest;
    }

    return SUCCEEDED (hr);
}
