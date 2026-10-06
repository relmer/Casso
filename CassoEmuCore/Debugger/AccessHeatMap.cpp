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

    m_counts.assign (kKindCount * kAddressCount, 0);
    m_heat.assign   (kKindCount * kAddressCount, 0.0f);
    m_totals.assign (kKindCount * kAddressCount, 0);

    m_mostTotal    = 0;
    m_foldedAt     = cycle;
    m_lastRead.reset();
    m_operandsLeft = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::Stop
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::Stop()
{
    std::vector<uint32_t>  noCounts;
    std::vector<float>     noHeat;
    std::vector<uint64_t>  noTotals;



    m_counts.swap (noCounts);
    m_heat.swap   (noHeat);
    m_totals.swap (noTotals);

    m_mostTotal      = 0;
    m_instructionSet = nullptr;
    m_lastRead.reset();
    m_operandsLeft   = 0;
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



        m_heat[i]   = (heat < kColdHeat) ? 0.0f : heat;
        m_totals[i] += m_counts[i];
        m_mostTotal  = std::max (m_mostTotal, m_totals[i]);
        m_counts[i]  = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::Reset
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::Reset()
{
    std::ranges::fill (m_counts, 0u);
    std::ranges::fill (m_heat,   0.0f);
    std::ranges::fill (m_totals, 0ull);

    m_mostTotal = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::SetFadeSeconds
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::SetFadeSeconds (double seconds)
{
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

double AccessHeatMap::GetRate (HeatKind kind, Word address) const
{
    return (double) GetHeat (kind, address) * GetRatePerHeat();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetHeat
//
////////////////////////////////////////////////////////////////////////////////

float AccessHeatMap::GetHeat (HeatKind kind, Word address) const
{
    return IsOn() ? m_heat[GetIndex (kind, address)] : 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetTotal
//
////////////////////////////////////////////////////////////////////////////////

uint64_t AccessHeatMap::GetTotal (HeatKind kind, Word address) const
{
    return IsOn() ? m_totals[GetIndex (kind, address)] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetLevels
//
//  One level per address for one kind, of the rate its heat stands for;
//  empty while the map is off.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::GetLevels (HeatKind kind, std::vector<Byte> & levels) const
{
    size_t  first   = GetIndex (kind, 0);
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

void AccessHeatMap::GetTotalLevels (HeatKind kind, std::vector<Byte> & levels) const
{
    size_t  first = GetIndex (kind, 0);
    double  top   = (double) m_mostTotal;



    levels.clear();

    if (!IsOn())
    {
        return;
    }

    levels.resize (kAddressCount);

    for (size_t address = 0; address < kAddressCount; address++)
    {
        levels[address] = ToLevel ((double) m_totals[first + address], top);
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
//  AccessHeatMap::OnWatchedAccess
//
//  The operand bytes the last fetch is still owed are executed, not read; any
//  other read ends them, since the CPU fetches them before anything else.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::OnWatchedAccess (Word address, Byte value, BusAccess access, std::optional<Byte> previous)
{
    (void) value;
    (void) previous;



    if (!IsOn())
    {
        return;
    }

    if (access == BusAccess::Write)
    {
        m_operandsLeft = 0;
        m_counts[GetIndex (HeatKind::Write, address)]++;
        return;
    }

    if (m_operandsLeft > 0 && address == m_nextOperand)
    {
        m_nextOperand++;
        m_operandsLeft--;
        return;
    }

    m_operandsLeft = 0;
    m_lastRead     = address;
    m_counts[GetIndex (HeatKind::Read, address)]++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::OnFetch
//
//  The bus reported the opcode as a read just before; it becomes an execute,
//  as do the operand bytes the instruction takes, which the bus reports next.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::OnFetch (Word pc, Byte opcode)
{
    size_t  readIndex = GetIndex (HeatKind::Read, pc);
    int     length    = 1;



    if (!IsOn())
    {
        return;
    }

    if (m_instructionSet != nullptr && m_instructionSet[opcode].isLegal)
    {
        length = 1 + (int) OpcodeTable::GetOperandSize (m_instructionSet[opcode].globalAddressingMode);
    }

    if (m_lastRead == pc && m_counts[readIndex] > 0)
    {
        m_counts[readIndex]--;
    }

    for (int i = 0; i < length; i++)
    {
        m_counts[GetIndex (HeatKind::Execute, (Word) (pc + i))]++;
    }

    m_lastRead.reset();
    m_nextOperand  = (Word) (pc + 1);
    m_operandsLeft = length - 1;
}





