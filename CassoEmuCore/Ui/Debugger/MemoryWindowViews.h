#pragma once

#include "Ui/Debugger/DebuggerViewSnapshot.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowViews
//
//  The memory pane and the memory windows: where each starts, and their rows
//  in a snapshot.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryWindowViews
{
public:
    static constexpr int  kMemoryRows = 16;

    //  Memory windows: the first is always open, and up to three more. Each
    //  reads this many bytes from a sixteen-byte boundary, enough for the rows a
    //  window shows at its widest columns and some to scroll into.
    static constexpr int  kMaxMemoryWindows  = 4;
    static constexpr int  kMemoryWindowBytes = 4096;
    static constexpr int  kMemoryRowBytes    = 16;

    //  The memory pane starts at the zero page.
    void  SetMemoryAddress (Word address) { m_memoryAddress = address; }
    Word  GetMemoryAddress () const       { return m_memoryAddress; }

    //  Windows 2 to 4 open at an address or close; window 1 is the memory pane
    //  and moves with SetMemoryAddress. Other ids are ignored.
    void                 OpenMemoryWindow       (int id, Word address);
    void                 CloseMemoryWindow      (int id);
    std::optional<Word>  GetMemoryWindowAddress (int id) const;

    void  BuildMemoryPanes (DebugSession & session, DebuggerViewSnapshot & snapshot) const;

    static std::string  GetRegionLabel (MemoryRegion region);

private:
    static DebuggerViewSnapshot::MemoryWindow  ReadMemoryWindow (DebugSession & session, int id, Word address);

    Word                                                    m_memoryAddress = 0x0000;
    std::array<std::optional<Word>, kMaxMemoryWindows - 1>  m_extraWindows;
};
