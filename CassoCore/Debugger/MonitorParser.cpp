#include "Pch.h"

#include "Debugger/MonitorParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kCommandCharacters
//
//  The union of every shipped Apple II ROM's command table, as 7-bit codes.
//  No single ROM carries all of them: the ][+ and //e dropped step and trace,
//  and only the Enhanced //e and //c reach the mini-assembler with `!`.
//  Casso offers the union on every machine (FR-016).
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte  s_kCommandCharacters[] =
{
    0x02,   // Ctrl+B, BASIC cold start
    0x03,   // Ctrl+C, BASIC warm start
    0x05,   // Ctrl+E, show registers and arm the register edit
    0x0B,   // Ctrl+K, input hook to a slot
    0x0D,   // Return, continue examining
    0x10,   // Ctrl+P, output hook to a slot
    0x19,   // Ctrl+Y, the user vector at $03F8
    0x20,   // space, continue examining
    '!',    // the mini-assembler
    '+',
    '-',
    '.',    // range delimiter
    ':',    // deposit, or set the registers after Ctrl+E
    '<',    // destination delimiter
    'G',
    'I',
    'L',
    'M',
    'N',
    'R',
    'S',
    'T',
    'V',
    'W',
};



static constexpr Word  s_kIntegerAssembler = 0xF666;
static constexpr Word  s_kBytesPerLine     = 8;





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::Parse
//
//  Left to right, one character at a time. Hex digits accumulate; `.`, `<`,
//  `+` and `-` move what has accumulated along; a command character spends
//  it. `:`, `R` and `W` take the rest of the line, because what follows them
//  is a byte list or a file name rather than more commands.
//
////////////////////////////////////////////////////////////////////////////////

MonitorParseResult MonitorParser::Parse (const std::string & line, MonitorState & state)
{
    MonitorParseResult  result;
    Scan                scan;
    size_t              index    = 0;
    size_t              opening  = line.find_first_not_of (" \t");



    //  A `/` line was never the Monitor's.
    if (opening != std::string::npos && line[opening] == '/')
    {
        result.status       = ParseStatus::Ok;
        result.appleWinLine = Trim (line.substr (opening + 1));
        return result;
    }

    while (index < line.size())
    {
        DebugCommand  command;
        char          character = 0;
        int           digit     = 0;



        if (!TryReadCharacter (line, index, character, result.error))
        {
            result.status = ParseStatus::Invalid;
            result.commands.clear();
            return result;
        }

        if (TryHexDigit (character, digit))
        {
            scan.value = (Word) ((scan.value.value_or (0) << 4) | digit);
            continue;
        }

        switch (character)
        {
        //  A `.` with nothing in front of it continues from the last byte
        //  examined, which is what makes `.30F` mean "on to $030F".
        case '.':
            scan.first = scan.value.has_value() ? *scan.value : (Word) (state.lastExamined + 1);
            scan.value.reset();
            continue;

        case '<':
            scan.dest = scan.value.value_or (0);
            scan.value.reset();
            continue;

        case '+':
        case '-':
            scan.first = scan.value.value_or (0);
            scan.op    = character;
            scan.value.reset();
            continue;

        //  Return is $0D, which is why it is not listed again as a control
        //  command: reaching the end of what was typed and pressing Return
        //  are the same thing to the scan.
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            FlushExamine (scan, state, result);
            continue;

        //  Everything after the colon is the byte list.
        case ':':
        {
            std::vector<Byte>  values;

            if (!TryParseBytes (line.substr (index), values))
            {
                result.error  = "A deposit takes hex bytes.";
                result.status = ParseStatus::Invalid;
                result.commands.clear();
                return result;
            }

            if (state.registerEditPending)
            {
                command        = MakeCommand (DebugVerb::EditRegisters, ':');
                command.values = values;

                state.registerEditPending = false;
            }
            else
            {
                command        = MakeCommand (DebugVerb::Deposit, ':');
                command.a1     = scan.value.value_or (state.storeAddress);
                command.hasA1  = true;
                command.values = values;

                state.storeAddress = (Word) (command.a1 + values.size());
            }

            index = line.size();
            break;
        }

        //  And everything after R or W is the file name.
        case 'R':
        case 'W':
            command      = MakeCommand (character == 'R' ? DebugVerb::ReadFile : DebugVerb::WriteFile, character);
            command.text = Unquote (Trim (line.substr (index)));
            ApplyRange (scan, command);

            index = line.size();
            break;

        //  `S` STEPS UNLESS A VALUE AND A RANGE CAME FIRST, which is the
        //  Monitor's own rule: the letter is the same and the scan is not.
        case 'S':
            if (scan.dest.has_value())
            {
                command = MakeCommand (DebugVerb::SearchMemory, 'S');
                ApplyRange (scan, command);
                command.values.push_back ((Byte) (*scan.dest & 0xFF));

                if (*scan.dest > 0xFF)
                {
                    command.values.push_back ((Byte) (*scan.dest >> 8));
                }
            }
            else
            {
                command       = MakeCommand (DebugVerb::StepInto, 'S');
                command.a1    = scan.value.value_or (0);
                command.hasA1 = scan.value.has_value();
                command.count = 1;
            }

            break;

        case 'M':
        case 'V':
            command = MakeCommand (character == 'M' ? DebugVerb::MoveMemory : DebugVerb::Verify, character);
            ApplyRange (scan, command);

            if (scan.dest.has_value())
            {
                command.a3    = *scan.dest;
                command.hasA3 = true;
            }

            break;

        //  `F666G` IS AN ALIAS FOR `!`, NOT A JUMP. $F666 is the
        //  mini-assembler only in the original ]['s ROM; on the other four
        //  machines those bytes are Applesoft.
        case 'G':
            if (scan.value.has_value() && *scan.value == s_kIntegerAssembler)
            {
                command = MakeCommand (DebugVerb::EnterAssembler, 'G');
            }
            else
            {
                command       = MakeCommand (DebugVerb::Go, 'G');
                command.a3    = scan.value.value_or (0);
                command.hasA3 = scan.value.has_value();
            }

            break;

        case 'L':
            command = MakeCommand (DebugVerb::List, 'L');
            ApplyRange (scan, command);
            break;

        case 'T':
            command       = MakeCommand (DebugVerb::Trace, 'T');
            command.a1    = scan.value.value_or (0);
            command.hasA1 = scan.value.has_value();
            break;

        case 'I':
            command = MakeCommand (DebugVerb::SetInverse, 'I');
            break;

        case 'N':
            command = MakeCommand (DebugVerb::SetNormal, 'N');
            break;

        case '!':
            command = MakeCommand (DebugVerb::EnterAssembler, '!');
            break;

        case 0x0B:
            command       = MakeCommand (DebugVerb::SetInputSlot, character);
            command.count = scan.value.value_or (0);
            break;

        case 0x10:
            command       = MakeCommand (DebugVerb::SetOutputSlot, character);
            command.count = scan.value.value_or (0);
            break;

        case 0x02:
            command = MakeCommand (DebugVerb::BasicColdStart, character);
            break;

        case 0x03:
            command = MakeCommand (DebugVerb::BasicWarmStart, character);
            break;

        case 0x19:
            command = MakeCommand (DebugVerb::UserVector, character);
            break;

        case 0x05:
            command                   = MakeCommand (DebugVerb::ShowRegistersForEdit, character);
            state.registerEditPending = true;
            break;

        default:
            result.error  = std::format ("{} is not a Monitor command.", MakeCommand (DebugVerb::None, character).sourceName);
            result.status = ParseStatus::Invalid;
            result.commands.clear();
            return result;
        }

        result.commands.push_back (command);
        scan = Scan();
    }

    if (scan.op != 0)
    {
        DebugCommand  arithmetic = MakeCommand (DebugVerb::Arithmetic, scan.op);

        arithmetic.a1    = scan.first.value_or (0);
        arithmetic.a2    = scan.value.value_or (0);
        arithmetic.hasA1 = true;
        arithmetic.hasA2 = true;
        arithmetic.text  = std::string (1, scan.op);

        result.commands.push_back (arithmetic);
    }
    else
    {
        FlushExamine (scan, state, result);
    }

    result.status = result.commands.empty() ? ParseStatus::Empty : ParseStatus::Ok;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::FlushExamine
//
//  What an accumulated address means when nothing spent it: examine.
//
//  With nothing accumulated it is the Monitor's continuation -- a bare
//  Return or space shows the next line of eight bytes -- but only when the
//  line has produced no other command, so a separator between two commands
//  does not turn into a third.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorParser::FlushExamine (Scan & scan, MonitorState & state, MonitorParseResult & result)
{
    DebugCommand  command = MakeCommand (DebugVerb::Examine, ' ');



    if (!scan.value.has_value() && !scan.first.has_value())
    {
        if (!result.commands.empty())
        {
            return;
        }

        command.a1    = (Word) (state.lastExamined + 1);
        command.a2    = (Word) (command.a1 + s_kBytesPerLine - 1);
        command.hasA1 = true;
        command.hasA2 = true;
    }
    else
    {
        ApplyRange (scan, command);
    }

    state.lastExamined = command.hasA2 ? command.a2 : command.a1;

    result.commands.push_back (command);
    scan = Scan();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::ApplyRange
//
//  The same fields AppleWin's own range commands fill, so one handler serves
//  both modes.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorParser::ApplyRange (const Scan & scan, DebugCommand & command)
{
    if (scan.first.has_value())
    {
        command.a1    = *scan.first;
        command.a2    = scan.value.value_or (*scan.first);
        command.hasA1 = true;
        command.hasA2 = true;
    }
    else if (scan.value.has_value())
    {
        command.a1    = *scan.value;
        command.hasA1 = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::TryReadCharacter
//
//  One character, uppercased, with `^X` folded to its control code so a
//  script can write what a terminal sends.
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorParser::TryReadCharacter (const std::string & line, size_t & index, char & character, std::string & error)
{
    static constexpr char  kCaret = '^';
    char                   raw    = line[index];
    bool                   isPair = false;



    if (raw != kCaret)
    {
        character = (char) toupper ((unsigned char) raw);
        index    += 1;
        return true;
    }

    isPair = index + 1 < line.size() && isalpha ((unsigned char) line[index + 1]) != 0;

    if (!isPair)
    {
        error = "^ must be followed by a letter.";
        return false;
    }

    character = (char) (toupper ((unsigned char) line[index + 1]) - 'A' + 1);
    index    += 2;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::TryHexDigit
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorParser::TryHexDigit (char character, int & digit)
{
    if (character >= '0' && character <= '9')
    {
        digit = character - '0';
        return true;
    }

    if (character >= 'A' && character <= 'F')
    {
        digit = character - 'A' + 10;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::TryParseBytes
//
//  Whitespace-separated hex bytes. An empty list is allowed, which is how a
//  bare `:` reads.
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorParser::TryParseBytes (const std::string & text, std::vector<Byte> & values)
{
    std::istringstream  stream (text);
    std::string         token;



    while (stream >> token)
    {
        Word  value = 0;

        for (char character : token)
        {
            int  digit = 0;

            if (!TryHexDigit ((char) toupper ((unsigned char) character), digit))
            {
                return false;
            }

            value = (Word) ((value << 4) | digit);
        }

        if (token.size() > 2)
        {
            return false;
        }

        values.push_back ((Byte) value);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::MakeCommand
//
//  sourceName is what the reader typed, so an error can quote it: `^E` for a
//  control code and the character itself otherwise.
//
////////////////////////////////////////////////////////////////////////////////

DebugCommand MonitorParser::MakeCommand (DebugVerb verb, char source)
{
    static constexpr char  kFirstPrintable = 0x20;
    DebugCommand           command;



    command.verb = verb;
    command.mode = CommandMode::Monitor;

    command.sourceName = (source < kFirstPrintable)
                       ? std::string ("^") + (char) (source + 'A' - 1)
                       : std::string (1, source);

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::Trim
//
////////////////////////////////////////////////////////////////////////////////

std::string MonitorParser::Trim (const std::string & text)
{
    size_t  first = text.find_first_not_of (" \t\r\n");
    size_t  last  = text.find_last_not_of  (" \t\r\n");



    return (first == std::string::npos) ? std::string() : text.substr (first, last - first + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::Unquote
//
//  A file name with spaces in it is quoted; the quotes are not part of it.
//
////////////////////////////////////////////////////////////////////////////////

std::string MonitorParser::Unquote (const std::string & text)
{
    bool  isQuoted = text.size() >= 2
                  && (text.front() == '"' || text.front() == '\'')
                  && text.back() == text.front();



    return isQuoted ? text.substr (1, text.size() - 2) : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::GetCommandCharacters
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> MonitorParser::GetCommandCharacters()
{
    return std::span<const Byte> (s_kCommandCharacters);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::IsCommandCharacter
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorParser::IsCommandCharacter (Byte character)
{
    std::span<const Byte>  characters = GetCommandCharacters();



    return std::find (characters.begin(), characters.end(), character) != characters.end();
}
