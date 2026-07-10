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
//  Crc32
//
//  Standard reflected CRC-32 (poly 0xEDB88320, init/final XOR 0xFFFFFFFF) --
//  the algorithm the WOZ header CRC is computed with, over every byte after
//  the 12-byte header. Bit-serial (no table); serialization is cold, so the
//  per-byte inner loop is not worth a 1 KB static table.
//
////////////////////////////////////////////////////////////////////////////////

static uint32_t Crc32 (const Byte * data, size_t len)
{
    uint32_t   crc = 0xFFFFFFFFu;
    size_t     i   = 0;
    int        b   = 0;

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (b = 0; b < 8; b++)
        {
            uint32_t   mask = static_cast<uint32_t> (-static_cast<int32_t> (crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
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
    HRESULT        hr                   = S_OK;
    bool           isV2                 = false;
    bool           sawInfo              = false;
    bool           sawTmap              = false;
    bool           sawTrks              = false;
    bool           writeProtected       = false;
    size_t         pos                  = 0;
    size_t         chunkPos             = 0;
    Byte           tmap[kTmapChunkSize] = {};
    const Byte *   trksData             = nullptr;
    size_t         trksSize             = 0;
    int            qt                   = 0;
    Byte           trackIndex           = 0;
    int            trackI               = 0;

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
    out.ClearQuarterTrackMap ();

    {
        int   maxSlot = -1;

        for (qt = 0; qt < static_cast<int> (kTmapChunkSize); qt++)
        {
            if (tmap[qt] != kTmapEmptyTrack && tmap[qt] > maxSlot)
            {
                maxSlot = tmap[qt];
            }
        }

        out.EnsureTrackSlots (maxSlot + 1);
    }

    if (isV2)
    {
        vector<bool>   parsed (kV2TrkRecordCount, false);

        if (trksSize < kV2TrkRecordCount * kV2TrkRecordSize)
        {
            hr = E_FAIL;
            goto Error;
        }

        for (qt = 0; qt < static_cast<int> (kTmapChunkSize); qt++)
        {
            trackIndex = tmap[qt];
            if (trackIndex == kTmapEmptyTrack || trackIndex >= kV2TrkRecordCount)
            {
                continue;
            }

            if (!parsed[trackIndex])
            {
                HRESULT   hrTrack = ParseV2Track (
                    raw,
                    trksData + static_cast<size_t> (trackIndex) * kV2TrkRecordSize,
                    trackIndex,
                    out);

                if (FAILED (hrTrack))
                {
                    hr = hrTrack;
                    goto Error;
                }

                parsed[trackIndex] = true;
            }

            out.SetQuarterTrackSlot (qt, trackIndex);
        }
    }
    else
    {
        vector<bool>   parsed (kV2TrkRecordCount, false);

        for (qt = 0; qt < static_cast<int> (kTmapChunkSize); qt++)
        {
            trackIndex = tmap[qt];
            if (trackIndex == kTmapEmptyTrack || trackIndex >= kV2TrkRecordCount)
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

                if (!parsed[trackIndex])
                {
                    ParseV1Track (trksData + recOffset, trackIndex, out);
                    parsed[trackIndex] = true;
                }
            }

            out.SetQuarterTrackSlot (qt, trackIndex);
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




////////////////////////////////////////////////////////////////////////////////
//
//  WozLoader::Serialize
//
//  Emit a WOZ v2 byte image from a DiskImage's live per-track bit streams,
//  so guest writes round-trip on flush (the write-back path that was never
//  finished -- Serialize's WOZ arm used to return the untouched source
//  bytes). Layout mirrors BuildSyntheticV2 but spans every populated slot:
//
//      [0..11]       header (WOZ2 sig + CRC32)
//      [12..79]      INFO chunk (8-byte hdr + 60-byte payload)
//      [80..247]     TMAP chunk (8-byte hdr + 160 quarter-track slots)
//      [248..1535]   TRKS chunk (8-byte hdr + 160 x 8-byte TRK records)
//      [block 3..]   per-track bit streams, each block-aligned (512 bytes)
//
//  The TMAP is rebuilt from the image's quarter-track map (ResolveQuarterTrack),
//  and each slot's TRK record points at its block-aligned bit stream. The
//  write-protect flag is carried through from INFO. Emitting v2 for any source
//  variant is fine -- the loader reads v2 and Casso only models 5.25" disks.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WozLoader::Serialize (const DiskImage & img, vector<Byte> & outBytes)
{
    struct TrkGeom
    {
        uint16_t   startBlock = 0;
        uint16_t   blockCount = 0;
        uint32_t   bitCount   = 0;
    };

    HRESULT             hr           = S_OK;
    int                 slotCount    = img.GetTrackCount ();
    uint16_t            nextBlock    = 3;                 // blocks 0..2 hold the chunks
    uint16_t            largestTrack = 0;
    size_t              trksRecBytes = kV2TrkRecordCount * kV2TrkRecordSize;
    size_t              pos          = 0;
    int                 slot         = 0;
    int                 qt           = 0;
    vector<TrkGeom>     geom (kV2TrkRecordCount);

    if (slotCount > static_cast<int> (kV2TrkRecordCount))
    {
        slotCount = static_cast<int> (kV2TrkRecordCount);
    }

    // Pass 1: assign each populated slot a block-aligned region after the
    // three fixed header blocks.
    for (slot = 0; slot < slotCount; slot++)
    {
        size_t   bitCount = img.GetTrackBitCount (slot);
        size_t   byteCount = 0;
        size_t   blocks    = 0;

        if (bitCount == 0)
        {
            continue;
        }

        byteCount = (bitCount + 7) / 8;
        blocks    = (byteCount + kV2BlockSize - 1) / kV2BlockSize;

        geom[slot].startBlock = nextBlock;
        geom[slot].blockCount = static_cast<uint16_t> (blocks);
        geom[slot].bitCount   = static_cast<uint32_t> (bitCount);

        nextBlock = static_cast<uint16_t> (nextBlock + blocks);
        if (blocks > largestTrack)
        {
            largestTrack = static_cast<uint16_t> (blocks);
        }
    }

    outBytes.assign (static_cast<size_t> (nextBlock) * kV2BlockSize, 0);

    // Header (CRC filled in last).
    memcpy (outBytes.data (), kSigV2, kSigLen);

    pos = kHeaderSize;

    // INFO chunk.
    memcpy    (outBytes.data () + pos, kInfoMagic, 4);
    Write32LE (outBytes.data () + pos + 4, static_cast<uint32_t> (kInfoChunkSize));
    {
        Byte *        info      = outBytes.data () + pos + 8;
        const char    creator[] = "Casso";

        info[0] = 2;                                            // INFO version 2
        info[1] = 1;                                            // disk type: 5.25"
        info[2] = static_cast<Byte> (img.IsWriteProtected () ? 1 : 0);
        info[3] = 0;                                            // synchronized
        info[4] = 1;                                            // cleaned
        memset (info + 5, ' ', 32);                             // creator: 32 bytes, space-padded
        memcpy (info + 5, creator, sizeof (creator) - 1);
        info[37] = 1;                                           // disk sides
        info[38] = 0;                                           // boot sector format: unknown
        info[39] = 32;                                          // optimal bit timing: 5.25" = 4us
        // compatible hardware (+40..41) / required RAM (+42..43): unknown = 0
        Write16LE (info + 44, largestTrack);                   // largest track, in blocks
    }
    pos += 8 + kInfoChunkSize;

    // TMAP chunk: one slot index (or 0xFF) per quarter-track phase.
    memcpy    (outBytes.data () + pos, kTmapMagic, 4);
    Write32LE (outBytes.data () + pos + 4, static_cast<uint32_t> (kTmapChunkSize));
    {
        Byte *   tmap = outBytes.data () + pos + 8;

        for (qt = 0; qt < static_cast<int> (kTmapChunkSize); qt++)
        {
            int   resolved = img.ResolveQuarterTrack (qt);

            tmap[qt] = (resolved >= 0 && resolved < slotCount)
                       ? static_cast<Byte> (resolved)
                       : kTmapEmptyTrack;
        }
    }
    pos += 8 + kTmapChunkSize;

    // TRKS chunk: 160 fixed 8-byte records; populated slots reference their
    // block-aligned bit stream, the rest stay zero (empty).
    memcpy    (outBytes.data () + pos, kTrksMagic, 4);
    Write32LE (outBytes.data () + pos + 4, static_cast<uint32_t> (trksRecBytes));
    {
        Byte *   trks = outBytes.data () + pos + 8;

        for (slot = 0; slot < static_cast<int> (kV2TrkRecordCount); slot++)
        {
            Byte *   rec = trks + static_cast<size_t> (slot) * kV2TrkRecordSize;

            Write16LE (rec,     geom[slot].startBlock);
            Write16LE (rec + 2, geom[slot].blockCount);
            Write32LE (rec + 4, geom[slot].bitCount);
        }
    }

    // Per-track bit-stream payload at each slot's block offset.
    for (slot = 0; slot < slotCount; slot++)
    {
        const vector<Byte> *  bits      = nullptr;
        size_t                byteCount = 0;
        size_t                dstOff    = 0;

        if (geom[slot].bitCount == 0)
        {
            continue;
        }

        bits      = &img.GetTrackBits (slot);
        byteCount = (geom[slot].bitCount + 7) / 8;
        dstOff    = static_cast<size_t> (geom[slot].startBlock) * kV2BlockSize;

        if (byteCount > bits->size ())
        {
            byteCount = bits->size ();
        }

        memcpy (outBytes.data () + dstOff, bits->data (), byteCount);
    }

    // Header CRC32 over everything after the 12-byte header.
    Write32LE (outBytes.data () + kSigLen,
               Crc32 (outBytes.data () + kHeaderSize, outBytes.size () - kHeaderSize));

    return hr;
}
