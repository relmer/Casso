#include "Pch.h"

#include "WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WOZ format constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte    kSigV1[]       = { 'W', 'O', 'Z', '1', 0xFF, 0x0A, 0x0D, 0x0A };
static constexpr Byte    kSigV2[]       = { 'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A };
static constexpr size_t  kSigLen        = 8;
static constexpr size_t  kCrcLen        = 4;
static constexpr Byte    kInfoMagic[]   = { 'I', 'N', 'F', 'O' };
static constexpr Byte    kTmapMagic[]   = { 'T', 'M', 'A', 'P' };
static constexpr Byte    kTrksMagic[]   = { 'T', 'R', 'K', 'S' };
static constexpr Byte    kMetaMagic[]   = { 'M', 'E', 'T', 'A' };
static constexpr Byte    kTmapEmptyTrack = 0xFF;
static constexpr int     kQuarterTracksPerTrack = 4;
static constexpr int     kMaxTracks      = 40;





////////////////////////////////////////////////////////////////////////////////
//
//  Read16LE / Read32LE
//
////////////////////////////////////////////////////////////////////////////////

static uint16_t Read16LE (const Byte * p)
{
    return static_cast<uint16_t> (p[0] | (p[1] << 8));
}


static uint32_t Read32LE (const Byte * p)
{
    return static_cast<uint32_t> (p[0]
        | (static_cast<uint32_t> (p[1]) << 8)
        | (static_cast<uint32_t> (p[2]) << 16)
        | (static_cast<uint32_t> (p[3]) << 24));
}


static void Write16LE (Byte * p, uint16_t v)
{
    p[0] = static_cast<Byte> (v        & 0xFF);
    p[1] = static_cast<Byte> ((v >> 8) & 0xFF);
}


static void Write32LE (Byte * p, uint32_t v)
{
    p[0] = static_cast<Byte> (v        & 0xFF);
    p[1] = static_cast<Byte> ((v >> 8) & 0xFF);
    p[2] = static_cast<Byte> ((v >> 16) & 0xFF);
    p[3] = static_cast<Byte> ((v >> 24) & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchSig
//
////////////////////////////////////////////////////////////////////////////////

static bool MatchSig (const Byte * p, const Byte * sig)
{
    size_t   i = 0;

    for (i = 0; i < kSigLen; i++)
    {
        if (p[i] != sig[i])
        {
            return false;
        }
    }

    return true;
}


static bool MatchMagic (const Byte * p, const Byte * magic)
{
    size_t   i = 0;

    for (i = 0; i < 4; i++)
    {
        if (p[i] != magic[i])
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseV1Track / ParseV2Track
//
//  Each populates one DiskImage track from the WOZ bit-stream payload.
//  Both copy the packed bytes verbatim (WOZ tracks are MSB-first packed
//  bit streams, matching DiskImage's storage convention).
//
////////////////////////////////////////////////////////////////////////////////

static void ParseV1Track (
    const Byte    *  trackPtr,
    int              destTrack,
    DiskImage     &  out)
{
    size_t   bitCount = 0;

    bitCount = Read16LE (trackPtr + 6648);

    if (bitCount == 0 || bitCount > WozLoader::kV1TrackRecordSize * 8)
    {
        return;
    }

    out.ResizeTrack (destTrack, bitCount);

    {
        vector<Byte>  &  buf       = out.GetTrackBitsForWrite (destTrack);
        size_t           byteCount = (bitCount + 7) / 8;

        if (byteCount > buf.size ())
        {
            byteCount = buf.size ();
        }

        memcpy (buf.data (), trackPtr, byteCount);
    }

    out.SetTrackBitCount (destTrack, bitCount);
}


static HRESULT ParseV2Track (
    const vector<Byte>  &  raw,
    const Byte          *  trkRecord,
    int                    destTrack,
    DiskImage           &  out)
{
    HRESULT    hr            = S_OK;
    uint16_t   startBlock    = 0;
    uint16_t   blockCount    = 0;
    uint32_t   bitCount      = 0;
    size_t     byteOffset    = 0;
    size_t     byteCount     = 0;

    startBlock = Read16LE (trkRecord);
    blockCount = Read16LE (trkRecord + 2);
    bitCount   = Read32LE (trkRecord + 4);

    if (startBlock == 0 || blockCount == 0 || bitCount == 0)
    {
        goto Error;
    }

    byteOffset = static_cast<size_t> (startBlock) * WozLoader::kV2BlockSize;
    byteCount  = (bitCount + 7) / 8;

    if (byteOffset + byteCount > raw.size ())
    {
        hr = E_FAIL;
        goto Error;
    }

    out.ResizeTrack (destTrack, bitCount);

    {
        vector<Byte>  &  buf = out.GetTrackBitsForWrite (destTrack);

        if (byteCount > buf.size ())
        {
            byteCount = buf.size ();
        }

        memcpy (buf.data (), raw.data () + byteOffset, byteCount);
    }

    out.SetTrackBitCount (destTrack, bitCount);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozLoader::Load
//
//  Parse a WOZ v1 or v2 image. Validates the signature, walks chunks,
//  routes per-track bit streams into the DiskImage. Sets the
//  write-protect flag from the INFO chunk. Returns E_FAIL on malformed
//  or unsupported variants (unknown signature, truncated chunk, missing
//  TMAP/TRKS).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WozLoader::Load (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT        hr             = S_OK;
    bool           isV2           = false;
    bool           sawInfo        = false;
    bool           sawTmap        = false;
    bool           sawTrks        = false;
    bool           writeProtected = false;
    size_t         pos            = 0;
    size_t         chunkPos       = 0;
    Byte           tmap[kTmapChunkSize] = {};
    const Byte *   trksData       = nullptr;
    size_t         trksSize       = 0;
    int            qt             = 0;
    int            destTrack      = 0;
    Byte           trackIndex     = 0;
    int            trackI         = 0;

    if (raw.size () < kHeaderSize)
    {
        hr = E_FAIL;
        goto Error;
    }

    if (MatchSig (raw.data (), kSigV2))
    {
        isV2 = true;
    }
    else if (MatchSig (raw.data (), kSigV1))
    {
        isV2 = false;
    }
    else
    {
        hr = E_FAIL;
        goto Error;
    }

    pos = kSigLen + kCrcLen;

    while (pos + 8 <= raw.size ())
    {
        const Byte *   id        = raw.data () + pos;
        uint32_t       chunkSize = 0;
        bool           known     = false;

        known = MatchMagic (id, kInfoMagic)
             || MatchMagic (id, kTmapMagic)
             || MatchMagic (id, kTrksMagic)
             || MatchMagic (id, kMetaMagic);

        if (!known)
        {
            // After TRKS the remainder of a v2 file is the per-track
            // bit-stream blocks; they aren't chunks. Stop scanning.
            break;
        }

        chunkSize = Read32LE (raw.data () + pos + 4);
        chunkPos  = pos + 8;

        if (chunkPos + chunkSize > raw.size ())
        {
            hr = E_FAIL;
            goto Error;
        }

        if (MatchMagic (id, kInfoMagic))
        {
            if (chunkSize < kInfoChunkSize)
            {
                hr = E_FAIL;
                goto Error;
            }
            writeProtected = (raw[chunkPos + 2] != 0);
            sawInfo        = true;
        }
        else if (MatchMagic (id, kTmapMagic))
        {
            if (chunkSize < kTmapChunkSize)
            {
                hr = E_FAIL;
                goto Error;
            }
            memcpy (tmap, raw.data () + chunkPos, kTmapChunkSize);
            sawTmap = true;
        }
        else if (MatchMagic (id, kTrksMagic))
        {
            trksData = raw.data () + chunkPos;
            trksSize = chunkSize;
            sawTrks  = true;
        }
        else if (MatchMagic (id, kMetaMagic))
        {
            // META chunk is optional, ignored on load.
        }

        pos = chunkPos + chunkSize;
    }

    if (!sawInfo || !sawTmap || !sawTrks)
    {
        hr = E_FAIL;
        goto Error;
    }

    out.SetWriteProtected (writeProtected);
    out.SetSourceFormat   (DiskFormat::Woz);

    if (isV2)
    {
        if (trksSize < kV2TrkRecordCount * kV2TrkRecordSize)
        {
            hr = E_FAIL;
            goto Error;
        }

        for (qt = 0; qt < kTmapChunkSize; qt++)
        {
            trackIndex = tmap[qt];
            if (trackIndex == kTmapEmptyTrack)
            {
                continue;
            }

            destTrack = qt / kQuarterTracksPerTrack;
            if (destTrack >= kMaxTracks)
            {
                continue;
            }

            if ((qt % kQuarterTracksPerTrack) != 0)
            {
                continue;
            }

            HRESULT   hrTrack = ParseV2Track (
                raw,
                trksData + static_cast<size_t> (trackIndex) * kV2TrkRecordSize,
                destTrack,
                out);

            if (FAILED (hrTrack) && destTrack < out.GetTrackCount ())
            {
                hr = hrTrack;
                goto Error;
            }
        }
    }
    else
    {
        for (qt = 0; qt < kTmapChunkSize; qt++)
        {
            trackIndex = tmap[qt];
            if (trackIndex == kTmapEmptyTrack)
            {
                continue;
            }

            destTrack = qt / kQuarterTracksPerTrack;
            if (destTrack >= kMaxTracks)
            {
                continue;
            }

            if ((qt % kQuarterTracksPerTrack) != 0)
            {
                continue;
            }

            {
                size_t   recOffset = static_cast<size_t> (trackIndex) * kV1TrackRecordSize;

                if (recOffset + kV1TrackRecordSize > trksSize)
                {
                    hr = E_FAIL;
                    goto Error;
                }

                ParseV1Track (trksData + recOffset, destTrack, out);
            }
        }
    }

    out.ClearDirty ();

    UNREFERENCED_PARAMETER (trackI);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSyntheticV2
//
//  Test helper: emits a minimal valid WOZ v2 image holding a single track
//  (track 0). Used by WozLoaderTests and DiskImageStoreTests to exercise
//  the loader end-to-end without depending on third-party WOZ content.
//
//  Layout:
//      [0..11]   header (sig + zero CRC)
//      [12..71]  INFO chunk (8 byte hdr + 60 byte payload)
//      [72..239] TMAP chunk (8 byte hdr + 160 byte payload)
//      [240..1535] TRKS chunk (8 byte hdr + 1280 byte TRK records)
//                  TRK[0] = (startingBlock=3, blockCount=N, bitCount)
//      [block 3..]  bit-stream payload
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WozLoader::BuildSyntheticV2 (
    Byte                  diskType,
    bool                  writeProtected,
    const vector<Byte> &  trackZeroBitStream,
    size_t                trackZeroBitCount,
    vector<Byte>       &  outBytes)
{
    HRESULT   hr             = S_OK;
    size_t    blocks         = 0;
    size_t    payloadBytes   = 0;
    size_t    fileBytes      = 0;
    size_t    pos            = 0;
    size_t    trksRecBytes   = kV2TrkRecordCount * kV2TrkRecordSize;
    size_t    trksHdr        = 8;
    size_t    bitStreamStart = 0;
    int       qt             = 0;

    payloadBytes = (trackZeroBitCount + 7) / 8;
    blocks       = (payloadBytes + kV2BlockSize - 1) / kV2BlockSize;

    if (blocks == 0)
    {
        blocks = 1;
    }

    fileBytes = 3 * kV2BlockSize + blocks * kV2BlockSize;

    outBytes.assign (fileBytes, 0);

    memcpy (outBytes.data (), kSigV2, kSigLen);
    Write32LE (outBytes.data () + kSigLen, 0);

    pos = kHeaderSize;

    memcpy (outBytes.data () + pos, kInfoMagic, 4);
    Write32LE (outBytes.data () + pos + 4, static_cast<uint32_t> (kInfoChunkSize));
    outBytes[pos + 8 + 0] = 2;                                    // version
    outBytes[pos + 8 + 1] = diskType;                              // 1 = 5.25
    outBytes[pos + 8 + 2] = static_cast<Byte> (writeProtected ? 1 : 0);
    outBytes[pos + 8 + 3] = 0;                                    // synchronized
    outBytes[pos + 8 + 4] = 1;                                    // cleaned
    pos += 8 + kInfoChunkSize;

    memcpy (outBytes.data () + pos, kTmapMagic, 4);
    Write32LE (outBytes.data () + pos + 4, static_cast<uint32_t> (kTmapChunkSize));

    for (qt = 0; qt < static_cast<int> (kTmapChunkSize); qt++)
    {
        outBytes[pos + 8 + qt] = kTmapEmptyTrack;
    }

    outBytes[pos + 8 + 0] = 0;
    outBytes[pos + 8 + 1] = 0;
    outBytes[pos + 8 + 3] = 0;

    pos += 8 + kTmapChunkSize;

    memcpy (outBytes.data () + pos, kTrksMagic, 4);
    Write32LE (outBytes.data () + pos + 4, static_cast<uint32_t> (trksRecBytes));

    bitStreamStart = 3 * kV2BlockSize;

    Write16LE (outBytes.data () + pos + trksHdr,                  static_cast<uint16_t> (3));
    Write16LE (outBytes.data () + pos + trksHdr + 2,              static_cast<uint16_t> (blocks));
    Write32LE (outBytes.data () + pos + trksHdr + 4,              static_cast<uint32_t> (trackZeroBitCount));

    if (payloadBytes > 0 && payloadBytes <= trackZeroBitStream.size ())
    {
        memcpy (outBytes.data () + bitStreamStart, trackZeroBitStream.data (), payloadBytes);
    }

    return hr;
}
