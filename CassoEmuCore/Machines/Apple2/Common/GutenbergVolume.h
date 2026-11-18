#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume
//
//  The file system of the Gutenberg and Gutenberg, Jr. word processors on a
//  140K disk, over a flat sector buffer in DOS logical order.
//
//  Every sector of a file begins with six bytes: the track and sector of the
//  previous sector, of this one, and of the next. 250 bytes of data follow.
//  The list is circular. Bit 7 of the next track is set in a file's last
//  sector, and bit 7 of the current sector in its first. A track or sector
//  of 0 is stored as $40 in the directory and in some links. Backward links
//  are stale in some files, so a chain is followed by its forward links
//  alone.
//
//  The directory is a file, DIR, whose first sector is track 17, sector 7.
//  Each of its sectors has the volume name in bytes $06 to $0E, $8D at $0F,
//  and fifteen 16-byte entries: a name of 12 high-ASCII characters padded
//  with spaces, the first track and sector, a type, and $8D. The types are a
//  high-ASCII space for a document, L for a locked document, and P or M for
//  a program. A slot whose first byte is a space is empty. The first entry
//  is DIR.
//
//  A file's length isn't stored: it is 250 bytes for each sector of its
//  chain, and a document ends at its first $00.
//
//  The allocation map in track 17, sector 6 covers tracks 3 to 34, eight
//  bytes a track, one byte for each pair of sectors: bit 7 is set when the
//  even sector is in use, and bit 0 when the odd one is. Tracks 0 to 2 aren't
//  in the map and are never allocated. Gutenberg's FORMAT marks track 17,
//  sectors 0 to 7 in use, and stores $FF for track 17, sectors 0 and 1 and
//  for track 33, sector 8 to the end of the disk, so 480 sectors are free. Gutenberg allocates the lowest free sector in map order, and a
//  directory that outgrows its sectors gains one from the map, with the
//  volume name, before the new file's data. A delete clears the file's
//  bits and fills its entry with spaces. The links in a sector header store
//  0 as $00; a link to a file's first sector has bit 7 set in its sector,
//  except in the last sector's next link, which has bit 7 set in its track.
//  The map was worked out from disks Gutenberg, Jr. formatted and wrote in
//  Casso.
//
//  The rest of the layout is from CiderPress II's Gutenberg notes, under its Apache
//  license, which credit David Schmidt's reverse engineering.
//
//  Addressing for the integrity pass is one unit per sector, track * 16 +
//  sector.
//
////////////////////////////////////////////////////////////////////////////////

class GutenbergVolume : public IVolume
{
public:
    static constexpr uint32_t  kUnitCount                 = 560;
    static constexpr int       kTracks                    = 35;
    static constexpr int       kSectorsPerTrack           = 16;
    static constexpr int       kDirectoryTrack            = 17;
    static constexpr int       kDirectorySector           = 7;
    static constexpr int       kMapSector                 = 6;
    static constexpr int       kFirstDataTrack            = 3;
    static constexpr int       kDirectorySectorsMax       = 64;
    static constexpr int       kEntriesPerSector          = 15;
    static constexpr size_t    kEntryBytes                = 16;
    static constexpr size_t    kHeaderBytes               = 6;
    static constexpr size_t    kDataBytes                 = 250;
    static constexpr size_t    kSectorBytes               = 256;
    static constexpr Byte      kTypeDocument              = ' ';
    static constexpr Byte      kTypeLocked                = 'L';
    static constexpr Byte      kTypeProgram               = 'P';
    static constexpr Byte      kTypeProgramM              = 'M';
    static constexpr Byte      kDocumentEnd               = 0x00;
    static constexpr size_t    kNameBytesMax              = 12;
    static constexpr size_t    kMapBytesPerTrack          = 8;
    static constexpr Byte      kMapEvenSector             = 0x80;
    static constexpr Byte      kMapOddSector              = 0x01;
    static constexpr Byte      kMapReserved               = 0xFF;
    static constexpr int       kDirectorySectorsFormatted = 8;

    //  The buffer is accessed by reference and isn't modified.
    explicit GutenbergVolume (const vector<Byte> & sectors);

    void     SetReadObserver (ISectorReadObserver * observer) { m_readObserver = observer; }

    HRESULT  Enumerate (VolumeListing & outListing) const override;
    HRESULT  Read      (const FilePath & path, FilePayload & outPayload) const override;

    HRESULT  Write     (const FilePath     & path,
                        const FilePayload  & payload,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  Delete    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  Delete    (const FilePath  & path,
                        vector<Byte>    & outBuffer,
                        DeleteOutcome   & outOutcome) const override;

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  Rename    (const FilePath     & from,
                        const std::string  & to,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  SetAttributes (const FilePath        & path,
                            const FileAttributes  & attributes,
                            vector<Byte>          & outBuffer) const override;

    //  Whether the DIR chain from track 17, sector 7 is a directory: every
    //  sector's current track and sector are where it is, $8D is at every
    //  sixteenth byte, and the chain ends.
    static bool  IsFound (const vector<Byte> & sectors);

    //  Whether a catalog type is a document's.
    static bool  IsDocumentType (Byte type) { return type == kTypeDocument || type == kTypeLocked; }

    //  Whether a catalog type is one Gutenberg stores.
    static bool  IsKnownType    (Byte type) { return IsDocumentType (type) || type == kTypeProgram || type == kTypeProgramM; }

    //  One to twelve printable characters, with no space or slash.
    static bool  IsNameValid    (const std::string & name);

    //  Whether the map marks a sector in use. A sector in tracks 0 to 2 is
    //  always in use.
    static bool  IsAllocatedInMap (const vector<Byte> & sectors, uint32_t unit);

private:
    //  One directory entry, and where it is in the buffer.
    struct RawFile
    {
        std::string  name;
        int          track   = 0;
        int          sector  = 0;
        Byte         type    = 0;
        size_t       entryAt = 0;
    };

    //  The sectors of one file in order, as units, and what stopped the walk
    //  when it didn't end at a last sector.
    struct Chain
    {
        vector<uint32_t>  units;
        std::string       damage;
    };

    static int          DecodeLink         (Byte stored);
    static bool         IsInMap            (uint32_t unit);
    static void         SetAllocatedInMap  (vector<Byte> & sectors, uint32_t unit, bool allocated);
    static bool         IsReserved         (const vector<Byte> & sectors, uint32_t unit);
    static void         WriteHeader        (vector<Byte> & sectors, uint32_t unit, uint32_t previous, uint32_t next, uint32_t first, bool isLast);
    static void         WriteEntry         (vector<Byte> & sectors, size_t at, const std::string & name, uint32_t first, Byte type);
    static void         WriteName          (vector<Byte> & sectors, size_t at, const std::string & name);
    static void         ClearEntry         (vector<Byte> & sectors, size_t at);
    static void         AddDirectorySector (vector<Byte> & sectors, const Chain & directory, uint32_t unit);
    static void         LocateInMap        (uint32_t unit, size_t & outAt, Byte & outBit);
    static bool         TryAllocate        (const vector<Byte> & sectors, vector<bool> & ioTaken, uint32_t & outUnit);
    static bool         TryFindEmptySlot   (const vector<Byte> & sectors, const Chain & directory, size_t & outAt);
    static Byte         GetStoredType      (const FilePayload & payload);
    static HRESULT      HandBackResult     (const VolumeIntegrityReport & before, const vector<Byte> & result, vector<Byte> & outBuffer);
    static bool         IsDirectorySector  (const vector<Byte> & sectors, uint32_t unit);
    static void         WalkChain          (const vector<Byte> & sectors, int track, int sector, Chain & outChain);
    static std::string  ReadText           (const vector<Byte> & sectors, size_t at, size_t length);
    static bool         TryFindFile        (const vector<RawFile> & files, const std::string & name, size_t & outIndex);

    void  CollectFiles (vector<RawFile> & outFiles, Chain & outDirectory, std::string & outVolumeName) const;
    void  NoteRead     (const Chain & chain) const;

    const vector<Byte> &   m_sectors;
    ISectorReadObserver  * m_readObserver = nullptr;
};
