#include "Pch.h"

#include "DiskImage.h"
#include "DiskImageStore.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"





// Source of DiskImage identities. Shared by every image, so an identity is
// never reused within the process, whichever thread loads the medium.
static std::atomic<uint64_t>  s_nextImageId { 0 };





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImage::DiskImage
//
//  Construct an empty disk: 35 tracks of zero-length bit streams. Load()
//  resizes individual tracks to hold their nibblized bit data.
//
////////////////////////////////////////////////////////////////////////////////

DiskImage::DiskImage()
{
    m_trackBits.resize      (kDefaultTrackCount);
    m_trackBitCounts.resize (kDefaultTrackCount, 0);
    m_trackDirty.resize     (kDefaultTrackCount, false);
    InitWholeTrackMap();
    RenewIdentity();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitWholeTrackMap
//
//  Default quarter-track map for sector images: every quarter-track
//  resolves to its whole track (qt / 4). WOZ loads overwrite this with the
//  image's TMAP so half/quarter-track-formatted tracks resolve to distinct
//  storage slots.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::InitWholeTrackMap()
{
    int   qt = 0;



    m_quarterTrackMap.assign (kQuarterTrackCount, -1);

    for (qt = 0; qt < kQuarterTrackCount; qt++)
    {
        m_quarterTrackMap[qt] = qt / kQuarterTracksPerWholeTrack;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveQuarterTrack
//
//  Maps a head quarter-track position to its backing storage slot, or -1
//  when the position holds no data (unformatted: an out-of-range slot or a
//  zero-length stream). Callers treat -1 as no flux under the head.
//
//  WORTH KNOWING BEFORE WRITING A TEST THAT MOVES THE HEAD. On a WOZ the map
//  comes from the image's own TMAP, so distinct quarter-tracks address distinct
//  flux and a head parked between tracks reads what a real drive would. On a
//  sector image it is the synthetic qt / 4 default, because .dsk and .po
//  physically cannot carry half-track data -- there is nothing else it could
//  be, and this is a property of the format rather than a defect here.
//
//  The consequence lands on tests, not on the product: against a sector image
//  an off-track head resolves to the neighboring whole track and reads it
//  perfectly, so a stepper that leaves the head in the wrong place stays
//  invisible to the guest, where a real drive would be off-track and read
//  nothing. A stepper mutation survived a direct-boot gate for exactly this
//  reason and was caught only by a structural assertion. Gates that are meant
//  to be sensitive to head positioning belong on a WOZ.
//
////////////////////////////////////////////////////////////////////////////////

int DiskImage::ResolveQuarterTrack (int quarterTrack) const
{
    int  slot = 0;



    bool  inMap = (quarterTrack >= 0
                   && quarterTrack < static_cast<int> (m_quarterTrackMap.size()));
    slot = inMap ? m_quarterTrackMap[quarterTrack] : -1;



    // Three ways to hold no data -- off the end of the map, an unmapped or
    // out-of-range slot, or a slot whose stream is empty -- and callers
    // treat them identically, so they all fold into the one -1.
    if (slot < 0
        || slot >= static_cast<int> (m_trackBitCounts.size())
        || m_trackBitCounts[slot] == 0)
    {
        slot = -1;
    }

    return slot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearQuarterTrackMap / SetQuarterTrackSlot / EnsureTrackSlots
//
//  Bulk-loader surface (WozLoader). ClearQuarterTrackMap marks every
//  quarter-track unformatted; SetQuarterTrackSlot points one quarter-track
//  at a storage slot; EnsureTrackSlots grows the slot storage to hold at
//  least slotCount distinct bit streams.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::ClearQuarterTrackMap()
{
    m_quarterTrackMap.assign (kQuarterTrackCount, -1);
}


void DiskImage::SetQuarterTrackSlot (int quarterTrack, int slot)
{
    if (quarterTrack >= 0 && quarterTrack < static_cast<int> (m_quarterTrackMap.size()))
    {
        m_quarterTrackMap[quarterTrack] = slot;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureTrackSlots
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::EnsureTrackSlots (int slotCount)
{
    // Grow only -- the three vectors are index-parallel and shrinking one
    // would orphan the quarter-track map entries pointing past the new end.
    if (slotCount > static_cast<int> (m_trackBits.size()))
    {
        m_trackBits.resize      (slotCount);
        m_trackBitCounts.resize (slotCount, 0);
        m_trackDirty.resize     (slotCount, false);
        m_trackGeneration.resize (slotCount, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTrackCount
//
////////////////////////////////////////////////////////////////////////////////

int DiskImage::GetTrackCount() const
{
    return static_cast<int> (m_trackBits.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTrackBitCount
//
////////////////////////////////////////////////////////////////////////////////

size_t DiskImage::GetTrackBitCount (int track) const
{
    bool  inRange = (track >= 0 && track < static_cast<int> (m_trackBitCounts.size()));



    // An absent track is indistinguishable from an unformatted one: 0 bits.
    return inRange ? m_trackBitCounts[track] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryLocateBit
//
//  The bit-addressing arithmetic ReadBit and WriteBit both need. Wrapping
//  the index by the track's own bit count is what makes the head circle
//  the track instead of running off its end.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImage::TryLocateBit (int track, size_t bitIndex, size_t & byteIdx, int & shift) const
{
    bool    located   = (track >= 0 && track < static_cast<int> (m_trackBits.size()));
    size_t  trackBits = 0;
    size_t  wrapped   = 0;



    if (located)
    {
        trackBits = m_trackBitCounts[track];
        located   = (trackBits != 0);
    }

    if (located)
    {
        wrapped = bitIndex % trackBits;
        byteIdx = wrapped >> 3;
        shift   = 7 - static_cast<int> (wrapped & 7);
    }

    return located;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadBit
//
////////////////////////////////////////////////////////////////////////////////

uint8_t DiskImage::ReadBit (int track, size_t bitIndex) const
{
    size_t   byteIdx = 0;
    int      shift   = 0;
    uint8_t  bit     = 0;



    // Out of range or unformatted: no flux under the head, which reads as 0.
    if (TryLocateBit (track, bitIndex, byteIdx, shift))
    {
        bit = static_cast<uint8_t> ((m_trackBits[track][byteIdx] >> shift) & 1);
    }

    return bit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBit
//
//  Writes one bit into a track's bitstream and marks it dirty.
//
//  A single BIT, not a byte, because the Disk II has no byte concept on the
//  media -- the drive writes a serial flux stream and the controller shifts
//  bits out one at a time. Modelling the write at that granularity is what
//  lets copy-protected formats with non-byte-aligned data work.
//
//  Write protection is checked BEFORE locating the bit, so a protected image
//  can never mark a track dirty however valid the address -- otherwise a
//  rejected write would still cause a flush.
//
//  Both a per-track and a whole-image dirty flag are set: the track flag says
//  what to re-encode, and the image flag says whether to write anything at
//  all.
//
//  An out-of-range track or bit index is silently ignored, matching the
//  hardware -- a head positioned off the media writes nowhere and reports
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::WriteBit (int track, size_t bitIndex, uint8_t bit)
{
    size_t   byteIdx = 0;
    int      shift   = 0;
    Byte     mask    = 0;



    // Write-protect is checked before locating so a protected image never
    // marks a track dirty, however valid the address.
    if (!IsWriteProtected() && TryLocateBit (track, bitIndex, byteIdx, shift))
    {
        mask = static_cast<Byte> (1 << shift);

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

        TouchTrack (track);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDirty / IsWriteProtected / GetWriteProtectInfo / GetSourceFormat /
//  IsTrackDirty / ClearDirty
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImage::IsDirty() const
{
    return m_dirty;
}


bool DiskImage::IsWriteProtected() const
{
    return m_imageWriteProtected
        || m_userWriteProtected
        || m_fileReadOnly
        || m_fileNoPermission
        || m_sourceCrcMismatch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWriteProtectInfo
//
////////////////////////////////////////////////////////////////////////////////

WriteProtectInfo DiskImage::GetWriteProtectInfo() const
{
    WriteProtectInfo  info;



    info.imageFlag        = m_imageWriteProtected;
    info.userSetting      = m_userWriteProtected;
    info.readOnlyFile     = m_fileReadOnly;
    info.noPermission     = m_fileNoPermission;
    info.checksumMismatch = m_sourceCrcMismatch;

    return info;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSourceFormat
//
////////////////////////////////////////////////////////////////////////////////

DiskFormat DiskImage::GetSourceFormat() const
{
    return m_format;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsTrackDirty
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImage::IsTrackDirty (int track) const
{
    // Short-circuit order guards the lookup; a track that does not exist
    // cannot have unsaved changes.
    return track >= 0
           && track < static_cast<int> (m_trackDirty.size())
           && m_trackDirty[track];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImage::MarkTrackDirty
//
//  Says a track's bits were replaced by something other than WriteBit.
//
//  THE BULK WRITERS NEED THIS AND THE GUEST DOES NOT. WriteBit records the
//  change as it makes it; the bulk paths take the buffer through
//  GetTrackBitsForWrite and write into it directly, so nothing records
//  anything. That was invisible for as long as the only serializer rebuilt
//  every track regardless -- and stopped being invisible the moment one of
//  them copied clean tracks and re-derived dirty ones, which read a freshly
//  re-encoded track as untouched and copied the old bytes over the edit.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::MarkTrackDirty (int track)
{
    if (track >= 0 && track < static_cast<int> (m_trackDirty.size()))
    {
        m_trackDirty[track] = true;
        m_dirty             = true;

        TouchTrack (track);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearDirty
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::ClearDirty()
{

    m_dirty = false;

    // vector<bool> hands out a proxy, so a range-for element would have to be
    // auto&& to write through it -- and a plain uto would silently clear a
    // copy. assign says the whole thing in one line and dodges that entirely.
    m_trackDirty.assign (m_trackDirty.size(), false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Serialize
//
//  For sector-based formats (DSK/DO/PO) this calls back into the
//  NibblizationLayer to recover the flat sector image. For WOZ images it
//  re-emits a WOZ v2 byte image from the live per-track bit streams via
//  WozLoader::Serialize, so guest writes round-trip on flush.
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
            hr = WozLoader::Serialize (*this, outBytes);
            break;

        case DiskFormat::Nib:
            //  NOT through Denibblize. A nibble image's file format IS the byte
            //  stream, so writing one back means re-deriving bytes and nothing
            //  else -- no sector decode is involved. That is what makes a track
            //  which does not decode to standard sectors cost this path nothing,
            //  which matters because such tracks are much of the point of the
            //  format. The source bytes go in so untouched tracks are copied
            //  rather than re-derived.
            hr = NibbleImageCodec::Serialize (*this, m_rawSourceBytes, outBytes);
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



    if (track < 0 || track >= static_cast<int> (m_trackBits.size()))
    {
        return;
    }

    bytesNeeded = (bitCount + 7) / 8;

    m_trackBits[track].assign (bytesNeeded, 0);
    m_trackBitCounts[track] = bitCount;

    TouchTrack (track);
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
    if (track < 0 || track >= static_cast<int> (m_trackBitCounts.size()))
    {
        return;
    }

    m_trackBitCounts[track] = bitCount;

    TouchTrack (track);
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
    m_wozMetadata.Clear();
    InitWholeTrackMap();
    RenewIdentity();

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

        case DiskFormat::Nib:
            hr = NibbleImageCodec::Load (raw, *this);
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
        fileOk = file.good();
        CBR (fileOk);

        raw.resize (kDos33ImageSize);
        file.read (reinterpret_cast<char *> (raw.data()), kDos33ImageSize);
        bytesRead = file.gcount();
        CBR (bytesRead == kDos33ImageSize);
    }

    hr = LoadDsk (raw);
    CHR (hr);

    m_filePath       = filePath;
    m_loaded         = true;
    m_dirty          = false;
    m_format         = DiskFormat::Dsk;
    m_rawSourceBytes = move (raw);
    m_wozMetadata.Clear();
    InitWholeTrackMap();

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

void DiskImage::Eject()
{
    HRESULT   hr = S_OK;



    if (m_dirty && !IsWriteProtected())
    {
        hr = Flush();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_filePath.clear();
    m_rawSourceBytes.clear();
    m_wozMetadata.Clear();
    m_trackBits.assign      (kDefaultTrackCount, vector<Byte> ());
    m_trackBitCounts.assign (kDefaultTrackCount, 0);
    m_trackDirty.assign     (kDefaultTrackCount, false);
    InitWholeTrackMap();
    RenewIdentity();
    m_loaded              = false;
    m_dirty               = false;
    m_imageWriteProtected = false;
    m_userWriteProtected  = false;
    m_fileReadOnly        = false;
    m_fileNoPermission    = false;
    m_sourceCrcMismatch   = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flush
//
//  Serializes dirty track state back to the original file using the
//  appropriate inverse path. A synthesized image with no backing file has
//  nothing to write and simply stops being dirty.
//
//  Two behaviors here used to lose data. The write opened the target
//  directly, which truncates it before the first byte lands, and never
//  checked the stream afterwards -- so a full volume replaced a working
//  image with a truncated one and reported success. And a failed Serialize
//  fell back to writing the file's PRE-SESSION bytes over the user's disk,
//  returning S_OK: every guest write of that session silently reverted.
//
//  Both are gone. The bytes go through WriteFileAtomically, and a failed
//  serialize fails loudly and leaves the image dirty so a later flush
//  retries. A WOZ whose Serialize fails now refuses to save and says so,
//  where it used to quietly revert the file -- which is the point: writing
//  stale bytes and calling it success is never the right answer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::Flush()
{
    HRESULT       hr       = S_OK;
    bool          hasPath  = false;
    vector<Byte>  bytes;



    BAIL_OUT_IF (!m_dirty, S_OK);

    hasPath = !m_filePath.empty();

    if (hasPath)
    {
        hr = Serialize (bytes);
        CHR (hr);

        hr = DiskImageStore::WriteFileAtomically (m_filePath, bytes);
        CHR (hr);
    }

    m_dirty = false;
    ClearDirty();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TouchTrack
//
//  Gives a track a generation no earlier state of this medium had, so a
//  snapshot taken before the change no longer matches it. The counter only
//  ever rises, which keeps that true after a snapshot is restored and the
//  machine runs a different way from there.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::TouchTrack (int track)
{
    if (track >= 0 && track < static_cast<int> (m_trackGeneration.size()))
    {
        m_trackGeneration[track] = ++m_lastGeneration;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenewIdentity
//
//  A new medium (or none): no snapshot of the old one may share its buffers.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::RenewIdentity()
{
    m_imageId        = ++s_nextImageId;
    m_lastGeneration = 0;

    m_trackGeneration.assign (m_trackBits.size(), 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTrackGeneration
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DiskImage::GetTrackGeneration (int track) const
{
    bool  inRange = (track >= 0 && track < static_cast<int> (m_trackGeneration.size()));



    return inRange ? m_trackGeneration[track] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
//  Every track in full. A sharing writer gets each track as a buffer kept
//  until the track's next write, so saves in between share it rather than
//  copy it; the blob they stand for is the same.
//
//  The dirty flags are not here. They say what the host file lacks, which a
//  flush changes without the machine changing, so a snapshot that held them
//  would differ between a run that flushed and its replay that did not.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::SaveState (StateWriter & writer) const
{
    constexpr size_t  kHeaderBytes = sizeof (uint64_t) + sizeof (uint32_t);
    Byte              header[kHeaderBytes];
    size_t            track        = 0;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteBool   (m_loaded);
    writer.WriteUInt32 (static_cast<uint32_t> (m_trackBits.size()));

    for (track = 0; track < m_trackBits.size(); track++)
    {
        // The bit count and the byte count, as WriteUInt64 and WriteUInt32
        // would put them, in one write: a ring checkpoint saves every track,
        // so this runs dozens of times a frame.
        MakeTrackHeader (m_trackBitCounts[track], static_cast<uint32_t> (m_trackBits[track].size()), header);

        writer.WriteBytes (header, kHeaderBytes);

        if (writer.IsSharing())
        {
            writer.WriteShared (GetSharedTrack (track), m_trackBits[track].data());
        }
        else
        {
            writer.WriteBytes (m_trackBits[track].data(), m_trackBits[track].size());
        }
    }

    writer.WriteBool (m_imageWriteProtected);
    writer.WriteBool (m_userWriteProtected);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeTrackHeader
//
//  One track's header in SaveState's stream form: the bit count as eight
//  bytes and the byte count as four, little-endian.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImage::MakeTrackHeader (
    uint64_t    bitCount,
    uint32_t    byteCount,
    Byte      * outHeader)
{
    size_t  i = 0;



    for (i = 0; i < sizeof (bitCount); i++)
    {
        outHeader[i] = static_cast<Byte> (bitCount >> (i * CHAR_BIT));
    }

    for (i = 0; i < sizeof (byteCount); i++)
    {
        outHeader[sizeof (bitCount) + i] = static_cast<Byte> (byteCount >> (i * CHAR_BIT));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSharedTrack
//
//  The track's bits as an immutable buffer: the one made last time while the
//  medium and the track's generation are unchanged, since a generation is
//  renewed on every write, or a fresh copy.
//
////////////////////////////////////////////////////////////////////////////////

const shared_ptr<const vector<Byte>> & DiskImage::GetSharedTrack (size_t track) const
{
    SharedTrack  * shared = nullptr;



    if (m_sharedImageId != m_imageId || m_sharedTracks.size() != m_trackBits.size())
    {
        m_sharedTracks.assign (m_trackBits.size(), SharedTrack());
        m_sharedImageId = m_imageId;
    }

    shared = &m_sharedTracks[track];

    if (shared->bits == nullptr || shared->generation != m_trackGeneration[track])
    {
        shared->bits       = make_shared<const vector<Byte>> (m_trackBits[track]);
        shared->generation = m_trackGeneration[track];
    }

    return shared->bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  The medium must already be mounted: a saved state whose loaded flag or
//  track count differs from this image's belongs to another disk. Each track's
//  byte size is capped before it is allocated, and its bit count must fit in
//  its bytes, so a corrupt blob cannot size a buffer or send ReadBit past one.
//  The tracks are read into a staging copy and committed only after the
//  section closes cleanly.
//
//  A track whose bits the load changes is marked dirty, and the dirty flags of
//  the rest are kept: the host file holds what the disk held before the load,
//  so a track that now differs from it is one the next flush must write. That
//  is what makes a flush after a step back write the disk at that position.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImage::LoadState (StateReader & reader)
{
    HRESULT               hr                  = S_OK;
    uint16_t              version             = 0;
    bool                  loaded              = false;
    uint32_t              trackCount          = 0;
    uint32_t              track               = 0;
    uint32_t              byteCount           = 0;
    uint64_t              bitCount            = 0;
    uint64_t              maxBits             = 0;
    bool                  isChanged           = false;
    bool                  imageWriteProtected = false;
    bool                  userWriteProtected  = false;
    vector<vector<Byte>>  trackBits;
    vector<size_t>        trackBitCounts;
    uint32_t              diskTracks          = static_cast<uint32_t> (m_trackBits.size());



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    // Version 1 carried the dirty flags.
    CBREx (version == kStateVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    reader.ReadBool   (loaded);
    reader.ReadUInt32 (trackCount);

    CBREx (loaded     == m_loaded,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (trackCount == diskTracks, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    trackBits.resize      (trackCount);
    trackBitCounts.resize (trackCount, 0);

    for (track = 0; track < trackCount; track++)
    {
        reader.ReadUInt64 (bitCount);
        reader.ReadUInt32 (byteCount);

        maxBits = static_cast<uint64_t> (byteCount) * CHAR_BIT;

        CBREx (byteCount <= kMaxStateTrackSize, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        CBREx (bitCount  <= maxBits,            HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        trackBits[track].resize (byteCount);
        reader.ReadBytes (trackBits[track].data(), byteCount);

        trackBitCounts[track] = static_cast<size_t> (bitCount);
    }

    reader.ReadBool (imageWriteProtected);
    reader.ReadBool (userWriteProtected);

    hr = reader.EndSection();
    CHR (hr);

    for (track = 0; track < trackCount; track++)
    {
        isChanged = trackBitCounts[track] != m_trackBitCounts[track] || trackBits[track] != m_trackBits[track];

        if (isChanged)
        {
            m_trackDirty[track] = true;
            m_dirty             = true;
        }
    }

    m_trackBits.swap      (trackBits);
    m_trackBitCounts.swap (trackBitCounts);

    m_imageWriteProtected = imageWriteProtected;
    m_userWriteProtected  = userWriteProtected;

    for (track = 0; track < trackCount; track++)
    {
        TouchTrack (static_cast<int> (track));
    }

Error:
    return hr;
}


