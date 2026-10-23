#pragma once

#include "Pch.h"

#include "Debugger/Basic/ApplesoftProgram.h"
#include "Debugger/DebugHook.h"
#include "Debugger/Reply.h"

class BreakpointTable;
class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStepKind
//
////////////////////////////////////////////////////////////////////////////////

enum class BasicStepKind
{
    Statement,
    Line,
    Out,
};





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStop
//
//  The stop Poll found: a breakpoint's, with its id, a step's end, or an
//  error, and what the stop reports.
//
////////////////////////////////////////////////////////////////////////////////

struct BasicStop
{
    StopReason           reason = StopReason::Breakpoint;
    std::optional<int>   breakpointId;
    BasicStopReport      report;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopMonitor
//
//  Applesoft's breakpoints, its error stop and the BASIC steps, tested where
//  the interpreter's own code passes (research R-043): NEWSTT's call of
//  CHRGET, where every statement begins with CURLIN and TXTPTR set; NEWSTT
//  finding the end of the program; and ERROR, with the error's code in X.
//  The hook filter marks those pages only while an entry or a step is
//  armed, so a run with none pays nothing.
//
//  A line begins where TXTPTR is on the high byte of the line's own number,
//  where NEWSTT sets it after a new line's header; after a colon it is on
//  the colon. A variable entry compares the value at each statement boundary of
//  the program with the value at the one before, so the statement that made
//  a change stops at the boundary after it. A boundary after one in direct
//  mode only takes the values, so RUN's clearing and a value typed at the
//  prompt stop nothing.
//
////////////////////////////////////////////////////////////////////////////////

class BasicStopMonitor
{
public:
    static constexpr Word  kStatement  = 0xD81D;    // NEWSTT's JSR CHRGET: a statement begins
    static constexpr Word  kProgramEnd = 0xD826;    // NEWSTT at the program's end
    static constexpr Word  kError      = 0xD412;    // ERROR, the error's code in X
    static constexpr Word  kMessages   = 0xD260;    // the error messages, each one's last character with its high bit set

    explicit BasicStopMonitor (BreakpointTable & table) : m_table (table) {}

    //  Applesoft's statement loop at its address, as it is in the ][+, //e,
    //  enhanced //e and //c ROMs; false while the $D000 bank has other code.
    static bool  IsApplesoftVisible (const IDebugExpressionContext & context);

    //  The pages the enabled entries are tested on.
    void  MarkPages            (DebugHookFilter::Table & pages) const;
    void  ForgetDisabledValues ();

    //  A step to the next statement, the next line, or the statement after
    //  the newest GOSUB returns; false, with the reason, where Applesoft is
    //  not in memory or no GOSUB is active.
    bool  TryBeginStep (BasicStepKind kind, const IDebugExpressionContext & context, std::string & error);
    void  EndStep    ()       { m_step.reset(); }
    bool  IsStepping () const { return m_step.has_value(); }

    //  Clears the values the variable entries compare against, for a reset
    //  or a different machine: the next boundary takes them again.
    void  Rebase();

    //  Takes the values the variable entries compare against from memory as
    //  it is now, for a machine whose memory changed other than by the
    //  program: a move through history, a replay's snapshot, an edit. A
    //  change the program makes from here on stops at the next boundary.
    void  TakeValues (const IDebugExpressionContext & context);

    //  Before the instruction at pc: true when an entry or the step stops.
    //  Every entry that matches counts the hit.
    bool  Poll (Word pc, const IDebugExpressionContext & context);

    bool                     HasStop   () const { return m_stop.has_value(); }
    const BasicStop        & GetStop   () const { return *m_stop; }
    void                     ClearStop ()       { m_stop.reset(); }

    //  For a replay's stop test: Poll counts no hit and makes no report
    //  beyond whether an entry stops.
    void  SetReplay() { m_isReplay = true; }

private:
    struct Step
    {
        BasicStepKind  kind  = BasicStepKind::Statement;
        Word           frame = 0;      // Out: the GOSUB frame's first byte
    };

    using Peek = ApplesoftProgram::Peek;

    bool  PollBoundary (Word pc, const IDebugExpressionContext & context);
    bool  PollError    (const IDebugExpressionContext & context);
    bool  IsStepDone   (bool isLine, const IDebugExpressionContext & context) const;

    static Peek         MakePeek        (const IDebugExpressionContext & context);
    static bool         IsLineStart     (Word line, const IDebugExpressionContext & context);
    static std::string  FormatBytes     (const ApplesoftProgram & program, BasicType type, const std::vector<Byte> & bytes);
    static std::string  GetErrorMessage (Byte code, const IDebugExpressionContext & context);

    BreakpointTable                      & m_table;
    std::map<int, std::vector<Byte>>       m_values;
    bool                                   m_wasProgram = false;
    std::optional<Word>                    m_lastLine;
    std::optional<Step>                    m_step;
    std::optional<BasicStop>               m_stop;
    bool                                   m_isReplay   = false;
};
