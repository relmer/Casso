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
//  NibblizationLayer::RenibblizeTracks
//
//  Re-encodes only the named tracks, leaving every other track's packed bits
//  exactly as they are.
//
//  This is what makes a write to a bit-stream image safe. Re-encoding the whole
//  image would replace all 35 tracks with freshly synthesized standard nibbles,
//  discarding timing, sync patterns, and weak bits everywhere -- including on
//  tracks the operation never touched. A 512-byte file has no business
//  rewriting a disk.
//
//  Tracks outside the image are skipped rather than treated as an error: the
//  caller names the tracks its edit landed on, and an image shorter than the
//  nominal geometry simply has nothing at those positions.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::RenibblizeTracks (
    const vector<Byte>    &  sectors,
    DiskFormat               fmt,
    std::span<const int>     tracks,
    DiskImage             &  inOutImage)
{
    HRESULT       hr         = S_OK;
    const int *   interleave = nullptr;
    size_t        rawSize    = sectors.size();
    size_t        i          = 0;
    int           track      = 0;
    int           logical    = 0;
    int           trackCount = inOutImage.GetTrackCount();

    CBRAEx (rawSize == (size_t) kImageByteSize, E_INVALIDARG);

    switch (fmt)
    {
        case DiskFormat::Dsk: interleave = kDsk_LtoP;             break;
        case DiskFormat::Do:  interleave = kDsk_LtoP;             break;
        case DiskFormat::Po:  interleave = kPo_DosLogicalToFile;  break;
        case DiskFormat::Woz: interleave = kDsk_LtoP;             break;
        default:              hr         = E_INVALIDARG;          break;
    }

    CHR (hr);

    for (i = 0; i < tracks.size(); i++)
    {
        size_t  bitOffset = 0;

        track = tracks[i];

        if (track < 0 || track >= kTrackCount || track >= trackCount)
        {
            continue;
        }

        inOutImage.ResizeTrack (track, kTrackBitCapacity);

        for (logical = 0; logical < kSectorsPerTrack; logical++)
        {
            size_t  offset = static_cast<size_t> (track * kSectorsPerTrack + interleave[logical])
                           * kSectorByteSize;

            AppendAddressField (inOutImage.GetTrackBitsForWrite (track), bitOffset,
                                kDefaultVolume,
                                static_cast<Byte> (track),
                                static_cast<Byte> (logical));
            AppendDataField    (inOutImage.GetTrackBitsForWrite (track), bitOffset, &sectors[offset]);
        }

        inOutImage.SetTrackBitCount (track, bitOffset);
    }

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
//  outSawAddressField reports whether an address prologue was located at all,
//  which is what separates a blank track from a damaged one. It is set even
//  when the call goes on to fail, because "found a header then lost the data"
//  is damage while "found no header anywhere" is empty space.
//
//  The address checksum is verified rather than ignored. Without it a corrupt
//  header yields a plausible but wrong sector number, and the caller then files
//  the payload under the wrong logical sector -- silent corruption rather than
//  a reported failure. A mismatch fails the call with the cursor left past the
//  bad header, so the caller resynchronizes on the next one.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT DecodeOneSector (
    const DiskImage   &  img,
    int                  track,
    size_t            &  bitPos,
    Byte              &  outSector,
    Byte              *  outData,
    bool              &  outSawAddressField,
    size_t            &  outFieldStart)
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
    Byte      volume                    = 0;
    Byte      addrTrack                 = 0;
    Byte      checksum                  = 0;
    Byte      foundProlog               = 0;
    Byte      encoded[kEncodedDataSize] = {};
    Byte      prev                      = 0;
    Byte      raw                       = 0;
    int       i                         = 0;
    size_t    trackBits                 = img.GetTrackBitCount (track);
    size_t    startBitPos               = bitPos;
    size_t    bitsConsumed              = 0;
    bool      checksumOk                = false;

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

    outSawAddressField = true;
    outFieldStart      = bitPos;

    vOdd  = ReadNibbleAt (img, track, bitPos);
    vEven = ReadNibbleAt (img, track, bitPos);
    tOdd  = ReadNibbleAt (img, track, bitPos);
    tEven = ReadNibbleAt (img, track, bitPos);
    sOdd  = ReadNibbleAt (img, track, bitPos);
    sEven = ReadNibbleAt (img, track, bitPos);
    cOdd  = ReadNibbleAt (img, track, bitPos);
    cEven = ReadNibbleAt (img, track, bitPos);

    volume    = Decode44 (vOdd, vEven);
    addrTrack = Decode44 (tOdd, tEven);
    outSector = Decode44 (sOdd, sEven);
    checksum  = Decode44 (cOdd, cEven);

    // The writer emits volume ^ track ^ sector; a header that does not agree
    // is not a header we may act on.
    checksumOk = checksum == (Byte) (volume ^ addrTrack ^ outSector);
    CBR (checksumOk);

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
//  ClassifyTrack
//
//  Two independent questions, and neither substitutes for the other. Whether
//  any address field was found separates a blank track from a damaged one;
//  whether coverage is complete separates an intact track from a lossy one.
//
//  Completeness requires every slot filled EXACTLY once, not merely filled.
//  The duplicate flag is what makes that stronger test real: without it the
//  answer would depend on how many attempts the scan is allowed, which is
//  exactly the kind of incidental bound an invariant should not rest on.
//
////////////////////////////////////////////////////////////////////////////////

static TrackDecodeOutcome ClassifyTrack (bool sawAddressField, Word coverage, bool duplicated)
{
    TrackDecodeOutcome  outcome = TrackDecodeOutcome::Partial;
    bool                intact  = coverage == SectorDecodeReport::kFullCoverage && !duplicated;



    if (!sawAddressField)
    {
        outcome = TrackDecodeOutcome::Unformatted;
    }
    else if (intact)
    {
        outcome = TrackDecodeOutcome::Complete;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  Walk every track's bit stream and recover its 16 sectors into the
//  flat output buffer. The output layout is the inverse of the matching
//  Nibblize call: DSK/DO use the DOS interleave; PO uses the ProDOS map.
//
//  This overload reports what each track actually yielded. Reporting is not a
//  refinement: without it a sector the decoder could not read is indistinguish-
//  able from a sector genuinely full of zeros, and a write path built over that
//  silently overwrites what it failed to read.
//
//  The scan continues past a bad sector and resynchronizes on the next address
//  prologue rather than abandoning the track, and the outcome is decided by a
//  COVERAGE mask rather than by how the loop exited. Three separate mechanisms
//  leave a logical slot unfilled -- a failed decode, a sector number outside the
//  valid range, and two sectors claiming the same number -- and only the first
//  involves a failure at all. Coverage catches all three with one test.
//
//  The scan covers exactly ONE revolution. The cursor advances monotonically
//  while reads wrap, so stopping at a revolution's worth of bits visits each
//  physical sector once. Scanning further would re-read sectors already
//  recovered and report them as duplicates -- damage invented by the scan
//  rather than found on the disk. An attempt cap backstops the bound in case a
//  future decoder consumes no bits on some path.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (
    const DiskImage     &  img,
    DiskFormat             fmt,
    vector<Byte>        &  out,
    SectorDecodeReport  &  outReport)
{
    //  Backstop only -- the revolution bound below is what normally ends the
    //  scan. A standard track carries 16 sectors; twice that leaves room to
    //  resynchronize past damaged headers.
    constexpr int  kMaxAttemptsPerTrack = kSectorsPerTrack * 2;

    HRESULT       hr                    = S_OK;
    const int *   interleave            = nullptr;
    int           trackCount            = img.GetTrackCount();
    int           track                 = 0;
    int           attempt               = 0;
    Byte          outSector             = 0;
    Byte          data[kSectorByteSize] = {};
    size_t        bitPos                = 0;
    size_t        offset                = 0;
    size_t        trackBits             = 0;
    size_t        fieldStart            = 0;
    Word          coverage              = 0;
    bool          duplicated            = false;
    bool          sawAddressField       = false;
    bool          sawAnyAddressField    = false;



    out.assign (kImageByteSize, 0);
    outReport.Reset (kTrackCount);

    switch (fmt)
    {
        case DiskFormat::Dsk: interleave = kDsk_LtoP;             break;
        case DiskFormat::Do:  interleave = kDsk_LtoP;             break;
        case DiskFormat::Po:  interleave = kPo_DosLogicalToFile;  break;
        default:              hr         = E_INVALIDARG;          break;
    }

    CHR (hr);

    for (track = 0; track < kTrackCount && track < trackCount; track++)
    {
        bitPos             = 0;
        coverage           = 0;
        duplicated         = false;
        sawAnyAddressField = false;

        trackBits = img.GetTrackBitCount (track);

        for (attempt = 0; attempt < kMaxAttemptsPerTrack; attempt++)
        {
            HRESULT  hrSector = S_OK;
            Word     bit      = 0;
            int      slot     = 0;

            if (coverage == SectorDecodeReport::kFullCoverage || bitPos >= trackBits)
            {
                break;
            }

            sawAddressField = false;
            fieldStart      = 0;
            hrSector        = DecodeOneSector (img, track, bitPos, outSector, data,
                                               sawAddressField, fieldStart);

            sawAnyAddressField = sawAnyAddressField || sawAddressField;

            // One revolution, and no further. The cursor can still sit inside
            // the gap that trails the last sector, so the bound has to be tested
            // where the HEADER was found, not where the cursor happens to be --
            // otherwise one more attempt wraps onto a header already recovered
            // and reports it as a duplicate the disk does not have.
            if (sawAddressField && fieldStart >= trackBits)
            {
                break;
            }

            // A failed decode does not end the track -- the cursor has moved
            // past the damage, so the next header is still reachable.
            if (FAILED (hrSector))
            {
                continue;
            }

            // A sector number the geometry cannot hold is not a slot we can
            // fill; the track is short one either way.
            if (outSector >= kSectorsPerTrack)
            {
                continue;
            }

            // Coverage is indexed by the slot in the OUTPUT buffer, not by the
            // number the address field carried. Every consumer of this report
            // holds the flat buffer, so "sector 3 was recovered" has to mean
            // the same sector 3 they can index -- the interleave between the
            // two is this layer's business, not theirs.
            slot = interleave[outSector];
            bit  = (Word) (1u << slot);

            // Filling a slot twice is a coverage violation in its own right:
            // the second copy overwrites the first and some other slot went
            // unclaimed to pay for it.
            if ((coverage & bit) != 0)
            {
                duplicated = true;
                continue;
            }

            coverage = (Word) (coverage | bit);

            offset = static_cast<size_t> (track * kSectorsPerTrack + slot)
                   * kSectorByteSize;

            memcpy (&out[offset], data, kSectorByteSize);
        }

        outReport.SetOutcome (track, ClassifyTrack (sawAnyAddressField, coverage, duplicated),
                              coverage, duplicated);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize (reporting-free overload)
//
//  Kept for every caller that does not want the report -- but deliberately NOT
//  a passthrough. It forwards to the reporting form and FAILS when the report
//  shows data loss, so no caller can obtain a silently truncated buffer by
//  choosing the simpler-looking signature. An Unformatted track still succeeds:
//  a blank track really is all zeros, and refusing one would make blank and
//  newly formatted media unreadable.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (const DiskImage & img, DiskFormat fmt, vector<Byte> & out)
{
    HRESULT             hr      = S_OK;
    bool                lostAny = false;
    SectorDecodeReport  report;



    hr = Denibblize (img, fmt, out, report);
    CHR (hr);

    lostAny = report.HasDataLoss();
    CBR (!lostAny);

Error:
    return hr;
}
