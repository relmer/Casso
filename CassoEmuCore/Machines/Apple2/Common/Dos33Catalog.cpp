#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Devices/Disk/ChainWalkGuard.h"
#include "Devices/Disk/FilePath.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::Dos33Catalog
//
////////////////////////////////////////////////////////////////////////////////

Dos33Catalog::Dos33Catalog (const Dos33SectorAccess & access)
    : m_access (access)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::CollectEntries
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

void Dos33Catalog::CollectEntries (
    vector<Dos33CatalogEntry>    & outEntries,
    vector<std::string> & outDamage,
    bool                & outFullyParsed) const
{
    ChainWalkGuard  guard (m_access.GetGeometry().GetUnitCount());
    int             track  = 0;
    int             sector = 0;
    int             slot   = 0;
    size_t          i      = 0;
    bool            more   = true;



    outFullyParsed = true;

    track  = m_access.ReadByte (Dos33Skeleton::kVtocTrack, 0, Dos33Skeleton::kVtocOffCatalogTrack);
    sector = m_access.ReadByte (Dos33Skeleton::kVtocTrack, 0, Dos33Skeleton::kVtocOffCatalogSector);

    while (more && track != 0)
    {
        bool  stepOk = guard.TryVisit (m_access.ToUnit (track, sector));

        if (!stepOk)
        {
            outDamage.push_back ("the catalog chain loops or leaves the volume; "
                                 "the entries listed are those reachable before it did");
            outFullyParsed = false;
            break;
        }

        if (track >= m_access.GetGeometry().tracks
         || sector >= m_access.GetGeometry().sectorsPerTrack)
        {
            outDamage.push_back ("the catalog chain points to a sector outside the volume");
            outFullyParsed = false;
            break;
        }

        for (slot = 0; slot < kEntriesPerSector; slot++)
        {
            Dos33CatalogEntry  entry;
            Byte               tsTrack = 0;

            entry.entryOffset   = kEntryBase + (size_t) slot * kEntrySize;
            entry.catalogTrack  = track;
            entry.catalogSector = sector;

            tsTrack = m_access.ReadByte (track, sector, entry.entryOffset + kEntOffTsTrack);

            if (tsTrack == kEntryUnused)
            {
                continue;
            }

            entry.isDeleted    = tsTrack == kEntryDeleted;
            entry.tsListTrack  = tsTrack;
            entry.tsListSector = m_access.ReadByte (track, sector, entry.entryOffset + kEntOffTsSector);
            entry.typeByte     = m_access.ReadByte (track, sector, entry.entryOffset + kEntOffType);
            entry.sectorCount  = (uint16_t)
                (m_access.ReadByte (track, sector, entry.entryOffset + kEntOffSectorCount)
              | (m_access.ReadByte (track, sector, entry.entryOffset + kEntOffSectorCount + 1) << 8));

            // Names are high ASCII padded with high-ASCII spaces. Strip the high
            // bit and the padding; keep interior spaces, which are legal.
            for (i = 0; i < kNameBytes; i++)
            {
                Byte  c = m_access.ReadByte (track, sector, entry.entryOffset + kEntOffName + i);

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
            int  nextTrack  = m_access.ReadByte (track, sector, Dos33Skeleton::kCatalogOffNextTrack);
            int  nextSector = m_access.ReadByte (track, sector, Dos33Skeleton::kCatalogOffNextSector);

            more   = nextTrack != 0;
            track  = nextTrack;
            sector = nextSector;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::TryFindEntry
//
//  Case-insensitive, because the catalog is upper case and the shell the caller
//  typed into is not.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Catalog::TryFindEntry (
    const vector<Dos33CatalogEntry>  & entries,
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
//  Dos33Catalog::FindEntry
//
//  DOS USES THE FIRST MATCH; THIS DOES NOT. DOS 3.3 does not enforce unique
//  names: a catalog editor writes entries directly, and the Merlin disk has ten
//  identical heading entries. Using the first of several would delete or
//  rename an entry the caller did not select, so a shared name returns
//  ERROR_DUP_NAME, and the catalog index selects exactly one entry.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Catalog::FindEntry (
    const vector<Dos33CatalogEntry>  & entries,
    const FilePath          & path,
    uint16_t                & outOwner)
{
    HRESULT      hr      = S_OK;
    std::string  leaf    = path.GetLeaf();
    size_t       matches = 0;
    size_t       index   = 0;
    bool         inRange = false;
    bool         named   = false;



    if (path.HasLeafIndex())
    {
        index   = path.GetLeafIndex();
        inRange = index < entries.size();
        CBREx (inRange, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

        named = _stricmp (entries[index].name.c_str(), leaf.c_str()) == 0;
        CBREx (named, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

        outOwner = (uint16_t) index;
    }
    else
    {
        for (index = 0; index < entries.size(); index++)
        {
            if (_stricmp (entries[index].name.c_str(), leaf.c_str()) == 0)
            {
                outOwner = (matches == 0) ? (uint16_t) index : outOwner;
                matches++;
            }
        }

        CBREx (matches > 0,  HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
        CBREx (matches == 1, HRESULT_FROM_WIN32 (ERROR_DUP_NAME));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::TryEncodeName
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

bool Dos33Catalog::TryEncodeName (const std::string & name, vector<Byte> & outBytes)
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
//  Dos33Catalog::TryFindFreeSlot
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

bool Dos33Catalog::TryFindFreeSlot (int & outTrack, int & outSector, size_t & outOffset) const
{
    ChainWalkGuard  guard (m_access.GetGeometry().GetUnitCount());
    int             track  = 0;
    int             sector = 0;
    int             slot   = 0;
    bool            more   = true;



    track  = m_access.ReadByte (Dos33Skeleton::kVtocTrack, 0, Dos33Skeleton::kVtocOffCatalogTrack);
    sector = m_access.ReadByte (Dos33Skeleton::kVtocTrack, 0, Dos33Skeleton::kVtocOffCatalogSector);

    while (more && track != 0)
    {
        bool  stepOk = guard.TryVisit (m_access.ToUnit (track, sector));

        if (!stepOk || track >= m_access.GetGeometry().tracks
                    || sector >= m_access.GetGeometry().sectorsPerTrack)
        {
            break;
        }

        for (slot = 0; slot < kEntriesPerSector; slot++)
        {
            size_t  entryOffset = kEntryBase + (size_t) slot * kEntrySize;
            Byte    tsTrack     = m_access.ReadByte (track, sector, entryOffset + kEntOffTsTrack);

            if (tsTrack == kEntryUnused || tsTrack == kEntryDeleted)
            {
                outTrack  = track;
                outSector = sector;
                outOffset = entryOffset;

                return true;
            }
        }

        {
            int  nextTrack  = m_access.ReadByte (track, sector, Dos33Skeleton::kCatalogOffNextTrack);
            int  nextSector = m_access.ReadByte (track, sector, Dos33Skeleton::kCatalogOffNextSector);

            more   = nextTrack != 0;
            track  = nextTrack;
            sector = nextSector;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::WriteEntry
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Catalog::WriteEntry (
    vector<Byte>        & buffer,
    int                   catalogTrack,
    int                   catalogSector,
    size_t                entryOffset,
    uint32_t              listUnit,
    Byte                  typeByte,
    uint16_t              sectorCount,
    const vector<Byte>  & nameBytes) const
{
    size_t  i = 0;



    m_access.WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffTsTrack,
                          (Byte) m_access.GetTrack (listUnit));
    m_access.WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffTsSector,
                          (Byte) m_access.GetSector (listUnit));
    m_access.WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffType, typeByte);

    for (i = 0; i < kNameBytes; i++)
    {
        Byte  c = (i < nameBytes.size()) ? nameBytes[i] : kNamePad;

        m_access.WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffName + i, c);
    }

    m_access.WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffSectorCount,
                          (Byte) (sectorCount & 0xFF));
    m_access.WriteByteAt (buffer, catalogTrack, catalogSector, entryOffset + kEntOffSectorCount + 1,
                          (Byte) (sectorCount >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::ReadStoredName
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Catalog::ReadStoredName (const Dos33CatalogEntry & entry, vector<Byte> & outBytes) const
{
    size_t  i = 0;



    outBytes.clear();

    for (i = 0; i < kNameBytes; i++)
    {
        outBytes.push_back (m_access.ReadByte (entry.catalogTrack, entry.catalogSector, entry.entryOffset + kEntOffName + i));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::WriteName
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Catalog::WriteName (vector<Byte> & buffer, const Dos33CatalogEntry & entry, const vector<Byte> & nameBytes) const
{
    size_t  i = 0;



    for (i = 0; i < kNameBytes; i++)
    {
        m_access.WriteByteAt (buffer,
                              entry.catalogTrack,
                              entry.catalogSector,
                              entry.entryOffset + kEntOffName + i,
                              nameBytes[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::WriteTypeByte
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Catalog::WriteTypeByte (vector<Byte> & buffer, const Dos33CatalogEntry & entry, Byte typeByte) const
{
    m_access.WriteByteAt (buffer, entry.catalogTrack, entry.catalogSector, entry.entryOffset + kEntOffType, typeByte);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::WriteTombstone
//
//  DOS's own tombstone: the original track byte moves into the last byte of
//  the name and $FF takes its place, which is how an undelete tool finds the
//  file again.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Catalog::WriteTombstone (vector<Byte> & buffer, const Dos33CatalogEntry & entry) const
{
    m_access.WriteByteAt (buffer,
                          entry.catalogTrack,
                          entry.catalogSector,
                          entry.entryOffset + kEntOffLastNameByte,
                          (Byte) entry.tsListTrack);

    m_access.WriteByteAt (buffer,
                          entry.catalogTrack,
                          entry.catalogSector,
                          entry.entryOffset + kEntOffTsTrack,
                          kEntryDeleted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog::IsValidName
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Catalog::IsValidName (const std::string & name)
{
    vector<Byte>  bytes;



    return TryEncodeName (name, bytes);
}





