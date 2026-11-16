#include "Pch.h"

#include "Machines/Apple2/Common/TrackLayoutWriter.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackLayoutWriter::BuildTrack
//
//  The gaps' nibbles and their extra zero bits are spread evenly over the
//  16 gaps. A sync nibble has two zero bits, and one has a single zero
//  when the extra bits are odd, as a real disk's sync nibbles can.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TrackLayoutWriter::BuildTrack (const std::vector<Byte> & sectors, int track, const TrackLayout & layout, std::vector<uint8_t> & outBits)
{
    HRESULT               hr        = S_OK;
    int                   gapNibs   = layout.nibbles - NibblizationLayer::kSectorsPerTrack * kFieldNibbles;
    int                   zeros     = layout.bits - NibblizationLayer::kSectorsPerTrack * kFieldBits - 8 * gapNibs;
    int                   syncs     = (zeros + 1) / 2;
    int                   fills     = gapNibs - syncs;
    long long             start0    = 0;
    long long             length    = 0;
    long long             shift     = 0;
    bool                  isWhole   = sectors.size() >= (size_t) NibblizationLayer::kImageByteSize;
    bool                  isFit     = IsFeasible (layout);
    std::vector<uint8_t>  bits;



    outBits.clear();

    CBREx (track >= 0 && track < NibblizationLayer::kTrackCount && isWhole, E_INVALIDARG);
    CBREx (isFit, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    bits.reserve ((size_t) layout.bits);

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        int  gapFills = fills / NibblizationLayer::kSectorsPerTrack + (physical < fills % NibblizationLayer::kSectorsPerTrack ? 1 : 0);
        int  gapSyncs = syncs / NibblizationLayer::kSectorsPerTrack + (physical < syncs % NibblizationLayer::kSectorsPerTrack ? 1 : 0);

        for (int i = 0; i < gapFills; i++)
        {
            PutNibble (bits, 0xFF, 0);
        }

        for (int i = 0; i < gapSyncs; i++)
        {
            int  zeroBits = (zeros % 2 != 0 && physical == 0 && i == 0) ? 1 : 2;

            PutNibble (bits, 0xFF, zeroBits);
        }

        start0 = (physical == 0) ? (long long) bits.size() : start0;

        PutSector (bits, sectors, track, physical, layout.volume);
    }

    length = (long long) bits.size();
    shift  = ((start0 - layout.sector0) % length + length) % length;

    outBits.resize (bits.size());

    for (size_t at = 0; at < bits.size(); at++)
    {
        outBits[at] = bits[(at + (size_t) shift) % bits.size()];
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackLayoutWriter::BuildWoz
//
//  NibblizeDsk sets up the image's tracks and WOZ details, and each track's
//  bits are then replaced by its layout's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TrackLayoutWriter::BuildWoz (const std::vector<Byte> & sectors, const std::vector<TrackLayout> & layouts, std::vector<Byte> & outWoz)
{
    HRESULT               hr        = S_OK;
    bool                  isEvery   = layouts.size() == (size_t) NibblizationLayer::kTrackCount;
    DiskImage             image;
    std::vector<uint8_t>  bits;



    outWoz.clear();

    CBREx (isEvery, E_INVALIDARG);

    hr = NibblizationLayer::NibblizeDsk (sectors, image);
    CHR (hr);

    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        TrackLayout  layout = layouts[(size_t) track];

        if (layout.nibbles == 0)
        {
            layout.nibbles = kDefaultNibbles;
            layout.bits    = kDefaultBits;
            layout.sector0 = 0;
        }

        hr = BuildTrack (sectors, track, layout, bits);
        CHR (hr);

        image.ResizeTrack (track, bits.size());

        for (size_t at = 0; at < bits.size(); at++)
        {
            image.WriteBit (track, at, bits[at]);
        }
    }

    hr = WozLoader::Serialize (image, outWoz);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackLayoutWriter::IsFeasible
//
//  The bits beyond the nibbles' 8 each are the sync nibbles' zeros, 2 a
//  sync, and the gaps take at least kMinGapSyncs syncs each.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackLayoutWriter::IsFeasible (const TrackLayout & layout)
{
    int  gapNibs = layout.nibbles - NibblizationLayer::kSectorsPerTrack * kFieldNibbles;
    int  zeros   = layout.bits - NibblizationLayer::kSectorsPerTrack * kFieldBits - 8 * gapNibs;
    int  syncs   = (zeros + 1) / 2;



    return syncs >= NibblizationLayer::kSectorsPerTrack * kMinGapSyncs && gapNibs - syncs >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackLayoutWriter::PutNibble
//
////////////////////////////////////////////////////////////////////////////////

void TrackLayoutWriter::PutNibble (std::vector<uint8_t> & ioBits, Byte nibble, int zeros)
{
    for (int bit = 7; bit >= 0; bit--)
    {
        ioBits.push_back ((uint8_t) ((nibble >> bit) & 1));
    }

    ioBits.insert (ioBits.end(), (size_t) zeros, (uint8_t) 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackLayoutWriter::PutSector
//
//  The address field, with its volume, track, sector and checksum in 4-and-4
//  form, the data gap, and the data field of the sector DOS 3.3 places at
//  this physical position.
//
////////////////////////////////////////////////////////////////////////////////

void TrackLayoutWriter::PutSector (std::vector<uint8_t> & ioBits, const std::vector<Byte> & sectors, int track, int physical, Byte volume)
{
    Byte               trackByte  = (Byte) track;
    Byte               sectorByte = (Byte) physical;
    Byte               checksum   = (Byte) (volume ^ trackByte ^ sectorByte);
    size_t             offset     = ((size_t) track * NibblizationLayer::kSectorsPerTrack + (size_t) NibblizationLayer::GetDosFileIndexForPhysicalSector (physical)) * NibblizationLayer::kSectorByteSize;
    std::vector<Byte>  data;



    PutNibble (ioBits, NibblizationLayer::kProlog0, 0);
    PutNibble (ioBits, NibblizationLayer::kProlog1, 0);
    PutNibble (ioBits, NibblizationLayer::kAddressProlog2, 0);

    for (Byte value : { volume, trackByte, sectorByte, checksum })
    {
        PutNibble (ioBits, (Byte) ((value >> 1) | 0xAA), 0);
        PutNibble (ioBits, (Byte) (value | 0xAA), 0);
    }

    PutNibble (ioBits, 0xDE, 0);
    PutNibble (ioBits, 0xAA, 0);
    PutNibble (ioBits, 0xEB, 0);

    for (int i = 0; i < kDataGapSyncs; i++)
    {
        PutNibble (ioBits, 0xFF, 2);
    }

    NibblizationLayer::EncodeDataNibbles (&sectors[offset], data);

    PutNibble (ioBits, NibblizationLayer::kProlog0, 0);
    PutNibble (ioBits, NibblizationLayer::kProlog1, 0);
    PutNibble (ioBits, NibblizationLayer::kDataProlog2, 0);

    for (Byte nibble : data)
    {
        PutNibble (ioBits, nibble, 0);
    }

    PutNibble (ioBits, 0xDE, 0);
    PutNibble (ioBits, 0xAA, 0);
    PutNibble (ioBits, 0xEB, 0);
}
