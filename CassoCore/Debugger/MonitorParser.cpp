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
//  The scan works on a copy of the state, kept only when the line parses: a
//  line with an error runs nothing, so it changes nothing either.
//
////////////////////////////////////////////////////////////////////////////////

MonitorParseResult MonitorParser::Parse (const std::string & line, MonitorState & state)
{
    static constexpr char  kShowRegisters = 0x05;
    MonitorParseResult     result;
    MonitorState           scratch        = state;
    Scan                   scan;
    size_t                 index          = 0;
    size_t                 opening        = line.find_first_not_of (" \t");



    //  A `/` line was never the Monitor's. It is input other than a `:`, so
    //  it ends a register edit `^E` armed.
    if (opening != std::string::npos && line[opening] == '/')
    {
        result.status             = ParseStatus::Ok;
        result.appleWinLine       = Trim (line.substr (opening + 1));
        state.registerEditPending = false;
        return result;
    }

    while (index < line.size())
    {
        DebugCommand  command;
        char          character   = 0;
        int           digit       = 0;
        bool          isSeparator = false;



        if (!TryReadCharacter (line, index, character, result.error))
        {
            result.status = ParseStatus::Invalid;
            result.commands.clear();
            return result;
        }

        isSeparator = character == ' ' || character == '\t' || character == '\r' || character == '\n';

        //  Anything typed but a separator, a `:` or `^E` itself is input
        //  other than a `:`, and ends the register edit `^E` armed.
        if (!isSeparator && character != ':' && character != kShowRegisters)
        {
            scratch.registerEditPending = false;
        }

        if (TryHexDigit (character, digit))
        {
            scan.value = (Word) ((scan.value.value_or (0) << 4) | digit);
            continue;
        }

        //  The digits, if there were any, end here.
        NoteTypedValue (scan, scratch);

        switch (character)
        {
        //  A `.` with nothing in front of it continues from the last byte
        //  examined, which is what makes `.30F` mean "on to $030F". A second
        //  `.` keeps the start the first one set, as the Monitor's A1 stays.
        case '.':
            if (scan.value.has_value())
            {
                scan.first = scan.value;
            }
            else if (!scan.first.has_value())
            {
                scan.first = (Word) (scratch.lastExamined + 1);
            }

            scan.value.reset();
            continue;

        //  The destination is the Monitor's A4, which M and V move on past
        //  the bytes they cover, so it outlasts the line.
        case '<':
            scan.dest  = scan.value.value_or (0);
            scratch.a4 = *scan.dest;
            scan.value.reset();
            continue;

        //  A second operator keeps the first operand, as the Monitor's A1
        //  does: `5+3-1` is 5-1.
        case '+':
        case '-':
            if (scan.op == 0)
            {
                scan.first = scan.value.value_or (0);
            }

            scan.op = character;
            scan.value.reset();
            continue;

        //  Return is $0D, which is why it is not listed again as a control
        //  command: reaching the end of what was typed and pressing Return
        //  are the same thing to the scan. A separator with nothing in front
        //  of it does nothing; only a line with nothing on it at all
        //  continues examining.
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            if (scan.op != 0)
            {
                result.commands.push_back (MakeArithmetic (scan));
                scan = Scan();
            }
            else if (scan.value.has_value() || scan.first.has_value())
            {
                FlushExamine (scan, scratch, result);
            }

            continue;

        //  Everything after the colon is the byte list.
        case ':':
            if (!TryParseColon (line, index, scan, scratch, command, result.error))
            {
                result.status = ParseStatus::Invalid;
                result.commands.clear();
                return result;
            }

            index = line.size();
            break;

        //  And everything after R or W is the file name.
        case 'R':
        case 'W':
            if (!TryParseFile (character, line.substr (index), scan, command, result.error))
            {
                result.status = ParseStatus::Invalid;
                result.commands.clear();
                return result;
            }

            index = line.size();
            break;

        //  `S` STEPS UNLESS A VALUE AND A RANGE CAME FIRST, which is the
        //  Monitor's own rule: the letter is the same and the scan is not.
        case 'S':
            if (scan.dest.has_value())
            {
                command = MakeCommand (DebugVerb::SearchMemory, 'S');
                ApplyRange (scan, command);

                //  EVERY VALUE BYTE CARRIES A MASK BYTE. The search matches
                //  value against memory through the mask, and AppleWin's `S`
                //  uses that for its `?` wildcards; the Monitor has no
                //  wildcard, so every byte matches in full. Leaving the mask
                //  empty is not "no wildcards" -- it is a shorter array than
                //  the search walks.
                command.values.push_back ((Byte) (*scan.dest & 0xFF));
                command.mask.push_back   (0xFF);

                if (*scan.dest > 0xFF)
                {
                    command.values.push_back ((Byte) (*scan.dest >> 8));
                    command.mask.push_back   (0xFF);
                }
            }
            else
            {
                command = MakeStep (DebugVerb::StepInto, 'S', scan);
            }

            break;

        case 'M':
        case 'V':
            command = MakeMoveOrVerify (character, scan, scratch);
            break;

        case 'G':
            command = MakeGo (scan);
            break;

        case 'L':
            command = MakeCommand (DebugVerb::List, 'L');
            ApplyRange (scan, command);
            break;

        case 'T':
            command = MakeStep (DebugVerb::Trace, 'T', scan);
            break;

        case 'I':
            command = MakeCommand (DebugVerb::SetInverse, 'I');
            break;

        case 'N':
            command = MakeCommand (DebugVerb::SetNormal, 'N');
            break;

        //  An address in front of `!` is where the assembly starts; with
        //  none it starts at the PC.
        case '!':
            command       = MakeCommand (DebugVerb::EnterAssembler, '!');
            command.a1    = GetAddress (scan).value_or (0);
            command.hasA1 = GetAddress (scan).has_value();
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

        case kShowRegisters:
            command                     = MakeCommand (DebugVerb::ShowRegistersForEdit, character);
            scratch.registerEditPending = true;
            break;

        default:
            result.error  = std::format ("{} is not a Monitor command.", GetUnknownCharacter (line, index, character));
            result.status = ParseStatus::Invalid;
            result.commands.clear();
            return result;
        }

        result.commands.push_back (command);
        scan = Scan();
    }

    NoteTypedValue (scan, scratch);

    if (scan.op != 0)
    {
        result.commands.push_back (MakeArithmetic (scan));
    }
    else
    {
        FlushExamine (scan, scratch, result);
    }

    result.status = result.commands.empty() ? ParseStatus::Empty : ParseStatus::Ok;
    state         = scratch;
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
//  does not turn into a third. A continuation is input other than a `:`, so
//  it ends a register edit `^E` armed.
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

        state.registerEditPending = false;

        //  TO THE END OF THE ROW, NOT EIGHT BYTES ON. Rows are labeled on
        //  eight-byte boundaries, so continuing from $0301 shows seven bytes
        //  and stops at $0307; the next Return starts a full row at $0308.
        //  Asking for eight from $0301 would spill one byte into the next
        //  row and print a second line with a single byte on it.
        command.a1    = (Word) (state.lastExamined + 1);
        command.a2    = (Word) (command.a1 | (s_kBytesPerLine - 1));
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
//  both modes. After `+` or `-` the value is the second operand rather than
//  the end of a range, so a command there acts on the first alone.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorParser::ApplyRange (const Scan & scan, DebugCommand & command)
{
    if (scan.op != 0)
    {
        command.a1    = scan.first.value_or (0);
        command.hasA1 = true;
    }
    else if (scan.first.has_value())
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
    static constexpr Byte  kFirstPrintable = 0x20;
    DebugCommand           command;



    command.verb = verb;
    command.mode = CommandMode::Monitor;

    command.sourceName = ((Byte) source < kFirstPrintable)
                       ? std::string ("^") + (char) (source + 'A' - 1)
                       : std::string (1, source);

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::GetAddress
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> MonitorParser::GetAddress (const Scan & scan)
{
    return scan.first.has_value() ? scan.first : scan.value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::NoteTypedValue
//
//  The Monitor copies an address typed outside a range or an operator into
//  A3, the pointer `:` stores through, so after `300` or `300.30F` a `: 12`
//  stores at $0300. storeAddress is A3.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorParser::NoteTypedValue (const Scan & scan, MonitorState & state)
{
    if (scan.value.has_value() && !scan.first.has_value() && scan.op == 0)
    {
        state.storeAddress = *scan.value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::TryParseColon
//
//  `: bytes` stores at A3 and moves it past them, unless `^E` armed the colon
//  and nothing has been typed since, when the bytes set the registers. A
//  colon with no bytes after it is an error, as is anything but hex bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorParser::TryParseColon (
    const std::string  & line,
    size_t               index,
    const Scan         & scan,
    MonitorState       & state,
    DebugCommand       & command,
    std::string        & error)
{
    std::vector<Byte>  values;
    bool               isEdit = state.registerEditPending && !scan.value.has_value() && !scan.first.has_value();



    if (!TryParseBytes (line.substr (index), values))
    {
        error = "A deposit takes hex bytes.";
        return false;
    }

    if (values.empty())
    {
        error = isEdit ? "A register change takes one or more bytes." : "A deposit takes one or more bytes.";
        return false;
    }

    if (isEdit)
    {
        command                   = MakeCommand (DebugVerb::EditRegisters, ':');
        command.values            = values;
        state.registerEditPending = false;
        return true;
    }

    command        = MakeCommand (DebugVerb::Deposit, ':');
    command.a1     = state.storeAddress;
    command.hasA1  = true;
    command.values = values;

    state.storeAddress = (Word) (command.a1 + values.size());
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::TryParseFile
//
//  `first.lastR name` and `first.lastW name`. A file transfer needs a range
//  that runs forward: with none, or one that ends before it starts, there is
//  no length to read or write.
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorParser::TryParseFile (
    char                 character,
    const std::string  & rest,
    const Scan         & scan,
    DebugCommand       & command,
    std::string        & error)
{
    command      = MakeCommand (character == 'R' ? DebugVerb::ReadFile : DebugVerb::WriteFile, character);
    command.text = Unquote (Trim (rest));
    ApplyRange (scan, command);

    if (!command.hasA1)
    {
        error = std::format ("{} takes a range: first.last{} name.", character, character);
        return false;
    }

    if (command.hasA2 && command.a2 < command.a1)
    {
        error = std::format ("{:04X}.{:04X} ends before it starts.", command.a1, command.a2);
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::MakeMoveOrVerify
//
//  The destination is the Monitor's A4: the `<` value, or with no `<` on the
//  line wherever the last move or verify left it. Each moves it past the
//  bytes it covered, at least one, so `500.5FFM` after `300<400.4FFM`
//  carries on at $0400 as it does on the ROM.
//
////////////////////////////////////////////////////////////////////////////////

DebugCommand MonitorParser::MakeMoveOrVerify (char character, const Scan & scan, MonitorState & state)
{
    DebugCommand  command = MakeCommand (character == 'M' ? DebugVerb::MoveMemory : DebugVerb::Verify, character);
    uint32_t      count   = 1;



    ApplyRange (scan, command);

    if (command.hasA2 && command.a2 > command.a1)
    {
        count = (uint32_t) (command.a2 - command.a1) + 1;
    }

    command.a3    = state.a4;
    command.hasA3 = true;
    state.a4      = (Word) (state.a4 + count);

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::MakeStep
//
//  `addrS` steps one instruction from addr, and `addrT` traces from it until
//  a stop condition or the budget ends the run, as the Monitor's T runs on
//  until something stops it.
//
////////////////////////////////////////////////////////////////////////////////

DebugCommand MonitorParser::MakeStep (DebugVerb verb, char character, const Scan & scan)
{
    static constexpr uint32_t  kUntilStopped = UINT32_MAX;
    DebugCommand               command       = MakeCommand (verb, character);
    std::optional<Word>        address       = GetAddress (scan);



    command.a1    = address.value_or (0);
    command.hasA1 = address.has_value();
    command.count = (verb == DebugVerb::Trace) ? kUntilStopped : 1;
    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::MakeGo
//
//  `F666G` IS AN ALIAS FOR `!`, NOT A JUMP. $F666 is the mini-assembler only
//  in the original ]['s ROM; on the other four machines those bytes are
//  Applesoft.
//
////////////////////////////////////////////////////////////////////////////////

DebugCommand MonitorParser::MakeGo (const Scan & scan)
{
    std::optional<Word>  address = GetAddress (scan);
    DebugCommand         command;



    if (address == s_kIntegerAssembler)
    {
        return MakeCommand (DebugVerb::EnterAssembler, 'G');
    }

    command       = MakeCommand (DebugVerb::Go, 'G');
    command.a3    = address.value_or (0);
    command.hasA3 = address.has_value();
    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::MakeArithmetic
//
////////////////////////////////////////////////////////////////////////////////

DebugCommand MonitorParser::MakeArithmetic (const Scan & scan)
{
    DebugCommand  arithmetic = MakeCommand (DebugVerb::Arithmetic, scan.op);



    arithmetic.a1    = scan.first.value_or (0);
    arithmetic.a2    = scan.value.value_or (0);
    arithmetic.hasA1 = true;
    arithmetic.hasA2 = true;
    arithmetic.text  = std::string (1, scan.op);
    return arithmetic;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::GetUnknownCharacter
//
//  A character that is not a command, as the reader typed it. A byte outside
//  ASCII starts a UTF-8 character the window sent, and is quoted whole
//  rather than read as a control code.
//
////////////////////////////////////////////////////////////////////////////////

std::string MonitorParser::GetUnknownCharacter (const std::string & line, size_t index, char character)
{
    static constexpr Byte  kFirstNonAscii  = 0x80;
    static constexpr Byte  kLeadingBits    = 0xC0;
    size_t                 end             = index;



    if ((Byte) character < kFirstNonAscii)
    {
        return MakeCommand (DebugVerb::None, character).sourceName;
    }

    //  A continuation byte is 10xxxxxx.
    while (end < line.size() && ((Byte) line[end] & kLeadingBits) == kFirstNonAscii)
    {
        ++end;
    }

    return line.substr (index - 1, end - index + 1);
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
