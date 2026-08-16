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
    int               bit         = 0;
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

    // Free bitmap: 4 bytes per track at VTOC+0x38. Byte 0 holds sectors 15..8
    // in bits 7..0, byte 1 holds sectors 7..0. A SET bit means free.
    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        size_t  at   = Dos33Skeleton::kVtocOffFreeBitmap + (size_t) track * 4;
        Byte    high = ReadByte (Dos33Skeleton::kVtocTrack, 0, at);
        Byte    low  = ReadByte (Dos33Skeleton::kVtocTrack, 0, at + 1);

        for (bit = 0; bit < 8; bit++)
        {
            freeUnits += (uint32_t) ((high >> bit) & 1);
            freeUnits += (uint32_t) ((low  >> bit) & 1);
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
    vector<RawEntry>     entries;
    vector<uint32_t>     units;
    vector<std::string>  damage;
    RawEntry             match;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBREx (single, E_INVALIDARG);

    CollectEntries (entries, damage, fullyParsed);

    for (const RawEntry & entry : entries)
    {
        if (_stricmp (entry.name.c_str(), path.GetLeaf().c_str()) == 0)
        {
            match = entry;
            found = true;
            break;
        }
    }

    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    {
        ChainWalkGuard  guard (kUnitCount);

        walked = CollectDataSectors (match, units, guard);
    }

    CBREx (walked, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    outPayload          = FilePayload();
    outPayload.type     = (Byte) (match.typeByte & ~kLockedBit);
    outPayload.encoding = PayloadEncoding::Verbatim;

    for (uint32_t unit : units)
    {
        int     track  = (int) (unit / NibblizationLayer::kSectorsPerTrack);
        int     sector = (int) (unit % NibblizationLayer::kSectorsPerTrack);
        size_t  i      = 0;

        for (i = 0; i < (size_t) NibblizationLayer::kSectorByteSize; i++)
        {
            outPayload.bytes.push_back (ReadByte (track, sector, i));
        }
    }

    ApplyTypeHeader (match.typeByte, outPayload.bytes, outPayload);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::BuildIntegrityReport
//
//  Every catalog entry's chain walked into the claim map, then compared against
//  the VTOC's own free bitmap. The catalog track's own sectors are claimed too:
//  they are as allocated as any file's, and a report that ignored them would
//  call every volume inconsistent.
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
    int                  bit         = 0;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    outReport.Reset (kUnitCount);

    CollectEntries (entries, damage, fullyParsed);
    outReport.SetCatalogFullyParsed (fullyParsed);

    for (owner = 0; owner < (uint16_t) entries.size(); owner++)
    {
        ChainWalkGuard    guard (kUnitCount);
        vector<uint32_t>  units;
        bool              walked = CollectDataSectors (entries[owner], units, guard);

        // The list sectors themselves are part of the file's footprint -- but
        // only for an entry that occupies storage at all. A decorative entry's
        // pointer names a sector it does not own.
        if (entries[owner].sectorCount > 0)
        {
            outReport.AddClaim (ToUnit (entries[owner].tsListTrack, entries[owner].tsListSector), owner);
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
        size_t  at   = Dos33Skeleton::kVtocOffFreeBitmap + (size_t) track * 4;
        Byte    high = ReadByte (Dos33Skeleton::kVtocTrack, 0, at);
        Byte    low  = ReadByte (Dos33Skeleton::kVtocTrack, 0, at + 1);

        for (bit = 0; bit < 8; bit++)
        {
            bool  freeHigh = ((high >> bit) & 1) != 0;
            bool  freeLow  = ((low  >> bit) & 1) != 0;

            outReport.SetAllocatedInFreeMap (ToUnit (track, 8 + bit), !freeHigh);
            outReport.SetAllocatedInFreeMap (ToUnit (track, bit),     !freeLow);
        }
    }

    outReport.Finish();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Write / Delete / SetStartupProgram
//
//  Not yet implemented. They return a failure rather than silently doing
//  nothing, so a caller cannot mistake an absent capability for a completed
//  operation -- the same rule the decoder now follows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Write (
    const FilePath     & path,
    const FilePayload  & payload,
    vector<Byte>       & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (payload);
    UNREFERENCED_PARAMETER (outBuffer);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return E_NOTIMPL;
}
