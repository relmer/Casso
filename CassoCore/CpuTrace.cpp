#include "Pch.h"

#include "CpuTrace.h"
#include "CpuFetchObservers.h"
#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::CpuTrace
//
////////////////////////////////////////////////////////////////////////////////

CpuTrace::CpuTrace (CpuFetchObservers & observers, const Microcode * instructionSet) :
    m_observers      (observers),
    m_instructionSet (instructionSet)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::Enable
//
//  Allocates the ring, or frees it with a capacity of 0, and empties it.
//
////////////////////////////////////////////////////////////////////////////////

void CpuTrace::Enable (size_t capacity)
{
    m_head        = 0;
    m_count       = 0;
    m_pendingIntr = kIntrNone;

    if (capacity == 0)
    {
        m_isEnabled = false;
        m_capacity  = 0;
        m_entries.clear();
        m_entries.shrink_to_fit();
    }
    else
    {
        m_capacity  = capacity;
        m_entries.assign (capacity, Entry {});
        m_isEnabled = true;
    }

    m_observers.SetTraceOn (m_isEnabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::Stop
//
////////////////////////////////////////////////////////////////////////////////

void CpuTrace::Stop()
{
    m_isEnabled   = false;
    m_pendingIntr = kIntrNone;

    m_observers.SetTraceOn (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::Restart
//
////////////////////////////////////////////////////////////////////////////////

void CpuTrace::Restart()
{
    m_head        = 0;
    m_count       = 0;
    m_pendingIntr = kIntrNone;
    m_isEnabled   = m_capacity != 0;

    m_observers.SetTraceOn (m_isEnabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::GetSize
//
//  Every entry pushed until the ring wraps, then its capacity.
//
////////////////////////////////////////////////////////////////////////////////

size_t CpuTrace::GetSize() const
{
    return (m_count < (uint64_t) m_capacity) ? (size_t) m_count : m_capacity;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::TryGetEntry
//
//  The entry `index` places from the oldest the ring has.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuTrace::TryGetEntry (size_t index, Entry & entry) const
{
    size_t  size  = GetSize();
    size_t  start = (m_count > (uint64_t) m_capacity) ? m_head : 0;



    if (index >= size)
    {
        return false;
    }

    entry = m_entries[(start + index) % m_capacity];
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::RecordAccess
//
//  A bus access, stored on the newest entry as its last access. Only the
//  instruction's data accesses count, so three kinds are left out:
//
//    - the reads of the instruction's bytes, at pc to pc+2;
//    - the next instruction's opcode fetch, which reads at pc before that
//      instruction's entry exists;
//    - an interrupt's stack writes and vector reads, which happen while a
//      tag is pending and belong to no instruction.
//
//  A data read that falls on those addresses is rare (code reading its
//  bytes) and is left out with them.
//
////////////////////////////////////////////////////////////////////////////////

void CpuTrace::RecordAccess (Word address, Byte data, bool isWrite, Word pc)
{
    static constexpr Word  kInstructionSpan = 3;
    Entry                * e                = nullptr;



    if (!m_isEnabled || m_count == 0 || m_pendingIntr != kIntrNone)
    {
        return;
    }

    e = &m_entries[(m_head + m_capacity - 1) % m_capacity];

    if (!isWrite && (address == pc || (Word) (address - e->pc) < kInstructionSpan))
    {
        return;
    }

    e->hasAccess     = true;
    e->accessIsWrite = isWrite;
    e->accessData    = data;
    e->accessAddress = address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::GetName
//
////////////////////////////////////////////////////////////////////////////////

const char * CpuTrace::GetName (Byte opcode) const
{
    const char  * name = m_instructionSet[opcode].instructionName;



    return (name != nullptr) ? name : "???";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::DumpRecent
//
//  Dumps the ring buffer newest-first via DEBUGMSG. Format mirrors
//  conventional 6502 disassemblers so the output is easy to skim:
//
//    [-N]  PC=$XXXX  op=$XX (NAME)   A=$XX X=$XX Y=$XX SP=$XX P=$XX
//
//  Pre-faces with a banner so the dump is easy to spot in DbgView.
//
////////////////////////////////////////////////////////////////////////////////

void CpuTrace::DumpRecent (Byte faultOpcode, Word faultPC) const
{
    size_t  i = 0;



    // Short newest-first look-back to stderr -- enough to see the
    // JMP/JSR/RTS chain that landed PC on the bad byte. The full ring
    // (which may be millions of entries under --trace) is written
    // separately by DumpToFile, so cap this quick dump.
    static constexpr size_t  kMaxLookback = 256;

    size_t  total   = (m_count < (uint64_t) m_capacity) ? (size_t) m_count
                                                                  : m_capacity;

    if (total > kMaxLookback)
    {
        total = kMaxLookback;
    }

    DEBUGMSG (L"\n");
    DEBUGMSG (L"[Casso] === CPU illegal-opcode fault ===\n");
    DEBUGMSG (L"[Casso] Fault: opcode $%02X at PC=$%04X\n",
              faultOpcode, faultPC);
    DEBUGMSG (L"[Casso] Trace (newest-first, %zu entries):\n", total);

    for (i = 0; i < total; i++)
    {
        // head currently points one PAST the most recent push, so the
        // newest entry is (head - 1) mod size. Step backward from there.
        const Entry &       e        = m_entries[(m_head + m_capacity - 1 - i) % m_capacity];
        const char *        opName   = GetName (e.opcode);

        DEBUGMSG (L"[Casso] [-%03zu] PC=$%04X  op=$%02X (%hs)  "
                  L"A=$%02X X=$%02X Y=$%02X SP=$%02X P=$%02X\n",
                  i, (unsigned) e.pc, (unsigned) e.opcode, opName,
                  (unsigned) e.a, (unsigned) e.x, (unsigned) e.y,
                  (unsigned) e.sp, (unsigned) e.p);
    }

    DEBUGMSG (L"[Casso] === end trace ===\n");
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuTrace::DumpToFile
//
//  Write the recorded ring to `path` as text, oldest-first (chronological).
//  One instruction per line:
//
//    00000001  PC=$XXXX  op=$XX OPS=$XX $XX (NAME)  A=$XX X=$XX Y=$XX SP=$XX P=$XX
//
//  `preamble` follows the title line verbatim; the emulator puts the power-on
//  seed there. `onProgress` (if set) is called every kProgressStride entries and once
//  at completion with (entriesWritten, totalEntries). CassoCore owns no UI;
//  the caller drives any progress dialog.
//
//  The user asks for this dump explicitly, so a failure has to say which one
//  it was: E_INVALIDARG for an unopenable path, E_UNEXPECTED for a trace ring
//  that was never allocated (--trace not on), E_FAIL for a write that started
//  and then failed. A bool collapsed all three into "nothing happened".
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpuTrace::DumpToFile (
    const std::wstring                              & path,
    const std::string                               & preamble,
    const std::function<void (uint64_t, uint64_t)>  & onProgress) const
{
    static constexpr uint64_t  kProgressStride = 100000;
    size_t                     start           = 0;
    bool                       isOpen          = false;
    bool                       wrote           = false;
    char                       line[160];



    HRESULT        hr    = S_OK;
    std::ofstream  out (path, std::ios::binary | std::ios::trunc);
    uint64_t       total = (m_count < (uint64_t) m_capacity) ? m_count
                                                                       : (uint64_t) m_capacity;

    isOpen = out.is_open();

    CBREx (isOpen,               E_INVALIDARG);
    CBREx (m_capacity != 0, E_UNEXPECTED);

    {
        uint64_t  irqCount = 0;
        uint64_t  nmiCount = 0;

        // When the ring has wrapped, the oldest surviving entry sits at the
        // current head; otherwise it starts at index 0.
        start = (m_count > (uint64_t) m_capacity) ? m_head : 0;

        out << "Casso CPU execution trace -- " << total << " instructions (oldest first)\n";
        out << preamble;

        // Pre-scan for the interrupt-rate summary. An interrupt storm (stuck
        // IRQ line, unacknowledged source, an ISR that never returns) is
        // otherwise invisible in a flat instruction trace, so surface the rate
        // up top. Cheap -- one pass reading a single byte per entry, dwarfed by
        // the write below.

        for (uint64_t i = 0; i < total; i++)
        {
            Byte  kind = m_entries[(start + (size_t) i) % m_capacity].intr;

            if      (kind == kIntrIrq) { ++irqCount; }
            else if (kind == kIntrNmi) { ++nmiCount; }
        }

        {
            char  hdr[128];
            int   hn = (irqCount > 0)
                     ? std::snprintf (hdr, sizeof (hdr),
                           "interrupts: %llu IRQ, %llu NMI  (1 IRQ per %llu instrs, ~%.1f/frame)\n",
                           (unsigned long long) irqCount, (unsigned long long) nmiCount,
                           (unsigned long long) (total / irqCount),
                           (double) irqCount * 17030.0 / ((double) total * 3.0))
                     : std::snprintf (hdr, sizeof (hdr),
                           "interrupts: %llu IRQ, %llu NMI\n",
                           (unsigned long long) irqCount, (unsigned long long) nmiCount);

            if (hn > 0)
            {
                out.write (hdr, hn);
            }
        }

        for (uint64_t i = 0; i < total; i++)
        {
            size_t              index = (start + (size_t) i) % m_capacity;
            const Entry &       e     = m_entries[index];
            const char *        name  = GetName (e.opcode);
            const char *        mark  = (e.intr == kIntrIrq) ? "  [IRQ]"
                                      : (e.intr == kIntrNmi) ? "  [NMI]"
                                      :                             "";

            int  n = std::snprintf (line, sizeof (line),
                                    "%08llu  PC=$%04X  op=$%02X OPS=$%02X $%02X (%s)  "
                                    "A=$%02X X=$%02X Y=$%02X SP=$%02X P=$%02X%s\n",
                                    (unsigned long long) i,
                                    (unsigned) e.pc, (unsigned) e.opcode,
                                    (unsigned) e.op1, (unsigned) e.op2, name,
                                    (unsigned) e.a, (unsigned) e.x, (unsigned) e.y,
                                    (unsigned) e.sp, (unsigned) e.p, mark);
            if (n > 0)
            {
                out.write (line, n);
            }

            if (onProgress && (i % kProgressStride) == 0)
            {
                onProgress (i, total);
            }
        }

        out.flush();

        if (onProgress)
        {
            onProgress (total, total);
        }

        // Distinct from the open failure above: the file opened and we wrote
        // into it, then the stream went bad -- a full disk or a vanished share,
        // not a bad path. E_FAIL is the CBR default, so no -Ex here.
        wrote = out.good();

        CBR (wrote);
    }

Error:
    return hr;
}
