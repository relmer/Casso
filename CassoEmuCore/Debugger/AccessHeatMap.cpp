#include "Pch.h"

#include "Debugger/AccessHeatMap.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::Start
//
//  A map already on keeps its heat; the instruction set is taken again, since
//  a machine switch may have brought another CPU.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::Start (const Microcode * instructionSet, uint64_t cycle)
{
    m_instructionSet = instructionSet;

    if (IsOn())
    {
        return;
    }

    m_counts.assign (kEntryCount, 0);
    m_heat.assign   (kEntryCount, 0.0f);
    m_totals.assign (kEntryCount, 0);
    m_last.assign   (kLastCount, HeatLastAccess());

    m_mostTotal    = {};
    m_foldedAt     = cycle;
    m_countFrom    = (m_position != nullptr) ? *m_position : 0;
    m_lastFrom     = m_countFrom;
    m_operandsLeft = 0;

    m_lastRead.reset();
    m_instruction.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::Stop
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::Stop()
{
    std::vector<uint32_t>        noCounts;
    std::vector<float>           noHeat;
    std::vector<int64_t>         noTotals;
    std::vector<HeatLastAccess>  noLast;



    m_counts.swap (noCounts);
    m_heat.swap   (noHeat);
    m_totals.swap (noTotals);
    m_last.swap   (noLast);

    m_mostTotal      = {};
    m_instructionSet = nullptr;
    m_operandsLeft   = 0;

    m_lastRead.reset();
    m_instruction.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::Fold
//
//  The heat fades by the fade per frame for each frame of cycles since the
//  last fold, then takes the counts, which the totals take too. A cycle count
//  that went backward, as a rewind makes it, fades nothing.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::Fold (uint64_t cycle)
{
    double  frames = 0.0;
    float   fade   = 1.0f;
    size_t  space  = 0;



    if (!IsOn())
    {
        return;
    }

    if (cycle > m_foldedAt)
    {
        frames = (double) (cycle - m_foldedAt) / (double) kCyclesPerFrame;
    }

    fade       = (float) std::pow (m_fadePerFrame, frames);
    m_foldedAt = cycle;

    for (size_t i = 0; i < m_heat.size(); i++)
    {
        float  heat = m_heat[i] * fade + (float) m_counts[i];



        space              = i / kSpaceEntryCount;
        m_heat[i]          = (heat < kColdHeat) ? 0.0f : heat;
        m_totals[i]       += m_counts[i];
        m_mostTotal[space] = std::max (m_mostTotal[space], (uint64_t) std::max<int64_t> (m_totals[i], 0));
        m_counts[i]        = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::Reset
//
//  The last accesses stay as they are, since history keeps them as they
//  change, and are shown only from here on.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::Reset()
{
    std::ranges::fill (m_counts, 0u);
    std::ranges::fill (m_heat,   0.0f);
    std::ranges::fill (m_totals, 0ll);

    m_mostTotal = {};
    m_lastFrom  = (m_position != nullptr) ? *m_position : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetTotalsNow
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::GetTotalsNow (std::vector<int64_t> & outTotals) const
{
    outTotals.resize (m_totals.size());

    for (size_t i = 0; i < m_totals.size(); i++)
    {
        outTotals[i] = m_totals[i] + m_counts[i];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::SetTotals
//
//  The machine was put back where these were the totals, so the counts made
//  since the last fold belong to positions it no longer stands after, and
//  go; so does what the last read left owed, and the instruction running.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::SetTotals (const std::vector<int64_t> & totals)
{
    bool  isSameSize = totals.size() == m_totals.size();



    if (!IsOn() || !isSameSize)
    {
        return;
    }

    m_totals = totals;

    std::ranges::fill (m_counts, 0u);

    m_lastRead.reset();
    m_instruction.reset();
    m_operandsLeft = 0;

    FindMostTotal();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetLastAccess
//
//  A record from before the position the map was started or reset at is
//  none. One from at or after the machine's position is of the future the
//  machine has been back from, and the access before here is not known.
//
////////////////////////////////////////////////////////////////////////////////

HeatAccessState AccessHeatMap::GetLastAccess (
    HeatSpace          space,
    bool               isWrite,
    Word               address,
    HeatLastAccess   & outAccess) const
{
    HeatLastAccess  access;



    outAccess = HeatLastAccess();

    if (!IsOn())
    {
        return HeatAccessState::None;
    }

    access = m_last[GetLastIndex (space, isWrite, address)];

    if (access.IsForgotten())
    {
        return HeatAccessState::Unknown;
    }

    if (access.IsNone() || access.GetPosition() < m_lastFrom)
    {
        return HeatAccessState::None;
    }

    if (m_position != nullptr && access.GetPosition() >= *m_position)
    {
        return HeatAccessState::Unknown;
    }

    outAccess = access;
    return HeatAccessState::Found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::ForgetAccessesFrom
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::ForgetAccessesFrom (uint64_t position)
{
    for (HeatLastAccess & access : m_last)
    {
        if (!access.IsNone() && !access.IsForgotten() && access.GetPosition() >= position)
        {
            access.stamp = HeatLastAccess::kForgotten;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::ClearHeat
//
//  After a move through history: the counts the move's replay made are the
//  totals' to keep, while the heat starts over as of cycle, to be rebuilt.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::ClearHeat (uint64_t cycle)
{
    if (!IsOn())
    {
        return;
    }

    for (size_t i = 0; i < m_totals.size(); i++)
    {
        m_totals[i] += m_counts[i];
        m_counts[i]  = 0;
    }

    std::ranges::fill (m_heat, 0.0f);

    m_foldedAt = cycle;

    FindMostTotal();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::MergeHeat
//
//  Heat built as of cycle, faded by the frames from there to the last fold,
//  joins what the map has gathered since; fading is linear, so the sum is
//  what one map would hold. Heat from a cycle after the last fold is added
//  as it is.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::MergeHeat (
    const std::vector<float>  & heat,
    uint64_t                    cycle)
{
    bool    isSameSize = heat.size() == m_heat.size();
    double  frames     = 0.0;
    float   fade       = 1.0f;
    float   merged     = 0.0f;



    if (!IsOn() || !isSameSize)
    {
        return;
    }

    if (m_foldedAt > cycle)
    {
        frames = (double) (m_foldedAt - cycle) / (double) kCyclesPerFrame;
    }

    fade = (float) std::pow (m_fadePerFrame, frames);

    for (size_t i = 0; i < m_heat.size(); i++)
    {
        merged    = m_heat[i] + heat[i] * fade;
        m_heat[i] = (merged < kColdHeat) ? 0.0f : merged;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::FindMostTotal
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::FindMostTotal()
{
    size_t  space = 0;



    m_mostTotal = {};

    for (size_t i = 0; i < m_totals.size(); i++)
    {
        space              = i / kSpaceEntryCount;
        m_mostTotal[space] = std::max (m_mostTotal[space], (uint64_t) std::max<int64_t> (m_totals[i], 0));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::SetFadeSeconds
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::SetFadeSeconds (double seconds)
{
    m_fadeSeconds  = seconds;
    m_fadePerFrame = MakeFadePerFrame (seconds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::MakeFadePerFrame
//
//  What one frame leaves of the heat, such that a single access, a heat of
//  1, falls below kColdHeat after `seconds` of frames.
//
////////////////////////////////////////////////////////////////////////////////

double AccessHeatMap::MakeFadePerFrame (double seconds)
{
    constexpr double  kShortest = 0.1;
    double            frames    = std::max (seconds, kShortest) * kCyclesPerSecond / (double) kCyclesPerFrame;



    return std::pow ((double) kColdHeat, 1.0 / frames);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetRate
//
//  An address touched n times every frame settles at a heat of n over what
//  each frame lets fade, so that much of the heat stands for n a frame.
//
////////////////////////////////////////////////////////////////////////////////

double AccessHeatMap::GetRate (HeatSpace space, HeatKind kind, Word address) const
{
    return (double) GetHeat (space, kind, address) * GetRatePerHeat();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetHeat
//
////////////////////////////////////////////////////////////////////////////////

float AccessHeatMap::GetHeat (HeatSpace space, HeatKind kind, Word address) const
{
    return IsOn() ? m_heat[GetIndex (space, kind, address)] : 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetTotal
//
////////////////////////////////////////////////////////////////////////////////

uint64_t AccessHeatMap::GetTotal (HeatSpace space, HeatKind kind, Word address) const
{
    return IsOn() ? (uint64_t) std::max<int64_t> (m_totals[GetIndex (space, kind, address)], 0) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetLevels
//
//  One level per address for one kind, of the rate its heat stands for;
//  empty while the map is off.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::GetLevels (HeatSpace space, HeatKind kind, std::vector<Byte> & levels) const
{
    size_t  first   = GetIndex (space, kind, 0);
    double  perHeat = GetRatePerHeat();



    levels.clear();

    if (!IsOn())
    {
        return;
    }

    levels.resize (kAddressCount);

    for (size_t address = 0; address < kAddressCount; address++)
    {
        levels[address] = ToLevel ((double) m_heat[first + address] * perHeat, kHottestPerSecond);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetTotalLevels
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::GetTotalLevels (HeatSpace space, HeatKind kind, std::vector<Byte> & levels) const
{
    size_t  first = GetIndex (space, kind, 0);
    double  top   = (double) GetMostTotal (space);



    levels.clear();

    if (!IsOn())
    {
        return;
    }

    levels.resize (kAddressCount);

    for (size_t address = 0; address < kAddressCount; address++)
    {
        levels[address] = ToLevel ((double) std::max<int64_t> (m_totals[first + address], 0), top);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::ToLevel
//
//  Logarithmic, so one touch and a tight loop's millions both show, and a
//  few busy loops do not wash out everything else: any value above zero is
//  at least 1, and `top` or more is 255.
//
////////////////////////////////////////////////////////////////////////////////

Byte AccessHeatMap::ToLevel (double value, double top)
{
    constexpr double  kTop   = 255.0;
    double            scaled = kTop;



    if (value <= 0.0)
    {
        return 0;
    }

    if (top > value)
    {
        scaled = kTop * std::log1p (value) / std::log1p (top);
    }

    return (Byte) std::clamp (std::lround (scaled), 1L, (long) kTop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::Count
//
//  One access of a kind, at the address the CPU used and where the bank map
//  says it landed.
//
////////////////////////////////////////////////////////////////////////////////

AccessHeatMap::Landing AccessHeatMap::Count (
    HeatKind  kind,
    Word      address,
    bool      isWrite)
{
    Landing       landing;
    HeatLocation  location;



    landing.cpu = GetIndex (kind, address);
    m_counts[landing.cpu]++;

    landing.hasBank = m_bankMap.TryResolve (address, isWrite, location);

    if (landing.hasBank)
    {
        landing.space = location.space;
        landing.index = location.index;
        landing.bank  = GetIndex (location.space, kind, location.index);
        m_counts[landing.bank]++;
    }

    return landing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::RecordLast
//
//  The instruction running as the last to write or read the address, where
//  the CPU addressed it and where it landed, unless a record there is from
//  later on, as when the past is replayed. Nothing before the first fetch is
//  known to be any instruction's.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::RecordLast (
    bool              isWrite,
    Word              address,
    const Landing   & landing)
{
    HeatLastAccess  * cpu  = nullptr;
    HeatLastAccess  * bank = nullptr;



    if (!m_instruction.has_value())
    {
        return;
    }

    cpu = &m_last[GetLastIndex (HeatSpace::Cpu, isWrite, address)];

    if (cpu->stamp <= m_instruction->stamp || cpu->IsForgotten())
    {
        *cpu = *m_instruction;
    }

    if (!landing.hasBank)
    {
        return;
    }

    bank = &m_last[GetLastIndex (landing.space, isWrite, landing.index)];

    if (bank->stamp <= m_instruction->stamp || bank->IsForgotten())
    {
        *bank = *m_instruction;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::OnWatchedAccess
//
//  The operand bytes the last fetch is still owed are executed, not read; any
//  other read ends them, since the CPU fetches them before anything else.
//  A read may be the next opcode's, which its fetch then takes back, so the
//  last reads it replaces are kept until then.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::OnWatchedAccess (Word address, Byte value, BusAccess access, std::optional<Byte> previous)
{
    Landing  landing;



    (void) value;
    (void) previous;

    if (!IsOn() || !IsCounting())
    {
        return;
    }

    if (access == BusAccess::Write)
    {
        m_operandsLeft = 0;
        landing        = Count (HeatKind::Write, address, true);

        RecordLast (true, address, landing);
        return;
    }

    if (m_operandsLeft > 0 && address == m_nextOperand)
    {
        m_nextOperand++;
        m_operandsLeft--;
        return;
    }

    m_operandsLeft    = 0;
    m_lastRead        = address;
    m_lastReadLanding = Count (HeatKind::Read, address, false);
    m_lastReadWasCpu  = m_last[GetLastIndex (HeatSpace::Cpu, false, address)];

    if (m_lastReadLanding.hasBank)
    {
        m_lastReadWasBank = m_last[GetLastIndex (m_lastReadLanding.space, false, m_lastReadLanding.index)];
    }

    RecordLast (false, address, m_lastReadLanding);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::OnFetch
//
//  The bus reported the opcode as a read just before; it becomes an execute,
//  as do the operand bytes the instruction takes, which the bus reports next.
//  The read gives back the last reads it took, and the instruction fetched
//  is the one running from here.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::OnFetch (Word pc, Byte opcode)
{
    int       length   = 1;
    uint64_t  position = 0;
    uint64_t  cycle    = 0;



    if (!IsOn() || !IsCounting())
    {
        return;
    }

    if (m_instructionSet != nullptr && m_instructionSet[opcode].isLegal)
    {
        length = 1 + (int) OpcodeTable::GetOperandSize (m_instructionSet[opcode].globalAddressingMode);
    }

    if (m_lastRead == pc && m_counts[m_lastReadLanding.cpu] > 0)
    {
        m_counts[m_lastReadLanding.cpu]--;
        m_last[GetLastIndex (HeatSpace::Cpu, false, pc)] = m_lastReadWasCpu;

        if (m_lastReadLanding.hasBank && m_counts[m_lastReadLanding.bank] > 0)
        {
            m_counts[m_lastReadLanding.bank]--;
            m_last[GetLastIndex (m_lastReadLanding.space, false, m_lastReadLanding.index)] = m_lastReadWasBank;
        }
    }

    for (int i = 0; i < length; i++)
    {
        (void) Count (HeatKind::Execute, (Word) (pc + i), false);
    }

    position = (m_position != nullptr) ? *m_position : 0;
    cycle    = (m_cycles   != nullptr) ? *m_cycles   : 0;

    m_instruction  = HeatLastAccess::Make (pc, position, cycle);
    m_nextOperand  = (Word) (pc + 1);
    m_operandsLeft = length - 1;

    m_lastRead.reset();
}





