#include "Pch.h"

#include "Ui/Debugger/MemoryWindowViews.h"

#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/MemoryHandlers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowViews::BuildMemoryPanes
//
//  The memory pane and every memory window open.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowViews::BuildMemoryPanes (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    static constexpr uint32_t  kPaneDumpBytes = 64;
    MemoryData                 memory;



    //  The rows D would show, read without running D, so a bare D typed later
    //  still continues from the user's own last dump.
    memory = MemoryHandlers::MakeRows (session.GetTarget(), m_memoryAddress, (Word) std::min<uint32_t> ((uint32_t) m_memoryAddress + kPaneDumpBytes - 1, 0xFFFF));

    for (const MemoryRow & row : memory.rows)
    {
        DebuggerViewSnapshot::MemoryLine  line;



        line.address = row.address;
        line.region  = GetRegionLabel (row.region);

        for (const std::optional<Byte> & b : row.bytes)
        {
            //  An I/O byte is not read for display, because reading one can
            //  change the machine; it shows as "--" rather than a guess.
            line.bytes      += b.has_value() ? std::format ("{:02X} ", *b) : std::string ("-- ");
            line.characters += (b.has_value() && (*b & 0x7F) >= 0x20 && (*b & 0x7F) < 0x7F) ? (char) (*b & 0x7F) : '.';
        }

        if (!line.bytes.empty())
        {
            line.bytes.pop_back();
        }

        snapshot.memory.push_back (std::move (line));

        if ((int) snapshot.memory.size() >= kMemoryRows)
        {
            break;
        }
    }

    for (int id = 1; id <= kMaxMemoryWindows; id++)
    {
        std::optional<Word>  address = GetMemoryWindowAddress (id);

        if (address.has_value())
        {
            snapshot.memoryWindows.push_back (ReadMemoryWindow (session, id, *address));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowViews::OpenMemoryWindow
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowViews::OpenMemoryWindow (int id, Word address)
{
    if (id == 1)
    {
        m_memoryAddress = address;
    }
    else if (id >= 2 && id <= kMaxMemoryWindows)
    {
        m_extraWindows[(size_t) (id - 2)] = address;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowViews::CloseMemoryWindow
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowViews::CloseMemoryWindow (int id)
{
    if (id >= 2 && id <= kMaxMemoryWindows)
    {
        m_extraWindows[(size_t) (id - 2)].reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowViews::GetMemoryWindowAddress
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> MemoryWindowViews::GetMemoryWindowAddress (int id) const
{
    std::optional<Word>  address;



    if (id == 1)
    {
        address = m_memoryAddress;
    }
    else if (id >= 2 && id <= kMaxMemoryWindows)
    {
        address = m_extraWindows[(size_t) (id - 2)];
    }

    return address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowViews::ReadMemoryWindow
//
//  The rows D would show, read without running D, so the user's bare D still
//  continues from their own last dump; each row's region is given to every
//  byte in it, which is exact because rows start on eight-byte boundaries and
//  no region changes inside one.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot::MemoryWindow MemoryWindowViews::ReadMemoryWindow (DebugSession & session, int id, Word address)
{
    DebuggerViewSnapshot::MemoryWindow  window;
    Word                                first = (Word) (address & ~(kMemoryRowBytes - 1));
    Word                                last  = (Word) std::min<uint32_t> ((uint32_t) first + kMemoryWindowBytes - 1, 0xFFFF);
    MemoryData                          data  = MemoryHandlers::MakeRows (session.GetTarget(), first, last);



    window.id    = id;
    window.first = first;

    for (const MemoryRow & row : data.rows)
    {
        window.bytes.insert (window.bytes.end(), row.bytes.begin(), row.bytes.end());
        window.regions.insert (window.regions.end(), row.bytes.size(), row.region);
    }

    return window;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowViews::GetRegionLabel
//
////////////////////////////////////////////////////////////////////////////////

std::string MemoryWindowViews::GetRegionLabel (MemoryRegion region)
{
    switch (region)
    {
    case MemoryRegion::MainRam: return "RAM";
    case MemoryRegion::AuxRam:  return "AUX";
    case MemoryRegion::LcBank1: return "LC1";
    case MemoryRegion::LcBank2: return "LC2";
    case MemoryRegion::Rom:     return "ROM";
    case MemoryRegion::SlotRom: return "SLOT";
    case MemoryRegion::Io:      return "I/O";
    }

    return "?";
}
