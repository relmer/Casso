#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"

class DiskImage;
class SectorDecodeReport;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkThirteenSector
//
//  A 13-sector disk, DOS 3.2's, over the link (FR-216). The program reads
//  and writes 16-sector fields alone, so both go through the 13-sector module,
//  which must be loaded. A read is each track's data fields by Read 13-sector
//  track, decoded on the PC; a write is each track's stream as
//  ThirteenSectorCodec lays it out, by Write nibble track.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkThirteenSector
{
public:
    static constexpr Byte  kVolume = 254;

    using ProgressFn = std::function<void (int track)>;
    using CancelFn   = std::function<bool ()>;

    //  The disk's sectors, a .d13 buffer, with each track's sectors in
    //  outReport. A sector with no data field is zeros and in the report; one
    //  not read after s_kTrackTries requests is zeros and missing from it. The
    //  first track whose Status has an error stops the read, with the Status
    //  in outStatus. ERROR_CANCELLED for a cancel.
    static HRESULT  ReadDisk  (CassoLinkHost & host, Byte drive, const ProgressFn & progress, const CancelFn & cancel,
                               std::vector<Byte> & outSectors, SectorDecodeReport & outReport, CassoLinkStatus & outStatus);

    //  A .d13 buffer written to the disk, track by track, under kVolume. The
    //  first track whose Status has an error stops the write, with the Status
    //  in outStatus.
    static HRESULT  WriteDisk (CassoLinkHost & host, Byte drive, const std::vector<Byte> & sectors, const ProgressFn & progress, const CancelFn & cancel,
                               CassoLinkStatus & outStatus);

    //  A track's bits as Write nibble track's stream: each byte framed from
    //  the bits a nibble, and an FF followed by zero bits a self-sync byte,
    //  a 0.
    static void     MakeStream (const DiskImage & image, int slot, std::vector<Byte> & outStream);

private:
    static constexpr Byte  s_kSyncByte    = 0xFF;
    static constexpr Word  s_kAllSectors  = 0x1FFF;
    static constexpr int   s_kTrackTries  = 4;

    static HRESULT  ReadTrack (CassoLinkHost & host, Byte drive, int track, std::vector<Byte> & inOutSectors, Word & outCoverage, CassoLinkStatus & outStatus);
};
