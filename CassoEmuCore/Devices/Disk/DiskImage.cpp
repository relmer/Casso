#include "Pch.h"

#include "DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DOS 3.3 sector interleave table (logical → physical)
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int kDos33Interleave[16] =
{
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};





////////////////////////////////////////////////////////////////////////////////
//
//  6-and-2 write translate table (64 entries)
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


static constexpr Byte    kVolumeNumber  = 254;
static constexpr int     kSectorsPerTrack = 16;
static constexpr int     kSectorByteSize  = 256;
static constexpr int     kEncodedDataSize = 342;
static constexpr int     kBitsPerNibble   = 8;





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
//  Append 8 bits (MSB-first) of `nibble` to a packed bit-stream byte
//  vector at the given bit offset. Caller pre-sized the destination.
//
////////////////////////////////////////////////////////////////////////////////

static void PackNibbleBits (vector<Byte> & dst, size_t & bitOffset, Byte nibble)
{
    int  bit       = 0;
    Byte mask      = 0;
    Byte b         = 0;

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
//  DiskImage::DiskImage
//
//  Construct an empty disk: 35 tracks of zero-length bit streams. Load()
//  resizes individual tracks to hold their nibblized bit data.
//
////////////////////////////////////////////////////////////////////////////////

DiskImage::DiskImage ()
{
    m_trackBits.resize      (kDefaultTrackCount);
    m_trackBitCounts.resize (kDefaultTrackCount, 0);
    m_trackDirty.resize     (kDefaultTrackCount, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTrackCount
//
////////////////////////////////////////////////////////////////////////////////

int DiskImage::GetTrackCount () const
{
    return static_cast<int> (m_trackBits.size ());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTrackBitCount
//
////////////////////////////////////////////////////////////////////////////////

size_t DiskImage::GetTrackBitCount (int track) const
{
    if (track < 0 || track >= static_cast<int> (m_trackBitCounts.size ()))
    {
        return 0;
    }

    return m_trackBitCounts[track];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadBit
//
////////////////////////////////////////////////////////////////////////////////

uint8_t DiskImage::ReadBit (int track, size_t bitIndex) const
{
    size_t   trackBits = 0;
    size_t   byteIdx   = 0;
    int      shift     = 0;

    if (track < 0 || track >= static_cast<int> (m_trackBits.size ()))
    {
        return 0;
    }

    trackBits = m_trackBitCounts[track];

    if (trackBits == 0)
    {
        return 0;
    }

    byteIdx = (bitIndex % trackBits) >> 3;
    shift   = 7 - static_cast<int> ((bitIndex % trackBits) & 7);

    return static_cast<uint8_t> ((m_trackBits[track][byteIdx] >> shift) & 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBit
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::WriteBit (int track, size_t bitIndex, uint8_t bit)
{
    size_t   trackBits = 0;
    size_t   byteIdx   = 0;
    int      shift     = 0;
    Byte     mask      = 0;

    if (track < 0 || track >= static_cast<int> (m_trackBits.size ()))
    {
        return;
    }

    if (m_writeProtected)
    {
        return;
    }

    trackBits = m_trackBitCounts[track];

    if (trackBits == 0)
    {
        return;
    }

    byteIdx = (bitIndex % trackBits) >> 3;
    shift   = 7 - static_cast<int> ((bitIndex % trackBits) & 7);
    mask    = static_cast<Byte> (1 << shift);

    if (bit & 1)
    {
        m_trackBits[track][byteIdx] = static_cast<Byte> (m_trackBits[track][byteIdx] | mask);
    }
    else
    {
        m_trackBits[track][byteIdx] = static_cast<Byte> (m_trackBits[track][byteIdx] & ~mask);
    }

    m_trackDirty[track] = true;
    m_dirty             = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDirty / IsWriteProtected / GetSourceFormat / IsTrackDirty / ClearDirty
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImage::IsDirty () const
{
    return m_dirty;
}


bool DiskImage::IsWriteProtected () const
{
    return m_writeProtected;
}


DiskFormat DiskImage::GetSourceFormat () const
{
    return m_format;
}


bool DiskImage::IsTrackDirty (int track) const
{
    if (track < 0 || track >= static_cast<int> (m_trackDirty.size ()))
    {
        return false;
    }

    return m_trackDirty[track];
}


void DiskImage::ClearDirty ()
{
    size_t   i = 0;

    m_dirty = false;

    for (i = 0; i < m_trackDirty.size (); i++)
    {
        m_trackDirty[i] = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Serialize
//
//  Phase 10 implements full WOZ / .DSK / .DO / .PO writeback via
//  NibblizationLayer. Phase 9 stub returns the cached raw source bytes
//  if available so existing Flush() round-trips through the original
//  bytes (Phase 8 behavior); otherwise reports E_NOTIMPL.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::Serialize (vector<Byte> & outBytes) const
{
    HRESULT   hr = S_OK;

    if (m_rawSourceBytes.empty ())
    {
        hr = E_NOTIMPL;
        goto Error;
    }

    outBytes = m_rawSourceBytes;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResizeTrack
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::ResizeTrack (int track, size_t bitCount)
{
    size_t   bytesNeeded = 0;

    if (track < 0 || track >= static_cast<int> (m_trackBits.size ()))
    {
        return;
    }

    bytesNeeded = (bitCount + 7) / 8;

    m_trackBits[track].assign (bytesNeeded, 0);
    m_trackBitCounts[track] = bitCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetLoadedForTest
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::SetLoadedForTest (bool loaded, bool dirty)
{
    m_loaded = loaded;
    m_dirty  = dirty;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
//  Reads a 143360-byte .dsk image from the host filesystem and synthesizes
//  per-track bit streams via 6+2 GCR nibblization (DOS 3.3 interleave).
//  Phase 10 will move the nibblization to a dedicated NibblizationLayer
//  and add WOZ / .DO / .PO support.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::Load (const string & filePath)
{
    HRESULT       hr        = S_OK;
    streamsize    bytesRead = 0;
    bool          fileOk    = false;
    vector<Byte>  raw;

    {
        ifstream file (filePath, ios::binary);
        fileOk = file.good ();
        CBREx (fileOk, E_FAIL);

        raw.resize (kDos33ImageSize);
        file.read (reinterpret_cast<char *> (raw.data ()), kDos33ImageSize);
        bytesRead = file.gcount ();
        CBREx (bytesRead == kDos33ImageSize, E_FAIL);
    }

    hr = LoadDsk (raw);
    CHR (hr);

    m_filePath       = filePath;
    m_loaded         = true;
    m_dirty          = false;
    m_format         = DiskFormat::Dsk;
    m_rawSourceBytes = move (raw);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadDsk
//
//  Synthesizes per-track packed bit streams from a 143360-byte .dsk
//  blob. Each track produces ~6400 nibble bytes → 51200 packed bits.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::LoadDsk (const vector<Byte> & raw)
{
    HRESULT   hr      = S_OK;
    int       track   = 0;
    int       sector  = 0;
    size_t    offset  = 0;

    if (raw.size () != kDos33ImageSize)
    {
        hr = E_FAIL;
        goto Error;
    }

    m_trackBits.assign      (kDefaultTrackCount, vector<Byte> ());
    m_trackBitCounts.assign (kDefaultTrackCount, 0);
    m_trackDirty.assign     (kDefaultTrackCount, false);

    for (track = 0; track < kDefaultTrackCount; track++)
    {
        Byte     sectorData[kSectorByteSize];
        size_t   bitOffset = 0;

        ResizeTrack (track, kDefaultTrackByteSize * kBitsPerNibble);

        for (sector = 0; sector < kSectorsPerTrack; sector++)
        {
            int  physical = kDos33Interleave[sector];

            offset = static_cast<size_t> (track * kSectorsPerTrack + physical) * kSectorByteSize;

            memcpy (sectorData, &raw[offset], kSectorByteSize);
            NibblizeTrackToBits (track, bitOffset, static_cast<Byte> (sector), sectorData, kVolumeNumber);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizeTrackToBits
//
//  Append one sector's worth of nibbles (gap, address field, data field,
//  epilogue) to the track's packed bit stream. Caller iterates over the
//  16 sectors per track and pre-resizes the track byte vector.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::NibblizeTrackToBits (
    int           track,
    size_t      & bitOffset,
    Byte          sectorIdx,
    const Byte  * sectorData,
    Byte          volume)
{
    Byte    encoded[kEncodedDataSize] = {};
    Byte    checksum  = 0;
    int     i         = 0;
    Byte    prev      = 0;
    Byte    enc       = 0;

    checksum = static_cast<Byte> (volume ^ track ^ sectorIdx);

    for (i = 0; i < 20; i++)
    {
        PackNibbleBits (m_trackBits[track], bitOffset, 0xFF);
    }

    PackNibbleBits (m_trackBits[track], bitOffset, 0xD5);
    PackNibbleBits (m_trackBits[track], bitOffset, 0xAA);
    PackNibbleBits (m_trackBits[track], bitOffset, 0x96);
    PackNibbleBits (m_trackBits[track], bitOffset, Encode44Odd  (volume));
    PackNibbleBits (m_trackBits[track], bitOffset, Encode44Even (volume));
    PackNibbleBits (m_trackBits[track], bitOffset, Encode44Odd  (static_cast<Byte> (track)));
    PackNibbleBits (m_trackBits[track], bitOffset, Encode44Even (static_cast<Byte> (track)));
    PackNibbleBits (m_trackBits[track], bitOffset, Encode44Odd  (sectorIdx));
    PackNibbleBits (m_trackBits[track], bitOffset, Encode44Even (sectorIdx));
    PackNibbleBits (m_trackBits[track], bitOffset, Encode44Odd  (checksum));
    PackNibbleBits (m_trackBits[track], bitOffset, Encode44Even (checksum));
    PackNibbleBits (m_trackBits[track], bitOffset, 0xDE);
    PackNibbleBits (m_trackBits[track], bitOffset, 0xAA);
    PackNibbleBits (m_trackBits[track], bitOffset, 0xEB);

    for (i = 0; i < 6; i++)
    {
        PackNibbleBits (m_trackBits[track], bitOffset, 0xFF);
    }

    PackNibbleBits (m_trackBits[track], bitOffset, 0xD5);
    PackNibbleBits (m_trackBits[track], bitOffset, 0xAA);
    PackNibbleBits (m_trackBits[track], bitOffset, 0xAD);

    for (i = 0; i < 86; i++)
    {
        Byte   v = 0;

        v = static_cast<Byte> (
                ((sectorData[i] & 0x01) << 1) |
                ((sectorData[i] & 0x02) >> 1));

        if (i + 86 < kSectorByteSize)
        {
            v |= static_cast<Byte> (
                    ((sectorData[i + 86] & 0x01) << 3) |
                    ((sectorData[i + 86] & 0x02) << 1));
        }

        if (i + 172 < kSectorByteSize)
        {
            v |= static_cast<Byte> (
                    ((sectorData[i + 172] & 0x01) << 5) |
                    ((sectorData[i + 172] & 0x02) << 3));
        }

        encoded[i] = v;
    }

    for (i = 0; i < kSectorByteSize; i++)
    {
        encoded[86 + i] = static_cast<Byte> (sectorData[i] >> 2);
    }

    prev = 0;

    for (i = 0; i < kEncodedDataSize; i++)
    {
        enc  = static_cast<Byte> (encoded[i] ^ prev);
        PackNibbleBits (m_trackBits[track], bitOffset, kWriteTranslate[enc & 0x3F]);
        prev = encoded[i];
    }

    PackNibbleBits (m_trackBits[track], bitOffset, kWriteTranslate[prev & 0x3F]);
    PackNibbleBits (m_trackBits[track], bitOffset, 0xDE);
    PackNibbleBits (m_trackBits[track], bitOffset, 0xAA);
    PackNibbleBits (m_trackBits[track], bitOffset, 0xEB);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::Eject ()
{
    HRESULT   hr = S_OK;

    if (m_dirty && !m_writeProtected)
    {
        hr = Flush ();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_filePath.clear ();
    m_rawSourceBytes.clear ();
    m_trackBits.assign      (kDefaultTrackCount, vector<Byte> ());
    m_trackBitCounts.assign (kDefaultTrackCount, 0);
    m_trackDirty.assign     (kDefaultTrackCount, false);
    m_loaded = false;
    m_dirty  = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flush
//
//  Phase 9: writes back the cached raw source bytes if any (preserves
//  Phase 8 round-trip for .dsk). Phase 10 will replace this with a true
//  bit-stream → sector denibblization via NibblizationLayer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::Flush ()
{
    HRESULT   hr     = S_OK;
    bool      fileOk = false;

    if (!m_dirty || m_filePath.empty () || m_rawSourceBytes.empty ())
    {
        m_dirty = false;
        goto Error;
    }

    {
        ofstream file (m_filePath, ios::binary);
        fileOk = file.good ();
        CBREx (fileOk, E_FAIL);

        file.write (reinterpret_cast<const char *> (m_rawSourceBytes.data ()),
                    static_cast<streamsize> (m_rawSourceBytes.size ()));
    }

    m_dirty = false;
    ClearDirty ();

Error:
    return hr;
}
