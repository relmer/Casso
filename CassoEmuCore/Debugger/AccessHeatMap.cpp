#include "Pch.h"

#include "Debugger/AccessHeatMap.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::Start
//
//  A map already on keeps its heat; the instruction set is taken again, since
//  a machine switch may have brought another CPU. At cycle zero the machine
//  is at power-on and nothing has written its RAM; later, what was written
//  before is not known, so every byte counts as written.
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
    m_opcodes.assign (kSpaceCount * kAddressCount, 0);

    m_mostTotal    = {};
    m_foldedAt     = cycle;
    m_countFrom    = (m_position != nullptr) ? *m_position : 0;
    m_lastFrom     = m_countFrom;
    m_operandsLeft = 0;

    m_isStartedClear      = cycle == 0;
    m_isTracking          = m_isStartedClear;
    m_isWrittenChanged    = true;
    m_isLastReadUnwritten = false;
    m_isLastReadStop      = false;

    MakeWrittenBits (!m_isStartedClear, m_written);

    m_lastRead.reset();
    m_instruction.reset();
    m_previous.reset();
    m_unwrittenStop.reset();
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
    std::vector<uint64_t>        noWritten;
    std::vector<Byte>            noOpcodes;



    m_counts.swap  (noCounts);
    m_heat.swap    (noHeat);
    m_totals.swap  (noTotals);
    m_last.swap    (noLast);
    m_written.swap (noWritten);
    m_opcodes.swap (noOpcodes);

    m_mostTotal      = {};
    m_instructionSet = nullptr;
    m_operandsLeft   = 0;
    m_isTracking     = false;

    m_lastRead.reset();
    m_instruction.reset();
    m_previous.reset();
    m_unwrittenStop.reset();
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
    std::ranges::fill (m_opcodes, (Byte) 0);

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
//  AccessHeatMap::MarkOpcode
//
//  An opcode fetched, where the CPU addressed it and where it landed.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::MarkOpcode (
    Word              pc,
    const Landing   & landing)
{
    m_opcodes[pc] = 1;

    if (landing.hasBank)
    {
        m_opcodes[(size_t) landing.space * kAddressCount + landing.index] = 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetOpcodeMarks
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::GetOpcodeMarks (HeatSpace space, std::vector<Byte> & marks) const
{
    size_t  first = (size_t) space * kAddressCount;



    marks.clear();

    if (!IsOn() || m_opcodes.size() < first + kAddressCount)
    {
        return;
    }

    marks.assign (m_opcodes.begin() + (ptrdiff_t) first, m_opcodes.begin() + (ptrdiff_t) (first + kAddressCount));
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
    (void) previous;

    if (!IsOn() || !IsCounting())
    {
        return;
    }

    if (access == BusAccess::Write)
    {
        OnWrite (address, value);
        return;
    }

    if (m_operandsLeft > 0 && address == m_nextOperand)
    {
        m_nextOperand++;
        m_operandsLeft--;
        return;
    }

    OnRead (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::OnWrite
//
//  A write, a change too when the byte it replaced, read just before, held
//  something else or could not be read. The byte it lands on is written
//  from here on.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::OnWrite (Word address, Byte value)
{
    Landing  landing;
    bool     isChange = true;
    size_t   bit      = 0;



    m_operandsLeft = 0;
    landing        = Count (HeatKind::Write, address, true);
    isChange       = !m_previous.has_value() || m_previousAt != address || *m_previous != value;

    m_previous.reset();

    if (isChange)
    {
        CountAlso (HeatKind::ChangedWrite, address, landing);
    }

    if (TryGetWrittenBit (landing, bit) && !IsBitSet (m_written, bit))
    {
        SetBit (m_written, bit);

        m_isWrittenChanged = true;
    }

    RecordLast (true, address, landing);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::OnRead
//
//  A read as data, unless the fetch that follows takes it back as the
//  opcode. One of RAM nothing has written is a read before written too, and,
//  with the break armed and no stop held, outside the ranges left out, the
//  stop.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::OnRead (Word address, Byte value)
{
    size_t  bit = 0;



    m_operandsLeft        = 0;
    m_lastRead            = address;
    m_lastReadLanding     = Count (HeatKind::Read, address, false);
    m_lastReadWasCpu      = m_last[GetLastIndex (HeatSpace::Cpu, false, address)];
    m_isLastReadUnwritten = TryGetWrittenBit (m_lastReadLanding, bit) && !IsBitSet (m_written, bit);
    m_isLastReadStop      = false;

    if (m_lastReadLanding.hasBank)
    {
        m_lastReadWasBank = m_last[GetLastIndex (m_lastReadLanding.space, false, m_lastReadLanding.index)];
    }

    if (m_isLastReadUnwritten)
    {
        CountAlso (HeatKind::UnwrittenRead, address, m_lastReadLanding);
    }

    if (m_isLastReadUnwritten && m_isBreakArmed && !m_unwrittenStop.has_value() && !IsUnwrittenIgnored (address))
    {
        m_unwrittenStop  = HeatUnwrittenRead { address, value, m_instruction.has_value() ? m_instruction->GetPc() : (Word) 0 };
        m_isLastReadStop = true;
    }

    RecordLast (false, address, m_lastReadLanding);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::OnBeforeWrite
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::OnBeforeWrite (Word address)
{
    const Byte  * cell = nullptr;



    m_previous.reset();

    if (!IsOn() || !IsCounting())
    {
        return;
    }

    cell = m_bankMap.GetCell (address, true);

    if (cell != nullptr)
    {
        m_previous   = *cell;
        m_previousAt = address;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::OnPowerCycle
//
//  RAM holds the power-on pattern again, which nothing wrote, so every bit
//  clears and from here the map knows which bytes are written. Before the
//  map counts, as in a replay of the past, a power cycle is no more its
//  business than any access.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::OnPowerCycle()
{
    if (!IsOn() || !IsCounting())
    {
        return;
    }

    MakeWrittenBits (false, m_written);

    m_isTracking       = true;
    m_isWrittenChanged = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::CountAlso
//
//  A second kind for an access already counted, where the CPU addressed it
//  and where it landed.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::CountAlso (
    HeatKind          kind,
    Word              address,
    const Landing   & landing)
{
    m_counts[GetIndex (kind, address)]++;

    if (landing.hasBank)
    {
        m_counts[GetIndex (landing.space, kind, landing.index)]++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::UncountAlso
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::UncountAlso (
    HeatKind          kind,
    Word              address,
    const Landing   & landing)
{
    size_t  cpu  = GetIndex (kind, address);
    size_t  bank = landing.hasBank ? GetIndex (landing.space, kind, landing.index) : 0;



    if (m_counts[cpu] > 0)
    {
        m_counts[cpu]--;
    }

    if (landing.hasBank && m_counts[bank] > 0)
    {
        m_counts[bank]--;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::TryGetWrittenBit
//
//  Main RAM's bits come first, then aux RAM's; anything else has none.
//
////////////////////////////////////////////////////////////////////////////////

bool AccessHeatMap::TryGetWrittenBit (
    const Landing   & landing,
    size_t          & outBit)
{
    if (!landing.hasBank || (landing.space != HeatSpace::Main && landing.space != HeatSpace::Aux))
    {
        return false;
    }

    outBit = ((landing.space == HeatSpace::Aux) ? kAddressCount : 0) + landing.index;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::IsWritten
//
//  Anything but RAM, and anything while the map is off, counts as written.
//
////////////////////////////////////////////////////////////////////////////////

bool AccessHeatMap::IsWritten (HeatSpace space, Word index) const
{
    Landing  landing;
    size_t   bit     = 0;



    landing.hasBank = true;
    landing.space   = space;
    landing.index   = index;

    if (!IsOn() || !TryGetWrittenBit (landing, bit))
    {
        return true;
    }

    return IsBitSet (m_written, bit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::NoteHostWrite
//
//  The debugger put a value there on purpose, so a read of it is no read of
//  whatever power-on left.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::NoteHostWrite (Word address)
{
    Landing       landing;
    HeatLocation  location;
    size_t        bit      = 0;



    if (!IsOn())
    {
        return;
    }

    landing.hasBank = m_bankMap.TryResolve (address, true, location);
    landing.space   = location.space;
    landing.index   = location.index;

    if (TryGetWrittenBit (landing, bit) && !IsBitSet (m_written, bit))
    {
        SetBit (m_written, bit);

        m_isWrittenChanged = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::SetWrittenBits
//
//  As history kept them at a keyframe now loaded. The map knows them from
//  here, unless they are every bit set, which says only that nothing was
//  known.
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::SetWrittenBits (const std::vector<uint64_t> & bits)
{
    bool  isSameSize = bits.size() == kWrittenWords;



    if (!IsOn())
    {
        return;
    }

    if (isSameSize)
    {
        m_written = bits;
    }
    else
    {
        MakeWrittenBits (true, m_written);
    }

    m_isTracking       = std::ranges::any_of (m_written, [] (uint64_t word) { return word != UINT64_MAX; });
    m_isWrittenChanged = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::TakeWrittenChange
//
//  Whether the bits changed since this was last asked; asking clears it.
//
////////////////////////////////////////////////////////////////////////////////

bool AccessHeatMap::TakeWrittenChange()
{
    bool  isChanged = m_isWrittenChanged;



    m_isWrittenChanged = false;
    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::MakeWrittenBits
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::MakeWrittenBits (
    bool                     isWritten,
    std::vector<uint64_t>  & outBits)
{
    outBits.assign (kWrittenWords, isWritten ? UINT64_MAX : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::TryGetUnwrittenStop
//
////////////////////////////////////////////////////////////////////////////////

bool AccessHeatMap::TryGetUnwrittenStop (HeatUnwrittenRead & outRead) const
{
    if (!m_unwrittenStop.has_value())
    {
        return false;
    }

    outRead = *m_unwrittenStop;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::SetUnwrittenIgnore
//
////////////////////////////////////////////////////////////////////////////////

void AccessHeatMap::SetUnwrittenIgnore (const std::vector<std::pair<Word, Word>> & spans)
{
    std::ranges::fill (m_ignored, 0ull);

    for (const auto & [first, last] : spans)
    {
        for (size_t address = first; address <= last; address++)
        {
            SetBit (m_ignored, address);
        }
    }

    m_ignoredSpans = spans.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::IsUnwrittenIgnored
//
////////////////////////////////////////////////////////////////////////////////

bool AccessHeatMap::IsUnwrittenIgnored (Word address) const
{
    return IsBitSet (m_ignored, address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap::GetUnwrittenStatus
//
//  The totals and the counts not yet folded, in the CPU's space, of every
//  address not left out.
//
////////////////////////////////////////////////////////////////////////////////

HeatUnwrittenStatus AccessHeatMap::GetUnwrittenStatus() const
{
    HeatUnwrittenStatus  status;
    size_t               first  = GetIndex (HeatKind::UnwrittenRead, 0);
    int64_t              reads  = 0;



    status.isOn         = IsOn();
    status.isTracking   = IsTrackingWrites();
    status.ignoredSpans = m_ignoredSpans;

    if (!IsOn())
    {
        return status;
    }

    for (size_t address = 0; address < kAddressCount; address++)
    {
        reads = m_totals[first + address] + m_counts[first + address];

        if (reads <= 0 || IsUnwrittenIgnored ((Word) address))
        {
            continue;
        }

        status.reads += (uint64_t) reads;
        status.addresses++;
    }

    return status;
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

        if (m_isLastReadUnwritten)
        {
            UncountAlso (HeatKind::UnwrittenRead, pc, m_lastReadLanding);
        }

        if (m_isLastReadStop)
        {
            m_unwrittenStop.reset();
        }
    }

    m_isLastReadUnwritten = false;
    m_isLastReadStop      = false;

    MarkOpcode (pc, Count (HeatKind::Execute, pc, false));

    for (int i = 1; i < length; i++)
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





