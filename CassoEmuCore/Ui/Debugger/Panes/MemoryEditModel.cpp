#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryEditModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::SetContents
//
////////////////////////////////////////////////////////////////////////////////

void MemoryEditModel::SetContents (Word first, std::vector<std::optional<Byte>> bytes, std::vector<MemoryRegion> regions)
{
    size_t  previous = 0;



    m_changed.assign (bytes.size(), false);

    for (size_t i = 0; i < bytes.size(); i++)
    {
        if (bytes[i].has_value() && TryGetShown ((uint64_t) first + i, previous) && m_bytes[previous].has_value())
        {
            m_changed[i] = *m_bytes[previous] != *bytes[i];
        }
    }

    m_first   = first;
    m_bytes   = std::move (bytes);
    m_regions = std::move (regions);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::TryGetShown
//
//  Where address sits among the bytes this window was shown, if it does.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryEditModel::TryGetShown (uint64_t address, size_t & outIndex) const
{
    bool  shown = address >= m_first && address - m_first < m_bytes.size();



    outIndex = shown ? (size_t) (address - m_first) : 0;
    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::TryGetRegion
//
////////////////////////////////////////////////////////////////////////////////

std::optional<MemoryRegion> MemoryEditModel::TryGetRegion (Word address) const
{
    std::optional<MemoryRegion>  region;
    size_t                       index = 0;



    if (TryGetShown (address, index) && index < m_regions.size())
    {
        region = m_regions[index];
    }

    return region;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::ReadBytes
//
////////////////////////////////////////////////////////////////////////////////

void MemoryEditModel::ReadBytes (uint64_t offset, std::span<uint8_t> out) const
{
    size_t  index = 0;



    for (size_t i = 0; i < out.size(); i++)
    {
        out[i] = TryGetShown (GetAddressOf (offset + i), index) ? m_bytes[index].value_or (0) : 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::ReadMarks
//
//  I/O and ROM each get a mark, so a window can color the bytes an edit
//  cannot reach and the ones it patches rather than writes. A byte that
//  changed since the last read is marked so above either.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryEditModel::ReadMarks (uint64_t offset, std::span<uint8_t> out) const
{
    size_t  index = 0;



    for (size_t i = 0; i < out.size(); i++)
    {
        out[i] = kMarkNone;

        if (TryGetShown (GetAddressOf (offset + i), index) && index < m_changed.size() && m_changed[index])
        {
            out[i] = kMarkChanged;
        }
        else if (TryGetShown (GetAddressOf (offset + i), index) && index < m_regions.size())
        {
            switch (m_regions[index])
            {
            case MemoryRegion::Io:      out[i] = kMarkIo;  break;
            case MemoryRegion::Rom:
            case MemoryRegion::SlotRom: out[i] = kMarkRom; break;
            default:                                        break;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::WriteBytes
//
//  Every byte must have been shown and must not be I/O, or none is written.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryEditModel::WriteBytes (uint64_t offset, std::span<const uint8_t> bytes) const
{
    Edit    edit;
    size_t  index = 0;



    edit.address = GetAddressOf (offset);

    for (size_t i = 0; i < bytes.size(); i++)
    {
        if (!TryGetShown (GetAddressOf (offset + i), index) || !m_bytes[index].has_value() ||
            (index < m_regions.size() && m_regions[index] == MemoryRegion::Io))
        {
            return false;
        }

        edit.replaced.push_back (*m_bytes[index]);
    }

    for (size_t i = 0; i < bytes.size(); i++)
    {
        (void) TryGetShown (GetAddressOf (offset + i), index);
        m_bytes[index] = bytes[i];
    }

    m_history.push_back (std::move (edit));
    SendPatch (edit.address, bytes);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::Undo
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryEditModel::Undo()
{
    Edit    edit;
    size_t  index = 0;



    if (m_history.empty())
    {
        return false;
    }

    edit = std::move (m_history.back());
    m_history.pop_back();

    for (size_t i = 0; i < edit.replaced.size(); i++)
    {
        if (TryGetShown ((uint64_t) edit.address + i, index))
        {
            m_bytes[index] = edit.replaced[i];
        }
    }

    SendPatch (edit.address, edit.replaced);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::SendPatch
//
////////////////////////////////////////////////////////////////////////////////

void MemoryEditModel::SendPatch (Word address, std::span<const Byte> bytes) const
{
    std::string  line = std::format ("PATCH {:04X}", address);



    for (Byte value : bytes)
    {
        line += std::format (" {:02X}", value);
    }

    if (m_onCommand)
    {
        m_onCommand (line);
    }
}
