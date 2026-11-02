#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume
//
//  A CP/M 2.2 filesystem on an Apple II 140K disk, over a flat sector buffer in
//  DOS logical order.
//
//  Allocation blocks are 1 KB, four 256-byte CP/M sectors, numbered from track
//  3. CP/M sector n is physical sector 3n mod 16. Blocks 0 and 1 are the
//  directory. A block number past the data area wraps to tracks 0 to 2, which
//  is where a boot disk's system entry in user 31 points. Each directory entry
//  has a user number, an 8.3 name, an extent number, a count of 128-byte records
//  and up to 16 block numbers. There is no free map: a block no entry lists is
//  free. A free entry has a first byte of $E5.
//
//  A file's name in this class is NAME.TYP, or U:NAME.TYP for a user other than
//  0. A file's length is a count of 128-byte records, so its size is a multiple
//  of 128 bytes, and a text file ends at its first $1A.
//
//  Addressing for the integrity pass is one unit per block.
//
////////////////////////////////////////////////////////////////////////////////

class CpmVolume : public IVolume
{
public:
    static constexpr uint32_t  kVolumeBlocks     = 128;
    static constexpr uint32_t  kUnitCount        = 140;
    static constexpr int       kDirectoryTrack   = 3;
    static constexpr int       kDirectoryBlocks  = 2;
    static constexpr int       kSectorsPerBlock  = 4;
    static constexpr int       kEntries          = 64;
    static constexpr size_t    kEntryBytes       = 32;
    static constexpr size_t    kBlockBytes       = 1024;
    static constexpr size_t    kRecordBytes      = 128;
    static constexpr Byte      kFree             = 0xE5;
    static constexpr Byte      kTextEnd          = 0x1A;

    //  The buffer is accessed by reference and isn't modified.
    explicit CpmVolume (const vector<Byte> & sectors);

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

    //  Whether the directory looks like CP/M's, from its entries alone, since
    //  CP/M has no signature. A file entry and nothing invalid is enough. On a
    //  disk with leftover data in part of its directory, a few plausible entries
    //  are required. A directory where every slot is free, each one all $E5 or
    //  the remains of an erased entry, is an empty CP/M disk, which is what
    //  CP/M's FORMAT and `disk create` make.
    static bool  IsFound (const vector<Byte> & sectors);

    //  A blank disk as CP/M's FORMAT leaves it: $E5 in every byte.
    static void  WriteBlank (vector<Byte> & outSectors);

    //  The buffer offset of a block's nth 256-byte sector.
    static size_t  GetSectorOffset (int block, int sector);

    //  The sector numbers in the buffer, track * 16 + sector, of the directory.
    static vector<size_t>  GetDirectorySectors ();

    //  An optional user of 0 to 15 and a colon, then 1 to 8 characters and an
    //  optional period and up to 3 more, with no space or any of < > , ; : = ? * [ ].
    static bool  IsValidName (const std::string & name);

    //  A text file's bytes up to its first $1A.
    static void  DecodeText (std::span<const Byte> bytes, std::string & outText);

    //  Host text with each line ending in CR LF.
    static void  EncodeText (const std::string & text, vector<Byte> & outBytes);

private:
    //  One directory entry, as stored.
    struct RawEntry
    {
        int                   slot       = 0;
        int                   user       = 0;
        int                   extent     = 0;
        int                   records    = 0;
        bool                  isReadOnly = false;
        bool                  isSystem   = false;
        std::string           name;
        std::array<Byte, 16>  blocks     = {};
    };

    //  A file's entries in extent order.
    struct RawFile
    {
        std::string       name;
        bool              isFixed    = false;
        bool              isReadOnly = false;
        bool              isSystem   = false;
        vector<RawEntry>  extents;
    };

    //  The name's parts, space padded, and its user.
    struct ParsedName
    {
        int                   user = 0;
        std::array<char, 11>  name = {};
    };

    static bool         TryParseName     (const std::string & name, ParsedName & outParsed);
    static std::string  MakeName         (const ParsedName & parsed);
    static bool         IsPlausibleEntry (std::span<const Byte> entry);
    static bool         IsFreeSlot       (std::span<const Byte> entry);

    void  ReadDirectory (vector<Byte> & outDirectory) const;
    void  ReadBlock     (int block, std::span<Byte> outBytes) const;

    //  The files in order of each one's first entry, and how many entries
    //  aren't free or valid.
    static void  CollectFiles (const vector<Byte> & directory, vector<RawFile> & outFiles, int & outInvalid);

    static bool  TryFindFile  (const vector<RawFile> & files, const std::string & name, size_t & outIndex);
    static void  FillEntry    (const RawFile & file, size_t index, FileEntry & outEntry);
    static int   CountRecords (const RawFile & file);

    static void     WriteBlockBytes (vector<Byte> & buffer, int block, std::span<const Byte> bytes);
    static void     WriteDirectory  (vector<Byte> & buffer, const vector<Byte> & directory);
    static HRESULT  HandBackResult  (const VolumeIntegrityReport & before, const vector<Byte> & result, vector<Byte> & outBuffer);

    const vector<Byte> &   m_sectors;
    ISectorReadObserver  * m_readObserver = nullptr;
};
