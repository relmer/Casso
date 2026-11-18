#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IOpcodeWatcher
//
//  Gets each fetch by a Cpu of an opcode it watches, and the fetch after each
//  one, before the instruction runs: the registers are the ones it starts
//  from. An interrupt the CPU takes reads as opcode $00, BRK's, which is what
//  the 6502 forces into its instruction register to take one.
//
////////////////////////////////////////////////////////////////////////////////

class IOpcodeWatcher
{
public:
    virtual ~IOpcodeWatcher() = default;

    virtual void  OnWatchedFetch (Word pc, Byte sp, Byte opcode) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IFetchSink
//
//  Gets every opcode a Cpu fetches, before the instruction runs, with the
//  address it came from. Unlike IOpcodeWatcher it takes no table and gets
//  every opcode; an interrupt taken in place of an instruction is no fetch
//  and is not reported.
//
////////////////////////////////////////////////////////////////////////////////

class IFetchSink
{
public:
    virtual ~IFetchSink() = default;

    virtual void  OnFetch (Word pc, Byte opcode) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CpuFetchObservers
//
//  What a CPU's opcode fetches go to: the opcode watch, the fetch sink and
//  the trace. IsObserved is the one test the fetch path makes per
//  instruction; it is false while none of them is on.
//
////////////////////////////////////////////////////////////////////////////////

class CpuFetchObservers
{
public:
    static constexpr Byte  kInterruptOpcode = 0x00;

    // The opcodes, a 256-entry table, whose fetches the watcher gets, with
    // the fetch after each; null for none. The table is read in place.
    void  SetOpcodeWatch (const bool * opcodes, IOpcodeWatcher * watcher);

    // Where every opcode fetch goes; null for nowhere.
    void  SetFetchSink (IFetchSink * sink) { m_fetchSink = sink; Update(); }

    // Set by the trace as it turns on and off.
    void  SetTraceOn (bool isOn) { m_isTraceOn = isOn; Update(); }

    bool  IsObserved() const { return m_isObserved; }

    // Forced inline: these run once per instruction while a fetch is
    // observed, before the trace and after it.
    __forceinline void  ObserveWatch (Byte opcode, Word pc, Byte sp)
    {
        if (m_watchFetch[opcode])
        {
            ReportWatchedFetch (opcode, pc, sp);
        }
    }

    __forceinline void  ObserveSink (Byte opcode, Word pc)
    {
        if (m_fetchSink != nullptr)
        {
            m_fetchSink->OnFetch (pc, opcode);
        }
    }

    // An interrupt about to be taken in place of the instruction at pc.
    void  ObserveInterrupt (Word pc, Byte sp)
    {
        if (m_isObserved && m_watchFetch[kInterruptOpcode])
        {
            ReportWatchedFetch (kInterruptOpcode, pc, sp);
        }
    }

private:
    using OpcodeTable = std::array<bool, 0x100>;

    static constexpr OpcodeTable  s_kNoOpcodes  = {};
    static constexpr OpcodeTable  s_kAllOpcodes = [] { OpcodeTable all {}; all.fill (true); return all; }();

    void  Update             () { m_isObserved = m_isTraceOn || m_watcher != nullptr || m_fetchSink != nullptr; }
    void  ReportWatchedFetch (Byte opcode, Word pc, Byte sp);

    bool                m_isObserved   = false;

    // The watcher's table, and the one the next fetch is looked up in: the
    // watcher's, or every opcode for the fetch after a watched one.
    const bool        * m_watchOpcodes = nullptr;
    const bool        * m_watchFetch   = s_kNoOpcodes.data();
    IOpcodeWatcher    * m_watcher      = nullptr;
    IFetchSink        * m_fetchSink    = nullptr;
    bool                m_isTraceOn    = false;
};
