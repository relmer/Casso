#include "Pch.h"

#include "Debugger/CallStack.h"





static constexpr Byte  s_kBrk         = 0x00;
static constexpr Byte  s_kPhp         = 0x08;
static constexpr Byte  s_kJsr         = 0x20;
static constexpr Byte  s_kPlp         = 0x28;
static constexpr Byte  s_kRti         = 0x40;
static constexpr Byte  s_kPha         = 0x48;
static constexpr Byte  s_kJmp         = 0x4C;
static constexpr Byte  s_kPhy         = 0x5A;
static constexpr Byte  s_kRts         = 0x60;
static constexpr Byte  s_kPla         = 0x68;
static constexpr Byte  s_kJmpIndirect = 0x6C;
static constexpr Byte  s_kPly         = 0x7A;
static constexpr Byte  s_kJmpIndexed  = 0x7C;
static constexpr Byte  s_kTxs         = 0x9A;
static constexpr Byte  s_kTas         = 0x9B;    // 6502 undocumented: SP = A & X
static constexpr Byte  s_kLas         = 0xBB;    // 6502 undocumented: SP = SP & operand
static constexpr Byte  s_kPhx         = 0xDA;
static constexpr Byte  s_kPlx         = 0xFA;

static constexpr Word  s_kStackPage   = 0x0100;
static constexpr Word  s_kNmiVector   = 0xFFFA;
static constexpr int   s_kStackTop    = 0xFF;





////////////////////////////////////////////////////////////////////////////////
//
//  StackWalker::Walk
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CallStackFrame> StackWalker::Walk (Byte sp, const CallStackPeek & peek, const std::set<Word> * routineEntries)
{
    std::vector<CallStackFrame>  frames;
    CallStackFrame               frame;
    Word                         pushed = 0;
    Word                         site   = 0;
    Word                         target = 0;
    int                          i      = sp + 1;



    while (i + 1 <= s_kStackTop)
    {
        pushed = (Word) (peek ((Word) (s_kStackPage + i)) | (peek ((Word) (s_kStackPage + i + 1)) << 8));
        site   = (Word) (pushed - 2);

        if (peek (site) == s_kJsr)
        {
            target = (Word) (peek ((Word) (site + 1)) | (peek ((Word) (site + 2)) << 8));

            if (routineEntries == nullptr || routineEntries->contains (target))
            {
                frame            = CallStackFrame();
                frame.callSite   = site;
                frame.target     = target;
                frame.kind       = CallFrameKind::Call;
                frame.provenance = CallProvenance::Guessed;
                frame.stackLevel = (Byte) (i + 1);

                frames.push_back (frame);
                i += 2;
                continue;
            }
        }

        i++;
    }

    return frames;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::Begin
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::Begin (Word pc, Byte opcode, bool isPowerOn)
{
    Pending  at;



    m_active = true;
    m_pending.reset();
    m_frames.clear();
    m_breaks.clear();
    m_lastReturn.reset();

    at.pc     = pc;
    at.opcode = opcode;
    AddBreak (isPowerOn ? CallBreakKind::PowerOn : CallBreakKind::TrackingBegan, at);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::End
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::End()
{
    m_active = false;
    m_pending.reset();
    m_frames.clear();
    m_breaks.clear();
    m_lastReturn.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::OnInstruction
//
//  The instruction at pc is about to execute: the one held before it has
//  run, so its effect is judged now, and this one is held in its place.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::OnInstruction (Word pc, Byte sp, Byte opcode)
{
    if (!m_active)
    {
        return;
    }

    Settle (pc, sp);
    m_pending = Pending { pc, sp, opcode };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::Settle
//
//  The registers after the held instruction ran. Called at a stop and before
//  the chain is read, between instructions, so the record includes the last
//  instruction executed.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::Settle (Word pc, Byte sp)
{
    Pending  held;



    if (!m_pending.has_value())
    {
        return;
    }

    held = *m_pending;
    m_pending.reset();
    Apply (held, pc, sp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::OnReset
//
//  Every frame recorded is void; the reset itself is the bottom of the chain,
//  and a power cycle is also the start of everything the machine has run.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::OnReset (Word pc, Byte opcode, bool isPowerCycle)
{
    Pending  at;



    if (!m_active)
    {
        return;
    }

    m_pending.reset();
    m_frames.clear();
    m_breaks.clear();
    m_lastReturn.reset();

    at.pc     = pc;
    at.opcode = opcode;
    AddBreak (isPowerCycle ? CallBreakKind::PowerOn : CallBreakKind::Reset, at);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::OnStackWrite
//
//  A frame's return address is the two bytes at its level and the one below.
//  A push -- the instruction's own, or an interrupt's -- writes at or below
//  SP as the instruction began, which the held instruction carries: a marked
//  opcode is held while it runs, and any other leaves SP where the one held
//  before it did. Only a store above that SP reaches a live frame.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::OnStackWrite (Word address, Byte value, std::optional<Byte> previous)
{
    Byte  offset = (Byte) address;
    Word  writer = 0;



    if (!m_active || m_frames.empty() || (previous.has_value() && *previous == value))
    {
        return;
    }

    if (m_pending.has_value() && offset <= m_pending->sp)
    {
        return;
    }

    for (auto it = m_frames.rbegin(); it != m_frames.rend(); ++it)
    {
        if (offset != it->stackLevel && offset != (Byte) (it->stackLevel - 1))
        {
            continue;
        }

        writer          = m_locateWriter ? m_locateWriter() : 0;
        it->isRewritten = true;
        it->note        = std::format ("return address changed by the store at ${:04X}", writer);
        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::MarkOpcodes
//
//  Apply's opcodes, the pushes it counts toward a stack wrap, and the two
//  undocumented 6502 instructions that load the stack pointer, which Apply
//  treats as TXS.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::MarkOpcodes (bool * opcodes)
{
    static constexpr Byte  kMarked[] =
    {
        s_kBrk, s_kPhp, s_kJsr, s_kPlp, s_kRti, s_kPha, s_kJmp, s_kPhy, s_kRts, s_kPla,
        s_kJmpIndirect, s_kPly, s_kJmpIndexed, s_kTxs, s_kTas, s_kLas, s_kPhx, s_kPlx,
    };



    for (Byte opcode : kMarked)
    {
        opcodes[opcode] = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::Apply
//
//  An instruction whose stack pointer did not move as its opcode moves it
//  did not do what the opcode says -- a 6502 running a 65C02 push as a NOP
//  -- and is left alone. A push or pull that crossed the page edge wrapped.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::Apply (const Pending & held, Word pc, Byte sp)
{
    int   delta    = GetStackDelta (held.opcode);
    int   level    = held.sp + delta;
    bool  isAsSaid = delta != 0 && sp == (Byte) level;



    if (TryApplyInterrupt (held, pc, sp))
    {
        return;
    }

    switch (held.opcode)
    {
    case s_kJsr:
        if (isAsSaid)
        {
            Push (CallFrameKind::Call, held, pc);
        }

        break;

    case s_kBrk:
        if (isAsSaid)
        {
            Push (CallFrameKind::Brk, held, pc);
        }

        break;

    case s_kRts:
    case s_kRti:
        Return (held, pc, sp);
        break;

    case s_kJmp:
    case s_kJmpIndirect:
    case s_kJmpIndexed:
        Jump (held, sp);
        break;

    case s_kPla:
    case s_kPlp:
    case s_kPlx:
    case s_kPly:
        if (isAsSaid)
        {
            Pull (held, sp);
        }

        break;

    case s_kTxs:
        ReloadStack (held, sp);
        break;

    case s_kTas:
    case s_kLas:
        //  A 65C02 runs both as a NOP; where SP did not move, nothing did.
        if (sp != held.sp)
        {
            ReloadStack (held, sp);
        }

        break;

    default:
        break;
    }

    if (isAsSaid && (level < 0 || level > s_kStackTop))
    {
        AddBreak (CallBreakKind::StackWrap, held);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::TryApplyInterrupt
//
//  An interrupt is dispatched in place of the held instruction: three bytes
//  pushed, the first two the held instruction's own address. BRK pushes its
//  address plus two, and nothing else pushes three, so this is exact.
//
////////////////////////////////////////////////////////////////////////////////

bool CallStackRecorder::TryApplyInterrupt (const Pending & held, Word pc, Byte sp)
{
    static constexpr int  kPushed = 3;
    Word                  pushed  = 0;
    Word                  nmi     = 0;



    if (sp != (Byte) (held.sp - kPushed))
    {
        return false;
    }

    pushed = (Word) (m_peek ((Word) (s_kStackPage + (Byte) (sp + 2))) | (m_peek ((Word) (s_kStackPage + (Byte) (sp + 3))) << 8));

    if (pushed != held.pc)
    {
        return false;
    }

    nmi = PeekWord (s_kNmiVector);
    Push ((pc == nmi) ? CallFrameKind::Nmi : CallFrameKind::Irq, held, pc);

    if (held.sp < kPushed)
    {
        AddBreak (CallBreakKind::StackWrap, held);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::Push
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::Push (CallFrameKind kind, const Pending & held, Word target)
{
    CallStackFrame  frame;



    if (m_frames.size() >= kMaxFrames)
    {
        m_frames.erase (m_frames.begin());

        for (Break & each : m_breaks)
        {
            each.depth = (each.depth > 0) ? each.depth - 1 : 0;
        }
    }

    frame.callSite   = held.pc;
    frame.target     = target;
    frame.kind       = kind;
    frame.provenance = CallProvenance::Recorded;
    frame.stackLevel = held.sp;

    m_frames.push_back (frame);
    m_lastReturn.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::Return
//
//  Frames the return passed over entirely had their return addresses pulled
//  already, and go. The frame at the level the return reached is the one it
//  ends: to the address pushed is a clean return, a few bytes past it from
//  an RTS is a return past inline parameters, and anywhere else is a break.
//  A frame whose return address a store changed returns wherever the store
//  sent it, which the frame's note already says.
//  A return that leaves SP below the frame's level is not the frame's own --
//  an RTS used as a computed jump -- and changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::Return (const Pending & held, Word pc, Byte sp)
{
    CallStackFrame  frame;
    Word            expected = 0;
    Word            past     = 0;



    while (!m_frames.empty() && m_frames.back().stackLevel < sp)
    {
        PopFrame();
    }

    if (m_frames.empty() || m_frames.back().stackLevel != sp)
    {
        return;
    }

    frame = m_frames.back();
    PopFrame();

    if (frame.isRewritten)
    {
        return;
    }

    expected = GetExpectedReturn (frame);
    past     = (Word) (pc - expected);

    if (past == 0)
    {
        return;
    }

    if (held.opcode == s_kRts && frame.kind == CallFrameKind::Call && past <= kInlineParameterLimit)
    {
        frame.note   = "returned past inline parameters";
        m_lastReturn = frame;
        return;
    }

    AddBreak (CallBreakKind::ReturnMismatch, held);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::Jump
//
//  A jump with SP back at a frame's level ends that frame without a return.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::Jump (const Pending & held, Byte sp)
{
    if (m_frames.empty() || sp < m_frames.back().stackLevel)
    {
        return;
    }

    while (!m_frames.empty() && m_frames.back().stackLevel <= sp)
    {
        PopFrame();
    }

    AddBreak (CallBreakKind::EndedByJump, held);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::Pull
//
//  A pull that reaches a byte of the innermost frame's return address. The
//  break stands until the frame ends; a frame that then returns cleanly or
//  past inline parameters takes the break with it.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::Pull (const Pending & held, Byte sp)
{
    size_t  depth    = m_frames.size();
    bool    isMarked = false;



    if (m_frames.empty() || (int) sp < (int) m_frames.back().stackLevel - 1)
    {
        return;
    }

    isMarked = std::any_of (m_breaks.begin(), m_breaks.end(), [depth] (const Break & each)
    {
        return each.depth == depth && each.info.kind == CallBreakKind::PulledReturn;
    });

    if (!isMarked)
    {
        AddBreak (CallBreakKind::PulledReturn, held);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::ReloadStack
//
//  TXS: frames whose return address is now above SP are gone; the rest are
//  unverified.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::ReloadStack (const Pending & held, Byte sp)
{
    while (!m_frames.empty() && m_frames.back().stackLevel <= sp)
    {
        PopFrame();
    }

    AddBreak (CallBreakKind::Txs, held);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::PopFrame
//
//  A break above more frames than remain was about frames that are gone.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::PopFrame()
{
    size_t  depth = 0;



    m_frames.pop_back();
    depth = m_frames.size();

    std::erase_if (m_breaks, [depth] (const Break & each) { return each.depth > depth; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::AddBreak
//
//  A new break over the same frames as an older one replaces it, so a
//  program that reloads its stack in a loop keeps one break, not thousands.
//  Where recording began, a reset and power-on stay: they are the bottom of
//  the chain.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackRecorder::AddBreak (CallBreakKind kind, const Pending & held)
{
    size_t  depth = m_frames.size();
    Break   added;



    std::erase_if (m_breaks, [depth] (const Break & each)
    {
        return each.depth >= depth && each.info.kind != CallBreakKind::TrackingBegan && each.info.kind != CallBreakKind::Reset && each.info.kind != CallBreakKind::PowerOn;
    });

    added.info.kind   = kind;
    added.info.pc     = held.pc;
    added.info.opcode = held.opcode;
    added.depth       = depth;

    m_breaks.push_back (added);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::PeekWord
//
////////////////////////////////////////////////////////////////////////////////

Word CallStackRecorder::PeekWord (Word address) const
{
    return (Word) (m_peek (address) | (m_peek ((Word) (address + 1)) << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::GetStackDelta
//
//  How far each instruction that moves the stack pointer moves it; zero for
//  the rest, and for TXS, which sets it.
//
////////////////////////////////////////////////////////////////////////////////

int CallStackRecorder::GetStackDelta (Byte opcode)
{
    switch (opcode)
    {
    case s_kJsr:
        return -2;

    case s_kBrk:
        return -3;

    case s_kPha:
    case s_kPhp:
    case s_kPhx:
    case s_kPhy:
        return -1;

    case s_kPla:
    case s_kPlp:
    case s_kPlx:
    case s_kPly:
        return 1;

    case s_kRts:
        return 2;

    case s_kRti:
        return 3;

    default:
        return 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRecorder::GetExpectedReturn
//
//  Where a clean return from the frame lands: past the JSR, past BRK and
//  its signature byte, or back at the interrupted instruction.
//
////////////////////////////////////////////////////////////////////////////////

Word CallStackRecorder::GetExpectedReturn (const CallStackFrame & frame)
{
    switch (frame.kind)
    {
    case CallFrameKind::Call:
        return (Word) (frame.callSite + 3);

    case CallFrameKind::Brk:
        return (Word) (frame.callSite + 2);

    default:
        return frame.callSite;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStack::Build
//
//  Hybrid walks below the recorded frames only where recording began after
//  the program did; below a reset there is nothing to find. A recorder that
//  never began has no record, and hybrid is then the walk.
//
////////////////////////////////////////////////////////////////////////////////

CallStackData CallStack::Build (
    CallStackMechanism          mechanism,
    const CallStackRecorder   & recorder,
    Byte                        sp,
    const CallStackPeek       & peek,
    const std::set<Word>      * routineEntries)
{
    CallStackData  data;
    bool           isWalkBelow = false;
    Byte           walkFrom    = sp;



    data.mechanism = mechanism;

    if (mechanism == CallStackMechanism::Walk || (mechanism == CallStackMechanism::Hybrid && !recorder.IsActive()))
    {
        for (const CallStackFrame & frame : StackWalker::Walk (sp, peek, routineEntries))
        {
            data.rows.push_back ({ frame, std::nullopt });
        }

        return data;
    }

    AddRecorded (recorder, data);

    isWalkBelow = mechanism == CallStackMechanism::Hybrid &&
                  std::any_of (recorder.GetBreaks().begin(), recorder.GetBreaks().end(), [] (const CallStackRecorder::Break & each)
                  {
                      return each.depth == 0 && each.info.kind == CallBreakKind::TrackingBegan;
                  });

    if (!isWalkBelow)
    {
        return data;
    }

    if (!recorder.GetFrames().empty())
    {
        walkFrom = recorder.GetFrames().front().stackLevel;
    }

    for (CallStackFrame frame : StackWalker::Walk (walkFrom, peek, routineEntries))
    {
        frame.isVerified = false;
        data.rows.push_back ({ frame, std::nullopt });
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStack::AddRecorded
//
//  Innermost first: at each depth the breaks above it, newest first, then
//  the frame. Every frame after the first break is unverified.
//
////////////////////////////////////////////////////////////////////////////////

void CallStack::AddRecorded (const CallStackRecorder & recorder, CallStackData & data)
{
    const std::vector<CallStackFrame>            & frames  = recorder.GetFrames();
    const std::vector<CallStackRecorder::Break>  & breaks  = recorder.GetBreaks();
    bool                                           isBelow = false;
    CallStackFrame                                 frame;



    for (size_t depth = frames.size(); ; depth--)
    {
        for (auto it = breaks.rbegin(); it != breaks.rend(); ++it)
        {
            if (it->depth == depth)
            {
                data.rows.push_back ({ std::nullopt, it->info });
                isBelow = true;
            }
        }

        if (depth == 0)
        {
            break;
        }

        frame            = frames[depth - 1];
        frame.isVerified = !isBelow;
        data.rows.push_back ({ frame, std::nullopt });
    }

    data.lastReturn = recorder.GetLastReturn();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStack::GetMechanismName
//
////////////////////////////////////////////////////////////////////////////////

const char * CallStack::GetMechanismName (CallStackMechanism mechanism)
{
    switch (mechanism)
    {
    case CallStackMechanism::Recorded: return "RECORDED";
    case CallStackMechanism::Walk:     return "WALK";
    default:                           return "HYBRID";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStack::GetKindName
//
////////////////////////////////////////////////////////////////////////////////

const char * CallStack::GetKindName (CallFrameKind kind)
{
    switch (kind)
    {
    case CallFrameKind::Brk: return "BRK";
    case CallFrameKind::Irq: return "IRQ";
    case CallFrameKind::Nmi: return "NMI";
    default:                 return "JSR";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStack::GetBreakKindName
//
////////////////////////////////////////////////////////////////////////////////

const char * CallStack::GetBreakKindName (CallBreakKind kind)
{
    switch (kind)
    {
    case CallBreakKind::Txs:            return "txs";
    case CallBreakKind::PulledReturn:   return "pulledReturn";
    case CallBreakKind::EndedByJump:    return "endedByJump";
    case CallBreakKind::ReturnMismatch: return "returnMismatch";
    case CallBreakKind::StackWrap:      return "stackWrap";
    case CallBreakKind::Reset:          return "reset";
    case CallBreakKind::PowerOn:        return "powerOn";
    default:                            return "trackingBegan";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStack::DescribeBreak
//
////////////////////////////////////////////////////////////////////////////////

std::string CallStack::DescribeBreak (const CallStackBreak & chainBreak)
{
    const char  * mnemonic = GetMnemonic (chainBreak.opcode);
    Word          pc       = chainBreak.pc;



    switch (chainBreak.kind)
    {
    case CallBreakKind::Txs:            return std::format ("{} at ${:04X}", (chainBreak.opcode == s_kTas || chainBreak.opcode == s_kLas) ? mnemonic : "TXS", pc);
    case CallBreakKind::PulledReturn:   return std::format ("{} at ${:04X} pulled a return address", mnemonic, pc);
    case CallBreakKind::EndedByJump:    return std::format ("{} at ${:04X} ended a call without a return", mnemonic, pc);
    case CallBreakKind::ReturnMismatch: return std::format ("{} at ${:04X} returned to an address its call did not push", mnemonic, pc);
    case CallBreakKind::StackWrap:      return std::format ("{} at ${:04X} wrapped the stack pointer", mnemonic, pc);
    case CallBreakKind::Reset:          return std::format ("reset at ${:04X}", pc);
    case CallBreakKind::PowerOn:        return std::format ("power-on at ${:04X}, cycle 0", pc);
    default:                            return std::format ("debugger opened at ${:04X}; no calls before it were recorded", pc);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStack::GetMnemonic
//
//  The instructions that can break a chain. A wrap raised by an interrupt
//  names the instruction it interrupted, which may be any.
//
////////////////////////////////////////////////////////////////////////////////

const char * CallStack::GetMnemonic (Byte opcode)
{
    switch (opcode)
    {
    case s_kBrk:         return "BRK";
    case s_kPhp:         return "PHP";
    case s_kJsr:         return "JSR";
    case s_kPlp:         return "PLP";
    case s_kRti:         return "RTI";
    case s_kPha:         return "PHA";
    case s_kJmp:
    case s_kJmpIndirect:
    case s_kJmpIndexed:  return "JMP";
    case s_kPhy:         return "PHY";
    case s_kRts:         return "RTS";
    case s_kPla:         return "PLA";
    case s_kPly:         return "PLY";
    case s_kTxs:         return "TXS";
    case s_kTas:         return "TAS";
    case s_kLas:         return "LAS";
    case s_kPhx:         return "PHX";
    case s_kPlx:         return "PLX";
    default:             return "interrupt";
    }
}
