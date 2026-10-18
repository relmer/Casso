#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "Debugger/DiskBreakSpecText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BPDISK's grammar
//
//  BPDISK [D1|D2] <event> [arguments] [options] [IF expression]. The IF
//  clause is taken off before these see the tokens. The error for a wrong
//  argument is the syntax line of the event typed.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char *  s_kpszDiskSyntax = "BPDISK [D1|D2] READ|ADDRESS|HEAD|WRITE|DOS|BLOCK|RWTS|DRIVER|MOTOR|SELECT|INSERT|EJECT [arguments] [options] [IF expr]";

static constexpr int   kQuartersPerTrack = 4;
static constexpr int   kMaxDosSector     = 15;
static constexpr Word  kMaxBlock         = 279;
static constexpr int   kMarkNibbles      = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseDiskArguments
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseDiskArguments (const Arguments & args, DebugCommand & command, std::string & error)
{
    const Tokens                        & tokens = args.tokens;
    size_t                                at     = 0;
    DiskBreakSpec                         spec;
    const DiskBreakSpecText::EventWord  * found  = nullptr;
    std::string                           drive  = tokens.empty() ? std::string() : ToUpper (tokens[0]);
    bool                                  isOk   = false;



    if (drive == "D1" || drive == "D2")
    {
        spec.drive = (drive == "D1") ? DiskDriveFilter::Drive1 : DiskDriveFilter::Drive2;
        at++;
    }

    for (const DiskBreakSpecText::EventWord & each : DiskBreakSpecText::GetEventWords())
    {
        found = (at < tokens.size() && ToUpper (tokens[at]) == each.word) ? &each : found;
    }

    if (found == nullptr)
    {
        error = s_kpszDiskSyntax;
        return false;
    }

    spec.event = found->event;
    at++;

    isOk = TryParseDiskEventArguments (tokens, at, *args.context, spec) && TryParseDiskOptions (tokens, at, spec) && at == tokens.size();

    if (!isOk)
    {
        error = found->syntax;
        return false;
    }

    command.disk = spec;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseDiskEventArguments
//
//  The event's own arguments, up to its first option word.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseDiskEventArguments (const Tokens & tokens, size_t & at, const IDebugExpressionContext & context, DiskBreakSpec & spec)
{
    std::string  next  = (at < tokens.size()) ? ToUpper (tokens[at]) : std::string();
    Word         block = 0;



    switch (spec.event)
    {
    case DiskEvent::SectorRead:
    case DiskEvent::DosSector:
        return TryParseDiskNumber (tokens, at, context, spec.track) && TryParseDiskNumber (tokens, at, context, spec.sector) &&
               (spec.event != DiskEvent::DosSector || spec.sector.value_or (0) <= kMaxDosSector);

    case DiskEvent::AddressField:
        return TryParseDiskFields (tokens, at, context, spec, 3);

    case DiskEvent::RwtsCall:
        return TryParseDiskFields (tokens, at, context, spec, 2) && TryParseDiskCommand (tokens, at, spec);

    case DiskEvent::Head:
        return TryParseDiskRange (tokens, at, context, spec, true);

    case DiskEvent::WriteMode:
        spec.event = (next == "BLOCKED") ? DiskEvent::WriteBlocked : (next.empty() || next == "QT" || next == "T") ? DiskEvent::WriteMode : DiskEvent::WritePrologue;
        at        += (spec.event == DiskEvent::WriteBlocked) ? 1 : 0;

        return (spec.event == DiskEvent::WriteBlocked)
            || (spec.event == DiskEvent::WriteMode && TryParseDiskRange (tokens, at, context, spec, false))
            || (spec.event == DiskEvent::WritePrologue && TryParseDiskNumber (tokens, at, context, spec.track) && TryParseDiskNumber (tokens, at, context, spec.sector));

    case DiskEvent::ProDosBlock:
        spec.block = (at < tokens.size() && TryEvaluateQuietly (tokens[at], context, block)) ? std::optional<Word> (block) : std::nullopt;
        at        += spec.block.has_value() ? 1 : 0;
        return spec.block.has_value() && *spec.block <= kMaxBlock;

    case DiskEvent::DriverCall:
        spec.block = (next == "BLOCK" && at + 1 < tokens.size() && TryEvaluateQuietly (tokens[at + 1], context, block)) ? std::optional<Word> (block) : std::nullopt;
        at        += spec.block.has_value() ? 2 : 0;

        return (next != "BLOCK" || (spec.block.has_value() && *spec.block <= kMaxBlock)) && TryParseDiskCommand (tokens, at, spec);

    case DiskEvent::Motor:
        spec.motor = (next == "ON") ? DiskMotorFilter::On : (next == "OFF") ? DiskMotorFilter::Off : (next == "STOPPED") ? DiskMotorFilter::Stopped : DiskMotorFilter::Any;
        at        += (spec.motor != DiskMotorFilter::Any) ? 1 : 0;
        return true;

    default:
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseDiskNumber
//
//  A number in the mode's syntax, or * for any.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseDiskNumber (const Tokens & tokens, size_t & at, const IDebugExpressionContext & context, std::optional<Byte> & outValue)
{
    Word  value  = 0;
    bool  isAny  = at < tokens.size() && tokens[at] == "*";
    bool  isByte = !isAny && at < tokens.size() && !IsDiskOptionWord (tokens[at]) && TryEvaluateQuietly (tokens[at], context, value) && value <= 0xFF;



    outValue = isByte ? std::optional<Byte> ((Byte) value) : std::nullopt;
    at      += (isAny || isByte) ? 1 : 0;

    return isAny || isByte;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseDiskFields
//
//  ADDRESS's track, sector and volume, or RWTS's track and sector: as many
//  as are given, each a number or *, before the first option word.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseDiskFields (const Tokens & tokens, size_t & at, const IDebugExpressionContext & context, DiskBreakSpec & spec, int count)
{
    std::array<std::optional<Byte> *, 3>  fields = { &spec.track, &spec.sector, &spec.volume };
    bool                                  isOk   = true;



    for (int i = 0; i < count && isOk && at < tokens.size() && !IsDiskOptionWord (tokens[at]); i++)
    {
        isOk = TryParseDiskNumber (tokens, at, context, *fields[i]);
    }

    return isOk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseDiskRange
//
//  QT first[:last] in quarter tracks, T first[:last] in whole tracks, or
//  HALF for HEAD; none at all matches any position.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseDiskRange (const Tokens & tokens, size_t & at, const IDebugExpressionContext & context, DiskBreakSpec & spec, bool takesHalf)
{
    std::string  unit    = (at < tokens.size()) ? ToUpper (tokens[at]) : std::string();
    bool         isRange = unit == "QT" || unit == "T";
    std::string  text    = (isRange && at + 1 < tokens.size()) ? tokens[at + 1] : std::string();
    size_t       colon   = text.find (':');
    Word         first   = 0;
    Word         last    = 0;
    int          scale   = (unit == "T") ? kQuartersPerTrack : 1;
    bool         isOk    = false;



    if (takesHalf && unit == "HALF")
    {
        spec.head = DiskHeadFilter::Half;
        at++;
        return true;
    }

    if (!isRange)
    {
        return true;
    }

    isOk = TryEvaluateQuietly (text.substr (0, colon), context, first);
    last = first;
    isOk = isOk && (colon == std::string::npos || TryEvaluateQuietly (text.substr (colon + 1), context, last));

    spec.head          = DiskHeadFilter::Range;
    spec.qtFirst       = first * scale;
    spec.qtLast        = last * scale;
    spec.isWholeTracks = scale != 1;
    at                += 2;

    return isOk && first <= last && spec.qtLast <= DiskBreakSpec::kMaxQuarterTrack;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseDiskCommand
//
//  RWTS's or DRIVER's request word, if given.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseDiskCommand (const Tokens & tokens, size_t & at, DiskBreakSpec & spec)
{
    std::string  word = (at < tokens.size()) ? ToUpper (tokens[at]) : std::string();



    for (Byte command = 0; command <= 4; command++)
    {
        const char  * name = DiskBreakSpecText::GetCommandWord (spec.event, command);



        if (name != nullptr && word == name)
        {
            spec.command = command;
        }
    }

    at += spec.command.has_value() ? 1 : 0;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseDiskOptions
//
//  The options after the event's arguments, in any order; each event takes
//  only the ones that mean something for it.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseDiskOptions (const Tokens & tokens, size_t & at, DiskBreakSpec & spec)
{
    DiskEvent  event      = spec.event;
    bool       isField    = event == DiskEvent::SectorRead || event == DiskEvent::AddressField || event == DiskEvent::WritePrologue;
    bool       takesData  = event == DiskEvent::SectorRead || event == DiskEvent::WritePrologue;
    bool       isPassable = event == DiskEvent::SectorRead || event == DiskEvent::DosSector || event == DiskEvent::ProDosBlock;
    bool       isOk       = true;



    while (isOk && at < tokens.size())
    {
        std::string  word = ToUpper (tokens[at++]);



        if      (word == "PASSED"     && isPassable)                      { spec.isPassed      = true; }
        else if (word == "BADSUM"     && event == DiskEvent::AddressField) { spec.isBadChecksum = true; }
        else if (word == "WRONGTRACK" && event == DiskEvent::AddressField) { spec.isWrongTrack  = true; }
        else if (word == "BUMP"       && event == DiskEvent::Head)        { spec.isBump        = true; }
        else if (word == "EVERY"      && event == DiskEvent::DriveSelect) { spec.isEvery       = true; }
        else if (word == "ADDR"       && isField)                         { isOk = TryParseDiskMarks (tokens, at, spec.addressMarks); }
        else if (word == "DATA"       && takesData)                       { isOk = TryParseDiskMarks (tokens, at, spec.dataMarks); }
        else if (word == "SECTORS"    && isField && at < tokens.size())
        {
            isOk        = tokens[at] == "13" || tokens[at] == "16";
            spec.format = (tokens[at++] == "13") ? DiskFieldKind::Thirteen : DiskFieldKind::Sixteen;
        }
        else
        {
            isOk = false;
        }
    }

    return isOk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseDiskMarks
//
//  Three nibbles, each two hex digits or ?? for any.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseDiskMarks (const Tokens & tokens, size_t & at, std::optional<DiskMarkPattern> & outMarks)
{
    DiskMarkPattern  pattern;
    std::string      text;
    std::string      error;
    bool             isOk = at + kMarkNibbles <= tokens.size();



    for (int i = 0; isOk && i < kMarkNibbles; i++)
    {
        text += (i > 0) ? " " + tokens[at + i] : tokens[at + i];
    }

    isOk = isOk && DiskMarkPattern::TryParse (text, pattern, error) && pattern.GetLength() == kMarkNibbles;

    outMarks = isOk ? std::optional<DiskMarkPattern> (pattern) : std::nullopt;
    at      += isOk ? kMarkNibbles : 0;

    return isOk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::IsDiskOptionWord
//
//  The words that end an event's arguments.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::IsDiskOptionWord (const std::string & token)
{
    static constexpr const char * kWords[] = { "PASSED", "BADSUM", "WRONGTRACK", "BUMP", "EVERY", "ADDR", "DATA", "SECTORS",
                                               "READ", "WRITE", "SEEK", "STATUS", "FORMAT" };
    std::string                   upper    = ToUpper (token);



    return std::any_of (std::begin (kWords), std::end (kWords), [&] (const char * word) { return upper == word; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryEvaluateQuietly
//
//  TryEvaluate where the error is the event's syntax line, not the
//  expression's.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryEvaluateQuietly (const std::string & text, const IDebugExpressionContext & context, Word & value)
{
    std::string  error;



    return !text.empty() && TryEvaluate (text, context, value, error);
}
