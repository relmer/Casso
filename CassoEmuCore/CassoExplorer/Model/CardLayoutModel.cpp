#include "Pch.h"

#include "CassoExplorer/Model/CardLayoutModel.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::CardLayoutModel
//
////////////////////////////////////////////////////////////////////////////////

CardLayoutModel::CardLayoutModel (uint64_t bytes, int volumeLimit, bool canFat32) :
    m_bytes       (bytes),
    m_volumeLimit (std::clamp (volumeLimit, 0, kMaxVolumes)),
    m_canFat32    (canFat32)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::GetLayout
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CardLayoutModel::GetLayout (int position) const
{
    return IsFat32 (position) ? RawDeviceProtocol::kLayoutFat32 : (uint64_t) std::clamp (position, 1, std::max (m_volumeLimit, 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::GetSlotKind
//
////////////////////////////////////////////////////////////////////////////////

CardLayoutModel::Segment CardLayoutModel::GetSlotKind (int position, int slot) const
{
    if (IsFat32 (position))
    {
        return Segment::Fat32;
    }

    if (slot > position)
    {
        return Segment::Unused;
    }

    return slot <= kUniversalVolumes ? Segment::Universal : Segment::NeedsSetup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::GetUnavailableBytes
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CardLayoutModel::GetUnavailableBytes() const
{
    uint64_t  mapped = (uint64_t) m_volumeLimit * kSliceBytes;



    return m_bytes > mapped ? m_bytes - mapped : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::DescribePosition
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CardLayoutModel::DescribePosition (int position) const
{
    if (IsFat32 (position))
    {
        return L"One FAT32 partition, the whole card, for a CFFA3000's disk image files";
    }

    if (m_bytes <= kSliceBytes)
    {
        return L"One ProDOS volume of the whole card";
    }

    return std::format (L"{} ProDOS volume{} of 32 MB", position, position == 1 ? L"" : L"s");
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::DescribeSpace
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CardLayoutModel::DescribeSpace (int position) const
{
    uint64_t      used   = std::min (m_bytes, (uint64_t) position * kSliceBytes);
    uint64_t      unused = m_bytes - used;
    std::wstring  text;



    if (IsFat32 (position))
    {
        return L"The whole card is used. Only a CFFA3000 reads it.";
    }

    if (position > kUniversalVolumes)
    {
        text = std::format (L"Volumes 5 to {} require CFFA 2.0 setup, and older adapters don't read them. ", position);
    }

    if (unused == 0)
    {
        return text + L"The whole card is used.";
    }

    text += std::format (L"{} of the card is unused by the CFFA layout.", TextEncoding::NarrowToWide (PartitionAccess::FormatSize (unused)));

    if (m_canFat32)
    {
        text += L" FAT32 uses all of it with a CFFA3000.";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::GetRows
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutModel::GetRows (int position, std::vector<Row> & outRows) const
{
    int  n = position;



    outRows.clear();

    if (IsFat32 (position))
    {
        outRows.push_back (Row { L"CFFA 1.x",  Support::Fails, L"Reads only its fixed layout" });
        outRows.push_back (Row { L"CFFA 2.0",  Support::Fails, L"Reads only its fixed layout" });
        outRows.push_back (Row { L"CFFA3000",  Support::Works, L"Copy disk images to the card in Windows, then assign them in its menu" });
        return;
    }

    outRows.push_back (Row { L"CFFA 1.x, ProDOS 8",          n <= 4 ? Support::Works : Support::Step,  n <= 4 ? L"" : L"Reads volumes 1 to 4 only" });
    outRows.push_back (Row { L"CFFA 1.2 8-drive firmware",   n <= 8 ? Support::Works : Support::Step,  n <= 8 ? L"" : L"Reads volumes 1 to 8 only" });
    outRows.push_back (Row { L"CFFA 1.x, GS/OS driver",      n <= 4 ? Support::Works : Support::Fails,
                             n <= 4 ? std::wstring() : std::format (L"Reads volume 5 as a 1 GB drive and doesn't read 6 to {}. Erasing that drive destroys them", n) });
    outRows.push_back (Row { L"CFFA 2.0, 65C02 firmware",    n <= 4 ? Support::Works : Support::Step,
                             n <= 4 ? std::wstring() : std::format (L"Set its partition count to {} in its configuration menu. The factory setting is 4", n) });
    outRows.push_back (Row { L"CFFA 2.0, 6502 firmware",     n <= 2 ? Support::Works : Support::Step,  n <= 2 ? L"" : L"Reads volumes 1 to 2 only, or 1 to 4 in slot 5" });
    outRows.push_back (Row { L"CFFA3000, raw CF mode",       Support::Unknown, L"Not confirmed from its manual" });
}
