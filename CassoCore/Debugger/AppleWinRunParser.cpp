#include "Pch.h"

#include "Debugger/AppleWinArgumentForms.h"
#include "Debugger/AppleWinMemoryParser.h"
#include "Debugger/AppleWinRunParser.h"
#include "Debugger/AppleWinViewParser.h"

#include "Debugger/AppleWinBreakpointParser.h"
#include "Debugger/AppleWinText.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/AppleWinCommandTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRunParser::TryParseRunArguments
//
//  G and GG take a stop address and a skip range; T, TL, P and RTS a count;
//  R shows or sets a register; =, JSR and IN an address; OUT an address and
//  a value; KEY and PUSH values; TF a file.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinRunParser::TryParseRunArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    DebugCommand  skip;
    Word          value = 0;



    switch (command.verb)
    {
    case DebugVerb::Go:
    case DebugVerb::GoFullSpeed:
        if (!args.tokens.empty() && !AppleWinArgumentForms::TryEvaluate (args.tokens[0], *args.context, command.a1, error)) { return false; }
        command.hasA1 = !args.tokens.empty();
        if (args.tokens.size() > 1 && !AppleWinArgumentForms::TryParseRange (args.tokens[1], *args.context, skip, error))   { return false; }
        command.a2    = skip.a1;
        command.a3    = skip.hasA2 ? skip.a2 : skip.a1;

        if (command.a3 < command.a2)
        {
            error = "The skip range ends before it starts.";
            return false;
        }

        command.hasA2 = args.tokens.size() > 1;
        command.hasA3 = command.hasA2;
        return true;

    case DebugVerb::StepInto:
    case DebugVerb::StepOver:
    case DebugVerb::StepOut:
        if (!args.tokens.empty() && !AppleWinArgumentForms::TryEvaluate (args.tokens[0], *args.context, value, error)) { return false; }
        command.count = args.tokens.empty() ? 1 : value;
        return true;

    case DebugVerb::ShowRegisters:
        return TryParseRegisterArguments (args, command, error);

    case DebugVerb::Disassemble:
        return args.tokens.empty() || AppleWinArgumentForms::TryParseRange (args.tokens[0], *args.context, command, error);

    case DebugVerb::SetProgramCounter:
    case DebugVerb::CallSubroutine:
    case DebugVerb::ReadIo:
        command.hasA1 = !args.tokens.empty();
        return args.tokens.empty() || AppleWinArgumentForms::TryEvaluate (args.tokens[0], *args.context, command.a1, error);

    case DebugVerb::WriteIo:
        command.hasA1 = !args.tokens.empty();

        if (!command.hasA1)
        {
            error = "OUT needs an address and one or more values.";
            return false;
        }

        return AppleWinArgumentForms::TryEvaluate (args.tokens[0], *args.context, command.a1, error) &&
               AppleWinArgumentForms::TryParseValues (args.tokens, 1, AppleWinArgumentForms::ValueWidth::Bytes, *args.context, command, error);

    case DebugVerb::InjectKey:
    case DebugVerb::PushStack:
        return AppleWinArgumentForms::TryParseValues (args.tokens, 0, AppleWinArgumentForms::ValueWidth::Bytes, *args.context, command, error);

    //  NOP acts on the instruction at PC; it has no address to take.
    case DebugVerb::WriteNop:
        if (!args.tokens.empty())
        {
            error = std::format ("{} takes no arguments. It replaces the instruction at PC.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        return true;

    default:
        command.text = args.rest;
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRunParser::TryParseRegisterArguments
//
//  R alone shows the registers. `R A 41`, `R A=41` and `R PC = FA62` set one.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinRunParser::TryParseRegisterArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
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
    name  = AppleWinText::ToUpper (joined.substr (0, split));
    value = (split == std::string::npos) ? std::string() : joined.substr (split);

    if (name.empty())
    {
        error = "R takes a register and a value. The registers are A, X, Y, P, S, and PC.";
        return false;
    }

    if (std::find (std::begin (kRegisters), std::end (kRegisters), name) == std::end (kRegisters))
    {
        error = std::format ("{} is not a register. The registers are A, X, Y, P, S, and PC.", name);
        return false;
    }

    command.verb  = DebugVerb::SetRegister;
    command.text  = name;
    command.hasA1 = true;
    return AppleWinArgumentForms::TryEvaluate (value, *args.context, command.a1, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRunParser::TryParseFlagArguments
//
//  CLC and SEC carry the flag in the name, RC and SC likewise; CL and SE
//  take it as an argument.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinRunParser::TryParseFlagArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    static constexpr std::string_view  kFlagLetters = "CZIDBRVN";
    std::string  name = AppleWinText::ToUpper (command.sourceName);



    command.text = (name.size() == 2 && args.entry->aliasOf == nullptr)
                 ? (args.tokens.empty() ? std::string() : AppleWinText::ToUpper (args.tokens[0]))
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
//  AppleWinRunParser::TryParseEngineArguments
//
//  MODE [APPLEWIN | MONITOR | GSSQUARED | WINDBG | CASSO], OUTPUT with the
//  same names, and BUDGET n with n in decimal, as --max-cycles takes it.
//  PATCH takes its arguments as ME does, and PROFILE, STOPWATCH, VIDEOLOG and
//  SOUNDLOG their arguments as text.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinRunParser::TryParseEngineArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    std::string  mode;



    if (command.verb == DebugVerb::ListStepFilter)
    {
        return TryParseSkipArguments (args, command, error);
    }

    if (command.verb == DebugVerb::ShowCallStack)
    {
        return TryParseCallsArguments (args, command, error);
    }

    if (command.verb == DebugVerb::ShowHistory)
    {
        return TryParseHistoryArguments (args, command, error);
    }

    if (command.verb == DebugVerb::ListPanels)
    {
        return AppleWinViewParser::TryParsePanelArguments (args, command, error);
    }

    if (command.verb == DebugVerb::PatchBytes)
    {
        return AppleWinMemoryParser::TryParseMemoryArguments (args, command, error);
    }

    //  PROJECT's subcommand and file name, the name's case as typed, and the
    //  machine commands' text, which PASTE types and SCREENSHOT saves to.
    if (command.verb == DebugVerb::Project        || command.verb == DebugVerb::ResetMachine   ||
        command.verb == DebugVerb::PasteText      || command.verb == DebugVerb::ShowScreen     ||
        command.verb == DebugVerb::SaveScreenshot || command.verb == DebugVerb::ExitCasso)
    {
        command.text = args.rest;
        return true;
    }

    //  A file name keeps its case.
    if (command.verb == DebugVerb::SaveState || command.verb == DebugVerb::LoadState ||
        command.verb == DebugVerb::LoadBasic || command.verb == DebugVerb::SaveBasic)
    {
        command.text = args.rest;

        if (command.text.empty())
        {
            error = std::format ("{} needs a file name.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        return true;
    }

    if (command.verb == DebugVerb::Profile  || command.verb == DebugVerb::Stopwatch ||
        command.verb == DebugVerb::VideoLog || command.verb == DebugVerb::SoundLog  ||
        command.verb == DebugVerb::Seek     || command.verb == DebugVerb::ListBreakpointLogs)
    {
        command.text = args.rest;
        return true;
    }

    if (command.verb == DebugVerb::ShowMode && !args.tokens.empty())
    {
        if (!CommandModeNames::TryParse (args.tokens[0], command.mode))
        {
            error = std::format ("The modes are {}.", CommandModeNames::GetList (true));
            return false;
        }

        command.verb = DebugVerb::SetMode;
        return true;
    }

    if (command.verb == DebugVerb::ShowOutputFormat && !args.tokens.empty())
    {
        if (!CommandModeNames::TryParse (args.tokens[0], command.output))
        {
            error = std::format ("The output formats are {}.", CommandModeNames::GetList (true));
            return false;
        }

        command.verb = DebugVerb::SetOutputFormat;
        return true;
    }

    if (command.verb == DebugVerb::ShowSource && !args.tokens.empty())
    {
        mode = AppleWinText::ToUpper (args.tokens[0]);

        //  SRC CLEAR unloads the debug file.
        if (mode == "CLEAR")
        {
            command.text = mode;
            return true;
        }

        if (mode != "ON" && mode != "OFF")
        {
            error = "Use SRC ON for source mode, SRC OFF for disassembly mode, SRC CLEAR to unload the debug file, or SRC alone for the line at PC.";
            return false;
        }

        command.verb  = DebugVerb::SetSourceStepping;
        command.count = (mode == "ON") ? 1 : 0;
        return true;
    }

    if (command.verb == DebugVerb::SetBudget)
    {
        if (args.tokens.empty() || !AppleWinArgumentForms::TryParseCount (args.tokens[0], command.count, args.context->GetNumberSyntax()))
        {
            error = "BUDGET takes a number of cycles in decimal, up to 4294967295. Use BUDGET 0 to remove the budget.";
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRunParser::TryParseCallsArguments
//
//  CALLS shows the chain; CALLS MODE reports the mechanism, and CALLS MODE
//  RECORDED|WALK|HYBRID chooses it. The text carries the name chosen, and is
//  empty for the report.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinRunParser::TryParseCallsArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    std::string  mechanism;



    if (args.tokens.empty())
    {
        return true;
    }

    if (AppleWinText::ToUpper (args.tokens[0]) != "MODE" || args.tokens.size() > 2)
    {
        error = "Use CALLS to show the call stack, or CALLS MODE RECORDED|WALK|HYBRID to choose how it is found.";
        return false;
    }

    command.verb = DebugVerb::SetCallStackMode;

    if (args.tokens.size() == 1)
    {
        return true;
    }

    mechanism = AppleWinText::ToUpper (args.tokens[1]);

    if (mechanism != "RECORDED" && mechanism != "WALK" && mechanism != "HYBRID")
    {
        error = "The call-stack mechanisms are RECORDED, WALK and HYBRID.";
        return false;
    }

    command.text = mechanism;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRunParser::TryParseHistoryArguments
//
//  HISTORY ON and HISTORY OFF switch the trace; HISTORY SAVE file writes it;
//  HISTORY cycle [count] lists count entries from the first at or after the
//  cycle, and a bare HISTORY the newest. The numbers are decimal, since a
//  cycle count runs past what a 16-bit expression has room for.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinRunParser::TryParseHistoryArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    static constexpr const char * kUsage = "HISTORY takes ON, OFF, SAVE and a file name, or a cycle and a count in decimal.";
    std::string                   first;
    uint64_t                      value  = 0;
    NumberSyntax                  syntax = args.context->GetNumberSyntax();



    if (args.tokens.empty())
    {
        return true;
    }

    first = AppleWinText::ToUpper (args.tokens[0]);

    if ((first == "ON" || first == "OFF") && args.tokens.size() == 1)
    {
        command.verb  = DebugVerb::SetHistory;
        command.count = (first == "ON") ? 1 : 0;
        return true;
    }

    if (first == "SAVE")
    {
        command.verb = DebugVerb::SaveHistory;
        command.text = AppleWinArgumentForms::GetTextAfterFirstWord (args);

        if (command.text.empty())
        {
            error = "HISTORY SAVE needs a file name.";
            return false;
        }

        return true;
    }

    if (args.tokens.size() > 2 || !AppleWinArgumentForms::TryParseDecimal (args.tokens[0], value, syntax))
    {
        error = kUsage;
        return false;
    }

    command.fromCycle = value;

    if (args.tokens.size() == 2)
    {
        if (!AppleWinArgumentForms::TryParseDecimal (args.tokens[1], value, syntax) || value == 0 || value > UINT32_MAX)
        {
            error = kUsage;
            return false;
        }

        command.count = (uint32_t) value;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRunParser::TryParseSkipArguments
//
//  SKIP lists the step filter; SKIP name|addr|first.last adds to it; SKIP -
//  name removes one; SKIP CLEAR empties it. A name is resolved here, so one
//  no symbol table holds is an error. The text keeps the name as typed, and
//  is empty for an address or a range.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinRunParser::TryParseSkipArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    std::string  first;
    std::string  ignored;



    if (args.tokens.empty())
    {
        return true;
    }

    first = args.tokens[0];

    if (AppleWinText::ToUpper (first) == "CLEAR")
    {
        command.verb = DebugVerb::ClearStepFilter;
        return true;
    }

    if (first.starts_with ('-'))
    {
        command.verb = DebugVerb::RemoveStepFilter;
        command.text = (first.size() > 1) ? first.substr (1) : (args.tokens.size() > 1) ? args.tokens[1] : std::string();

        if (command.text.empty())
        {
            error = "SKIP - takes the name, address or range to remove.";
            return false;
        }

        //  A name whose symbol is gone still removes by name, so a failure to
        //  evaluate here is not an error.
        if (!TryParseSkipRange (command.text, *args.context, command, ignored))
        {
            command.hasA1 = false;
            command.hasA2 = false;
        }

        return true;
    }

    command.verb = DebugVerb::AddStepFilter;

    if (!AppleWinArgumentForms::IsNumberOrRange (first, args.context->GetNumberSyntax()))
    {
        command.text = first;
    }

    return TryParseSkipRange (first, *args.context, command, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRunParser::TryParseSkipRange
//
//  `addr` or `first.last`, into a1 and a2; a single address is a range of
//  one.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinRunParser::TryParseSkipRange (
    const std::string              & text,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    size_t  period = text.find ('.');



    command.hasA1 = true;
    command.hasA2 = true;

    if (period == std::string::npos)
    {
        if (!AppleWinArgumentForms::TryEvaluate (text, context, command.a1, error))
        {
            return false;
        }

        command.a2 = command.a1;
        return true;
    }

    if (!AppleWinArgumentForms::TryEvaluate (text.substr (0, period), context, command.a1, error) ||
        !AppleWinArgumentForms::TryEvaluate (text.substr (period + 1), context, command.a2, error))
    {
        return false;
    }

    if (command.a2 < command.a1)
    {
        error = std::format ("{} ends before it begins.", text);
        return false;
    }

    return true;
}
