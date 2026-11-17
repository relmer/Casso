#include "Pch.h"

#include "CassoLink/Session/CassoLinkThirteenSector.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkSession.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkThirteenSector::ReadDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkThirteenSector::ReadDisk (CassoLinkHost & host, Byte drive, std::span<const Byte> module, const CassoLinkCapture::ProgressFn & progress,
                                           const CancelFn & cancel, std::vector<Byte> & outSectors, SectorDecodeReport & outReport)
{
    HRESULT                   hr = S_OK;
    CassoLinkCapture::Result  result;
    DiskImage                 image;



    outSectors.clear();

    hr = CassoLinkCapture::Capture (host, drive, module, std::string(), progress, result, CassoLinkCapture::kTracks - 1, cancel);
    CHR (hr);

    hr = WozLoader::Load (result.woz, image);
    CHR (hr);

    ThirteenSectorCodec::Denibblize (image, outSectors, outReport);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkThirteenSector::WriteDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkThirteenSector::WriteDisk (CassoLinkHost & host, Byte drive, const std::vector<Byte> & sectors, const ProgressFn & progress, const CancelFn & cancel,
                                            CassoLinkStatus & outStatus)
{
    HRESULT            hr     = S_OK;
    DiskImage          image;
    std::vector<Byte>  stream;



    outStatus = {};

    hr = ThirteenSectorCodec::Nibblize (sectors, kVolume, image);
    CHR (hr);

    for (int track = 0; track < ThirteenSectorCodec::kTrackCount; track++)
    {
        bool  canceled = cancel && cancel();

        CBREx (!canceled, HRESULT_FROM_WIN32 (ERROR_CANCELLED));

        MakeStream (image, image.ResolveWholeTrack (track), stream);

        hr = host.WriteNibbleTrack (drive, (Byte) track, stream, outStatus);
        CHR (hr);
        BAIL_OUT_IF (outStatus.error != CassoLinkHost::kErrorNone, S_OK);

        if (progress)
        {
            progress (track);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkThirteenSector::MakeStream
//
//  A byte is complete when its top bit is set; the zero bits after it count
//  toward a self-sync byte, which an FF with any is taken as. Zero bits
//  before the first byte are dropped.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkThirteenSector::MakeStream (const DiskImage & image, int slot, std::vector<Byte> & outStream)
{
    size_t  count = (slot >= 0) ? image.GetTrackBitCount (slot) : 0;
    Byte    value = 0;



    outStream.clear();

    for (size_t at = 0; at < count; at++)
    {
        value = (Byte) ((value << 1) | image.ReadBit (slot, at));

        if ((value & 0x80) == 0)
        {
            continue;
        }

        if (value == s_kSyncByte && at + 1 < count && image.ReadBit (slot, at + 1) == 0)
        {
            outStream.push_back (0);
        }
        else
        {
            outStream.push_back (value);
        }

        value = 0;

        while (at + 1 < count && image.ReadBit (slot, at + 1) == 0)
        {
            at++;
        }
    }
}
