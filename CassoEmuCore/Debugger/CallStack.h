#pragma once

#include "Debugger/Reply.h"





//  A byte of the machine's address space as the CPU sees it, read without
//  side effects.
using CallStackPeek = std::function<Byte (Word)>;





////////////////////////////////////////////////////////////////////////////////
//
//  StackWalker
//
//  The walk mechanism (R-035): the stack page from SP+1 up to $01FF, read as
//  words. A word W is a return address when the opcode at W-2 is JSR, since
//  JSR pushes the address of its own third byte. With a debug file loaded,
//  a candidate is kept only when that JSR targets a routine entry. A word
//  found skips the scan past both its bytes.
//
//  Every frame it finds is labeled guessed, innermost first.
//
////////////////////////////////////////////////////////////////////////////////

class StackWalker
{
public:
    static std::vector<CallStackFrame>  Walk (Byte sp, const CallStackPeek & peek, const std::set<Word> * routineEntries);
};





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder
//
//  The recorded mechanism (R-035): a shadow stack kept from the instructions
//  as they execute, only while the debugger is attached (FR-064, FR-068).
//  JSR, BRK and interrupt dispatch push a frame; RTS and RTI pop one.
//
//  IT SEES EACH INSTRUCTION BEFORE IT EXECUTES, AND JUDGES IT AFTER. The
//  instruction about to run is held until the next one arrives, or until
//  Settle is called with the registers after it ran, and only then is its
//  effect on the stack known. An interrupt is dispatched in place of the
//  instruction held, which is how it is told apart: the stack pointer drops
//  by three and the address pushed is the held instruction's own.
//
//  A FRAME ENDS BY THE STACK-POINTER RULE OF R-033: once the stack pointer is
//  back at its level before the call and a return or a jump has just run.
//
//  Each way the record can be defeated is kept as a break (FR-069), placed
//  above the frames it leaves unverified: TXS, a pull into a frame's return
//  address, a frame ended by a jump, a return to an address other than the
//  one pushed, a stack pointer that wraps, a reset, and the point at which
//  recording began. A break is dropped once the frames beneath it have
//  returned, since the chain it cast doubt on is gone. A return a few bytes
//  past the address pushed is the inline-parameter idiom and is noted, not
//  broken.
//
////////////////////////////////////////////////////////////////////////////////

class CallStackRecorder
{
public:
    //  A break and the number of frames beneath it.
    struct Break
    {
        CallStackBreak  info;
        size_t          depth = 0;
    };

    //  How far past the pushed address a return may land and still be read
    //  as returning past inline parameters.
    static constexpr Word  kInlineParameterLimit = 8;

    void    SetPeek       (CallStackPeek peek) { m_peek = std::move (peek); }

    //  Recording starts at the instruction at pc, with nothing known about
    //  the calls already on the stack.
    void    Begin         (Word pc, Byte opcode);
    void    End           ();
    bool    IsActive      () const { return m_active; }

    void    OnInstruction (Word pc, Byte sp, Byte opcode);
    void    Settle        (Word pc, Byte sp);
    void    OnReset       (Word pc, Byte opcode);

    //  Outermost first.
    const std::vector<CallStackFrame>    & GetFrames     () const { return m_frames; }
    const std::vector<Break>             & GetBreaks     () const { return m_breaks; }
    const std::optional<CallStackFrame>  & GetLastReturn () const { return m_lastReturn; }

private:
    //  More frames than the stack page could hold drop the outermost, so a
    //  program whose stack wraps forever does not grow the record forever.
    static constexpr size_t  kMaxFrames = 256;

    struct Pending
    {
        Word  pc     = 0;
        Byte  sp     = 0;
        Byte  opcode = 0;
    };

    void    Apply           (const Pending & held, Word pc, Byte sp);
    bool    TryApplyInterrupt (const Pending & held, Word pc, Byte sp);
    void    Push            (CallFrameKind kind, const Pending & held, Word target);
    void    Return          (const Pending & held, Word pc, Byte sp);
    void    Jump            (const Pending & held, Byte sp);
    void    Pull            (const Pending & held, Byte sp);
    void    ReloadStack     (const Pending & held, Byte sp);
    void    PopFrame        ();
    void    AddBreak        (CallBreakKind kind, const Pending & held);
    Word    PeekWord        (Word address) const;

    static int   GetStackDelta (Byte opcode);
    static Word  GetExpectedReturn (const CallStackFrame & frame);

    CallStackPeek                  m_peek;
    bool                           m_active     = false;
    std::optional<Pending>         m_pending;
    std::vector<CallStackFrame>    m_frames;
    std::vector<Break>             m_breaks;
    std::optional<CallStackFrame>  m_lastReturn;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CallStack
//
//  The chain as CALLS and the pane show it, innermost first, for one of the
//  three mechanisms (FR-068): recorded frames and their breaks; the walk from
//  SP; or hybrid, the recorded frames with the walk below the point where
//  recording began. Every frame below a break is unverified.
//
////////////////////////////////////////////////////////////////////////////////

class CallStack
{
public:
    static CallStackData  Build (CallStackMechanism          mechanism,
                                 const CallStackRecorder   & recorder,
                                 Byte                        sp,
                                 const CallStackPeek       & peek,
                                 const std::set<Word>      * routineEntries);

    //  Upper case, as CALLS MODE takes them.
    static const char *  GetMechanismName (CallStackMechanism mechanism);

    //  JSR, BRK, IRQ or NMI.
    static const char *  GetKindName      (CallFrameKind kind);

    //  The protocol's names: txs, pulledReturn and so on.
    static const char *  GetBreakKindName (CallBreakKind kind);

    //  A break as one line, naming the instruction, its address and what it
    //  did: "TXS at $0812".
    static std::string   DescribeBreak    (const CallStackBreak & chainBreak);

private:
    static void          AddRecorded      (const CallStackRecorder & recorder, CallStackData & data);
    static const char *  GetMnemonic      (Byte opcode);
};
