#pragma once

#include "Pch.h"

class Dos33SectorAccess;
class FilePath;





//  One catalog record, plus where it was found so a rewrite can address it.
struct Dos33CatalogEntry
{
    int       catalogTrack  = 0;
    int       catalogSector = 0;
    size_t    entryOffset   = 0;
    int       tsListTrack   = 0;
    int       tsListSector  = 0;
    Byte      typeByte      = 0;
    uint16_t  sectorCount   = 0;
    bool      isDeleted     = false;
    string    name;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Catalog
//
//  The DOS 3.3 catalog: the chain of catalog sectors on track 17, the entries
//  in them, and their file names. Reads go through the volume's
//  sector access; writes go into a copy of the buffer.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33Catalog
{
public:
    explicit Dos33Catalog (const Dos33SectorAccess & access);

    //  Walks the catalog chain. Damage is appended rather than thrown, and
    //  `outFullyParsed` reports whether anything had to be skipped.
    void  CollectEntries (vector<Dos33CatalogEntry>  & outEntries,
                          vector<std::string>        & outDamage,
                          bool                       & outFullyParsed) const;

    //  A catalog slot is reusable only when its track byte says never-used or
    //  deleted. AN ENTRY OCCUPYING NO SECTORS IS STILL AN ENTRY -- twenty of
    //  the sixty-three on Merlin's own disk are exactly that, drawing headings
    //  in CATALOG -- so the sector count must not be consulted here.
    bool  TryFindFreeSlot (int & outTrack, int & outSector, size_t & outOffset) const;

    //  The thirty name bytes an entry stores, exactly as stored.
    void  ReadStoredName  (const Dos33CatalogEntry & entry, vector<Byte> & outBytes) const;

    void  WriteEntry      (vector<Byte>        & buffer,
                           int                   catalogTrack,
                           int                   catalogSector,
                           size_t                entryOffset,
                           uint32_t              listUnit,
                           Byte                  typeByte,
                           uint16_t              sectorCount,
                           const vector<Byte>  & nameBytes) const;

    void  WriteName       (vector<Byte> & buffer, const Dos33CatalogEntry & entry, const vector<Byte> & nameBytes) const;
    void  WriteTypeByte   (vector<Byte> & buffer, const Dos33CatalogEntry & entry, Byte typeByte) const;
    void  WriteTombstone  (vector<Byte> & buffer, const Dos33CatalogEntry & entry) const;

    //  Case-insensitive, because the catalog is upper case and the caller's
    //  shell is not.
    static bool     TryFindEntry  (const vector<Dos33CatalogEntry>  & entries,
                                   const std::string                & leaf,
                                   uint16_t                         & outOwner);

    //  One entry by path: at its catalog index when the path includes one, whose
    //  name must still match, and otherwise by a name only one entry has.
    //  Returns ERROR_DUP_NAME for a name that several entries share.
    static HRESULT  FindEntry     (const vector<Dos33CatalogEntry>  & entries,
                                   const FilePath                   & path,
                                   uint16_t                         & outOwner);

    //  30 bytes of high ASCII, space-padded. False when the name is one DOS
    //  could not open again.
    static bool     TryEncodeName (const std::string & name, vector<Byte> & outBytes);

    //  Whether DOS 3.3 could open a file of this name again, the rule a rename
    //  applies.
    static bool     IsValidName   (const std::string & name);

private:
    //  Catalog geometry. The VTOC's offsets are in Dos33Skeleton.
    static constexpr size_t  kEntryBase        = 0x0B;
    static constexpr size_t  kEntrySize        = 0x23;
    static constexpr int     kEntriesPerSector = 7;
    static constexpr size_t  kNameBytes        = 30;

    static constexpr size_t  kEntOffTsTrack     = 0x00;
    static constexpr size_t  kEntOffTsSector    = 0x01;
    static constexpr size_t  kEntOffType        = 0x02;
    static constexpr size_t  kEntOffName        = 0x03;
    static constexpr size_t  kEntOffSectorCount = 0x21;

    //  Where DOS stashes the original track byte when it tombstones an entry.
    static constexpr size_t  kEntOffLastNameByte = 0x20;

    //  A track byte of $FF marks a deleted entry; $00 means the catalog has no
    //  further entries at all.
    static constexpr Byte    kEntryDeleted = 0xFF;
    static constexpr Byte    kEntryUnused  = 0x00;

    //  High-ASCII space, which is what a short name is padded out with.
    static constexpr Byte    kNamePad = 0xA0;

    const Dos33SectorAccess  & m_access;
};
