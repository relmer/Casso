#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryEditModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::SetContents
//
////////////////////////////////////////////////////////////////////////////////

void MemoryEditModel::SetContents (Word first, std::vector<std::optional<Byte>> bytes, std::vector<MemoryRegion> regions)
{
    ByteChanges::Seen  seen;



    //  Compared with what the snapshots read, not with what an edit put on
    //  screen at once, so an edit shows in the changed color as a step does.
    for (size_t i = 0; i < bytes.size(); i++)
    {
        if (bytes[i].has_value())
        {
            seen.push_back ({ (Word) ((first + i) & 0xFFFF), *bytes[i] });
        }
    }

    m_changes.Update (m_isPaused, seen);

    if (m_keptBytes.empty())
    {
        m_keptBytes.resize   ((size_t) kAddressSpace);
        m_keptRegions.resize ((size_t) kAddressSpace, 0);
    }

    for (size_t i = 0; i < bytes.size() && i < regions.size(); i++)
    {
        size_t  address = (size_t) ((first + i) & 0xFFFF);

        m_keptBytes[address]   = bytes[i];
        m_keptRegions[address] = (uint8_t) ((uint8_t) regions[i] + 1);
    }

    m_first   = first;
    m_bytes   = std::move (bytes);
    m_regions = std::move (regions);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::TryGetKnown
//
//  The current read first, since an edit shows there at once; then whatever
//  an earlier read left.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryEditModel::TryGetKnown (Word address, std::optional<Byte> & outByte, MemoryRegion & outRegion) const
{
    size_t  index = 0;



    if (TryGetShown (address, index) && index < m_regions.size())
    {
        outByte   = m_bytes[index];
        outRegion = m_regions[index];
        return true;
    }

    if (address >= m_keptRegions.size() || m_keptRegions[address] == 0)
    {
        return false;
    }

    outByte   = m_keptBytes[address];
    outRegion = (MemoryRegion) (m_keptRegions[address] - 1);
    return true;
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
    std::optional<Byte>  value;
    MemoryRegion         region = MemoryRegion::MainRam;



    for (size_t i = 0; i < out.size(); i++)
    {
        value.reset();

        out[i] = TryGetKnown (GetAddressOf (offset + i), value, region) ? value.value_or (0) : 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::ReadMarks
//
//  I/O and ROM each get a mark, so a window can color the bytes an edit
//  cannot reach and the ones it patches rather than writes. A byte that
//  changed since the last read is marked so above either. Outside the current
//  read a byte keeps the mark the last read to reach it gave, so the marks
//  come with the values in the same paint; a byte no read has reached is
//  marked unread rather than drawn as plain RAM.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryEditModel::ReadMarks (uint64_t offset, std::span<uint8_t> out) const
{
    size_t               index  = 0;
    std::optional<Byte>  value;
    MemoryRegion         region = MemoryRegion::MainRam;



    for (size_t i = 0; i < out.size(); i++)
    {
        Word  address = GetAddressOf (offset + i);

        out[i] = kMarkNone;

        if (TryGetShown (address, index) && m_changes.IsChanged (address))
        {
            out[i] = kMarkChanged;
            continue;
        }

        if (!TryGetKnown (address, value, region))
        {
            out[i] = kMarkUnread;
            continue;
        }

        switch (region)
        {
        case MemoryRegion::Io:      out[i] = kMarkIo;  break;
        case MemoryRegion::Rom:
        case MemoryRegion::SlotRom: out[i] = kMarkRom; break;
        default:                                        break;
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

    edit.written.assign (bytes.begin(), bytes.end());
    SendPatch (edit.address, bytes);
    m_history.push_back (std::move (edit));
    m_redo.clear();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::TryGetByteTip
//
//  The byte's address, which the view's own offset column gives only for the
//  first byte of each row, then what its color means, as its mark says:
//  changed, I/O or ROM. Plain RAM has no color and no word.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryEditModel::TryGetByteTip (uint64_t offset, std::wstring & tip) const
{
    uint8_t  mark = kMarkNone;



    ReadMarks (offset, std::span<uint8_t> (&mark, 1));

    tip = std::format (L"${:04X}", GetAddressOf (offset));

    switch (mark)
    {
    case kMarkChanged: tip += L"  changed";      break;
    case kMarkIo:      tip += L"  I/O";          break;
    case kMarkRom:     tip += L"  ROM";          break;
    case kMarkUnread:  tip += L"  not read yet"; break;
    default:                                     break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::GetRegionKey
//
//  A slot's ROM in $C100-$C7FF is that slot's, by its page; slot ROM above
//  $C7FF is the card's expansion ROM.
//
////////////////////////////////////////////////////////////////////////////////

uint16_t MemoryEditModel::GetRegionKey (MemoryRegion region, Word address)
{
    constexpr Word  kFirstSlotPage = 0xC1;
    constexpr Word  kLastSlotPage  = 0xC7;
    constexpr Word  kSlotMask      = 0x07;
    Word            page           = (Word) (address >> 8);



    switch (region)
    {
    case MemoryRegion::Rom:     return kRegionRom;
    case MemoryRegion::Io:      return kRegionIo;
    case MemoryRegion::LcBank1: return kRegionLcBank1;
    case MemoryRegion::LcBank2: return kRegionLcBank2;
    case MemoryRegion::AuxRam:  return kRegionAux;
    case MemoryRegion::SlotRom:
        return (page >= kFirstSlotPage && page <= kLastSlotPage) ? (uint16_t) (kRegionSlotRom + (page & kSlotMask)) : kRegionExpansionRom;
    case MemoryRegion::MainRam: return 0;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::GetRegionLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryEditModel::GetRegionLabel (uint16_t key)
{
    constexpr uint16_t  kSlotCount = 7;



    if (key > kRegionSlotRom && key <= kRegionSlotRom + kSlotCount)
    {
        return std::format (L"Slot {} ROM", key - kRegionSlotRom);
    }

    switch (key)
    {
    case kRegionRom:          return L"ROM";
    case kRegionIo:           return L"I/O";
    case kRegionLcBank1:      return L"LC bank 1";
    case kRegionLcBank2:      return L"LC bank 2";
    case kRegionAux:          return L"Aux RAM";
    case kRegionExpansionRom: return L"Expansion ROM";
    default:                  break;
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::ReadRegions
//
//  The region of each byte as the last read to reach it gave it, so a window
//  scrolled past its read keeps the outlines it was shown there; a byte no
//  read has reached is in none.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryEditModel::ReadRegions (uint64_t offset, std::span<uint16_t> out) const
{
    std::optional<Byte>  value;
    MemoryRegion         region = MemoryRegion::MainRam;



    for (size_t i = 0; i < out.size(); i++)
    {
        Word  address = GetAddressOf (offset + i);

        out[i] = TryGetKnown (address, value, region) ? GetRegionKey (region, address) : (uint16_t) 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::TryGetRegionStyle
//
//  A slot's ROM and expansion ROM are outlined in ROM's color, as their bytes
//  are drawn.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryEditModel::TryGetRegionStyle (uint16_t region, uint32_t & outArgb, std::wstring & outLabel) const
{
    outLabel = GetRegionLabel (region);

    switch (region)
    {
    case kRegionIo:      outArgb = m_regionColors.io;      break;
    case kRegionLcBank1: outArgb = m_regionColors.lcBank1; break;
    case kRegionLcBank2: outArgb = m_regionColors.lcBank2; break;
    case kRegionAux:     outArgb = m_regionColors.aux;     break;
    default:             outArgb = m_regionColors.rom;     break;
    }

    return !outLabel.empty() && outArgb != 0;
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
    m_redo.push_back (std::move (edit));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::Redo
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryEditModel::Redo()
{
    Edit    edit;
    size_t  index = 0;



    if (m_redo.empty())
    {
        return false;
    }

    edit = std::move (m_redo.back());
    m_redo.pop_back();

    for (size_t i = 0; i < edit.written.size(); i++)
    {
        if (TryGetShown ((uint64_t) edit.address + i, index))
        {
            m_bytes[index] = edit.written[i];
        }
    }

    SendPatch (edit.address, edit.written);
    m_history.push_back (std::move (edit));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::SendPatch
//
////////////////////////////////////////////////////////////////////////////////

void MemoryEditModel::SendPatch (Word address, std::span<const Byte> bytes) const
{
    if (m_onPatch)
    {
        m_onPatch (address, bytes);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::GetUndoText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryEditModel::GetUndoText() const
{
    return m_history.empty() ? std::wstring() : ByteChanges::GetEditText (m_history.back().address, m_history.back().written.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel::GetRedoText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryEditModel::GetRedoText() const
{
    return m_redo.empty() ? std::wstring() : ByteChanges::GetEditText (m_redo.back().address, m_redo.back().written.size());
}
