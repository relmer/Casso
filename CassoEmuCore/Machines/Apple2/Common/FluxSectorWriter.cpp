#include "Pch.h"

#include "Machines/Apple2/Common/FluxSectorWriter.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/FluxBitView.h"
#include "Machines/Apple2/Common/NibbleFieldWriter.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/TrackBitReader.h"
#include "Machines/Apple2/Common/TrackSectorDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FluxSectorWriter::WriteFluxTrackSectors
//
//  A bit track takes a sector write by being regenerated, which costs it any
//  timing or weak bits it had. A flux track does not: each sector's data
//  field is found in the track decoded at the controller's cell, and only the
//  fields whose bytes changed are spliced back into the flux, at the cell a
//  drive writes. Address fields, gaps and every other sector keep the timing
//  they were recorded with.
//
//  The splice starts at the data prologue rather than the sync bytes before
//  it, so the sync already on the disk -- written at whatever speed the disk
//  was -- stays.
//
//  A changed sector whose data field cannot be found fails the write, rather
//  than regenerating the track to make room for it. A track with no sectors
//  at all is blank, so there is nothing to keep and a standard track is laid
//  down in its place.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FluxSectorWriter::WriteFluxTrackSectors (
    const vector<Byte>  &  sectors,
    const int           *  interleave,
    int                    track,
    DiskImage           &  inOutImage)
{
    constexpr int     kMaxAttempts    = NibbleFormat::kSectorsPerTrack * 2;
    constexpr size_t  kSyncNibbleBits = 8 + NibbleFieldWriter::kSyncTailZeros;
    constexpr size_t  kMaxLeadZeros   = 16;



    HRESULT                    hr                                  = S_OK;
    FluxBitView                view;
    TrackBitReader::TrackBits  src                                 = TrackBitReader::GetTrackBits (inOutImage, track, view);
    size_t                     bitPos                              = 0;
    size_t                     start                               = 0;
    size_t                     fieldBits                           = 0;
    size_t                     offset                              = 0;
    size_t                     lead                                = 0;
    int                        attempt                             = 0;
    int                        physical                            = 0;
    uint16_t                   found                               = 0;
    uint16_t                   writeMask                           = 0;
    Byte                       outSector                           = 0;
    Byte                       data[NibbleFormat::kSectorByteSize] = {};
    bool                       changed                             = false;
    bool                       located                             = false;
    vector<size_t>   dataStart (NibbleFormat::kSectorsPerTrack, SIZE_MAX);
    vector<Byte>     priorData (static_cast<size_t> (NibbleFormat::kSectorsPerTrack) * NibbleFormat::kSectorByteSize, 0);
    vector<Byte>     field;
    vector<uint8_t>  bits;



    for (attempt = 0; attempt < kMaxAttempts && found != 0xFFFF && src.bitCount > 0; attempt++)
    {
        size_t         fieldStart = SIZE_MAX;
        size_t         dataAt     = SIZE_MAX;
        SectorOutcome  outcome    = SectorOutcome::Lost;
        HRESULT        hrSector   = TrackSectorDecoder::DecodeOneSector (src, bitPos, outSector, data, outcome, fieldStart, dataAt);

        if (FAILED (hrSector) || outcome == SectorOutcome::Lost || dataAt == SIZE_MAX)
        {
            continue;
        }

        if (outSector >= NibbleFormat::kSectorsPerTrack || (found & (1 << outSector)) != 0)
        {
            continue;
        }

        found = static_cast<uint16_t> (found | (1 << outSector));
        dataStart[outSector] = dataAt;
        memcpy (&priorData[static_cast<size_t> (outSector) * NibbleFormat::kSectorByteSize], data, NibbleFormat::kSectorByteSize);
    }

    if (found == 0)
    {
        WriteBlankFluxTrack (sectors, interleave, track, inOutImage);
    }

    // Every changed sector is found before any is written, so a write that
    // cannot be completed leaves the track exactly as it was.
    for (physical = 0; found != 0 && physical < NibbleFormat::kSectorsPerTrack; physical++)
    {
        offset  = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical]) * NibbleFormat::kSectorByteSize;
        changed = memcmp (&sectors[offset], &priorData[static_cast<size_t> (physical) * NibbleFormat::kSectorByteSize], NibbleFormat::kSectorByteSize) != 0;
        located = (dataStart[physical] != SIZE_MAX);

        CBREx (!changed || located, HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));

        writeMask = static_cast<uint16_t> (writeMask | (changed ? (1 << physical) : 0));
    }

    for (physical = 0; physical < NibbleFormat::kSectorsPerTrack; physical++)
    {
        if ((writeMask & (1 << physical)) == 0)
        {
            continue;
        }

        offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical]) * NibbleFormat::kSectorByteSize;

        field.assign (NibbleFormat::kTrackBitCapacity / 8, 0);
        fieldBits = 0;
        NibbleFieldWriter::AppendDataField (field, fieldBits, &sectors[offset]);
        TrackBitReader::UnpackBits (field, NibbleFieldWriter::kDataPrologueGap * kSyncNibbleBits, fieldBits, bits);

        // The decoder's position can sit in the zero tail of the sync byte
        // before the prologue; the field itself starts at its first 1 bit.
        start = dataStart[physical];

        for (lead = 0; lead < kMaxLeadZeros && TrackBitReader::GetBit (src, start) == 0; lead++)
        {
            start++;
        }

        inOutImage.SpliceFluxBulk (track, view.GetTickForBit (start), bits);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxSectorWriter::WriteBlankFluxTrack
//
//  A flux track with no sectors on it has no timing worth keeping, so it gets
//  a whole standard track written at the controller's cell.
//
////////////////////////////////////////////////////////////////////////////////

void FluxSectorWriter::WriteBlankFluxTrack (
    const vector<Byte>  &  sectors,
    const int           *  interleave,
    int                    track,
    DiskImage           &  inOutImage)
{
    vector<Byte>     field (NibbleFormat::kTrackBitCapacity / 8, 0);
    vector<uint8_t>  bits;
    size_t           fieldBits = 0;
    size_t           offset    = 0;
    int              physical  = 0;



    for (physical = 0; physical < NibbleFormat::kSectorsPerTrack; physical++)
    {
        offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical]) * NibbleFormat::kSectorByteSize;

        NibbleFieldWriter::AppendAddressField (field, fieldBits, NibbleFormat::kDefaultVolume, static_cast<Byte> (track), static_cast<Byte> (physical));
        NibbleFieldWriter::AppendDataField    (field, fieldBits, &sectors[offset]);
    }

    TrackBitReader::UnpackBits (field, 0, fieldBits, bits);
    inOutImage.SpliceFluxBulk (track, 0, bits);
}





