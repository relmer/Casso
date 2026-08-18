#include "Pch.h"

#include "Version.h"
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
static constexpr Byte    kTmapEmptyTrack = 0xFF;
static constexpr int     kQuarterTracksPerTrack = 4;
static constexpr int     kMaxTracks      = 40;
static constexpr size_t  kChunkHeaderSize = 8;      // 4-byte id + 4-byte size





////////////////////////////////////////////////////////////////////////////////
//
//  INFO chunk field offsets, from the start of the chunk's payload
//
//  Casso owns four of these -- version, disk type, write protect and largest
//  track -- and re-emits the rest of the chunk exactly as it was read. The
//  values below are the ones it writes for a disk it authored itself.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr size_t  kInfoVersionOff      = 0;
static constexpr size_t  kInfoDiskTypeOff     = 1;
static constexpr size_t  kInfoWriteProtectOff = 2;
static constexpr size_t  kInfoCleanedOff      = 4;
static constexpr size_t  kInfoCreatorOff      = 5;
static constexpr size_t  kInfoCreatorSize     = 32;
static constexpr size_t  kInfoDiskSidesOff    = 37;
static constexpr size_t  kInfoBitTimingOff    = 39;
static constexpr size_t  kInfoLargestTrackOff = 44;

static constexpr Byte    kInfoVersion2        = 2;
static constexpr Byte    kDiskType525         = 1;
static constexpr Byte    kSingleSided         = 1;
static constexpr Byte    kCleaned             = 1;
static constexpr Byte    kBitTiming525        = 32;   // 4us, in 125ns units

// Stamped into creator only on a disk Casso authored. A disk Casso merely
// edited keeps whoever imaged it in that field -- overwriting it is how a
// preservation dump loses the one record of where it came from.
static constexpr char    kCassoCreator[]      = "Casso " VERSION_STRING;





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





////////////////////////////////////////////////////////////////////////////////
//
//  Write16LE
//
////////////////////////////////////////////////////////////////////////////////

static void Write16LE (Byte * p, uint16_t v)
{
    p[0] = static_cast<Byte> (v        & 0xFF);
    p[1] = static_cast<Byte> ((v >> 8) & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write32LE
//
////////////////////////////////////////////////////////////////////////////////

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
    size_t   i       = 0;
    bool     matches = true;



    for (i = 0; matches && i < kSigLen; i++)
    {
        matches = (p[i] == sig[i]);
    }

    return matches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchMagic
//
////////////////////////////////////////////////////////////////////////////////

static bool MatchMagic (const Byte * p, const Byte * magic)
{
    size_t   i       = 0;
    bool     matches = true;



    for (i = 0; matches && i < 4; i++)
    {
        matches = (p[i] == magic[i]);
    }

    return matches;
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

        if (byteCount > buf.size())
        {
            byteCount = buf.size();
        }

        memcpy (buf.data(), trackPtr, byteCount);
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
    size_t     rawSize       = 0;

    startBlock = Read16LE (trkRecord);
    blockCount = Read16LE (trkRecord + 2);
    bitCount   = Read32LE (trkRecord + 4);

    BAIL_OUT_IF (startBlock == 0 || blockCount == 0 || bitCount == 0, S_OK);

    byteOffset = static_cast<size_t> (startBlock) * WozLoader::kV2BlockSize;
    byteCount  = (bitCount + 7) / 8;
    rawSize    = raw.size();

    CBR (byteOffset + byteCount <= rawSize);

    out.ResizeTrack (destTrack, bitCount);

    {
        vector<Byte>  &  buf = out.GetTrackBitsForWrite (destTrack);

        if (byteCount > buf.size())
        {
            byteCount = buf.size();
        }

        memcpy (buf.data(), raw.data() + byteOffset, byteCount);
    }

    out.SetTrackBitCount (destTrack, bitCount);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindChunkPayload
//
//  Locate one chunk by walking the table from the header, the same way Load
//  does: a chunk id is four uppercase letters, and a chunk that overruns the
//  file ends the walk. Reports the payload's offset and declared size.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT FindChunkPayload (
    const vector<Byte>  &  raw,
    const Byte          *  magic,
    size_t              &  outOffset,
    uint32_t            &  outSize)
{
    HRESULT   hr      = E_FAIL;
    size_t    pos     = WozLoader::kHeaderSize;
    size_t    rawSize = raw.size();
    bool      found   = false;

    outOffset = 0;
    outSize   = 0;

    while (!found && pos + kChunkHeaderSize <= rawSize)
    {
        const Byte *  id        = raw.data() + pos;
        uint32_t      chunkSize = 0;
        bool          isChunkId = true;
        int           idByte    = 0;

        for (idByte = 0; isChunkId && idByte < 4; idByte++)
        {
            isChunkId = (id[idByte] >= 'A' && id[idByte] <= 'Z');
        }

        if (!isChunkId)
        {
            break;
        }

        chunkSize = Read32LE (raw.data() + pos + 4);

        if (pos + kChunkHeaderSize + chunkSize > rawSize)
        {
            break;
        }

        if (MatchMagic (id, magic))
        {
            outOffset = pos + kChunkHeaderSize;
            outSize   = chunkSize;
            found     = true;
            hr        = S_OK;
        }

        pos += kChunkHeaderSize + chunkSize;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozLoader::SetWriteProtectFlag
//
//  Patch the write-protect flag into a WOZ file's bytes without rebuilding
//  the file. Only the INFO flag byte and the header CRC change.
//
//  This exists because the flag lives inside the file, so changing it has to
//  write -- and the only writer available was the full rebuild-from-model
//  serializer. Sending a one-bit edit through that meant a menu click could
//  relayout an entire image, which is a lot of blast radius for one bit.
//  Bytes that are never parsed here are bytes that cannot be damaged here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WozLoader::SetWriteProtectFlag (vector<Byte> & fileBytes, bool writeProtected)
{
    HRESULT    hr         = S_OK;
    size_t     rawSize    = fileBytes.size();
    size_t     infoOffset = 0;
    uint32_t   infoSize   = 0;
    bool       sigOk      = false;
    bool       infoUsable = false;



    CBR (rawSize >= kHeaderSize);

    sigOk = MatchSig (fileBytes.data(), kSigV2) || MatchSig (fileBytes.data(), kSigV1);
    CBR (sigOk);

    hr = FindChunkPayload (fileBytes, kInfoMagic, infoOffset, infoSize);
    CHR (hr);

    infoUsable = (infoSize >= kInfoChunkSize);
    CBR (infoUsable);

    fileBytes[infoOffset + kInfoWriteProtectOff] = static_cast<Byte> (writeProtected ? 1 : 0);

    // The stored CRC covers everything after the 12-byte header, so the one
    // changed byte invalidates it. Recompute rather than zero it: zero means
    // "not computed", which would quietly drop the file's own damage check.
    Write32LE (fileBytes.data() + kSigLen,
               Crc32 (fileBytes.data() + kHeaderSize, rawSize - kHeaderSize));

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
    bool           sigV2                = false;
    bool           sigV1                = false;
    size_t         rawSize              = 0;
    uint32_t       storedCrc            = 0;
    bool           crcOk                = true;
    bool           isChunkId            = false;
    int            idByte               = 0;
    WozMetadata    metadata;

    rawSize = raw.size();
    CBR (rawSize >= kHeaderSize);

    // Signature match is captured into locals first so the guard below tests a
    // plain variable rather than calling MatchSig from inside the macro.
    sigV2 = MatchSig (raw.data(), kSigV2);
    sigV1 = MatchSig (raw.data(), kSigV1);

    CBR (sigV2 || sigV1);

    isV2 = sigV2;

    // A stored CRC of zero means the writer computed none, which the format
    // defines as "skip validation". Any other value must match, and a
    // mismatch is REPORTED rather than fatal: a damaged preservation dump is
    // precisely the file a user needs to be able to open and inspect. The
    // image carries the fact so a later flush can warn before replacing the
    // file with a freshly checksummed copy of the same damage.
    storedCrc = Read32LE (raw.data() + kSigLen);

    if (storedCrc != 0)
    {
        crcOk = (storedCrc == Crc32 (raw.data() + kHeaderSize, rawSize - kHeaderSize));
    }

    out.SetSourceCrcMismatch (!crcOk);

    if (!crcOk)
    {
        EhmNotifyUser (L"This disk image's stored checksum does not match its "
                       L"contents, so the file is damaged or was written by a "
                       L"tool that miscomputed it.\n\nCasso has loaded it anyway "
                       L"so you can read it. Saving the disk will replace the "
                       L"file with a newly checksummed copy, after which the "
                       L"damage can no longer be detected.");
    }

    pos = kSigLen + kCrcLen;

    while (pos + 8 <= raw.size())
    {
        const Byte *   id        = raw.data() + pos;
        uint32_t       chunkSize = 0;

        // Any four-uppercase-letter tag is a chunk, not only the three Casso
        // models. An unrecognized chunk has to be stepped over and kept, not
        // treated as end-of-table -- stopping there loses it AND everything
        // after it on the next rewrite. Bit-stream blocks cannot be mistaken
        // for a tag: 6-and-2 nibbles all have the high bit set, so every one
        // of them falls outside 'A'..'Z'.
        isChunkId = true;

        for (idByte = 0; isChunkId && idByte < 4; idByte++)
        {
            isChunkId = (id[idByte] >= 'A' && id[idByte] <= 'Z');
        }

        if (!isChunkId)
        {
            // A TRKS size covering only the record table -- what Casso
            // itself wrote before 1.16.2 -- leaves the walk pointing into
            // track data. Stop rather than read bit streams as a table.
            break;
        }

        chunkSize = Read32LE (raw.data() + pos + 4);
        chunkPos  = pos + 8;

        CBR (chunkPos + chunkSize <= rawSize);

        if (MatchMagic (id, kInfoMagic))
        {
            CBR (chunkSize >= kInfoChunkSize);
            writeProtected = (raw[chunkPos + 2] != 0);
            sawInfo        = true;

            // Held verbatim so a rewrite re-emits the fields Casso does not
            // model -- creator, synchronized, cleaned, boot sector format,
            // timing, compatible hardware, required RAM -- instead of
            // synthesizing a spec-valid replacement that says "unknown".
            metadata.infoPayload.assign (raw.data() + chunkPos,
                                         raw.data() + chunkPos + chunkSize);
        }
        else if (MatchMagic (id, kTmapMagic))
        {
            CBR (chunkSize >= kTmapChunkSize);
            memcpy (tmap, raw.data() + chunkPos, kTmapChunkSize);
            sawTmap = true;
        }
        else if (MatchMagic (id, kTrksMagic))
        {
            trksData = raw.data() + chunkPos;
            trksSize = chunkSize;
            sawTrks  = true;
        }
        else
        {
            // META, and whatever a later revision of the format adds. Casso
            // reads none of it, which is precisely why it can be handed back
            // untouched.
            WozChunk   passThrough;

            memcpy (passThrough.id, id, 4);
            passThrough.payload.assign (raw.data() + chunkPos,
                                        raw.data() + chunkPos + chunkSize);
            metadata.passThrough.push_back (passThrough);
        }

        pos = chunkPos + chunkSize;
    }

    CBR (sawInfo && sawTmap && sawTrks);

    out.SetImageWriteProtected (writeProtected);
    out.SetSourceFormat        (DiskFormat::Woz);
    out.SetWozMetadata         (metadata);
    out.ClearQuarterTrackMap();

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

        CBR (trksSize >= kV2TrkRecordCount * kV2TrkRecordSize);

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

                CHR (hrTrack);

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

                CBR (recOffset + kV1TrackRecordSize <= trksSize);

                if (!parsed[trackIndex])
                {
                    ParseV1Track (trksData + recOffset, trackIndex, out);
                    parsed[trackIndex] = true;
                }
            }

            out.SetQuarterTrackSlot (qt, trackIndex);
        }
    }

    out.ClearDirty();

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
//  The TRKS chunk size spans the record table AND the bit-stream payload
//  that follows it, per the WOZ2 spec -- the payload is chunk content, not
//  trailing data, so a size covering only the records leaves every later
//  chunk unreachable to a conformant parser.
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
    size_t    trksSize       = 0;
    size_t    trksHdr        = 8;
    size_t    bitStreamStart = 0;
    int       qt             = 0;

    payloadBytes = (trackZeroBitCount + 7) / 8;
    blocks       = (payloadBytes + kV2BlockSize - 1) / kV2BlockSize;

    if (blocks == 0)
    {
        blocks = 1;
    }

    fileBytes = (kV2FirstDataBlock + blocks) * kV2BlockSize;

    outBytes.assign (fileBytes, 0);

    memcpy (outBytes.data(), kSigV2, kSigLen);
    Write32LE (outBytes.data() + kSigLen, 0);

    pos = kHeaderSize;

    memcpy (outBytes.data() + pos, kInfoMagic, 4);
    Write32LE (outBytes.data() + pos + 4, static_cast<uint32_t> (kInfoChunkSize));
    outBytes[pos + 8 + 0] = 2;                                    // version
    outBytes[pos + 8 + 1] = diskType;                              // 1 = 5.25
    outBytes[pos + 8 + 2] = static_cast<Byte> (writeProtected ? 1 : 0);
    outBytes[pos + 8 + 3] = 0;                                    // synchronized
    outBytes[pos + 8 + 4] = 1;                                    // cleaned
    pos += 8 + kInfoChunkSize;

    memcpy (outBytes.data() + pos, kTmapMagic, 4);
    Write32LE (outBytes.data() + pos + 4, static_cast<uint32_t> (kTmapChunkSize));

    for (qt = 0; qt < static_cast<int> (kTmapChunkSize); qt++)
    {
        outBytes[pos + 8 + qt] = kTmapEmptyTrack;
    }

    outBytes[pos + 8 + 0] = 0;
    outBytes[pos + 8 + 1] = 0;
    outBytes[pos + 8 + 3] = 0;

    pos += 8 + kTmapChunkSize;

    // Records plus the block-aligned payload that follows them.
    trksSize = trksRecBytes + blocks * kV2BlockSize;

    memcpy (outBytes.data() + pos, kTrksMagic, 4);
    Write32LE (outBytes.data() + pos + 4, static_cast<uint32_t> (trksSize));

    bitStreamStart = kV2FirstDataBlock * kV2BlockSize;

    Write16LE (outBytes.data() + pos + trksHdr,                  kV2FirstDataBlock);
    Write16LE (outBytes.data() + pos + trksHdr + 2,              static_cast<uint16_t> (blocks));
    Write32LE (outBytes.data() + pos + trksHdr + 4,              static_cast<uint32_t> (trackZeroBitCount));

    if (payloadBytes > 0 && payloadBytes <= trackZeroBitStream.size())
    {
        memcpy (outBytes.data() + bitStreamStart, trackZeroBitStream.data(), payloadBytes);
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
//  The TRKS chunk size spans the records AND the bit streams after them:
//  they are the chunk's content, not trailing data. Sizing it to the record
//  table alone puts a conformant parser's next chunk offset in the middle of
//  track data, so it sees no valid id and stops -- which silently hides any
//  chunk written after TRKS.
//
//  The TMAP is rebuilt from the image's quarter-track map (ResolveQuarterTrack),
//  and each slot's TRK record points at its block-aligned bit stream. The
//  write-protect flag is carried through from INFO. Emitting v2 for any source
//  variant is fine -- the loader reads v2 and Casso only models 5.25" disks.
//
//  Rebuilding from the model is what makes guest writes survive, and it is
//  also why anything the model does not hold has to come from somewhere else:
//  the image's retained WozMetadata supplies the source INFO chunk and every
//  chunk Casso never parsed, so a round trip preserves them byte for byte
//  instead of replacing them with spec-valid defaults that say "unknown".
//  Casso stamps its own name into creator only on a disk it authored -- one
//  with no retained source INFO.
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

    HRESULT               hr            = S_OK;
    int                   slotCount     = img.GetTrackCount();
    uint16_t              nextBlock     = kV2FirstDataBlock;
    uint16_t              largestTrack  = 0;
    size_t                trksRecBytes  = kV2TrkRecordCount * kV2TrkRecordSize;
    size_t                trksSize      = 0;
    size_t                pos           = 0;
    int                   slot          = 0;
    int                   qt            = 0;
    const WozMetadata &   meta          = img.GetWozMetadata();
    bool                  hasSourceInfo = false;
    size_t                chunkCount    = 0;
    size_t                chunkIdx      = 0;
    vector<TrkGeom>       geom (kV2TrkRecordCount);

    if (slotCount > static_cast<int> (kV2TrkRecordCount))
    {
        slotCount = static_cast<int> (kV2TrkRecordCount);
    }

    hasSourceInfo = (meta.infoPayload.size() >= kInfoChunkSize);
    chunkCount    = meta.passThrough.size();

    // Pass 1: assign each populated slot a block-aligned region after the
    // three fixed header blocks.
    for (slot = 0; slot < slotCount; slot++)
    {
        size_t  bitCount  = img.GetTrackBitCount (slot);
        size_t  byteCount = 0;
        size_t  blocks    = 0;

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

    // Pass 1 fixed the payload span, so the TRKS chunk size is now known:
    // the record table plus every block assigned above.
    trksSize = trksRecBytes
             + static_cast<size_t> (nextBlock - kV2FirstDataBlock) * kV2BlockSize;

    outBytes.assign (static_cast<size_t> (nextBlock) * kV2BlockSize, 0);

    // Header (CRC filled in last).
    memcpy (outBytes.data(), kSigV2, kSigLen);

    pos = kHeaderSize;

    // INFO chunk.
    memcpy    (outBytes.data() + pos, kInfoMagic, 4);
    Write32LE (outBytes.data() + pos + 4, static_cast<uint32_t> (kInfoChunkSize));
    {
        Byte *   info = outBytes.data() + pos + kChunkHeaderSize;

        if (hasSourceInfo)
        {
            // Start from what the file said. Everything Casso does not model
            // -- creator, synchronized, cleaned, boot sector format, bit
            // timing, compatible hardware, required RAM -- belongs to the
            // source, and synthesizing replacements for them is what quietly
            // degraded every WOZ that passed through a flush.
            memcpy (info, meta.infoPayload.data(), kInfoChunkSize);
        }
        else
        {
            // No source INFO means Casso authored this disk, and only here
            // does it put its own name in the creator field.
            info[kInfoDiskTypeOff] = kDiskType525;
            info[kInfoCleanedOff]  = kCleaned;
            memset (info + kInfoCreatorOff, ' ', kInfoCreatorSize);
            memcpy (info + kInfoCreatorOff, kCassoCreator, sizeof (kCassoCreator) - 1);
        }

        // A version 1 INFO ends after the creator string, so a v1 source
        // leaves the later fields zero -- and zero is not a legal disk-sides
        // or bit-timing value. Fill those two, since emitting a v2 container
        // means they have to say something. The three Casso cannot derive
        // stay zero, which this format defines as "unknown": boot sector
        // format, compatible hardware and required RAM.
        if (info[kInfoVersionOff] < kInfoVersion2)
        {
            info[kInfoVersionOff]   = kInfoVersion2;
            info[kInfoDiskSidesOff] = kSingleSided;
            info[kInfoBitTimingOff] = kBitTiming525;
        }

        // The fields Casso owns, written last so they win over the source.
        // Only the image's OWN write-protect flag is persisted: a transient
        // user setting or a read-only backing file must not be baked into
        // the image bytes.
        info[kInfoWriteProtectOff] = static_cast<Byte> (img.IsImageWriteProtected() ? 1 : 0);
        Write16LE (info + kInfoLargestTrackOff, largestTrack);
    }

    pos += 8 + kInfoChunkSize;

    // TMAP chunk: one slot index (or 0xFF) per quarter-track phase.
    memcpy    (outBytes.data() + pos, kTmapMagic, 4);
    Write32LE (outBytes.data() + pos + 4, static_cast<uint32_t> (kTmapChunkSize));
    {
        Byte *   tmap = outBytes.data() + pos + 8;

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
    memcpy    (outBytes.data() + pos, kTrksMagic, 4);
    Write32LE (outBytes.data() + pos + 4, static_cast<uint32_t> (trksSize));
    {
        Byte *   trks = outBytes.data() + pos + 8;

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

        if (byteCount > bits->size())
        {
            byteCount = bits->size();
        }

        memcpy (outBytes.data() + dstOff, bits->data(), byteCount);
    }

    // Chunks Casso does not model, re-emitted after the last bit-stream
    // block -- where the source had them -- in source order and byte for
    // byte. META is the one that matters: it carries a preservation dump's
    // title, publisher, copyright and imaging provenance, none of which the
    // track model can hold, and none of which can be reconstructed once a
    // rewrite has dropped it.
    for (chunkIdx = 0; chunkIdx < chunkCount; chunkIdx++)
    {
        const WozChunk &   chunk       = meta.passThrough[chunkIdx];
        size_t             payloadSize = chunk.payload.size();
        size_t             at          = outBytes.size();

        outBytes.resize (at + kChunkHeaderSize + payloadSize);

        memcpy    (outBytes.data() + at, chunk.id, sizeof (chunk.id));
        Write32LE (outBytes.data() + at + sizeof (chunk.id), static_cast<uint32_t> (payloadSize));

        if (payloadSize > 0)
        {
            memcpy (outBytes.data() + at + kChunkHeaderSize, chunk.payload.data(), payloadSize);
        }
    }

    // Header CRC32 over everything after the 12-byte header.
    Write32LE (outBytes.data() + kSigLen,
               Crc32 (outBytes.data() + kHeaderSize, outBytes.size() - kHeaderSize));

    return hr;
}
