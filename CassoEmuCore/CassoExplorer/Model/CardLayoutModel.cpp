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
        text = std::format (L"{} require{} CFFA 2.0 setup, and older adapters don't read {}. ", FormatRange (5, position, true), position == 5 ? L"s" : L"", position == 5 ? L"it" : L"them");
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
                             n <= 4 ? std::wstring() : n == 5 ? std::wstring (L"Reads volume 5 as a 1 GB drive. Erasing that drive destroys the volume")
                                                     : std::format (L"Reads volume 5 as a 1 GB drive and doesn't read {}. Erasing that drive destroys {}", FormatRange (6, n, false), FormatRange (5, n, false)) });
    outRows.push_back (Row { L"CFFA 2.0, 65C02 firmware",    n <= 4 ? Support::Works : Support::Step,
                             n <= 4 ? std::wstring() : std::format (L"Set its partition count to {} in its configuration menu. The factory setting is 4", n) });
    outRows.push_back (Row { L"CFFA 2.0, 6502 firmware",     n <= 2 ? Support::Works : Support::Step,  n <= 2 ? L"" : L"Reads volumes 1 to 2 only, or 1 to 4 in slot 5" });
    outRows.push_back (Row { L"CFFA3000, raw CF mode",       Support::Unknown, L"Not confirmed from its manual" });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::FormatRange
//
//  "volume 5" for a range of one, and "volumes 5 to 8" otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CardLayoutModel::FormatRange (int first, int last, bool capitalized)
{
    std::wstring  text = first == last ? std::format (L"volume {}", first) : std::format (L"volumes {} to {}", first, last);



    if (capitalized)
    {
        text[0] = L'V';
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::GetAdapterLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CardLayoutModel::GetAdapterLabel (Adapter adapter)
{
    switch (adapter)
    {
        case Adapter::Cffa1ProDos:      return L"CFFA 1.x, ProDOS 8 only";
        case Adapter::Cffa1Gsos:        return L"CFFA 1.x, with GS/OS";
        case Adapter::Cffa12EightDrive: return L"CFFA 1.2 with the 8-drive firmware";
        case Adapter::Cffa2Enhanced:    return L"CFFA 2.0 on an enhanced IIe or a IIgs";
        case Adapter::Cffa2Original:    return L"CFFA 2.0 on a II, II+ or unenhanced IIe";
        case Adapter::Cffa3000:         return L"CFFA3000";
        default:                        return L"I'm not sure";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::GetRecommendedPosition
//
////////////////////////////////////////////////////////////////////////////////

int CardLayoutModel::GetRecommendedPosition (Adapter adapter) const
{
    int  most = 0;



    switch (adapter)
    {
        case Adapter::Cffa12EightDrive: most = 8;                break;
        case Adapter::Cffa2Enhanced:    most = kMaxVolumes;      break;
        case Adapter::Cffa2Original:    most = 2;                break;
        default:                        most = kUniversalVolumes; break;
    }

    if (adapter == Adapter::Cffa3000 && m_canFat32)
    {
        return m_volumeLimit + 1;
    }

    return std::clamp (most, 1, std::max (m_volumeLimit, 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::GetChoices
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutModel::GetChoices (Adapter adapter, std::vector<int> & outPositions) const
{
    outPositions.clear();

    if (adapter == Adapter::Cffa3000 && m_canFat32)
    {
        outPositions.push_back (m_volumeLimit + 1);
    }

    for (int count = 1; count <= m_volumeLimit; count++)
    {
        outPositions.push_back (count);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::GetRows
//
//  The rows of GetRows (position, outRows) that are about the adapter.
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutModel::GetRows (int position, Adapter adapter, std::vector<Row> & outRows) const
{
    std::vector<Row>  all;
    std::vector<int>  selectedRows;



    GetRows (position, all);

    outRows.clear();

    if (adapter == Adapter::Unsure)
    {
        outRows = std::move (all);
        return;
    }

    if (IsFat32 (position))
    {
        selectedRows = adapter == Adapter::Cffa3000 ? std::vector<int> { 2 } : adapter == Adapter::Cffa2Enhanced || adapter == Adapter::Cffa2Original ? std::vector<int> { 1 } : std::vector<int> { 0 };
    }
    else
    {
        switch (adapter)
        {
            case Adapter::Cffa1ProDos:      selectedRows = { 0 };    break;
            case Adapter::Cffa1Gsos:        selectedRows = { 0, 2 }; break;
            case Adapter::Cffa12EightDrive: selectedRows = { 1, 2 }; break;
            case Adapter::Cffa2Enhanced:    selectedRows = { 3 };    break;
            case Adapter::Cffa2Original:    selectedRows = { 4 };    break;
            default:                        selectedRows = { 5 };    break;
        }
    }

    for (int index : selectedRows)
    {
        if ((size_t) index < all.size())
        {
            outRows.push_back (all[(size_t) index]);
        }
    }
}