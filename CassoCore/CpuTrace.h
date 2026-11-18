#pragma once

#include "Pch.h"

class CpuFetchObservers;
class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace
//
//  A circular trace of the instructions a CPU ran, for post-mortem
//  debugging and the debugger's HISTORY. Turned on at run time and
//  allocated only then, so it costs one predicted branch per step while
//  off, in every build config (the --trace switch). In Debug a small
//  look-back ring is on from construction, so the illegal-opcode dump works
//  without --trace.
//
//  The trace's on state is one of the CPU's fetch observers, so turning it
//  on or off sets the observers' flag the fetch path tests.
//
////////////////////////////////////////////////////////////////////////////////

class CpuTrace
{
public:
    //  One instruction, before it ran. The access fields have the
    //  instruction's last data access, filled by RecordAccess while a bus
    //  reports every access; hasAccess is false otherwise.
    struct Entry
    {
        uint64_t  cycles;         // the cycle counter before the instruction
        Word      pc;
        Byte      opcode;
        Byte      op1;            // operand bytes at PC+1 and PC+2, read
        Byte      op2;            // through the bank the CPU executes from
        Byte      a;
        Byte      x;
        Byte      y;
        Byte      sp;
        Byte      p;
        Byte      intr;           // kIntr*: this instruction is a handler entry
        bool      hasAccess;
        bool      accessIsWrite;
        Byte      accessData;
        Word      accessAddress;
    };

    // Interrupt-dispatch tags. The dispatch path marks the pending kind
    // before the vector is taken; the next push moves it onto that entry
    // (the handler's first instruction). The dump flags interrupt entries and
    // summarizes the rate: an interrupt storm (a stuck IRQ line, an
    // unacknowledged source, an ISR that never returns) is otherwise
    // invisible in a flat instruction trace.
    static constexpr Byte    kIntrNone = 0;
    static constexpr Byte    kIntrIrq  = 1;
    static constexpr Byte    kIntrNmi  = 2;

    static constexpr size_t  kDefaultLookback = 256;

    CpuTrace (CpuFetchObservers & observers, const Microcode * instructionSet);

    CpuTrace (const CpuTrace &)             = delete;
    CpuTrace & operator= (const CpuTrace &) = delete;

    // Turns tracing on with a ring of capacity entries, emptied; 0 turns it
    // off and frees the ring.
    void      Enable      (size_t capacity);
    bool      IsEnabled   () const { return m_isEnabled; }
    uint64_t  GetCount    () const { return m_count; }
    size_t    GetCapacity () const { return m_capacity; }

    // Stops recording and keeps what the ring has.
    void      Stop        ();

    // Empties the ring and starts tracing again into the ring it has, with
    // no new allocation; nothing when there is no ring.
    void      Restart     ();

    // Where an entry's cycle count is read: the counter, plus the base that
    // puts it on history's clock. Null for none.
    void      SetCycleSources (const uint64_t * cycles, const uint64_t * cycleBase) { m_cycles = cycles; m_cycleBase = cycleBase; }

    // Marked from the interrupt-dispatch path just before the vector is
    // taken. A no-op while tracing is off, the common case.
    void      MarkInterrupt (Byte kind) { if (m_isEnabled) m_pendingIntr = kind; }

    // The next entry, stamped with the cycle count and any pending
    // interrupt, for the caller to fill with the instruction about to run.
    // Inline: it runs for every instruction while tracing.
    Entry  &  PushNext()
    {
        Entry  & e = m_entries[m_head];



        e.cycles    = ((m_cycles != nullptr) ? *m_cycles : 0) + ((m_cycleBase != nullptr) ? *m_cycleBase : 0);
        e.hasAccess = false;
        e.intr      = m_pendingIntr;

        m_pendingIntr = kIntrNone;

        // A compare rather than %: a 64-bit divide was the costliest part.
        m_head++;

        if (m_head == m_capacity)
        {
            m_head = 0;
        }

        m_count++;

        return e;
    }

    // Sets the newest entry's last bus access. pc is the address of the
    // next instruction.
    void      RecordAccess (Word address, Byte data, bool isWrite, Word pc);

    // The entries the ring has, oldest first.
    size_t    GetSize     () const;
    bool      TryGetEntry (size_t index, Entry & entry) const;

    // Writes the ring to a text file, oldest first. onProgress (may be
    // empty) is called periodically with (entriesWritten, totalEntries) so a
    // UI can show progress; CassoCore stays UI-free. preamble is written
    // under the title line as given.
    HRESULT   DumpToFile (const std::wstring                              & path,
                          const std::string                               & preamble,
                          const std::function<void (uint64_t, uint64_t)>  & onProgress) const;

    // The newest entries, newest first, to the debugger output, after an
    // illegal opcode.
    void      DumpRecent (Byte faultOpcode, Word faultPC) const;

private:
    const char  * GetName (Byte opcode) const;

    CpuFetchObservers      & m_observers;
    const Microcode        * m_instructionSet = nullptr;
    std::vector<Entry>       m_entries;                     // sized by Enable
    size_t                   m_capacity       = 0;
    size_t                   m_head           = 0;          // next slot to write
    uint64_t                 m_count          = 0;          // total entries pushed
    bool                     m_isEnabled      = false;
    Byte                     m_pendingIntr    = kIntrNone;
    const uint64_t         * m_cycles         = nullptr;
    const uint64_t         * m_cycleBase      = nullptr;    // added to m_cycles, so a trace counts on history's clock
};
