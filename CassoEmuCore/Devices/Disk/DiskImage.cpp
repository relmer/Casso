#include "Pch.h"

#include "DiskImage.h"
#include "NibblizationLayer.h"
#include "WozLoader.h"





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
//  For sector-based formats (DSK/DO/PO) this calls back into the
//  NibblizationLayer to recover the flat sector image. For WOZ images we
//  fall back to the cached raw source bytes (Phase 10 does not yet
//  generate WOZ output from edited bit streams).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::Serialize (vector<Byte> & outBytes) const
{
    HRESULT   hr = S_OK;

    switch (m_format)
    {
        case DiskFormat::Dsk:
        case DiskFormat::Do:
        case DiskFormat::Po:
            hr = NibblizationLayer::Denibblize (*this, m_format, outBytes);
            break;

        case DiskFormat::Woz:
            if (m_rawSourceBytes.empty ())
            {
                hr = E_NOTIMPL;
            }
            else
            {
                outBytes = m_rawSourceBytes;
            }
            break;

        default:
            hr = E_NOTIMPL;
            break;
    }

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
//  SetTrackBitCount
//
//  Used by bulk loaders that hand-pack the byte buffer (WozLoader). The
//  caller already filled m_trackBits[track] via GetTrackBitsForWrite or a
//  swap; this records the bit-count so ReadBit/WriteBit honor the actual
//  track length.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::SetTrackBitCount (int track, size_t bitCount)
{
    if (track < 0 || track >= static_cast<int> (m_trackBitCounts.size ()))
    {
        return;
    }

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
//  LoadFromBytes
//
//  In-memory load entry point used by DiskImageStore and unit tests. The
//  raw bytes are routed through the appropriate loader (NibblizationLayer
//  for DSK/DO/PO; WozLoader for WOZ). The cached raw bytes preserve the
//  Phase 9 round-trip path until WOZ writeback lands.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::LoadFromBytes (DiskFormat fmt, const vector<Byte> & raw, const string & sourcePath)
{
    HRESULT   hr = S_OK;

    m_filePath       = sourcePath;
    m_format         = fmt;
    m_loaded         = false;
    m_dirty          = false;
    m_rawSourceBytes = raw;

    switch (fmt)
    {
        case DiskFormat::Dsk:
        case DiskFormat::Do:
        case DiskFormat::Po:
            hr = NibblizationLayer::Nibblize (raw, fmt, *this);
            break;

        case DiskFormat::Woz:
            hr = WozLoader::Load (raw, *this);
            break;

        default:
            hr = E_INVALIDARG;
            break;
    }

    if (SUCCEEDED (hr))
    {
        m_loaded = true;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
//  Reads a disk image from the host filesystem. The .dsk path remains the
//  primary route used by the controller; full extension routing lives in
//  DiskImageStore::Mount.
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
//  Thin shim — delegates to NibblizationLayer::NibblizeDsk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::LoadDsk (const vector<Byte> & raw)
{
    return NibblizationLayer::NibblizeDsk (raw, *this);
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
//  Serializes dirty track state back to the original file using the
//  appropriate inverse path. WOZ and any source without a known on-disk
//  path no-op (cached raw bytes are written verbatim if present).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::Flush ()
{
    HRESULT       hr      = S_OK;
    bool          fileOk  = false;
    vector<Byte>  bytes;

    if (!m_dirty)
    {
        goto Error;
    }

    if (m_filePath.empty ())
    {
        m_dirty = false;
        ClearDirty ();
        goto Error;
    }

    hr = Serialize (bytes);

    if (FAILED (hr))
    {
        if (m_rawSourceBytes.empty ())
        {
            goto Error;
        }
        bytes = m_rawSourceBytes;
        hr    = S_OK;
    }

    {
        ofstream file (m_filePath, ios::binary);
        fileOk = file.good ();
        CBREx (fileOk, E_FAIL);

        file.write (reinterpret_cast<const char *> (bytes.data ()),
                    static_cast<streamsize> (bytes.size ()));
    }

    m_dirty = false;
    ClearDirty ();

Error:
    return hr;
}
