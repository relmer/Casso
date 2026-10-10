#include "Pch.h"

#include "Devices/Disk/Inspector/TrackExport.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Machines/Apple2/Common/WozLoader.h"





static constexpr int  s_kSectors13      = 13;
static constexpr int  s_kBlocksPerTrack = 8;
static constexpr int  s_kHalvesPerBlock = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::ExportSector
//
//  One sector's 256 bytes: as decoded when its data field is there, zeros
//  when it is not; anything but a good sector is listed.
//
////////////////////////////////////////////////////////////////////////////////

void TrackExport::ExportSector (const TrackAnalysis & track, int wholeTrack, int sectorIndex, vector<Byte> & inOutBytes, vector<ExportProblem> & inOutProblems)
{
    const AnalyzedSector &  sector = track.sectors[sectorIndex];
    ExportProblem           problem;



    if (sector.dataField >= 0)
    {
        const auto &  bytes = track.fields[sector.dataField].data.bytes;

        inOutBytes.insert (inOutBytes.end(), bytes.begin(), bytes.end());
    }
    else
    {
        inOutBytes.insert (inOutBytes.end(), DiskFieldFormat::kSectorBytes, Byte (0));
    }

    problem.track  = wholeTrack;
    problem.sector = sector.sector;

    switch (sector.state)
    {
        case SectorState::BadAddress:  problem.reason = L"address checksum failed; written as decoded"; break;
        case SectorState::BadData:     problem.reason = L"data checksum failed; written as decoded";    break;
        case SectorState::NotChecked:  problem.reason = L"checksums not checked; written as decoded";   break;
        case SectorState::NoDataField: problem.reason = L"no data field; written as zeros";             break;
        default:                                                                                         break;
    }

    if (!problem.reason.empty())
    {
        inOutProblems.push_back (problem);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::ExportTrackSectors
//
//  A whole track's sectors in the order chosen; a 13-sector track has only
//  its physical order. A sector not on the track is written as zeros.
//
////////////////////////////////////////////////////////////////////////////////

void TrackExport::ExportTrackSectors (const TrackAnalysis * track, int wholeTrack, SectorOrder order, vector<Byte> & inOutBytes, vector<ExportProblem> & inOutProblems)
{
    bool         is13     = track != nullptr && track->has13 && !track->has16;
    int          count    = is13 ? s_kSectors13 : kSectorsPerTrack;
    SectorOrder  used     = is13 ? SectorOrder::Physical : order;
    int          position = 0;
    int          index    = -1;



    for (position = 0; position < count; position++)
    {
        index = (track != nullptr) ? FindSector (*track, wholeTrack, used, position) : -1;

        if (index >= 0)
        {
            ExportSector (*track, wholeTrack, index, inOutBytes, inOutProblems);
        }
        else
        {
            inOutBytes.insert (inOutBytes.end(), DiskFieldFormat::kSectorBytes, Byte (0));
            inOutProblems.push_back ({ wholeTrack, -1, std::format (L"{} {} is missing; written as zeros",
                                                                    used == SectorOrder::Dos33  ? L"DOS 3.3 sector" :
                                                                    used == SectorOrder::ProDos ? L"ProDOS sector"  : L"sector",
                                                                    InspectorFormat::FormatSector (position)) });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::ExportSectors
//
//  Whole tracks first to last, each from the record its whole track plays.
//
////////////////////////////////////////////////////////////////////////////////

void TrackExport::ExportSectors (const DiskAnalysis & analysis, int firstTrack, int lastTrack, SectorOrder order, vector<Byte> & outBytes,
                                 vector<ExportProblem> & outProblems)
{
    int                    t     = 0;
    int                    slot  = -1;
    const TrackAnalysis *  track = nullptr;



    outBytes.clear();
    outProblems.clear();

    for (t = firstTrack; t <= lastTrack; t++)
    {
        slot  = (t >= 0 && t * DiskImage::kQuarterTracksPerWholeTrack < DiskImage::kQuarterTrackCount) ? analysis.entries[t * DiskImage::kQuarterTracksPerWholeTrack].slot : -1;
        track = (slot >= 0 && slot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[slot].get() : nullptr;

        ExportTrackSectors (track, t, order, outBytes, outProblems);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::ExportNibbles
//
//  From the first nibble at or after the index, once around.
//
////////////////////////////////////////////////////////////////////////////////

void TrackExport::ExportNibbles (const TrackAnalysis & track, vector<Byte> & outBytes, vector<std::wstring> & outNotes)
{
    const FramedTrack &  framed = track.framed;
    size_t               n      = framed.nibbles.size();
    size_t               first  = 0;
    size_t               i      = 0;
    uint32_t             count  = std::max<uint32_t> (framed.cellCount, 1);



    outBytes.clear();
    outNotes.clear();

    for (i = 1; i < n; i++)
    {
        if (framed.nibbles[i].startCell % count < framed.nibbles[first].startCell % count)
        {
            first = i;
        }
    }

    for (i = 0; i < n; i++)
    {
        outBytes.push_back (framed.nibbles[(first + i) % n].value);
    }

    for (const RandomRegion & region : framed.randomRegions)
    {
        outNotes.push_back (std::format (L"Random bits at cells {}-{}, written as the nibbles framed there", InspectorFormat::FormatCount (region.startCell),
                                         InspectorFormat::FormatCount (region.startCell + region.cellCount - 1)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::ExportTrackBits
//
//  The record exactly as Casso holds it, mapped at its quarter track alone:
//  a bit track with its bit count, or a flux track with every interval.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TrackExport::ExportTrackBits (const TrackCopy & copy, int quarterTrack, vector<Byte> & outBytes)
{
    HRESULT                    hr     = S_OK;
    vector<WozSyntheticTrack>  tracks (1);



    tracks[0].isFlux        = copy.kind == TrackKind::Flux;
    tracks[0].data          = tracks[0].isFlux ? copy.fluxBytes : copy.bits;
    tracks[0].bitCount      = tracks[0].isFlux ? 0 : copy.bitCount;
    tracks[0].quarterTracks = { quarterTrack };

    hr = WozLoader::BuildSyntheticV21 (tracks, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::GetSectorsName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TrackExport::GetSectorsName (const std::wstring & image, int firstTrack, int lastTrack)
{
    std::wstring  tracks = FormatTrack (firstTrack) + ((lastTrack != firstTrack) ? L"-" + FormatTrack (lastTrack) : L"");



    return image + L" " + tracks + L" sectors.bin";
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::GetNibblesName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TrackExport::GetNibblesName (const std::wstring & image, int quarterTrack)
{
    return image + L" T" + InspectorFormat::FormatQuarterTrack (quarterTrack) + L" nibbles.bin";
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::GetBitsName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TrackExport::GetBitsName (const std::wstring & image, int quarterTrack)
{
    return image + L" T" + InspectorFormat::FormatQuarterTrack (quarterTrack) + L".woz";
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::FindSector
//
//  The sector at a position of the chosen order: by its number, by its DOS
//  3.3 logical sector, or by the half of the ProDOS block it holds.
//
////////////////////////////////////////////////////////////////////////////////

int TrackExport::FindSector (const TrackAnalysis & track, int wholeTrack, SectorOrder order, int position)
{
    int     found = -1;
    size_t  s     = 0;



    for (s = 0; found < 0 && s < track.sectors.size(); s++)
    {
        const AnalyzedSector &  sector = track.sectors[s];

        if ((order == SectorOrder::Physical && sector.sector == position) || (order == SectorOrder::Dos33 && sector.dos33Logical == position) ||
            (order == SectorOrder::ProDos && sector.prodosBlock == wholeTrack * s_kBlocksPerTrack + position / s_kHalvesPerBlock &&
             sector.prodosHalf == position % s_kHalvesPerBlock))
        {
            found = static_cast<int> (s);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport::FormatTrack
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TrackExport::FormatTrack (int wholeTrack)
{
    return std::format (L"T{:02}", wholeTrack);
}
