#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "Debugger/BreakpointDefinition.h"
#include "Debugger/DiskBreakSpecText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinition::MakeExecute
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDefinition BreakpointDefinition::MakeExecute (const std::string & address)
{
    BreakpointDefinition  definition;



    definition.address = address;
    return definition;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinition::MakeSwitch
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDefinition BreakpointDefinition::MakeSwitch (Type type, bool isOn)
{
    BreakpointDefinition  definition;



    definition.type  = type;
    definition.isOff = !isOn;
    return definition;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryMakeBreakpoint
//
//  The command a definition's type sets, with each field evaluated by the
//  function that reads that argument of a typed line. The caller's command
//  has the name replies quote. An error is worded for the field it is in.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryMakeBreakpoint (const BreakpointDefinition & definition, const IDebugExpressionContext & context, DebugCommand & command, std::string & error)
{
    using Type = BreakpointDefinition::Type;

    static constexpr Word  kMaxByte = 0xFF;
    NumberSyntax           syntax   = context.GetNumberSyntax();
    bool                   takesIf  = definition.type != Type::Condition && definition.type != Type::UnwrittenRead &&
                                      definition.type != Type::VideoLine && definition.type != Type::Beam;
    bool                   isSwitch = definition.type == Type::Brk || definition.type == Type::Interrupt ||
                                      definition.type == Type::BasicError || definition.type == Type::UnwrittenRead;
    Word                   value    = 0;
    HRESULT                hr       = S_OK;



    if (definition.isOff)
    {
        return TryMakeSwitchOff (definition, command, error);
    }

    if (takesIf && !definition.condition.empty())
    {
        hr = DebugExpressionEvaluator::Parse (definition.condition, command.expression, error, syntax);

        if (FAILED (hr))
        {
            return false;
        }
    }

    if (isSwitch)
    {
        command.verb = (definition.type == Type::Brk)        ? DebugVerb::BreakOnBrk
                     : (definition.type == Type::Interrupt)  ? DebugVerb::BreakOnInterrupt
                     : (definition.type == Type::BasicError) ? DebugVerb::BreakOnBasicError
                     :                                         DebugVerb::BreakOnUnwrittenRead;
        command.text = "ON";
        return true;
    }

    switch (definition.type)
    {
    case Type::Read:
    case Type::Write:
    case Type::ReadWrite:
        command.verb = (definition.type == Type::Read)  ? DebugVerb::SetReadWatchpoint
                     : (definition.type == Type::Write) ? DebugVerb::SetWriteWatchpoint
                     :                                    DebugVerb::SetMemoryWatchpoint;
        command.text = definition.isBefore ? "BEFORE" : "AFTER";
        return TryParseRange (definition.address, context, command, error) && IsLiveRange (command, error);

    case Type::Value:
        command.verb = DebugVerb::SetValueBreakpoint;

        if (!TryParseRange (definition.address, context, command, error) || !IsLiveRange (command, error) ||
            !TryEvaluate (definition.value, context, value, error))
        {
            return false;
        }

        if (value > kMaxByte)
        {
            error = std::format ("${:X} is not a byte. The value is one byte, $00-$FF.", value);
            return false;
        }

        command.hasA1  = true;
        command.values = { (Byte) value };
        return true;

    case Type::Condition:
        command.verb = DebugVerb::SetRegisterBreakpoint;

        if (definition.condition.empty())
        {
            error = "The condition is empty.";
            return false;
        }

        return TryParseFreeCondition (definition.condition, command, error, syntax);

    case Type::Opcode:
        command.verb = DebugVerb::BreakOnOpcode;
        return TryParseOpcodes ({ nullptr, Split (definition.value), definition.value, &context }, command, error);

    case Type::Disk:
        command.verb = DebugVerb::SetDiskBreakpoint;
        return TryMakeDiskSpec (definition, context, command, error);

    case Type::BasicLine:
    case Type::BasicVariable:
        command.verb = (definition.type == Type::BasicLine) ? DebugVerb::SetBasicLineBreakpoint : DebugVerb::SetBasicVariableBreakpoint;
        command.text = ToUpper (definition.address);
        return true;

    case Type::VideoLine:
        command.verb = DebugVerb::BreakOnVideoLine;
        return TryParseRange (definition.address, context, command, error) && IsLiveRange (command, error);

    case Type::Beam:
        command.verb = DebugVerb::BreakOnBeam;

        if (ToUpper (definition.address) == "VBL")
        {
            command.text = "VBL";
            return true;
        }

        command.hasA1 = true;
        command.hasA2 = true;
        return TryEvaluate (definition.address, context, command.a1, error) && TryEvaluate (definition.value, context, command.a2, error);

    case Type::SourceLine:
        command.verb = DebugVerb::SetSourceBreakpoint;
        command.text = definition.file;

        if (!TryParseCount (definition.address, command.count, syntax))
        {
            error = std::format ("{} is not a line number.", definition.address);
            return false;
        }

        return true;

    default:
        return TryMakeExecute (definition, context, command, error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryMakeExecute
//
//  An address or a range; a comparison, as `>300`, is a condition on the PC;
//  `file:line` typed as the address is a source line.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryMakeExecute (const BreakpointDefinition & definition, const IDebugExpressionContext & context, DebugCommand & command, std::string & error)
{
    static constexpr std::string_view  kOperators = "<>=!";
    Tokens                             tokens     = Split (definition.address);



    command.verb = DebugVerb::SetBreakpoint;

    if (tokens.empty())
    {
        error = "The address is empty.";
        return false;
    }

    if (kOperators.find (tokens[0][0]) != std::string_view::npos)
    {
        if (!definition.condition.empty())
        {
            error = "A comparison is the whole condition, so the Condition field must be empty.";
            return false;
        }

        command.verb = DebugVerb::SetConditionalBreakpoint;
        return TryParseCondition ("PC", tokens, 0, command, error, context.GetNumberSyntax());
    }

    if (tokens.size() > 1)
    {
        error = "The address is one address or range.";
        return false;
    }

    if (TryParseSourceLine (tokens[0], context, command))
    {
        return true;
    }

    if (!TryParseRange (tokens[0], context, command, error))
    {
        return false;
    }

    NoteSymbol (tokens[0], context, command);
    return IsLiveRange (command, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryMakeSwitchOff
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryMakeSwitchOff (const BreakpointDefinition & definition, DebugCommand & command, std::string & error)
{
    using Type = BreakpointDefinition::Type;



    switch (definition.type)
    {
    case Type::Brk:           command.verb = DebugVerb::BreakOnBrk;           break;
    case Type::Interrupt:     command.verb = DebugVerb::BreakOnInterrupt;     break;
    case Type::BasicError:    command.verb = DebugVerb::BreakOnBasicError;    break;
    case Type::UnwrittenRead: command.verb = DebugVerb::BreakOnUnwrittenRead; break;
    case Type::VideoLine:     command.verb = DebugVerb::BreakOnVideoLine;     break;
    case Type::Beam:          command.verb = DebugVerb::BreakOnBeam;          break;
    default:
        error = "This type of breakpoint is cleared by its id.";
        return false;
    }

    command.text = "OFF";
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryMakeDiskSpec
//
//  The drive and the event are the controls' choices; the arguments are the
//  text typed for that event.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryMakeDiskSpec (const BreakpointDefinition & definition, const IDebugExpressionContext & context, DebugCommand & command, std::string & error)
{
    std::span<const DiskBreakSpecText::EventWord>  words  = DiskBreakSpecText::GetEventWords();
    Tokens                                         tokens = Split (definition.arguments);
    size_t                                         at     = 0;
    DiskBreakSpec                                  spec;



    if (definition.event < 0 || (size_t) definition.event >= words.size())
    {
        error = "The event is not one a disk breakpoint has.";
        return false;
    }

    spec.drive = (definition.drive == 1) ? DiskDriveFilter::Drive1 : (definition.drive == 2) ? DiskDriveFilter::Drive2 : spec.drive;
    spec.event = words[(size_t) definition.event].event;

    if (!TryParseDiskEventArguments (tokens, at, context, spec) || !TryParseDiskOptions (tokens, at, spec) || at != tokens.size())
    {
        error = words[(size_t) definition.event].syntax;
        return false;
    }

    command.disk = spec;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::IsLiveRange
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::IsLiveRange (const DebugCommand & command, std::string & error)
{
    if (command.hasA2 && command.a2 < command.a1)
    {
        error = "The range ends before it starts or runs past $FFFF, so nothing could ever match it.";
        return false;
    }

    return true;
}
