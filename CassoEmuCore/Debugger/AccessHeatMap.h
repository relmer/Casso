#pragma once

#include "Cpu.h"
#include "Core/IWatchSink.h"
#include "Debugger/HeatBankMap.h"
#include "Debugger/HeatMapOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatKind
//
//  Besides executes, reads and writes, the reads of RAM nothing had written
//  since power-on (UnwrittenRead), and the writes that changed the byte they
//  stored to (ChangedWrite); each is also counted as the read or the write
//  it is.
//
////////////////////////////////////////////////////////////////////////////////

enum class HeatKind
{
    Execute,
    Read,
    Write,
    UnwrittenRead,
    ChangedWrite,
};





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMap
//
//  How often each address was executed, read and written. The CPU reports
//  every read and write it makes and every opcode it fetches; an
//  instruction's bytes count as executed, not read.
//
//  Each access counts twice: at the address the CPU used, in the Cpu space,
//  and, given a bank map, where it landed, in the space of the bank that
//  holds it (HeatSpace), so the map can show main and aux RAM, the language
//  card's banks and ROM apart.
//
//  Accesses are counted as they come and folded in on Fold, two ways. The
//  heat fades: what was there is first faded by the frames of machine time
//  that went by since the last fold, so a busy address glows bright while it
//  stays busy and dims once the program leaves it, and a single access is gone
//  after the fade time. The totals never fade, and only Reset clears them.
//
//  Beside the counts, for every address in every space, the instruction that
//  last wrote it and the one that last read it as data (HeatLastAccess). An
//  interrupt's pushes are put down to the instruction it interrupted. A
//  record only ever moves later: a replay of the past leaves a later record
//  in place. So the record is the last access up to the furthest the machine
//  has run, and as of any position before that it is the last access there
//  when it is from before it, and unknown otherwise; history finds the one
//  before (HeatHistory::LookUpLastAccess).
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
//  before a reset below zero; they read as zero there. A last access from
//  before the map was started or reset reads as none.
//
//  Which RAM has been written. For every byte of main and aux RAM, the
//  language card's banks among them, one bit says whether the CPU has
//  written it since power-on; a read that lands on a byte whose bit is clear
//  counts as an UnwrittenRead, and, with the break armed, is held as a stop
//  for the debugger to take. A power cycle clears every bit, and a reset
//  leaves them, as the RAM itself keeps its bytes across a reset. A map
//  started on a machine that has run since power-on cannot know what was
//  written before it, so every bit starts set and nothing counts until the
//  next power cycle; one started at power-on, cycle zero, starts clear.
//  Reset leaves the bits alone, since they say what the RAM holds, not what
//  was counted.
//
//  The debugger's own writes, a memory window's edit, a poke, a fill or a
//  binary loaded, are no guest's: they count in no kind, record no last
//  writer and set no written bit. Each marks its byte as edited instead, in
//  the CPU's space and where it landed, and a read of an edited byte is no
//  read before written, since the debugger put the value there.
//
//  The kept bits: which RAM was written, which bytes the debugger edited and
//  where an opcode was fetched, all of which follow the machine's position.
//  History keeps them beside each keyframe, and a keyframe loaded puts them
//  back (SetKeptBits).
//
//  A write the CPU makes is told first, so the byte it replaces is read
//  before the store; a write that stores a different value counts as a
//  ChangedWrite too. A write the map cannot read the byte of, to I/O or where
//  no memory takes it, counts as a change.
//
////////////////////////////////////////////////////////////////////////////////

class AccessHeatMap : public IWatchSink, public IFetchSink
{
public:
    static constexpr size_t    kAddressCount    = 0x10000;
    static constexpr size_t    kKindCount       = 5;
    static constexpr size_t    kSpaceCount      = 4;
    static constexpr size_t    kSpaceEntryCount = kKindCount * kAddressCount;
    static constexpr size_t    kEntryCount      = kSpaceCount * kSpaceEntryCount;
    static constexpr size_t    kLastCount       = kSpaceCount * 2 * kAddressCount;   // a write and a read per address
    static constexpr size_t    kBitsPerWord     = 64;
    static constexpr size_t    kWrittenWords    = 2 * kAddressCount / kBitsPerWord;  // main RAM's bits, then aux RAM's
    static constexpr size_t    kAddressWords    = kAddressCount / kBitsPerWord;
    static constexpr size_t    kEditedWords     = 3 * kAddressWords;                 // the CPU's space, then main RAM's, then aux RAM's
    static constexpr size_t    kOpcodeWords     = kSpaceCount * kAddressWords;
    static constexpr size_t    kKeptWords       = kWrittenWords + kEditedWords + kOpcodeWords;
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

    //  The machine's position and cycle count, read on each access; a null
    //  position counts every access. Start counts from where it points then,
    //  or SetCountFrom from a position given.
    void       SetPositionSource (const uint64_t * position) { m_position = position; }
    void       SetCycleSource    (const uint64_t * cycles)   { m_cycles   = cycles;   }
    void       SetCountFrom      (uint64_t position)         { m_countFrom = position; m_lastFrom = position; }

    //  Where each access lands, beside the address the CPU used; cleared,
    //  every access counts in the Cpu space alone.
    HeatBankMap &        GetBankMap ()       { return m_bankMap; }
    const HeatBankMap &  GetBankMap () const { return m_bankMap; }

    //  The counts since the last fold, faded heat plus counts after it, and
    //  added to the totals.
    void   Fold  (uint64_t cycle);

    //  Zeroes the heat, the totals and the counts not yet folded, and shows
    //  no last access from before here; a map that is on stays on.
    void   Reset ();

    //  The totals as they stand, counts not yet folded included; the totals
    //  set, with the counts not yet folded dropped.
    void   GetTotalsNow (std::vector<int64_t> & outTotals) const;
    void   SetTotals    (const std::vector<int64_t> & totals);

    //  The instruction that last wrote or read an address in a space, as of
    //  where the machine stands.
    HeatAccessState  GetLastAccess (HeatSpace space, bool isWrite, Word address, HeatLastAccess & outAccess) const;

    //  History was cut at position: the records of the future it dropped are
    //  forgotten, and read as unknown until an access sets them again.
    void   ForgetAccessesFrom (uint64_t position);

    //  Where the records are shown from: where the map was started or reset;
    //  and where it counts from.
    uint64_t  GetLastFrom  () const { return m_lastFrom; }
    uint64_t  GetCountFrom () const { return m_countFrom; }

    //  The counts not yet folded go to the totals alone and the heat is
    //  cleared, as of cycle; heat built elsewhere as of a cycle is faded to
    //  the last fold and added in.
    void   ClearHeat (uint64_t cycle);
    void   MergeHeat (const std::vector<float> & heat, uint64_t cycle);

    const std::vector<float> &  GetHeatTable () const { return m_heat; }

    //  How many entries have heat or counts not yet folded: all a fold
    //  visits.
    size_t  GetListedCount () const { return m_listed.size(); }

    //  One mark per address of a space, nonzero where an opcode was fetched
    //  since the map was started or reset, as of where the machine stands:
    //  the bytes executed that are not marked were only ever an instruction's
    //  operands. Empty while off.
    void   GetOpcodeMarks (HeatSpace space, std::vector<Byte> & marks) const;

    //  One mark per address of a space, nonzero where the debugger wrote the
    //  byte since power-on; none in ROM's. Empty while off.
    void   GetEditedMarks (HeatSpace space, std::vector<Byte> & marks) const;

    //  How long, in seconds of machine time, a single access stays on the
    //  map before it fades to cold.
    void   SetFadeSeconds  (double seconds);
    double GetFadeSeconds  () const { return m_fadeSeconds; }
    double GetFadePerFrame () const { return m_fadePerFrame; }

    //  The heat, the rate in accesses a second it stands for, and the total,
    //  in the Cpu space or the one given.
    float     GetHeat  (HeatKind kind, Word address) const                  { return GetHeat  (HeatSpace::Cpu, kind, address); }
    double    GetRate  (HeatKind kind, Word address) const                  { return GetRate  (HeatSpace::Cpu, kind, address); }
    uint64_t  GetTotal (HeatKind kind, Word address) const                  { return GetTotal (HeatSpace::Cpu, kind, address); }
    float     GetHeat  (HeatSpace space, HeatKind kind, Word address) const;
    double    GetRate  (HeatSpace space, HeatKind kind, Word address) const;
    uint64_t  GetTotal (HeatSpace space, HeatKind kind, Word address) const;

    //  The busiest address's total in a space, of any kind: what the totals'
    //  level 255 stands for there.
    uint64_t  GetMostTotal () const                { return GetMostTotal (HeatSpace::Cpu); }
    uint64_t  GetMostTotal (HeatSpace space) const { return m_mostTotal[(size_t) space]; }

    //  One level per address for one kind in a space, from the heat or from
    //  the totals; the totals' top level is the space's busiest address of
    //  any kind.
    void   GetLevels      (HeatKind kind, std::vector<Byte> & levels) const      { GetLevels      (HeatSpace::Cpu, kind, levels); }
    void   GetTotalLevels (HeatKind kind, std::vector<Byte> & levels) const      { GetTotalLevels (HeatSpace::Cpu, kind, levels); }
    void   GetLevels      (HeatSpace space, HeatKind kind, std::vector<Byte> & levels) const;
    void   GetTotalLevels (HeatSpace space, HeatKind kind, std::vector<Byte> & levels) const;

    //  0 for none, 1 to 255 rising with the logarithm of the value, 255 at
    //  `top` and above.
    static Byte  ToLevel (double value, double top);

    //  Whether a byte of main or aux RAM has been written by the CPU since
    //  power-on, and whether the debugger wrote a byte of a space; the kept
    //  bits as a whole, to keep and put back, and whether any changed since
    //  they were last taken. Bits put back of the wrong size set every
    //  written bit and clear the rest. Whether the map knows what was
    //  written: it was started at power-on or has seen a power cycle since.
    bool   IsWritten         (HeatSpace space, Word index) const;
    bool   IsEdited          (HeatSpace space, Word index) const;
    void   GetKeptBits       (std::vector<uint64_t> & outBits) const;
    void   SetKeptBits       (const std::vector<uint64_t> & bits);
    bool   TakeKeptChange    ();
    bool   IsStartedClear    () const { return m_isStartedClear; }
    bool   IsTrackingWrites  () const { return IsOn() && m_isTracking; }

    //  A byte the debugger wrote, as a memory window's edit or a poke, at an
    //  address of the CPU's: marked as edited there and where a write of it
    //  lands now.
    void   NoteHostWrite     (Word address);

    //  The kept bits a map starts with: every written bit set, or every one
    //  clear, and no byte edited or fetched as an opcode.
    static void  MakeKeptBits (bool isWritten, std::vector<uint64_t> & outBits);

    //  The break on a read before written: while armed, the first such read
    //  outside the ranges left out is held for the debugger to stop on.
    void   SetUnwrittenBreak     (bool isArmed) { m_isBreakArmed = isArmed; }
    bool   IsUnwrittenBreakArmed () const       { return m_isBreakArmed; }
    bool   TryGetUnwrittenStop   (HeatUnwrittenRead & outRead) const;
    void   ClearUnwrittenStop    ()             { m_unwrittenStop.reset(); }

    //  The CPU addresses whose reads before written neither stop the
    //  machine nor count in the status, as first and last of each span.
    void   SetUnwrittenIgnore    (const std::vector<std::pair<Word, Word>> & spans);
    bool   IsUnwrittenIgnored    (Word address) const;

    //  The reads before written counted in the CPU's space, outside the
    //  ranges left out.
    HeatUnwrittenStatus  GetUnwrittenStatus () const;

    // IWatchSink
    void   OnWatchedAccess (Word                  address,
                            Byte                  value,
                            BusAccess             access,
                            std::optional<Byte>   previous) override;
    void   OnBeforeWrite   (Word address) override;
    void   OnPowerCycle    () override;

    // IFetchSink
    void   OnFetch (Word pc, Byte opcode) override;

    static size_t  GetIndex     (HeatKind kind, Word address)                     { return GetIndex (HeatSpace::Cpu, kind, address); }
    static size_t  GetIndex     (HeatSpace space, HeatKind kind, Word address)    { return (size_t) space * kSpaceEntryCount + (size_t) kind * kAddressCount + address; }
    static size_t  GetLastIndex (HeatSpace space, bool isWrite, Word address)     { return ((size_t) space * 2 + (isWrite ? 0 : 1)) * kAddressCount + address; }

private:
    //  An access counted where the CPU addressed it and where it landed.
    struct Landing
    {
        size_t     cpu     = 0;
        size_t     bank    = 0;
        bool       hasBank = false;
        HeatSpace  space   = HeatSpace::Cpu;
        Word       index   = 0;
    };

    static double  MakeFadePerFrame (double seconds);
    double         GetRatePerHeat   () const { return (1.0 - m_fadePerFrame) * kFramesPerSecond; }
    bool           IsCounting       () const { return m_position == nullptr || *m_position >= m_countFrom; }
    void           FindMostTotal    ();
    Landing        Count            (HeatKind kind, Word address, bool isWrite);
    void           CountAlso        (HeatKind kind, Word address, const Landing & landing);
    void           UncountAlso      (HeatKind kind, Word address, const Landing & landing);
    void           RecordLast       (bool isWrite, Word address, const Landing & landing);
    void           OnWrite          (Word address, Byte value);
    void           OnRead           (Word address, Byte value);

    //  The written bit of a landing, and whether the landing has one; and
    //  the edited bit of an address of a space, and whether it has one.
    static bool    TryGetWrittenBit (const Landing & landing, size_t & outBit);
    static bool    TryGetEditedBit  (HeatSpace space, Word index, size_t & outBit);

    static bool    IsBitSet         (const std::vector<uint64_t> & bits, size_t bit) { return (bits[bit / kBitsPerWord] & (1ull << (bit % kBitsPerWord))) != 0; }
    static void    SetBit           (std::vector<uint64_t> & bits, size_t bit)       { bits[bit / kBitsPerWord] |= 1ull << (bit % kBitsPerWord); }
    void           MarkOpcode       (Word pc, const Landing & landing);

    //  One more access at an entry, which then has something to fold; and
    //  the list of such entries emptied, once nothing on it has any.
    void           Bump             (size_t entry);
    void           ClearListed      ();

    const Microcode                    * m_instructionSet = nullptr;
    const uint64_t                     * m_position       = nullptr;
    const uint64_t                     * m_cycles         = nullptr;
    uint64_t                             m_countFrom      = 0;
    uint64_t                             m_lastFrom       = 0;
    std::vector<uint32_t>                m_counts;
    std::vector<float>                   m_heat;
    std::vector<int64_t>                 m_totals;
    std::vector<HeatLastAccess>          m_last;
    std::vector<Byte>                    m_opcodes;

    //  The entries with heat or counts not yet folded, the only ones a fold
    //  has anything to do for, and a flag per entry for whether it is listed.
    //  An idle machine touches a few hundred of the 1.3 million.
    std::vector<uint32_t>                m_listed;
    std::vector<Byte>                    m_isListed;

    std::array<uint64_t, kSpaceCount>    m_mostTotal      = {};
    uint64_t                             m_foldedAt       = 0;
    double                               m_fadeSeconds    = HeatMapOptions::kDefaultFadeSeconds;
    double                               m_fadePerFrame   = MakeFadePerFrame (HeatMapOptions::kDefaultFadeSeconds);
    HeatBankMap                          m_bankMap;

    //  The last read the bus reported, which is the opcode when a fetch is
    //  reported next, with the last reads it replaced, and the operand bytes
    //  still to come for that opcode.
    std::optional<Word>                  m_lastRead;
    Landing                              m_lastReadLanding;
    HeatLastAccess                       m_lastReadWasCpu;
    HeatLastAccess                       m_lastReadWasBank;
    Word                                 m_nextOperand    = 0;
    int                                  m_operandsLeft   = 0;

    //  The instruction running: the one whose fetch came last.
    std::optional<HeatLastAccess>        m_instruction;

    //  Which RAM has been written, main's bits then aux's, and which bytes
    //  the debugger edited; whether the map started with the written bits
    //  clear, and whether it knows them now; whether any kept bit, opcode
    //  marks included, changed since they were last taken.
    std::vector<uint64_t>                m_written;
    std::vector<uint64_t>                m_edited;
    bool                                 m_isStartedClear = false;
    bool                                 m_isTracking     = false;
    bool                                 m_isKeptChanged  = false;

    //  Whether the last read was a read before written, which the next
    //  fetch takes back with the read when it was the opcode, and the stop
    //  it raised, if it raised one.
    bool                                 m_isLastReadUnwritten = false;
    bool                                 m_isLastReadStop      = false;

    //  The byte the write the CPU is about to make replaces, and where.
    std::optional<Byte>                  m_previous;
    Word                                 m_previousAt = 0;

    //  The break on a read before written, the read it is holding, and the
    //  CPU addresses it leaves out, a bit each.
    bool                                 m_isBreakArmed = false;
    std::optional<HeatUnwrittenRead>     m_unwrittenStop;
    std::vector<uint64_t>                m_ignored      = std::vector<uint64_t> (kAddressWords, 0);
    size_t                               m_ignoredSpans = 0;
};
