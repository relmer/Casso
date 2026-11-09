#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume
//
//  The file system of the Gutenberg and Gutenberg, Jr. word processors on a
//  140K disk, over a flat sector buffer in DOS logical order. Read-only: the
//  format of the allocation map in track 17, sector 6 is unknown, so every
//  change fails with ERROR_WRITE_PROTECT.
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
//  The layout is from CiderPress II's Gutenberg notes, under its Apache
//  license, which credit David Schmidt's reverse engineering.
//
//  Addressing for the integrity pass is one unit per sector, track * 16 +
//  sector.
//
////////////////////////////////////////////////////////////////////////////////

class GutenbergVolume : public IVolume
{
public:
    static constexpr uint32_t  kUnitCount           = 560;
    static constexpr int       kTracks              = 35;
    static constexpr int       kSectorsPerTrack     = 16;
    static constexpr int       kDirectoryTrack      = 17;
    static constexpr int       kDirectorySector     = 7;
    static constexpr int       kMapSector           = 6;
    static constexpr int       kFirstDataTrack      = 3;
    static constexpr int       kDirectorySectorsMax = 64;
    static constexpr int       kEntriesPerSector    = 15;
    static constexpr size_t    kEntryBytes          = 16;
    static constexpr size_t    kHeaderBytes         = 6;
    static constexpr size_t    kDataBytes           = 250;
    static constexpr size_t    kSectorBytes         = 256;
    static constexpr Byte      kTypeDocument        = ' ';
    static constexpr Byte      kTypeLocked          = 'L';
    static constexpr Byte      kTypeProgram         = 'P';
    static constexpr Byte      kTypeProgramM        = 'M';
    static constexpr Byte      kDocumentEnd         = 0x00;

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

private:
    //  One directory entry.
    struct RawFile
    {
        std::string  name;
        int          track  = 0;
        int          sector = 0;
        Byte         type   = 0;
    };

    //  The sectors of one file in order, as units, and what stopped the walk
    //  when it didn't end at a last sector.
    struct Chain
    {
        vector<uint32_t>  units;
        std::string       damage;
    };

    static constexpr HRESULT  kReadOnly = HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);

    static int          DecodeLink        (Byte stored);
    static bool         IsDirectorySector (const vector<Byte> & sectors, uint32_t unit);
    static void         WalkChain         (const vector<Byte> & sectors, int track, int sector, Chain & outChain);
    static std::string  ReadText          (const vector<Byte> & sectors, size_t at, size_t length);
    static bool         TryFindFile       (const vector<RawFile> & files, const std::string & name, size_t & outIndex);

    void  CollectFiles (vector<RawFile> & outFiles, Chain & outDirectory, std::string & outVolumeName) const;
    void  NoteRead     (const Chain & chain) const;

    const vector<Byte> &   m_sectors;
    ISectorReadObserver  * m_readObserver = nullptr;
};
