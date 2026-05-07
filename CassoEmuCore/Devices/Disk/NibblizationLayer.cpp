#include "Pch.h"

#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  6-and-2 write translate table (64 entries → nibble bytes 0x96..0xFF)
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte kWriteTranslate[64] =
{
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};


static constexpr int   kEncodedDataSize       = 342;
static constexpr int   kBitsPerNibble         = 8;
static constexpr int   kAddrPrologueGap       = 20;
static constexpr int   kDataPrologueGap       = 6;
static constexpr Byte  kAddrProlog0           = 0xD5;
static constexpr Byte  kAddrProlog1           = 0xAA;
static constexpr Byte  kAddrProlog2           = 0x96;
static constexpr Byte  kDataProlog2           = 0xAD;
static constexpr Byte  kEpilog0               = 0xDE;
static constexpr Byte  kEpilog1               = 0xAA;
static constexpr Byte  kEpilog2               = 0xEB;
static constexpr Byte  kSyncNibble            = 0xFF;
static constexpr int   kThirdGroupSize        = 86;





////////////////////////////////////////////////////////////////////////////////
//
//  DOS 3.3 logical-to-physical interleave (used when nibblizing .dsk/.do)
//
//  Interpretation (matching the Phase 9 helper this file replaces):
//  loop var L = 0..15 is the DOS logical sector address mark we emit; the
//  256 source bytes for that mark come from file offset
//  (track * 16 + kDsk_LtoP[L]) * 256.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int kDsk_LtoP[16] =
{
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProDOS logical-to-physical interleave for .po
//
//  .po arranges its 16 sectors per track in ProDOS-block order rather than
//  DOS-sector order. ProDOS-sector index 0..15 stored in the file maps to
//  DOS logical sector via kPo_FileToDosLogical:
//      file[0] = DOS logical 0     file[8]  = DOS logical 1
//      file[1] = DOS logical 14    file[9]  = DOS logical 13   ... etc
//  When emitting DOS logical sector L at nibble time, the 256 source bytes
//  live at file offset (track*16 + kPo_DosLogicalToFile[L]) * 256.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int kPo_DosLogicalToFile[16] =
{
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};





////////////////////////////////////////////////////////////////////////////////
//
//  Encode44Odd / Encode44Even
//
////////////////////////////////////////////////////////////////////////////////

static Byte Encode44Odd (Byte val)
{
    return static_cast<Byte> ((val >> 1) | 0xAA);
}


static Byte Encode44Even (Byte val)
{
    return static_cast<Byte> (val | 0xAA);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PackNibbleBits
//
//  Append 8 bits (MSB-first) of `nibble` into a packed bit-stream byte
//  vector at the given bit offset. Caller pre-sized the destination.
//
////////////////////////////////////////////////////////////////////////////////

static void PackNibbleBits (vector<Byte> & dst, size_t & bitOffset, Byte nibble)
{
    int    bit  = 0;
    Byte   b    = 0;
    Byte   mask = 0;

    for (bit = 7; bit >= 0; bit--)
    {
        b    = static_cast<Byte> ((nibble >> bit) & 1);
        mask = static_cast<Byte> (b << (7 - (bitOffset & 7)));
        dst[bitOffset >> 3] = static_cast<Byte> (dst[bitOffset >> 3] | mask);
        bitOffset++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendAddressField
//
//  Emits sync gap + address prologue + 4-and-4 V/T/S/checksum + epilogue
//  for one sector.
//
////////////////////////////////////////////////////////////////////////////////

static void AppendAddressField (
    vector<Byte>   &  dst,
    size_t         &  bitOffset,
    Byte              volume,
    Byte              track,
    Byte              sector)
{
    Byte    checksum = static_cast<Byte> (volume ^ track ^ sector);
    int     i        = 0;

    for (i = 0; i < kAddrPrologueGap; i++)
    {
        PackNibbleBits (dst, bitOffset, kSyncNibble);
    }

    PackNibbleBits (dst, bitOffset, kAddrProlog0);
    PackNibbleBits (dst, bitOffset, kAddrProlog1);
    PackNibbleBits (dst, bitOffset, kAddrProlog2);
    PackNibbleBits (dst, bitOffset, Encode44Odd  (volume));
    PackNibbleBits (dst, bitOffset, Encode44Even (volume));
    PackNibbleBits (dst, bitOffset, Encode44Odd  (track));
    PackNibbleBits (dst, bitOffset, Encode44Even (track));
    PackNibbleBits (dst, bitOffset, Encode44Odd  (sector));
    PackNibbleBits (dst, bitOffset, Encode44Even (sector));
    PackNibbleBits (dst, bitOffset, Encode44Odd  (checksum));
    PackNibbleBits (dst, bitOffset, Encode44Even (checksum));
    PackNibbleBits (dst, bitOffset, kEpilog0);
    PackNibbleBits (dst, bitOffset, kEpilog1);
    PackNibbleBits (dst, bitOffset, kEpilog2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendDataField
//
//  Emits sync gap + data prologue + 6-and-2 encoded payload + checksum +
//  epilogue.
//
////////////////////////////////////////////////////////////////////////////////

static void AppendDataField (
    vector<Byte>   &  dst,
    size_t         &  bitOffset,
    const Byte     *  sectorData)
{
    Byte    encoded[kEncodedDataSize] = {};
    Byte    prev = 0;
    Byte    enc  = 0;
    int     i    = 0;

    for (i = 0; i < kDataPrologueGap; i++)
    {
        PackNibbleBits (dst, bitOffset, kSyncNibble);
    }

    PackNibbleBits (dst, bitOffset, kAddrProlog0);
    PackNibbleBits (dst, bitOffset, kAddrProlog1);
    PackNibbleBits (dst, bitOffset, kDataProlog2);

    for (i = 0; i < kThirdGroupSize; i++)
    {
        Byte   v = static_cast<Byte> (
            ((sectorData[i] & 0x01) << 1) |
            ((sectorData[i] & 0x02) >> 1));

        if (i + kThirdGroupSize < NibblizationLayer::kSectorByteSize)
        {
            v = static_cast<Byte> (v |
                ((sectorData[i + kThirdGroupSize] & 0x01) << 3) |
                ((sectorData[i + kThirdGroupSize] & 0x02) << 1));
        }

        if (i + 2 * kThirdGroupSize < NibblizationLayer::kSectorByteSize)
        {
            v = static_cast<Byte> (v |
                ((sectorData[i + 2 * kThirdGroupSize] & 0x01) << 5) |
                ((sectorData[i + 2 * kThirdGroupSize] & 0x02) << 3));
        }

        encoded[i] = v;
    }

    for (i = 0; i < NibblizationLayer::kSectorByteSize; i++)
    {
        encoded[kThirdGroupSize + i] = static_cast<Byte> (sectorData[i] >> 2);
    }

    prev = 0;

    for (i = 0; i < kEncodedDataSize; i++)
    {
        enc = static_cast<Byte> (encoded[i] ^ prev);
        PackNibbleBits (dst, bitOffset, kWriteTranslate[enc & 0x3F]);
        prev = encoded[i];
    }

    PackNibbleBits (dst, bitOffset, kWriteTranslate[prev & 0x3F]);
    PackNibbleBits (dst, bitOffset, kEpilog0);
    PackNibbleBits (dst, bitOffset, kEpilog1);
    PackNibbleBits (dst, bitOffset, kEpilog2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizeWithMap
//
//  Common nibblization driver. `interleave` selects the source-byte
//  offset for each DOS-logical sector.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT NibblizeWithMap (
    const vector<Byte>  &  raw,
    const int           *  interleave,
    DiskImage           &  out)
{
    HRESULT   hr        = S_OK;
    int       track     = 0;
    int       logical   = 0;
    size_t    offset    = 0;

    if (raw.size () != NibblizationLayer::kImageByteSize)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        size_t   bitOffset = 0;

        out.ResizeTrack (track, NibblizationLayer::kTrackBitCapacity);

        for (logical = 0; logical < NibblizationLayer::kSectorsPerTrack; logical++)
        {
            offset = static_cast<size_t> (track * NibblizationLayer::kSectorsPerTrack + interleave[logical])
                   * NibblizationLayer::kSectorByteSize;

            AppendAddressField (out.GetTrackBitsForWrite (track), bitOffset,
                                NibblizationLayer::kDefaultVolume,
                                static_cast<Byte> (track),
                                static_cast<Byte> (logical));
            AppendDataField    (out.GetTrackBitsForWrite (track), bitOffset, &raw[offset]);
        }
    }

    out.ClearDirty ();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer::NibblizeDsk / NibblizeDo / NibblizePo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::NibblizeDsk (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, kDsk_LtoP, out);

    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Dsk);
    }

    return hr;
}


HRESULT NibblizationLayer::NibblizeDo (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, kDsk_LtoP, out);

    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Do);
    }

    return hr;
}


HRESULT NibblizationLayer::NibblizePo (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, kPo_DosLogicalToFile, out);

    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Po);
    }

    return hr;
}


HRESULT NibblizationLayer::Nibblize (const vector<Byte> & raw, DiskFormat fmt, DiskImage & out)
{
    switch (fmt)
    {
        case DiskFormat::Dsk: return NibblizeDsk (raw, out);
        case DiskFormat::Do:  return NibblizeDo  (raw, out);
        case DiskFormat::Po:  return NibblizePo  (raw, out);
        default:              return E_INVALIDARG;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read/Find helpers for Denibblize
//
////////////////////////////////////////////////////////////////////////////////

static Byte ReadNibbleAt (const DiskImage & img, int track, size_t & bitPos)
{
    size_t   trackBits = img.GetTrackBitCount (track);
    Byte     value     = 0;
    int      i         = 0;
    Byte     bit       = 0;

    if (trackBits == 0)
    {
        return 0;
    }

    while ((value & 0x80) == 0)
    {
        bit    = img.ReadBit (track, bitPos % trackBits);
        bitPos++;
        value  = static_cast<Byte> ((value << 1) | (bit & 1));

        if (bitPos > trackBits * 4)
        {
            return 0;
        }
    }

    for (i = 0; i < 7; i++)
    {
        bit    = img.ReadBit (track, bitPos % trackBits);
        bitPos++;
        value  = static_cast<Byte> ((value << 1) | (bit & 1));
    }

    return value;
}


static Byte Decode44 (Byte odd, Byte even)
{
    return static_cast<Byte> (((odd << 1) | 1) & even);
}


static Byte InverseTranslate (Byte nib)
{
    int   i = 0;

    for (i = 0; i < 64; i++)
    {
        if (kWriteTranslate[i] == nib)
        {
            return static_cast<Byte> (i);
        }
    }

    return 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeOneSector
//
//  Walks the bit stream until it locates the next address-field prologue,
//  then decodes the V/T/S header followed by the data field. On success
//  fills outSectorData[256] and returns the address-field track/sector via
//  out params. Returns E_FAIL if no valid sector can be decoded before
//  the cursor wraps.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT DecodeOneSector (
    const DiskImage   &  img,
    int                  track,
    size_t            &  bitPos,
    Byte              &  outSector,
    Byte              *  outData)
{
    HRESULT   hr = S_OK;
    Byte      n0 = 0;
    Byte      n1 = 0;
    Byte      n2 = 0;
    Byte      vOdd = 0;
    Byte      vEven = 0;
    Byte      tOdd = 0;
    Byte      tEven = 0;
    Byte      sOdd = 0;
    Byte      sEven = 0;
    Byte      cOdd = 0;
    Byte      cEven = 0;
    Byte      foundProlog = 0;
    Byte      encoded[kEncodedDataSize] = {};
    Byte      prev = 0;
    Byte      raw  = 0;
    int       i    = 0;
    size_t    trackBits = img.GetTrackBitCount (track);
    size_t    startBitPos = bitPos;
    size_t    bitsConsumed = 0;

    if (trackBits == 0)
    {
        hr = E_FAIL;
        goto Error;
    }

    foundProlog = 0;

    while (foundProlog == 0)
    {
        n0 = ReadNibbleAt (img, track, bitPos);
        if (n0 != kAddrProlog0)
        {
            bitsConsumed = (bitPos - startBitPos);
            if (bitsConsumed > trackBits + 8)
            {
                hr = E_FAIL;
                goto Error;
            }
            continue;
        }

        n1 = ReadNibbleAt (img, track, bitPos);
        n2 = ReadNibbleAt (img, track, bitPos);

        if (n1 == kAddrProlog1 && n2 == kAddrProlog2)
        {
            foundProlog = 1;
        }
    }

    vOdd  = ReadNibbleAt (img, track, bitPos);
    vEven = ReadNibbleAt (img, track, bitPos);
    tOdd  = ReadNibbleAt (img, track, bitPos);
    tEven = ReadNibbleAt (img, track, bitPos);
    sOdd  = ReadNibbleAt (img, track, bitPos);
    sEven = ReadNibbleAt (img, track, bitPos);
    cOdd  = ReadNibbleAt (img, track, bitPos);
    cEven = ReadNibbleAt (img, track, bitPos);

    UNREFERENCED_PARAMETER (vOdd);
    UNREFERENCED_PARAMETER (vEven);
    UNREFERENCED_PARAMETER (tOdd);
    UNREFERENCED_PARAMETER (tEven);
    UNREFERENCED_PARAMETER (cOdd);
    UNREFERENCED_PARAMETER (cEven);

    outSector = Decode44 (sOdd, sEven);

    foundProlog = 0;

    while (foundProlog == 0)
    {
        n0 = ReadNibbleAt (img, track, bitPos);
        if (n0 != kAddrProlog0)
        {
            bitsConsumed = (bitPos - startBitPos);
            if (bitsConsumed > trackBits + 8)
            {
                hr = E_FAIL;
                goto Error;
            }
            continue;
        }

        n1 = ReadNibbleAt (img, track, bitPos);
        n2 = ReadNibbleAt (img, track, bitPos);

        if (n1 == kAddrProlog1 && n2 == kDataProlog2)
        {
            foundProlog = 1;
        }
    }

    prev = 0;

    for (i = 0; i < kEncodedDataSize; i++)
    {
        raw         = ReadNibbleAt (img, track, bitPos);
        encoded[i]  = static_cast<Byte> (InverseTranslate (raw) ^ prev);
        prev        = encoded[i];
    }

    for (i = 0; i < NibblizationLayer::kSectorByteSize; i++)
    {
        Byte    high = static_cast<Byte> (encoded[kThirdGroupSize + i] << 2);
        Byte    bit0 = 0;
        Byte    bit1 = 0;
        int     idx  = 0;
        int     shift = 0;

        if (i < kThirdGroupSize)
        {
            idx   = i;
            shift = 0;
        }
        else if (i < 2 * kThirdGroupSize)
        {
            idx   = i - kThirdGroupSize;
            shift = 2;
        }
        else
        {
            idx   = i - 2 * kThirdGroupSize;
            shift = 4;
        }

        bit0 = static_cast<Byte> ((encoded[idx] >> (shift + 1)) & 1);
        bit1 = static_cast<Byte> ((encoded[idx] >> (shift + 0)) & 1);

        outData[i] = static_cast<Byte> (high | (bit0 << 0) | (bit1 << 1));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  Walk every track's bit stream and recover its 16 sectors into the
//  flat output buffer. The output layout is the inverse of the matching
//  Nibblize call: DSK/DO use the DOS interleave; PO uses the ProDOS map.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (const DiskImage & img, DiskFormat fmt, vector<Byte> & out)
{
    HRESULT       hr         = S_OK;
    const int *   interleave = nullptr;
    int           track      = 0;
    int           sec        = 0;
    Byte          outSector  = 0;
    Byte          data[kSectorByteSize] = {};
    size_t        bitPos     = 0;
    size_t        offset     = 0;

    out.assign (kImageByteSize, 0);

    switch (fmt)
    {
        case DiskFormat::Dsk: interleave = kDsk_LtoP;             break;
        case DiskFormat::Do:  interleave = kDsk_LtoP;             break;
        case DiskFormat::Po:  interleave = kPo_DosLogicalToFile;  break;
        default:
            hr = E_INVALIDARG;
            goto Error;
    }

    for (track = 0; track < kTrackCount; track++)
    {
        if (track >= img.GetTrackCount ())
        {
            break;
        }

        bitPos = 0;

        for (sec = 0; sec < kSectorsPerTrack; sec++)
        {
            HRESULT   hrSector = DecodeOneSector (img, track, bitPos, outSector, data);

            if (FAILED (hrSector))
            {
                break;
            }

            if (outSector >= kSectorsPerTrack)
            {
                continue;
            }

            offset = static_cast<size_t> (track * kSectorsPerTrack + interleave[outSector])
                   * kSectorByteSize;

            memcpy (&out[offset], data, kSectorByteSize);
        }
    }

Error:
    return hr;
}
