#include "Pch.h"

#include "Dos33Volume.h"
#include "Dos33Skeleton.h"
#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Dos33Volume
//
////////////////////////////////////////////////////////////////////////////////

Dos33Volume::Dos33Volume (const vector<Byte> & sectors)
    : m_sectors (sectors)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::ToUnit
//
//  One flat address per sector, so the integrity pass can talk about a DOS 3.3
//  volume without knowing anything about tracks.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Dos33Volume::ToUnit (int track, int sector)
{
    return (uint32_t) (track * NibblizationLayer::kSectorsPerTrack + sector);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::TrackOf
//
////////////////////////////////////////////////////////////////////////////////

int Dos33Volume::TrackOf (uint32_t unit)
{
    return (int) (unit / (uint32_t) NibblizationLayer::kSectorsPerTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::SectorOf
//
////////////////////////////////////////////////////////////////////////////////

int Dos33Volume::SectorOf (uint32_t unit)
{
    return (int) (unit % (uint32_t) NibblizationLayer::kSectorsPerTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::AllocationTrackAt
//
//  The order free sectors are handed out in: track 18 outward to the rim, then
//  track 16 inward to track 0. That is what the VTOC's own allocation hint and
//  direction byte describe, and it keeps a file's sectors near its catalog.
//
//  The catalog track itself is never produced, so it cannot be handed out even
//  if its free bitmap were wrong.
//
////////////////////////////////////////////////////////////////////////////////

int Dos33Volume::AllocationTrackAt (int index)
{
    constexpr int  kOutwardCount = NibblizationLayer::kTrackCount - Dos33Skeleton::kVtocTrack - 1;



    if (index < kOutwardCount)
    {
        return Dos33Skeleton::kVtocTrack + 1 + index;
    }

    return Dos33Skeleton::kVtocTrack - 1 - (index - kOutwardCount);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::IsFreeInBitmap
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Volume::IsFreeInBitmap (const vector<Byte> & buffer, int track, int sector)
{
    size_t  at     = 0;
    size_t  byteAt = 0;
    int     bit    = 0;



    if (track < 0 || track >= NibblizationLayer::kTrackCount
     || sector < 0 || sector >= NibblizationLayer::kSectorsPerTrack)
    {
        return false;
    }

    at     = Dos33Skeleton::SectorOffset (Dos33Skeleton::kVtocTrack, 0)
           + (size_t) Dos33Skeleton::kVtocOffFreeBitmap
           + (size_t) track * kBitmapBytesPerTrack;
    byteAt = (sector >= kBitmapSplitSector) ? at : at + 1;
    bit    = (sector >= kBitmapSplitSector) ? (sector - kBitmapSplitSector) : sector;

    if (byteAt >= buffer.size())
    {
        return false;
    }

    return ((buffer[byteAt] >> bit) & 1) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::SetFreeInBitmap
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Volume::SetFreeInBitmap (vector<Byte> & buffer, int track, int sector, bool isFree)
{
    size_t  at     = 0;
    size_t  byteAt = 0;
    int     bit    = 0;



    if (track < 0 || track >= NibblizationLayer::kTrackCount
     || sector < 0 || sector >= NibblizationLayer::kSectorsPerTrack)
    {
        return;
    }

    at     = Dos33Skeleton::SectorOffset (Dos33Skeleton::kVtocTrack, 0)
           + (size_t) Dos33Skeleton::kVtocOffFreeBitmap
           + (size_t) track * kBitmapBytesPerTrack;
    byteAt = (sector >= kBitmapSplitSector) ? at : at + 1;
    bit    = (sector >= kBitmapSplitSector) ? (sector - kBitmapSplitSector) : sector;

    if (byteAt >= buffer.size())
    {
        return;
    }

    if (isFree)
    {
        buffer[byteAt] = (Byte) (buffer[byteAt] | (1 << bit));
    }
    else
    {
        buffer[byteAt] = (Byte) (buffer[byteAt] & ~(1 << bit));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::WriteByteAt
//
//  Out-of-range writes do nothing, mirroring ReadByte. Nothing here addresses a
//  sector it did not first obtain from the allocator or the catalog walk, so
//  this is a backstop rather than a supported way to address the volume.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Volume::WriteByteAt (vector<Byte> & buffer, int track, int sector, size_t offset, Byte value)
{
    size_t  at = 0;



    if (track < 0 || track >= NibblizationLayer::kTrackCount
     || sector < 0 || sector >= NibblizationLayer::kSectorsPerTrack)
    {
        return;
    }

    at = Dos33Skeleton::SectorOffset (track, sector) + offset;

    if (at >= buffer.size())
    {
        return;
    }

    buffer[at] = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::ReadByte
//
//  Out-of-range reads answer zero rather than asserting. Every caller here is
//  following a pointer that came off the disk, and a damaged disk names sectors
//  that do not exist -- that is a finding to report, not a programming error.
//
////////////////////////////////////////////////////////////////////////////////

Byte Dos33Volume::ReadByte (int track, int sector, size_t offset) const
{
    size_t  at = 0;



    if (track < 0 || track >= NibblizationLayer::kTrackCount
     || sector < 0 || sector >= NibblizationLayer::kSectorsPerTrack)
    {
        return 0;
    }

    at = Dos33Skeleton::SectorOffset (track, sector) + offset;

    if (at >= m_sectors.size())
    {
        return 0;
    }

    return m_sectors[at];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::CollectEntries
//
//  Walks the catalog chain from the VTOC, collecting every entry it can read.
//
//  Damage is collected alongside the entries rather than aborting the walk: the
//  developer this serves is recovering old disks, and a catalog that goes bad
//  halfway is exactly when the first half matters most. The chain itself is
//  guarded, because a corrupted next-pointer on the catalog track loops as
//  readily as one in a file.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Volume::CollectEntries (
    vector<RawEntry>    & outEntries,
    vector<std::string> & outDamage,
    bool                & outFullyParsed) const
{
    ChainWalkGuard  guard (kUnitCount);
    int             track  = 0;
    int             sector = 0;
    int             slot   = 0;
    size_t          i      = 0;
    bool            more   = true;



    outFullyParsed = true;

    track  = ReadByte (Dos33Skeleton::kVtocTrack, 0, Dos33Skeleton::kVtocOffCatalogTrack);
    sector = ReadByte (Dos33Skeleton::kVtocTrack, 0, Dos33Skeleton::kVtocOffCatalogSector);

    while (more && track != 0)
    {
        bool  stepOk = guard.TryVisit (ToUnit (track, sector));

        if (!stepOk)
        {
            outDamage.push_back ("the catalog chain loops or leaves the volume; "
                                 "the entries listed are those reachable before it did");
            outFullyParsed = false;
            break;
        }

        if (track >= NibblizationLayer::kTrackCount
         || sector >= NibblizationLayer::kSectorsPerTrack)
        {
            outDamage.push_back ("the catalog chain names a sector outside the volume");
            outFullyParsed = false;
            break;
        }

        for (slot = 0; slot < kEntriesPerSector; slot++)
        {
            RawEntry  entry;
            Byte      tsTrack = 0;

            entry.entryOffset   = kEntryBase + (size_t) slot * kEntrySize;
            entry.catalogTrack  = track;
            entry.catalogSector = sector;

            tsTrack = ReadByte (track, sector, entry.entryOffset + kEntOffTsTrack);

            if (tsTrack == kEntryUnused)
            {
                continue;
            }

            entry.isDeleted    = tsTrack == kEntryDeleted;
            entry.tsListTrack  = tsTrack;
            entry.tsListSector = ReadByte (track, sector, entry.entryOffset + kEntOffTsSector);
            entry.typeByte     = ReadByte (track, sector, entry.entryOffset + kEntOffType);
            entry.sectorCount  = (uint16_t)
                (ReadByte (track, sector, entry.entryOffset + kEntOffSectorCount)
              | (ReadByte (track, sector, entry.entryOffset + kEntOffSectorCount + 1) << 8));

            // Names are high ASCII padded with high-ASCII spaces. Strip the high
            // bit and the padding; keep interior spaces, which are legal.
            for (i = 0; i < kNameBytes; i++)
            {
                Byte  c = ReadByte (track, sector, entry.entryOffset + kEntOffName + i);

                entry.name += (char) (c & 0x7F);
            }

            while (!entry.name.empty() && entry.name.back() == ' ')
            {
                entry.name.pop_back();
            }

            if (!entry.isDeleted)
            {
                outEntries.push_back (entry);
            }
        }

        {
            int  nextTrack  = ReadByte (track, sector, Dos33Skeleton::kCatalogOffNextTrack);
            int  nextSector = ReadByte (track, sector, Dos33Skeleton::kCatalogOffNextSector);

            more   = nextTrack != 0;
            track  = nextTrack;
            sector = nextSector;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::CollectDataSectors
//
//  Walks a file's track/sector list, gathering its data sectors in order.
//
//  Both the list chain and the data sectors go through the guard, so a file
//  whose list points back into itself -- or at another file's list -- stops
//  rather than spinning. Returns false when the walk hit a bound.
//
//  AN ENTRY THAT CLAIMS NO SECTORS IS NOT A BROKEN CHAIN. Real disks carry
//  catalog entries that occupy no storage at all -- Merlin Pro's own disk uses
//  them to draw section headings in CATALOG, with a sector count of zero and a
//  track/sector pointer of $7F/$7F that was never meant to be followed. DOS
//  lists them without complaint, so a reader that called them damaged would
//  report a shipped disk as broken.
//
//  The discriminator is the entry's own sector count, not the pointer: zero
//  sectors means the entry declares it occupies nothing, so there is nothing to
//  reach and nothing lost. A pointer that leads nowhere while the entry claims
//  sectors IS damage, and stays reported.
//
//  A zero track/sector pair is a sparse hole, which DOS 3.3 writes for a file
//  with gaps. It is kept in the list and read back as zeros, because that is
//  what the guest sees.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Volume::CollectDataSectors (
    const RawEntry    & entry,
    vector<uint32_t>  & outUnits,
    vector<uint32_t>  & outListUnits,
    ChainWalkGuard    & guard) const
{
    int   listTrack  = entry.tsListTrack;
    int   listSector = entry.tsListSector;
    int   pair       = 0;
    bool  ok         = true;



    // An entry occupying no sectors has no chain, which is a fact about the
    // entry rather than a failure to read one.
    if (entry.sectorCount == 0)
    {
        return true;
    }

    while (ok && listTrack != 0)
    {
        bool  stepOk = guard.TryVisit (ToUnit (listTrack, listSector));

        if (!stepOk || listTrack >= NibblizationLayer::kTrackCount
                    || listSector >= NibblizationLayer::kSectorsPerTrack)
        {
            ok = false;
            break;
        }

        // EVERY list sector, not merely the first. A file large enough to need
        // a second list occupies that sector as surely as it occupies its data,
        // and an accounting that saw only the head left the rest allocated with
        // nothing owning it -- which a delete would then fail to return.
        outListUnits.push_back (ToUnit (listTrack, listSector));

        for (pair = 0; pair < kTsPairsPerList; pair++)
        {
            size_t  at    = kTsOffFirstPair + (size_t) pair * 2;
            Byte    dataT = ReadByte (listTrack, listSector, at);
            Byte    dataS = ReadByte (listTrack, listSector, at + 1);

            if (dataT == 0 && dataS == 0)
            {
                continue;   // sparse hole -- reads as zeros
            }

            if (dataT >= NibblizationLayer::kTrackCount
             || dataS >= NibblizationLayer::kSectorsPerTrack)
            {
                ok = false;
                break;
            }

            outUnits.push_back (ToUnit (dataT, dataS));
        }

        // Both halves of the next pointer come from the sector being left, and
        // must be read BEFORE either is overwritten. Taking the sector half
        // from the file's head instead walks correctly on the first hop -- the
        // head IS the current sector there -- and then loops on the second,
        // which reports a healthy file as an unfollowable chain.
        {
            int  nextTrack  = ReadByte (listTrack, listSector, kTsOffNextTrack);
            int  nextSector = ReadByte (listTrack, listSector, kTsOffNextSector);

            listTrack  = nextTrack;
            listSector = nextSector;
        }
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::ApplyTypeHeader
//
//  DOS 3.3 stores a file's length -- and, for binaries, its load address --
//  inside the file's own first bytes rather than in the catalog. Stripping that
//  header is what turns "the sectors this file occupies" into "the file".
//
//  A header claiming more than the sectors hold is trusted only as far as the
//  data goes: a truncated file is reported at the length actually present
//  rather than padded out to the length it claims.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Volume::ApplyTypeHeader (Byte typeByte, vector<Byte> & inOutBytes, FilePayload & outPayload)
{
    Byte    type    = (Byte) (typeByte & ~kLockedBit);
    size_t  claimed = 0;
    size_t  header  = 0;



    if (type == kTypeBinary)
    {
        if (inOutBytes.size() < kBinaryHeaderBytes)
        {
            return;
        }

        outPayload.loadAddress    = (Word) (inOutBytes[0] | (inOutBytes[1] << 8));
        outPayload.hasLoadAddress = true;

        claimed = (size_t) (inOutBytes[2] | (inOutBytes[3] << 8));
        header  = kBinaryHeaderBytes;
    }
    else if (type == kTypeApplesoft || type == kTypeInteger)
    {
        if (inOutBytes.size() < kTokenizedHeaderBytes)
        {
            return;
        }

        claimed = (size_t) (inOutBytes[0] | (inOutBytes[1] << 8));
        header  = kTokenizedHeaderBytes;
    }
    else if (type == kTypeText)
    {
        // Sequential text ends at the first zero byte; the rest of the last
        // sector is padding the guest never sees.
        size_t  end = 0;

        while (end < inOutBytes.size() && inOutBytes[end] != 0)
        {
            end++;
        }

        inOutBytes.resize (end);
        return;
    }
    else
    {
        return;   // no header this layer knows how to strip
    }

    inOutBytes.erase (inOutBytes.begin(), inOutBytes.begin() + (ptrdiff_t) header);

    if (claimed < inOutBytes.size())
    {
        inOutBytes.resize (claimed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Enumerate (VolumeListing & outListing) const
{
    HRESULT           hr          = S_OK;
    size_t            bufferBytes = m_sectors.size();
    vector<RawEntry>  entries;
    bool              fullyParsed = true;
    int               track       = 0;
    int               sector      = 0;
    uint32_t          freeUnits   = 0;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    outListing = VolumeListing();

    outListing.volumeNumber    = ReadByte (Dos33Skeleton::kVtocTrack, 0,
                                           Dos33Skeleton::kVtocOffVolumeNumber);
    outListing.hasVolumeNumber = true;
    outListing.totalUnits      = kUnitCount;

    CollectEntries (entries, outListing.damage, fullyParsed);

    for (const RawEntry & entry : entries)
    {
        FileEntry  listed;

        listed.name        = entry.name;
        listed.type        = (Byte) (entry.typeByte & ~kLockedBit);
        listed.isLocked    = (entry.typeByte & kLockedBit) != 0;
        listed.sizeUnits   = entry.sectorCount;
        listed.isDirectory = false;

        outListing.entries.push_back (listed);
    }

    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        for (sector = 0; sector < NibblizationLayer::kSectorsPerTrack; sector++)
        {
            bool  isFree = IsFreeInBitmap (m_sectors, track, sector);

            freeUnits += isFree ? 1u : 0u;
        }
    }

    outListing.freeUnits = freeUnits;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Read
//
//  DOS 3.3 has no subdirectories, so a path with more than one component names
//  something this filesystem cannot express. Refusing it is the honest answer;
//  quietly using the last component would open a different file than the caller
//  asked for.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT              hr          = S_OK;
    size_t               bufferBytes = m_sectors.size();
    bool                 single      = path.IsSingleComponent();
    bool                 found       = false;
    bool                 walked      = false;
    bool                 fullyParsed = true;
    uint16_t             owner       = 0;
    vector<RawEntry>     entries;
    vector<uint32_t>     units;
    vector<std::string>  damage;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBREx (single, E_INVALIDARG);

    CollectEntries (entries, damage, fullyParsed);

    found = TryFindEntry (entries, path.GetLeaf(), owner);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    {
        ChainWalkGuard    guard (kUnitCount);
        vector<uint32_t>  listUnits;

        walked = CollectDataSectors (entries[owner], units, listUnits, guard);
    }

    CBREx (walked, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    outPayload          = FilePayload();
    outPayload.type     = (Byte) (entries[owner].typeByte & ~kLockedBit);
    outPayload.encoding = PayloadEncoding::Verbatim;

    for (uint32_t unit : units)
    {
        int     track  = TrackOf (unit);
        int     sector = SectorOf (unit);
        size_t  i      = 0;

        for (i = 0; i < (size_t) NibblizationLayer::kSectorByteSize; i++)
        {
            outPayload.bytes.push_back (ReadByte (track, sector, i));
        }
    }

    ApplyTypeHeader (entries[owner].typeByte, outPayload.bytes, outPayload);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::ClaimVolumeStructures
//
//  The tracks a formatted volume reserves for itself: 0-2, where DOS lives on a
//  bootable disk and which every 1980s formatter kept allocated on a data disk
//  too, and 17, which holds the VTOC and the catalog.
//
//  Sixty-four sectors, named by nothing in the catalog. Crediting them to the
//  volume is what makes the free map and the claim map agree on a healthy disk;
//  without it every DOS 3.3 volume reports those sixty-four as space allocated
//  to nobody, which is indistinguishable from the leak a bad delete leaves.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Volume::ClaimVolumeStructures (VolumeIntegrityReport & outReport)
{
    int  track  = 0;
    int  sector = 0;



    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        bool  reserved = track < Dos33Skeleton::kDosImageTracks
                      || track == Dos33Skeleton::kVtocTrack;

        if (!reserved)
        {
            continue;
        }

        for (sector = 0; sector < NibblizationLayer::kSectorsPerTrack; sector++)
        {
            outReport.AddClaim (ToUnit (track, sector), VolumeIntegrityReport::kVolumeOwner);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::BuildIntegrityReport
//
//  Every catalog entry's chain walked into the claim map, then compared against
//  the VTOC's own free bitmap. The volume's own tracks -- the boot area and the
//  catalog track -- are claimed too, under a reserved owner: they are as
//  allocated as any file's sectors, and a report that ignored them would call
//  every volume inconsistent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    HRESULT              hr          = S_OK;
    size_t               bufferBytes = m_sectors.size();
    vector<RawEntry>     entries;
    vector<std::string>  damage;
    bool                 fullyParsed = true;
    uint16_t             owner       = 0;
    int                  track       = 0;
    int                  sector      = 0;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    outReport.Reset (kUnitCount);

    CollectEntries (entries, damage, fullyParsed);
    outReport.SetCatalogFullyParsed (fullyParsed);

    ClaimVolumeStructures (outReport);

    for (owner = 0; owner < (uint16_t) entries.size(); owner++)
    {
        ChainWalkGuard    guard (kUnitCount);
        vector<uint32_t>  units;
        vector<uint32_t>  listUnits;
        bool              walked = CollectDataSectors (entries[owner], units, listUnits, guard);

        // The list sectors themselves are part of the file's footprint. An
        // entry occupying no sectors contributes none of either: the walk
        // returns before it starts, so a decorative entry's $7F/$7F pointer is
        // never treated as a sector it owns.
        for (uint32_t unit : listUnits)
        {
            outReport.AddClaim (unit, owner);
        }

        for (uint32_t unit : units)
        {
            outReport.AddClaim (unit, owner);
        }

        if (!walked)
        {
            outReport.MarkChainUnfollowable (owner);
        }
    }

    // The free bitmap's view. A CLEAR bit means the sector is in use.
    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        for (sector = 0; sector < NibblizationLayer::kSectorsPerTrack; sector++)
        {
            bool  isFree = IsFreeInBitmap (m_sectors, track, sector);

            outReport.SetAllocatedInFreeMap (ToUnit (track, sector), !isFree);
        }
    }

    outReport.Finish();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::TryFindEntry
//
//  Case-insensitive, because the catalog is upper case and the shell the caller
//  typed into is not.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Volume::TryFindEntry (
    const vector<RawEntry>  & entries,
    const std::string       & leaf,
    uint16_t                & outOwner)
{
    uint16_t  index = 0;



    for (index = 0; index < (uint16_t) entries.size(); index++)
    {
        if (_stricmp (entries[index].name.c_str(), leaf.c_str()) == 0)
        {
            outOwner = index;

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::TryEncodeCatalogName
//
//  Thirty bytes of high ASCII, padded with high-ASCII spaces.
//
//  THIS VALIDATES ONLY NAMES BEING CREATED, and that distinction matters. A
//  printable-text check on names being READ was tried and reverted: it rejects
//  twenty of the sixty-three entries on a disk Merlin shipped, whose names are a
//  high-ASCII letter followed by eight backspace characters so that DOS draws a
//  heading at column zero. Reading imposes no rule at all.
//
//  Creating one is the opposite direction. DOS's own SAVE and BSAVE refuse a
//  name that does not begin with a letter, and a comma ends a filename in every
//  DOS command line, so a name breaking either is one the guest cannot open
//  again. Refusing to create it costs nothing and rejects nothing that exists.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Volume::TryEncodeCatalogName (const std::string & name, vector<Byte> & outBytes)
{
    size_t  length = name.size();
    size_t  i      = 0;
    Byte    first  = 0;



    if (length == 0 || length > kNameBytes)
    {
        return false;
    }

    first = (Byte) name[0];

    if (!(first >= 'A' && first <= 'Z') && !(first >= 'a' && first <= 'z'))
    {
        return false;
    }

    for (i = 0; i < length; i++)
    {
        Byte  c = (Byte) name[i];

        if (c == ',' || c < 0x20 || c >= 0x7F)
        {
            return false;
        }
    }

    outBytes.assign (kNameBytes, kNamePad);

    for (i = 0; i < length; i++)
    {
        Byte  c = (Byte) name[i];

        // The guest's keyboard produces upper case and its catalog stores it,
        // so a lower-case name would list as something nobody can type.
        if (c >= 'a' && c <= 'z')
        {
            c = (Byte) (c - 'a' + 'A');
        }

        outBytes[i] = (Byte) (c | 0x80);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::ComposeStoredBytes
//
//  The exact inverse of ApplyTypeHeader: DOS 3.3 records a file's length -- and
//  a binary's load address -- inside the file's own first bytes, so placing a
//  file means rebuilding that header rather than storing the payload as it came.
//
//  A binary with no load address is refused rather than defaulted. $0000 is a
//  legal load address, so a default would be indistinguishable from an answer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::ComposeStoredBytes (const FilePayload & payload, vector<Byte> & outStored)
{
    HRESULT  hr      = S_OK;
    Byte     type    = (Byte) (payload.type & ~kLockedBit);
    size_t   length  = payload.bytes.size();
    bool     fits    = length <= 0xFFFF;
    bool     hasAddr = payload.hasLoadAddress;



    outStored.clear();

    if (type == kTypeBinary)
    {
        CBREx (hasAddr, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER));
        CBREx (fits,    HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));

        outStored.push_back ((Byte) (payload.loadAddress & 0xFF));
        outStored.push_back ((Byte) (payload.loadAddress >> 8));
        outStored.push_back ((Byte) (length & 0xFF));
        outStored.push_back ((Byte) ((length >> 8) & 0xFF));
    }
    else if (type == kTypeApplesoft || type == kTypeInteger)
    {
        CBREx (fits, HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));

        outStored.push_back ((Byte) (length & 0xFF));
        outStored.push_back ((Byte) ((length >> 8) & 0xFF));
    }

    outStored.insert (outStored.end(), payload.bytes.begin(), payload.bytes.end());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::TryFindFreeCatalogSlot
//
//  The first slot in the catalog chain whose track byte says never-used or
//  tombstoned.
//
//  A SLOT OCCUPYING NO SECTORS IS STILL OCCUPIED. Twenty of the sixty-three
//  entries on Merlin's own disk claim zero sectors and exist to draw headings in
//  CATALOG; reusing one would delete a vendor's disk decoration to make room,
//  and the user would see a file appear where a heading used to be. The sector
//  count is deliberately not consulted here.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Volume::TryFindFreeCatalogSlot (int & outTrack, int & outSector, size_t & outOffset) const
{
    ChainWalkGuard  guard (kUnitCount);
    int             track  = 0;
    int             sector = 0;
    int             slot   = 0;
    bool            more   = true;



    track  = ReadByte (Dos33Skeleton::kVtocTrack, 0, Dos33Skeleton::kVtocOffCatalogTrack);
    sector = ReadByte (Dos33Skeleton::kVtocTrack, 0, Dos33Skeleton::kVtocOffCatalogSector);

    while (more && track != 0)
    {
        bool  stepOk = guard.TryVisit (ToUnit (track, sector));

        if (!stepOk || track >= NibblizationLayer::kTrackCount
                    || sector >= NibblizationLayer::kSectorsPerTrack)
        {
            break;
        }

        for (slot = 0; slot < kEntriesPerSector; slot++)
        {
            size_t  entryOffset = kEntryBase + (size_t) slot * kEntrySize;
            Byte    tsTrack     = ReadByte (track, sector, entryOffset + kEntOffTsTrack);

            if (tsTrack == kEntryUnused || tsTrack == kEntryDeleted)
            {
                outTrack  = track;
                outSector = sector;
                outOffset = entryOffset;

                return true;
            }
        }

        {
            int  nextTrack  = ReadByte (track, sector, Dos33Skeleton::kCatalogOffNextTrack);
            int  nextSector = ReadByte (track, sector, Dos33Skeleton::kCatalogOffNextSector);

            more   = nextTrack != 0;
            track  = nextTrack;
            sector = nextSector;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::TryAllocateUnits
//
//  Hands out exactly `count` sectors or none at all, so a refusal leaves nothing
//  half-reserved.
//
//  A sector must be free in the VTOC bitmap AND unclaimed by the catalog. The
//  second condition is the one that matters: a bitmap calling a sector free
//  while a catalog entry still points at it is the disagreement that hands the
//  next file someone else's live data, and it is exactly the state a previous
//  bad delete leaves behind. Skipping those sectors costs a little space and
//  leaves the disagreement for the integrity report to name.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Volume::TryAllocateUnits (
    const VolumeIntegrityReport  & report,
    size_t                         count,
    vector<uint32_t>             & outUnits) const
{
    constexpr int  kAllocatableTracks = NibblizationLayer::kTrackCount - 1;
    int            slot               = 0;
    size_t         taken              = 0;



    outUnits.clear();

    for (slot = 0; slot < kAllocatableTracks && taken < count; slot++)
    {
        int  track  = AllocationTrackAt (slot);
        int  sector = 0;

        for (sector = NibblizationLayer::kSectorsPerTrack - 1; sector >= 0 && taken < count; sector--)
        {
            uint32_t  unit    = ToUnit (track, sector);
            bool      isFree  = IsFreeInBitmap (m_sectors, track, sector);
            bool      claimed = report.IsClaimed (unit);

            if (isFree && !claimed)
            {
                outUnits.push_back (unit);
                taken++;
            }
        }
    }

    return taken == count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::ZeroUnits
//
//  Every sector a new file occupies is cleared before anything is written into
//  it. A track/sector list still holding a previous file's pairs would be read
//  as this file's data, and the tail of a partly filled last sector would hand
//  back bytes nobody ever wrote.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Volume::ZeroUnits (vector<Byte> & buffer, const vector<uint32_t> & units)
{
    for (uint32_t unit : units)
    {
        size_t  at = Dos33Skeleton::SectorOffset (TrackOf (unit), SectorOf (unit));

        if (at + (size_t) NibblizationLayer::kSectorByteSize <= buffer.size())
        {
            std::fill (buffer.begin() + (ptrdiff_t) at,
                       buffer.begin() + (ptrdiff_t) (at + NibblizationLayer::kSectorByteSize),
                       (Byte) 0);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::PlaceFile
//
//  Lays the allocated sectors out as DOS does: a track/sector list, then the
//  data sectors it describes, then the next list, and so on. The allocator hands
//  the units over in exactly that order, so this walks them straight through.
//
//  Each list records the file-relative index of the first data sector it
//  describes, which is what lets a reader joining the chain partway know where
//  it is.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Volume::PlaceFile (
    vector<Byte>            & buffer,
    const vector<uint32_t>  & units,
    size_t                    dataSectorCount,
    const vector<Byte>      & stored)
{
    size_t  unitCount  = units.size();
    size_t  cursor     = 0;
    size_t  placed     = 0;
    size_t  storedSize = stored.size();



    while (cursor < unitCount)
    {
        uint32_t  listUnit   = units[cursor];
        int       listTrack  = TrackOf (listUnit);
        int       listSec    = SectorOf (listUnit);
        size_t    remaining  = dataSectorCount - placed;
        size_t    inThisList = std::min ((size_t) kTsPairsPerList, remaining);
        size_t    k          = 0;

        cursor++;

        WriteByteAt (buffer, listTrack, listSec, kTsOffSectorOffsetLo, (Byte) (placed & 0xFF));
        WriteByteAt (buffer, listTrack, listSec, kTsOffSectorOffsetHi, (Byte) ((placed >> 8) & 0xFF));

        for (k = 0; k < inThisList; k++)
        {
            uint32_t  dataUnit = units[cursor + k];
            size_t    pairAt   = kTsOffFirstPair + k * 2;
            size_t    from     = (placed + k) * (size_t) NibblizationLayer::kSectorByteSize;
            size_t    to       = Dos33Skeleton::SectorOffset (TrackOf (dataUnit), SectorOf (dataUnit));
            size_t    left     = (from < storedSize) ? storedSize - from : 0;
            size_t    copyable = std::min ((size_t) NibblizationLayer::kSectorByteSize, left);

            WriteByteAt (buffer, listTrack, listSec, pairAt,     (Byte) TrackOf (dataUnit));
            WriteByteAt (buffer, listTrack, listSec, pairAt + 1, (Byte) SectorOf (dataUnit));

            std::copy (stored.begin() + (ptrdiff_t) from,
                       stored.begin() + (ptrdiff_t) (from + copyable),
                       buffer.begin() + (ptrdiff_t) to);
        }

        cursor += inThisList;
        placed += inThisList;

        if (cursor < unitCount)
        {
            WriteByteAt (buffer, listTrack, listSec, kTsOffNextTrack,  (Byte) TrackOf (units[cursor]));
            WriteByteAt (buffer, listTrack, listSec, kTsOffNextSector, (Byte) SectorOf (units[cursor]));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::WriteCatalogEntry
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Volume::WriteCatalogEntry (
    vector<Byte>        & buffer,
    int                   catalogTrack,
    int                   catalogSector,
    size_t                entryOffset,
    uint32_t              listUnit,
    Byte                  typeByte,
    uint16_t              sectorCount,
    const vector<Byte>  & nameBytes)
{
    size_t  i = 0;



    WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffTsTrack,
                 (Byte) TrackOf (listUnit));
    WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffTsSector,
                 (Byte) SectorOf (listUnit));
    WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffType, typeByte);

    for (i = 0; i < kNameBytes; i++)
    {
        Byte  c = (i < nameBytes.size()) ? nameBytes[i] : kNamePad;

        WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffName + i, c);
    }

    WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffSectorCount,
                 (Byte) (sectorCount & 0xFF));
    WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffSectorCount + 1,
                 (Byte) (sectorCount >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::HandBackVerifiedResult
//
//  A write that never inspects what it produced is how a decoder shipped
//  returning success over a buffer it had partly zeroed. Every computed result
//  here is read back through the same integrity pass a user's disk goes
//  through, refused if the edit made the volume worse, and handed over only
//  once it survives that.
//
//  WORSE, not imperfect. Asking whether the result is CLEAN would refuse two
//  correct outcomes: a delete that declines to free a cross-linked sector
//  leaves space allocated with nothing claiming it, which is exactly what the
//  leak rule requires, and a volume that already carried a disagreement would
//  become permanently uneditable -- on the degraded disks this feature exists
//  to recover. The comparison is against the input, on the sets that matter.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::HandBackVerifiedResult (
    const VolumeIntegrityReport  & before,
    const vector<Byte>           & result,
    vector<Byte>                 & outBuffer)
{
    HRESULT                hr   = S_OK;
    Dos33Volume            volume (result);
    bool                   safe = false;
    VolumeIntegrityReport  after;



    hr = volume.BuildIntegrityReport (after);
    CHRA (hr);

    safe = after.IsSafeToCommitAfter (before);
    CBRAEx (safe, E_UNEXPECTED);

    outBuffer = result;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::AddFile
//
//  Allocate from the VTOC bitmap, build the track/sector lists, write the data
//  sectors, create the catalog entry, and leave the bitmap agreeing with what
//  was allocated.
//
//  Nothing is written into the volume this object holds. The complete new buffer
//  is produced instead, so a refusal at any point leaves the caller with exactly
//  what it had.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::AddFile (
    const vector<Byte>  & nameBytes,
    Byte                  typeByte,
    const vector<Byte>  & stored,
    vector<Byte>        & outBuffer) const
{
    HRESULT                hr         = S_OK;
    bool                   slotOk     = false;
    bool                   allocated  = false;
    int                    slotTrack  = 0;
    int                    slotSector = 0;
    size_t                 slotOffset = 0;
    size_t                 storedSize = stored.size();
    size_t                 dataCount  = 0;
    size_t                 listCount  = 0;
    vector<Byte>           result;
    vector<uint32_t>       units;
    VolumeIntegrityReport  report;



    slotOk = TryFindFreeCatalogSlot (slotTrack, slotSector, slotOffset);
    CBREx (slotOk, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    hr = BuildIntegrityReport (report);
    CHRA (hr);

    dataCount = (storedSize + (size_t) NibblizationLayer::kSectorByteSize - 1)
              / (size_t) NibblizationLayer::kSectorByteSize;
    listCount = (dataCount + (size_t) kTsPairsPerList - 1) / (size_t) kTsPairsPerList;

    // Even a file with no data occupies its track/sector list, which is what
    // makes it a file rather than a name.
    if (listCount == 0)
    {
        listCount = 1;
    }

    allocated = TryAllocateUnits (report, dataCount + listCount, units);
    CBREx (allocated, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    result = m_sectors;

    ZeroUnits (result, units);
    PlaceFile (result, units, dataCount, stored);

    for (uint32_t unit : units)
    {
        SetFreeInBitmap (result, TrackOf (unit), SectorOf (unit), false);
    }

    WriteCatalogEntry (result,
                       slotTrack,
                       slotSector,
                       slotOffset,
                       units[0],
                       typeByte,
                       (uint16_t) units.size(),
                       nameBytes);

    hr = HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Write
//
//  Adds a file, or replaces one of the same name.
//
//  REPLACEMENT IS COMPUTED AS ONE WHOLE. The removal is staged into a working
//  buffer that no caller can see, the new file is placed over that, and only a
//  finished buffer is ever handed back. What this must never be is a removal
//  APPLIED to the caller's image followed by a placement: a failure between the
//  two frees the old file without landing the new one, which loses the file
//  outright -- strictly worse than refusing.
//
//  The distinction is not merely internal ordering. Nothing here mutates the
//  volume's own buffer, so a refusal at any point, including one raised after
//  the staged removal, leaves the original image byte-for-byte as it was.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Write (
    const FilePath     & path,
    const FilePayload  & payload,
    vector<Byte>       & outBuffer) const
{
    HRESULT              hr          = S_OK;
    size_t               bufferBytes = m_sectors.size();
    bool                 single      = path.IsSingleComponent();
    bool                 nameOk      = false;
    bool                 exists      = false;
    bool                 isLocked    = false;
    bool                 fullyParsed = true;
    Byte                 typeByte    = (Byte) (payload.type & ~kLockedBit);
    uint16_t             owner       = 0;
    vector<Byte>         nameBytes;
    vector<Byte>         stored;
    vector<Byte>         staged;
    //  stagedVolume binds to `staged` by reference, and is read only once that
    //  buffer has been filled.
    vector<RawEntry>     entries;
    vector<std::string>  damage;
    DeleteOutcome        removal;
    Dos33Volume          stagedVolume (staged);



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    nameOk = single && TryEncodeCatalogName (path.GetLeaf(), nameBytes);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    hr = ComposeStoredBytes (payload, stored);
    CHR (hr);

    CollectEntries (entries, damage, fullyParsed);

    exists = TryFindEntry (entries, path.GetLeaf(), owner);

    if (exists)
    {
        isLocked = (entries[owner].typeByte & kLockedBit) != 0;
    }

    CBREx (!isLocked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    if (!exists)
    {
        hr = AddFile (nameBytes, typeByte, stored, outBuffer);
        CHR (hr);
    }
    else
    {
        // The removal is answered for by the same rules a bare delete follows,
        // including its refusal to hand back a sector another entry claims. The
        // replacement simply does not get that space.
        hr = Delete (path, staged, removal);
        CHRA (hr);

        hr = stagedVolume.AddFile (nameBytes, typeByte, stored, outBuffer);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::AppendDeleteWarnings
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Volume::AppendDeleteWarnings (DeleteOutcome & inOutOutcome)
{
    size_t  leaked = inOutOutcome.leakedUnits.size();



    if (leaked > 0)
    {
        inOutOutcome.warnings.push_back (
            std::to_string (leaked)
          + " sector(s) this file referenced are claimed by another catalog entry"
            " as well, and were left allocated rather than freed");
    }

    if (inOutOutcome.chainWasDamaged)
    {
        inOutOutcome.warnings.push_back (
            "this file's sector chain could not be followed to its end, so only"
            " the sectors the walk reached were considered");
    }

    if (!inOutOutcome.catalogFullyParsed)
    {
        inOutOutcome.warnings.push_back (
            "the catalog did not parse completely, so an entry that could not be"
            " read claims nothing this pass can see, and a sector it shares with the"
            " deleted file may have been freed while that entry still uses it");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Delete
//
//  The interface form. Everything it decided is available from the overload
//  below; this one discards it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    DeleteOutcome  outcome;



    return Delete (path, outBuffer, outcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Delete
//
//  Removes a catalog entry and returns ONLY the sectors the integrity report
//  shows this entry alone claims.
//
//  That restriction is the whole point. Freeing a sector a second file also
//  references does not fail: the delete succeeds, the volume looks healthy, and
//  the other file is destroyed the next time anything allocates -- possibly
//  weeks later, with nothing connecting the two events. Space left allocated
//  with nothing owning it is recoverable at any time; a sector handed out from
//  under a live file is not. So everything not uniquely owned is reported as
//  leaked and left exactly where it is.
//
//  A FILE WHOSE CHAIN IS DAMAGED IS STILL DELETABLE. Refusing would strand the
//  volume -- the entry could never be removed and its space never reclaimed --
//  and the developer this serves is working with precisely those disks. What the
//  walk reached is freed on the same unique-ownership terms; what it could not
//  reach was never observed and is not touched.
//
//  An entry occupying no sectors claims nothing, so nothing is freed for it. The
//  discriminator is the entry's own sector count, which is what the chain walk
//  already uses; twenty of the sixty-three entries on Merlin's own disk are of
//  that shape and following their $7F/$7F pointer would be inventing a chain.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Delete (
    const FilePath  & path,
    vector<Byte>    & outBuffer,
    DeleteOutcome   & outOutcome) const
{
    HRESULT                hr          = S_OK;
    size_t                 bufferBytes = m_sectors.size();
    bool                   single      = path.IsSingleComponent();
    bool                   found       = false;
    bool                   isLocked    = false;
    bool                   fullyParsed = true;
    uint16_t               owner       = 0;
    vector<RawEntry>       entries;
    vector<std::string>    damage;
    vector<Byte>           result;
    VolumeIntegrityReport  report;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBREx (single, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    CollectEntries (entries, damage, fullyParsed);

    found = TryFindEntry (entries, path.GetLeaf(), owner);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    isLocked = (entries[owner].typeByte & kLockedBit) != 0;
    CBREx (!isLocked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    hr = BuildIntegrityReport (report);
    CHRA (hr);

    outOutcome                    = DeleteOutcome();
    outOutcome.catalogFullyParsed = report.IsCatalogFullyParsed();

    result = m_sectors;

    // The report walks the same catalog in the same order over the same buffer,
    // so an owner index names the same entry on both sides.
    for (uint32_t unit : report.GetClaimsOf (owner))
    {
        bool  mineAlone = report.IsUniquelyOwnedBy (unit, owner);

        if (mineAlone)
        {
            SetFreeInBitmap (result, TrackOf (unit), SectorOf (unit), true);
            outOutcome.freedUnits.push_back (unit);
        }
        else
        {
            outOutcome.leakedUnits.push_back (unit);
        }
    }

    for (uint16_t broken : report.GetUnfollowableChains())
    {
        if (broken == owner)
        {
            outOutcome.chainWasDamaged = true;
        }
    }

    // DOS's own tombstone: the original track byte moves into the last byte of
    // the name and $FF takes its place, which is how an undelete tool finds the
    // file again.
    WriteByteAt (result,
                 entries[owner].catalogTrack,
                 entries[owner].catalogSector,
                 entries[owner].entryOffset + kEntOffLastNameByte,
                 (Byte) entries[owner].tsListTrack);

    WriteByteAt (result,
                 entries[owner].catalogTrack,
                 entries[owner].catalogSector,
                 entries[owner].entryOffset + kEntOffTsTrack,
                 kEntryDeleted);

    AppendDeleteWarnings (outOutcome);

    hr = HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::HasDosImage
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Volume::HasDosImage() const
{
    int     track  = 0;
    int     sector = 0;
    size_t  i      = 0;



    for (track = 0; track < Dos33Skeleton::kDosImageTracks; track++)
    {
        for (sector = 0; sector < NibblizationLayer::kSectorsPerTrack; sector++)
        {
            for (i = 0; i < (size_t) NibblizationLayer::kSectorByteSize; i++)
            {
                if (ReadByte (track, sector, i) != 0)
                {
                    return true;
                }
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::SetStartupProgram
//
//  DOS 3.3 keeps the name of the program it runs after booting inside its own
//  image on track 1, so setting it is a patch of thirty bytes and nothing else:
//  no catalog entry is added, moved or touched, and no file is written to chain
//  to another. A greeting that chained would cost a catalog slot and sectors,
//  change what CATALOG shows, and still not be how DOS does it.
//
//  THE NAME WRITTEN IS THE ONE THE CATALOG STORES, byte for byte, rather than
//  the one the caller spelled. DOS matches the greeting name against catalog
//  names on the disk, so a lower-case letter -- or one of the control characters
//  a real disk legitimately carries in a name -- would name a file DOS then
//  fails to find, with the disk booting to an error instead of to the program.
//
//  A VOLUME WITH NO DOS ON IT IS REFUSED. The tracks the image occupies are
//  reserved by the format whether or not anything was installed in them, so the
//  patch would land in space nothing reads and the command would report success
//  for a disk that still cannot boot at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    HRESULT                hr          = S_OK;
    size_t                 bufferBytes = m_sectors.size();
    bool                   single      = path.IsSingleComponent();
    bool                   hasDos      = false;
    bool                   found       = false;
    bool                   fullyParsed = true;
    uint16_t               owner       = 0;
    size_t                 i           = 0;
    vector<RawEntry>       entries;
    vector<std::string>    damage;
    vector<Byte>           result;
    VolumeIntegrityReport  report;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBREx (single, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    hasDos = HasDosImage();
    CBREx (hasDos, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    CollectEntries (entries, damage, fullyParsed);

    found = TryFindEntry (entries, path.GetLeaf(), owner);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = BuildIntegrityReport (report);
    CHRA (hr);

    result = m_sectors;

    for (i = 0; i < kNameBytes; i++)
    {
        Byte  stored = ReadByte (entries[owner].catalogTrack,
                                 entries[owner].catalogSector,
                                 entries[owner].entryOffset + kEntOffName + i);

        WriteByteAt (result, kGreetingTrack, kGreetingSector, kGreetingOffset + i, stored);
    }

    // The same self-check every other mutating call runs over its own output.
    // Nothing here moves a sector, so a difference would mean the patch landed
    // somewhere it had no business being.
    hr = HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}
