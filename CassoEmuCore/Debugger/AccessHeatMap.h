#pragma once

#include "Cpu.h"
#include "Core/IWatchSink.h"





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
//  How often each of the 64 KB addresses was executed, read and written over
//  the last few video frames. The CPU reports every read and write it makes
//  and every opcode it fetches; an instruction's bytes count as executed, not
//  read.
//
//  Accesses are counted as they come and folded into the heat on Fold, which
//  first fades what was there by the frames that went by since the last fold,
//  so an address the program stops touching cools off within about a second
//  of machine time.
//
//  The tables are allocated by Start and freed by Stop; a map that is off
//  holds no memory and ignores whatever it is told.
//
////////////////////////////////////////////////////////////////////////////////

class AccessHeatMap : public IWatchSink, public IFetchSink
{
public:
    static constexpr size_t    kAddressCount   = 0x10000;
    static constexpr size_t    kKindCount      = 3;
    static constexpr uint64_t  kCyclesPerFrame = 17030;
    static constexpr float     kFadePerFrame   = 0.8f;      // what one frame leaves of the heat
    static constexpr float     kColdHeat       = 0.01f;     // below this an address is cold again
    static constexpr float     kHottestHeat    = 4096.0f;   // the heat shown as level 255

    void   Start (const Microcode * instructionSet, uint64_t cycle);
    void   Stop  ();
    bool   IsOn  () const { return !m_heat.empty(); }

    //  The counts since the last fold, faded heat plus counts after it.
    void   Fold  (uint64_t cycle);

    float  GetHeat  (HeatKind kind, Word address) const;
    void   GetLevels (HeatKind kind, std::vector<Byte> & levels) const;

    //  0 for cold, 1 to 255 rising with the logarithm of the heat.
    static Byte  HeatToLevel (float heat);

    // IWatchSink
    void   OnWatchedAccess (Word                  address,
                            Byte                  value,
                            BusAccess             access,
                            std::optional<Byte>   previous) override;

    // IFetchSink
    void   OnFetch (Word pc, Byte opcode) override;

private:
    static size_t  GetIndex (HeatKind kind, Word address) { return (size_t) kind * kAddressCount + address; }

    const Microcode        * m_instructionSet = nullptr;
    std::vector<uint32_t>    m_counts;
    std::vector<float>       m_heat;
    uint64_t                 m_foldedAt       = 0;

    //  The last read the bus reported, which is the opcode when a fetch is
    //  reported next, and the operand bytes still to come for that opcode.
    std::optional<Word>      m_lastRead;
    Word                     m_nextOperand    = 0;
    int                      m_operandsLeft   = 0;
};
