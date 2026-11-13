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
        return L"One FAT32 partition";
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
    uint64_t  used   = std::min (m_bytes, (uint64_t) position * kSliceBytes);
    uint64_t  unused = m_bytes - used;



    if (IsFat32 (position))
    {
        return std::format (L"Creates a single {} FAT32 partition.", FormatBytes (m_bytes - (uint64_t) kFat32StartBlocks * 512));
    }

    if (unused == 0)
    {
        return std::format (L"Uses the whole card for {}.", position == 1 ? L"one ProDOS volume" : L"CFFA ProDOS volumes");
    }

    return std::format (L"Uses {} for CFFA ProDOS volumes, leaving {} unused and inaccessible.", FormatBytes (used), FormatBytes (unused));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutModel::FormatBytes
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CardLayoutModel::FormatBytes (uint64_t bytes)
{
    return TextEncoding::NarrowToWide (PartitionAccess::FormatSize (bytes));
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
        outRows.push_back (Row { L"CFFA 1.x",           Support::Fails, L"Reads only its fixed layout" });
        outRows.push_back (Row { L"CFFA 2.0",           Support::Fails, L"Reads only its fixed layout" });
        outRows.push_back (Row { L"CFFA3000",           Support::Works, L"Copy disk images to the card in Casso Explorer, then assign them to drives in the CFFA3000's menu" });
        outRows.push_back (Row { L"A2Pico, A2retroNET", Support::Works, L"Copy disk images to the card in Casso Explorer, then set them as drives in A2retroNET drives" });
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
    outRows.push_back (Row { L"CFFA3000, raw CF mode",       n <= 4 ? Support::Works : Support::Step,
                             n <= 4 ? std::wstring() : std::format (L"Set # 32MB partitions to {} in Other Settings, Raw CF Card Settings. Its manual's example has 4", n) });
    outRows.push_back (Row { L"A2Pico, A2retroNET",          Support::Fails,                           L"Reads disk image files on a FAT or exFAT volume only" });
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
        case Adapter::A2Pico:           return L"A2Pico with the A2retroNET firmware";
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

    if ((adapter == Adapter::Cffa3000 || adapter == Adapter::A2Pico) && m_canFat32)
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

    if ((adapter == Adapter::Cffa3000 || adapter == Adapter::A2Pico) && m_canFat32)
    {
        outPositions.push_back (m_volumeLimit + 1);
    }

    for (int count = 1; adapter != Adapter::A2Pico && count <= m_volumeLimit; count++)
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
        switch (adapter)
        {
            case Adapter::Cffa3000:      selectedRows = { 2 }; break;
            case Adapter::A2Pico:        selectedRows = { 3 }; break;
            case Adapter::Cffa2Enhanced:
            case Adapter::Cffa2Original: selectedRows = { 1 }; break;
            default:                     selectedRows = { 0 }; break;
        }
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
            case Adapter::A2Pico:           selectedRows = { 6 };    break;
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