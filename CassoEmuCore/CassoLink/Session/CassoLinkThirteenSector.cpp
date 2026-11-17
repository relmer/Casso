#include "Pch.h"

#include "CassoLink/Session/CassoLinkThirteenSector.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkSession.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkThirteenSector::ReadDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkThirteenSector::ReadDisk (CassoLinkHost & host, Byte drive, const ProgressFn & progress, const CancelFn & cancel,
                                           std::vector<Byte> & outSectors, SectorDecodeReport & outReport, CassoLinkStatus & outStatus)
{
    HRESULT  hr = S_OK;



    outStatus = {};
    outSectors.assign (ThirteenSectorCodec::kImageByteSize, 0);
    outReport.Reset (ThirteenSectorCodec::kTrackCount);

    for (int track = 0; track < ThirteenSectorCodec::kTrackCount; track++)
    {
        bool                canceled = cancel && cancel();
        Word                coverage = 0;
        TrackDecodeOutcome  outcome  = TrackDecodeOutcome::Unformatted;

        CBREx (!canceled, HRESULT_FROM_WIN32 (ERROR_CANCELLED));

        hr = ReadTrack (host, drive, track, outSectors, coverage, outStatus);
        CHR (hr);
        BAIL_OUT_IF (outStatus.error != CassoLinkHost::kErrorNone, S_OK);

        if (coverage != 0)
        {
            outcome  = (coverage == s_kAllSectors) ? TrackDecodeOutcome::Complete : TrackDecodeOutcome::Partial;
            coverage = (Word) (coverage | ThirteenSectorCodec::kUnusedCoverage);
        }

        outReport.SetOutcome (track, outcome, coverage, false);

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
//  CassoLinkThirteenSector::ReadTrack
//
//  Each request is for the sectors not yet read. A sector with no data
//  field, as on a track DOS 3.2's INIT formatted and never wrote, is taken
//  as zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkThirteenSector::ReadTrack (CassoLinkHost & host, Byte drive, int track, std::vector<Byte> & inOutSectors, Word & outCoverage, CassoLinkStatus & outStatus)
{
    HRESULT                         hr        = S_OK;
    Word                            pending   = s_kAllSectors;
    Word                            unwritten = 0;
    std::vector<std::vector<Byte>>  fields;



    outCoverage = 0;

    for (int attempt = 0; attempt < s_kTrackTries && pending != 0; attempt++)
    {
        hr = host.ReadThirteenTrack (drive, (Byte) track, pending, fields, unwritten, outStatus);
        CHR (hr);
        BAIL_OUT_IF (outStatus.error != CassoLinkHost::kErrorNone, S_OK);

        for (int sector = 0; sector < ThirteenSectorCodec::kSectorsPerTrack; sector++)
        {
            Word                       bit   = (Word) (1u << sector);
            const std::vector<Byte>  & field = fields[(size_t) sector];
            DataFieldDecode            decode;
            size_t                     base  = ((size_t) track * ThirteenSectorCodec::kSectorsPerTrack + (size_t) sector) * ThirteenSectorCodec::kSectorByteSize;

            if ((pending & bit) == 0 || (field.empty() && (unwritten & bit) == 0))
            {
                continue;
            }

            if (!field.empty())
            {
                DiskFieldFormat::DecodeData (DiskFieldKind::Thirteen, std::span<const Byte> (field).first (DiskFieldFormat::kBodyLength53),
                                             field[DiskFieldFormat::kBodyLength53], decode);

                if (!decode.isChecksumGood)
                {
                    continue;
                }

                std::copy (decode.bytes.begin(), decode.bytes.end(), inOutSectors.begin() + (ptrdiff_t) base);
            }

            pending     = (Word) (pending & ~bit);
            outCoverage = (Word) (outCoverage | bit);
        }
    }

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
