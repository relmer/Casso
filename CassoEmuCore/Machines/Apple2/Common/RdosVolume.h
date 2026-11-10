#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume
//
//  The file system of RDOS, the operating system of Strategic Simulations'
//  games, over a flat sector buffer in DOS logical order. Read-only: every
//  change fails with ERROR_WRITE_PROTECT.
//
//  A file is a run of consecutive 256-byte sectors. A sector index counts
//  from track 0, sector 0. Two layouts fit a 140K image:
//
//      Rdos33   RDOS 3.3. Sixteen sectors a track, in ProDOS sector order.
//      Rdos3    RDOS 2.1 copied to a 16-sector disk. Thirteen sectors a
//               track, in physical sector order. The last three physical
//               sectors of each track are outside the file system.
//
//  A 13-sector image is the third:
//
//      Rdos32   RDOS 2.1 on its original disk. Thirteen sectors a track, and
//               the buffer has thirteen sectors a track in that order.
//
//  The catalog is in track 1: all sixteen sectors in Rdos33, and sectors 0
//  to 10 in Rdos3 and Rdos32. Each entry is 32 bytes:
//
//      $00  name, 24 high-ASCII characters padded with spaces
//      $18  type, a high-ASCII A, B, T or S
//      $19  length in sectors
//      $1A  load address of a B file
//      $1C  length in bytes, a whole number of sectors for a T file
//      $1E  index of the first sector
//
//  A deleted entry has $80 as the first byte of the name, and $00 there ends
//  the list. The first entry is the operating system and the catalog, so its
//  first sector is 0 and its length is two tracks. That length selects the
//  layout. A T file ends at its first $00.
//
//  The layout is from CiderPress II's RDOS notes, under its Apache license.
//
//  Addressing for the integrity pass is one unit per sector of the buffer,
//  track * 16 + DOS logical sector, or track * 13 + sector in Rdos32.
//
////////////////////////////////////////////////////////////////////////////////

class RdosVolume : public IVolume
{
public:
    enum class Layout
    {
        None,
        Rdos33,
        Rdos3,
        Rdos32,
    };

    static constexpr uint32_t  kUnitCount             = 560;
    static constexpr int       kTracks                = 35;
    static constexpr int       kBufferSectorsPerTrack = 16;
    static constexpr int       kCatalogTrack          = 1;
    static constexpr int       kEntriesPerSector      = 8;
    static constexpr size_t    kEntryBytes            = 32;
    static constexpr size_t    kSectorBytes           = 256;
    static constexpr Byte      kTypeApplesoft         = 'A';
    static constexpr Byte      kTypeBinary            = 'B';
    static constexpr Byte      kTypeText              = 'T';
    static constexpr Byte      kTypeSystem            = 'S';

    //  The buffer is accessed by reference and isn't modified.
    explicit RdosVolume (const vector<Byte> & sectors);

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

    //  The layout of the catalog in track 1, or None when the first entry
    //  isn't the operating system's.
    static Layout  DetectLayout (const vector<Byte> & sectors);
    static bool    IsFound      (const vector<Byte> & sectors) { return DetectLayout (sectors) != Layout::None; }

    //  The sector of the buffer for a sector index, as track * 16 + sector.
    static uint32_t  GetUnit (Layout layout, uint32_t index);

    //  The sectors in the file system.
    static uint32_t  GetSectorTotal (Layout layout);

    //  The ProDOS file type for a catalog type: BAS, BIN, TXT, or 0.
    static Byte  GetProDosType (Byte type);

private:
    //  One catalog entry.
    struct RawFile
    {
        std::string  name;
        std::string  damage;
        size_t       slot        = 0;
        uint32_t     sectorCount = 0;
        uint32_t     firstSector = 0;
        Word         loadAddress = 0;
        Word         length      = 0;
        Byte         type        = 0;
    };

    static constexpr HRESULT  kReadOnly = HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);

    static int          GetSectorsPerTrack (Layout layout);
    static int          GetCatalogSectors  (Layout layout);
    static Word         ReadWord           (const vector<Byte> & sectors, size_t at);
    static std::string  ReadText           (const vector<Byte> & sectors, size_t at, size_t length);
    static bool         IsSystemName       (const std::string & name);
    static bool         TryFindFile        (const vector<RawFile> & files, const FilePath & path, size_t & outIndex);

    void  CollectFiles (vector<RawFile> & outFiles) const;
    void  ReadBytes    (const RawFile & file, bool notify, vector<Byte> & outBytes) const;

    const vector<Byte> &   m_sectors;
    Layout                 m_layout       = Layout::None;
    ISectorReadObserver  * m_readObserver = nullptr;
};
