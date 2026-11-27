#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/FieldLocator.h"
#include "Devices/Disk/SectorFieldWriter.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"

struct TrackMarks;





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec
//
//  Tracks of the 13-sector format of DOS 3.2 and earlier, to and from a flat
//  sector buffer. The buffer is the layout of a .d13 file: 35 tracks of 13
//  sectors of 256 bytes, in physical sector order, so sector S of track T is
//  at (T * 13 + S) * 256.
//
//  A 13-sector address field begins D5 AA B5 and its data field is 410
//  5-and-3 nibbles and a checksum nibble (DiskFieldFormat). RDOS 3.2 begins
//  its address fields D4 AA B7, and those are read too.
//
//  In a decode report, bit S of a track's coverage is physical sector S. Bits
//  13 to 15 are set on every formatted track, so a track with all thirteen
//  sectors has SectorDecodeReport::kFullCoverage.
//
////////////////////////////////////////////////////////////////////////////////

class ThirteenSectorCodec
{
public:
    static constexpr int     kTrackCount      = 35;
    static constexpr int     kSectorsPerTrack = 13;
    static constexpr size_t  kSectorByteSize  = 256;
    static constexpr size_t  kImageByteSize   = kSectorByteSize * kSectorsPerTrack * kTrackCount;
    static constexpr Word    kUnusedCoverage  = 0xE000;
    static constexpr Word    kAllSectors      = (Word) ((1u << kSectorsPerTrack) - 1);

    //  True when the first formatted track examined has more 13-sector
    //  address fields than 16-sector ones. Track 0 is examined last, because
    //  a 13-sector disk can have a 16-sector boot track.
    static bool     IsThirteenSector     (const DiskImage & image);

    //  True when the boot track has 13-sector address fields and no 16-sector
    //  ones, so only the 13-sector boot ROM can start the disk. For a disk
    //  with no address field on track 0, IsThirteenSector returns the answer.
    static bool     IsThirteenSectorBoot (const DiskImage & image);

    //  Succeeds over damage: a sector that failed to decode is zeros in the
    //  buffer and its track is Partial in the report.
    static void     Denibblize (const DiskImage & image, vector<Byte> & outSectors, SectorDecodeReport & outReport);

    //  A formatted disk from a sector buffer. A buffer of the wrong size is
    //  ERROR_BAD_LENGTH.
    static HRESULT  Nibblize   (const vector<Byte> & sectors, Byte volume, DiskImage & out);

    //  The tracks, and the sectors, that differ between two sector buffers.
    //  Both are empty unless each buffer is kImageByteSize bytes.
    static void     CollectChangedTracks (const vector<Byte> & priorSectors, const vector<Byte> & editedSectors, vector<int> & outTracks);
    static void     CollectSectorWrites  (const vector<Byte> & priorSectors, const vector<Byte> & editedSectors, vector<SectorWrite> & outWrites);

private:
    static FieldMarks  MakeMarks         ();
    static void        AddMark           (vector<DiskMarkPattern> & ioMarks, const DiskMarkPattern & mark);
    static FieldMarks  MakeStampedMarks  (const TrackMarks & marks);
    static void        LocateTrack       (const DiskImage & image, int track, const FieldMarks & marks, vector<LocatedField> & outFields);
    static void        TakeSectors       (const vector<LocatedField>  &  fields,
                                          int                            track,
                                          Word                           skip,
                                          Word                        &  ioCoverage,
                                          bool                        &  ioDuplicated,
                                          int                         &  ioAddresses,
                                          vector<Byte>                &  outSectors);
    static bool        IsGoodSector      (const vector<LocatedField> & fields, const LocatedField & address);
    static bool        IsUnwrittenSector (const LocatedField & address);
    static void        AppendNibble      (vector<Byte> & bits, size_t & bitOffset, Byte nibble);
    static void        AppendSync        (vector<Byte> & bits, size_t & bitOffset, int count);
    static void        AppendMark        (vector<Byte> & bits, size_t & bitOffset, const DiskMarkPattern & mark);
    static void        AppendSector      (vector<Byte> & bits, size_t & bitOffset, Byte volume, Byte track, Byte sector, std::span<const Byte> bytes);
};
