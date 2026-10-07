#pragma once

#include "Cpu.h"
#include "Core/IWatchSink.h"
#include "Debugger/HeatMapOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatKind
//
////////////////////////////////////////////////////////////////////////////////

enum class HeatKind
{
    Execute,
    Read,
    Write,
};





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap
//
//  How often each of the 64 KB addresses was executed, read and written. The
//  CPU reports every read and write it makes and every opcode it fetches; an
//  instruction's bytes count as executed, not read.
//
//  Accesses are counted as they come and folded in on Fold, two ways. The
//  heat fades: what was there is first faded by the frames of machine time
//  that went by since the last fold, so a busy address glows bright while it
//  stays busy and dims once the program leaves it, and a single access is gone
//  after the fade time. The totals never fade, and only Reset clears them.
//
//  The tables are allocated by Start and freed by Stop; a map that is off
//  holds no memory and ignores whatever it is told.
//
//  Given the machine's position, the map counts only the instructions at
//  or after the position it was started at, so a replay of the past from
//  before it counts nothing there either. Reverse execution sets the totals
//  to what they were at a keyframe it loads (SetTotals), and clears the heat
//  after a move for a rebuilt heat to be merged in (ClearHeat, MergeHeat).
//  The totals are kept signed, since history's arithmetic can put a position
//  before a reset below zero; they read as zero there.
//
////////////////////////////////////////////////////////////////////////////////

class AccessHeatMap : public IWatchSink, public IFetchSink
{
public:
    static constexpr size_t    kAddressCount    = 0x10000;
    static constexpr size_t    kKindCount       = 3;
    static constexpr size_t    kEntryCount      = kKindCount * kAddressCount;
    static constexpr uint64_t  kCyclesPerFrame  = 17030;
    static constexpr double    kCyclesPerSecond = 1020484.0;
    static constexpr double    kFramesPerSecond = kCyclesPerSecond / (double) kCyclesPerFrame;
    static constexpr float     kColdHeat        = 0.01f;    // below this an address is cold again

    //  The rate, in accesses a second, shown as level 255. The fading levels
    //  are of the rate the heat stands for, not of the heat itself, so a
    //  longer fade keeps an address on the map longer without making a
    //  steady one look hotter.
    static constexpr double    kHottestPerSecond = 50000.0;

    void   Start (const Microcode * instructionSet, uint64_t cycle);
    void   Stop  ();
    bool   IsOn  () const { return !m_heat.empty(); }

    //  The machine's position, read before each access; null counts every
    //  access. Start counts from where it points then, or SetCountFrom from
    //  a position given.
    void       SetPositionSource (const uint64_t * position) { m_position = position; }
    void       SetCountFrom      (uint64_t position)         { m_countFrom = position; }

    //  The counts since the last fold, faded heat plus counts after it, and
    //  added to the totals.
    void   Fold  (uint64_t cycle);

    //  Zeroes the heat, the totals and the counts not yet folded; a map that
    //  is on stays on.
    void   Reset ();

    //  The totals as they stand, counts not yet folded included; the totals
    //  set, with the counts not yet folded dropped.
    void   GetTotalsNow (std::vector<int64_t> & outTotals) const;
    void   SetTotals    (const std::vector<int64_t> & totals);

    //  The counts not yet folded go to the totals alone and the heat is
    //  cleared, as of cycle; heat built elsewhere as of a cycle is faded to
    //  the last fold and added in.
    void   ClearHeat (uint64_t cycle);
    void   MergeHeat (const std::vector<float> & heat, uint64_t cycle);

    const std::vector<float> &  GetHeatTable () const { return m_heat; }

    //  How long, in seconds of machine time, a single access stays on the
    //  map before it fades to cold.
    void   SetFadeSeconds  (double seconds);
    double GetFadeSeconds  () const { return m_fadeSeconds; }
    double GetFadePerFrame () const { return m_fadePerFrame; }

    //  The heat, the rate in accesses a second it stands for, and the total.
    float     GetHeat  (HeatKind kind, Word address) const;
    double    GetRate  (HeatKind kind, Word address) const;
    uint64_t  GetTotal (HeatKind kind, Word address) const;

    //  The busiest address's total, of any kind: what the totals' level 255
    //  stands for.
    uint64_t  GetMostTotal () const { return m_mostTotal; }

    //  One level per address for one kind, from the heat or from the totals;
    //  the totals' top level is the busiest address of any kind.
    void   GetLevels      (HeatKind kind, std::vector<Byte> & levels) const;
    void   GetTotalLevels (HeatKind kind, std::vector<Byte> & levels) const;

    //  0 for none, 1 to 255 rising with the logarithm of the value, 255 at
    //  `top` and above.
    static Byte  ToLevel (double value, double top);

    // IWatchSink
    void   OnWatchedAccess (Word                  address,
                            Byte                  value,
                            BusAccess             access,
                            std::optional<Byte>   previous) override;

    // IFetchSink
    void   OnFetch (Word pc, Byte opcode) override;

    static size_t  GetIndex (HeatKind kind, Word address) { return (size_t) kind * kAddressCount + address; }

private:
    static double  MakeFadePerFrame (double seconds);
    double         GetRatePerHeat   () const { return (1.0 - m_fadePerFrame) * kFramesPerSecond; }
    bool           IsCounting       () const { return m_position == nullptr || *m_position >= m_countFrom; }
    void           FindMostTotal    ();

    const Microcode        * m_instructionSet = nullptr;
    const uint64_t         * m_position       = nullptr;
    uint64_t                 m_countFrom      = 0;
    std::vector<uint32_t>    m_counts;
    std::vector<float>       m_heat;
    std::vector<int64_t>     m_totals;
    uint64_t                 m_mostTotal      = 0;
    uint64_t                 m_foldedAt       = 0;
    double                   m_fadeSeconds    = HeatMapOptions::kDefaultFadeSeconds;
    double                   m_fadePerFrame   = MakeFadePerFrame (HeatMapOptions::kDefaultFadeSeconds);

    //  The last read the bus reported, which is the opcode when a fetch is
    //  reported next, and the operand bytes still to come for that opcode.
    std::optional<Word>      m_lastRead;
    Word                     m_nextOperand    = 0;
    int                      m_operandsLeft   = 0;
};
