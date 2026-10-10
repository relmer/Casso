#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/SectorSource.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorSource::SectorSource
//
////////////////////////////////////////////////////////////////////////////////

SectorSource::SectorSource (const DiskAnalysis & analysis)
{
    int  track    = 0;
    int  slot     = -1;
    int  thirteen = 0;
    int  sixteen  = 0;



    for (track = 0; track < kTracks; track++)
    {
        int                    qt     = track * DiskImage::kQuarterTracksPerWholeTrack;
        const TrackAnalysis *  record = nullptr;

        slot   = analysis.entries[qt].slot;
        record = (analysis.entries[qt].content != QuarterTrackContent::Damaged && slot >= 0 && slot < static_cast<int> (analysis.tracks.size()))
                     ? analysis.tracks[slot].get() : nullptr;

        if (record == nullptr)
        {
            continue;
        }

        thirteen += (record->has13 && !record->has16) ? 1 : 0;
        sixteen  += record->has16 ? 1 : 0;

        //  The first field in passing order for each physical sector.
        for (const AnalyzedSector & sector : record->sectors)
        {
            Sector &  out = m_sectors[track][sector.sector & (kSectors - 1)];

            if (sector.kind != DiskFieldKind::Sixteen || sector.sector >= kSectors || out.result != SectorResult::Missing || (sector.isAddressCheck && !sector.isAddressGood))
            {
                continue;
            }

            if (sector.dataField >= 0)
            {
                out.bytes  = record->fields[sector.dataField].data.bytes.data();
                out.result = !sector.isDataCheck ? SectorResult::NotChecked : (sector.isDataGood ? SectorResult::Good : SectorResult::Bad);
            }
        }
    }

    m_isThirteenSector = thirteen > sixteen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorSource::GetPhysical
//
////////////////////////////////////////////////////////////////////////////////

const SectorSource::Sector & SectorSource::GetPhysical (int track, int physical) const
{
    return (track >= 0 && track < kTracks && physical >= 0 && physical < kSectors) ? m_sectors[track][physical] : m_missing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorSource::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

bool SectorSource::ReadBlock (int block, std::array<Byte, 2 * kBytes> & outBytes) const
{
    bool  isRead = block >= 0 && block < kBlocks;



    for (int half = 0; isRead && half < 2; half++)
    {
        const Sector &  sector = GetHalf (block, half);

        isRead = IsReadable (sector.result) && sector.bytes != nullptr;

        if (isRead)
        {
            memcpy (outBytes.data() + half * kBytes, sector.bytes, kBytes);
        }
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorSource::GetDos33Physical
//
//  The inverse of the analyzer's physical-to-DOS 3.3 table.
//
////////////////////////////////////////////////////////////////////////////////

int SectorSource::GetDos33Physical (int logical)
{
    int  physical = 0;



    for (int p = 0; p < kSectors; p++)
    {
        physical = (NibblizationLayer::GetDosFileIndexForPhysicalSector (p) == logical) ? p : physical;
    }

    return physical;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorSource::GetBlockPhysical
//
//  The physical sector holding a half of a ProDOS block, which Pascal shares.
//
////////////////////////////////////////////////////////////////////////////////

int SectorSource::GetBlockPhysical (int block, int half)
{
    int  wanted   = (block % kBlocksPerTrack) * 2 + half;
    int  physical = 0;



    for (int p = 0; p < kSectors; p++)
    {
        int  dos = NibblizationLayer::GetDosFileIndexForPhysicalSector (p);

        physical = (NibblizationLayer::GetPoFileIndexForDosLogicalSector (dos) == wanted) ? p : physical;
    }

    return physical;
}
