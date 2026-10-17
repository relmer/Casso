#include "Pch.h"

#include "Debugger/RoutineProfile.h"

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::RoutineProfile
//
////////////////////////////////////////////////////////////////////////////////

RoutineProfile::RoutineProfile()
{
    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::Reset
//
//  Top level is routine 0 and node 0, the root every call hangs under.
//
////////////////////////////////////////////////////////////////////////////////

void RoutineProfile::Reset()
{
    m_routines.clear();
    m_routineIndex.clear();
    m_nodes.clear();
    m_children.clear();
    m_stack.clear();
    m_frames.clear();
    m_pending.reset();
    m_lastSp = 0xFF;

    (void) FindRoutine (0, RoutineKind::Top);
    m_nodes.push_back (Node());
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::Record
//
//  A call the last instruction made starts here; the cycles go to the node
//  running; then the frames the stack pointer has risen past end, and after
//  a JSR or a BRK the next instruction starts a call.
//
////////////////////////////////////////////////////////////////////////////////

void RoutineProfile::Record (Word pc, Byte opcode, uint64_t cycles, Byte spAfter, uint64_t startCycle, uint64_t cyclesPerFrame)
{
    int  node = 0;



    if (m_pending.has_value())
    {
        Begin (pc);
    }

    node = m_stack.empty() ? 0 : m_stack.back().node;

    m_nodes[(size_t) node].self += cycles;
    AddToFrame (m_nodes[(size_t) node].routine, cycles, startCycle, cyclesPerFrame);

    EndReturned (spAfter);

    if (opcode == kJsr)
    {
        m_pending = Pending { RoutineKind::Call, (Byte) (spAfter + 2) };
    }
    else if (opcode == kBrk)
    {
        m_pending = Pending { RoutineKind::Brk, (Byte) (spAfter + 3) };
    }

    m_lastSp = spAfter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::OnInterrupt
//
//  The interrupt returns to the stack pointer after the last instruction.
//
////////////////////////////////////////////////////////////////////////////////

void RoutineProfile::OnInterrupt()
{
    m_pending = Pending { RoutineKind::Interrupt, m_lastSp };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::Begin
//
//  A stack of kMaxDepth frames drops its outermost, which a program that
//  calls and never returns would otherwise grow without end.
//
////////////////////////////////////////////////////////////////////////////////

void RoutineProfile::Begin (Word pc)
{
    int     parent  = m_stack.empty() ? 0 : m_stack.back().node;
    size_t  routine = FindRoutine (pc, m_pending->kind);
    int     node    = FindChild (parent, routine);



    if (m_stack.size() >= kMaxDepth)
    {
        m_stack.erase (m_stack.begin());
    }

    m_nodes[(size_t) node].calls++;
    m_stack.push_back (Active { node, m_pending->returnSp });
    m_pending.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::EndReturned
//
////////////////////////////////////////////////////////////////////////////////

void RoutineProfile::EndReturned (Byte sp)
{
    while (!m_stack.empty() && HasReturned (m_stack.back().returnSp, sp))
    {
        m_stack.pop_back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::HasReturned
//
////////////////////////////////////////////////////////////////////////////////

bool RoutineProfile::HasReturned (Byte returnSp, Byte sp)
{
    Byte  depth = (Byte) (returnSp - sp);



    return depth == 0 || depth > 0x80;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::FindRoutine
//
////////////////////////////////////////////////////////////////////////////////

size_t RoutineProfile::FindRoutine (Word address, RoutineKind kind)
{
    uint32_t  key   = ((uint32_t) kind << 16) | address;
    auto      found = m_routineIndex.find (key);



    if (found != m_routineIndex.end())
    {
        return found->second;
    }

    m_routines.push_back (Routine { address, kind });
    m_routineIndex[key] = m_routines.size() - 1;

    return m_routines.size() - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::FindChild
//
//  With the tree full, a new call runs in its caller's node.
//
////////////////////////////////////////////////////////////////////////////////

int RoutineProfile::FindChild (int parent, size_t routine)
{
    uint64_t  key   = ((uint64_t) (uint32_t) parent << 32) | routine;
    auto      found = m_children.find (key);
    Node      node;



    if (found != m_children.end())
    {
        return found->second;
    }

    if (m_nodes.size() >= kMaxNodes)
    {
        return parent;
    }

    node.routine = routine;
    node.parent  = parent;

    m_nodes.push_back (node);
    m_children[key] = (int) m_nodes.size() - 1;

    return (int) m_nodes.size() - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::AddToFrame
//
//  A frame is a span of cyclesPerFrame cycles; a new one starts when an
//  instruction starts past the last, and the oldest goes past kFrameCount.
//  The cycles of a routine past kSharesPerFrame are added to the frame's
//  other cycles.
//
////////////////////////////////////////////////////////////////////////////////

void RoutineProfile::AddToFrame (size_t routine, uint64_t cycles, uint64_t startCycle, uint64_t cyclesPerFrame)
{
    uint64_t  index = (cyclesPerFrame != 0) ? startCycle / cyclesPerFrame : 0;
    Frame   * frame = nullptr;



    if (m_frames.empty() || m_frames.back().index != index)
    {
        m_frames.push_back (Frame { index, {}, 0 });

        if (m_frames.size() > kFrameCount)
        {
            m_frames.pop_front();
        }
    }

    frame = &m_frames.back();

    for (Share & share : frame->shares)
    {
        if (share.routine == routine)
        {
            share.cycles += cycles;
            return;
        }
    }

    if (frame->shares.size() < kSharesPerFrame)
    {
        frame->shares.push_back (Share { routine, cycles });
        return;
    }

    frame->other += cycles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::GetNodeTotals
//
//  A node comes after its parent, so one pass from the last adds each total
//  to its parent's.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint64_t> RoutineProfile::GetNodeTotals() const
{
    std::vector<uint64_t>  totals (m_nodes.size());



    for (size_t i = 0; i < m_nodes.size(); i++)
    {
        totals[i] = m_nodes[i].self;
    }

    for (size_t i = m_nodes.size(); i-- > 1;)
    {
        totals[(size_t) m_nodes[i].parent] += totals[i];
    }

    return totals;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfile::GetRoutineTotals
//
//  A node's total counts toward its routine unless a node above it runs the
//  same routine, whose total has it already.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint64_t> RoutineProfile::GetRoutineTotals() const
{
    std::vector<uint64_t>  nodeTotals = GetNodeTotals();
    std::vector<uint64_t>  totals (m_routines.size());



    for (size_t i = 0; i < m_nodes.size(); i++)
    {
        bool  isNested = false;
        int   above    = m_nodes[i].parent;

        while (above >= 0 && !isNested)
        {
            isNested = m_nodes[(size_t) above].routine == m_nodes[i].routine;
            above    = m_nodes[(size_t) above].parent;
        }

        if (!isNested)
        {
            totals[m_nodes[i].routine] += nodeTotals[i];
        }
    }

    return totals;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRoutineEntry::GetName
//
////////////////////////////////////////////////////////////////////////////////

std::string ProfileRoutineEntry::GetName() const
{
    std::string  name = symbol.empty() ? std::format ("${:04X}", address) : symbol;



    switch (kind)
    {
    case RoutineKind::Top:       return "Top level";
    case RoutineKind::Brk:       return "BRK " + name;
    case RoutineKind::Interrupt: return "Interrupt " + name;
    default:                     return name;
    }
}
