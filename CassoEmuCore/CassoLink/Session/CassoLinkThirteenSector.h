#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/Session/CassoLinkCapture.h"

class DiskImage;
class SectorDecodeReport;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkThirteenSector
//
//  A 13-sector disk, DOS 3.2's, over the link (FR-216). The program reads
//  and writes 16-sector fields alone. A read is a raw capture
//  (CassoLinkCapture), whose WOZ is decoded on the PC by
//  ThirteenSectorCodec; a write is each track's stream as the codec lays it
//  out, by the nibble module's Write nibble track, which must be loaded.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkThirteenSector
{
public:
    static constexpr Byte  kVolume = 254;

    using ProgressFn = std::function<void (int track)>;
    using CancelFn   = std::function<bool ()>;

    //  The disk's sectors, a .d13 buffer, from a capture by `module`, the
    //  capture module, with each track's sectors in outReport. A track that
    //  doesn't decode has its sectors as zeros and missing from the report.
    //  ERROR_CANCELLED for a cancel.
    static HRESULT  ReadDisk  (CassoLinkHost & host, Byte drive, std::span<const Byte> module, const CassoLinkCapture::ProgressFn & progress,
                               const CancelFn & cancel, std::vector<Byte> & outSectors, SectorDecodeReport & outReport);

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
    static constexpr Byte  s_kSyncByte = 0xFF;
};
