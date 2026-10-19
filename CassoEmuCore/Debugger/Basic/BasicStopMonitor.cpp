#include "Pch.h"

#include "Debugger/Basic/BasicStopMonitor.h"
#include "Debugger/BreakpointTable.h"
#include "Debugger/IDebugExpressionContext.h"





static constexpr Byte  kHighBit      = 0x80;
static constexpr Word  kStackPage    = 0x0100;
static constexpr int   kMaxMessage   = 32;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::IsApplesoftVisible
//
//  JSR CHRGET, JSR the statement dispatcher, at the statement entry.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicStopMonitor::IsApplesoftVisible (const IDebugExpressionContext & context)
{
    static constexpr Byte  kEntry[] = { 0x20, 0xB1, 0x00, 0x20, 0x28, 0xD8 };
    bool                   isSame   = true;



    for (size_t i = 0; i < std::size (kEntry) && isSame; i++)
    {
        Byte  value = 0;



        isSame = context.TryPeek ((Word) (kStatement + i), value) && value == kEntry[i];
    }

    return isSame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::MarkPages
//
////////////////////////////////////////////////////////////////////////////////

void BasicStopMonitor::MarkPages (DebugHookFilter::Table & pages) const
{
    for (const Breakpoint & entry : m_table.GetAll())
    {
        if (!entry.enabled || !entry.IsBasic())
        {
            continue;
        }

        if (entry.kind == BreakpointKind::BasicError)
        {
            pages[kError >> 8] = true;
        }
        else
        {
            pages[kStatement   >> 8] = true;
            pages[kProgramEnd  >> 8] = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::TryBeginStep
//
//  Out takes the newest GOSUB frame now on the stack; the step ends at the
//  first statement that begins once it has been taken off.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicStopMonitor::TryBeginStep (BasicStepKind kind, const IDebugExpressionContext & context, std::string & error)
{
    ApplesoftProgram         program (MakePeek (context));
    std::vector<BasicFrame>  frames;
    Step                     step    = { kind, 0 };



    if (!IsApplesoftVisible (context))
    {
        error = "Applesoft is not in memory: the $D000 bank has other code.";
        return false;
    }

    if (kind == BasicStepKind::Out)
    {
        auto  found = frames.end();



        program.GetFrames (frames);
        found = std::ranges::find_if (frames, [] (const BasicFrame & frame) { return frame.kind == BasicFrameKind::Gosub; });

        if (found == frames.end())
        {
            error = "No GOSUB is active, so there is nothing to step out of.";
            return false;
        }

        step.frame = found->address;
    }

    m_step = step;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::Rebase
//
////////////////////////////////////////////////////////////////////////////////

void BasicStopMonitor::Rebase()
{
    m_values.clear();
    m_wasProgram = false;
    m_lastLine.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::TakeValues
//
////////////////////////////////////////////////////////////////////////////////

void BasicStopMonitor::TakeValues (const IDebugExpressionContext & context)
{
    ApplesoftProgram  program (MakePeek (context));



    Rebase();

    if (!IsApplesoftVisible (context))
    {
        return;
    }

    for (const Breakpoint & entry : m_table.GetAll())
    {
        BasicReference  reference;



        if (entry.enabled && entry.kind == BreakpointKind::BasicVariable && ApplesoftProgram::TryParseReference (entry.basicName, reference))
        {
            m_values[entry.id] = program.GetValueBytes (reference);
        }
    }

    m_lastLine   = program.GetCurrentLine();
    m_wasProgram = m_lastLine.has_value();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::Poll
//
////////////////////////////////////////////////////////////////////////////////

bool BasicStopMonitor::Poll (Word pc, const IDebugExpressionContext & context)
{
    if (pc != kStatement && pc != kProgramEnd && pc != kError)
    {
        return false;
    }

    if (!IsApplesoftVisible (context))
    {
        return false;
    }

    return (pc == kError) ? PollError (context) : PollBoundary (pc, context);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::PollBoundary
//
//  Each variable entry's value is taken at every boundary; one that differs
//  from the last boundary's, both in the program, stops. A line entry stops
//  as its line begins, and a step ends at a statement of the program.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicStopMonitor::PollBoundary (Word pc, const IDebugExpressionContext & context)
{
    using Change = std::pair<std::string, std::string>;

    ApplesoftProgram         program    (MakePeek (context));
    std::optional<Word>      line       = program.GetCurrentLine();
    std::optional<Word>      changeLine = m_lastLine;
    bool                     isProgram  = line.has_value();
    bool                     isLine     = pc == kStatement && isProgram && IsLineStart (*line, context);
    std::map<int, Change>    changes;
    int                      hitId      = -1;
    bool                     isHit      = false;
    auto                     isMatch    = [&] (const Breakpoint & entry)
    {
        return (entry.kind == BreakpointKind::BasicLine     && isLine && entry.first == *line) ||
               (entry.kind == BreakpointKind::BasicVariable && changes.contains (entry.id));
    };



    for (const Breakpoint & entry : m_table.GetAll())
    {
        BasicReference     reference;
        std::vector<Byte>  bytes;
        auto               found = m_values.find (entry.id);



        if (!entry.enabled || entry.kind != BreakpointKind::BasicVariable || !ApplesoftProgram::TryParseReference (entry.basicName, reference))
        {
            continue;
        }

        bytes = program.GetValueBytes (reference);

        if (isProgram && m_wasProgram && found != m_values.end() && found->second != bytes)
        {
            changes[entry.id] = { FormatBytes (program, reference.type, found->second), FormatBytes (program, reference.type, bytes) };
        }

        m_values[entry.id] = bytes;
    }

    m_wasProgram = isProgram;
    m_lastLine   = line;

    if (m_isReplay)
    {
        return m_table.IsBasicStop (isMatch, context);
    }

    isHit = m_table.TryMatchBasic (isMatch, context, hitId);

    if (isHit)
    {
        Breakpoint  entry;



        m_table.TryFind (hitId, entry);
        m_stop = BasicStop { StopReason::Breakpoint, hitId, { line } };

        if (changes.contains (hitId))
        {
            m_stop->report.variable   = entry.basicName;
            m_stop->report.changeLine = changeLine;
            m_stop->report.previous   = changes[hitId].first;
            m_stop->report.value      = changes[hitId].second;
        }

        return true;
    }

    if (pc == kStatement && isProgram && IsStepDone (isLine, context))
    {
        m_stop = BasicStop { StopReason::BasicStep, std::nullopt, { line } };
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::PollError
//
//  ERROR's own first instruction tests ONERR's flag, so the stop comes
//  before ONERR's handler takes the error.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicStopMonitor::PollError (const IDebugExpressionContext & context)
{
    ApplesoftProgram  program (MakePeek (context));
    Word              code    = 0;
    Byte              flag    = 0;
    int               hitId   = -1;
    auto              isMatch = [] (const Breakpoint & entry) { return entry.kind == BreakpointKind::BasicError; };



    if (m_isReplay)
    {
        return m_table.IsBasicStop (isMatch, context);
    }

    if (!m_table.TryMatchBasic (isMatch, context, hitId))
    {
        return false;
    }

    context.TryGetRegister ("X", code);
    context.TryPeek (ApplesoftProgram::kErrFlag, flag);

    m_stop                     = BasicStop { StopReason::BasicError, hitId, { program.GetCurrentLine() } };
    m_stop->report.errorCode   = (Byte) code;
    m_stop->report.error       = GetErrorMessage ((Byte) code, context);
    m_stop->report.isHandled   = (flag & kHighBit) != 0;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::IsStepDone
//
//  NEWSTT stores the stack pointer each statement begins with in REMSTK,
//  and the newest frame is just above it, so a GOSUB frame has been taken
//  off once REMSTK is at or past the frame's first byte.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicStopMonitor::IsStepDone (bool isLine, const IDebugExpressionContext & context) const
{
    Byte  stack = 0;



    if (!m_step.has_value())
    {
        return false;
    }

    switch (m_step->kind)
    {
    case BasicStepKind::Line:
        return isLine;

    case BasicStepKind::Out:
        context.TryPeek (ApplesoftProgram::kRemStk, stack);
        return kStackPage + stack >= m_step->frame;

    default:
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::MakePeek
//
////////////////////////////////////////////////////////////////////////////////

BasicStopMonitor::Peek BasicStopMonitor::MakePeek (const IDebugExpressionContext & context)
{
    return [&context] (Word address)
    {
        Byte  value = 0;



        context.TryPeek (address, value);
        return value;
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::IsLineStart
//
//  After a new line's header TXTPTR is on its number's high byte, with the
//  low byte before it and the zero that ends the line before, or the one
//  before the program, three bytes further back. After a colon TXTPTR is on
//  the colon, which no line number's high byte and zero together match.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicStopMonitor::IsLineStart (Word line, const IDebugExpressionContext & context)
{
    Byte  low  = 0;
    Byte  high = 0;
    Byte  end  = 0;
    Word  text = 0;



    context.TryPeek (ApplesoftProgram::kTxtPtr,              low);
    context.TryPeek ((Word) (ApplesoftProgram::kTxtPtr + 1), high);
    text = (Word) (low | (high << 8));

    context.TryPeek (text,              high);
    context.TryPeek ((Word) (text - 1), low);
    context.TryPeek ((Word) (text - 4), end);

    return high == (Byte) (line >> 8) && low == (Byte) line && end == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::FormatBytes
//
//  The bytes GetValueBytes reads, as PRINT prints the value.
//
////////////////////////////////////////////////////////////////////////////////

std::string BasicStopMonitor::FormatBytes (const ApplesoftProgram & program, BasicType type, const std::vector<Byte> & bytes)
{
    std::string          text;
    std::array<Byte, 5>  real = {};



    if (bytes.empty())
    {
        return (type == BasicType::String) ? std::string() : std::string ("0");
    }

    switch (type)
    {
    case BasicType::Integer:
        return std::to_string ((int16_t) ((bytes[0] << 8) | bytes[1]));

    case BasicType::String:
        std::ranges::transform (bytes, std::back_inserter (text), [] (Byte ch) { return (char) (ch & (Byte) ~kHighBit); });
        return text;

    default:
        std::copy_n (bytes.begin(), std::min (bytes.size(), real.size()), real.begin());
        return program.FormatRealByRom (real);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor::GetErrorMessage
//
//  The message ERROR prints, read where it prints it from: X indexes the
//  table, and the message's last character has its high bit set.
//
////////////////////////////////////////////////////////////////////////////////

std::string BasicStopMonitor::GetErrorMessage (Byte code, const IDebugExpressionContext & context)
{
    std::string  message;
    Byte         ch      = 0;



    for (int i = 0; i < kMaxMessage && (ch & kHighBit) == 0; i++)
    {
        context.TryPeek ((Word) (kMessages + code + i), ch);
        message += (char) (ch & (Byte) ~kHighBit);
    }

    return message;
}
