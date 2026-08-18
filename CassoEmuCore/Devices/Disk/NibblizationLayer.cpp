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
//  PackSyncNibbleBits
//
//  Real Disk II hardware writes a sync nibble (0xFF) as a 10-bit pattern
//  on the disk: the 8 nibble bits followed by 2 zero "sync-gap" bits. The
//  zero gap tail makes the next nibble's MSB-set bit arrive ~2 bit-times
//  later than a byte boundary would, which intentionally shifts the
//  Disk II read latch's bit alignment. After enough sync nibbles in a
//  row the latch is guaranteed to be aligned on real nibble MSBs, which
//  is how the boot ROM's $C65E loop synchronizes on the D5 prologue.
//
//  Without these gap bits, an 8-bit-only sync run is byte-aligned with
//  the rest of the bit stream so the latch can lock onto a rotation that
//  *never* contains a 0xD5 nibble -- the boot ROM spins forever.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int  kSyncTailZeros = 2;

static void PackSyncNibbleBits (vector<Byte> & dst, size_t & bitOffset)
{
    PackNibbleBits (dst, bitOffset, kSyncNibble);
    bitOffset += kSyncTailZeros;          // bits are zero-initialized
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
        PackSyncNibbleBits (dst, bitOffset);
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
    Byte    prev                      = 0;
    Byte    enc                       = 0;
    int     i                         = 0;

    for (i = 0; i < kDataPrologueGap; i++)
    {
        PackSyncNibbleBits (dst, bitOffset);
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
        // Apple disk encoding (standard DOS 3.3 6+2 data field):
        // each on-disk nibble is the encoded payload byte XOR'd with
        // the PREVIOUS raw payload byte (not with a running checksum).
        // The boot ROM's decode loop does
        //     A := A XOR inverse_translate[disk_byte]
        // which produces A_i = encoded[i] only if disk[i] = translate[
        // encoded[i] XOR encoded[i-1]] (the running A *recovers* the
        // encoded sequence, so the on-disk values must be consecutive
        // XORs of raw encoded bytes, not a running cumulative XOR).
        // The 343rd checksum nibble is just the FINAL raw encoded byte
        // (without any XOR), which makes A_final XOR inv_translate[
        // checksum] == 0 -- the boot ROM's success gate.
        enc  = static_cast<Byte> (encoded[i] ^ prev);
        prev = encoded[i];
        PackNibbleBits (dst, bitOffset, kWriteTranslate[enc & 0x3F]);
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
    size_t    rawSize   = 0;

    rawSize = raw.size();
    CBRAEx (rawSize == NibblizationLayer::kImageByteSize, E_INVALIDARG);

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

        // Trim the track to what we actually wrote so the engine wraps
        // back to the address-field sync gap (real Disk II behavior),
        // not into a run of zero bits left over from the resize.
        out.SetTrackBitCount (track, bitOffset);
    }

    out.ClearDirty();

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





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizeDo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::NibblizeDo (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, kDsk_LtoP, out);



    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Do);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizePo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::NibblizePo (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, kPo_DosLogicalToFile, out);



    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Po);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Nibblize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Nibblize (const vector<Byte> & raw, DiskFormat fmt, DiskImage & out)
{
    HRESULT  hr = S_OK;



    switch (fmt)
    {
        case DiskFormat::Dsk: hr = NibblizeDsk (raw, out); break;
        case DiskFormat::Do:  hr = NibblizeDo  (raw, out); break;
        case DiskFormat::Po:  hr = NibblizePo  (raw, out); break;
        default:              hr = E_INVALIDARG;           break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read/Find helpers for Denibblize
//
//  Reading nibbles and locating address / data marks in a track's raw
//  bitstream.
//
//  A nibble is not a byte at a known offset. The Disk II shifts bits in until
//  the MSB sets -- that is the state machine's own nibble-complete rule, and
//  it is what makes self-synchronizing sync bytes work -- so a nibble read
//  starts wherever the last one ended and takes however many bits it takes.
//
//  Every read is bounded by ONE REVOLUTION. A track carrying no sync bytes
//  never sets the MSB, and without the bound the search would spin forever on
//  a blank or corrupt track; 0 is the caller's "no nibble".
//
//  The overrun test deliberately sits AFTER the read and outranks a completed
//  nibble. A value whose MSB set on the very read that crossed the revolution
//  boundary is discarded, because its high bits came from before the wrap and
//  the nibble is a splice of two places on the track.
//
//  The bit position is taken by REFERENCE and advanced, so a caller scanning
//  for a mark walks the track continuously instead of restarting each time.
//
////////////////////////////////////////////////////////////////////////////////

static Byte ReadNibbleAt (const DiskImage & img, int track, size_t & bitPos)
{
    size_t   trackBits = img.GetTrackBitCount (track);
    Byte     value     = 0;
    Byte     bit       = 0;
    size_t   start     = bitPos;
    bool     overran   = false;



    // Shift in bits until the MSB sets (the LSS's own nibble-complete rule).
    // A full revolution with no MSB means the track carries no sync bytes, so
    // give up rather than spin forever -- 0 is the caller's "no nibble".
    //
    // The overrun test stays AFTER the read and outranks a completed nibble:
    // a value whose MSB set on the very read that crossed the revolution
    // boundary is discarded, because its high bits came from before the wrap.
    if (trackBits != 0)
    {
        while ((value & 0x80) == 0 && !overran)
        {
            bit    = img.ReadBit (track, bitPos % trackBits);
            bitPos++;
            value  = static_cast<Byte> ((value << 1) | (bit & 1));

            overran = (bitPos - start > trackBits);
        }

        if (overran)
        {
            value = 0;
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Decode44
//
////////////////////////////////////////////////////////////////////////////////

static Byte Decode44 (Byte odd, Byte even)
{
    return static_cast<Byte> (((odd << 1) | 1) & even);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InverseTranslate
//
////////////////////////////////////////////////////////////////////////////////

static Byte InverseTranslate (Byte nib)
{
    int   i     = 0;
    Byte  value = 0xFF;      // 0xFF == not a legal 6-and-2 nibble



    for (i = 0; i < 64 && value == 0xFF; i++)
    {
        if (kWriteTranslate[i] == nib)
        {
            value = static_cast<Byte> (i);
        }
    }

    return value;
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
    HRESULT   hr                        = S_OK;
    Byte      n0                        = 0;
    Byte      n1                        = 0;
    Byte      n2                        = 0;
    Byte      vOdd                      = 0;
    Byte      vEven                     = 0;
    Byte      tOdd                      = 0;
    Byte      tEven                     = 0;
    Byte      sOdd                      = 0;
    Byte      sEven                     = 0;
    Byte      cOdd                      = 0;
    Byte      cEven                     = 0;
    Byte      foundProlog               = 0;
    Byte      encoded[kEncodedDataSize] = {};
    Byte      prev                      = 0;
    Byte      raw                       = 0;
    int       i                         = 0;
    size_t    trackBits                 = img.GetTrackBitCount (track);
    size_t    startBitPos               = bitPos;
    size_t    bitsConsumed              = 0;

    CBR (trackBits != 0);

    foundProlog = 0;

    while (foundProlog == 0)
    {
        n0 = ReadNibbleAt (img, track, bitPos);
        if (n0 != kAddrProlog0)
        {
            bitsConsumed = (bitPos - startBitPos);
            CBR (bitsConsumed <= trackBits + 8);
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
            CBR (bitsConsumed <= trackBits + 8);
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
        // Inverse of the standard Apple DOS 3.3 6+2 convention used in
        // the writer above: on-disk nibbles encode encoded[i] XOR'd
        // with the previous raw encoded value, so we recover raw
        // values via XOR with the previous DECODED value.
        raw         = ReadNibbleAt (img, track, bitPos);
        encoded[i]  = static_cast<Byte> (InverseTranslate (raw) ^ prev);
        prev        = encoded[i];
    }

    for (i = 0; i < NibblizationLayer::kSectorByteSize; i++)
    {
        Byte    high  = static_cast<Byte> (encoded[kThirdGroupSize + i] << 2);
        Byte    bit0  = 0;
        Byte    bit1  = 0;
        int     idx   = 0;
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
    DenibblizeReport  ignored;



    return Denibblize (img, fmt, out, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  Decode every track's bit stream back to a plain 143,360-byte sector image.
//
//  What used to lose data was the SILENCE, not the early stop. This returned
//  S_OK over a track it had only partly decoded, and the flush path wrote that
//  buffer over the user's file -- so a damaged track was saved as a mixture of
//  real sectors, zeroed sectors and wrong sectors, reported as a clean save.
//
//  Measured, on an image with one sector's data field destroyed: that sector
//  comes back as zeros, AND a second sector comes back holding the wrong data,
//  because the scan for the missing data field runs on and finds the NEXT
//  sector's, storing it under the sector number the address field gave. Two
//  sectors wrong from one point of damage, and nothing said so.
//
//  So the fix that matters is the report, and a partial track failing the whole
//  operation: the zeros and the misfiled sectors are lost data rather than
//  blank media, and refusing to save is the only answer that destroys nothing.
//  The caller keeps the image dirty and the existing file intact.
//
//  A track that decodes nothing at all is a different case and still succeeds:
//  an unformatted track in a sector image legitimately IS zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (
    const DiskImage   &  img,
    DiskFormat           fmt,
    vector<Byte>      &  out,
    DenibblizeReport  &  report)
{
    // Enough attempts to still find all sixteen good sectors when several
    // reads fail or repeat, without letting a hostile track spin forever.
    constexpr int  kMaxAttemptsPerTrack = kSectorsPerTrack * 2;
    constexpr int  kAllSectorsMask      = (1 << kSectorsPerTrack) - 1;

    HRESULT       hr                    = S_OK;
    const int *   interleave            = nullptr;
    int           track                 = 0;
    int           attempt               = 0;
    int           decoded               = 0;
    int           bit                   = 0;
    uint16_t      mask                  = 0;
    Byte          outSector             = 0;
    Byte          data[kSectorByteSize] = {};
    size_t        bitPos                = 0;
    size_t        offset                = 0;
    size_t        trackBits             = 0;
    int           trackLimit            = img.GetTrackCount();
    bool          lostSectors           = false;



    out.assign (kImageByteSize, 0);

    report = DenibblizeReport();

    switch (fmt)
    {
        case DiskFormat::Dsk: interleave = kDsk_LtoP;             break;
        case DiskFormat::Do:  interleave = kDsk_LtoP;             break;
        case DiskFormat::Po:  interleave = kPo_DosLogicalToFile;  break;
        default:              hr         = E_INVALIDARG;          break;
    }

    CHR (hr);

    if (trackLimit > kTrackCount)
    {
        trackLimit = kTrackCount;
    }

    report.decodedSectorMask.assign (static_cast<size_t> (trackLimit), 0);

    for (track = 0; track < trackLimit; track++)
    {
        bitPos    = 0;
        mask      = 0;
        trackBits = img.GetTrackBitCount (track);

        for (attempt = 0; attempt < kMaxAttemptsPerTrack && mask != kAllSectorsMask; attempt++)
        {
            HRESULT   hrSector = DecodeOneSector (img, track, bitPos, outSector, data);

            // Keep scanning rather than abandoning the track. This changes
            // nothing today -- DecodeOneSector only fails after sweeping the
            // whole track without finding a prolog, and from that position
            // every later attempt fails identically, which is why the `break`
            // this replaces was never the mechanism of loss. It matters the
            // moment the decoder gains a per-sector failure mode (a checksum
            // check, say), when abandoning the track really would cost the
            // sectors after the damaged one.
            if (FAILED (hrSector))
            {
                continue;
            }

            if (outSector >= kSectorsPerTrack)
            {
                continue;
            }

            offset = static_cast<size_t> (track * kSectorsPerTrack + interleave[outSector])
                   * kSectorByteSize;

            memcpy (&out[offset], data, kSectorByteSize);
            mask = static_cast<uint16_t> (mask | (1 << outSector));
        }

        report.decodedSectorMask[static_cast<size_t> (track)] = mask;

        if (trackBits == 0)
        {
            continue;
        }

        decoded = 0;

        for (bit = 0; bit < kSectorsPerTrack; bit++)
        {
            if ((mask & (1 << bit)) != 0)
            {
                decoded++;
            }
        }

        report.tracksPresent++;
        report.sectorsDecoded += decoded;

        if (decoded == 0)
        {
            report.tracksUnformatted++;
        }
        else if (decoded < kSectorsPerTrack)
        {
            report.tracksPartial++;
            report.sectorsMissing += kSectorsPerTrack - decoded;
        }
        else
        {
            report.tracksComplete++;
        }
    }

    // A half-decoded track means the output holds zeros and misfiled sectors
    // where the user's data should be. Saying so is the whole point: the
    // caller writes this buffer over their file, and once it lands nothing
    // distinguishes it from a clean save.
    lostSectors = report.HasPartialTrack();
    CBR (!lostSectors);

Error:
    return hr;
}
